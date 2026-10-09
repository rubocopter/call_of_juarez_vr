# Architecture

## Product boundary

Call of Juarez (2006), Windows x86, is the reference implementation for native
stereo PCVR through D3D9 and OpenVR/SteamVR. The current presentation combines
tracked head and independent native hands with the game's torso and legs.
Connected full-arm IK and full physical reloads remain development work.
D3D10, OpenXR and additional games are separate future/experimental tracks.

Only game-neutral policy may be reused from Penumbra VR Framework. HPL layouts,
addresses and hooks do not apply here. Chrome Engine contracts remain CoJ-specific
until a second game independently demonstrates them. [VALIDATION.md](VALIDATION.md)
owns acceptance state and unresolved physical gates; [ROADMAP.md](ROADMAP.md)
owns development priorities.

## Layering

| Layer | Owns | Must not own |
| --- | --- | --- |
| Shared runtime | tracking/recenter, metre-space eye data, logical actions, haptics, gesture policy | exact game addresses/classes |
| Renderer backend | device/frame ownership, capture/transport, render targets, compositor | player/weapon/UI semantics |
| Game backend | exact camera, actor, skeleton, UI, input, weapon and physics seams | reusable compositor policy |
| Diagnostics/evidence | source/build/deployment correlation, hook/device identity, telemetry | gameplay policy |

## Safety and provenance invariants

Game-specific mutation requires recognized executable/engine/archive SHA-256
identity, not filenames. Unknown builds, ambiguous ownership, invalid bases/poses
and failed restoration fail closed. Hooks retain object/generation identity,
preserve foreign replacements and undo only their own changes.

Each physical candidate binds source state, build manifest, deployed hashes and
one fresh run ID to one game process. `tools/vr_test.ps1 prepare` builds, checks
and stages; `finish` collects evidence and restores original assets transactionally.
The operator starts and closes the game and SteamVR. Retained inventory and
backup identities are checked before any recovery mutation; missing or damaged
backups preserve the journal instead of permitting partial restoration.

The optional Large Address Aware derivative changes only the PE flag of the
recognized x86 executable. Original and derivative hashes are pinned separately;
code/RVAs remain identical and the executable is journaled/restored. This improves
user address capacity, not VRAM. Exact identities and allocation boundaries live
in [the resource census](research/COJ_D3D9_RESOURCE_CENSUS.md).

## Exact-build integration

The active renderer uses D3D9Ex shared textures for D3D11/OpenVR transport;
classic D3D9 remains the compatibility reference. The exact observed MANAGED
texture/cube requests translate to DEFAULT|DYNAMIC; WRITEONLY VB/IB requests
translate to DEFAULT while preserving usage, format/FVF and size. This is bounded
compatibility, not generic MANAGED emulation. Successful BeginStateBlock returns
immediately reacquire owned hook slots because the engine can rewrite its vtable.
A periodic watchdog alone cannot protect the next engine call.

The journaled `code.pak` patches target verified original classes only and retain
native fallback paths. The `Data0.pak` player-mesh partition is similarly hash
pinned. All original archives restore byte-for-byte. Optional quick/automatic
save-preview capture is suppressed at its exact invocation while ordinary save
serialization remains native; see [save thumbnails](research/COJ_SAVE_THUMBNAIL_PATH.md).

## Camera and stereo

`Camera -> View/Projection -> full ChromeEngine3 render-view wrapper -> D3D9 target -> presenter -> SteamVR`

The natural camera remains authoritative. Each eye uses a scoped transient
camera/view transaction and restores it afterward. The exact source basis is
right/up/forward/position; no cross-product reconstruction of its first axis.
Reflected/non-rigid bases fail closed. XR data stays in metres; only the CoJ
adapter converts to centimetres.

Both eyes execute full wrapper `0x00030FB0`, including its guard and post-core
work. Core-only `0x00030E00` cannot supply a valid second-eye result. Capture
reads each currently bound color RT0, not a swap-chain backbuffer that may echo
both eyes. Frame identity includes the exact HMD render pose/sequence submitted
through OpenVR explicit-pose submission. See [camera contracts](research/COJ_CAMERA_PATH.md).

## Presentation modes

`native_stereo` contains two game eye renders. `flat_theater` projects native
Present content onto a finite-depth head-anchored plane for startup, menus,
loading and blocking native UI. Per-eye geometry and asymmetric optics determine
texture extents; identical centered images or fixed padding are insufficient.

Game callbacks produce captured frames; the presenter owns compositor cadence,
scene focus, repeat behavior and submission. Immediate flat capture reserves a
mono SYSTEMMEM ring, with both eyes borrowing one producer lease. Consumer
completion permits owner-thread unlock/reuse. Matching same-device Reset retains
the reservation with fresh generation metadata; incompatible replacements defer
until leases release. No persistent DEFAULT resource or duplicate CPU eye image
is retained for this path.

## Flat UI ownership

The active exact `LawmanModuleSingle` supplies `IsTimerFreezed`,
`IsMainPlayerAlive` and `IsMenuMissionEndVisible` (menu 38). A stopped timer,
dead player, visible mission-end UI or unavailable observation yields native
stereo/body/gameplay ownership and captures native Present in flat theater.
A post-input check aborts the frame before body/eye writes if dispatch acquired
blocking UI. Death UI cannot be inferred from timer state alone.

The presenter intersects tracked `/pose/tip` with the flat screen automatically,
retains a valid hand owner and lets a fresh L2/R2 select that hand. No shoulder
activation is required. A valid tip missing the screen does not switch rays.
A hand/claim/click mailbox carries value-only coordinates to the game thread.

The exact native sprite-tree dispatcher `0xC8F00` receives previous/new source
positions before the visual cursor is moved. `GetMousePos` observes native input
in X/Z, normalized to source X/Y; visual cursor echo is not proof of hover.
The global MainMenuModule UI is preferred for pause; an actually visible
`m_cYesNoDlg` is the modal root. Object existence alone is insufficient.
Passive discovery uses `FindUI`/existing fields rather than creating UI factories.

L2/R2 select through native input-context mouse press/release at `0xCC420`.
Cross retains global accept, Circle UI-back and Options pause. Back uses native
Escape routing, not `ShowPrevUI`. Intro/loading and pausing hints use their own
shipped consumers; HintManager belongs to GameMode through `LawmanModule.m_GameMode`.
Native input readback must agree within one pixel. Physical mouse activity has
priority for 1.5 seconds; observation continues while the VR beam is hidden.

Queued clicks retain matching hand/claim/index identity through a confirming
Present. Failed delivery, owner/focus loss, navigation or UI-index change cancels
only the matching click. Held gameplay shoulders cannot cross into gameplay
until an available released sample. [Exact UI seams](research/COJ_UI_MOUSE_PATH.md)
record binary contracts and rejected sprite-only/Windows-input alternatives.

## Expanded campaign input

The runtime exposes semantic actions; only the CoJ adapter maps native IDs.
Shaped analog actions 4–7 execute under the shipped controller-state lock,
then unlock/apply before digital one-shots. Native F/reload/jump/kick/equipment
reject locked calls and are not replayed by ApplyState. Partial delivery retains
per-action history and unsent edges without replaying successful commands.

The control mapper owns release-confirmed radial commands, Focus/manual crouch
toggles and held-button barriers. The wheel consumes right-stick/digital gameplay
input, retains existing movement/toggles and blocks snap until a centered sample.
Native permission remains final; denied sectors require center/another sector
before becoming eligible again. Unavailable actions never count as releases.

Fresh verified `IsRidingHorse` observation selects on-foot kick or mounted gallop;
unknown/changed owner requires release. F can mount synchronously, so mount/context
is rechecked before later shoulder/crouch/body/fire consumers. On-foot native
action 16 merges manual and physical crouch; persistent toggle action 17 is unused.
Mounted/unknown owners suppress synthetic crouch and its camera compensation.
Create hold recenters once at 800 ms; a short released tap requests Objectives.
[The controls document](research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls)
owns the sole default layout and exact action mappings.

Optional wheel haptics describe fresh highlighting, not command success: right
hand, 12 ms, amplitude 0.18, 120 Hz, minimum 80 ms cadence and maximum 100 ms capture
age. Initial/reacquired/context/resource baselines are silent; stale/centered
captures and output failures never queue/retry. Per-hand output faults are isolated.

## Equipment wheel and wrist compass

Read-only game-thread readers copy native inventory permissions, actual rotor
IDs, visible waypoint/map orientation and owned HUD caches into bounded neutral
snapshots. No eligibility factory, timer, HUD or gameplay update is invoked.
JNI references never cross threads. The compass follows the captured left grip
and native guidance; invalid pose, back-facing geometry and blocking UI hide it.

The presenter caches transparent wheel/compass rasters and a separate wrist
status card. Native health/ammunition have priority within ten copied rows;
optional countdown, horse, concentration and posture/shadow rows fail independently.
Fatigue is not remaining stamina; shadow does not guarantee enemy invisibility.
Pixel rebuilds and bounded game-owner observation run at up to 10 Hz while each
captured frame retains its own geometry, optics and resource identity.

Ordinary Focus reads cached squint factor/maximum and applies
`1 + (maximum - 1) * factor` to copied asymmetric render-eye tangents. Eye transforms,
IPD, runtime and UI optics remain unchanged. It never calls CalculateZoom via
GetBeingZoom. Invalid caches/context fall back to physical optics; bows and scoped
Winchester remain excluded from this ordinary route.

## Essential gameplay text

Bounded readers preserve owned native visibility/alpha, localized hint/interaction
text and subtitle settings/dialogue ownership. UTF-16 snapshots travel with exact
capture identity and eye optics. The D3D11 presenter caches finite-depth Unicode
panels after copying fresh world content; empty snapshots remove old panels.

Copied prompts translate only explicitly delimited default campaign keys and
observed aliases into Sense gestures. Native strings/dialogue/bare letters and
digits remain unchanged; custom bindings and unobserved locale names are outside
the mapping. Text/status recovery is separate from graphical native HUD capture.

## Tracking, locomotion and body ownership

Native actor position, collision, grounding and locomotion remain authoritative.
Analog movement preserves the shipped per-axis 0.04 shaping and float actions 4–7.
Residual HMD yaw rotates the stick before shaping. Body yaw absorbs only excess
outside the 35-degree free-look cone; that yaw is removed once from tracked camera,
hand and weapon mapping. Snap maps positive stick X to native -45 degrees and
negative X to +45 degrees, retaining engage/release latching.

Room-scale camera and controller targets share tracking-Z/yaw conventions.
Only horizontal room-scale displacement enters the render-only pelvis overlay;
vertical actor position stays native. Physical crouch height uses a calibrated
hysteresis and compensates duplicate native camera descent without altering
collision. Recenter/player/mount changes invalidate the relevant references.
Physical-walk animation remains unresolved; root updates affect child animation
and gameplay events. See [animation ownership](research/COJ_ARM_SKINNING_AND_AIM.md#physical-walk-animation-ownership).

## Body IK

The active `-BodyIkAtStart` presentation hides the partitioned connected arm mesh
only during independent-hand ownership and retains native torso/legs. Twenty
named forearm/twist/hand/finger/holding-socket frames per hand receive one rigid
map; armed hands share the measured weapon map, unarmed sockets map to absolute
grip orientation. There is no upper-arm enlargement or span fit in this mode.
Native animation owns conventional reload. An explicit manual candidate can
retain the existing scoped weapon/independent-hand maps during an admitted native
reload session; its independent-hand presentation has scoped operator acceptance
in the exercised manual context. Both eye draws
verify frames and arm visibility.

Reload diagnostics are default-off, game-thread value snapshots of the exact
owner, native states/animation advance, reserve and raw ammunition readback.
They report controller/native ownership policy without changing that ownership.
An opt-in archive derivative supplies a bounded native wait/zero-transfer probe:
only an admitted paced ticket attaches its exact hand machine to a pre-Clear
veto. It waits 1.5 native-clock units, then retains native closing callbacks while
skipping ammunition transfer; explicit cancellation precedes save/load. This
does not itself enable persistent loading or tracked hands during reload. A
separate `-ManualReload` candidate extends this proven logical boundary with
fresh Square preparation, native watchdog supervision and one immediate original
`PawnArmed.WeaponReload` call per support-trigger insertion. The native game owns
ammunition, reserve, capacity and mechanical animation. VR owns input barriers,
cartridge gestures and the existing scoped weapon/hand presentation. Native end,
save/load and cancellation retain zero-transfer closing. No ammunition replica,
deferred credit or new body/IK solver exists. This candidate is host-tested and
bounded live-tested on a right-hand Peacemaker: correlated unit transfers,
sustained waiting and sampled scoped presentation/restoration recover to ready.
Square preparation/closure, continued hand tracking, repeated manual loading
and weapon/menu recovery have scoped operator acceptance. Exact port clearance,
prolonged comfort and other interruption contexts require further evidence.
The ordinary candidate remains unchanged; see
[reload ownership](research/COJ_RELOAD_OWNERSHIP.md#audited-input-animation-and-observation-boundary).

Parent hand/socket applies before child weapon; restoration restores the parent
before its captured child and then the outer pelvis. Captured positions rebase
by actor translation. Failed restoration retains references for retry and blocks
further mutation. Manual closing restores hands then weapon before invoking
native callbacks; a failed restore retains ownership and stops watchdog renewal.
An outer pelvis restore subtracts its removed offset from any
retained inner capture exactly once. The legacy connected-arm solver is diagnostic;
its hierarchy/writer limits remain in [arm research](research/COJ_ARM_SKINNING_AND_AIM.md).

Optional finger curls interpolate exact Ray/Billy authored relaxed-rest/fist
local orientations while preserving translations/lengths and Hand/socket frames.
All fifteen targets join the existing transaction. Empty-hand eligibility checks
actual/active/desired equipment and pending states, carrying and shared two-hand
ownership. Missing curls or ambiguity normally retain native fingers. A bridge-owned
manual opening/wait can admit the empty support hand through the existing native
reload-recovery reader plus a matching weapon/hand/probe snapshot. Actual carrying,
replacement, death and failed reads still reject it; the armed hand remains native.
During manual waiting the support uses current curls, or authored rest when no
curl sample exists. The estimated cartridge pinch requires a physical trigger
claim and held round, not merely an open weapon or a replenished available token.
Curl zero means authored rest, not maximum opening.

The manual cartridge uses the full third-party CC0 .44 Magnum OBJ (122 positions,
224 triangles), baked into bounded runtime geometry with authored colors. It is
captured with the stereo pair and batched into one compositor draw per eye. Its
centre follows the midpoint of verified displayed distal thumb/index joint frames,
converted from native centimetres into tracking metres. This is an attachment
reference, not measured finger-pad contact. Missing or unrestored hand/weapon
presentation hides the manual cartridge. Native ammunition and the existing grip
insertion gesture are unchanged. Scene-depth occlusion and physical visual fit
remain pending; asset provenance is in [assets/models](../assets/models/README.md).

## Weapon ownership

Tracked grip anchors the authored holding socket; tip -Z supplies direction.
The adapter rigidly maps all measured weapon elements through
`FromUpForwardPosElementWorld`, preserving internal animation/socket-to-muzzle
geometry. The general object setter is unsafe because it reaches recursive
`+0x82F00` notification. Readback verifies the complete muzzle basis through both
eyes before ray publication. Shared two-hand weapons have one visual owner.

Exact-class nullable per-instance vectors feed native ballistic/visual origin
and direction consumers after current muzzle publication. Null caches, unknown
hands and network-forced attacks retain original methods; native spread remains
downstream. Invalid/menu/generation boundaries clear ownership. Persistent FX
use detached configured emitters, full native barrel frames and FXDetach commit;
a starting transform alone is insufficient. Native counters demonstrate consumption,
not visible hits/effects. [Weapon research](research/COJ_ARM_SKINNING_AND_AIM.md)
retains exact consumers, emitter layout and authored rearward-trail interpretation.

Contextual F now uses the left Sense aim ray through exact local-player
CheckTriggers, retaining native range, collision-element and permission decisions.
Missing publication suppresses Action until an available release; there is no HMD
fallback. The small cyan reference follows that ray only while L1 is held in eligible
gameplay, outside the wheel. Its nominal 1.5 m depth is presentation geometry,
not a collision hit or native range. Cached active/executing trigger diagnostics
are passive, bounded and sampled before/after fresh F edges; they never trace or
select. This replacement's physical gate is separate from the accepted older
HMD-based drawer interaction.

Motion reload is a hybrid request into native action 31, retaining Square and
native ammo/reserve/state/animation authority. Exact passive admission supports
four audited single-pistol/empty-support classes; the paced single-round extension
is limited to Peacemaker/Frontier. A waist pickup, held journey and release near
the grip requests one native round. Following an observed reload interval, ordinary
eligible ownership must recover before the presentation-only solid token replenishes.
Fresh trigger press/release can then request another round without returning to waist.

An explicitly selected archive probe can veto the owned native body-state exit
before `Clear`, wait for a bounded native-clock deadline and close through native
callbacks while skipping the ammunition transfer. This logical boundary has
bounded Peacemaker runtime evidence. It retains native reload presentation;
held logical state does not establish a fixed mechanical pose or tracked hands.
The default path continues to transfer one native round per admitted gesture.

A separate passive recovery reader preserves an accepted continuation through
verified native reload/two-hand animation. It proves no carried object via
IsCarrying, rather than mistaking HasSomethingInHand's animation fallback for a
world object. Held support trigger is consumed through verified reload until an
available release, preventing cross-hand attack. No VR ammo counter or retry queue
exists. Precise cartridge-tip/chamber policy and displayed drum/gate observation
are implemented separately but not enabled as physical loading. Full gate/ejection,
persistent mechanical pose and engine save/load recovery remain unresolved.
The opt-in manual policy uses `READY -> OPENING -> MANUAL_LOAD -> CLOSING -> READY`.
It consumes rejected Square admission until release, preserving native fallback
only for positively identified unsupported contexts. Missing observation cannot
start conventional reload. Invalid owner/context/poses/input availability,
recenter, stale samples, incompatible actions or presentation failure close the
session. Native-clock watchdog expiry is a recovery fallback, not a fixed mesh
frame. JNI cleanup retains the player on failed cancellation for retry. See
[reload ownership](research/COJ_RELOAD_OWNERSHIP.md).

Optional insertion feedback is qualified on the game thread by a completed,
accepted native call and coherent same-weapon/hand readbacks of loaded +1 and
reserve -1. The adapter copies one value-only event into that capture's HUD
snapshot; native references and ammunition state remain game-owned. The OpenVR
presenter can pulse the free hand once (18 ms, amplitude 0.25, 120 Hz) only on a
fresh accepted stereo upload with matching capture/input/native context epochs,
valid tracking and current gameplay focus. Maximum capture age is 100 ms and
minimum pulse cadence is 80 ms.
An opaque game-owned epoch also binds the latest observed player/weapon/armed
hand identity; replacement, cancellation or missing observation invalidates
queued events even if gameplay availability remains unchanged. The presenter
rechecks this atomic epoch immediately before optional output. No native
reference or game layout crosses that boundary.
Loss/reacquisition and resource replacement
establish silent baselines. Dropped captures, rejected insertions and output
faults never queue or retry feedback and cannot affect native loading. This
policy is host-tested, with live submission records and operator acceptance of
insertion-only delivery in the exercised manual context. Broader recovery and
prolonged comfort remain pending; Validation owns those acceptance limits.

Protected-target feedback can color the per-eye aiming cross red only from coherent
native warning/trace-owner/ray observations. Invalid observations clear it. Source
implementation and host checks do not establish visor visibility. Billy's whip
uses its authored holding-socket cache and naturally scheduled AdditionalSynchro;
no rigid whole-rope transform or forced simulation is introduced.

## D3D9 native-stereo transport

`RT0 -> StretchRect shared D3D9Ex DEFAULT ring -> handle/pose/lease mailbox -> D3D11 OpenSharedResource/CopyResource -> owned OpenVR textures`

Three slots hold separate non-MSAA eye textures and a D3D9 event query. One
bounded FLUSH poll can submit commands, but the producer never waits. Ready pairs
publish atomically. A D3D11 completion query retains the producer lease until
both consumer copies complete. Ring exhaustion skips production; replacement or
failure releases leases safely. Rebuild/invalidation defers while a consumer owns
a slot. OpenSharedResource itself does not copy.

This removes native-stereo CPU readback/LockRect/UpdateSubresource; immediate flat
capture remains separate. The classic CPU stereo path is diagnostic compatibility,
not the active performance path. Telemetry separates production, new submissions,
repeats, drops, frame age and query retirement from compositor refresh.

The game producer cap follows the HMD display-frequency property. One bounded
cancellation-aware CPU schedule covers stereo and flat modes, rebases at rate/reset/
pause changes and samples latest tracking afterward. Valid HMD timing selects
IMMEDIATE presentation to avoid monitor-vsync double wait; desktop mode remains
native. Failed Create/Reset restores caller parameters and does not commit cadence.
Missing timing retains the last valid cap; late timing can require native Reset.
The cap is not compositor phase lock; WaitGetPoses owns presenter cadence.

## Evidence and lifecycle

The exact pre-DestroyGame import boundary stops producers/restores hooks, joins the
presenter, drains pending GPU copies, releases capture resources and forwards
native destruction once. Atexit is too late for worker cleanup and only reports
incomplete shutdown without COM/GPU/thread work. [Shutdown contracts](research/COJ_SHUTDOWN_BOUNDARY.md)
retain exact hashes/RVAs, ownership and final-counter requirements.

Steady-state GPU queries do not wait. Shutdown alone permits a bounded completion
barrier; failed drains retain leases until D3D11 session destruction. Final GPU/
proxy summaries must be unique and agree on copied/completed totals, zero pending
copies/abandoned leases and full published-lease reclamation.

Raw videos/logs/dumps and run metadata belong under ignored local evidence and may
be removed once their durable conclusions are captured in source, tests and focused
research. Active staging/recovery journals and their original backups must remain
intact. Cleanup cannot substitute for finish or promote a physical gate.

## Primary validation hardware

PS VR2 with Sense controllers through SteamVR is the development reference.
Bindings belong to runtime assets; game logic consumes logical actions/poses.
Compatibility with other devices requires independent acceptance.

## Physical-candidate image quality

`prepare` preserves the selected game resolution and stages FSAA 0 unless the
operator elects to keep video settings. Original video configuration is journaled
and restored by `finish`. Source resolution, eye target extents and runtime optics
retain explicit provenance; no silent supersampling/performance preset replaces
native ownership. Frame-generation experiments remain separate from the active
stereo path. Sustained pacing, recording compatibility, resource loss and broader
campaign behavior retain independent gates.
