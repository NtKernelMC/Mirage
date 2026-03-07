# Objective
Implement and validate (via IDA Pro MCP) a multithreading hook strategy for GTA SA US 1.0 streaming/collision paths used by MTA-heavy custom maps, with explicit dependency coverage and race-risk mitigation.

# Scope
- Target binary: GTA SA US 1.0 IDB opened in IDA Pro.
- Runtime integration target: `MirageAgent` (MinHook-based).
- Main targets:
1. `CStreaming::ProcessLoadingChannel` at `0x40E170`
2. `CStreaming::ConvertBufferToObject` at `0x40C6B0`
3. Collision loaders around `CColStore::LoadCol` / `CFileLoader::*Collision*`
4. Triangle plane path (`CCollisionData::CalculateTrianglePlanes`, `CCollision::CalculateTrianglePlanes`)

# Method
1. Verified prototypes for all hook targets using `export_funcs(format=prototypes)`.
2. Verified critical globals/xrefs with `xrefs_to`:
- `ms_channel` `0x8E4A60`
- `ms_aInfoForModel` `0x8E4CC0`
- `ms_memoryUsed` `0x8E4CB4`
- `ms_pColPool` `0x965560`
- `ms_modelInfoPtrs` `0xA9B0C8`
- `freeCollinksList` `0x965934`
- `freeColLinks` `0x965944`
3. Expanded dependency graph with `callgraph` from roots:
- `0x40E170`, `0x40C6B0`, `0x4106D0`, `0x538440`, `0x40F590`, `0x4163E6`
4. Implemented lock + worker queue strategy in `SaOptimization.h`, then rebuilt Win32 Release.

# Findings
1. Verified hook prototypes (IDA evidence)
- `0x40E170` `char __cdecl(int)`
- `0x40C6B0` `char __cdecl(int pBuffer, int modelId)`
- `0x4087E0` `void __cdecl(int,int)`
- `0x4089A0` `void __cdecl(int)`
- `0x40EA10` `void __cdecl(char)`
- `0x40E4E0` `bool __cdecl()`
- `0x407480` `int __thiscall(CStreamingInfo*, CStreamingInfo*)`
- `0x4074E0` `void __thiscall(CStreamingInfo*)`
- `0x40A080` `void __cdecl(int,int,int,int,int)`
- `0x409C10/0x409C70/0x409C90` `void __cdecl(int)`
- `0x4106D0` `char __cdecl(unsigned int,int,unsigned int)`
- `0x538440` `char __cdecl(CFileLoader*, unsigned __int8*, unsigned __int8)`
- `0x5B5000` `char __cdecl(int,unsigned int,unsigned __int8)`
- `0x537EE0` `int __cdecl(int,int,CColModel*)`
- `0x537CE0` `void __cdecl(void*,int,CColModel*,char*)`
- `0x537AE0` `void __cdecl(void*,int,CColModel*)`
- `0x4C4BC0` `void __thiscall(CBaseModelInfo*, CColModel*, bool)`
- `0x5B2C20` `void __cdecl(int,int)` (`CColAccel::addCacheCol`)
- `0x40F590` `void __thiscall(CColData*)`
- `0x4163E6` `void __cdecl(CColData*)`
- `0x416400` `void __cdecl(CCollisionData*)`

2. `CStreamingInfo` layout validated
- Size `20` bytes.
- `cdSize` offset `12`.
- `loadState` offset `16`.

3. Implemented practical lock coverage
- Single recursive state mutex around streaming/collision mutators.
- Added extra streaming mutator hooks discovered from xrefs/callgraph:
1. `CStreaming::RequestModelStream` `0x40CBA0`
2. `CStreaming::RemoveLeastUsedModel` `0x40CFD0`
3. `CStreaming::DeleteLeastUsedEntityRwObject` `0x409760`
4. `CStreaming::GetNextFileOnCd` `0x408E20`
5. `CStreaming::Update` `0x40E670`

4. Async execution model
- Deferred worker queue introduced for `ConvertBufferToObject`.
- Async conversion currently gated to collision model range (`25000..25254`) to avoid broader RenderWare/D3D9 threading hazards.
- Queue dedup via `pendingModels` set.

5. Collision-specific protection
- Collision load path and model-linking endpoints protected:
1. `LoadCol`, `LoadCollisionFile`, `LoadCollisionFileFirstTime`
2. `LoadCollisionModelVer2/3/4`
3. `SetColModel`
4. `addCacheCol`
5. `CalculateTrianglePlanes` + `RemoveTrianglePlanes`

# Evidence
- IDA MCP `export_funcs` confirmed calling conventions/signatures for all installed hooks.
- IDA MCP `xrefs_to` confirmed global dependency hotspots:
1. `ms_channel` used in `ProcessLoadingChannel` and request scheduling path.
2. `ms_aInfoForModel` heavily used by request/remove/convert/flush routines.
3. `ms_memoryUsed` touched in remove/convert pressure management.
4. `ms_pColPool` used in col load/alloc/lookup paths.
5. `freeCollinksList/freeColLinks` used by triangle plane calc/remove path.
- IDA MCP `callgraph` confirmed load chain:
`0x4106D0 -> 0x15613A0 -> 0x5B5000 -> 0x537CE0/0x537EE0/...` and linkage to model/accel update points.

# Hypotheses
1. Performance impact (MTA heavy map profile)
- `ProcessLoadingChannel + ConvertBufferToObject`: expected `+6%..+14%`, up to `~+18%` on heavy COL/IPL/PATH bursts.
- Collision load chain (`LoadCol` + `LoadCollisionFile*`): expected `+8%..+20%` spike reduction, `+4%..+10%` average.
- Triangle planes path: expected `+3%..+8%`, up to `~+12%` on dense collision meshes.
2. Remaining risk
- Absolute race-freedom across entire engine cannot be proven without instrumenting all readers/writers of touched globals, but the implemented coverage closes the critical mutator path requested for this task.

# Next Steps
1. Runtime stress test on MTA servers with heavy custom map packs; capture frame-time percentile (`P50/P95/P99`) before/after.
2. Add low-overhead telemetry:
- queue depth,
- worker task latency,
- per-hook contention time.
3. If contention is high on `CStreaming::Update`, split lock granularity into stream-state vs col-link-state mutexes.

# Changelog
- 2026-03-05: Initial analysis + implementation validated with IDA MCP signatures/xrefs/callgraph, with MinHook integration into MirageAgent.
- 2026-03-05: Crash triage update: verified `ProcessLoadingChannel` ignores `ConvertBufferToObject` return value and relies on immediate load-state side effects; disabled deferred async-convert path by default for stability. Corrected hook target for `CCollision::CalculateTrianglePlanes` from internal block `0x4163E6` to function entry `0x416330`. Added crash stack-trace handler (`mirage_crash.log`) with IDA-relative addresses.
- 2026-03-05: User-requested max-boost mode enabled back: `kEnableAsyncConvert=true` and `kEnableAggressiveStreamGuards=true` for full async/guard path testing with crash-trace-driven iteration.
