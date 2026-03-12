# Objective
- Найти все обработчики входящих пакетов в серверном `net.dll`, включая скрытые pre-parser'ы, synthetic dispatch и raw RakNet internal path.
- Сопоставить реальные сетевые байты/packet IDs с открытыми исходами MTA (`packetenums.h`, `Packets.h`, `CPacketTranslator.cpp`).
- Вытащить неочевидные уязвимости, которые дают:
- однопакетный crash/info leak;
- сильную CPU/memory-нагрузку, способную положить сервер за минуты;
- любые заметные RCE-примитивы.

# Scope
- IDA database: `C:\Users\VanillaTempest\Desktop\NetReverse\net.dll`
- Image base: `0x10000000`
- Architecture: x86 (`metapc`)
- Source refs:
- `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Shared\sdk\net\packetenums.h`
- `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Shared\sdk\net\Packets.h`
- `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Server\mods\deathmatch\logic\CPacketTranslator.cpp`
- `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Server\mods\deathmatch\logic\packets\CPlayerJoinDataPacket.cpp`
- `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Server\mods\deathmatch\logic\packets\CPlayerModInfoPacket.cpp`
- `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Server\mods\deathmatch\logic\packets\CPlayerDiagnosticPacket.cpp`
- `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Server\mods\deathmatch\logic\packets\CPlayerACInfoPacket.cpp`
- Важная константа из исходов: `RID_USER_PACKET_ENUM = 99`.
- Следствие для этого билда: raw байт MTA user packet'а в `net.dll` равен `99 + PACKET_ID_*`, а `NetServerInterface__ProcessIncomingPackets` делает `packet_id = raw_byte - 99`.

# Method
1. Разобрал главный ingress-loop `NetServerInterface__ProcessIncomingPackets` (`0x100243C0`) и все его callee/xref.
2. Отдельно развернул raw RakNet internal dispatcher, скрытые helper'ы состояния/rate-limit и post-sync synthetic dispatch.
3. Сопоставил packet IDs с открытыми MTA enum'ами и клиент->сервер packet map из `CPacketTranslator.cpp`.
4. Проверил packet-specific parser'ы `PLAYER_JOINDATA`, `PLAYER_MODINFO`, `LIGHTSYNC` и service-хелперы вокруг них.
5. Переименовал в IDA ключевые неизвестные функции по роли и добавил комментарии на опасные места.

# Findings
## 1. Полная карта ingress в `net.dll`
### 1.1 Raw RakNet internal path
`NetServerInterface__ProcessIncomingPackets` делит трафик так:
- raw byte `1..98` уходит в `NetServerInterface__DispatchRakNetInternalPacket` (`0x10038600`);
- raw byte `>= 99` превращается в `PACKET_ID_*` через вычитание `99` и идёт в MTA path.

Найденные raw/internal handlers:

| Raw byte | Source enum/comment | Handler in `net.dll` | What happens дальше |
| --- | --- | --- | --- |
| `17` | `RID_NEW_INCOMING_CONNECTION` | `NetServerInterface__DispatchRakNetInternalPacket` | synthetic `PACKET_ID_PLAYER_JOIN (3)` наверх |
| `19` | `RID_DISCONNECTION_NOTIFICATION` | `NetServerInterface__HandleRakNetInternal_DisconnectionNotification` | synthetic `PACKET_ID_PLAYER_QUIT (5)` + cleanup connection map |
| `20` | `RID_CONNECTION_LOST` | `NetServerInterface__HandleRakNetInternal_ConnectionLost` | synthetic `PACKET_ID_PLAYER_TIMEOUT (6)` + cleanup connection map |
| `33` | stock source enum на этом raw ID уже не совпадает по смыслу с билдом | `NetServerInterface__HandleRakNetInternal_NoSocket` | synthetic `PACKET_ID_PLAYER_NO_SOCKET (102)` + disconnect/remove |
| `34` | stock source enum на этом raw ID уже не совпадает по смыслу с билдом | `NetServerInterface__HandleRakNetInternal_PlayerNetworkStatus` | synthetic `PACKET_ID_PLAYER_NETWORK_STATUS (103)` |

Вывод: часть реально входящей поверхности вообще не видна через open-source `CPacketTranslator.cpp`, потому что рождается раньше него из raw RakNet events.

### 1.2 MTA packet-specific logic внутри `net.dll`
Кроме generic dispatch наверх, в `net.dll` сидят отдельные скрытые pre/post-parser'ы:

| Packet ID | Name | Raw byte | net.dll role |
| --- | --- | --- | --- |
| `4` | `PACKET_ID_PLAYER_JOINDATA` | `103` | жирный dedicated parser |
| `24` | `PACKET_ID_RPC` | `123` | generic dispatch + hidden flag setup для deferred AC/modinfo flow |
| `25` | `PACKET_ID_PLAYER_LIST` | `124` | dedicated relay через `NetServerInterface__HandlePacket_PLAYER_LIST` |
| `32` | `PACKET_ID_PLAYER_PURESYNC` | `131` | generic dispatch + hidden flush `93/104` |
| `33` | `PACKET_ID_PLAYER_VEHICLE_PURESYNC` | `132` | generic dispatch + hidden flush `93/104` |
| `34` | `PACKET_ID_LIGHTSYNC` | `133` | dedicated nested parser с kick/uplink side effect |
| `42` | `PACKET_ID_COMMAND` | `141` | generic dispatch, но с отдельным rate-limit/kick gate |
| `84` | `PACKET_ID_CUSTOM_DATA` | `183` | generic dispatch, но с byte-budget gate |
| `90` | `PACKET_ID_DISCONNECT_MESSAGE` | `189` | generic dispatch, но участвует в early-throttle helper |
| `93` | `PACKET_ID_PLAYER_MODINFO` | `192` | dedicated deferred parser + synthetic `92` |
| `91` | `PACKET_ID_PLAYER_TRANSGRESSION` | synthetic | не приходит напрямую: рождается из kick helper |
| `92` | `PACKET_ID_PLAYER_DIAGNOSTIC` | synthetic | рождается из `PLAYER_MODINFO` subtype `0x60` |
| `104` | `PACKET_ID_PLAYER_ACINFO` | synthetic | рождается на первом `PURESYNC/VEHICLE_PURESYNC` |
| `23` | `PACKET_ID_SERVER_DISCONNECTED` | synthetic | может рождаться из kick helper до полного join state |

### 1.3 Полный client->server MTA packet map и как его трогает `net.dll`
Ниже список inbound MTA packet IDs из `CPacketTranslator.cpp` и реальная роль `net.dll`:

| ID | Name | net.dll handling |
| --- | --- | --- |
| `3` | `PACKET_ID_PLAYER_JOIN` | synthetic from raw `17` |
| `4` | `PACKET_ID_PLAYER_JOINDATA` | dedicated parser + early-throttle |
| `5` | `PACKET_ID_PLAYER_QUIT` | synthetic from raw `19` |
| `6` | `PACKET_ID_PLAYER_TIMEOUT` | synthetic from raw `20` |
| `25` | `PACKET_ID_PLAYER_LIST` | dedicated relay |
| `27` | `PACKET_ID_PLAYER_WASTED` | generic dispatch |
| `31` | `PACKET_ID_PLAYER_KEYSYNC` | generic dispatch |
| `32` | `PACKET_ID_PLAYER_PURESYNC` | generic dispatch + hidden flush `93/104` |
| `33` | `PACKET_ID_PLAYER_VEHICLE_PURESYNC` | generic dispatch + hidden flush `93/104` |
| `34` | `PACKET_ID_LIGHTSYNC` | dedicated parser |
| `37` | `PACKET_ID_EXPLOSION` | generic dispatch |
| `39` | `PACKET_ID_PROJECTILE` | generic dispatch |
| `40` | `PACKET_ID_DETONATE_SATCHELS` | generic dispatch |
| `41` | `PACKET_ID_DESTROY_SATCHELS` | generic dispatch |
| `42` | `PACKET_ID_COMMAND` | generic dispatch + rate-limit gate |
| `56` | `PACKET_ID_UNOCCUPIED_VEHICLE_SYNC` | generic dispatch |
| `58` | `PACKET_ID_VEHICLE_INOUT` | generic dispatch |
| `59` | `PACKET_ID_VEHICLE_DAMAGE_SYNC` | generic dispatch |
| `60` | `PACKET_ID_VEHICLE_TRAILER` | generic dispatch |
| `63` | `PACKET_ID_PED_SYNC` | generic dispatch |
| `64` | `PACKET_ID_PED_WASTED` | generic dispatch |
| `71` | `PACKET_ID_VOICE_DATA` | generic dispatch |
| `72` | `PACKET_ID_VOICE_END` | generic dispatch |
| `81` | `PACKET_ID_LUA_EVENT` | generic dispatch |
| `84` | `PACKET_ID_CUSTOM_DATA` | generic dispatch + byte-budget gate |
| `85` | `PACKET_ID_CAMERA_SYNC` | generic dispatch |
| `88` | `PACKET_ID_OBJECT_SYNC` | generic dispatch |
| `91` | `PACKET_ID_PLAYER_TRANSGRESSION` | synthetic via kick helper |
| `92` | `PACKET_ID_PLAYER_DIAGNOSTIC` | synthetic via `PLAYER_MODINFO` subtype `0x60` |
| `93` | `PACKET_ID_PLAYER_MODINFO` | dedicated deferred parser |
| `94` | `PACKET_ID_PLAYER_SCREENSHOT` | generic dispatch |
| `97` | `PACKET_ID_VEHICLE_PUSH_SYNC` | generic dispatch |
| `98` | `PACKET_ID_PLAYER_BULLETSYNC` | generic dispatch |
| `100` | `PACKET_ID_WEAPON_BULLETSYNC` | generic dispatch |
| `101` | `PACKET_ID_PED_TASK` | generic dispatch |
| `102` | `PACKET_ID_PLAYER_NO_SOCKET` | synthetic from raw `33` |
| `103` | `PACKET_ID_PLAYER_NETWORK_STATUS` | synthetic from raw `34` |
| `104` | `PACKET_ID_PLAYER_ACINFO` | synthetic after first `PURESYNC/VEHICLE_PURESYNC` |
| `108` | `PACKET_ID_PLAYER_RESOURCE_START` | generic dispatch |

### 1.4 Скрытый kick path по serial-like join строке
Отдельно разметил цепочку, которую легко принять за "можно кикать чужих игроков", но там есть важные ограничения:

- более свежая сверка с `NetServerInterface__HandlePacket_PLAYER_JOINDATA` показывает, что строка по смещению `player+0x888` наполняется именно из раннего фиксированного 32-байтного XOR-decoded join field;
- по open-source аналогу это куда больше похоже на `m_strSerialUser`, а не на ник;
- `NetServerInterface__QueuePlayerNotice` (`0x10004E90`) действительно использует строку из `a2+2184`, но это не доказывает, что поле человекочитаемое или равно нику;
- `NetServerInterface__SendAugmentedPacketToPeer` (`0x10031C50`) в case `2` сериализует строку из `player+0x888` в исходящий internal packet `PACKET_ID_SERVER_JOIN_COMPLETE (2)`, а не в `PACKET_ID_SERVER_JOINEDGAME (23)` из open-source;
- `NetServerInterface__HandleUplinkKickCommand` (`0x1002CA70`) всегда пишет pending reason в state текущего player object, но tree entry по `player+0x888` создаёт только если `a5 != 0`;
- сам tree key строится из `player+0x888` (`0x1002CB62`);
- `NetServerInterface__ApplyPendingKickForPlayerName` (`0x10023020`) ищет entry тоже по `player+0x888` (`0x100230BC`), и его xref'ы есть только из `PACKET_ID_PLAYER_JOINDATA`.

Практический вывод:
  - pending-kick tree по `player+0x888` потребляется только на join-time;
- клиентские packet-path в `net.dll` не нашли способа наполнять этот tree для произвольной жертвы;
- ненулевой delay/TTL (`a5`) для tree insert я нашёл только в `UplinkResponse__HandleDecodedMessage` (`0x1000B76E`), то есть во внешнем uplink-control path, а не во входящих игровых пакетах.

## 2. Карта уязвимостей
| Threat | Entry | Core functions | Effect | Оценка времени до пиздеца | Confidence |
| --- | --- | --- | --- | --- | --- |
| `HIGH` | `PACKET_ID_PLAYER_JOINDATA (4)` | `NetServerInterface__HandlePacket_PLAYER_JOINDATA` | malformed/truncated packet даёт info leak/logic corruption primitive из-за unchecked 32-byte stack read перед XOR decode | `1 packet` | `medium-high` |
| `HIGH` | `PACKET_ID_PLAYER_MODINFO (93)` subtype `0x7D..0x82`, затем `PURESYNC/VEHICLE_PURESYNC (32/33)` | `NetServerInterface__HandlePacket_PLAYER_MODINFO`, `ModInfoBucketMap__GetOrCreateListBySubtype`, `ModInfoNodeList__PushRawRecord0x7C`, `NetServerPlayer__FlushDeferredModInfoToUpperLayer` | линейный heap growth + дорогая deferred сериализация; можно загонять сервер в memory/CPU DoS | `десятки секунд - несколько минут` | `high` |
| `MEDIUM` | `PACKET_ID_LIGHTSYNC (34)` | `NetServerInterface__HandlePacket_LIGHTSYNC`, `NetServerInterface__HandleUplinkKickCommand`, `NetServerInterface__HandlePacket_PLAYER_LIST` | nested parser позволяет вызывать дополнительные uplink/kick side effects и synthetic `PLAYER_LIST`, т.е. делать parser/traffic amplification | `минуты при sustained spam` | `medium` |
| `LOW` | `PACKET_ID_COMMAND (42)` | `NetServerInterface__RateLimitPacket` | явная flood-поверхность, но в этом билде уже режется и уходит в kick helper | mitigation срабатывает сразу | `high` |
| `LOW` | `PACKET_ID_CUSTOM_DATA (84)` | `NetServerInterface__RateLimitPacket` | байтовый flood тоже уже под ограничителем и kick'ом | mitigation срабатывает сразу | `high` |

## 3. Глубокий разбор опасных мест
### 3.1 `PLAYER_JOINDATA (4)`: однопакетный info leak / logic-corruption primitive
Самая неприятная штука в `NetServerInterface__HandlePacket_PLAYER_JOINDATA` (`0x10029370`) не в строках, а в раннем helper-пайплайне:

- в `0x100293D9` читается `m_usNetVersion`;
- в `0x100293E7` вызывается `NetBitStream__ReadAlignedBytes(v150, 0x20)`;
- результат этого вызова вообще не проверяется;
- сразу после этого в `0x100293EC` ставится только `v150[32] = 0`;
- затем цикл `0x100293F3..0x10029410` XOR-декодит все 32 байта `v150`, даже если чтение не удалось и буфер остался стековым мусором.

Почему это реально плохо:
- `NetBitStream__ReadAlignedBytes` при недостатке данных просто возвращает `0` и буфер не трогает;
- `v150` заранее не зануляется;
- дальше этот буфер превращается в строку версии/идентификатора клиента и проходит дальше по join/kick/uplink path;
- в зависимости от ветки код либо пишет эту строку в состояние игрока (`0x10029C53`, `0x1002A168`), либо проталкивает её дальше в notify/uplink flow через `NetServerConnectionMap__NotifyUplinkConnect`.

Итог:
- это не выглядит как clean RCE;
- это выглядит как реальный one-packet info leak primitive и частичный logic corruption;
- плюс у этого поля есть кандидат в reflection sink: после записи в `player+0x888` join-path вызывает virtual sender (`vtable+0x38` -> `NetServerInterface__SendAugmentedPacketToPeer`), а case `2` этого sender'а шлёт `player+0x888` обратно в internal packet `PACKET_ID_SERVER_JOIN_COMPLETE (2)` через 16-bit length + payload;
- payload там не XOR'ится обратно, а просто побайтно инкрементится на `+1`, так что это не "чистый serial echo", а отражение получившейся строки в слегка изменённом виде;
- максимальная длина такого отражения ограничена теми же `32` байтами, потому что строка строится как `strlen(v150)` после принудительного `v150[32] = 0`.
- ранний flood-гейт вокруг этого пути теперь тоже подтверждён по конструктору `NetServerInterface__Construct (0x10003990)`: для сырого packet id `4` выставлен per-source-IP budget `100` в ~1s окне, суммарный early-packet budget `200`, penalty block `30000 ms`, а stale tracker entries живут примерно `60000 ms`. То есть массовый сбор сэмплов одним адресом упирается сначала в RTT/reconnect cost, а потом быстро в 30-секундный бан по раннему join/disconnect rate limiter.

Почему confidence не `high-high`:
- по статике видно сам primitive, но я не выполнял live PoC и не наблюдал конкретный байтовый выхлоп с сети.

### 3.2 `PLAYER_MODINFO (93)`: deferred heap/CPU DoS
Ветка `NetServerInterface__HandlePacket_PLAYER_MODINFO` (`0x1002A660`) имеет два режима:

1. subtype `0x60`:
- читает два XOR-encoded string'а и capability bitmask;
- при изменении кэша собирает synthetic `PACKET_ID_PLAYER_DIAGNOSTIC (92)`.

2. subtype `0x7D..0x82`:
- берёт per-subtype list через `ModInfoBucketMap__GetOrCreateListBySubtype` два раза;
- на каждый закрытый raw-record в `0x1002B046..0x1002B065` делает две вставки `ModInfoNodeList__PushRawRecord0x7C`;
- сами list-node аллоцируются по `0x84` байта и practical cap отсутствует: helper падает только на absurd `32537631` элементов.

Почему это DoS:
- записи не отправляются наверх сразу;
- они копятся в deferred state игрока;
- потом `NetServerPlayer__FlushDeferredModInfoToUpperLayer` (`0x1002E020`) на первом `PACKET_ID_PLAYER_PURESYNC (32)` или `PACKET_ID_PLAYER_VEHICLE_PURESYNC (33)` проходит по всему накопленному набору и сериализует его в synthetic `PACKET_ID_PLAYER_MODINFO (93)`;
- flush сам по себе уже дорогой: ходит по дереву, обрабатывает строки, размеры, MD5/SHA256 поля и строит новый bitstream.

Практический сценарий:
- клиент шлёт пачки `93` subtype `0x7D..0x82`, копя deferred records;
- потом дёргает `32/33`, заставляя сервер сериализовать всё накопленное;
- повторяет цикл.

Эффект:
- рост памяти линейный;
- CPU-пики тоже линейные по размеру набитого deferred state;
- сервер, скорее всего, не упадёт с одного packet'а, но под sustained spam вполне можно уложить его за минуты.

### 3.3 `LIGHTSYNC (34)`: nested parser с amplification side effects
`NetServerInterface__HandlePacket_LIGHTSYNC` (`0x1002B0A0`) сначала выглядит как обычный packet handler, но внутри это отдельный вложенный формат:

- длина custom payload читается специальным кодом;
- потом `NetBitStream__HasRemainingBytes` (`0x10022DF0`) проверяет, что длина не больше остатка bitstream;
- затем payload парсится в структуру key/value и дополнительный список integer value'ов;
- дальше возможны side effects:
- `0x1002B766` и `0x1002B839`: вызовы `NetServerInterface__HandleUplinkKickCommand`;
- `0x1002BB72`: synthetic вызов `NetServerInterface__HandlePacket_PLAYER_LIST`.

Что это значит:
- clean memory corruption отсюда по статике я не вижу;
- зато есть parser amplification: один packet может породить ещё один скрытый parser path и лишний uplink/control-plane traffic;
- как point exploit для мгновенного падения сервера это слабее `MODINFO`, но как pressure point для долгого насилия по CPU/uplink выглядит годно.

### 3.4 Что я специально проверил и что не превратилось в дыру
`NetServerInterface__RateLimitPacket` (`0x10005CB0`) уже режет две жирные поверхности:
- `PACKET_ID_COMMAND (42)` ограничен по packet-count и уводит в kick helper при превышении;
- `PACKET_ID_CUSTOM_DATA (84)` ограничен по total byte budget и тоже уводит в kick helper.

Отдельно проверил candidate на giant-length crash в `LIGHTSYNC`:
- длина payload там не берётся "из воздуха";
- `NetBitStream__HasRemainingBytes` требует, чтобы длина была `<=` остатку bitstream;
- значит instant heap bomb на сотни мегабайт именно через этот parser не подтверждается.

### 3.5 Можно ли packet'ом кикнуть другого игрока
По итогам разбора именно packet-only пути через MTA/RakNet internal handlers подтверждения не дал.

Что проверено:
- `LIGHTSYNC` ветки `ka/km` вызывают `NetServerInterface__HandleUplinkKickCommand` только с current player context (`0x1002B762`, `0x1002B835`);
- rate-limit kick path в `NetServerInterface__RateLimitPacket` тоже бьёт по текущему offender player object;
- во всех client-reachable вызовах `HandleUplinkKickCommand`, которые я раскопал внутри `net.dll`, `a5 = 0`, а значит nick-keyed pending-kick tree не наполняется.

Итог:
- live-kick произвольного уже подключённого другого игрока через входящий игровой packet в этом `net.dll` я не подтвердил;
- join-time deny по нику существует как механизм, но в этой DLL он наполняется не игровыми пакетами, а uplink-control path'ом;
- если контролировать uplink/его ответы, можно ставить delayed kick entry по нику и срезать следующий join, но это уже другой threat model, не "клиент шлёт пакет и кикает другого игрока".

## 4. RCE / info-leak / kick assessment
### RCE
- Подтверждённого RCE-примитива в самом `net.dll` не нашёл.
- `NetServerInterface__HandleUplinkKickCommand` не выглядит как format-string или command-injection sink:
- reason string копируется в state;
- дальше идёт length-prefixed bitstream serialization;
- строки не подставляются как формат.
- Самый реалистичный путь к RCE лежит уже не в `net.dll`, а выше, в upper-layer packet readers, которые `net.dll` вызывает через callback.

### Info leak
- Подтверждённый сильный кандидат: `PLAYER_JOINDATA` unchecked read в stack buffer `v150`.
- В `PLAYER_MODINFO` и `LIGHTSYNC` большинство временных буферов либо нулится заранее, либо читается через helper'ы, которые возвращают ошибку без использования stack garbage.
- open-source `PACKET_ID_SERVER_JOINEDGAME (23)` serial/identity field не содержит;
- но в этом бинаре возможный reflection sink сидит раньше, во внутреннем `PACKET_ID_SERVER_JOIN_COMPLETE (2)`.

### Чужой kick
- Через packet-only ingress (`raw RakNet internal + MTA user packets`) не подтвердился примитив, который позволяет клиенту кикнуть произвольного уже живого другого игрока.
- Подтверждён только self-targeted kick/transgression path и отдельный uplink-controlled delayed join deny по нику.

## 5. Evidence
- `0x100243C0` `NetServerInterface__ProcessIncomingPackets`
- главный ingress-loop;
- raw `1..98` -> `NetServerInterface__DispatchRakNetInternalPacket`;
- raw `>= 99` -> `PACKET_ID = raw - 99`.
- `0x10038600` `NetServerInterface__DispatchRakNetInternalPacket`
- raw `17/19/20/33/34` -> synthetic upper packets `3/5/6/102/103`.
- `0x10007850` `NetServerInterface__ShouldThrottleEarlyJoinOrDisconnectPacket`
- special early gate только для `PACKET_ID_PLAYER_JOINDATA (4)` и `PACKET_ID_DISCONNECT_MESSAGE (90)`.
- constructor-seeded defaults: raw `4` budget = `100`, raw `90` budget = `500`, combined early-packet budget = `200`, penalty block = `30000 ms`, stale entry expiry ~= `60000 ms`.
- `0x10029370` `NetServerInterface__HandlePacket_PLAYER_JOINDATA`
- unchecked `NetBitStream__ReadAlignedBytes(v150, 0x20)` at `0x100293E7`;
- XOR decode over possibly uninitialized `v150` at `0x100293F3..0x10029410`.
- `0x1002A660` `NetServerInterface__HandlePacket_PLAYER_MODINFO`
- subtype `0x60` -> synthetic `PACKET_ID_PLAYER_DIAGNOSTIC (92)`;
- subtype `0x7D..0x82` -> deferred record accumulation.
- `0x1002B046` / `0x1002B065`
- каждый raw modinfo record добавляется в обе deferred lists.
- `0x10037480` `ModInfoNodeList__PushRawRecord0x7C`
- аллокация `0x84` на node;
- practical bound отсутствует.
- `0x1002E020` `NetServerPlayer__FlushDeferredModInfoToUpperLayer`
- на первом `32/33` сериализует весь deferred modinfo state в synthetic `93`.
- `0x1002DBF0` `NetServerPlayer__EmitDeferredACInfoToUpperLayer`
- serializes deferred AC bits/hashes в synthetic `104`.
- `0x1002B0A0` `NetServerInterface__HandlePacket_LIGHTSYNC`
- nested custom parser;
- optional synthetic `PLAYER_LIST`;
- uplink kick side effects.
- `0x1002CA70` `NetServerInterface__HandleUplinkKickCommand`
- пишет pending reason в player state всегда;
- nick-keyed tree insert делает только при `a5 != 0`.
- `0x10023020` `NetServerInterface__ApplyPendingKickForPlayerName`
- единственный consumer nick-keyed tree;
- xref'ы только из `PLAYER_JOINDATA`, то есть effect = join-time deny, а не live-kick connected peer.
- `0x10004E90` `NetServerInterface__QueuePlayerNotice`
- использует `player+0x888` в форматируемом сообщении;
- но более поздняя сверка с join parser показывает, что это именно ранний 32-byte join field, по смыслу ближе к `m_strSerialUser`.
- `0x10031C50` `NetServerInterface__SendAugmentedPacketToPeer`
- internal packet builder;
- case `2` (`PACKET_ID_SERVER_JOIN_COMPLETE`) отражает `player+0x888` обратно клиенту через `uint16 length + data`, предварительно делая `byte++` для каждого символа.
- `0x10005CB0` `NetServerInterface__RateLimitPacket`
- built-in mitigation для `42` и `84`.

## 6. Hypotheses
- Raw bytes `33/34` в этом билде используются как player no-socket / network-status notifications, хотя stock `packetenums.h` comments на этих значениях описывают уже другую RakNet plugin-семантику. Это либо кастом MTA build, либо локальная модификация сетевой библиотеки.
- Если искать именно `instant crash` или `RCE`, следующий логичный слой аудита уже не `net.dll`, а upper-layer handlers для:
- `PACKET_ID_COMMAND (42)`
- `PACKET_ID_CUSTOM_DATA (84)`
- `PACKET_ID_LUA_EVENT (81)`
- `PACKET_ID_PLAYER_BULLETSYNC (98)`
- `PACKET_ID_WEAPON_BULLETSYNC (100)`
- `PLAYER_JOINDATA` leak стоит добить динамикой: по статике primitive есть, но live capture покажет, выходит ли стековый мусор наружу стабильно.
- Если интерес именно в "клиент кикает другого клиента", следующий слой аудита уже не `net.dll`, а то, как внешний uplink/auth/control plane аутентифицирует команды `kick`.

# Next Steps
1. Сделать live PoC на malformed/truncated `PACKET_ID_PLAYER_JOINDATA (4)` и посмотреть, выходит ли decoded `v150` в лог/uplink/отладочный канал.
2. Написать нагрузочный сценарий на `PACKET_ID_PLAYER_MODINFO (93)` subtype `0x7D..0x82` + `PURESYNC (32)` для замера:
- heap growth на игрока;
- latency flush;
- сколько времени нужно до заметной деградации/смерти.
3. После этого добивать уже upper-layer packet readers, а не сам `net.dll`, потому что сетевой shell здесь в основном routing/validation слой.

# Changelog
- 2026-03-11: полностью размечен ingress `net.dll`, найдены hidden raw/internal handlers, dedicated parser'ы `JOINDATA/MODINFO/LIGHTSYNC`, synthetic `91/92/93/104`, подтверждён один сильный info-leak primitive и один сильный deferred heap/CPU DoS path.
- 2026-03-11: отдельно добита nick-keyed kick chain; подтверждено, что `player+0x888` это ник, `ApplyPendingKickForPlayerName` живёт только на join-path, а packet-only live-kick чужого уже подключённого игрока через `net.dll` не подтвердился.
- 2026-03-11: уточнена семантика `player+0x888`: это ранний 32-byte serial-like join field (аналог `m_strSerialUser`), а не подтверждённый ник; найден кандидат в reflection через internal `PACKET_ID_SERVER_JOIN_COMPLETE (2)`.
- 2026-03-11: подтверждены точные дефолты раннего join/disconnect flood-гейта: `JOINDATA=100/source IP/~1s`, `DISCONNECT_MESSAGE=500/source IP/~1s`, combined early budget `200`, temporary block `30000 ms`, stale tracker expiry около `60000 ms`.
