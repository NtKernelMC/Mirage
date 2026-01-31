# Public serial cache / GetPublicSerial / LoadCredReadW

## Контекст
- Бинарь: netc.dll (base 0x10000000, size 0x65d000).
- AGENTS.md не найден.

## Ключевые функции и адреса
- `GetPublicSerial` = 0x1007D2A0  
  - вызывает `PrivateSerial_LoadCached` с декодером `sub_1008A870` и константами `97, 82`.
- `PrivateSerial_LoadCached` = 0x1008AD80  
  - общий кэш‑механизм для строк (public/private/прочее).
- `sub_1008A870` = 0x1008A870  
  - декодирование: `buf[i] = a3 ^ (buf[i] - a4*i)` (XOR + сдвиг).
- `GetCredentialFromWindowsStore` = 0x100794B0  
  - читает Windows Credential Manager через `CredReadW` (объект `SSO_RND_Device`).
- `LoadCredReadW` = 0x10088FF0  
  - динамический импорт `CredReadW`.
- `CNet__SendPacket` = 0x10041DF0  
  - вызывает `GetPublicSerial`, затем сразу обфусцирует серийник для пакета.

## Что реально используется в сетевом пакете
- В `CNet__SendPacket` (0x10041DF0) вызывается `GetPublicSerial` (0x10041DF8), затем серийник проходит XOR/битовую обфускацию и проверку на `0-9A-Z` (0x10041E20..0x10041EB6).  
  => это **точка, которая реально уходит в сеть**, поэтому подмена через `GetPublicSerial` гарантированно влияет на отправляемый public serial.

## Кэш публичного серийника
- `GetPublicSerial` вызывает `PrivateSerial_LoadCached(a1, dword_104D98E0, ..., sub_1008A870, 97, 82)` (0x1007D2BE).  
  Это означает, что public serial **берется из кэша**, а не всегда генерируется заново.
- `PrivateSerial_LoadCached` (0x1008AD80) сравнивает сохраненные блоки и, если они валидны, прогоняет их через декодер `sub_1008A870`, затем копирует в выходную строку.

## Windows Credential Manager
- `GetCredentialFromWindowsStore` (0x100794B0) через `CredReadW` читает запись **SSO_RND_Device** (динамический импорт через `LoadCredReadW`, 0x10088FF0).
- `Serial_GenerateNT2SerialNumber` вызывает `GetCredentialFromWindowsStore` (xref 0x100721FC) и **сравнивает** полученный токен с локально вычисленным/кэшированным буфером.  
  Это может влиять на принятие/отклонение серийника, даже если кэш очищен.

## Почему подмена в GenerateNT2Public не срабатывала
- В сетевой отправке используется **GetPublicSerial → PrivateSerial_LoadCached**, т.е. значение берётся из кэша независимо от генерации `Serial_GenerateNT2Public`.  
- Поэтому хуки на `GenerateNT2Public/NTPublic` могут вообще не влиять на то, что реально уходит в пакете.

## Практический вывод
- Самая прямая точка для подмены public serial — **GetPublicSerial** (до отправки пакета).
- Если нужно полностью исключить восстановление старого значения, надо также рассматривать `GetCredentialFromWindowsStore` и связанные проверки в `Serial_GenerateNT2SerialNumber`.

## Изменения в IDB
- Только анализ, изменений не вносил.

## Открытые вопросы
- Где именно кэш public serial хранится на диске/в реестре (внутренние `dword_104D98E0/…` и `sub_10089D30` требуют отдельного анализа).

## Ответ: где хранится кэш public serial (диск/реестр)
- В текущей ветке `GetPublicSerial -> PrivateSerial_LoadCached -> sub_10089D30` **нет** обращений к реестру/файлам. Вся логика — в памяти: `GetPublicSerial` берёт значение из in-memory карты `dword_104D98FC` по ключу `dword_104D98E0` (0x1007D2BE), `PrivateSerial_LoadCached` ищет ключи через `sub_10089D30` и сравнивает пару `key`/`key+256` (0x1008AD80..0x1008AF3E), а запись в кэш идёт через `sub_1008B430` (0x1008B430) и `sub_10089C40` (0x10089C40).
- `sub_10089D30` — это поиск по дереву (RB-tree/`std::map`-подобная структура): проход по узлам до sentinel-флага `byte+13`, выбор ветки по `node->key` (0x10089D30..0x10089D72). Он **не** делает I/O.
- `dword_104D98E0` — один из 14 случайных “слот-ключей”, генерируемых в `sub_10089D80` (0x10089D80..0x10089FC9). Массив ключей: `dword_104D98C4..dword_104D98F8`, включая `dword_104D98E0` (строки 0x10089EFF..0x10089F5C). Карта `dword_104D98FC` инициализируется в `NetworkManager_Initialize` (0x10024249..0x1002426F) через `sub_10089D80`.
- Регистр используется отдельными обёртками (`sub_100404D0`/`sub_1004E720`) с базой `HKLM\Software\Multi Theft Auto: San Andreas All` (строка в 0x100370E0), но **эти функции не вызываются** из цепочки public-serial cache. Признаков сохранения public serial в реестре/файле в этой ветке не найдено.

Итого: кэш public serial живёт **только в памяти** (дерево `dword_104D98FC`), а не в реестре/на диске — по крайней мере в исследованном пути.
