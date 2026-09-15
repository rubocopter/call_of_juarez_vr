# Codex handoff — Call of Juarez VR

## Current checkpoint

Audit-remediation Phases 0-4 are **host-tested**, and two run-bound manual Call of Juarez observations have now been collected. Both failed specifically on device-vtable hook ownership after three frames; the second proved that the four lost slots return to their recorded Windows D3D9 originals. Do not begin Phase 5 or ask for headset-visible confirmation until the prepared Steam-Overlay-disabled A/B observation determines whether that restoration still occurs without `gameoverlayrenderer.dll` on the factory `CreateDevice` target.

No Call of Juarez, SteamVR or headset process was launched during the Phase 0-4 implementation/validation pass.

Read in order before continuing:

1. `AGENTS.md`
2. `docs/TECHNICAL_AUDIT.md`
3. `docs/AUDIT_REMEDIATION_PLAN.md`
4. this handoff
5. `docs/VALIDATION.md`
6. `ARCHITECTURE.md`
7. `ROADMAP.md`

## Evidence that remains authoritative

- Call of Juarez (2006) DX9 is the reference implementation.
- Classic D3D9 -> CPU readback -> D3D11 upload has succeeded in the exact game build.
- OpenVR has initialized against manually started SteamVR, returned a valid PSVR2 HMD pose and accepted D3D11 submissions.
- The historical in-game flat bridge completed genuine captured-game-frame submissions.
- Historical diagnostics observed exactly three project `Present`, `BeginScene` and `EndScene` callbacks, then detected loss of installed device-vtable hook integrity while monitor rendering continued.
- Run `20260914T214318Z-fe71b222b664` reproduced that cutoff with the remediated Phase 0-4 ownership/telemetry candidate: 3/3/3 device callbacks, three successful capture/upload/balanced-eye submissions, then simultaneous loss of `Reset`, `Present`, `BeginScene` and `EndScene` ownership on the same device vtable.
- Run `20260914T215121Z-9bac4e22cffd` resolved the target ambiguity: all four lost device slots returned exactly to their recorded original addresses in `C:\WINDOWS\system32\d3d9.dll`. The factory and swapchain hooks remained owned. The factory `CreateDevice` entry was already intercepted by `C:\Program Files (x86)\Steam\gameoverlayrenderer.dll` before the project installed its factory hook.
- Historical replacement-owner candidate `369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF` remains baseline evidence only.
- No game stereo camera, 6DOF or motion-controller gameplay is implemented.

## Phase 0 — host-tested

- Build manifests bind commit/tree/dirty state, diagnostic mode and artifact SHA-256.
- Staging requires a matching manifest, assigns a unique `run_id`, records exact game/engine/deployment identities and preserves older logs.
- Verifiers bind log, run manifest, staging state, copied build manifest and deployed hashes.
- Evidence collection recognizes structured `run_end`; absence of finalization remains explicitly incomplete.
- Provenance tests cover matching, altered-deployment and complete/incomplete paths.

## Phase 1 — host-tested

- Neutral runtime, build identity, diagnostics, OpenVR and OpenXR targets are separated.
- OpenVR/OpenXR are independently selectable and CI prepares only enabled pinned dependencies.
- Integration artifacts explicitly require Win32/x86.
- Native D3D9 tests assert the system runtime; proxy tests are isolated.
- HAL/capability unavailability is CTest SKIP, not PASS evidence.
- Readback validates full asymmetric frames, pitch and temporal changes.
- Fresh Debug and Release suites each produce 16 PASS and one explicit capability SKIP out of 17 tests.
- OpenVR-only and OpenXR-only Debug configurations produce the same outcomes with the disabled SDK root deliberately absent.

## Phase 2 — host-tested

- `VtablePatch`/`HookRegistry` provide conditional replacement, explicit outcomes, retained originals/ownership, per-vtable records, conflict-safe restore, synchronization and module pinning.
- Factory, device and swapchain hooks use the same registry model.
- Failure injection covers before/after replacement failures, conflicts, two vtables, reinstall, integrity loss, partial rollback, foreign-hook-safe restore and callback reentrancy.
- Native hook tests preserve original HRESULT behavior.

## Phase 3 — host-tested

- Classic `Direct3DCreate9` returns the native factory; native factory vtables observe all reachable `CreateDevice` calls.
- `GetDirect3D` canonical COM identity and device creation through a recovered factory are tested.
- Factory/device/swapchain IDs, creation thread and device generation are recorded; successful Reset advances generation.
- Hook events record original/replacement/current target modules.
- The D3D9Ex wrapper remains laboratory-only; classic staging rejects its environment switch and marker.

## Phase 4 — host-tested

- `COJVR_EVENT` JSONL binds every record to run/build, PID/TID and monotonic time.
- Events carry factory/device/swapchain/generation, sequences, duration, HRESULT/runtime result and exact process/per-device counters.
- Callback detail is sampled at bounded milestones; exact counters and active-stage state update for every observation.
- An observer thread emits per-device summaries and factory/device/swapchain hook integrity without re-hooking.
- Flat bridge stages are split diagnostically into capture, content publication, upload, pose wait, left submit and right submit.
- RGB content hashes ignore the D3D9 X/alpha byte; repeated captures do not advance content sequence even when submit attempts advance.
- Normal process exit emits `run_end`; smoke tests verify it. Missing finalization is rejected as incomplete.
- `tools/read_render_telemetry.ps1` and `tools/verify_d3d9_openvr_flat_live_test.ps1` report exact ownership/progression and reject malformed, mixed-run, failed-stage or incomplete evidence. The parser also exposes the factory `CreateDevice` original paths; a run can bind `validation.requireSteamOverlayAbsent=true`, which makes the verifier reject `gameoverlayrenderer.dll` automatically.

## Exact next gate

Runs `20260914T214318Z-fe71b222b664` and `20260914T215121Z-9bac4e22cffd` are complete and packaged. The second package SHA-256 is `0FF39018DF5FF9FF6B7AAFC76672B89BD3F84A813B431E5D228A8CC83EE99420`.

The exact-target diagnostic is now live-tested. It proved restoration to the native Windows D3D9 targets, not takeover by another hook. Production source contains no runtime callsite that invokes `HookRegistry::Restore` for the device hooks. Steam Overlay is the next controlled variable because `gameoverlayrenderer.dll` owns the factory `CreateDevice` target before the project hook, but this is a hypothesis rather than a root-cause conclusion.

The A/B verifier now makes the controlled variable explicit. Synthetic provenance and
telemetry paths pass in both Debug and Release, and the parser correctly re-identifies
`C:\Program Files (x86)\Steam\gameoverlayrenderer.dll` from historical run
`20260914T215121Z-9bac4e22cffd`. The previous staged run
`20260914T224423Z-3a4527d3e3ab` never produced a runtime log and is superseded before
manual execution. The replacement candidate is currently staged as run ID
`20260914T224904Z-overlay-ab` with `validation.requireSteamOverlayAbsent=true`, build
manifest ID `054813FAF3446544AC26F25D07D919EE2B893323736885F3701DBD7F877E4A93`
and proxy SHA-256 `8D5B93665E6E7067EAC02054EE2967ABB0DE6F8E3062386AB2C633B21CB9393B`.
Its manifest records a dirty source snapshot rooted at
`ef27ac7451370e54391534885a1bf8b59e6436d1`, so after this stabilization work is
committed the staged run remains tied to that captured snapshot rather than the new HEAD.

The remaining manual gate is:

1. Disable Steam Overlay for Call of Juarez before launching the game.
2. Start SteamVR and Call of Juarez manually, exercise the same DX9 gameplay path, then exit the game normally.
3. Verify from telemetry that the factory `CreateDevice` original target is no longer `gameoverlayrenderer.dll`.
4. Compare whether the device hooks still revert to the four Windows D3D9 originals after three callbacks.

The single verifier report must identify:

- every factory/device/swapchain/generation and which device received callbacks;
- whether all hook slots remained owned, plus the current owning module for any loss;
- exact per-device Present/BeginScene/EndScene/swapchain activity;
- capture/new-content/upload/left-submit/right-submit counts and stage failures/stalls;
- whether submit counts advanced while capture remained fixed;
- a unique normal `run_end` or an explicit incomplete run.

Do not choose Phase 5/6 work yet. The next run must discriminate Steam Overlay involvement before changing the interception boundary.

## Constraints

- Never launch Call of Juarez or SteamVR automatically.
- Do not use blind periodic re-hooking.
- Do not reactivate D3D9Ex as the main path.
- Do not add camera, stereo, 6DOF, UI or controller gameplay work yet.
- Do not equate submit count with new game content.
- Do not promote any Phase 0-4 result above `host-tested` until the exact manual run supplies live evidence.
