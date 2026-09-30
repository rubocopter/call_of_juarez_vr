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

## Safety and provenance invariants

The stabilization work that made physical testing trustworthy is now a
baseline contract rather than a separate remediation track:

- every physical candidate binds source commit/dirty state, supported-game
  SHA-256, build manifest, deployed proxy hash, a fresh run ID and restoration
  state;
- a run ID belongs to one game process; a second process requires a new
  `prepare` cycle;
- hooks own a specific object/generation, preserve foreign hooks and restore
  only their own mutations;
- ambiguous build, object, generation, camera basis, tracked pose, restoration
  or provenance state fails closed for game-specific mutation;
- game render callbacks produce frames, while the presenter owns compositor
  cadence, repeat behavior, scene focus and submission;
- `tools/vr_test.ps1 prepare` is the physical-test front door and `finish`
  collects available evidence and restores staging transactionally.

## Exact-build integration

Game-specific mutation is SHA-256 gated. A recognized filename is never sufficient to authorize exact offsets or bytecode/native seams. Unknown builds may use safe generic diagnostics only.

The active Call of Juarez integration is Windows x86 and the D3D9 API. The
native-stereo implementation currently has an experimental
`IDirect3D9Ex` factory/device path for shared eye images. Exact-game Ex startup
is now live-tested for the bounded production startup/reset path on the
inspected Steam build/host, and a live classic-D3D9 probe confirms successful
`D3DPOOL_MANAGED` texture creation, which an Ex device does not support. The
resource census is incomplete, so classic D3D9 remains the compatibility
reference while the transport/device decision is open. An independent
classic-D3D9 startup probe now establishes successful MANAGED texture, cube,
vertex-buffer and index-buffer requests, and immediate hook restoration
after BeginStateBlock prevents the observed loss of creation-hook coverage.
The native-stereo worker's 100 ms reacquisition is not equivalent to recovery
before the next engine call. The production Ex compatibility layer is now
**host-tested** for the exact observed MANAGED WRITEONLY VB/IB profiles: it
maps them to DEFAULT while preserving usage/format/FVF, and it immediately
reacquires its HookRegistry slots when BeginStateBlock returns. The host test
reproduced `D3DERR_INVALIDCALL` before those changes and now exercises
Lock/Unlock plus post-state-block creation successfully. Texture translation
remains DEFAULT|DYNAMIC. A manual production run now retains Ex identity,
completes a reset and sustains Present/capture through normal outer finalization.
The geometry-based texture extent correction is now live-tested: flat startup
content reaches the compositor, and videos/menu visibility is headset-validated
on PS VR2. Production shared native-stereo publication, D3D11 copy and successful
new-frame submission with an explicit render pose are now **live-tested** in
gameplay. Normal-quit inner presenter shutdown and complete finalization are
also **live-tested**. The short-run production cadence target is measured,
and the operator confirms correct stereo depth and stable head turns in-headset.
Sustained pacing and tail latency remain separate from that bounded acceptance.
The exact profiles and evidence limits live in
[the resource census](research/COJ_D3D9_RESOURCE_CENSUS.md).
D3D10 is a later
renderer target.
OpenVR/SteamVR is the primary runtime for the PS VR2 path; OpenXR remains
separate and experimental.

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

Flat-theater projection uses per-eye geometry rather than identical centered images, avoiding the earlier doubled-menu artifact. Texture padding uses both eyes' projected screen centers and asymmetric FOV to keep the entire source rectangle in bounds; the live optical trace confirms that a fixed 35% margin alone could not contain the PS VR2 startup image. The extent correction is live-tested, with videos/menu visibility headset-validated. Capture is immediate and does not retain default-pool resources across D3D9 reset/loading boundaries.

Create on the left Sense controller recenters/reanchors the current VR reference.

## Flat UI ownership

UI pointing is presentation plus exact-game UI policy:

1. OpenVR supplies `/pose/tip` and global UI actions.
2. The presenter intersects the Sense ray with the flat screen and reports normalized/source coordinates plus controller and hit positions.
3. The projected point is currently published to the game window with Win32 `SetCursorPos`, absolute `SendInput` mouse movement and `WM_MOUSEMOVE`. This route reaches shipped hover/select behavior, but the latest complete physical run still found it uncontrollable and required the physical mouse.
4. The previously discovered `MainMenuModule.GetGlobalCursor() -> UICursorGame.SetPos(LVector;)V -> OnMouseMove(FFI)V` route remains available for diagnostics, but the current candidate does not drive it simultaneously. Run `20260921T163309Z-481defca3401` showed chaotic pointer behavior when the Java cursor and Win32 input path both owned motion.
5. Cross is global accept, Circle is global back, and L2/R2 remain ray-select inputs.
6. Back dispatches normal Escape press/release through the active `GameUserInterface.CallOnInputKeyGlobal`. When gameplay has no current UI, it calls `LawmanGame.sm_cActiveGameModule.OnInputKey(Escape)` so the shipped module creates the pause UI. `MainMenuModule.ShowPrevUI()` is not a valid Escape substitute. Startup skip uses `IntroModule.OnInputKey`, while blocking load continuation uses `GameUILoading.OnInputKey(IZC)V` directly.
7. Paused-hint dismissal gets `HintManager` through shipped `LawmanModule.GetHintManager()` before calling `DisableCurrentHint`, avoiding direct inherited-field lookup on the old JVM.

The current single-owner Win32 pointer path is now live-exercised and its pointer is visible in the headset during startup/menu presentation. Accurate pointing and controller-only UI operation remain unvalidated. The preceding dual-owner route was physically rejected even though it could select menu items.

## Tracking, locomotion and body ownership

Room-scale HMD translation is camera-owned. The native actor keeps authoritative world position, grounding, collision and ordinary locomotion. Body yaw follows HMD yaw only outside the configured comfort cone; actor yaw must not be applied a second time when mapping controller targets.

Sense handgrip poses feed body/IK tracking. `/pose/tip` remains separately available for UI and weapon aim.

Native analog movement uses the shipped `InputAnalog` contract: per-axis 0.04 deadzone/saturation and native float actions 4-7. Each action follows only `m_Targets[InputSettings.GetTargetTypeForAction(action)]`, and analog updates reproduce the shipped `LockApplyControllerState -> dispatch/Translate -> UnlockApplyControllerState -> ApplyControllerState` transaction. Physical comparison now establishes native normal/walk speed and jump apex/duration parity with vanilla; movement/input/physics tuning is closed. The earlier visual impression of slow locomotion came from presentation cadence. Run remains boolean and right-stick snap turn is an exact ±45° actor rotation.

Physical crouch is detected from calibrated HMD-height change with hysteresis, but the HMD drop does not automatically press the native crouch action. The current candidate applies only the mapped horizontal room-scale component to the local pelvis/skeleton for both eye renders and restores the natural pelvis world basis after the second eye; the latest run observed 64 such writes with zero vertical offset. Vertical actor position, grounding and collision remain game-owned. A separate visual-animation contract is now required: horizontal room-scale displacement should drive a walk animation comparable to stick locomotion without moving the collision actor solely because the player walked inside the tracking area. Explicit controller crouch still uses the native action.

## Body IK

The body adapter reads the live Call of Juarez skeleton and builds game-space controller targets. Arm solving uses measured native segment lengths; it does not silently scale skeleton bones.

For the observed exact model:

- the native two-bone upper+forearm chain is about 49.843 game units;
- FORETWIST behaves as a sibling of forearm beneath upper;
- hand follows forearm and does not inherit FORETWIST roll;
- `EBones` ordering is semantic numbering, not parentage;
- visible writes use exact-build `RotateElementWithChildren(ILVector;F)V` with element-local axes;
- child/parent restoration is verified against the captured natural state and failures disable further mutation.

The visible writer and restore path are live-exercised, but Body IK remains visually rejected. Safety now denies unsafe writes instead of forcing extreme rotations; the latest run denied 43/128 sampled arm updates and the user saw the resulting ownership changes as repeated snap-back to the default game pose. Continuous tracked ownership is therefore a first-class requirement: the solver must remain plausible across normal reachable motion without alternating visibly between VR and native animation. Shoulder/clavicle participation remains a separate measured experiment. Lower-body writing remains unpromoted.

Normal successful arm tracking/restore telemetry is sampled to reduce synchronous logging overhead; faults, rollback and failed restoration remain unconditional evidence.

## Weapon ownership

The controller `/pose/tip` drives per-hand aim direction and visual origin through exact game fields:

- direction: `m_avLookDirDevForHand[hand]`;
- visual origin: `m_avAimFromPoint[hand]`;
- ordinary ballistic origin: `Being.m_vLookFromPoint`;
- native spread/accuracy remains downstream in the game weapon code;
- the network-forced attack branch remains untouched.

For local fire, shipped bytecode shows that `InputDigital.Translate` selects the requested hand/fire state while the actual attack runs later through `OnHandStateStarted_Attack -> WeaponAttack -> Weapon.Attack`. On the fire press transition, the adapter captures the native `Being.m_vLookFromPoint` value and publishes the selected controller origin for that pending native attack. A failed `Translate` rolls the field back immediately; on success, the later shipped `UpdateLookAndAimPoints` pass reclaims normal ownership after the attack transition. Held-fire frames do not repeatedly republish the field, and normal gameplay no longer creates a diagnostic `LaserPointer` object.

The attack-transition fire-origin path is physically exercised, but visible/ballistic origin and direction remain rejected. Current diagnostics emit `controller_aim_geometry` and `controller_aim_fire_transition`; the latest run produced 120 grip-to-tip axis samples with local `-Z` at `0.939388..0.939389`, strongly confirming the Sense tip direction convention. The remaining weapon contract is therefore downstream: the visible weapon pose, muzzle/barrel origin and ballistic path must agree. A temporary controller-tip gameplay ray may be rendered solely to compare tracked direction against the visible weapon; it is diagnostic and is not the production shot origin.

## D3D9 native-stereo transport

The previous native-stereo path was:

`DEFAULT render target -> StretchRect DEFAULT ring target -> GetRenderTargetData SYSTEMMEM -> LockRect/CPU copy -> mailbox -> D3D11 UpdateSubresource -> OpenVR`

At the physically exercised 1920x1080-per-eye source, readback/copy cost was
about 7-7.5 ms median, 9-9.5 ms p95 and 10-12 ms maximum. Disabling only that
boundary, while retaining both complete ChromeEngine eye renders, changed game
update cadence from about 83 Hz to 138 Hz. This is the demonstrated bottleneck.

The live-tested D3D9Ex path replaces it with:

`DEFAULT render target -> StretchRect D3D9Ex shared DEFAULT texture ring -> mailbox(handle + pose + lease) -> D3D11 OpenSharedResource -> CopyResource presenter texture -> OpenVR`

The ring has three slots, each containing separate left/right
`IDirect3DTexture9` render targets (`D3DUSAGE_RENDERTARGET`, `D3DPOOL_DEFAULT`,
non-MSAA) and an event query. The currently observed game source is render
target 0/backbuffer, `D3DFMT_A8R8G8B8`, `D3DPOOL_DEFAULT`, non-MSAA. `StretchRect`
is therefore one GPU copy per eye, not an MSAA resolve in the demonstrated
configuration. A future multisampled source would be resolved by that same
operation into the non-MSAA shared texture.

The producer ends a D3D9 event query after both eyes. At a later frame it first
polls without flushing. If the query has not yet been submitted, that slot gets
one `D3DGETDATA_FLUSH` poll; this submits pending commands but never waits, and
subsequent `S_FALSE` results leave the slot GPU-owned. A ready pair is published atomically with the exact
render pose and capture time. The presenter opens each shared handle on its
matching-adapter D3D11 device, queues one `CopyResource` per eye into its stable
OpenVR submission textures, ends a D3D11 event query and submits without a CPU
wait. The lease keeps both source textures unavailable to the producer until
that D3D11 query completes. A mailbox replacement or failure releases the same
lease safely; ring exhaustion drops/skips production rather than stalling the
game thread.

Resource lifetime is also host-tested across teardown and producer transitions.
A size/device/generation rebuild or explicit invalidation is deferred while a
consumer still owns a producer slot. Disabling diagnostic capture therefore no
longer destroys the ring. During finalization producer callbacks are stopped and
hooks restored first, then the presenter is joined so pending mailbox/bridge
leases are released before the D3D9 capture rings are destroyed. Steady-state
presentation never waits on those D3D11 event queries. Shutdown is the sole
exception: if copies are still pending, the presenter issues one bounded
D3D11 event-query completion barrier after them before releasing their leases.
If that drain fails or times out, the bridge retains the leases until its D3D11
session is destroyed instead of recycling D3D9 resources prematurely.

The native-stereo GPU path removes `GetRenderTargetData`, SYSTEMMEM staging,
`LockRect`, the approximately 16 MiB stereo CPU copy, sampled CPU eye hashes and
`UpdateSubresource`. It retains two necessary GPU copies per eye: the game
target-to-shared `StretchRect` and shared-to-presenter `CopyResource`.
`OpenSharedResource` itself does not copy. Both synchronization points are
nonblocking during steady-state presentation and telemetry reports
producer/consumer waits, queue/ring depth, drops, overwrites, frame age,
copy-fence retirement, submissions and repeats. The bounded shutdown-only drain
described above is not part of the frame loop.

The bounded production trace now measures approximately 136 Hz game updates
and 135.5 rendered stereo pairs/s, close to the readback-off reference and well
above the old CPU-transport path. Pair production rate is distinct from compositor
submission and headset refresh. The operator confirms correct depth and stable
head turns; sampled native CPU readback/copy and producer/consumer waits are zero.
The mailbox can replace unconsumed pairs, and ring drops and frame-age outliers
still occur. This establishes the short cadence target, not sustained tail-latency
or pending-frame reset/device-loss acceptance.

Full GPU sharing is not available from a classic `IDirect3D9` device: shared
DEFAULT-pool handles for DXGI/D3D11 interop require D3D9Ex. If the game cannot
create or run on the substituted Ex device, the candidate fails over at device
creation to the prior classic-D3D9 deferred CPU path and reports the reason.
That fallback still uses `GetRenderTargetData` and is not acceptable for the
current physical transport gate. `GetRenderTargetData` also remains in the
transient flat-theater/menu capture, currently sampled every second `Present`,
because that path composes CPU-side pointer/UI content and avoids persistent
DEFAULT resources across resets. It is not used by native stereo when the
D3D9Ex path is active.

A previous production D3D9Ex candidate crashed the exact game during physical
testing. Independent LTR research on the same game build has since moved the
compatibility boundary forward: a semantic adapter maps observed usage-0
MANAGED 2D/cube textures to `DEFAULT | DYNAMIC`, maps the observed MANAGED
WRITEONLY vertex/index buffers to DEFAULT while preserving their metadata, and
restores the device hook set immediately after `BeginStateBlock`. That research
path is **live-tested** through a bounded 120-Present startup window. A real
reset kept all nine tracked adapted resources alive; four directly hashable
textures retained identical contents and a generation-1 texture remained bound
in generation 2. The same research path also exercised the two-slot x86/x64
transport across reset: a stalled generation was cancelled and the new
generation completed 12/12 submissions with matching ready/done values and zero
sampled mismatches.

The production mod now contains the corresponding bounded VB/IB translation
and immediate `BeginStateBlock` hook reacquisition alongside its existing
texture translation. The bounded production compatibility/startup/reset path
is now **live-tested**: Ex identity, successful translated texture allocation,
reset, sustained Present and flat capture are observed. The corrected allocation
now passes the complete startup gate, with compositor uploads/submissions and
operator-confirmed videos/menu in the visor. Production shared native-stereo
frames now reach D3D11 and both compositor eyes successfully with their explicit
render pose. Bounded cadence, operator-confirmed stereo/head-turn stability and
normal-quit inner shutdown now pass. Texture contents across reset, pending-frame
production reset and device loss remain unpromoted. The research results are scoped evidence for
this build/host; they do not establish generic MANAGED or device-loss semantics.

OFXR-Bridge is not part of the active architecture. It is an experimental
OpenXR optical-flow frame-generation layer and cannot replace this D3D9/OpenVR
resource boundary.

## Evidence and lifecycle

Every physical candidate correlates source state, build manifest, deployed proxy, run ID, runtime telemetry and retained evidence metadata. Raw run material remains local under ignored `work/` and may be discarded after its durable conclusion is represented by source, tests or the focused research documents.

Validation states are:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

Earlier shared-stereo gameplay stopped finalization at capture cleanup after
incomplete inner presenter shutdown. A fresh physical run now completes the
exact pre-exit boundary, real owning-runtime shutdown, capture cleanup, hook
restoration and `run_end`. This normal-quit lifecycle is **live-tested**.

The candidate now finalizes before the exact executable's imported
`ChromeEngine3!DestroyGame`, with both binary hashes, the loaded call site and
export/import identity checked before mutation. The original call is preserved
and import restoration respects foreign ownership. This is **implemented /
host-tested / live-tested**: a real x86 DLL/host exit fixture shows owner cleanup before
original forwarding and DLL atexit, while its unhooked baseline skips cleanup.
The GPU fixture also verifies producer resource reclamation after pending
consumer copies drain. The physical run's unique final GPU/proxy records agree:
all consumer copies complete, with zero pending copies or abandoned leases and
all published producer leases reclaimed. Abnormal exit and device loss remain
**experiment-pending**.

An unpublished final producer frame is cancelled during resource release rather
than submitted after quit. A **host-tested** telemetry correction refreshes ring
depth after slot reset; the physical binary's final depth was stale despite
completed cleanup. A subsequent physical run now reports final depth zero,
all published leases reclaimed and consumer copies fully drained; the corrected
final statistic is **live-tested**.

Runtime state has explicit process lifetime; DLL static destruction does not
repeat GPU/COM/thread cleanup. Atexit only reports incomplete pre-exit shutdown
through a direct file write that avoids the shared logger mutex. It cannot
replace the success summary. See the exact contract and evidence limits in
[the shutdown boundary](research/COJ_SHUTDOWN_BOUNDARY.md).

## Primary validation hardware

Current physical development uses PS VR2 through SteamVR with PS VR2 Sense controllers. Hardware-specific bindings belong to assets/runtime input; game logic consumes logical actions and tracked poses.
