# RE: Serial_GenerateNT2SerialNumber (cred token vs HKCU Access)

## Контекст
- Бинарь: `netc.dll`
- База: `0x10000000`
- IDB: `C:\Province Games\MTA\netc.dll`
- AGENTS.md в корне проекта не найден.

## Метод
- Декомпиляция `Serial_GenerateNT2SerialNumber` (0x10072080).
- Дизассемблирование `sub_1007A3A0` (0x1007A3A0) для восстановления строк.
- Проверка цепочки `GetCredentialFromWindowsStore` > `DecryptDpapiXorToString`.

## Наблюдения
1) Чтение креденшала и сравнение с HKCU
- `GetCredentialFromWindowsStore` вызывается в `Serial_GenerateNT2SerialNumber` (0x100721FC), результат копируется в локальный `v474`.
- Сразу после этого сырой blob сравнивается с `v465`, полученным из `sub_1007A3A0` (0x100720E5..0x100722B? в рамках той же функции).

2) Что такое `v465` (sub_1007A3A0)
- `sub_1007A3A0` декодирует строки XOR?ом 0x93 и вызывает `sub_101E6C60` с корнем `HKEY_CURRENT_USER` (0x80000001).
- Декодированные строки (из дизассемблера 0x1007A3BF..0x1007A4C1):
  - Ключ: `Software\Microsoft\Windows\CurrentVersion\Explorer\CLSID2\{871C5380-42A0-1069-A2EA-08002B30309D}\ShellFolder`
  - Значение: `Access`
- То есть `v465` — содержимое HKCU\...\ShellFolder\Access.

3) Дальше идет DPAPI+XOR расшифровка нескольких blob
- `DecryptDpapiXorToString` вызывается для `v465`, `v444`, и для creds?blob (`&v474`).
- Расшифрованный creds?blob сохраняется в `v442`.

4) Точные точки вызовов `DecryptDpapiXorToString` и аргументы (по дизассемблеру)
- Вызов #1: 0x10072ADC — `push &v474` (a1 = `[ebp-2Ch]`), `push &v465` (Src = `[ebp-140h]`). Результат копируется в `v442` (`[ebp-244h]`) сразу после возврата.
- Вызов #2: 0x10072B5E — `push &v474` (a1 = `[ebp-2Ch]`), `push &v444` (Src = `[ebp-1F8h]`). Результат копируется в `v440` (`[ebp-22Ch]`).
- Вызов #3: 0x10072BF3 — `push &v474` (a1 = `[ebp-2Ch]`), `push &v474`? (Src = `[ebp-104h]` — копия креденшал blob после `GetCredentialFromWindowsStore`). Результат копируется в `v438` (`[ebp-214h]`).

## Вывод
- `SSO_RND_Device` — это **blob**, который сравнивается с HKCU\...\ShellFolder\Access.
- Расшифровка проходит через DPAPI (CryptUnprotectData) + XOR?ключ на базе MachineGuid/ProductId/BaseBoardProduct.
- Публичный serial в netc.dll берётся другим путём (`GetPublicSerial`/`PrivateSerial_LoadCached`), а не напрямую из Credential Manager.

## Изменения в IDB
- Только анализ, изменений не вносил.

## Открытые вопросы
- Где формируется и пишется HKCU\...\ShellFolder\Access (похоже на скрытое хранилище DPAPI?blob).
- Где именно формируется/пишется `SSO_RND_Device` (CredWriteW/эквивалент не найден в netc.dll).