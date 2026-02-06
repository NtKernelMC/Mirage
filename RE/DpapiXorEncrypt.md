# RE: DPAPI + XOR шифр/дешифр (BuildHwKeyString / XorWithHwKey / DecryptDpapiXorToString)

## Контекст
- Бинарь: `netc.dll`
- База: `0x10000000`
- IDB: `C:\Province Games\MTA\netc.dll`
- AGENTS.md в корне проекта не найден, цели взяты из текущего запроса.

## Метод
- Декомпиляция: `DecryptDpapiXorToString` (0x10075CC0), `XorWithHwKey` (0x10088EB0), `BuildHwKeyString` (0x1007D330)
- Декомпиляция зависимостей: `ReadMachineGuid` (0x1007B650), `ReadProductId` (0x1007E080), `ReadBaseBoardProduct` (0x1007A510), `sub_101E6C60` (0x101E6C60)
- Дизассемблирование критических участков (DPAPI вызов, XOR цикл)

## Наблюдения
1) DPAPI-раскрытие входного blob
- `DecryptDpapiXorToString` вызывает `CryptUnprotectData` через резолвер `sub_10089150`.
- Параметры: pDataIn = локальный DATA_BLOB из входной строки, pOptionalEntropy = NULL, pPromptStruct = NULL, dwFlags = 0x1 (CRYPTPROTECT_UI_FORBIDDEN), pDataOut = hMem. Адрес вызова: 0x10075D91; подготовка аргументов видна на 0x10075D78–0x10075D8C.
- Результат копируется в локальный `Block` через `EnsureBufferAndCopy` (0x10075DA0), затем `LocalFree`.

2) XOR-постобработка результата DPAPI
- `XorWithHwKey` сначала копирует `Src` в `a1` (std::string конструктор на 0x10088F3B–0x10088F50), затем XOR’ит байты `a1` циклическим ключом.
- Цикл XOR: 0x10088F82, индекс по ключу рассчитывается `i % key_len` (0x10088F8A).

3) Формирование ключа для XOR
- `BuildHwKeyString` собирает ключ через формат `"%s-%s-%s"` (0x1007DC16):
  - `MachineGuid` из `HKLM\SOFTWARE\Microsoft\Cryptography` (ReadMachineGuid, 0x1007B650)
  - `ProductId` из `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion` (ReadProductId, 0x1007E080)
  - `BaseBoardProduct` из `HKLM\HARDWARE\DESCRIPTION\System\BIOS` (ReadBaseBoardProduct, 0x1007A510)
- Чтение значений делает `sub_101E6C60` (0x101E6C60): формирует строки ключ/значение, затем читает реестр и приводит результат к std::string.

4) Фоллбеки, если значение отсутствует/пустое
- Если `MachineGuid` пуст, используется строка `"908a62fa-a205-4dd6-936c-d53e49bb3ca2"` (константа, копирование в 0x1007D5CE; создание на 0x1007D3FD).
- Если `ProductId` пуст, используется `"a745d-ecd77-b2471-cb0b7"` (копирование в 0x1007D85E; создание на 0x1007D685).
- Если `BaseBoardProduct` пуст, используется `"To be filled by O.E.M."` (копирование в 0x1007DADE; создание на 0x1007D908).

## Вывод (алгоритм)
- Дешифрование:
  1) CryptUnprotectData(blob, flags=0x1)
  2) XOR результата ключом `MachineGuid-ProductId-BaseBoardProduct` (циклично)
- Обратная операция (шифрование): XOR входного текста тем же ключом, затем CryptProtectData с флагом 0x1.

## Изменения в IDB
- Переименованы функции:
  - 0x10075CC0 -> DecryptDpapiXorToString
  - 0x10088EB0 -> XorWithHwKey
  - 0x1007D330 -> BuildHwKeyString
  - 0x1007B650 -> ReadMachineGuid
  - 0x1007E080 -> ReadProductId
  - 0x1007A510 -> ReadBaseBoardProduct
- Добавлены комментарии:
  - 0x10075D91: параметры CryptUnprotectData и флаг 0x1
  - 0x10088F82: XOR циклическим ключом
  - 0x1007DC16: формат ключа "%s-%s-%s"

## Открытые вопросы
- Требуется ли для сохранения совместимости точная кодировка строк реестра (ANSI/UTF-8) при формировании ключа.

## Следующие шаги
- Подтвердить, как вызывающая сторона хранит DPAPI blob (raw bytes vs base64/hex), чтобы правильно сериализовать результат шифрования.
