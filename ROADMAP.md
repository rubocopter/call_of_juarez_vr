# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`.

The current engineering priority is the audit-driven stabilization track defined in `docs/TECHNICAL_AUDIT.md` and `docs/AUDIT_REMEDIATION_PLAN.md`. Product milestones below remain valid, but camera/stereo/controller work is blocked until the stabilization gates are satisfied.

## Stabilization track — audit remediation

### Phase 0 — auditable provenance

- Source/build/deployment/run manifest: **planned**.
- Preserve historical logs and run-specific evidence: **planned**.
- Bind exact game/engine/proxy/runtime hashes to each run: **planned**.
- Separate known build from supported integration: **planned documentation/tooling correction**.

### Phase 1 — build and test validity

- Decouple neutral runtime from OpenXR dependency: **planned**.
- Make OpenVR/OpenXR independently selectable: **planned**.
- Bootstrap every enabled dependency in CI: **planned**.
- Assert system D3D9 in native tests and isolate proxy tests: **planned**.
- Explicit PASS/FAIL/SKIP semantics: **planned**.
- Exercise the active scene path and temporal readback behavior in host tests: **planned**.

### Phase 2 — safe hook infrastructure

- Replace ad-hoc global vtable mutation with `VtablePatch` / `HookRegistry`: **planned**.
- Conditional patch ownership, conflict reporting and complete rollback: **planned**.
- Multi-vtable/device support and integrity verification: **planned**.
- Failure-injection and conflict tests: **planned**.

### Phase 3 — factory/device discovery

- Preserve native COM identity while observing all relevant device-creation paths: **planned**.
- Track factory/device/swapchain/thread/generation identity: **planned**.
- Detect device recreation/new generations: **planned**.
- Keep D3D9Ex substitution isolated as a laboratory path: **planned**.

### Phase 4 — structured run/render telemetry

- `run_id` and source/build/deployment correlation: **planned**.
- Structured callback/stage entry/exit/timing: **planned**.
- Separate callback, capture, new-content, upload and submit sequences: **planned**.
- Periodic/final summaries and explicit incomplete-run state: **planned**.

**Next manual runtime gate:** only after Phases 0-4 meet their host acceptance criteria. This gate is a manual game-observation run; headset use is unnecessary.

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
- Current native device-vtable hook integrity: **live-tested failure mode** — one diagnostic observed three project frame callbacks followed by loss of installed-hook integrity while monitor rendering continued.
- Historical replacement-owner diagnostic `369754A6...A14A6AF`: **implemented / host-tested baseline artifact**; not the first action of the stabilization pass.
- Sustained, run-auditable changing game-frame capture and presentation: **blocked by stabilization track**.
- Sustained game image physically visible in headset: **planned headset gate after flat integration is auditable**.
- Rotational HMD tracking / 3DOF camera proof: **planned after stabilization**.
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
