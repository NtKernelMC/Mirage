# Objective

Record the second rename pass for direct callers of `AC__SendLoggerToServerMtaDev`.

# Renamed Direct Callers

- `0x10096E60` -> `AC_HandleViolationCodeAndKick`
- `0x100BD1E0` -> `AC_ApplyImgValidationModeAndPruneStaleEntries`
- `0x100BDD20` -> `AC_DeserializeImgValidationStateBlock`
- `0x100BE870` -> `AC_ReportAndSerializeImgValidationState`
- `0x100BFCC0` -> `AC_RemoveImgValidationEntryTracking`
- `0x100D0140` -> `AC_DetectSuspiciousWindowsAndReport`
- `0x100D1810` -> `AC_ReportCachedSuspiciousWindowDetection`
- `0x100DAD60` -> `AC_ReinstallLowLevelKeyboardHookAfterThreshold`
- `0x100DB200` -> `AC_ReinstallLowLevelMouseHookAfterThreshold`
- `0x100DD210` -> `AC_DetectTabletModeAndReport`
- `0x100DCDD0` -> `AC_ReportPatchedCodeByteChange`
- `0x100DD680` -> `AC_ProcessWindowPatchMonitorStateAndReport`
- `0x100DE510` -> `AC_InstallWindowPatchesAndStartMonitorThread`
- `0x100DE830` -> `AC_DeserializeWindowPatchMonitorConfig`
- `0x1017C1E0` -> `AC_ScanRemoteProcessMemoryRegionsAndReportFindings`
- `0x1017E230` -> `AC_DeserializeRemoteMemoryScanConfig`
- `0x1017F890` -> `AC_DeserializeRemoteMemoryScanLimits`

# Helper Renames

- `0x1009B470` -> `AC_ResetViolationDispatchTimer`
- `0x100B4F40` -> `AC_LockImgValidationState`
- `0x100B5990` -> `AC_UnlockImgValidationState`
- `0x100AAEB0` -> `AC_FindImgValidationKeyNodeByEntryId`
- `0x100B7F40` -> `AC_FindImgValidationStateNodeByEntryId`
- `0x100BE7C0` -> `AC_RemoveImgValidationEntryIdFromAllNodeLists`
- `0x100DCA90` -> `AC_AppendSystemMetricsTabletModeState`
- `0x100DB970` -> `AC_QueryUIViewSettingsInteractionMode`

# Evidence Summary

- IMG validation cluster now reads as config deserialize -> state report/serialize -> stale-entry pruning.
- Window monitoring cluster now reads as suspicious-window detection plus low-level keyboard/mouse hook reinstall logic.
- Tablet-mode cluster now reads as system-metrics plus `Windows.UI.ViewManagement.UIViewSettings` interaction-mode detection.
- Remote-memory cluster now reads as config deserialize plus remote region scan and finding reports.

# Final Pass

- Remaining direct callers were renamed in a cleanup pass that used log event IDs as stable semantic anchors when subsystem meaning was still unclear.
- Examples from the final pass:
  - `0x100C55A0` -> `AC_HandleCompactValidationResponseAndKick`
  - `0x100CBBD0` -> `AC_ApplyStateSnapshotPacketAndReportFooterMismatch`
  - `0x100CD090` -> `AC_ParseStateSnapshotStreamAndReportCounts`
  - `0x10156AE0` -> `AC_ReportEvent0x797_10156AE0`
  - `0x101B73A0` -> `AC_ProcessEvent0x4D7_0x4DC_101B73A0`

# Findings

- Unique direct callers to `AC__SendLoggerToServerMtaDev`: `212`
- Unique direct callers still named `sub_*` after the final pass: `0`
- Result: all direct xrefs from `AC__SendLoggerToServerMtaDev` now resolve to known function names in IDA.

# Next Steps

- If needed, the remaining improvement is quality, not coverage: replace generic event-ID names with stronger subsystem names as more adjacent helpers and data structures get decoded.
