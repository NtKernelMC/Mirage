# Objective

Сделать дистилляцию `AC_` функций по ролям в античите, углубить самые мутные `ReportEvent/ProcessEvent` helper'ы через decompile/callees/xrefs, переименовать подтверждённые функции и повесить доказательные function comments в IDA.

# Scope

- Весь текущий `AC_` пул в IDA (`370` функций на момент прохода).
- Приоритет: generic helper'ы с именами вида `AC_ReportEvent*` / `AC_ProcessEvent*`.
- Глубокий разбор в этом проходе: service/SCM recovery cluster, remote-module scan cluster, DOS-device mapping cluster.

# Method

1. Снял полный список `AC_` функций из IDA и разложил его по role buckets по именам, xref-контексту и соседним helper'ам.
2. Отдельно выделил generic event-helper'ы и собрал для них callers/callees/xrefs.
3. Для жирных кластеров прошёл decompile вручную:
   - service create/start/recovery
   - remote-module enumeration / read-check dispatch
   - DOS-device mapping / config-mem reporting
4. Переименовал только те функции, где роль подтверждалась control-flow и соседними вызовами.
5. На все переименованные функции поставил repeatable function comments в IDA с коротким доказательным summary.

# Findings

1. Текущий `AC_` пул уже неплохо раскладывается по ролям даже до полного добивания generic-хвоста.
   - `core_runtime`: `29`
   - `network_packets`: `15`
   - `img_file_integrity`: `21`
   - `window_input_ui`: `13`
   - `process_remote_memory`: `43`
   - `service_driver_scm`: `3`
   - `filesystem_driver_scan`: `8`
   - `imports_nt_wrappers`: `116`
   - `generic_event_helpers`: было `97`, после этого прохода осталось `81`
   - `other`: `25`

2. Generic event-helper'ы уже начали распадаться на реальные подсистемы, а не на безликие log-code контейнеры.
   - service/SCM recovery
   - remote-module scan / chunked read checks
   - DOS-device mapping / config-mem change reporting

3. Подтверждённые переименования этого прохода:
   - `0x101BA910` -> `AC_VerifyServiceBinaryHashesAndTrust`
   - `0x101BBB50` -> `AC_VerifyServiceDeploymentArtifacts`
   - `0x101C0140` -> `AC_QueryServiceStateAndReport`
   - `0x101444C0` -> `AC_RunDirectoryScanAndReportConfigMemChanges`
   - `0x1016E320` -> `AC_ProcessRemoteModuleStateAndQueueReadChecks`
   - `0x10174D10` -> `AC_EnumerateRemoteModulesAndQueueReadChecks`
   - `0x10171050` -> `AC_ReportRemoteModuleStateVariantA`
   - `0x10170690` -> `AC_ReportRemoteModuleStateVariantB`
   - `0x10173D10` -> `AC_ReportRemoteModuleScanFailureAndQueueReads`
   - `0x10176E40` -> `AC_ReportRemoteModuleBranchState`
   - `0x10177590` -> `AC_ReportRemoteModuleFileHashFindings`
   - `0x10142AC0` -> `AC_QueryAndReportDosDeviceMappings`
   - `0x10147420` -> `AC_ReportDosDeviceMappingSummary`
   - `0x10145B20` -> `AC_FormatDosDeviceMappingSummary`
   - `0x101BB6D0` -> `AC_RetryCreateAndStartServiceAndReportFailure`
   - `0x101BEF80` -> `AC_UpdateServiceDaclAndRecoverService`

4. Service cluster теперь читается осмысленно.
   - `AC_VerifyServiceBinaryHashesAndTrust` грузит embedded `BINARY` manifest, хеширует normalized files, сравнивает с embedded blob и отдельно гоняет trust verification.
   - `AC_VerifyServiceDeploymentArtifacts` сидит по xref'ам прямо на `AC_CreateAndStartService` / `AC_StartServiceAndReport` и репортит deployment/artifact path до и после service-start recovery.
   - `AC_QueryServiceStateAndReport` открывает SCM/service, опрашивает `QueryServiceStatus` примерно до 2 секунд и репортит open/status failures.
   - `AC_RetryCreateAndStartServiceAndReportFailure` и `AC_UpdateServiceDaclAndRecoverService` закрывают recovery path поверх service DACL / re-create / re-start.

5. Remote-module cluster тоже стал читаться по роли.
   - `AC_ProcessRemoteModuleStateAndQueueReadChecks` выбирает один из module-state formatter'ов, репортит ветку `0x52F/0x530`, кэширует module path и ставит command `105` на chunked reads.
   - `AC_EnumerateRemoteModulesAndQueueReadChecks` перечисляет remote modules через `AC_EnumerateModulesWithWow64Filter`, вызывает branch/hash/failure helper'ы и гонит дальнейшие reads.
   - `AC_ReportRemoteModuleFileHashFindings` реально считает file hash и сериализует hash в hex перед `0x734/0x735`.

6. DOS-device/config-mem cluster отделился от “просто directory scan”.
   - `AC_RunDirectoryScanAndReportConfigMemChanges` совмещает `AC_ScanDirectoryAndReportMatches` с контролем config-mem snapshot.
   - `AC_QueryAndReportDosDeviceMappings` строит wildcard/device-path state, дёргает `AC_QueryDosDeviceMapping` и отправляет `0x6D2`, `0x8C7`, `0x8C8`.

# Evidence

- Full AC inventory snapshot: `370` functions with `AC_` prefix.
- Generic event helper tail after this pass: `81`.
- `0x101BA910`
  - callers: `AC_CreateAndStartService`, `AC_StartServiceAndReport`
  - key callees: `AC_LoadEmbeddedResourceBlock`, `ComputeFileHash`, `ShaderUtils__RunTrustVerification`
  - sendlogger events: `0x4F5`, `0x4F6`, `0x4F7`
- `0x101C0140`
  - key callees: `AC_ResolveOpenSCManagerA`, `AC_ResolveOpenServiceA`, `AC_ResolveQueryServiceStatus`, `AC_ResolveCloseServiceHandle`
  - sendlogger events: `0x4D7`, `0x4D8`, `0x4D9`, `0x4DA`
- `0x1016E320`
  - key callees: `AC_LogModuleInformation`, `AC_ReportRemoteModuleStateVariantA`, `AC_ReportRemoteModuleStateVariantB`, `sub_10172B70`
  - sendlogger events: `0x52F`, `0x530`
- `0x10174D10`
  - key callees: `AC_EnumerateModulesWithWow64Filter`, `AC_ReportRemoteModuleFileHashFindings`, `AC_ReportRemoteModuleBranchState`, `AC_ReportRemoteModuleScanFailureAndQueueReads`
  - sendlogger events: `0x7FE`, `0x7FF`, `0x800`, `0x801`
- `0x10142AC0`
  - key callees: `AC_QueryDosDeviceMapping`, `AC_ReportDosDeviceMappingSummary`
  - sendlogger events: `0x8C7`, `0x8C8`

# Hypotheses

1. Оставшиеся `81` generic helper'ов не одинаково мутные.
   - часть уже легко садится на process/module/remote-memory cluster
   - часть всё ещё держится только на event-code + близости к кластеру

2. Самые перспективные следующие generic-кластеры:
   - `0x100E0610` / `0x100EC3C0` / `0x100EEB20` / `0x100F17E0`
   - `0x10101800` / `0x10103FF0` / `0x10104860`
   - `0x10111840` / `0x10112720` / `0x101150D0` / `0x10115E30` / `0x10116300`
   - `0x10189C80` / `0x10192000` / `0x101961A0` / `0x10197480` / `0x10199390`

# Next Steps

1. Продолжать давить именно `AC_ReportEvent*` / `AC_ProcessEvent*`, а не уже нормальные named AC helpers.
2. Следующий проход логично делать по remote-memory/process-watchlist cluster.
3. После него закрывать старый `0x100E*` / `0x100F*` self-integrity tail, где ещё много helper'ов с кодовыми именами.

# Changelog

- `2026-03-10`: построена role-distillation карта всего `AC_` пула (`370` функций).
- `2026-03-10`: глубоко разобраны и переименованы service-recovery, remote-module и DOS-device mapping helper'ы; на них добавлены repeatable function comments в IDA.
