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

## Capture/presentation separation

The historical flat diagnostic bridge performed capture, D3D11 upload, pose wait and OpenVR
submission synchronously from the game callback. That path proved transport but is no longer the
current sustained architecture.

The implemented native-stereo path is:

```text
ChromeEngine per-eye render-view boundary
  -> D3D9StereoCapture on the game/render thread
  -> owned CPU StereoCpuFrame
  -> bounded FrameMailbox
  -> OpenVrStereoPresenter with exclusive D3D11/OpenVR ownership
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

The OpenVR adapter owns explicit lifecycle state (`initialized`, HMD `connected`, scene `focused`, `tracking_valid`, stereo `presenting`, shutdown requested/completed) and processes relevant OpenVR events instead of inferring runtime health from submit count. One process-level owner gate controls OpenVR initialization; competing owners fail closed, and ownership is released by normal runtime teardown outside `DllMain`.

The presenter owns the D3D11 immediate context. Its normal handoff is explicit: CPU frame upload through `UpdateSubresource`, no unconditional GPU-wide wait, left/right `Submit_TextureWithPose`, then `PostPresentHandoff`. `none`, `Flush` and bounded D3D11 event-query synchronization exist as controlled diagnostic strategies; the production presenter remains on `none` unless evidence justifies a narrower change.

The isolated OpenVR path has demonstrated runtime initialization, eye configuration, valid HMD
pose acquisition and accepted D3D11 submissions. The exact Call of Juarez path has additionally
demonstrated sustained physically visible stereo submission, scene-focus handoff, repeated-frame
presentation, explicit render-pose submission and clean runtime teardown. Frame pacing/performance
remains the active physical gate.

## OpenXR direction

OpenXR remains experimental/future. Its existing work is useful research evidence, but it must be build-configurable independently and must not contaminate the neutral runtime ownership model.

OpenXR runtime/handle lifetime and neutral pose/FOV semantics must be corrected before that backend is promoted.

## Validation progression

Audit-remediation Phases 0-4 are the established host-tested baseline. The current
user-directed gate combines the corrected HMD camera convention with the first exact-build
native-stereo render-view proof:

1. auditable source/build/run provenance;
2. valid clean build/CI/tests;
3. safe hook ownership;
4. complete native factory/device discovery;
5. structured render/run telemetry;
6. exact-build `CBaseCamera -> view/projection -> renderer` static proof — complete;
7. one manual, non-headset proof of the camera/render boundary plus external FOV control and clean restoration — live-tested;
8. backend-neutral HMD pose/recenter boundary feeding the same transient camera path — headset/live-tested for the current OpenVR/CoJ path;
9. exact ChromeEngine render-view boundary plus per-eye translation/asymmetric projection — live-tested for the exact CoJ build;
10. first physical native-stereo attempt — failed before eye capture/submission because of an OpenVR vertical-FOV sign conversion defect and exposed source/frustum restoration occurring before later scene visibility work;
11. second physical attempt — reached visible in-game OpenVR submission, but all sampled left/right captures were identical and the run did not reach `run_end`; this is live evidence of headset presentation, not native stereo;
12. third physical attempt — HMD yaw/pitch direction was correct, but right-eye capture ended on `D3DFMT_NULL` because the implementation used core-only `0x30E00` for the second eye while the first eye traversed full wrapper `0x30FB0`; no stereo pair was submitted;
13. fourth physical attempt — both complete `0x30FB0` passes captured distinct real color RT0 results and submitted them to OpenVR; binocular gameplay was visible, but fusion/comfort and frame pacing were poor and shutdown remained incomplete;
14. corrected CoJ world scale, Sense recenter, scene-focus handoff and clean finalization — live-tested;
15. preserve the exact render HMD pose through the asynchronous capture/mailbox path and submit new/repeated frames with OpenVR explicit render-pose metadata — live-tested by `20260916T224239Z-e43b46698e5c`, which removed the reported head-turn snap-back;
16. reduce capture/readback/copy overhead and validate sustained frame pacing without regressing stereo geometry, recenter, scene focus, explicit render pose or teardown — current physical gate;
17. Phase 5 resize/Reset/new-device and paused-producer acceptance coverage — host-tested;
18. Phase 6 OpenVR state/ownership/failure simulation and controlled D3D11 synchronization — host-tested; one consolidated physical run now checks those contracts together with the active performance gate;
19. positional 6DOF and body/IK preflight are implemented at host level, including read-only pelvis/leg geometry plus measured two-bone leg solving; run `20260917T161917Z-909b63e114af` proved Sense tracking and showed Session-based campaign actor discovery is empty;
20. shipped bytecode supplies the exact campaign ownership route `LawmanGame.sm_cActiveGameModule -> LawmanModuleSingle.GetMainPlayer()`; the JNI bridge verifies the active module type and uses that fallback only for the empty-Session single-player case — live-tested by `20260917T172007Z-e6232c4778d2` together with successful actor reconciliation and arm-writer invocation, while visual arm composition failed and remains under correction;
21. unresolved/invalid actor reconciliation fails closed to HMD rotation plus native stereo eye offsets: room-scale head translation is suppressed until the actor can absorb it, preventing the render camera from walking away from the character body; run `20260917T163732Z-03df947b8d50` physically confirmed that visible-body fallback while body IK itself was disabled.

See `docs/AUDIT_REMEDIATION_PLAN.md` for phase acceptance criteria.

The camera probe is game/build integration. Exact RVAs, native layouts and
`ChromeEngine3.dll` identities stay below the neutral runtime boundary. It uses D3D9 only
as a bootstrap/forwarding DLL and does not depend on the known-fragile D3D9 frame hooks.

The HMD-rotation candidate adds a neutral `PoseSource` boundary plus
`RelativePoseTracker`. Runtime pose semantics are right-handed `+X` right, `+Y` up,
`-Z` forward, metres, quaternion `(x,y,z,w)`, local/device to tracking space. OpenVR is
the current producer because its x86 standing-space HMD path already has live evidence;
the Call of Juarez integration depends only on `PoseSource`. Recenter computes an absolute
relative orientation from a captured base (`R_base^T * R_current`), so no per-frame delta
is accumulated. Invalid/missing pose data immediately produces natural camera passthrough.
For the exact CoJ build, camera orientation has a paired native representation. The
world/camera transform begins at `+0x44`; its inverse/view source begins at `+0x04`.
Native setters keep them synchronized through matrix-inverse RVA `0x001F3F80`, then
`0x0022BB10` derives `+0x104` view, `+0x144` world/culling and `+0x204` view-projection.
The first derived-basis attempt was overwritten; the second `+0x44`-only attempt changed
world visibility/culling but not the visual camera. Synchronized paired-transform runs then
reached the visible first-person camera but exposed reversed yaw and incomplete environment
visibility. A later run also showed the right-hand weapon on the left side of the view. Exact
binary inspection resolved the shared cause: native `FromForwardUpPos` builds the source
matrix as **right/up/forward**, with `right = up x forward` at `+0x44`; the hook had written
the opposite left vector there, creating a horizontal reflection. Current source writes the
native right axis explicitly and keeps the paired world/view update and restoration. Run
`20260916T104036Z-24b3e3010d4c` then showed that this removes the mirrored character and
obvious culling/scene corruption, but same-sign HMD yaw still rotates the visible camera in
the opposite horizontal direction. The game-specific pose adapter therefore negates only
physical yaw; pitch keeps the direction confirmed by the live attempts. Runs on 2026-09-18 then
exposed a comfort defect on physical head tilt: the camera basis excluded roll while the compositor
received the full HMD render pose. Current host source extracts roll around the tracked forward axis
and applies it around the native forward axis with the sign inversion required by the tracking
`-Z` -> CoJ `+Z` reflection. The rendered right/up/forward basis and the pose used by
`Submit_TextureWithPose` now describe the same full head orientation; physical comfort remains
pending a fresh headset run.
The integration treats a rigid right-handed source basis as a runtime invariant: both source
world and source view/inverse matrices must preserve the native homogeneous `0/0/0/1`
layout, and natural/applied bases must remain orthonormal with determinant approximately
`+1`; otherwise the hook uses natural-camera passthrough.

The native-stereo candidate hooks the exact render-view entry at RVA `0x00030FB0`. Exact-build
disassembly shows this wrapper tests/sets a rendered-this-frame guard at `view+0xD7`, stores the
active view at owner `+0x3B8`, calls core RVA `0x00030E00`, then performs additional post-core
work. A live attempt that used the wrapper for the left eye and core-only `0x00030E00` for the
right eye left RT0 as `D3DFMT_NULL` after the right pass, so current source replays the complete
wrapper for both eyes. Before the second pass it clears only `view+0xD7` and restores the
natural post-left value after the wrapper returns. Per-eye data reaches the game through
`CameraStereoRuntimeCallbacks`, so the ChromeEngine adapter does not depend on OpenVR types.
`EyeView::pose` is explicitly eye-to-head; the adapter maps that translation into the native
right/up/forward camera basis. The neutral runtime stores translation in metres. CoJ gameplay
data explicitly documents `MoveSpeed` in `cm/s` and acceleration in `cm/s^2`, so this game
adapter converts eye translation with `100` game units per metre before composing it onto the
camera basis. OpenVR raw `top/bottom` projection signs are converted into the
neutral positive-up/negative-down `EyeFov` convention before mapping to engine frustum fields
at `+0x244..+0x250`. Each eye pass keeps source `+0x04/+0x44`, frustum and derived matrices
active through the complete render-view call, then restores the full snapshot including
`+0x84`, `+0xC4`, `+0x104`, `+0x144`, `+0x184` and `+0x204`. This matters because exact
disassembly shows scene/visibility work continues after the camera virtual update inside
`0x00030E00`.

For the first proof only, each native eye pass is captured from classic D3D9 into a small
GPU-copy ring, collected into owned CPU frames when ready, published through a bounded latest-frame
mailbox, uploaded by the presenter into separate D3D11 textures and submitted to OpenVR. The first
live submission attempt read the swap-chain
backbuffer while executing inside the render-view boundary; every sampled left/right pair was
pixel-identical even though the two eye camera/projection passes executed. Current transport
therefore reads the D3D9 render target currently bound at slot 0, records whether it aliases
the backbuffer, keys resources to that capture-surface description and rejects identical eye
hashes before OpenVR submission. `D3DFMT_NULL` is explicitly treated as an auxiliary/wrong
capture boundary and fails closed. The candidate observes device creation through the factory
hook and does not install `Present`, `BeginScene`, `EndScene` or `Reset` hooks. CPU readback,
owned CPU copying and upload remain proof-only costs that require frame-pacing work. Full-frame
diagnostic hashes are sampled telemetry only; every frame still performs fail-closed RGB eye
comparison. Contiguous D3D9 locks construct the owned byte range directly instead of first
value-initializing and then overwriting a same-sized destination vector. The capture component also
exposes explicit resource invalidation for owners with a real Reset/recreation lifecycle signal;
the exact CoJ proof does not depend on a Reset hook.
Run `20260916T133322Z-36c287cc43d8` live-tested distinct left/right engine captures and OpenVR
submission through this path, but the user reported poor fusion/comfort and performance. That
run used the pre-fix 1:1 metre-to-game-unit eye translation, making the 65 mm physical IPD only
0.65 mm in CoJ world scale. Current source fixes that adapter scale and logs the D3D9 viewport
plus applied per-eye position/frustum so the next run can separate geometric correctness from
transport cost. Sampled transport telemetry breaks out deferred D3D9 readback, CPU/hash/D3D11
upload, total eye capture and OpenVR submission time.

Run `20260916T221254Z-861f3c15abd4` subsequently proved clean presenter/runtime shutdown and
SteamVR scene-focus handoff, but exposed strong head-turn ghosting/elastic reprojection. The
presenter can submit an image substantially later than the HMD pose used to render it, and may
repeat that image while newer compositor poses continue to arrive. Current transport therefore
carries the exact raw HMD render pose and pose sequence with each `StereoCpuFrame`; the presenter
retains that metadata with the uploaded textures and submits both new and repeated frames using
OpenVR `VRTextureWithPose_t` / `Submit_TextureWithPose`. A missing or invalid render pose fails
closed before submission. This preserves the compositor's ability to reproject from the actual
pose associated with the image instead of implicitly treating the texture as if it were rendered
at the newest `WaitGetPoses` result.

Run `20260916T224239Z-e43b46698e5c` then physically confirmed that explicit render-pose metadata
removes the reported snap-back during slow/fast head turns and mouse rotation. The remaining
presentation problem is throughput. Telemetry showed approximately `15-22 ms` of owned CPU copy
and `11-14 ms` of full diagnostic hashing on sampled 2560x1440 stereo frames. The hot path now
checks left/right RGB inequality directly on every frame and computes full hashes only on the
telemetry samples that are logged; contiguous D3D9 locks use one bulk `memcpy`. This preserves the
fail-closed distinct-eye contract while removing diagnostic work from almost every frame.

Recenter now uses a small game-neutral OpenVR action boundary. The runtime owns the action
manifest, action-set/action handles and press-edge semantics; the current PS VR2 Sense profile
maps left Create to `/actions/global/in/recenter`. The ChromeEngine adapter receives only a
logical `recenter_requested` flag and forwards it into the existing `RelativePoseTracker`, so
controller paths and OpenVR handles do not leak into game camera code. The JSON/terminal command
remains a diagnostic fallback. The same OpenVR sample now also carries both controller-role poses.
`RelativePoseTracker::TransformPose()` maps those poses into the exact HMD recenter space without
mutating tracker state, and the neutral `BodyTracker` receives only XR-neutral poses.

Gameplay input is a separate logical state carried beside those tracked poses. The OpenVR adapter
owns `/actions/gameplay` and converts the current controller binding into a neutral
`GameplayInputState`; it does not expose OpenVR handles or PS VR2-specific paths to the game layer.
The exact Call of Juarez adapter then maps that semantic state to shipped action IDs and invokes the
existing `GameInputController.InputAction.Translate` route, preserving the game's configured target
device/code/sign contract instead of synthesizing process-global keyboard/mouse input. Loss of scene
focus, dashboard ownership or invalid tracking produces a neutral state so prior actions are
released. The CoJ action IDs/JVM route remain game-specific and are not a Chrome Engine contract.

The first exact-build body adapter keeps ChromeEngine ownership game-specific. It attaches to the
existing Java 1.4 VM through `JNI_GetCreatedJavaVMs` and resolves the current `NetPlayer.m_Being`.
`Session.sm_LocalPlayer` remains the primary source. Shipped `Session.class` shows that field is set
only when `NetPlayer.GetNetIsOwner()` is true, so campaign may leave it null; in that case the
adapter accepts `Session.sm_Players[0]` only when the session contains exactly one player. Any
zero-player or multi-player ambiguity fails closed. When that happens, the camera/body composition
also fails closed: HMD orientation and stereo eye offsets remain active, but physical tracking
translation is neutralized until a native actor can be proven and reconciled. The adapter uses
shipped `MeshObject` methods rather than hard-coded bone-memory offsets. Joint geometry still comes
from `GetBoneJointPos`, while arm write composition now reads the exact mesh-element world frame
with `GetElementPos`, `GetElementLeftVector` and `GetElementUpVector`.
`ChromeEngine3.dll` disassembly shows that `GetElementForwardVector` writes its output vector but
returns false unconditionally, so production reconstructs forward from the paired stored +X/up
axes instead of treating that return value as success. The same disassembly shows
`FromUpForwardPosElementWorld` normalizes forward, derives the first axis as `up x forward`, rebuilds
up and stores the complete element world position/orientation.

Arm lengths therefore come from the live animated shoulder/elbow/wrist joints, while native element
origins and axes remain separate mesh/bind state. A game-neutral two-bone solver places the
elbow/wrist targets. Runs through `20260917T223157Z-8061a216a065` established that correct
world-space targets are still insufficient when solved poses are written as absolute mesh-element
world transforms: both arms can track the Sense controllers yet remain reversed/contorted.

Run `20260918T160300Z-cd4137a48fca` closed the `BoneRotate(BLVector;FZ)V` question: 96 sampled
`body_arm_write_probe` records and 192 sampled `body_arm_render_probe` records all remained equal to
the natural arm geometry even though the JNI calls and inverse restores returned success. Tracking,
head-anchored targets and the two-bone solution remained valid. `BoneRotate` is therefore not an
active visible-mesh writer for this build and must not be retried without contradictory native
evidence.

The current exact-game candidate instead uses the shipped
`RotateElementWithChildren(ILVector;F)V` path at RVA `0x0009A070`. Exact-build disassembly shows that
it reads the element's existing world matrix, composes an axis/angle rotation onto that matrix,
invalidates the element/descendant matrix chain and refreshes attached child objects. It does not
replace the native element origin with a bone joint and does not rebuild the base animation pose.
The related `RotateElement` handler performs the same relative matrix composition without the final
child-object refresh; `RotateElementWithAnim` changes the animation-side transform, while
`CopyXformAnimToElement` copies an animation transform back into an element. Those latter routes are
kept as research evidence rather than mixed into the first render-element candidate.

The CoJ adapter still derives the shortest-arc upper-arm delta from natural shoulder->elbow to the
solved segment, applies that parent delta mathematically before deriving the forearm delta, and then
writes upper element before forearm element. Run `20260918T165754Z-845101e7557b` proved that the
writer reaches the visible mesh but rejected the initial axis convention: the native handler
post-multiplies its matrix and consumes an element-local axis, while the failed candidate supplied
world-space axes. Current source converts the upper axis through its live element frame, applies the
parent, re-reads the resulting forearm frame and converts the child axis there. The overlay is
transactional around the two stereo eye draws and is undone forearm-before-upper after capture.
Runtime validation now requires four
observable geometry stages: natural immediately before write, changed immediately after a non-zero
write, the same changed geometry after each complete eye render, and the original natural geometry
after restore. It also requires elbow/wrist agreement with the solved targets, preventing an
arbitrarily changed but misoriented mesh from passing. Inverse drift is corrected from the captured
complete natural element frames and verified; failure of both restore paths fail-closes the writer.
The controller-orientation extension also reads the native hand element and calibrates the animated
hand basis against the controller orientation at the current recenter/actor generation. Run
`20260918T233902Z-0cb2e565e886` proved the controller/gameplay route physically but rejected the arm
appearance: target reach could be correct while wrist/forearm anatomy remained badly twisted.
Inspection of the shipped `EBones.class` explains a concrete hierarchy mistake in that candidate:
the exact chains are left `upper=7 -> forearm=8 -> foretwist=9 -> hand=10` and right
`upper=12 -> forearm=13 -> foretwist=14 -> hand=15`. Controller pronation/supination therefore
belongs to the dedicated FORETWIST element rather than directly to the forearm element. Current
host source applies the relative controller delta as FORETWIST roll plus residual hand rotation
through the same element-relative `RotateElementWithChildren` contract. Mutation/persistence and
natural-frame verification include FORETWIST, and restoration runs hand -> FORETWIST -> forearm ->
upper. Telemetry names this ownership explicitly as `twist_owner=foretwist_element`. The FORETWIST
write is allowed to propagate through the real native hierarchy first; the hand frame is then
re-read and its final residual is recomputed from that observed post-FORETWIST basis instead of from
an idealized child transform. This avoids hard-coding a controller-local palm axis or an unproven
hierarchy composition and keeps the physically accepted hand-position mapping unchanged. Pelvis/leg
writers remain disabled until this corrected exact arm/hand path passes a fresh physical run.

Three additional first-person ownership boundaries remain deliberately separate from arm IK.
Head/hair suppression must hide only the local head geometry from HMD rendering while preserving
the rest of the body/shadow/animation. Physical crouch should be derived from calibrated HMD height
and enter the game through the existing exact-game crouch action/state rather than by moving the
camera independently of the actor. Firearm aiming must eventually separate weapon/muzzle aim from
the flat game's camera/crosshair authority and derive the shot direction from tracked weapon/
controller orientation. These are planned exact-game adapters, not consequences of the current
Sense button mapping.

The OpenVR presenter treats SteamVR's dashboard as a system-owned layer. A visible dashboard gates
scene submission and compositor-paced `WaitGetPoses`, while the latest valid game frame remains
retained for immediate resumption. Global game-action polling is also suspended while the dashboard
owns presentation. This policy is backend/presenter state handling; it does not change the CoJ
camera or body contracts.

The lower-body preflight uses the same exact skeleton readers without writing any lower-body
element. The game-specific adapter records the native actor position plus animated pelvis offset,
reads hip/knee/ankle and current thigh/shin/foot bases, and runs the same measured two-bone solver
with reach clamping. The knee pole comes from the live animated knee plane, falling back only when
that plane degenerates, while foot orientation remains the native animated basis. Sampled
`body_lower_tracking` telemetry exposes both sides, measured lengths, pelvis target, clamp state and
knee-plane validity. This is host-tested observation/preflight only until the arm composition gate
passes physically.

Run `20260916T153109Z-8976b8f77775` also demonstrates that modal/flat UI is a separate
presentation boundary: the game menu was not visible through the current native gameplay stereo
path, while returning to gameplay resumed HMD-driven rendering. Do not solve that by forcing the
menu through the exact gameplay camera hook. The later UI layer should treat flat menus/videos as
explicit VR presentation content (for example a compositor/scene quad or equivalent game-owned
surface) and may evaluate suppressing unnecessary 3D scene work while a fully modal flat surface
is active. That policy is not implemented or validated yet.

## Primary validation hardware

The primary headset target is PlayStation VR2 on PC through OpenVR -> SteamVR. The primary motion-controller target is the paired PS VR2 Sense controllers.

Physical headset/controller validation is intentionally later than host and game-observation gates. Do not require the user to wear the headset for evidence that can be obtained from structured game/runtime telemetry.
