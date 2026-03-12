# Objective

Разобрать все ранее неподписанные caller'ы к `AC_ResolveRuntimeImport` в `netc.dll`, переименовать их по смыслу в IDA и проверить покрытие plaintext-комментами для xor-импортов.

# Scope

- Точка входа: `AC_ResolveRuntimeImport` at `0x101E0EC0`
- Только caller-функции, которые до прохода оставались `sub_*`
- В рамках строк: только xor-импорты, проходящие через `AC_ResolveRuntimeImport`

# Method

1. Собрал все xref'ы к `AC_ResolveRuntimeImport`.
2. Отфильтровал только ранее неподписанные caller'ы.
3. Разобрал функции по псевдокоду, callsite-комментам и набору резолвимых API.
4. Выполнил batch rename в IDA.
5. Перепроверил, что неподписанных caller'ов больше не осталось.
6. Проверил покрытие callsite-комментами вида `Import: DLL -> API`.

# Findings

1. Все ранее неподписанные caller'ы к `AC_ResolveRuntimeImport` подписаны.
   - До прохода: `145` функций с именами `sub_*`
   - После прохода: `0`

2. Всего у `AC_ResolveRuntimeImport` сейчас `184` уникальных caller'а.
   - Из них `145` были переименованы в этом проходе
   - Остальные уже были подписаны в базе

3. Все xor-импорты на callsite уже покрыты plaintext-комментами.
   - Проверка показала `340/340` вызовов `AC_ResolveRuntimeImport` с комментом
   - Формат: `Import: <dll> -> <api>`
   - Это закрывает именно импортные xor-строки в рамках текущего прохода

4. Самые полезные новые имена легли в несколько кластеров.
   - Импорт-резолверы: `AC_ResolveCreateToolhelp32Snapshot`, `AC_ResolveGetCurrentProcess`, `AC_ResolveNtQueryInformationFile`, `AC_ResolveVirtualProtect`, `AC_ResolveK32EnumProcessModules`, `AC_ResolveRtlDosPathNameToNtPathName_U`
   - Процессы/модули/память: `AC_CollectRunningProcessIds`, `AC_EnumerateProcessesToolhelp`, `AC_EnumerateProcessModulesToolhelp`, `AC_UpdateProcessWatchList`, `AC_ScanRemoteProcessMemory`, `AC_GetProcessImagePath`
   - Драйверы/тома/реестр/сервисы: `AC_EnumerateLoadedDeviceDrivers`, `AC_EnumerateLogicalDrives`, `AC_OpenRegistryKeyForScan`, `AC_WriteRegistryValue`, `AC_CreateAndStartService`, `AC_StopAndDeleteService`
   - Версии/ресурсы/логирование: `AC_QueryFileVersionStringA`, `AC_QueryFileVersionMetadata`, `AC_LoadEmbeddedResourceBlob`, `AC_ReadEventLogEntries`, `AC_ReportProcessScanFindings`

5. Часть крупных функций названа с хорошей, но не максимальной уверенностью.
   - Высокая уверенность: чистые import-resolver'ы и короткие WinAPI wrapper'ы
   - Средняя уверенность: `AC_HandleProcessScanState`, `AC_ScanDirectoryAndReportMatches`, `AC_ScanDirectoryMetadata`, `AC_ReportProcessScanFindings`, `AC_CheckCurrentProcessCapability`
   - Причина: тяжелый decompiler output, неполные типы и плотная string-deobfuscation логика без полного type recovery

# Evidence

- `0x101E0EC0` -> `AC_ResolveRuntimeImport`
- Примеры новых имен:
  - `0x1004FA20` -> `AC_ResolveCreateToolhelp32Snapshot`
  - `0x10057FE0` -> `AC_CollectRunningProcessIds`
  - `0x1013ADD0` -> `AC_EnumerateProcessModulesToolhelp`
  - `0x10183D10` -> `AC_UpdateProcessWatchList`
  - `0x101BD5F0` -> `AC_CreateAndStartService`
  - `0x101EA7A0` -> `AC_ReadEventLogEntries`
- Примеры проверенных callsite-комментов:
  - `0x1004FA7D` -> `Import: Kernel32.dll -> CreateToolhelp32Snapshot`
  - `0x10058043` -> `Import: Kernel32.dll -> K32EnumProcesses`
  - `0x101C52F3` -> `Import: Ntdll.dll -> NtClose`
  - `0x101EBE47` -> `Import: Ntdll.dll -> NtQueryInformationFile`

# Hypotheses

1. В нескольких крупных anti-cheat функциях остаются дополнительные не-import xor-строки через `AllocateStringDexorBuffer` / `CreateStringFromDexoredData`.
2. Эти строки не мешали закрыть задачу по xref'ам к `AC_ResolveRuntimeImport`, но для полного string-annotation прохода нужен отдельный дедикейтед пасс.

# Next Steps

1. Сделать отдельный проход по всем `AllocateStringDexorBuffer` внутри medium-confidence функций и расставить plaintext-комменты уже для не-import строк.
2. Добить type recovery для сервисных и process-watchlist структур, чтобы снять часть `medium confidence` имен.
3. Если нужно, выгрузить полный old-name -> new-name список всех `145` переименованных функций в отдельный markdown appendix.

# Changelog

- `2026-03-10`: все `145` ранее неподписанных caller'ов к `AC_ResolveRuntimeImport` переименованы; проверено отсутствие остаточных `sub_*`; подтверждено покрытие `340/340` callsite-комментами для xor-импортов.
