# Architecture

## Product boundary

Call of Juarez VR is intended to be one user-facing project with shared VR policy and multiple integration backends. A common installer/bootstrap may eventually select the correct backend from the detected game and exact executable build.

Call of Juarez (2006) is the reference implementation. Reuse is evidence-driven: a Chrome Engine behavior is not promoted to a shared contract until at least one additional game demonstrates the same boundary.

The current stabilization architecture is governed by `docs/TECHNICAL_AUDIT.md` and `docs/AUDIT_REMEDIATION_PLAN.md`.

## Layers

1. **Neutral runtime** — renderer/game-independent poses, eye/view data, tracking-space policy, configuration, logical input and haptics.
2. **VR runtime adapters** — OpenVR or OpenXR lifecycle, runtime state, tracking conversion and compositor/session integration.
3. **Renderer backends** — D3D9 or D3D10 device/frame ownership, capture/transport, eye targets and presentation handoff.
4. **Game/build integration** — exact executable identity plus camera, player, weapon, UI and physics knowledge for one game/build.
5. **Diagnostics/evidence** — hook ownership, factory/device/swapchain identity, device generations, run telemetry and source/build/deployment/run provenance.

This mirrors the useful separation proven in Penumbra VR while keeping Chrome Engine-specific behavior local to this project.

## Integration matrix

| Game | Engine evidence | Renderer path | Initial integration |
| --- | --- | --- | --- |
| Call of Juarez | Chrome Engine 3 | D3D9 + D3D10 | D3D9 reference; D3D10 retained as first-class later backend |
| Bound in Blood | Chrome Engine 4 | D3D9 | shared contracts only after independent evidence |
| Gunslinger | later Chrome Engine branch | D3D9 | shared contracts only after independent evidence |

## Runtime boundary

Neutral runtime code must not contain Chrome Engine addresses, native object layouts, camera offsets, executable hashes or weapon assumptions.

Build/game identity belongs to an integration layer rather than to the neutral VR runtime. OpenVR and OpenXR are separate adapters and must be independently selectable: configuring the OpenVR path must not require the experimental OpenXR backend.

Renderer-neutral types must have one semantic meaning across adapters. In particular, eye-to-head transforms, poses in tracking/reference space, FOV, units, handedness and composition order must be explicit before real stereo camera work.

## Build identity

Filename matching is diagnostic only. Exact SHA-256 identifies known inspected builds. A known executable is not synonymous with a supported VR integration.

Game-specific modifications fail closed on unknown builds. Renderer/runtime diagnostics may operate on unknown builds only where their behavior is demonstrably build-independent and safe.

## Current D3D9 evidence

D3D9 remains the first integration target because all three inspected games expose a D3D9 path. Call of Juarez additionally has a D3D10 renderer with visible graphics improvements, so D3D10 remains a later first-class backend.

The basic transport chain has been demonstrated:

`classic D3D9 backbuffer -> CPU readback -> D3D11 texture -> OpenVR -> SteamVR`

Established evidence includes:

- forwarding/bootstrap and native D3D9 observation in the exact game build;
- successful classic-D3D9 CPU readback -> D3D11 upload;
- successful isolated OpenVR initialization, PSVR2 HMD pose and D3D11 submission;
- genuine in-game captured-frame submissions through the flat bridge;
- one diagnostic run in which exactly three project `Present`, `BeginScene` and `EndScene` callbacks were followed by loss of integrity of the installed device-vtable entries while monitor rendering continued.

The last point is a confirmed failure mode of the current interception design. It does not by itself prove which module caused the replacement, whether another factory/device/generation becomes active, or whether hook loss is the only condition preventing a stable visible headset image.

The historical `369754A6...A14A6AF` candidate can resolve replacement-slot ownership and remains useful baseline evidence, but the stabilization plan does not treat another headset run with that candidate as the first priority.

## Hook and device ownership target

Ad-hoc global vtable patching is not the target architecture.

The stabilization target is:

- safe conditional `VtablePatch` operations with explicit result states;
- `HookRegistry` ownership per vtable/device;
- rollback only for entries still owned by this project;
- integrity verification and conflict reporting;
- support for multiple factories/devices/vtables;
- `DeviceContext` identity for factory, device, swapchain, thread and generation.

Preserve native COM identity where possible. The preferred audit direction is to observe/intercept device creation on native/reachable D3D9 factories using the safe hook infrastructure.

A complete `IDirect3DDevice9` forwarding wrapper is not the default remedy for current hook replacement. It may be considered only if evidence requires it and COM identity, `QueryInterface`, `GetDirect3D`, reference/lifetime and device-discovery semantics are explicitly validated.

Do not use blind periodic re-hooking as the normal ownership model.

## Flat capture/presentation target

The current diagnostic bridge performs capture, D3D11 upload, pose wait and OpenVR submission synchronously from the game callback. This proved transport but is not the intended sustained architecture.

The target flat path is:

```text
verified D3D9 callback
  -> D3D9Capture on the game/render thread
  -> owned CPU Frame
  -> bounded FrameMailbox
  -> OpenVrPresenter with exclusive D3D11/OpenVR ownership
  -> compositor
```

The frame contract includes at least device ID, generation, capture sequence/time, dimensions, stride, explicit pixel format and owned storage.

No D3D9 COM resource crosses to the presenter thread. Reset/device recreation invalidates the old generation. The presenter may repeat the last frame to prove runtime continuity, but repeated presentation increments `submit_sequence`, not `capture_sequence` or new-content sequence.

This separation exists to distinguish engine capture progress from compositor progress and to prevent SteamVR synchronization from directly owning the Chrome Engine render callback cadence.

## Evidence architecture

Every meaningful runtime result must be correlated by `run_id` to:

`sources -> build manifest -> package/deployed hashes -> process -> device generation -> events -> verifier result`

Minimum structured events and fields are defined in `docs/AUDIT_REMEDIATION_PLAN.md`.

A successful OpenVR submission does not prove a unique new game frame. A missing final summary marks evidence incomplete. Historical log phrases must never validate a new artifact.

## OpenVR direction

OpenVR -> SteamVR remains the initial PSVR2 runtime path.

The OpenVR adapter should own and expose explicit lifecycle/state transitions rather than leaking them into renderer callbacks. One process-level owner controls OpenVR initialization. D3D11 immediate-context ownership and GPU handoff are explicit.

The isolated OpenVR path has demonstrated runtime initialization, eye configuration, valid HMD pose acquisition and accepted D3D11 submissions. Sustained, physically visible compositor behavior remains a separate validation gate.

## OpenXR direction

OpenXR remains experimental/future. Its existing work is useful research evidence, but it must be build-configurable independently and must not contaminate the neutral runtime ownership model.

OpenXR runtime/handle lifetime and neutral pose/FOV semantics must be corrected before that backend is promoted.

## Validation progression

The current critical path is audit remediation, not camera work:

1. auditable source/build/run provenance;
2. valid clean build/CI/tests;
3. safe hook ownership;
4. complete native factory/device discovery;
5. structured render/run telemetry;
6. manual game observation without requiring a headset;
7. separated capture/presenter path and formal OpenVR lifecycle;
8. transactional deployment and flat integration validation;
9. neutral math contracts complete;
10. only then rotational camera, real stereo, 6DOF and motion-controller integration.

See `docs/AUDIT_REMEDIATION_PLAN.md` for phase acceptance criteria.

## Primary validation hardware

The primary headset target is PlayStation VR2 on PC through OpenVR -> SteamVR. The primary motion-controller target is the paired PS VR2 Sense controllers.

Physical headset/controller validation is intentionally later than host and game-observation gates. Do not require the user to wear the headset for evidence that can be obtained from structured game/runtime telemetry.