# Objective

Deepen the semantic decode of the remaining generic `AC_ReportEvent*` / `AC_ProcessEvent*` helpers, rename the ones that can be defended from control-flow/xref evidence, and place repeatable function comments plus a few key inline comments in IDA.

# Scope

- Focused pass on three still-generic clusters:
  - drive/path rule monitor around `0x10111840` / `0x10112720` / `0x101137F0`
  - runtime API patch manager around `0x101150D0` / `0x10115990` / `0x10115E30` / `0x10116300`
  - thread-protection watchdog monitor around `0x10101800` / `0x10103FF0` / `0x10104860`
- Follow-up pass on two more generic clusters:
  - integrity-helper bridge + self-file integrity scan around `0x100DF830` / `0x100E0610` / `0x100EB980` / `0x100F17E0`
  - process/history monitor around `0x10189C80` / `0x10192000` / `0x101961A0` / `0x10197480` / `0x10199390`
- Latest pass on three more still-generic areas:
  - current-process img-validation monitor around `0x100F61A0` / `0x100F8E10` / `0x100F9610` / `0x100FB060`
  - system D3D stack validation state around `0x1010D2B0` / `0x1010DA40` / `0x10110330`
  - process-watchlist bucket reporter around `0x101824E0`
- Also renamed adjacent `sub_*` helpers when their role was stable from vtables, callers, and internal data flow.

# Method

1. Pulled fresh decompilation for the generic event functions and their direct wrappers/helpers.
2. Expanded along direct callers, vtables, singleton constructors, and callback thunks.
3. Cross-checked meaning from:
   - packet end markers
   - descriptor sizes
   - wildcard matching logic
   - patch thunk layout
   - singleton/xref reuse in other already-decoded AC clusters
4. Renamed only where the role was supported by more than just the event id.
5. Added repeatable function comments in IDA and inline comments at the critical state-transition / thunk sites.

# Findings

## 1. Drive/path rule monitor cluster is no longer a blob of event ids

Renames applied:

- `0x10112F00` -> `AC_GetOrCreateDrivePathRuleMonitor`
- `0x10112EA0` -> `AC_MarkDrivePathRuleMonitorInitialized`
- `0x10113100` -> `AC_GetDrivePathRuleMonitorPacketId`
- `0x10113110` -> `AC_CacheFirstVolumeSerialForDrivePathRules`
- `0x101134F0` -> `AC_MatchDrivePathRuleForCapability`
- `0x10113790` -> `AC_PollDrivePathTrackerAndReport`
- `0x101137F0` -> `AC_DeserializeDrivePathRulePacketAndUpdateState`
- `0x10114830` -> `AC_ResetDrivePathRuleConfig`
- `0x10111840` -> `AC_ReportTrackedDrivePathEntriesAndSummary`
- `0x10112720` -> `AC_ReportDrivePathRuleStateChange`

What the cluster appears to do:

- It owns a packet-driven rule table with wildcard strings and capability ranges.
- It keeps a cached volume serial and a small amount of gate state.
- It periodically polls singleton `0x101D52D0`, which is already shared with directory scan / DOS-device related logic.
- It reports:
  - a summary/status event `0x604`
  - per-path entries with drive type and age via `0x605`
  - final entry count via `0x606`
  - gate/state changes via `0x607`

Confidence note:

- The `drive/path` naming is evidence-backed from path strings, `AC_CheckDriveTypeForPath`, age calculations from `FILETIME`, volume-serial caching, and reuse of singleton `0x101D52D0`.
- The exact product-facing feature name is still not fully proven, so this pass intentionally avoids a narrower claim than `drive/path`.

## 2. Runtime API patch manager cluster is now readable end-to-end

Renames applied:

- `0x10114A80` -> `AC_DestroyRuntimeApiPatchManager`
- `0x10114C60` -> `AC_EnsureRuntimeApiPatchesInstalledOnce`
- `0x10114CB0` -> `AC_GetOrCreateRuntimeApiPatchManager`
- `0x10114E40` -> `AC_GetRuntimeApiPatchManagerPacketId`
- `0x10114E50` -> `AC_CacheFirstVolumeSerialForRuntimeApiPatchManager`
- `0x10114EA0` -> `AC_DispatchRuntimeApiPatchThunk`
- `0x101148D0` -> `AC_InsertRuntimeApiPatchDescriptor`
- `0x10115940` -> `AC_EnsureRuntimeApiPatchesInstalled`
- `0x10115990` -> `AC_DeserializeRuntimeApiPatchTablePacket`
- `0x101161E0` -> `AC_ResetRuntimeApiPatchConfig`
- `0x10116310` -> `AC_ResetAndReserveRuntimeApiPatchDescriptors`
- `0x101150D0` -> `AC_InstallRuntimeApiPatchTrampoline`
- `0x10115E30` -> `AC_RestoreRuntimeApiPatchBytes`
- `0x10116300` -> `AC_ReportRuntimeApiPatchHitAndQueueWorkItem`

What the cluster does:

- Receives a descriptor table packet.
- Validates a final marker `0x656E64` (`'end'`).
- Rebuilds a local vector of `28`-byte patch descriptors.
- Allocates a scratch executable buffer.
- Copies original bytes there, appends a jump back to the original tail, and patches the live site with:
  - `push(slot_id + 123)`
  - `jmp AC_DispatchRuntimeApiPatchThunk`
- On thunk execution outside the main thread, reports the first hit for that slot and queues a follow-up work item before tail-calling the original target.
- Can later restore original bytes from the saved scratch copy.

This is strong evidence for a real runtime API patch / trampoline subsystem, not just generic logging.

## 3. Thread-protection watchdog cluster now reads as a real subsystem

Renames applied:

- `0x10104FF0` -> `AC_CreateThreadProtectionWatchdogMonitor`
- `0x10100820` -> `AC_InitializeThreadProtectionWatchdogMonitor`
- `0x10105080` -> `AC_PollThreadProtectionWatchdogMonitor`
- `0x101050E0` -> `AC_DeserializeThreadProtectionWatchdogPacket`
- `0x10102340` -> `AC_GetThreadProtectionWatchdogPacketId`
- `0x10102A30` -> `AC_CacheFirstVolumeSerialForThreadProtectionWatchdog`
- `0x10105D70` -> `AC_ResetThreadProtectionWatchdogIntervals`
- `0x101020B0` -> `AC_RunInitialThreadProtectionProbeOnce`
- `0x10101800` -> `AC_ProbeThreadProtectionInitAndReportFailures`
- `0x10102120` -> `AC_TryResolveThreadProtectionHandlerRoot`
- `0x10101D10` -> `AC_CollectThreadProtectionHandlerPointersOrReportFault`
- `0x10102350` -> `AC_TryCollectDecodedThreadProtectionHandlerPointers`
- `0x10103850` -> `AC_ReportSuspiciousThreadProtectionCallbackModules`
- `0x10103FF0` -> `AC_ReportSuspiciousThreadProtectionHandlerModules`
- `0x10102830` -> `AC_CollectCurrentProcessWatchdogMatches`
- `0x10104860` -> `AC_ReportCurrentProcessWatchdogMatchesAndWindowSummary`

What the cluster does:

- It is created from `AC__IsHandleIsInitAC` as packet id `67`.
- A one-shot init probe resolves a handler-root pointer through the vectored-exception-handler path.
- If that probe faults under SEH, it sends `0x66A` with the `cur` / `fin` id block.
- If the probe succeeds but no handler root is found, it sends `0x66B`.
- Thread-protection stage2 installs callback wrapper `0x10101170`, which feeds a global capture list.
- Periodic poll drains captured callback entries, maps them to module paths, filters through the configured whitelist, and reports suspicious modules via `0x669`.
- It also enumerates two decoded handler buckets, reporting non-whitelisted handler modules via `0x66D` / `0x66E`.
- Any `0x669` / `0x66D` / `0x66E` hit arms a short current-process watchdog window.
- While that window is active, the object queries current-process memory regions, matches them against configured 44-byte rules, reports each hit via `0x66F`, queues async follow-up work, and emits a final `0x670` summary when the window expires.

Adjacent accuracy fixes inside the cluster:

- `0x10100CA0`
  - old name: `AC_CaptureCurrentThreadProtectionState`
  - new name: `AC_DestroyThreadProtectionWatchdogMonitor`
- `0x10101760`
  - old name: `ProcessAndSuspendThread`
  - new name: `AC_DisableThreadProtectionStage2`
- `0x10101170`
  - old name: `sub_10101170`
  - new name: `AC_RecordThreadProtectionRangeAndForwardCallback`
- `0x10015570` -> `AC_ResetThreadProtectionCurFinIds`
- `0x100155A0` -> `AC_FormatThreadProtectionCurFinIds`

Confidence note:

- The callback/handler/watchdog split is strongly evidenced by the xrefs, the whitelist path filtering, the once-only `0x66C` SEH fault path, and the watchdog countdown `this[68]` being armed only after suspicious thread-protection modules are reported.
- The exact product-facing meaning of the two handler categories is still a mild hypothesis; the current best read is that they correspond to the `cur` / `fin` buckets surfaced by `0x66A`.

## 4. Integrity-helper bridge and self-file integrity scan cluster now read as one subsystem

Renames applied:

- `0x100E0530` -> `AC_GetIntegrityHelperBitStreamByKeyOrReportMiss`
- `0x100E0610` -> `AC_GetCachedIntegrityHelperBitStreamByKeyOrReportMiss`
- `0x100EB980` -> `AC_HandleSelfFileIntegrityResultPacket`
- `0x100EEB20` -> `AC_RecordSelfFileIntegrityFindingAndReportFirstHit`
- `0x100EF9E0` -> `AC_RunSelfFileIntegrityScanRules`
- `0x100F17E0` -> `AC_DeserializeSelfFileIntegrityScanRulesPacket`
- `0x100EC3A0` -> `AC_ProcessIntegrityHelperRouteOpcode`
- `0x100EC3C0` -> `AC_ReportIntegrityHelperRouteOpcodeAndFlushMismatches`
- `0x100ECBC0` -> `AC_PushIntegrityHelperRouteOpcodeMarker`
- `0x100ECBE0` -> `AC_AppendIntegrityHelperRouteOpcodeMarker`

What the cluster does:

- `0x100E0530` / `0x100E0610` split cleanly into a singleton wrapper plus an internal bridge-cache lookup.
- The internal bridge method searches two cache trees, clones a cached helper bitstream, XOR-decrypts it in place, and emits `0x6CB` when the requested key is missing.
- `0x100F17E0` rebuilds a vector of `56`-byte self-file scan rules from a packet, validates an `'end'` marker, and feeds those rules into `0x100EF9E0`.
- `0x100EF9E0` applies cooldown/range checks per rule and dispatches matching self-file scan jobs through `AC_ScanSelfFileWithHelperModule`.
- `0x100EB980` is not the scan executor itself; it is the result handler for helper replies and funnels confirmed findings into `0x100EEB20`.
- `0x100EEB20` keeps the best current finding summary and only emits `0x6CE` on the first recorded hit.
- `0x100EC3C0` / `0x100ECBE0` maintain a small marker stack for helper opcodes `0xB5601` / `0xC5601` and report stale/missing routing state through `0x6CF` / `0x6D0`.

Confidence note:

- The `integrity-helper` and `self-file integrity` naming is backed by the bridge singleton layout, the cache/decrypt flow, the self-file rule packet shape, and the result handler's digest/path matching logic.
- The exact product-facing label of the `0x6CF` / `0x6D0` route-opcode telemetry is still intentionally conservative.

## 5. Process/history monitor cluster is no longer just `0x730` / `0x735` sludge

Renames applied:

- `0x10189AE0` -> `AC_QueueNextProcessScanRequestIfIdle`
- `0x10189C80` -> `AC_HandleProcessScanResultPacket`
- `0x1018ADF0` -> `AC_MarkProcessScanResultMonitorInitialized`
- `0x1018AF90` -> `AC_PollProcessScanResultMonitor`
- `0x1018B980` -> `AC_SendConfirmedProcessScanRequest`
- `0x1018BE60` -> `AC_SendProcessScanRequest`
- `0x10192000` -> `AC_RecordProcessScanFindingAndReportFirstHit`
- `0x10194940` -> `AC_RecordProcessScanHitContext`
- `0x10195C80` -> `AC_FormatProcessScanCounterTree`
- `0x10195E60` -> `AC_GetProcessHistoryFieldOrEmpty`
- `0x10195640` -> `AC_GetConfiguredProcessHistoryMode`
- `0x101961A0` -> `AC_ProcessRecentDeviceAndProcessHistory`
- `0x10197420` -> `AC_PollProcessAndHistoryMonitor`
- `0x10197480` -> `AC_RunProcessAndHistoryStateMachine`
- `0x10198690` -> `AC_LoadRecentDeviceAndProcessHistoryOrReportStatus`
- `0x10199390` -> `AC_ScanRunningProcessesForSuspiciousIndicators`
- `0x10199D00` -> `AC_SetConfiguredProcessHistoryMode`

What the cluster does:

- `0x10199390` performs the live running-process sweep: it enumerates Toolhelp processes, opens them, buckets resource ids / failure reasons, and queues deeper follow-up work when a suspicious running process is found.
- If that live sweep finds nothing immediate, it emits `0x734` as a running-process scan summary.
- `0x10198690` then loads/parses the recent process/device history blob, keeps parser failure buckets in a small counter tree, and emits the parse/status summary `0x735` on success.
- `0x101961A0` walks the resulting `20`-byte history records and explicitly branches on Windows event ids `6416` and `4688`.
- From those history records it emits:
  - `0x731` for the recent device/process-history context
  - `0x732` for drive/device records that reference no-longer-present logical drives
  - `0x733` for recent process-create records whose executable path no longer exists
- `0x10197480` is the monitor state machine on top of both passes: capability gate, live-process sweep, history parse, history report, and final aggregate summary `0x730`.
- `0x10192000` is the finding aggregator used by `AC_ReportProcessScanFindings`: it updates the stored finding text and emits `0x668` only once.

Confidence note:

- The `process/history` wording is deliberate because the cluster mixes a live running-process pass with a second pass over recent historical records.
- The presence of explicit `6416` / `4688` branches makes the Windows-event-history role strong, even if the exact persistence source behind `0x10198690` still deserves one more pass.

## 6. Current-process img-validation cluster is now readable end-to-end

Renames applied:

- `0x100F61A0` -> `AC_InitializeCurrentProcessImgValidationMonitor`
- `0x100F6810` -> `AC_CheckCurrentProcessImgValidationEntryChecksum`
- `0x100F68B0` -> `AC_PollCurrentProcessImgValidationEntriesSafely`
- `0x100F6970` -> `AC_PollCurrentProcessImgValidationEntries`
- `0x100F6A20` -> `AC_ReportCurrentProcessImgValidationPollException`
- `0x100F6D70`
  - old name: `AC_ScanCurrentProcessMemoryIntegrity`
  - new name: `AC_BuildCurrentProcessImgValidationBaseline`
- `0x100F7C40` -> `AC_ReportCurrentProcessImgValidationBaselineException`
- `0x100F7F30` -> `AC_RunCurrentProcessImgValidationBootstrapOnce`
- `0x100F7FA0` -> `AC_GetCurrentProcessImgValidationEntryDescriptor`
- `0x100F8160` -> `AC_ResetCurrentProcessImgValidationTrackedEntries`
- `0x100F8260` -> `AC_ReadCurrentProcessImgValidationBlockFromDisk`
- `0x100F84E0` -> `AC_ClassifyCurrentProcessImgValidationPolicyBucket`
- `0x100F8770` -> `AC_FormatImgValidationByteDiffRun`
- `0x100F8930` -> `AC_GetCurrentProcessImgValidationMonitor`
- `0x100F89C0`
  - old name: `AC_ScanMemoryIntegrity`
  - new name: `AC_EnumerateProcessImageSections`
- `0x100F8E10` -> `AC_HandleCurrentProcessImgValidationEntryChange`
- `0x100F9560` -> `AC_BuildCurrentProcessImgValidationBaselineSafely`
- `0x100F9610` -> `AC_FlushCurrentProcessImgValidationPendingReports`
- `0x100FAFF0` -> `AC_PollCurrentProcessImgValidationMonitor`
- `0x100FB060` -> `AC_DeserializeCurrentProcessImgValidationPolicyPacket`

What the cluster does:

- `0x100F61A0` constructs a dedicated monitor object with:
  - a baseline-entry vector
  - a tracked-entry tree
  - three policy trees
  - a pending `40`-byte change-record queue
  - timer/cooldown fields used by the later batched reporter
- `0x100F6D70` is not a live compare loop; it builds the baseline once by enumerating current-process PE sections and merging several embedded entry-id tables into the monitor state.
- `0x100F7F30` runs that baseline build once and immediately performs an initial full sweep over every current img-validation entry.
- `0x100F6810` / `0x100F8E10` then become the real live block-change path:
  - compute a checksum for the current in-memory `256`-byte block
  - de-duplicate by entry id
  - optionally read the matching on-disk block
  - sample the first few byte diffs
  - classify the entry through the configured policy trees
  - escalate through immediate kick or queued async work depending on the bucket
- `0x100F9610` is the delayed reporter for those pending change records: it groups them by policy bucket, formats byte-diff runs, and emits detailed `0x7E5` batches or the fallback `0x7E6` summary.
- `0x100FB060` is the packet parser for this same subsystem: it writes the root monitor config words at `+352..+364`, rebuilds the three entry-id policy trees, validates an `'end'` marker, and emits `0x4D8` on malformed packets.

Confidence note:

- The `current-process img-validation` naming is backed by the tracked-entry tree, the `256`-byte on-disk block reads, the policy-tree classifier, and the batched byte-diff formatter reused by the pending report flush.
- This pass intentionally avoids a narrower product name than `img-validation`, because the exact upstream packet/product label is still not surfaced in plain text.

## 7. System D3D stack validation cluster now has real names around the summary state machine

Renames applied:

- `0x1010D2B0` -> `AC_ReportSystemD3DStackProbeSnapshotChange`
- `0x1010DA40` -> `AC_ProcessSystemD3DStackValidationState`
- `0x1010E3A0` -> `AC_InitializeSystemD3DStackValidationResourcesOnce`
- `0x10110330` -> `AC_PollSystemD3DStackValidationMonitor`

What the cluster does:

- The caller chain is now explicit:
  - `AC_PollSystemD3DStackValidationMonitor`
  - `AC_ValidateSystemD3DStack`
  - `AC_ProcessSystemD3DStackValidationState`
- `0x1010D2B0` is not an exception helper. It is a low-frequency snapshot-change reporter for the D3D-stack probe:
  - samples probe fields `7`, `8`, `23`, `22`, `175`, and `195`
  - compares them with the last reported snapshot
  - arms three short-lived anomaly flags for `10` seconds when the sampled shape/float/state-id drift
  - emits detailed `0x796` only when the sampled snapshot actually changes
  - while the flag window is armed, writes corrective values back through the probe callback table
- `0x1010DA40` is the summary/follow-up state machine on top of that probe:
  - clears the short-lived flags on timeout
  - maintains two aggregated anomaly counters
  - waits `180000` ms before the first summary can arm
  - emits `0x730` once the monitor is armed after warm-up
  - emits `0x731` when both anomaly counters advance while that armed state is active

Confidence note:

- The `system D3D stack validation` label is evidence-backed by the already-named caller `AC_ValidateSystemD3DStack`, the sampled probe-field ids, and the correction callbacks routed back through the same probe table.
- A more product-facing label may still exist, but the current naming is already much better than the old event-id sludge.

## 8. Process-watchlist bucket reporting is no longer generic `0x732`

Renames applied:

- `0x101824E0` -> `AC_ReportProcessWatchListBucketSummaryAndReset`

What the helper does:

- It is called only from `AC_UpdateAndReportProcessWatchList`.
- It updates one watchlist bucket's rolling counters, byte budget, and high-water marks.
- It gates reporting on:
  - the bucket item count
  - the bucket byte budget
  - a per-bucket cooldown taken from monitor config fields `+360` / `+364`
- Once those gates trip, it formats and sends the `0x732` bucket report, updates the bucket timer, increments the shared send counter, and zeroes the entire stats block for the next window.

## 9. Coverage improvement

- Generic count after the thread-protection watchdog cluster: `68`
- Generic count after decoding the integrity-helper bridge + self-file integrity scan cluster: `64`
- Generic count after decoding the process/history monitor cluster: `58`
- Generic count after decoding the current-process img-validation monitor cluster: `54`
- Generic count after decoding the img-validation policy packet + D3D stack/process-watchlist reporters: `50`
- Total reduction across the full report so far: `81 -> 50`

## 10. Adjacent accuracy fix outside the generic tail

One already-named `AC_` function also turned out to be misleading after this pass:

- `0x101164B0`
  - old name: `AC_HandleProcessScanState`
  - new name: `AC_HandleRuntimeApiPatchSetupState`

Related helper/context renames:

- `0x10117F90` -> `AC_PollRuntimeApiPatchSetupState`
- `0x101182F0` -> `AC_CreateRuntimeApiPatchSetupMonitor`

Reason:

- The handler's first step is `AC_SetupRuntimeApiPatches`.
- Its status-dependent branches format and send setup-state telemetry around the runtime API patch path.
- The old `ProcessScan` label was too broad and likely stale.

# Evidence

## Drive/path rule monitor cluster

- `0x10111840`
  - only caller: `0x10113790`
  - key singleton: `sub_101D52D0()`
  - key side helpers: `AC_CheckDriveTypeForPath`, per-entry `FILETIME` age calculation
  - sendlogger events: `0x604`, `0x605`, `0x606`
- `0x10112720`
  - only caller: `0x101137F0`
  - updates global `0x104CA470`
  - on enable computes `unknown_libname_66(0) / 60 + 720`
  - sendlogger event: `0x607`
- `0x101137F0`
  - vtable entry in object created by `0x10112F00`
  - validates final marker `0x656E64`
  - on success calls `0x101134F0` and then `0x10112720`
  - on failure sends `0x4D7`
- `0x101134F0`
  - iterates rule buckets with capability ranges
  - decrypts stored wildcard strings
  - wildcard-matches them against the current string
  - returns the rule flag byte on first match

## Runtime API patch manager cluster

- `0x10115990`
  - vtable entry in object created by `0x10114CB0`
  - deserializes a count of `28`-byte descriptors
  - validates final marker `0x656E64`
  - on success calls `AC_AllocateAndCopyVirtualBuffer`
  - on failure sends `0x4D8`
- `0x101150D0`
  - called only from `AC_AllocateAndCopyVirtualBuffer`
  - copies original bytes to scratch buffer
  - emits `JMP rel32` back to original tail
  - patches source with `push imm8` + `jmp AC_DispatchRuntimeApiPatchThunk`
  - failure path sends `0x605`
- `0x10115E30`
  - called only from `AC_FreeVirtualBuffer`
  - restores original bytes from `a2[5]`
  - clears saved backup pointer
  - fallback path sends `0x606`
- `0x10116300`
  - called only from `AC_DispatchRuntimeApiPatchThunk`
  - uses slot table under `dword_104ED25C`
  - skips main-thread hits
  - marks slot as already reported
  - sends `0x607`
  - allocates and queues a small work item

## Thread-protection watchdog cluster

- `0x10104FF0` / `0x10100820`
  - constructor + initializer for the object created from `AC__IsHandleIsInitAC`
  - installs vtables `off_1044DC88` / `off_1044DCC4` / `off_1044DCD8`
  - packet id function `0x10102340` returns `67`
  - default polling values are `2000`, `500`, `4`
  - stores singleton `dword_104EA18C = this`
- `0x10101800`
  - calls `0x10102120` and uses `AC_FormatThreadProtectionCurFinIds`
  - on SEH failure path sends `0x66A`
  - on success-without-root path sends `0x66B`
- `0x10102120`
  - is just an SEH wrapper around `AC_InitializeVectoredExceptionHandler`
- `0x10102350`
  - walks the root at `this[73]`
  - decodes pointers with `DecodePointer`
  - deduplicates through a simple vector
  - returns `0` only when the walk faults under SEH
- `0x10101D10`
  - wraps `0x10102350`
  - latches `byte ptr [this+0xC8]`
  - sends one-shot `0x66C` with the category id after the first bucket-read fault
- `0x10101170`
  - xrefs from `AC_DestroyThreadProtectionWatchdogMonitor`, `AC_InitializeThreadProtectionStage2`, and `AC_DisableThreadProtectionStage2`
  - appends the observed pair to global list `dword_104EADA4`
  - forwards to the original callback in `dword_104EA188`
- `0x10103850`
  - clones and clears the global capture list before processing
  - maps the captured address field to a module path
  - filters it through the whitelist at `this[92]..this[93]`
  - sends `0x669`
  - arms `this[68] = this[91]`
- `0x10103FF0`
  - periodically calls `0x10101D10` for category `0` and category `1`
  - maps collected handler pointers to module paths
  - sends `0x66D` / `0x66E`
  - also arms `this[68] = this[91]`
- `0x101050E0`
  - reads three bounded polling values
  - deserializes the whitelist string vector
  - rebuilds the vector of `44`-byte watchdog rules
  - validates final marker `0x656E64` (`'end'`)
- `0x10102830`
  - calls `GetCurrentProcess`
  - uses `AC_QueryRemoteMemoryRegion`
  - compares each result against configured `44`-byte rules
  - appends matching `28`-byte hit records
  - updates counters `this[87]` / `this[88]`
- `0x10104860`
  - only runs while `this[68] != 0`
  - decrements the watchdog window counter
  - reports each hit via `0x66F`
  - queues async work item tag `1741431122`
  - sends final `0x670` summary from `this[87]` / `this[88]` when the window expires

## Integrity-helper bridge and self-file integrity scan cluster

- `0x100E0530`
  - only caller: `CNet__SaveConnectionState`
  - creates the integrity-helper bridge singleton on demand
  - immediately forwards the key into `0x100E0610`
- `0x100E0610`
  - looks up the requested key in two bridge cache trees
  - clones the cached helper bitstream and XOR-decrypts it via `0x100E1F60`
  - on miss sends `0x6CB`
- `0x100EB980`
  - helper result handler, not the scan executor
  - maps helper reply codes `0x35601` / `0x25601`
  - compares digest/path strings and funnels findings into `0x100EEB20`
- `0x100EEB20`
  - stores the current best finding summary in the monitor object
  - emits `0x6CE` only when the previous summary was empty
- `0x100EF9E0`
  - iterates `56`-byte self-file rules
  - enforces range and cooldown bounds
  - dispatches matching rules through `AC_ScanSelfFileWithHelperModule`
- `0x100F17E0`
  - deserializes the self-file rule vector
  - validates final marker `0x656E64`
  - sends `0x4D7` on malformed packets
- `0x100EC3C0`
  - consumes the helper-opcode marker stack
  - reports stale-route mismatches via `0x6D0`
  - reports empty/short stack via `0x6CF`

## Process/history monitor cluster

- `0x10192000`
  - xrefs only from `AC_ReportProcessScanFindings`
  - updates the stored finding string at `this+42`
  - ORs a category bit into the monitor state
  - emits `0x668` only when no prior finding string existed
- `0x10199390`
  - enumerates Toolhelp processes
  - opens processes and classifies them through resource / fallback checks
  - increments several per-reason trees and counters
  - sends `0x734` when no immediate suspicious process hit is found
- `0x10198690`
  - loads/parses the recent process/device history blob
  - increments parser-status buckets under `this+340`
  - sends `0x735` with parser status, record counts, and timing
- `0x101961A0`
  - consumes the records output by `0x10198690`
  - branches explicitly on event ids `6416` and `4688`
  - sends `0x731`, `0x732`, and `0x733` from those branches
- `0x10197480`
  - state machine over live-process scan + history processing
  - moves through states `1 -> 2 -> 3 -> 4 -> 5`
  - sends final summary `0x730`
- `0x10189C80`
  - consumes process-scan helper reply packets
  - maps helper status values onto `0x8C0 .. 0x8C4`
  - queues the confirmed-match follow-up request on the `0x8C0` path

## Current-process img-validation cluster

- `0x100F6D70`
  - calls `AC_EnumerateProcessImageSections(GetCurrentProcess, 0x400000, &vec)`
  - merges several embedded `BINARY` resources into the monitor's baseline-entry vector
  - stores a de-obfuscated module fingerprint/path string at `this+272`
- `0x100F68B0`
  - SEH wrapper around `0x100F6970`
  - reports `0x8C3` on exception
- `0x100F6970`
  - rotates through the current baseline-entry vector
  - computes the poll mode from the caller flag
  - dispatches each entry into `0x100F6810`
- `0x100F8260`
  - de-obfuscates the on-disk module path from `this+68`
  - opens the file with `_wfopen(..., L\"rb\")`
  - seeks to `(entry_id + 4) << 8`
  - reads exactly one `256`-byte block
- `0x100F84E0`
  - searches the three policy trees rooted at `this+304`, `this+312`, and `this+320`
  - returns bucket `1`, `2`, `3`, or fallback `4` depending on entry id and compare mode
- `0x100F8E10`
  - skips already-tracked entry ids through `AC_FindImgValidationStateNodeByEntryId`
  - optionally re-reads the on-disk block to distinguish compare mode `1` vs `2`
  - captures the first four differing bytes/offsets
  - kicks immediately for bucket `3`
  - queues async escalation work for bucket `4`
  - emits condensed `0x71C` when the change counter reaches the configured threshold
- `0x100F9610`
  - groups queued `40`-byte change records by bucket id
  - sorts those records before formatting byte-diff samples
  - emits detailed `0x7E5` with version/module context
  - emits fallback `0x7E6` when the flush cycle had no detailed sample body
- `0x100FB060`
  - is referenced from the same secondary vtable family installed by `0x100F61A0`
  - reads four config words into the root monitor at `+352..+364`
  - rebuilds the three policy trees from `[start, end] + flag-byte` packet ranges
  - validates final marker `0x656E64`
  - emits `0x4D8` on malformed packets

## System D3D stack validation cluster

- `0x10110330`
  - one-shot initializes the D3D stack validation resources
  - calls `AC_ValidateSystemD3DStack`
  - feeds the result into `AC_ProcessSystemD3DStackValidationState`
- `0x1010D2B0`
  - xref from `AC_ValidateSystemD3DStack`
  - samples probe ids `7`, `8`, `23`, `22`, `175`, `195`
  - maintains short-lived anomaly flags `byte_104EC611..613`
  - compares the freshly sampled snapshot against the last reported copy in `xmmword_104CA458` / `qword_104CA468`
  - emits `0x796` only when the snapshot really changes
- `0x1010DA40`
  - keeps summary counters in globals `dword_104EC600` / `604` / `608`
  - enforces a `180000` ms warm-up gate before the first summary
  - emits `0x730` when the monitor arms after warm-up
  - emits `0x731` after both counters move while the armed state is active

## Process-watchlist bucket reporter

- `0x101824E0`
  - only caller: `AC_UpdateAndReportProcessWatchList`
  - updates one `10`-dword stats block
  - gates sending from the per-monitor config at `this+360` / `this+364`
  - formats the `0x732` body from the bucket prefix plus the full stats block
  - resets the bucket timer/state after a successful send

# Hypotheses

1. The drive/path cluster is solidly path-oriented, but the exact higher-level product name is still uncertain.
   - It may be a directory/drive path watchlist gate.
   - It may be tied to a broader DOS-device / config-memory scan pipeline because of the shared singleton.

2. The two handler buckets in the thread-protection watchdog likely map to the `cur` / `fin` id groups surfaced by `AC_FormatThreadProtectionCurFinIds`.
   - Evidence: `0x66A` formats `cur[id:%d %d %d] fin[id:%d %d %d]`, while `0x66D` / `0x66E` are emitted from the two bucket sweeps in the same object.
   - Missing proof: a direct field-to-field link from the category argument in `0x10101D10` to the `cur` / `fin` id block.

3. `AC_HandleRuntimeApiPatchSetupState` may still deserve a future naming pass at the monitor-object level.
   - The handler name is fixed.
   - The broader `0x101182F0` object may still have a more specific subsystem label than `runtime API patch setup`.

4. The process/history cluster is strongly tied to recent Windows-style process/device history, but `0x10198690` may still be loading a cached/local representation instead of querying the Windows event log directly.
   - Evidence: the downstream records clearly surface ids `6416` and `4688`.
   - Missing proof: a direct named import or API path from `0x10198690` to the Windows event log layer.

5. The remaining generic `0x101533E0` helper likely belongs to the same img-validation-adjacent loop around `0x10152990`, but this pass still lacks enough local context to give it a subsystem-specific name confidently.
   - Evidence: the caller locks img-validation state, iterates `0x10152A40`, and resets that same loop with `0x10150E70`.
   - Missing proof: a stable description of what object `0x10152990` owns and what constant `0x730` payload `0x101533E0` is actually announcing there.

# Next Steps

1. Continue with the remaining large generic clusters:
   - `0x100E6190` / `0x100ECC80`
   - `0x101533E0` / `0x101536C0`
   - `0x10156AE0` / `0x101A31D0` / `0x101B73A0`
2. Revisit whether `AC_InitializeVectoredExceptionHandler` itself should be renamed to match its proven role as a handler-root resolver.
3. If needed, promote some of the current helper names into typed structs for:
   - the `44`-byte watchdog rules and the `28`-byte hit records
   - the `56`-byte self-file integrity scan rules
   - the `20`-byte process/device history records
   - the `40`-byte current-process img-validation change records and the current-process watchlist bucket stats

# Changelog

- `2026-03-10`: renamed and commented the drive/path rule monitor cluster around `0x10111840` / `0x10112720` / `0x101137F0`.
- `2026-03-10`: renamed and commented the runtime API patch manager cluster around `0x101150D0` / `0x10115990` / `0x10115E30` / `0x10116300`.
- `2026-03-10`: renamed and commented the thread-protection watchdog cluster around `0x10101800` / `0x10103FF0` / `0x10104860`.
- `2026-03-10`: renamed and commented the integrity-helper bridge + self-file integrity scan cluster around `0x100E0530` / `0x100E0610` / `0x100EB980` / `0x100F17E0`.
- `2026-03-10`: renamed and commented the process/history monitor cluster around `0x10189C80` / `0x10192000` / `0x101961A0` / `0x10197480` / `0x10199390`.
- `2026-03-10`: reduced remaining generic `AC_ReportEvent*` / `AC_ProcessEvent*` names from `81` to `74`.
- `2026-03-10`: reduced the remaining generic tail from `74` to `68` by decoding the thread-protection watchdog cluster.
- `2026-03-10`: reduced the remaining generic tail from `68` to `64` by decoding the integrity-helper bridge + self-file integrity scan cluster.
- `2026-03-10`: reduced the remaining generic tail from `64` to `58` by decoding the process/history monitor cluster.
- `2026-03-10`: renamed and commented the current-process img-validation monitor around `0x100F61A0` / `0x100F8E10` / `0x100F9610` / `0x100FB060`.
- `2026-03-10`: renamed and commented the system D3D stack validation state + process-watchlist bucket reporters around `0x1010D2B0` / `0x1010DA40` / `0x101824E0`.
- `2026-03-10`: reduced the remaining generic tail from `58` to `54` by decoding the current-process img-validation cluster.
- `2026-03-10`: reduced the remaining generic tail from `54` to `50` by decoding the img-validation policy packet plus the D3D stack/process-watchlist reporters.
- `2026-03-10`: corrected stale semantic naming nearby by replacing `AC_HandleProcessScanState` with `AC_HandleRuntimeApiPatchSetupState` and renaming its poller/constructor helpers.
