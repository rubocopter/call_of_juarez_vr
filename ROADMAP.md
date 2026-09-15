# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`.

The audit-driven stabilization track remains authoritative. The exact-build camera/render
boundary proof is now live-tested, and the current engineering priority is the monocular
HMD-rotation gate: physical HMD rotation must drive the same proven camera path 1:1 on the
monitor. Stereo, positional 6DOF and controller gameplay remain blocked until that gate and
the later stereo/math contracts are satisfied.

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

**Manual runtime gate:** Phases 0-4 meet their host acceptance criteria. Runs `20260914T214318Z-fe71b222b664` and `20260914T215121Z-9bac4e22cffd` remain authoritative evidence for the three-frame D3D9 hook loss. The Steam-Overlay-disabled A/B remains unresolved and deferred. The separate exact-build camera-boundary gate passed on run `20260915T150554Z-7e0d7da45949`: external FOV plus yaw/pitch visibly changed the gameplay view, telemetry correlated the controlled camera with the ChromeEngine3 renderer, disabling returned to natural passthrough, and both camera hooks restored on normal exit.

### Camera/render boundary probe

- Exact `CoJ.exe` + `ChromeEngine3.dll` identity gate: **host-tested**.
- `CBaseCamera` vtable/profile validation and safe hook ownership: **host-tested**.
- Static `Camera -> view/projection matrices -> renderer camera` path: **implemented evidence record** from exact-binary host inspection.
- External JSON FOV/yaw/pitch control with non-accumulating orientation override: **live-tested**.
- Dedicated D3D9-forwarding-only proxy and run verifier: **live-tested**.
- XR-neutral `PoseSource` plus base-orientation/recenter policy: **host-tested**.
- OpenVR HMD pose feeding the transient CoJ camera basis through a D3D9-forwarding-only candidate: **host-tested**.
- Physical HMD -> monocular monitor-camera rotation with run-bound renderer correlation: **manual live gate pending**.

### Phase 5 — capture/presenter separation

- Game-thread `D3D9Capture` producing owned CPU frames: **planned**.
- Bounded `FrameMailbox`: **planned**.
- `OpenVrPresenter` with exclusive D3D11/OpenVR ownership: **planned**.
- Device/generation-aware resource lifetime and stale-frame rejection: **planned**.

### Phase 6 — OpenVR state/lifetime/synchronization

- Explicit runtime/focus/tracking/presenting states: **planned**.
- One process-level OpenVR owner and safe teardown: **planned**.
- Per-eye submit/timing evidence and controlled GPU synchronization tests: **planned**.
- Configurable animated visible probe: **planned**.

### Phase 7 — transactional deployment

- Preflight and active-process rejection: **planned**.
- Journal-before-mutation staging: **planned**.
- Recoverable interrupted stage/unstage: **planned**.
- Run-bound verification that cannot consume stale logs: **planned**.

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

- Pinned OpenVR SDK 2.15.6 bootstrap/build integration: **host-tested**, build/CI ownership cleanup pending.
- OpenVR runtime lifecycle, standing tracking space and neutral eye/HMD conversion: **live-tested initial evidence**, lifecycle hardening pending.
- OpenVR-selected D3D11 device and stereo compositor submission backend: **live-tested for isolated synthetic submission**.
- Isolated OpenVR runtime/eye/pose/submission probe: **live-tested technical submission**; headset-visible confirmation remains separate.
- D3D9Ex -> D3D11 shared render-target transport: **host-tested**.
- Direct classic-D3D9 shared render targets: **host-tested unsupported** on the development host (`D3DERR_INVALIDCALL`).
- Opt-in classic-API -> D3D9Ex proxy bridge: **host-tested; live-rejected** after an engine access violation in the exact game build.
- Classic D3D9 -> CPU readback -> D3D11 upload fallback: **live-tested initial transport evidence**.
- In-game classic-D3D9 -> CPU -> D3D11 -> OpenVR flat submission: **live-tested for initial genuine captured frames**.
- Current native device-vtable hook integrity: **live-tested failure mode** — the Phase 0-4 run-bound candidate reproduced three project frame callbacks followed by simultaneous loss of the four instrumented device hooks; the exact-target run proved that every lost slot returned to its recorded original function in `C:\\WINDOWS\\system32\\d3d9.dll`. The external actor remains unresolved; the Steam Overlay A/B is preserved as a deferred controlled experiment.
- Historical replacement-owner diagnostic `369754A6...A14A6AF`: **implemented / host-tested baseline artifact**; not the first action of the stabilization pass.
- Sustained, run-auditable changing game-frame capture and presentation: **blocked by stabilization track**.
- Sustained game image physically visible in headset: **planned headset gate after flat integration is auditable**.
- External engine-camera orientation/FOV control: **live-tested exact-build proof**.
- Rotational HMD tracking / 3DOF camera proof: **host-tested implementation; manual live gate pending**.
- Stereo eye projection: **planned after stabilization and math-contract gate**.

### Experimental OpenXR track

- Pinned OpenXR.Loader 1.1.63 bootstrap/build integration: **host-tested**.
- OpenXR instance/system/session, frame timing, D3D11 binding and stereo swapchains: **implemented**.
- SteamVR runtime/session/swapchain creation plus one projection-frame submission: **live-tested runtime evidence only**.
- Independent build ownership/lifetime cleanup: **planned under stabilization**.

## Milestone 3 — full 6DOF and comfort

- Positional tracking and room-scale reconciliation: **planned**.
- Culling/visibility corrections: **planned**.
- HUD/menu strategy: **planned**.
- Cinematic and post-process handling: **planned**.
- Head/body/camera ownership and comfort validation: **planned**.

## Milestone 4 — controllers and interactions

- Logical OpenVR actions and controller profiles: **planned**.
- PS VR2 Sense OpenVR/SteamVR bindings and controller validation: **planned**.
- Decouple weapon aim from HMD view: **planned**.
- Motion-controlled guns/reload/interactions where game boundaries permit: **planned**.
- Per-game weapon/player adapters: **planned**.

## Milestone 5 — additional renderers/games

- Call of Juarez D3D10 backend: **planned**.
- Bound in Blood full VR backend: **planned**.
- Gunslinger full VR backend: **planned**.
