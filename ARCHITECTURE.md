# Architecture

## Product boundary

Call of Juarez VR turns the original Windows games into native-feeling PCVR experiences. Call of Juarez (2006) is the current reference implementation. The project may promote only game-neutral contracts that are independently demonstrated by another title; exact Chrome Engine layouts, RVAs, Java classes and gameplay behavior remain game-specific.

The supported end state is native stereo rendering, tracked head and hands, full-body IK and interactions rebuilt for VR.

## Layering

| Layer | Owns | Must not own |
| --- | --- | --- |
| Shared runtime | tracking/recenter policy, neutral eye data, logical actions, haptics, validation-neutral state | exact game addresses/classes |
| Renderer backend | D3D device/frame ownership, capture, transport, render targets, compositor presentation | player/weapon/UI semantics |
| Game backend | exact camera, actor, skeleton, UI, input, weapon and physics seams | reusable compositor policy |
| Diagnostics/evidence | provenance, structured telemetry, hook/device identity, run correlation | gameplay policy |

The architecture intentionally mirrors the useful separation already proven in the Penumbra VR Framework while keeping all HPL-specific implementation details out of this repository.

## Exact-build integration

Game-specific mutation is SHA-256 gated. A recognized filename is never sufficient to authorize exact offsets or bytecode/native seams. Unknown builds may use safe generic diagnostics only.

The active Call of Juarez integration is Windows x86 and classic D3D9. D3D10 is a later renderer target. OpenVR/SteamVR is the primary runtime for the PS VR2 path; OpenXR remains separate and experimental.

## Camera and stereo

The proven exact-game path is:

`Camera -> View/Projection -> ChromeEngine3 render-view wrapper -> D3D9 target -> presenter -> SteamVR`

The game camera remains authoritative. VR applies transient offsets around each render pass and restores the natural state afterward.

Key exact-game rules:

- source camera basis is right/up/forward/position;
- reflected or invalid bases fail closed instead of reconstructing an axis;
- HMD eye transforms are metres, Call of Juarez world units are centimetres;
- metres-to-centimetres conversion happens only in the Call of Juarez adapter;
- each eye must execute the complete render-view wrapper at `0x00030FB0`;
- core-only `0x00030E00` is insufficient for a valid second-eye pass;
- the exact render pose used to create a frame travels with that frame and is submitted through OpenVR explicit-pose submission.

Native stereo has physical validation for real distinct eye rendering, correct physical baseline, head orientation, positional offset and explicit render-pose behavior.

## Presentation modes

The D3D9 path has two presentation modes:

- `native_stereo`: two real game render passes with distinct eye content;
- `flat_theater`: a head-anchored finite-depth screen for startup, menus, loading and other non-stereo states.

The device `Present` path can publish flat content when no native-stereo producer is active. The presenter claims and maintains compositor scene ownership, repeats the latest frame at compositor cadence, and returns to native stereo when the game resumes the two-eye render path.

Flat-theater projection uses per-eye geometry rather than identical centered images, avoiding the earlier doubled-menu artifact. Capture is immediate and does not retain default-pool resources across D3D9 reset/loading boundaries.

Create on the left Sense controller recenters/reanchors the current VR reference.

## Flat UI ownership

UI pointing is presentation plus exact-game UI policy:

1. OpenVR supplies `/pose/tip` and global UI actions.
2. The presenter intersects the Sense ray with the flat screen and reports normalized/source coordinates plus controller and hit positions.
3. The Call of Juarez adapter resolves `MainMenuModule.GetGlobalCursor()`, updates its logical position with `UICursorGame.SetPos(LVector;)V`, then sends `OnMouseMove(FFI)V`. This keeps the game cursor visual synchronized but does not by itself own menu hover.
4. The same projected point is mirrored to the game window with Win32 `SetCursorPos` plus `WM_MOUSEMOVE`, because shipped UI hit-testing follows the real mouse position/process-mouse path. Physical evidence showed that moving the real mouse changed hover while logical cursor motion alone did not.
5. Cross is global accept, Circle is global back, and L2/R2 remain ray-select inputs.
6. Back dispatches normal Escape press/release through the active `GameUserInterface.CallOnInputKeyGlobal`; `MainMenuModule.ShowPrevUI()` is not a valid Escape substitute. Startup skip uses `IntroModule.OnInputKey`, while blocking load continuation uses `GameUILoading.OnInputKey(IZC)V` directly.
7. Paused-hint dismissal gets `HintManager` through shipped `LawmanModule.GetHintManager()` before calling `DisableCurrentHint`, avoiding direct inherited-field lookup on the old JVM.

The current implementation of this path is host-tested only. The previous physical run rejected the older cursor/selection route.

## Tracking, locomotion and body ownership

Room-scale HMD translation is camera-owned. The native actor keeps authoritative world position, grounding, collision and ordinary locomotion. Body yaw follows HMD yaw only outside the configured comfort cone; actor yaw must not be applied a second time when mapping controller targets.

Sense handgrip poses feed body/IK tracking. `/pose/tip` remains separately available for UI and weapon aim.

Native analog movement uses the shipped `InputAnalog` contract: per-axis 0.04 deadzone/saturation and native float actions 4-7. Run remains boolean and right-stick snap turn is an exact ±45° actor rotation.

Physical crouch is detected from calibrated HMD-height change with hysteresis, but the HMD drop does not automatically press the native crouch action. The latest physical evidence still exposed the full local avatar with `physical_crouch=true` and native `crouch=false`, so first-person body visibility is not explained by a double native-crouch transform. Physical vertical viewpoint remains camera-owned; actor position and grounding remain game-owned. Local-mesh suppression/ownership must be solved without losing tracked arms, shadows or future full-body behavior. Explicit controller crouch still uses the native action.

## Body IK

The body adapter reads the live Call of Juarez skeleton and builds game-space controller targets. Arm solving uses measured native segment lengths; it does not silently scale skeleton bones.

For the observed exact model:

- the native two-bone upper+forearm chain is about 49.843 game units;
- FORETWIST behaves as a sibling of forearm beneath upper;
- hand follows forearm and does not inherit FORETWIST roll;
- `EBones` ordering is semantic numbering, not parentage;
- visible writes use exact-build `RotateElementWithChildren(ILVector;F)V` with element-local axes;
- child/parent restoration is verified against the captured natural state and failures disable further mutation.

The visible writer and restore path are live-exercised, but Body IK remains visually rejected. Latest telemetry frequently reaches elbow/wrist positional targets while controller hand orientation remains far from the rendered hand, and reload animation visibly contorts the arms. Reach, shoulder/clavicle participation, hand orientation and native-animation ownership must therefore be treated as separate problems. Lower-body writing remains unpromoted.

Normal successful arm tracking/restore telemetry is sampled to reduce synchronous logging overhead; faults, rollback and failed restoration remain unconditional evidence.

## Weapon ownership

The controller `/pose/tip` drives per-hand aim direction and visual origin through exact game fields:

- direction: `m_avLookDirDevForHand[hand]`;
- visual origin: `m_avAimFromPoint[hand]`;
- ordinary ballistic origin: `Being.m_vLookFromPoint`;
- native spread/accuracy remains downstream in the game weapon code;
- the network-forced attack branch remains untouched.

For local fire, controller-derived ballistic origin may replace `Being.m_vLookFromPoint` only for the synchronous fire `InputDigital.Translate` call. The adapter captures the native value first and restores it immediately after the call, including the first press. No controller-owned ballistic origin persists across frames, and normal gameplay no longer creates a diagnostic `LaserPointer` object.

The transactional fire-origin path is host-tested. The latest gameplay process recorded 55 fire-pressed samples and reached normal `run_end`, so previous first-shot termination was not reproduced; however, visible/ballistic origin and direction remain physically rejected and no firing gate advances.

## D3D9 transport cost

The active classic-D3D9 path still performs CPU readback before publishing a stereo frame. To reduce avoidable cost without changing that structural boundary, the presenter samples eye-distinction hashes instead of doing a full RGB comparison every frame, and the mailbox recycles consumed CPU-frame storage so stable-resolution frames reuse their buffers. Telemetry exposes `cpu_storage_reused` plus readback/copy/producer timing.

Latest gameplay evidence measured CPU copy at 7.432 ms median, 8.994 ms p95 and 9.644 ms max for a 1920x1080-per-eye source while SteamVR recommended 3400x3468. A 120 Hz frame is about 8.33 ms, so this transport can consume the whole budget before remaining game/presenter work. A GPU-resident or lower-copy transport is the architectural priority before a large eye-resolution increase.

OFXR-Bridge is not part of the active architecture. It is an experimental OpenXR optical-flow frame-generation API layer; Call of Juarez currently submits through OpenVR after classic-D3D9 readback. It may be revisited only on a future OpenXR path and cannot remove the current D3D9 GPU->CPU boundary.

## Evidence and lifecycle

Every physical candidate correlates source state, build manifest, deployed proxy, run ID, runtime telemetry and retained evidence metadata. One staged run ID represents one game process; a second launch requires a new prepare cycle.

Validation states are:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

The current outer runtime can reach `run_end`, but recent body/playability runs still report incomplete inner presenter shutdown. That remains an open lifecycle issue and must not be described as resolved.

## Primary validation hardware

Current physical development uses PS VR2 through SteamVR with PS VR2 Sense controllers. Hardware-specific bindings belong to assets/runtime input; game logic consumes logical actions and tracked poses.
