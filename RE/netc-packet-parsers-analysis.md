# Objective
- Проаудировать все обработчики входящих пакетов в `netc.dll`, которые реально парсят или предобрабатывают данные внутри `netc.dll`.
- Отдельно понять, есть ли в `netc.dll` скрытые парсеры/валидаторы до передачи пакета в открытый клиентский `CPacketHandler`.
- Отдельный фокус: пакеты, которые другой игрок может косвенно вызвать через серверный релей в зоне стриминга.

# Scope
- IDA database: `C:\Province Games\MTA\netc.dll`
- Image base: `0x10001000`
- Architecture: x86 (`metapc`)
- Source references:
  - `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Shared\sdk\net\packetenums.h`
  - `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Shared\sdk\net\Packets.h`
  - `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Server\mods\deathmatch\logic\CPacketTranslator.cpp`
  - `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Client\mods\deathmatch\logic\CPacketHandler.cpp`
- Вне scope:
  - открытые обработчики в `client.dll`/MTA source;
  - функции, которые по xref используются только из `CNet__SendPacket` и не трогают входящий трафик.

# Method
1. Снял карту модуля через IDA MCP: imports, функция-интерфейсы, цепочка приёма UDP.
2. Разобрал входящий тракт `RakNet_ReceiveUDPPacket -> RakNet_ProcessIncomingPacket -> MTA_Process* / CNet__DoPulse`.
3. Собрал все xref на `CNet__RoutePacketToUserHandler` и отфильтровал места, где пакет реально уходит в верхний клиентский handler.
4. Сопоставил голые packet IDs с open-source enum-ами из `packetenums.h` и `Packets.h`.
5. Проверил, какие функции с `Packet` в имени вообще относятся к inbound пути, а какие оказываются outbound-only.

# Findings
## 1. Полная входящая поверхность `netc.dll`
Входящий парсинг в `netc.dll` разбит на четыре слоя:

1. Raw UDP ingress и спецпакеты RakNet/MTA:
   - `0x101F9450` `RakNet_ReceiveUDPPacket`
   - `0x101FCBB0` `RakNet_ProcessIncomingPacket`
   - `0x1020B250` `VerifyPacketIntegrity`
   - `0x101FCE10` `MTA_ProcessSpecialPackets`
   - `0x101FDA70` `MTA_ProcessConnectionRequest`
   - `0x101FD680` `MTA_ProcessACKPacket`
   - `0x101FD200` `MTA_ProcessControlPackets`
   - `0x101FD420` `MTA_ProcessPingPongPacket`

2. Системные/служебные RakNet packet IDs `< RID_USER_PACKET_ENUM`:
   - `0x101F33F0` `sub_101F33F0`

3. Пользовательские MTA packet IDs `>= RID_USER_PACKET_ENUM (99)`:
   - `0x1002BFD0` `CNet__DoPulse`
   - `0x1002D6B0` `CNet__RoutePacketToUserHandler`

4. Точечные предобработчики на отдельных MTA packet IDs:
   - `0x10035810` для `PACKET_ID_SERVER_JOIN_COMPLETE (2)`
   - `0x101A03B0` для `PACKET_ID_PLAYER_JOINDATA (4)`
   - `0x10034B10` для `PACKET_ID_PLAYER_QUIT (5)` и восстановления capability-битмаски после player list/join flow
   - inline-обработка `PACKET_ID_SERVER_JOINEDGAME (22)` внутри `CNet__DoPulse`
   - `0x10034DE0` `AC_PacketProcessor_ReportHandler` для `PACKET_ID_SERVER_DISCONNECTED (23)`
   - `0x10034B10` для `PACKET_ID_PLAYER_LIST (25)`
   - `0x1003C050` `AC_PED_AnimationPacketValidator` для `PACKET_ID_LUA_ELEMENT_RPC (77)`
   - `0x100D6840` для `PACKET_ID_RESOURCE_STOP (83)`
   - suppress-only ветка для `PACKET_ID_PLAYER_SCREENSHOT (94)`

## 2. Первичная картина была неполной: есть ещё inbound handlers до `CPacketHandler`
Дополнительные входящие хендлеры, которые не видны из open-source `CPacketHandler::ProcessPacket`:

- `sub_101F33F0` переводит raw RakNet IDs в MTA IDs:
  - raw `14` -> route как `PACKET_ID_SERVER_JOIN (0)`
  - raw `31` -> route как `PACKET_ID_PACKET_PROGRESS (8)`
  - raw `34` -> route как `PACKET_ID_PLAYER_NETWORK_STATUS (103)`
- raw `19/20/28/29` обрабатываются внутри `netc.dll` как disconnect/lost notification и только логируются/закрывают состояние.
- `MTA_ProcessSpecialPackets` отдельно жрёт низкоуровневые MTA control IDs `1/9/10/15/16/18/22/23/38/95` до того, как что-то вообще попадёт в user handler.

Итог: список “всех входящих обработчиков” для `netc.dll` заметно шире, чем просто switch из открытого `CPacketHandler.cpp`.

## 3. Для большинства пакетов другого игрока в зоне стриминга `netc.dll` ничего не парсит сверх обёртки bitstream
Пакеты, которые типично связаны с другим игроком рядом и которые я проверил по enum-ам:

- `31` `PACKET_ID_PLAYER_KEYSYNC`
- `32` `PACKET_ID_PLAYER_PURESYNC`
- `33` `PACKET_ID_PLAYER_VEHICLE_PURESYNC`
- `37` `PACKET_ID_EXPLOSION`
- `38` `PACKET_ID_FIRE`
- `39` `PACKET_ID_PROJECTILE`
- `56` `PACKET_ID_UNOCCUPIED_VEHICLE_SYNC`
- `59` `PACKET_ID_VEHICLE_DAMAGE_SYNC`
- `60` `PACKET_ID_VEHICLE_TRAILER`
- `63` `PACKET_ID_PED_SYNC`
- `71` `PACKET_ID_VOICE_DATA`
- `98` `PACKET_ID_PLAYER_BULLETSYNC`
- `99` `PACKET_ID_SYNC_SETTINGS`
- `100` `PACKET_ID_WEAPON_BULLETSYNC`
- `101` `PACKET_ID_PED_TASK`
- `106` `PACKET_ID_SERVER_INFO_SYNC`

Для этих ID в `CNet__DoPulse` нет отдельного netc-specific парсера. Они идут по дефолтной ветке:

- строится `NetBitStreamInterface` поверх payload;
- затем вызывается `CNet__RoutePacketToUserHandler`;
- дальше уже начинается вне-scope верхний клиентский код.

То есть если искать именно netc-specific краш у “пакетов от другого игрока в зоне прорисовки”, то поверхность тут очень узкая: почти всё опасное уже находится не в `netc.dll`, а выше.

## 4. Единственный внятный косяк в inbound-парсерах `netc.dll`: недостаточная outer length validation для спецпакетов `0x0A`/`0x26`
Это не draw-distance пакет, а низкоуровневый control traffic, но косяк реальный.

### Что происходит
- `RakNet_ProcessIncomingPacket` (`0x101FCBB0`) для raw packet type `10` и `38` требует только `pkt_len >= 17`.
  - ветка: `case 10` / `case 38`
  - выставляет `v12 = 1`, значит outer gate проверяет только `1 + 16` байт (`type + magic`)
- После этого:
  - `10` уходит в `MTA_ProcessACKPacket` (`0x101FD680`)
  - `38` уходит в `MTA_ProcessPingPongPacket` (`0x101FD420`)

### Почему это плохо
- `MTA_ProcessACKPacket` после `IgnoreBytes(1)` и `IgnoreBytes(16)` без проверки `ReadBits` читает ещё четыре `uint32` подряд.
  - Фактически ей нужен payload как минимум ещё на `16` байт.
- `MTA_ProcessPingPongPacket` после тех же `17` байт читает subtype и, в зависимости от него, ещё `4` или `8` байт.
- Оба обработчика игнорируют результат `ReadBits` и дальше работают с локалами как будто чтение прошло успешно.

### Практический эффект
- Малформед спецпакет может протолкнуть в ACK/ping state machine частично неинициализированные/мусорные значения.
- Это выглядит как минимум как DoS/logic-corruption поверхность на уровне сетевого состояния.
- Прямой clean crash по статике я тут не доказал, поэтому это не “confirmed RCE/instant crash”, а `potential DoS` с `medium confidence`.

### Почему это не про “игрока в зоне”
- Это низкоуровневые control packets до user-level MTA routing.
- Их реалистичный источник: сервер, MitM или отдельный прямой UDP sender, а не обычный nearby player через стандартный игровой релей.

## 5. `PACKET_ID_LUA_ELEMENT_RPC (77)` как раз имеет защитную netc-предвалидацию
`CNet__DoPulse` на `PACKET_ID_LUA_ELEMENT_RPC (77)` перед route вызывает `0x1003C050` `AC_PED_AnimationPacketValidator`.

Что она проверяет:
- вложенный sub-packet `37` (`SET_PED_ANIMATION`) и `38/200`;
- `ucBlockSize` и `ucAnimSize` не должны быть больше `0x3F`.

Если размер больше:
- пакет дропается в `netc.dll`;
- пишется AC log (`0x5A3`/`0x5A4`);
- вверх он не попадает.

Итог:
- это не найденная дыра, а наоборот already-in-place mitigation;
- именно на уровне `netc.dll` для этого кейса crash surface специально подрезана.

## 6. Часть “подозрительных” Packet-функций вообще не относится к inbound-пути
По xref:

- `0x10038630` `AC_Packet42_Scanner`
- `0x1003A690` `AC_ModInfoPacketScanner`
- `0x1003AEE0` `AC_Packet81_AntiCheatScanner`

вызываются из `0x10041CE0` `CNet__SendPacket`, то есть это outbound-only логика. В inbound-аудит их включать не надо, хоть имена и выглядят многообещающе.

# Evidence
- `0x101FCBB0` `RakNet_ProcessIncomingPacket`
  - outer switch по raw packet type;
  - длины для `10/38` проверяются только как `1 + 16`.
- `0x101FD680` `MTA_ProcessACKPacket`
  - после `IgnoreBytes(17)` читает ещё `4 x 32` бита без проверки успеха.
- `0x101FD420` `MTA_ProcessPingPongPacket`
  - после `IgnoreBits(136)` читает subtype и timestamps без проверки успеха.
- `0x1002BFD0` `CNet__DoPulse`
  - именно тут видно, какие MTA packet IDs имеют netc-specific предобработку, а какие сразу route-ятся в user handler.
- `0x101F33F0`
  - подтверждает отдельную обработку raw RakNet packet IDs `14`, `31`, `34`, `19`, `20`, `28`, `29`, `95`.
- `0x1003C050` `AC_PED_AnimationPacketValidator`
  - жёстко режет oversized animation payload before route.
- Source mapping:
  - `Shared/sdk/net/Packets.h` дал точные game-level IDs (`77`, `81`, `83`, `94`, `95`, `98`, `101`, `103`, `106` и т.д.)
  - `Server/mods/deathmatch/logic/CPacketTranslator.cpp` показал, какие player-generated packet classes вообще существуют в MTA source
  - `Client/mods/deathmatch/logic/CPacketHandler.cpp` использовался только как верхнеуровневый ориентир, не как объект аудита.

# Hypotheses
- `0x101A03B0` (`PACKET_ID_PLAYER_JOINDATA`) и `0x10035810` (`PACKET_ID_SERVER_JOIN_COMPLETE`) выглядят как жирные сервер-зависимые handshake/preload парсеры с кучей строковой логики. По статике я не увидел прямого уверенного крэша, но их стоит отдельно подёргать динамически, если нужен threat model “малicious server”.
- `0x10034B10` (`PACKET_ID_PLAYER_LIST`) может быть пригоден скорее для CPU/alloc abuse на join, чем для чистого memory corruption. Подтверждения на краш по статике нет.

# Next Steps
1. Динамически послать укороченные raw `0x0A` и `0x26` пакеты и проверить, можно ли увести `netc.dll` в assert/disconnect/crash, а не только в мусорное состояние.
2. Если интересен именно malicious server, отдельно добить `PACKET_ID_PLAYER_JOINDATA (4)` и `PACKET_ID_SERVER_JOIN_COMPLETE (2)` под WinDbg.
3. Если интересен именно nearby-player attack surface, следующий шаг уже не в `netc.dll`, а в верхних обработчиках `PACKET_ID_PLAYER_PURESYNC`, `PACKET_ID_VEHICLE_PURESYNC`, `PACKET_ID_PED_SYNC`, `PACKET_ID_PLAYER_BULLETSYNC`, `PACKET_ID_WEAPON_BULLETSYNC`, потому что `netc.dll` их почти не трогает.

# Changelog
- 2026-03-10: Полностью разобран inbound path `netc.dll`, найден дополнительный слой pre-handlers до user callback, зафиксирован length-validation gap для спецпакетов `0x0A`/`0x26`.
