# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`.

The audit-driven stabilization track remains authoritative. The exact-build camera/render
boundary and distinct two-eye ChromeEngine render path are live-tested. Corrected eye scale,
in-headset Sense recenter, SteamVR scene-focus handoff, clean runtime teardown and explicit
render-pose submission now have live evidence; the latter removed the reported head-turn
snap-back in run `20260916T224239Z-e43b46698e5c`. Sustained frame pacing/performance remains the
active presentation gate. Positional 6DOF and body/IK preflight were pulled forward as isolated
host-tested work: HMD translation can be reconciled
into player space, both Sense poses share the same recentered sample, exact CoJ skeleton reads are
available through the existing JVM and a measured two-bone arm overlay is implemented behind a
runtime toggle. Read-only lower-body preflight now measures pelvis/thigh/shin/foot geometry and
solves both leg chains with the animated knee plane, reach clamping and native foot basis. These
additions do not have live/headset promotion. Run `20260917T161917Z-909b63e114af` proved valid,
changing Sense poses but also proved that the campaign actor is absent from both current Session
discovery routes, so body promotion is parked. While unresolved, physical HMD translation now fails
closed to rotation-only rendering instead of leaving the native body behind. Controller gameplay,
interaction rebuilding and lower-body writes remain downstream of the physical body gate.

The supported-game end state is native stereo rendering, full-body IK and interactions
rebuilt around tracked VR input. These remain product milestones and do not bypass the
current camera, stereo, 6DOF and interaction validation gates.

## Stabilization track — audit remediation

### Phase 0 — auditable provenance

- Source/build/deployment/run manifest: **host-tested**.
- Preserve historical logs and run-specific evidence: **host-tested**.
- Bind exact game/engine/proxy/runtime hashes to each run: **host-tested**.
- Separate known build from supported integration: **implemented**; support promotion remains evidence-gated.

### Phase 1 — build and test validity

- Decouple neutral runtime from OpenXR dependency: **host-tested**.
- Make OpenVR/OpenXR independently selectable: **host-tested**.
- Bootstrap every enabled dependency in CI: **implemented**; CI execution remains external evidence.
- Assert system D3D9 in native tests and isolate proxy tests: **host-tested**.
- Explicit PASS/FAIL/SKIP semantics: **host-tested**.
- Exercise the active scene path and temporal readback behavior in host tests: **host-tested**.

### Phase 2 — safe hook infrastructure

- Replace ad-hoc global vtable mutation with `VtablePatch` / `HookRegistry`: **host-tested**.
- Conditional patch ownership, conflict reporting and complete rollback: **host-tested**.
- Multi-vtable/device support and integrity verification: **host-tested**.
- Failure-injection and conflict tests: **host-tested**.

### Phase 3 — factory/device discovery

- Preserve native COM identity while observing all relevant device-creation paths: **host-tested**.
- Track factory/device/swapchain/thread/generation identity: **host-tested**.
- Detect device recreation/new generations: **host-tested**.
- Keep D3D9Ex substitution isolated as a laboratory path: **host-tested**.

### Phase 4 — structured run/render telemetry

- `run_id` and source/build/deployment correlation: **host-tested**.
- Structured callback/stage entry/exit/timing: **host-tested**.
- Separate process/per-device callback, capture, new-content, upload and per-eye submit sequences: **host-tested**.
- Periodic/final summaries and explicit incomplete-run state: **host-tested**.

**Manual runtime gate:** Phases 0-4 meet their host acceptance criteria. Runs `20260914T214318Z-fe71b222b664` and `20260914T215121Z-9bac4e22cffd` remain authoritative evidence for the three-frame D3D9 hook loss. The Steam-Overlay-disabled A/B remains unresolved and deferred. The separate exact-build camera-boundary gate passed on run `20260915T150554Z-7e0d7da45949`: external FOV visibly changed the gameplay view, telemetry correlated the controlled camera with the ChromeEngine3 renderer, disabling returned to natural passthrough, and both camera hooks restored on normal exit. Later HMD evidence showed that run did not independently prove its old yaw/pitch injection point.

### Camera/render boundary probe

- Exact `CoJ.exe` + `ChromeEngine3.dll` identity gate: **host-tested**.
- `CBaseCamera` vtable/profile validation and safe hook ownership: **host-tested**.
- Static `Camera -> view/projection matrices -> renderer camera` path: **implemented evidence record** from exact-binary host inspection.
- External JSON FOV control and camera/render correlation: **live-tested**.
- External yaw/pitch orientation override: **live-tested through the paired world/view-source path with the native right-handed basis and game-specific yaw convention**; run `20260916T104036Z-24b3e3010d4c` isolated the final horizontal inversion and run `20260916T133322Z-36c287cc43d8` subsequently confirmed corrected yaw/pitch direction.
- Dedicated D3D9-forwarding-only proxy and run verifier: **live-tested**.
- XR-neutral `PoseSource` plus base-orientation/recenter policy: **host-tested**.
- OpenVR HMD pose acquisition/recenter through a D3D9-forwarding-only candidate: **live-observed input path**.
- Physical HMD -> monocular monitor-camera rotation with run-bound renderer correlation: **live-tested direction/basis path** — run `20260916T104036Z-24b3e3010d4c` proved the corrected right-handed basis and isolated the remaining yaw inversion; run `20260916T133322Z-36c287cc43d8` later confirmed corrected yaw/pitch direction while the native-stereo path was active.
- First native-stereo live attempt: **failed before stereo submission** — run `20260916T113600Z-native-stereo` acquired HMD pose and moved the visual camera, but an OpenVR vertical-FOV sign mismatch rejected every eye projection (`13,558` incomplete frames, `0` captures/submissions). It also exposed premature restoration of eye-specific state before later scene/visibility work. Subsequent candidates corrected both defects.
- Second native-stereo live attempt: **headset-visible but not native stereo** — run `20260916T123049Z-8d977bb5b439` executed both per-eye camera/projection passes and submitted frames to OpenVR, but every sampled pair had identical left/right pixel hashes because capture still read the swap-chain backbuffer at the render-view boundary. The run also ended without `native_stereo_runtime: stopped`/`run_end`. Subsequent candidates moved capture to active RT0 and reject identical-eye submission.
- Third native-stereo live attempt: **tracking correct, stereo capture incomplete** — run `20260916T130852Z-1438628c90c6` had correct horizontal/vertical HMD camera direction, but right-eye RT0 was always `D3DFMT_NULL`. Exact-build inspection proved the candidate used full wrapper `0x30FB0` for the left eye and core-only `0x30E00` for the right. The next candidate replayed the complete wrapper for both eyes and transactionally handled `view+0xD7`; the run itself remains diagnostic because its run ID contains three process starts and no clean `run_end`.
- Fourth native-stereo live attempt: **distinct native eye rendering live-tested; visual acceptance failed** — run `20260916T133322Z-36c287cc43d8` used one process start and the complete `0x30FB0` wrapper for both eyes. Both passes captured real `2560x1440` `D3DFMT_A8R8G8B8` RT0 content, hashes differed for every sampled pair, renderer-camera correlation was true for both eyes and frames were submitted to OpenVR. The user observed binocular gameplay, correct yaw/pitch direction, head-height placement and a complete-looking scene, but fusion/comfort and frame pacing were poor. Evidence is incomplete because shutdown again lacked `native_stereo_runtime: stopped` and `run_end`.
- Fifth native-stereo live attempt: **corrected scale and Sense recenter live-observed; performance/UI acceptance failed** — run `20260916T153109Z-8976b8f77775` applied the OpenVR eye baseline at `100` CoJ units/metre, kept distinct real-color eye submissions and physically validated left-Sense-Create recenter. The user reported very poor visual quality/frame pacing with discomfort. Opening the game menu did not present that menu in the headset; returning to gameplay resumed VR movement. Shutdown again stopped at `native_stereo_shutdown: stage=runtime_begin`, so the run is incomplete.
- Sixth native-stereo live attempt: **scene-focus/shutdown fixed; reprojection comfort failed** — run `20260916T221254Z-861f3c15abd4` completed with `runtimeEnded=true`, `incomplete=false`, `native_stereo_runtime: status=stopped` and `run_end`. SteamVR scene focus transferred to the CoJ PID on first submit and the dashboard no longer remained stuck over gameplay. The user reported better perceived performance, but strong head-turn ghosting/elastic snap-back remained. The run collected 5,139 frames, submitted 5,136 new frames plus 5,439 repeats and recorded one submit failure.
- Seventh native-stereo live attempt: **explicit render pose fixed snap-back; performance remains** — run `20260916T224239Z-e43b46698e5c` ended cleanly and the user tested slow/fast head turns plus mouse rotation. The previous backward pull/snap-back was no longer present and comfort improved substantially. The run collected 3,129 frames, submitted 3,127 new frames plus 6,099 repeats and recorded one submit failure. Remaining work is frame pacing/performance.
- Character/body yaw ownership: **planned with body/IK reconciliation**. Head rotation remains intentionally independent of the game-controlled body during the current camera/stereo gate; do not bind body yaw directly to HMD yaw as a substitute for the later body policy.
- Exact ChromeEngine render-view boundary (`0x30FB0` / core `0x30E00`) for two complete eye passes, including transactional replay of `view+0xD7`: **live-tested**.
- OpenVR eye-to-head semantics -> native per-eye translation and asymmetric engine frustum mapping: **live-tested structurally, including the corrected physical baseline scale**. Run `20260916T153109Z-8976b8f77775` observed `-0.032/+0.032` m as approximately `-3.2/+3.2` CoJ units at the neutral camera.
- Transactional restoration of derived camera state after each eye (`+0x84/+0xC4/+0x104/+0x144/+0x184/+0x204`): **host-tested**.
- First native-stereo transport (classic D3D9 active-render-target CPU readback -> separate left/right D3D11 textures -> OpenVR): **distinct-eye submission live-tested; proof-only performance remains unacceptable**.
- Minimal OpenVR global action contract + PS VR2 Sense left-Create recenter binding: **headset-validated for recenter**. Run `20260916T153109Z-8976b8f77775` proved the physical press reaches the existing XR-neutral `RelativePoseTracker` recenter path.
- Explicit render-pose submission for new and repeated frames: **live-tested** for the reported snap-back defect. Each transported stereo frame carries the exact HMD render pose/sequence and OpenVR submission uses `VRTextureWithPose_t` / `Submit_TextureWithPose`; invalid/missing render poses fail closed.
- Sampled diagnostic hashing with per-frame RGB distinction and direct owned-buffer construction: **host-tested**. Full hashes no longer consume roughly `11-14 ms` on every new stereo frame; contiguous locks no longer value-initialize the destination byte vector before copying the real pixels, and every eye pair still fails closed if its RGB content is identical.
- Usable binocular presentation/frame pacing: **current manual gate**. Clean shutdown, scene-focus handoff and snap-back correction are live-tested; remaining work is sustained performance/comfort.

### Phase 5 — capture/presenter separation

- Game-thread `D3D9StereoCapture` producing owned CPU frames: **host-tested**.
- Bounded `FrameMailbox`: **host-tested**.
- `OpenVrStereoPresenter` with exclusive D3D11/OpenVR ownership: **host-tested**.
- Device/generation-aware resource lifetime and stale-frame rejection: **host-tested**.
- Resize, explicit pre-Reset invalidation/post-Reset generation recovery and identical-format new-device resource rebuild: **host-tested**.
- Controlled paused-producer new/repeated presentation policy: **host-tested**; live telemetry already distinguishes new and repeated submissions.
- The separated path is **live-exercised** by the later native-stereo runs, including repeated-frame presentation, scene-focus handoff and clean teardown. Phase 5 host acceptance is complete; the active product gate remains sustained frame pacing/performance.

### Phase 6 — OpenVR state/lifetime/synchronization

- Explicit runtime/focus/tracking/presenting/shutdown states and relevant runtime-event processing: **host-tested**.
- One process-level OpenVR owner, conflict/release policy, move-assignment safety and teardown outside `DllMain`: **host-tested**.
- Per-eye submit-state integration, sampled submit/timing evidence and controlled `none`/`Flush`/event-query GPU synchronization strategies: **host-tested**. Production presentation keeps `gpu_sync=none`; no unconditional global wait was introduced.
- Configurable animated visible probe with recognizable per-eye animation, frame count and diagnostic GPU-sync selection: **implemented / host-built**; fresh physical probe evidence remains pending.
- The next native-stereo physical run is now a consolidated Phase 6/performance gate: the verifier requires connected/tracking/presenting state, a deliberate SteamVR-dashboard focus loss/reacquisition, repeated-frame presentation, the production `gpu_sync=none` handoff, and complete OpenVR shutdown in the same run. `finish` also generates and packages a run-bound timing/state summary with p50/p95/max values for the capture/readback/copy/upload/pose/submit stages.
- Phase 6 host/simulated acceptance: **complete**. No Call of Juarez or SteamVR process was launched for this increment; live/headset promotion remains separate and the active product gate is still sustained frame pacing/performance.

### Phase 7 — transactional deployment

- Basic preflight before mutation (game-process rejection plus Win32/x86 checks for the game/proxy/OpenVR/ChromeEngine artifacts): **implemented**. Full Phase 7 transaction acceptance remains pending.
- Journal-before-mutation staging: **planned**.
- Recoverable interrupted stage/unstage: **planned**.
- Run-bound verification that cannot consume stale logs: **implemented for the current native-stereo path**; the broader Phase 7 recovery matrix remains planned.

### Phase 8 — neutral VR math contracts

- Explicit eye-to-head vs tracking-space pose semantics: **planned**.
- Units/axes/handedness/composition documentation: **planned**.
- Asymmetric FOV/projection validation: **planned**.
- Invalid/non-finite transform rejection: **planned**.

## Milestone 0 — repository and initial evidence

- Win32 CMake project: **host-tested**.
- Exact-build SHA-256 catalog: **host-tested**.
- Runtime game classification: **host-tested**, with terminology/ownership cleanup pending under stabilization.
- SHA-256 implementation: **host-tested**.
- D3D9 availability probe: **host-tested**.
- Initial architecture/research documentation: **implemented**.

## Milestone 1 — transparent renderer bootstrap

- D3D9 forwarding bootstrap for Call of Juarez: **live-tested**.
- Verify unchanged non-VR rendering and clean unload: **live-tested**.
- Log exact host build and D3D9 device creation: **live-tested**.
- Observe D3D9 `Present` / `Reset` without replacing the native device object: **live-tested**.
- Repeat forwarding-only test on Bound in Blood and Gunslinger: **planned later**.

## Milestone 2 — OpenVR/SteamVR and first HMD proof

- Pinned OpenVR SDK 2.15.6 bootstrap/build integration: **host-tested**, with backend-independent build ownership established.
- OpenVR runtime lifecycle, standing tracking space and neutral eye/HMD conversion: **live-tested**, with lifecycle/ownership hardening **host-tested** under Phase 6.
- OpenVR-selected D3D11 device and stereo compositor submission backend: **live-tested for isolated synthetic submission**.
- Isolated OpenVR runtime/eye/pose/submission probe: **live-tested technical submission**; headset-visible confirmation remains separate.
- D3D9Ex -> D3D11 shared render-target transport: **host-tested**.
- Direct classic-D3D9 shared render targets: **host-tested unsupported** on the development host (`D3DERR_INVALIDCALL`).
- Opt-in classic-API -> D3D9Ex proxy bridge: **host-tested; live-rejected** after an engine access violation in the exact game build.
- Classic D3D9 -> CPU readback -> D3D11 upload fallback: **live-tested initial transport evidence**.
- In-game classic-D3D9 -> CPU -> D3D11 -> OpenVR flat submission: **live-tested for initial genuine captured frames**.
- Current native device-vtable hook integrity: **live-tested failure mode** — the Phase 0-4 run-bound candidate reproduced three project frame callbacks followed by simultaneous loss of the four instrumented device hooks; the exact-target run proved that every lost slot returned to its recorded original function in `C:\\WINDOWS\\system32\\d3d9.dll`. The external actor remains unresolved; the Steam Overlay A/B is preserved as a deferred controlled experiment.
- Historical replacement-owner diagnostic `369754A6...A14A6AF`: **implemented / host-tested baseline artifact**; not the first action of the stabilization pass.
- Sustained, run-auditable changing game-frame capture and presentation: **live-tested through the deferred Phase 5 path; performance remains the active gate**.
- Sustained game image physically visible in headset: **live-tested; frame pacing/comfort remain below the supported threshold**.
- External engine-camera orientation/FOV control: **live-tested exact-build proof**.
- Rotational HMD tracking / 3DOF camera proof: **live-tested direction/basis path** — run `20260916T133322Z-36c287cc43d8` confirmed correct yaw/pitch direction in the headset. The current positional extension is **host-tested**: horizontal room-scale translation is reconciled into the native actor while vertical translation remains camera/body-only, with previous accepted offsets removed to avoid double movement.
- Stereo eye transform/projection through the native engine render-view boundary: **live-tested distinct-eye path** — two complete `0x30FB0` passes, distinct real-color captures, corrected physical baseline scale, OpenVR submission, clean finalization and explicit render-pose reprojection are proven. Sustained frame pacing/performance remains the current manual gate.

### Experimental OpenXR track

- Pinned OpenXR.Loader 1.1.63 bootstrap/build integration: **host-tested**.
- OpenXR instance/system/session, frame timing, D3D11 binding and stereo swapchains: **implemented**.
- SteamVR runtime/session/swapchain creation plus one projection-frame submission: **live-tested runtime evidence only**.
- Independent build ownership: **host-tested**. OpenXR-specific runtime/handle lifetime semantics remain **planned** before promotion of that backend.

## Milestone 3 — full 6DOF and comfort

- Positional tracking and room-scale reconciliation: **implemented / host-tested, physical promotion parked**. HMD and both Sense poses share one OpenVR sample/recenter basis; when a native actor is resolved, horizontal HMD displacement is absorbed at `100` CoJ units/metre while vertical displacement remains camera/body-owned. If actor discovery/reconciliation fails, physical head translation is suppressed while HMD rotation and stereo eye offsets remain active. Run `20260917T161917Z-909b63e114af` showed the current campaign Session paths contain no actor.
- Culling/visibility corrections: **planned**.
- HUD/menu strategy: **planned; live-observed gap** — run `20260916T153109Z-8976b8f77775` showed that the flat game menu is not presented through the current native gameplay stereo path.
- Cinematic and post-process handling: **planned**.
- Head/body/camera ownership: **implemented / host-tested first slice**. HMD remains camera authority; actor translation, native torso/head rotation and tracked arm overlay have separate owners. Comfort validation remains pending.

## Milestone 4 — controllers and interactions

- Minimal logical OpenVR global action seam: **headset-validated for recenter only**; gameplay actions remain planned.
- PS VR2 Sense OpenVR/SteamVR binding: **headset-validated for left-Create recenter only**; tracked-hand/gameplay binding validation remains planned.
- Decouple weapon aim from HMD view: **planned**.
- Full-body IK driven by validated HMD/controller/body anchors: **implemented / host-tested, actor discovery blocked in campaign**. Exact shipped bone IDs, JNI `GetBoneJointPos`/`GetBoneDirVector`/`GetBonePerpVector`, measured two-bone shoulder/elbow/wrist solving and exact native `FromUpForwardPosElementWorld` arm writes are wired behind `bodyIkEnabled`. Run `20260917T161917Z-909b63e114af` proved both Sense poses are valid and changing, but no actor was exposed by `sm_LocalPlayer` or `sm_Players`, so no arm writer was reached. Pelvis/thigh/shin/foot solving remains read-only and lower-body writes stay disabled.
- Motion-controlled guns/reload/interactions where game boundaries permit: **planned**.
- Rebuild game interactions for VR instead of mapping all original flat interactions directly: **planned**.
- Per-game weapon/player adapters: **planned**.

## Milestone 5 — additional renderers/games

- Call of Juarez D3D10 backend: **planned**.
- Bound in Blood full VR backend: **planned**.
- Gunslinger full VR backend: **planned**.
