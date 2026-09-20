# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`.

Latest formal full/body run `20260920T090448Z-b1f54e3cb38e` is finalized/unstaged from clean source
`14d387bdcf63d24b7eae7abe90c2d624f423bd50`, build-manifest ID
`4E5D9F08DFC4F523EC37B3886AFBE2AA491E83092A5AFC245662DC6042841195` and proxy SHA-256
`B60181C0FC094176AA5288147506D9876F124A3EACCE65DA7406ECB01BB70BC3`. It is a single-process run
with normal outer `run_end`, zero submit failures and a complete evidence package. See
`docs/research/evidence/20260920T090448Z-b1f54e3cb38e.json`.

The device-`Present` startup/fallback policy now has physical evidence through startup and one level
load. The user saw the game in VR from the beginning, Create recenter worked, the SteamVR interface
did not remain stuck over the game, loading completed and gameplay reached `native_stereo`. The
per-eye flat projection and Reset-safe immediate readback therefore advance beyond host-only status;
the intermittent dashboard behavior remains an observation rather than a resolved support claim.

The same run physically rejected Win32 mouse-button injection as the CoJ menu activation route. The
Sense `/pose/tip` ray and compositor cursor were visible and telemetry recorded 15 native Win32
button down/up pairs, but no Sense button activated the menu and keyboard/mouse was still required.
Shipped Java bytecode shows the active UI contract: `MainMenuModule.GetCurrentUI()` and
`GameUserInterface.CallEnterKeyPressed/Released()` emit global Enter input, while
`GameUILoading.WaitForUserInput()` stops the game timer until its exclusive input handler resumes it.
Current source routes L2/R2 UI-select through that exact Java path, observes the loading timer resume
and suppresses gameplay fire until the trigger is released. This correction is **host-tested** and
is the next physical menu/loading gate.

Local first-person head/hair suppression is now **headset-validated** for the exercised Ray/Billy
player: 103 logged hidden samples agree with the user's report that the head no longer obstructs the
HMD view. Body IK remains visually rejected. The run sustained 8,465 applications per arm and
16,930 clean restores with zero writer/restore failures, but clamp remained asymmetric (22.58% left,
53.68% right), the measured native chain stayed ~49.843 game units and average residual hand
orientation remained ~103.8 degrees left / ~142.9 degrees right. Current host source removes the old
12-unit pre-clamp target shortening and applies no controller-driven axial twist while retaining the
position solver, FORETWIST sibling swing and orientation diagnostics. This is an ablation candidate,
not a Body IK promotion.

The PS VR2 Sense binding manifest itself is coherent and the run delivered gameplay action samples:
left stick move/run-click, right stick snap-turn/crouch-click, L2/R2 fire, Cross jump, Square reload,
Triangle interact, Circle kick and L1/R1 weapon previous/next. The coarse locomotion came from the
bridge converting stick motion through desktop digital directional semantics. Current host source
uses a radial deadzone and sends actions 4-7 directly to `GameObject.CallOnInputGameController` with
their float values; run remains boolean and snap turn remains exact +/-45 degrees. Smooth locomotion
therefore remains **host-tested** pending the next headset run.

Controller `/pose/tip` direction is already written into the exact per-hand look direction, but the
user again observed shots originating from the native player/fire-origin boundary. Ballistic origin
ownership remains open; instrument the exact attack boundary before changing it so native
spread/accuracy and the network-forced path remain intact.

Candidate `20260919T170916Z-d9a22d24eb0c` was never launched and is unstaged. Its run ID is retired.
Full/body run `20260919T174647Z-67b3c560acd0` is finalized/unstaged. It used clean source `8697a81`, with
build-manifest ID `96ACFF43B38E16C1FAA5A1177180F3567561B0587B72D3B39FB7622B0911F7A5` and proxy SHA-256
`4A6562851FE4CAA9845740ECBA35BCF85FF37E499FD7E3D0574C3F7C8C2D50DF`. Body IK is enabled from
startup and the reversible `1920x1080`/FSAA0 profile active. It failed startup scene ownership before
the Sense menu path could run. The device-Present correction is host-tested and requires a new run.

Run `20260919T213924Z-d5d149a5bf46` is finalized/unstaged. It live-proves device-Present flat menu
capture, scene focus and Sense pointer/click/re-anchor, but the user saw a doubled binocular image and
the game crashed in `MeshObject.LoadMesh()` during level load before native stereo. Current source
adds per-eye 1.5 m flat-plane projection and Reset-safe immediate flat readback. Debug and Release
each pass 24 tests plus the expected classic-D3D9 shared-texture capability SKIP. The next physical
gate is therefore: fused menu -> pointer/select/re-anchor -> successful load -> same-process
`native_stereo`.

Run `20260919T153546Z-705460dca03b` remains the earlier clean snap-turn/recenter/body reference; its
head-intrusion and old reach/twist observations are superseded where the 2026-09-20 run exercised the
newer suppression and body candidate. Historical evidence below remains useful for the sequence of
rejected arm-composition hypotheses.

The audit-driven stabilization track remains authoritative. The exact-build camera/render
boundary and distinct two-eye ChromeEngine render path are live-tested. Corrected eye scale,
in-headset Sense recenter, SteamVR scene-focus handoff, clean runtime teardown and explicit
render-pose submission now have live evidence; the latter removed the reported head-turn
snap-back in run `20260916T224239Z-e43b46698e5c`. Sustained frame pacing/performance remains the
active presentation gate. Positional 6DOF and body/IK preflight were pulled forward as isolated
work: HMD translation can be reconciled
into player space, both Sense poses share the same recentered sample, exact CoJ skeleton reads are
available through the existing JVM and a measured two-bone arm overlay is implemented behind a
runtime toggle. Read-only lower-body preflight now measures pelvis/thigh/shin/foot geometry and
solves both leg chains with the animated knee plane, reach clamping and native foot basis. Run
`20260917T172007Z-e6232c4778d2` live-proved the exact single-player fallback
`LawmanGame.sm_cActiveGameModule -> LawmanModuleSingle.GetMainPlayer()`, actor reconciliation,
changing Sense tracking and successful left/right native arm writes. The user saw both arms follow
the controllers, but the mesh contorted behind/over the body. Static native inspection identified
the composition error: the writer consumes a complete element world transform, while the old path
replaced each element origin with its bone joint. Later run `20260918T165754Z-845101e7557b`
proved the replacement `RotateElementWithChildren` path reaches the visible mesh through both eye
draws, but physically failed because its solver world axes were interpreted by the native handler
as element-local axes; a restoration mismatch then fail-closed the overlay and returned control to
the game animation. Current host source preserves the native origins, converts each world axis
through the live parent-adjusted element frame, requires the elbow/wrist to reach their solved
targets and verifies natural-frame recovery. Runs `20260918T204701Z-f561e493f4ab` and
`20260918T210459Z-b59448961f3c` then proved that corrected local-axis math reaches both solved
targets for hundreds of frames, but each run eventually fail-closed on a nominal restore mismatch.
The comparison threshold was below one representable world-position float step at the live
~39,700-unit coordinates. Current source keeps strict mutation detection but uses a separate
float-aware sub-millimetre restore tolerance and reports the measured joint/element/axis errors.
Run `20260918T215118Z-c46320012ff0` physically validates the corrected tracked-Z mapping: moving
both Sense controllers forward now moves both arms forward. The run sustained 9,296 arm
applications/restores with no restore failure, but the arms remain severely deformed/twisted.
Position mapping is therefore no longer the active body blocker; controller orientation and
forearm/wrist roll/twist composition are. Until actor reconciliation
succeeds, physical HMD translation still fails closed to
rotation-only rendering instead of leaving the native body behind.
Run `20260918T233902Z-0cb2e565e886` then physically exercised controller orientation and the new
Sense gameplay route. The controls reached the game's native `GameInputController.InputAction.Translate`
path and were usable in-headset, but the arm gate still failed visually despite target reach. The
user's exported video shows severe wrist/forearm twisting and recurring local head/hair intrusion.
Shipped `EBones.class` inspection identified dedicated FORETWIST elements `9/14` that the failed
candidate skipped. Subsequent physical runs proved FORETWIST ownership and explicit handgrip poses
but still rejected the additional hand residual visually. Current host source routes pronation/
supination through FORETWIST and leaves the remaining hand residual diagnostic-only; this
`foretwist_only` composition awaits a fresh physical gate.
Run `20260917T163732Z-03df947b8d50` physically confirmed that visible fallback and reduced sampled
CPU copy to `10.246 ms` average / `12.937 ms` p95 at `1920x1080` FSAA0. Controller gameplay,
interaction rebuilding and lower-body writes remain downstream of the physical body gate.
Run `20260917T220704Z-b57a36497e54` then exercised the same performance profile from the current
tree with Body IK excluded by the manifest. It collected 4,471 frames with zero ring drops/submit
failures and reduced sampled CPU copy to `9.412 ms` average / `8.888 ms` p50, but p95 was
`14.808 ms`. The run does not promote the gate because its dashboard cycle happened before active
presentation, explicit disable-to-passthrough was absent, and presenter shutdown remained
`shutdown_complete=false`.

The same three 2026-09-18 runs identify two presentation facts. Their reversible test manifests
forced `1920x1080`/FSAA0, directly explaining the reported soft/low-resolution image; that profile
trades image quality for a sampled CPU-copy cost around `9.7-10.7 ms` instead of the earlier
`15-25 ms` at `2560x1440`. More importantly, head roll was omitted from the native camera while the
full HMD pose was submitted to SteamVR. Full native-basis roll is now host-tested and the live
verifier requires a meaningful tilt sample. Resolution and sustained frame pacing remain open
because raising the backbuffer resolution increases the provisional CPU-readback cost.

Run `20260918T213453Z-a789ac61ac91` physically validated the native-camera roll correction: the
user no longer experienced the head-tilt nausea, across telemetry samples from `-27.286` to
`+40.762` degrees. It also proved the float-aware arm restoration over all 6,930 frames: 13,860
left/right applications and restorations completed with no fail-close. The remaining body failure
was spatial/visual in that artifact. Both arms moved and reached the solver targets, but physical
forward/back was reversed; the arms became visible in front only when the controllers moved behind
the user. Run `20260918T215118Z-c46320012ff0` physically validated the corrected tracked-Z position
mapping, but still showed severe wrist/hand deformation with `hand_orientation=natural`. Current
host source implements calibration-relative Sense orientation while preserving that validated
position mapping. Run `20260919T002540Z-fc19b8ae78a4` then exercised dedicated FORETWIST ownership:
the first left-arm write changed the mesh and reached its positional targets, but missed the
calibrated hand orientation and was safely restored; the writer fault latch then kept subsequent
writes fail-closed, explaining the user's observation that the body did not move. Current host
source now computes the final hand residual from the hand basis observed after the native FORETWIST
write instead of from an idealized pre-write hierarchy prediction. Run
`20260919T003727Z-73f20e13cc1a` physically proved that revised writer path can remain active and
restore cleanly for an entire session (15,230 applications and 15,230 restores), but visual anatomy
still failed and sampled targets required extreme FORETWIST/hand rotations. Run
`20260919T011421Z-bef5076cd07e` then physically exercised explicit PS VR2 Sense `/pose/handgrip`
actions with no raw-role fallback. It sustained 14,968 arm applications/restores and seven safe
recenter recoveries, yet anatomy still failed. In the sampled palms-up pose FORETWIST was already
approximately symmetric (~89/~96 degrees average) while the remaining hand residual was still
extreme (~98/~122 degrees average). Current host source therefore leaves that post-FORETWIST hand
residual diagnostic-only and applies no hand-element rotation (`hand_rotation_mode=foretwist_only`).
`/pose/tip` remains reserved for the later weapon-aim boundary. Fresh Debug/Release suites pass; the
twist-only candidate requires the next combined physical body gate.

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
- Run `20260917T220704Z-b57a36497e54` exercised the consolidated Phase 6/performance profile and produced useful transport/timing evidence, but it did not satisfy the full verifier: the dashboard open/close occurred before active presentation, explicit disable-to-passthrough was absent, and joined presenter shutdown did not report completion. A fresh performance run must exercise those remaining checks after presentation is active while preserving connected/tracking/presenting state, repeated-frame presentation and the production `gpu_sync=none` handoff. `finish` generates and packages a run-bound timing/state summary with p50/p95/max values for the capture/readback/copy/upload/pose/submit stages.
- Phase 6 host/simulated acceptance: **complete**. No Call of Juarez or SteamVR process was launched for this increment; live/headset promotion remains separate and the active product gate is still sustained frame pacing/performance.

### Phase 7 — transactional deployment

- Basic preflight before mutation (game-process rejection plus Win32/x86 checks for the game/proxy/OpenVR/ChromeEngine artifacts): **host-tested**.
- Journal-before-mutation staging plus verified temporary file/directory installation before final replacement: **host-tested**.
- Recoverable interrupted stage/unstage with fail-closed handling of missing backups and externally changed destinations/temporaries: **host-tested**.
- Run-bound verification that cannot consume stale logs: **host-tested for the current native-stereo path**.
- End-to-end transaction matrix: **host-tested** — 15 staging failure checkpoints, 10 unstaging failure checkpoints, two repeated full cycles, plus active-`CoJ.exe` rejection before mutation. Phase 7 host acceptance is complete.

### Phase 8 — neutral VR math contracts

- Explicit eye-to-head vs time-located tracking-space pose semantics, with render-size recommendations separated from optical poses: **host-tested**.
- Units/axes/handedness/composition documentation: **host-tested**.
- Backend-independent asymmetric FOV/reference-projection validation: **host-tested**.
- Invalid/non-finite pose/FOV/projection rejection in neutral math and runtime adapters: **host-tested**.
- Phase 8 neutral math/semantic acceptance: **complete at host level**. OpenXR runtime/session ownership remains a separate experimental-backend lifecycle task under A10.

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

- Positional tracking and room-scale reconciliation: **implemented / live-tested exact-game path; broader comfort promotion pending**. HMD and both Sense poses share one OpenVR sample/recenter basis; run `20260917T172007Z-e6232c4778d2` live-proved campaign actor discovery through `LawmanModuleSingle.GetMainPlayer()` and successful player reconciliation. When a native actor is resolved, horizontal HMD displacement is absorbed at `100` CoJ units/metre while vertical displacement remains camera/body-owned. If actor discovery/reconciliation fails, physical head translation is suppressed while HMD rotation and stereo eye offsets remain active.
- Culling/visibility corrections: **planned**.
- HUD/menu strategy: **implemented / host-tested first slice; physical gate pending** — modal intro/menu/loading content uses `flat_theater`; the Sense ray/cursor/select path maps the anchored flat source to the exact game-client mouse seam and remains separate from gameplay input. Native stereo resumes automatically when gameplay returns.
- Cinematic and post-process handling: **planned**.
- Head/body/camera ownership: **implemented / host-tested first slice**. HMD remains camera authority; actor translation, native torso/head rotation and tracked arm overlay have separate owners. Comfort validation remains pending.

## Milestone 4 — controllers and interactions

- Minimal logical OpenVR global action seam: **headset-validated for recenter**. Neutral gameplay action state is additionally **host-tested**.
- PS VR2 Sense OpenVR/SteamVR binding: **headset-validated for left-Create recenter and exact +/-45-degree right-stick snap; gameplay route live-tested**. Run `20260918T233902Z-0cb2e565e886` physically exercised the native gameplay-input route, and run `20260919T153546Z-705460dca03b` recorded 23 exact snap steps through `PlayerBeing.RotateHorizontally(F)` that the user confirmed worked correctly. Right-stick vertical crouch remains; other bindings are left stick move/run, L2/R2 fire, L1/R1 weapon previous/next, Square reload, Triangle interact, Cross jump and Circle kick.
- Local head/hair suppression for HMD first-person rendering: **implemented; physical validation pending**. The exact local head/hair elements are hidden individually and reversibly through the shipped mesh API; the actor body remains present. Confirm in-headset suppression and shadow behavior before promotion.
- Physical crouch from calibrated HMD height into the native crouch action/state: **planned**. The existing right-stick crouch remains a gameplay binding, not physical crouch.
- Decouple weapon aim from HMD/crosshair view and drive shot direction from tracked weapon/controller orientation: **implemented / host-tested; physical firing validation pending**. Separate left/right `/pose/tip` actions now drive the exact native per-hand look-direction array after the game update and before rendering, while native spread/accuracy, fire origin and the network-forced branch remain game-owned. A clean firing run must prove that each hand's shots follow its Sense controller before promotion.
- Full-body IK driven by validated HMD/controller/body anchors: **visible writer/restoration and positional controller mapping live-tested; visual anatomy improved but still unpromoted**. The exact shipped bone IDs, head-anchored targets and measured two-bone shoulder/elbow/wrist solving remain behind `bodyIkEnabled`. Run `20260919T153546Z-705460dca03b` sustains 8,452 applications/restores without fault. Current host candidate adds bounded overreach compensation and caps shared axial FORETWIST/hand roll at 100 degrees while retaining the measured sibling hierarchy and diagnostic full wrist residual. Pelvis/thigh/shin/foot writes remain disabled; the new arm policy must be judged physically.
- Motion-controlled guns/reload/interactions where game boundaries permit: **planned**.
- Rebuild game interactions for VR instead of mapping all original flat interactions directly: **planned**.
- Per-game weapon/player adapters: **planned**.

## Milestone 5 — additional renderers/games

- Call of Juarez D3D10 backend: **planned**.
- Bound in Blood full VR backend: **planned**.
- Gunslinger full VR backend: **planned**.
