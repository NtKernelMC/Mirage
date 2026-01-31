# Serial_GenerateNT2Public / Serial_GenerateNTPublic / CalculateAndAppend4ByteCheckSum

## Контекст
- Бинарь: netc.dll (base 0x10000000, size 0x65d000).
- AGENTS.md в корне проекта не найден, поэтому ориентируюсь на запрос пользователя.
- Целевые функции: Serial_GenerateNT2Public (0x10075E10), Serial_GenerateNTPublic (0x10064950), CalculateAndAppend4ByteCheckSum (0x10087E60), Serial_ConvertBytesToHexString (0x10028B60).

## Метод
- Декомпиляция Serial_GenerateNT2Public / Serial_GenerateNTPublic / CalculateAndAppend4ByteCheckSum / Serial_ConvertBytesToHexString.
- Xrefs/цепочки вызовов для понимания места checksum в общей генерации.

## Цепочка генерации (NT2)
- `Serial_WrapperGenerateNT2` (0x1007E45E) вызывает `Serial_GenerateNT2SerialNumber` (0x10072080), затем телеметрию и сбор private serial.
- Внутри `Serial_GenerateNT2SerialNumber` используется `Serial_GenerateNT2Public` (0x10075E10) как компонент генерации (вызовы через `sub_10078370`).
- `CalculateAndAppend4ByteCheckSum` вызывается **внутри** `Serial_GenerateNT2SerialNumber` на локальной строке (напр. 0x100750B9), а не внутри `Serial_GenerateNT2Public`.

## CalculateAndAppend4ByteCheckSum (0x10087E60)
- Работает по SString (MSVC std::string с SSO): длина по `*(DWORD*)(Src+16)`, capacity по `*(DWORD*)(Src+20)`.
- Алгоритм checksum:
  - acc1/acc2 инициализируются 0.
  - Для каждого байта `b` строки:  
    `acc1 = b + (b ^ acc1) + 2`  
    `acc2 = b + (b ^ acc2) + 8`
  - Далее вызывается `Serial_ConvertBytesToHexString` (0x10028B60), который **добавляет 4 символа** (2 байта → 4 тетрады) в конец строки.
- `Serial_ConvertBytesToHexString` кодирует тетрады как символы `'A'..'P'` (0x41 + nibble), а не как hex `0-9A-F`. Это важный формат checksum-суффикса.

## Serial_GenerateNT2Public (0x10075E10)
Ключевые этапы (по декомпиляции):
- Формирует промежуточную строку из входа `a3` (SString), применяя:
  - побайтовую маску/смещение: `v10 = i ^ byte ^ f(i) ^ 0x31 ^ (1 << (i&7))`;
  - фильтр `isalnum` и таблицу `byte_1036FB38` для нормализации символов.
- Если `a4 > 1`, прогоняет строку через `sub_10062BA0` (нормализация/усечение).
- Если длина < 0x17, делает `Serial_GenerateHashFromMemory` (SHA256) и добавляет хэш-суффикс до 23 байт.
- Затем сложная математика над 16-байтным буфером:
  - циклы умножения/сложения (×36, ×25, ×10, +a2) → формируют 16 байт.
  - строится ключевая строка из констант (`xmmword_1036FAF0/1036F9E0`) и применяется побайтно/побайтно-нойбловая смесь.
  - при `a4 > 1` дополнительно смешиваются первые 8 нибблов со вторыми.
- Финал: 16 байт превращаются в **32 hex-символа `0-9A-F`** (цикл 32 раза).
- Возвращаемая строка **без checksum**.

## Serial_GenerateNTPublic (0x10064950)
- Пайплайн практически идентичен NT2-версии (hash → 16-байт → 32 hex-символа).
- Также возвращает **32 hex-символа**, checksum внутри функции нет.
- Используется из `Serial_GeneratePublic` (0x10066740).

## Почему у тебя краш (наиболее вероятно)
1) **Неправильный объект строки в pGenerateCheckSum.**  
   `CalculateAndAppend4ByteCheckSum` работает с **SString netc.dll** и использует `AllocateAndAlignBuffer/FreeBlock`.  
   Ты передаёшь `SStringX` из своего модуля (std::string с другим CRT/аллокатором).  
   Внутри `Serial_ConvertBytesToHexString` строка может быть переаллоцирована/освобождена `FreeBlock` → **крах из-за несовместимых аллокаторов/ABI**.

2) **Запись 36 байт без обновления длины и без NUL.**  
   В `Hook_GenerateNT2Serial` ты копируешь 36 байт в буфер результата, но не обновляешь поле длины SString (остаётся 32).  
   Также `strncpy(..., 36)` не копирует `'\0'`, перезаписывая старый нуль-терминатор → возможные out-of-bounds чтения.

3) **Алгоритм SerialToEncryptedForm не соответствует реальному NT2.**  
   Реальный `Serial_GenerateNT2Public` генерирует **ASCII hex `0-9A-F`**, а у тебя `SerialToEncryptedForm` выставляет `0x80` и даёт не-ASCII.  
   Это ломает ожидания дальнейших проверок (конвертация/isalnum/hex-парсинг) и может приводить к падению.

4) **Checksum добавляется не там.**  
   В оригинале `CalculateAndAppend4ByteCheckSum` вызывается **внутри Serial_GenerateNT2SerialNumber** (например 0x100750B9).  
   Ты добавляешь checksum внутри `Serial_GenerateNT2Public`, что расходится с реальной цепочкой.

## Что это значит для генерации «по чексумме»
- Чексумма в netc.dll — это 2 байта → 4 символа `'A'..'P'`, вычисленные от **конкретной строки в Serial_GenerateNT2SerialNumber**, а не от сырых 32 hex из NT2Public.
- Если цель — подставить валидный serial, нужно:
  - либо воспроизвести **полную цепочку** NT2 (как в Serial_GenerateNT2SerialNumber),
  - либо аккуратно обновлять именно ту строку, на которую функция реально делает checksum, и корректно обновлять длину SString.

## Изменения в IDB
- Изменений не вносил (только чтение).

## Открытые вопросы
- Какой CRT/компилятор у твоего модуля vs netc.dll. Если отличаются — вызовы netc.dll над твоими std::string почти гарантированно ломают кучу.
- Нужна точная цель: подменить **публичный** serial (32 hex) или **полный** NT2 (с checksum) на сетевом уровне.
