# Objective

Разобрать прямые xref'ы к `AC__SendLoggerToServerMtaDev`, переименовать подтвержденные `sub_*` caller'ы и проставить нормальные plaintext/dexor-комменты там, где их не было или они были кривыми.

# Scope

- Точка входа: `AC__SendLoggerToServerMtaDev` at `0x1005A1B0`
- Только direct caller-xref'ы к этой функции
- Дополнительно: близкие helper'ы выше по стеку, если без них нельзя уверенно назвать direct caller

# Method

1. Снял полный список direct xref'ов к `AC__SendLoggerToServerMtaDev`.
2. Пересчитал остаток `sub_*` и функции с отсутствующими sendlogger-комментами.
3. Для жирных caller'ов прошелся по decompile/disasm и восстановил dexor-строки из:
   - inline blob'ов через локальные `v76/v77/...`
   - `xmmword_*` констант с тем же декодом `(((3 - i) ^ (blob[i] & 0x7F)) - i*i) & 0x7F`
4. Переименовал только те функции, где смысл подтверждался control-flow, соседними helper'ами и самими строками.
5. Переписал этот отчет под правильную цель вместо старого ложного прохода по `AC__SendLoggerToServerMtaDevIs`.

# Findings

1. У `AC__SendLoggerToServerMtaDev` сейчас `212` уникальных direct caller'ов.
   - После этого прохода `sub_*` осталось `124`
   - На старте corrected-pass было `133`
   - Значит именно direct caller'ов уже отмыто `9`

2. Пустых sendlogger-комментов на direct xref'ах больше не осталось.
   - Раньше было `97` blank callsite в `47` функциях
   - Сейчас blank callsite: `0`
   - Где был локальный dexor blob, поставлен exact decode
   - Где строка собиралась рантаймом без локального dexor перед самым callsite, поставлен честный semantic-коммент с этим фактом

3. Подтвержденные direct caller'ы, которые были переименованы по смыслу:
   - `0x10095130` -> `AC_ProcessExceptionStateChanges`
   - `0x100A6FB0` -> `AC_FixMissingSurfaudDatFile`
   - `0x100CDC70` -> `AC_ReportStateSnapshotCounts`
   - `0x100D9800` -> `AC_FlushQueuedTelemetryReports`
   - `0x10183170` -> `AC_UpdateAndReportProcessWatchList`
   - `0x1015D540` -> `AC_ReportFileVersionKeywordMatches`
   - `0x100CE790` -> `AC_HandleValidationOutcomeAndKick`
   - `0x1010E9E0` -> `AC_ValidateSystemD3DStack`
   - `0x100AB960` -> `AC_GenerateFixedImgDirAndDetectAnomalies`

4. Для file-version telemetry path восстановлены осмысленные маркеры.
   - `AC_ReportFileVersionKeywordMatches` репортит literal'ы `508317A`, `508317B`, `508317D`, `508317C`, `508317E`
   - В той же функции отдельно отправляются маркеры `CLEO`, `RENDER`, `QUANDO`
   - По caller'у выше видно, что речь идет о pending file-version checks, а не о случайном мусоре

5. Для validation dispatcher восстановлены все compact marker string'и на sendlogger-callsite'ах.
   - Репорты `0x72F..0x735` используют короткие control-marker'ы вида `\x01\`\x02`, `\x01\`\x03 [%s]`, `\x01\`\x08`, `\x01\`\x04`, `\x01\`\x05`, `\x01\`\x06`, `\x01\`\x07`
   - Функция после этих репортов уходит в `AC_SetClientKick`, то есть это реально outcome dispatcher для античит-валидации

6. Для D3D path восстановлены нормальные plaintext-комменты вместо пустоты.
   - `0x1010EEA6` -> `Can't find system D3D9.DLL (%s)`
   - `0x1010F206` -> `Can't find Direct3DCreate9 in (%s)`
   - `0x1010F5A6` -> `Can't create IDirect3D9 from (%s)`
   - `0x1010F8F9` -> `Can't create dummy Direct3DDevice9`
   - `0x1010FF30` -> `Can't find D3DXCreateEffectFromFileA in (%s)` with arg `D3DX9_42.dll`

7. Один полезный non-direct helper тоже пришлось назвать, чтобы не гадать по контексту.
   - `0x1015EBE0` -> `AC_ProcessPendingFileVersionChecks`
   - Он обходит список pending entries, строит version-info object, зовет `AC_QueryFileVersionInfoObject`, а потом уже `AC_ReportFileVersionKeywordMatches`

8. `AC_GenerateFixedImgDirAndDetectAnomalies` теперь тоже нормально читается по xref'ам.
   - `0x100AC065` -> `IMGTEST GenerateFixedImgDir YES wrong header tag=%08x numentries=%d tempsize=%d file='%s'`
   - `0x100AC561` -> `IMGTEST GenerateFixedImgDir name clash key=%08x  file='%s'  existing:'%s'  new:'%s'`
   - `0x100AC86E` -> `IMGTEST GenerateFixedImgDir NO wrong header tag=%08x numentries=%d tempsize=%d file='%s'`
   - `0x100ACBD7` -> `IMGTEST GenerateFixedImgDir offset clash offset=%08x  file='%s'  existing:'%s'  new:'%s'`
   - `0x100AD1D9` -> `IMGTEST GenerateFixedImgDir missing key=%08x  file=%s - added dummy with %d,%d`
   - `0x100AE001` и `0x100AE405` -> `Disallowing extra items in img %d (%s)`
   - У `0x100ABCE2` локального dexor blob перед самым вызовом нет, поэтому там стоит semantic-коммент, а не fake-decode

9. `AC_ProcessRequestWithTimeoutHandling` теперь закрыт полностью.
   - `0x100046B5` -> `ERROR_TIMEOUT_RESPONSE_EVENT`
   - `0x10004965` -> `ERROR_ERROR_RESPONSE_EVENT`
   - `0x10004C18` -> `ERROR_TIMEOUT_MUTEX2`
   - `0x10004EC8` -> `ERROR_ERROR_MUTEX2`
   - `0x10005199` -> `ERROR_BUFFER_SIZE_CHANGE2`

# Evidence

- `0x1005A1B0` -> `AC__SendLoggerToServerMtaDev`
- Свежий статус direct xref set:
  - `212` total callers
  - `124` unnamed callers
  - `0` blank callsites without comments

- `0x1015D540` -> `AC_ReportFileVersionKeywordMatches`
  - `0x1015D70A`: `Decoded literal: "508317A" (report 0x85D).`
  - `0x1015D880`: `Decoded literal: "508317B" (report 0x85E).`
  - `0x1015D9D8`: `Decoded literal: "508317D" (report 0x85F).`
  - `0x1015DB60`: `Decoded literal: "508317C" (report 0x860).`
  - `0x1015DD1A`: `Decoded literal: "508317E" (report 0x861).`
  - `0x1015E064`: `Decoded literal: "CLEO" (report 0x862).`
  - `0x1015E1F4`: `Decoded literal: "RENDER" (report 0x863).`
  - `0x1015E31C`: `Decoded literal: "QUANDO" (report 0x864).`

- `0x1015EBE0` -> `AC_ProcessPendingFileVersionChecks`
  - Итерирует список по `this + 196`
  - Для entry с флагом `+44` создает object `0x98`
  - Вызывает `AC_QueryFileVersionInfoObject`
  - Дальше дергает `AC_ReportFileVersionKeywordMatches`

- `0x100CE790` -> `AC_HandleValidationOutcomeAndKick`
  - `0x100CE9C5`: `Decoded literal: "\x01`\x02" (report 0x72F).`
  - `0x100CECF2`: `Decoded format: "\x01`\x03 [%s]" (report 0x730).`
  - `0x100CEFA9`: `Decoded literal: "\x01`\x08" (report 0x731).`
  - `0x100CF1B7`: `Decoded literal: "\x01`\x04" (report 0x732).`
  - `0x100CF305`: `Decoded literal: "\x01`\x05" (report 0x733).`
  - `0x100CF5A0`: `Decoded literal: "\x01`\x06" (report 0x734).`
  - `0x100CF6B6`: `Decoded literal: "\x01`\x07" (report 0x735).`
  - В failing branches вызывает `AC_SetClientKick`

- `0x1010E9E0` -> `AC_ValidateSystemD3DStack`
  - `0x1010EEA6`: `Can't find system D3D9.DLL (%s)`
  - `0x1010F206`: `Can't find Direct3DCreate9 in (%s)`
  - `0x1010F5A6`: `Can't create IDirect3D9 from (%s)`
  - `0x1010F8F9`: `Can't create dummy Direct3DDevice9`
  - `0x1010FF30`: `Can't find D3DXCreateEffectFromFileA in (%s)` with arg `D3DX9_42.dll`

- `0x100AB960` -> `AC_GenerateFixedImgDirAndDetectAnomalies`
  - `0x100AC065`: `IMGTEST GenerateFixedImgDir YES wrong header tag=%08x numentries=%d tempsize=%d file='%s'`
  - `0x100AC561`: `IMGTEST GenerateFixedImgDir name clash key=%08x  file='%s'  existing:'%s'  new:'%s'`
  - `0x100AC86E`: `IMGTEST GenerateFixedImgDir NO wrong header tag=%08x numentries=%d tempsize=%d file='%s'`
  - `0x100ACBD7`: `IMGTEST GenerateFixedImgDir offset clash offset=%08x  file='%s'  existing:'%s'  new:'%s'`
  - `0x100AD1D9`: `IMGTEST GenerateFixedImgDir missing key=%08x  file=%s - added dummy with %d,%d`
  - `0x100AE001`: `Disallowing extra items in img %d (%s)`
  - `0x100AE405`: `Disallowing extra items in img %d (%s)`

- `0x10003D70` -> `AC_ProcessRequestWithTimeoutHandling`
  - `0x10004068`: `ERROR_TIMEOUT_MUTEX1`
  - `0x10004359`: `ERROR_ERROR_MUTEX1`
  - `0x100046B5`: `ERROR_TIMEOUT_RESPONSE_EVENT`
  - `0x10004965`: `ERROR_ERROR_RESPONSE_EVENT`
  - `0x10004C18`: `ERROR_TIMEOUT_MUTEX2`
  - `0x10004EC8`: `ERROR_ERROR_MUTEX2`
  - `0x10005199`: `ERROR_BUFFER_SIZE_CHANGE2`

- Ранее уже закрытые direct caller'ы:
  - `AC_ProcessExceptionStateChanges`
  - `AC_FixMissingSurfaudDatFile`
  - `AC_ReportStateSnapshotCounts`
  - `AC_FlushQueuedTelemetryReports`
  - `AC_UpdateAndReportProcessWatchList`

# Hypotheses

1. `AC__SendLoggerToServerMtaDev` реально является public telemetry API, а не узким internal helper'ом.
   - Из-за этого большинство meaningful anti-cheat producer'ов сидят именно на нем
   - `...DevIs` для полноценного xref-аудита почти бесполезен

2. Следующий жирный хвост теперь уже не про blank-комменты, а про старые кривые decode-комменты с точками.
   - Пустых xref-комментов больше нет
   - Остался cleanup старых dotted-comment'ов, где decode исторически был записан хуево
   - Самые жирные кандидаты сейчас: `sub_100BA670`, `sub_100B8D50`, `sub_1017C1E0`, `AC_ReportProcessScanFindings`

3. Некоторые короткие control-marker'ы вида `\x01...\x02` не являются кривым decode.
   - Это похоже на внутренний compact telemetry format
   - Поэтому я не пытался насильно превращать их в printable ASCII там, где байты уже корректно восстановлены

# Next Steps

1. Следующий логичный проход уже не по пустым, а по кривым legacy decode-комментам в img/file cluster:
   - `sub_100BA670`
   - `sub_100B8D50`
   - `sub_100B1F60`
   - `sub_100B5B10`
   - `sub_100B3320`

2. Если цель именно быстрее выжечь `sub_*`, а не только строки, то затем идти в:
   - `sub_1017C1E0`
   - `sub_100D0140`
   - `sub_10096E60`

# Changelog

- `2026-03-10`: исправлена цель отчета с `AC__SendLoggerToServerMtaDevIs` на `AC__SendLoggerToServerMtaDev`.
- `2026-03-10`: ранее renamed direct caller'ы `AC_ProcessExceptionStateChanges`, `AC_FixMissingSurfaudDatFile`, `AC_ReportStateSnapshotCounts`, `AC_FlushQueuedTelemetryReports`, `AC_UpdateAndReportProcessWatchList`; добавлены/исправлены их dexor-комменты.
- `2026-03-10`: renamed `AC_ReportFileVersionKeywordMatches`, `AC_HandleValidationOutcomeAndKick`, `AC_ValidateSystemD3DStack`, а также upstream helper `AC_ProcessPendingFileVersionChecks`.
- `2026-03-10`: добавлены plaintext-комменты на callsite'ы `0x1015D70A`, `0x1015D880`, `0x1015D9D8`, `0x1015DB60`, `0x1015DD1A`, `0x1015E064`, `0x1015E1F4`, `0x1015E31C`, `0x100CE9C5`, `0x100CECF2`, `0x100CEFA9`, `0x100CF1B7`, `0x100CF305`, `0x100CF5A0`, `0x100CF6B6`, `0x1010F5A6`, `0x1010F8F9`, `0x1010FF30`.
- `2026-03-10`: renamed `sub_100AB960` -> `AC_GenerateFixedImgDirAndDetectAnomalies`; закрыты все ранее пустые xref-комменты к `AC__SendLoggerToServerMtaDev`; добавлены exact decode-комменты для `AC_ProcessRequestWithTimeoutHandling`, `AC_ReportProcessScanFindings` и основного `GenerateFixedImgDir` кластера; на callsite без локального dexor blob поставлены честные semantic-комменты вместо фейкового decode.
