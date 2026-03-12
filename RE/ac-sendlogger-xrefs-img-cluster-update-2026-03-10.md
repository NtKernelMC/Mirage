# Objective

Document the additional direct callers of `AC__SendLoggerToServerMtaDev` that were reversed and renamed in the IMG-related cluster on 2026-03-10.

# Summary

- Renamed six previously-unknown direct callers in the IMG integrity / fixed IMG directory cluster.
- Remaining unnamed direct callers to `AC__SendLoggerToServerMtaDev`: `118`.
- The renamed functions are all backed by decompilation, call relationships, and sendlogger message content.

# Renamed Functions

- `0x100B1F60` -> `AC_CheckImgDirOrderAndReportAnomalies`
  - Checks IMG headers and entry order against the synthesized fixed IMG layout.
  - Reports open failure, bad header, order mismatch, and entry-count mismatch.

- `0x100B3320` -> `AC_InitializeFixedImgDirStateAndReport`
  - Builds and validates fixed IMG directory state across IMG variants.
  - Reports corruption and version-state findings before the redirection path is used.

- `0x100B4610` -> `AC_DetectExpectedImgVersion`
  - Counts known hash matches per IMG version and decides which version is expected.
  - Reports ambiguous or mismatched version statistics.

- `0x100B5B10` -> `AC_OnReadFixedImgDir`
  - Read callback for the fake fixed IMG directory stream.
  - Serves the synthetic header and entry data and reports invalid read patterns.

- `0x100B8D50` -> `AC_CompareModelGeometryAndReportDifferences`
  - Compares model / DFF-derived geometry data against expected data for an IMG entry.
  - Reports mismatch metrics and per-model comparison details.

- `0x100BA670` -> `AC_ProcessImgEntryAndReportIntegrityFindings`
  - Top-level IMG entry integrity pipeline.
  - Resolves entry metadata, checks expected IMG version, compares entry data, and reports missing or mismatched findings.

# Evidence

- `AC_CheckImgDirOrderAndReportAnomalies`
  - sendlogger comments already recovered from this function include:
    - `IMGTEST ImgDirOrderCheck open failure file='%s' [%08x]`
    - `IMGTEST ImgDirOrderCheck wrong header tag file='%s'`

- `AC_InitializeFixedImgDirStateAndReport`
  - sendlogger comments already recovered from this function include:
    - `IMGTEST GetFixedImgDir is corrupted - file=%s`
    - `IMGINFO GetFixedImgDir both standard - uiIsV1=%d uiIsV2=%d`

- `AC_DetectExpectedImgVersion`
  - recovered sendlogger text:
    - `Axpected version detection ImgIdx:%d  Is10:%d  Is20:%d`

- `AC_OnReadFixedImgDir`
  - recovered sendlogger text:
    - `IMGTEST OnReadPre read tag failed Img=%d ReadRedirPos=%d uiReadSize=%d`

- `AC_CompareModelGeometryAndReportDifferences`
  - decompilation shows DFF/model parsing, geometry extraction, comparison metrics, and multiple telemetry branches.

- `AC_ProcessImgEntryAndReportIntegrityFindings`
  - decompilation shows it calling:
    - `AC_DetectExpectedImgVersion`
    - `AC_CompareModelGeometryAndReportDifferences`
  - It also reports missing entry mappings and higher-level IMG integrity telemetry.

# Notes

- I intentionally did not rename the more ambiguous anti-cheat state-machine callers in this pass.
- These six names are the ones that were strong enough to survive a meaning-based rename without bullshit guesses.
