# GeneratePrivateSerial

## Контекст
- Бинарь: netc.dll (base 0x10000000, size 0x65d000)
- AGENTS.md в корне проекта не найден, поэтому ориентируюсь на запрос пользователя.
- Ключевые функции: GeneratePrivateSerial (0x100659B0), GetPrivateSerial (0x10069D60), InitializePrivateSerialGen (0x1006C1A0).

## Метод
- Декомпиляция GeneratePrivateSerial / GetPrivateSerial / InitializePrivateSerialGen / CollectPrivateSerialData / SerialInfo_CollectWMIData.
- Поиск xrefs и цепочек вызовов.

## Главное (почему MAC/WMI не меняют private serial сейчас)
- `GetPrivateSerial` **не пересобирает** приватный серийник. Он читает **сохраненный** (кэш/хранилище) через `PrivateSerial_LoadCached` (0x1008AD80) и просто расшифровывает буфер (`sub_1008A870`). См. 0x10069D60.
- Реальная генерация выполняется в `InitializePrivateSerialGen` (0x1006C1A0): вызывает `GeneratePrivateSerial`, затем `CryptSavePrivateSerial` (0x1006E9C0), который шифрует и пишет в хранилище через `sub_1008B430` (см. 0x1006EAC7).
- `InitializePrivateSerialGen` вызывается в `NetworkManager_Initialize` (0x10024327). Если хук ставится **после** инициализации сети/античита — приватный серийник уже сгенерен и сохранен **старым** значением, и дальше `GetPrivateSerial` возвращает его неизменно.

## Коллекторы и участие в генерации
- `GeneratePrivateSerial` (0x100659B0):
  - вызывает `CollectPrivateSerial_ExtraData` (0x1001E8F0) — внутри используются `CollectCpuIdData` (0x1001DEE0) и `CollectSmbiosData` (0x1001FA40).
  - циклом вызывает `CollectPrivateSerialData` (0x1006B710) для a2=0..6 (WMI‑сбор и доп. фильтры).
  - отдельно дергает `CollectAdapterMacHexByName` (0x100686B0) — MAC входит в итог. См. 0x100662E3.
- `CollectPrivateSerialData` (0x1006B710):
  - вызывает `SerialInfo_CollectWMIData` (0x1006CBF0), которая берет WMI‑значения и прогоняет через `EncryptWmiBuffer` (см. 0x1006D585 / 0x1006D635).
  - данные складываются в выходной буфер через XOR‑кодирование (тот же шаблон, что и в строках netc).

## Вывод
Хуки на WMI/MAC работают **только если** `GeneratePrivateSerial` запускается после подмены. Сейчас `GetPrivateSerial` берет **сохраненный** приватный серийник и не запускает коллекторы, поэтому вы видите старый private serial в packet ID 4.

## Что нужно для эффекта
- Либо ставить хуки **до** `NetworkManager_Initialize` (до `InitializePrivateSerialGen`).
- Либо принудительно сбросить/перегенерировать приватный серийник (вызвать `InitializePrivateSerialGen` заново или очистить сохраненное значение).

## Изменения в IDB
- Переименованы функции: `GeneratePrivateSerial`, `GetPrivateSerial`, `InitializePrivateSerialGen`, `CryptSavePrivateSerial`, `PrivateSerial_LoadCached`.
- Добавлены комментарии на ключевых вызовах (0x10069D6A, 0x1006C22C, 0x100662E3, 0x1006BB82, 0x1006EAC7).

## Открытые вопросы
- Точное расположение хранилища (куда пишет `sub_1008B430`) — вероятно, внутренний конфиг/реестр/файл. Требуется доп. анализ функции `sub_1008B430`.
