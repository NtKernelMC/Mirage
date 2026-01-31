# RE: GetCredentialFromWindowsStore / LoadCredReadW

## Контекст
- Бинарь: `netc.dll`
- База: `0x10000000`
- Хэш (SHA-256): `f44e87d150a49aa81bbed2c789db78c6af57564411f207f19f6c5350daf1088b`
- AGENTS.md: не найден в корне репозитория (ориентировался на наблюдаемую цель анализа).

## Метод
- Декомпиляция функций `GetCredentialFromWindowsStore` (0x100794B0) и `LoadCredReadW` (0x10088FF0).
- Проверка статических данных и декодирование имени креда скриптом IDA Python.

## Наблюдения и выводы (с адресами)
1) **LoadCredReadW резолвит Advapi32!CredReadW через рантайм-импорт.**
   - Если `pCredReadW == 0`, строятся обфусцированные строки и вызывается `AC_ResolveRuntimeImport`, результат кладётся в `pCredReadW`. 
   - Адреса: 0x1008900A–0x10089047.

2) **Имя креда разворачивается из 14 обфусцированных байт и даёт строку `SSO_RND_Device`.**
   - Обфусцированные байты берутся из `xmmword_1036FF30` (адрес 0x1036FF30). 
   - Формула в скалярном fallback-цикле: 
     `out[i] = (obf[i] ^ (3 - i) - i*i) & 0x7F` для `i = 0..13`.
   - Адреса: 0x10079697–0x100796BC.
   - Декодированный результат: `SSO_RND_Device`.

3) **Чтение из Windows Credential Manager идёт через CredReadW.**
   - Вызов: `CredReadW(name, CRED_TYPE_GENERIC (1), 0, &pCredential)`.
   - Адреса: 0x10079799–0x100797AD.
   - При ошибке функция возвращает пустую внутреннюю строку/буфер (обнуление структуры). Адреса: 0x100798DD–0x100798FF.

4) **Данные трекинга берутся из `CREDENTIAL.CredentialBlob` без дальнейшего разбора.**
   - `CredentialBlob` читается по смещению `+28`, размер по смещению `+24` структуры `CREDENTIAL`.
   - Копирование в выходной буфер: `EnsureBufferAndCopy(credentialBuffer, blob, size)`.
   - Освобождение: `CredFree(pCredential)` (через `pCredFree`).
   - Адреса: 0x100797CC–0x10079840.

## Что это такое (по коду)
- Это **локальный токен устройства/установки**, сохранённый в Windows Credential Manager под именем `SSO_RND_Device` как `CRED_TYPE_GENERIC`.
- В рамках **этих** функций токен **не интерпретируется**: он копируется как непрозрачный байтовый blob.

## Формат записи
- В рамках `GetCredentialFromWindowsStore` **формат не раскрыт**: функция только читает `CredentialBlob` и возвращает его как есть.
- Чтобы установить формат, нужно найти путь записи (например, `CredWriteW` или собственный сериализатор), либо точку дальнейшего разбора blob.

## Дальнейшая декодировка blob (найдено)
- **DPAPI-раскрытие через CryptUnprotectData** находится в `sub_10075CC0` (0x10075CC0), где вызывается резолвер `sub_10089150` (0x10089150) и далее `CryptUnprotectData` (через `pCryptUnprotectData`).  
  После успешного вызова результат копируется в локальный буфер `Block` и используется дальше.  
- **После DPAPI идёт XOR-обработка** в `sub_10088EB0` (0x10088EB0): байты целевого буфера `a1` побайтно XOR’ятся с циклическим ключом, сформированным через `sub_1007D330` (0x1007D330) + `sub_1002ED60` (0x1002ED60).  
- **Связь с GetCredentialFromWindowsStore подтверждена по потоку управления:**  
  - `AC_SystemStateValidator_ComplexTelemetry`: `GetCredentialFromWindowsStore` (0x10070BD9) → `sub_10075CC0` (0x10071466), аргумент `a1` = тот же стековый буфер `[ebp-2Ch]`.  
  - `Serial_GenerateNT2SerialNumber`: `GetCredentialFromWindowsStore` (0x100721FC) → `sub_10075CC0` (0x10072ADC), аргумент `a1` = тот же `[ebp-2Ch]`.  
## Изменения в IDB
- Добавлены комментарии в местах декодирования имени креда, вызова `CredReadW` и чтения `CredentialBlob`.

## Открытые вопросы
- Где формируется/записывается `CredentialBlob` (поиск `CredWriteW` или эквивалента)?
  - В `netc.dll` **не найден** импорт `CredWriteW/A` и строка `CredWrite` (по списку импортов и строк), а в `Serial_GenerateNT2SerialNumber` виден только путь чтения → `sub_10075CC0` (0x10072ADC). Вероятно, запись выполняется в другом модуле/компоненте.
- Есть ли последующая декодировка/проверка содержимого blob?
