# Objective
Найти адрес функции `bool CPacketHandler::ProcessPacket(unsigned char ucPacketID, NetBitStreamInterface& bitStream)` в IDA Pro для `client.dll`.

# Scope
- IDA database: `C:\Program Files (x86)\UKRAINEGTA\game\mods\deathmatch\client.dll.i64`
- Base address: `0x10000000`
- Source reference: `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Client\mods\deathmatch\logic\CPacketHandler.cpp`

# Method
1. Подтвердил исходную сигнатуру в `CPacketHandler.cpp`.
2. Нашёл входной вызов в `CClientGame::StaticProcessPacket`.
3. Проверил целевую функцию через декомпиляцию и сопоставил switch/callees с исходником.

# Findings
- Искомая функция находится по адресу `0x100DB8D0`.
- В IDA она была без имени как `sub_100DB8D0`.
- Я переименовал её в `CPacketHandler__ProcessPacket`.
- Это именно `CPacketHandler::ProcessPacket`, потому что:
  - `CClientGame::StaticProcessPacket` на `0x10059570` вызывает `sub_100DB8D0` через поле `m_pPacketHandler`.
  - Внутри `sub_100DB8D0` сразу вызывается `m_pNetAPI->ProcessPacket(...)`, как и в исходнике.
  - Дальше идёт большой `switch(ucPacketID)` с вызовами `Packet_ServerConnected`, `Packet_ServerJoined`, `Packet_PlayerList`, `Packet_PlayerChangeNick`, `Packet_ResourceStart`, `Packet_ResourceClientScripts` и fallback в `CUnoccupiedVehicleSync::ProcessPacket` и `CRPCFunctions::ProcessPacket`, что совпадает с логикой исходника.

# Evidence
- Source signature:
  - `C:\Users\VanillaTempest\Desktop\mtasa-blue-1.6.0\Client\mods\deathmatch\logic\CPacketHandler.cpp:34`
- Caller:
  - `CClientGame::StaticProcessPacket` at `0x10059570`
  - Final call: `call sub_100DB8D0`
- Target callee matches:
  - `0x100D6FE0` `CPacketHandler::Packet_ServerConnected`
  - `0x100D8D90` `CPacketHandler::Packet_ServerJoined`
  - `0x100D42B0` `CPacketHandler::Packet_PlayerList`
  - `0x100D3DC0` `CPacketHandler::Packet_PlayerChangeNick`
  - `0x100D5C90` `CPacketHandler::Packet_ResourceStart`
  - `0x100D59E0` `CPacketHandler::Packet_ResourceClientScripts`
  - `0x1010ABA0` `CUnoccupiedVehicleSync::ProcessPacket`
  - `0x1021D110` `CRPCFunctions::ProcessPacket`

# Hypotheses
- Попытка навесить красивую C++-сигнатуру через IDA type API не применилась автоматически, скорее всего из-за отсутствующих/неполных локальных type definitions для класса `CPacketHandler` или `NetBitStreamInterface`. На сам адрес это не влияет.

# Next Steps
- Если хочешь, следующим сообщением могу:
  - найти точный оффсет относительно базы (`client.dll + 0xDB8D0`);
  - разметить аргументы/типы в IDA до нормального вида;
  - пройтись по packet IDs и расписать какие кейсы за что отвечают.

# Changelog
- 2026-03-09: Найден адрес `CPacketHandler::ProcessPacket` -> `0x100DB8D0`, функция переименована в IDA.
