# Validation

Validation states are intentionally strict:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

A higher state requires direct evidence for that boundary. Static disassembly, source existence, CTest and a successful build cannot substitute for a physical acceptance gesture.

Per-run logs, process IDs, videos, raw telemetry and evidence packages are local working data and belong under ignored `work/`. This document records only durable acceptance state and the current physical gate.

## Current physical gate

Independent native hands with retained torso/legs are **live-tested** for native
mesh loading and the separated presentation of the observed player model. The operator reports substantial
improvement but still rejects the gap between weapon and hand, especially the
left wrist orientation. The active follow-up is **host-tested**: it includes each
native holding socket, commits the weapon after its parent hand, verifies the
muzzle through both eyes and uses absolute authored-socket grip orientation for
unarmed hands. Comfortable wrists, hand/weapon cohesion and reload recovery
remain pending physical gates. The combined regression below is the acceptance
procedure; no T pose is required.

The holding-socket follow-up is now **live-tested technically**: both-eye hand
and weapon readbacks match, and pose/visibility restoration completes. The
operator's next clip still rejects shots and effects originating away from the
visible gun. The active **host-tested** follow-up unifies the visual direction
with the tracked ballistic base while preserving native spread, defers fire
delivery until current muzzle publication, and creates combustion/smoke at the
verified world muzzle without attachment to the restored native weapon. Actual
bullet/tracer origin, direction, flash, smoke and light remain physical gates.

First-level controller-only main-menu operation is **headset-validated** through
the native sprite-tree mouse-event route: hover/highlight, selection and physical
mouse takeover/resume were observed working. A later modal-routing candidate
regressed all menu/controller UI delivery because it queried
`MainMenuModule.m_bYesNoDlgVisible`, which the shipped class does not contain.
The optional `m_cYesNoDlg` fix is physically exercised: the visible Yes/No dialog
can be pointed at and selected with the VR ray. Exact shipped class inspection
establishes that the paused-hint precheck incorrectly called `GetHintManager()` on
`LawmanModule`; the getter belongs to its `m_GameMode` object. The corrected
`LawmanModule.m_GameMode -> GameMode.GetHintManager()` route is **implemented /
host-tested**, including ordinary selection, null mode/manager, pausing/non-pausing
hints and Yes/No selection. Static native tracing additionally shows that hover
and activation use distinct engine boundaries: L2/R2 ray selection now sends a
left-button press/release through the current UI input context at
`ChromeEngine3 + 0xCC420`, while Cross/global accept retains Enter. A hidden but
allocated Yes/No dialog is ignored unless `IsActuallyVisible()` is true. The
latest physical candidate confirms complete VR-pointer operation across the
ordinary menus exercised, the Yes/No dialog and the gameplay pause menu. The
current pointer-selection route is therefore **headset-validated for those
exercised menu paths**; loading continuation remains a separate boundary.

Continuous body-yaw following at the 35-degree comfort boundary remains
**headset-validated with Body IK disabled**. The absolute-frame Body IK writer has
now been physically exercised and the operator reports that the previous severe
pulsed locomotion, tiny-looking jumps and poor performance improved dramatically.
The remaining failure is arm ownership after actor translation: during joystick
locomotion a visible hand/arm branch can remain in world space and stretch back
toward the moving body until a native reload animation re-synchronizes it. The
same-frame restore rebase reduced the symptom but a follow-up run still recorded
multi-frame intervals where shoulder/elbow/wrist and arm element positions stayed
fixed while the actor advanced. The current host candidate keeps the restore
rebase and adds a conservative inter-frame continuity correction only when the
complete positional branch is unchanged despite material actor motion. Native
animation changes retain ownership. The combined correction and fail-closed
validation are **implemented / host-tested**; physical continuity remains
partly exercised: hands no longer remain stranded in world space, but occasional
recovery delay, limb torsion and short-arm feel remain rejected. Physical walking
forward/backward is inverted in the visible body. Physical crouch triggers native
animation, but it is abrupt and the arms remain too high. Shots still fail visible
barrel/direction alignment.

The nested pelvis/arm transaction and consistent forearm/FORETWIST/hand composition
are now live-exercised with further operator-reported improvement, but visible
anatomy remains rejected. The gameplay reticle is visible and impacts are reported
closer to its aim; weapon placement and muzzle origin remain rejected. Read-only
real-barrel samples confirm substantial positional/angular disagreement.

The latest live-exercised candidate removes the camera's opposite tracking Z sign and
uses the same actor-yaw-compensated translation frame as the pelvis/hands. It
compensates the native camera's duplicate descent during physical crouch, preserves
explicit controller crouch and maps full controller-tip translation around the
untracked camera for the aim origin. Sampled native ballistic/visual-origin
getters separate those fields from the rendered muzzle without consuming spread.
The operator reports substantial overall improvement, and sampled arm/pelvis
restoration succeeds through normal shutdown. Height compensation is observed
during physical crouch. Specific walking parity, comfortable crouch/arm height,
torso visibility, anatomy and ballistic alignment remain acceptance gates; the
overall improvement report alone does not accept each gesture. Visible weapon
poses and real-barrel samples still show controller/muzzle disagreement.

The next follow-up is **implemented / host-tested**: grip targets share the
camera/tip anchor, the elbow plane no longer follows idle animation, and a stable
bilateral T pose fits render-only arm reach to measured controller span. A rigid
weapon-element map verifies the actual muzzle before publishing nullable per-hand
shot-consumer vectors. The original global look-from mutation is removed. The
exact class/archive patch is transactionally staged and restored; the shipped
Java verifier accepts it. Geometry, fallback and restoration tests do not accept
actual bullet origin, fitted visual anatomy or headset comfort.

The combined candidate is now **live-exercised** for weapon-element/cache
publication and verified weapon/arm/pelvis restoration through normal shutdown.
Anatomy remains rejected: the operator performed the T pose, but the fixed
recenter-axis criterion never completed calibration. The weapon followed the
real hand while the short rendered arm stayed clamped nearer the body. The
**implemented / host-tested** follow-up judges bilateral extension in the
horizontal hand-to-hand frame, tolerating recenter yaw and visor offset from the
shoulder plane while retaining height, span and stability guards. Pending reasons
and sample counts are logged. Actual-shot alignment and fitted reach still need
headset acceptance; successful cache getters do not accept either.

The corrected hand-to-hand-frame classifier is now **live-tested** for completing
the operator's extended-arm calibration. The operator confirms longer arms, but
rejects the resulting anatomy. Verified wrist positions reach the solver targets;
hand-orientation residuals frequently exceed the preserved safety boundary and
therefore retain the composed native orientation. The visible wrist/forearm pose
remains unnatural. Further reach enlargement is not justified by this result;
hand/forearm orientation and skinning ownership remain the unresolved boundary.
The **implemented / host-tested** orientation follow-up calibrates against the
post-IK composed hand basis and preserves the verified displayed basis across
recenter. A host regression reproduces a large residual for an unchanged
controller under the previous native pre-IK reference. Neither positional reach
nor hand/twist safety limits change. Its physical follow-up still rejects the
enlarged connected limbs near the torso, so that presentation is superseded by
the independent-hand candidate described in the current gate.

The D3D9Ex compatibility/startup/reset path is now **live-tested** in the
production mod on the inspected Steam build and this host. The physical startup
run retained Ex factory/device identity, recorded successful Presents beyond
frame 2,970, completed a reset into generation 2 and captured flat images.
All 380 logged translated MANAGED texture creation results succeeded. Proxy hooks
restored and the outer runtime reached finalization. This does not validate
resource contents across reset, broader gameplay or native-stereo transport.

The corrected production startup gate has now **passed**. The geometry-derived
allocation for both asymmetric PS VR2 eyes is **live-tested**, and startup
videos plus the main menu on the mod's virtual screen are **headset-validated**
by the operator. The pointer was visible; this does not establish accurate
pointing or controller-only menu operation. Frames upload, compositor submission
and repeats advance with no frame rejection or submit failure. The physical eye
optics confirm that the old fixed 35% padding could not contain the source.
Shared native-stereo gameplay now supplies **live-tested** publication,
D3D11 consumer copies and successful new-frame OpenVR submission for both eyes
bound to an explicit render pose. Sampled producer timings report zero CPU
readback/copy and sampled producer/consumer waits are zero. The operator reports
no perceived slowdown; that initial impression was **observed**. A subsequent
bounded `vr-full` trace now establishes the production cadence target, and the
operator confirms correct depth and stable head turns in the visor.

The verifier's obsolete submission pattern rejected genuine telemetry because
it omitted the presenter's `presentation_mode` field. The corrected regression
uses the actual native-stereo record and rejects flat presentation, zero pose,
failed-eye submission and repeats without a successful new frame. Frozen replay
of the earlier failed run advances past submission and fails at incomplete
owning-runtime shutdown. That log ended during capture cleanup with no transport
summary or `run_end`; missing final counters remain unknown, not zero.
A fresh physical run now passes the corrected verifier through complete shutdown.
Bounded transport acceptance now also passes its measured cadence and visual
stereo/head-turn checks. Sustained pacing, tail latency and pending-frame
reset/device-loss recovery remain unproved.
A monoscopic gameplay clip
also shows avatar occlusion; body IK was disabled for this profile, so tracked
body behavior and stereo visual correctness are not promoted by this run.

**Implemented / host-tested / live-tested shutdown boundary:** finalization now executes
before the exact game's `DestroyGame` import. Binary hashes, x86 headers, loaded
call site and import/export identity are required, and the original game call
is preserved. Owner stop/join, protected import restoration and real process/DLL
exit ordering pass host regressions; pending GPU copies drain before capture
resources are reclaimed. The verifier rejects missing pre-exit completion,
abandoned/pending copies, unequal copied/completed totals or missing counters.
The new exact-build physical run completes pre-exit begin/end, import restoration,
same-owner OpenVR shutdown, capture cleanup and `run_end`. Both final summaries
agree on copied/completed totals with zero pending or abandoned copies; every
published producer lease is reclaimed. Normal-quit shutdown acceptance is closed
for this build/host. See [shutdown boundary](research/COJ_SHUTDOWN_BOUNDARY.md).

**Host-tested / live-tested telemetry follow-up:** releasing the last fenced but unpublished
producer frame reset its slot without refreshing ring-depth telemetry. A real GPU
regression reproduces the stale final depth and passes after refreshing that
counter. The next physical binary includes this correction and reports final
depth zero with every published producer lease reclaimed. The older run's trace
was disabled; cadence comes from the subsequent `vr-full` phase, not mixed
flat/native submission totals.

**Performance-validated for bounded production cadence:** the new trace measures
136.151 game updates/s and 135.527 rendered stereo pairs/s over approximately
72 seconds of render observation. This is about 63%/62% above the old CPU path,
respectively, and within 1.4%/1.6% of the readback-off reference. Update Hz uses
native simulation duration; pair Hz uses the render wall interval. These are
production rates, not headset refresh or compositor submission rates. Sampled
native CPU readback/copy and producer/consumer waits remain zero, with no fallback
or open/copy/submit failure. Ring drops and frame-age outliers remain observed;
full pacing and tail-latency performance are not promoted by this short run.

**Headset-validated for this bounded stereo path:** the operator confirms correct
depth and a stable image during head turns, without perceived dragging or jerks.
Body IK was disabled. Body/UI/weapon and abnormal-lifecycle acceptance remain
separate from the completed short transport gesture.

Independent LTR research on the same exact game build is materially stronger
than those old production attempts. Its semantic MANAGED adapter maps observed
usage-0 2D/cube textures to `DEFAULT | DYNAMIC` and observed MANAGED WRITEONLY
VB/IB requests to DEFAULT, survives 39 `BeginStateBlock` refreshes, services the
observed CPU-write locks and reaches 118 EndScenes / 120 Presents with zero
exception markers. A subsequent real reset keeps all nine tracked adapted
resources alive; four directly hashable textures retain byte-identical contents
and a generation-1 texture remains actively bound in generation 2. The research
two-slot transport then cancels a deliberately stalled generation at reset and
completes 12/12 submissions in generation 2 with ready=done=12, 12 D3D12 copies
and zero sampled mismatches. These are **live-tested research observations**;
they do not promote this mod's startup, transport, visual, performance or headset
state. See [resource census](research/COJ_D3D9_RESOURCE_CENSUS.md).

**Host-tested production compatibility:** the focused Ex compatibility test
reproduced `D3DERR_INVALIDCALL` for Chrome's observed 262,144-byte MANAGED
WRITEONLY vertex buffer and 131,072-byte MANAGED WRITEONLY INDEX16 index
buffer before production adaptation. The compatibility layer now maps those
observed profiles to DEFAULT while preserving their usage and buffer metadata;
both allocations and Lock/Unlock succeed in the test. A second regression
case reproduced loss of the creation hook after BeginStateBlock and now passes
with immediate HookRegistry reacquisition before the next engine call. Vtable
slots 26, 27 and 60 are compile-time checked. These allocation/Lock tests remain
host evidence; the physical run above adds bounded production startup/reset
evidence. The corrected presenter passes startup and normal-quit shutdown;
bounded cadence and stereo visual pairing also pass. Preserve any concrete production-only
divergence without restarting a broad resource census.

Locomotion diagnosis is closed. Physical measurements established that native
normal movement, the walk modifier and jump are not materially reduced in VR.
The decisive comparison was `139.949 Hz` vanilla versus `138.117 Hz` with both
VR eye renders active and only readback/copy disabled. The old full transport
ran at about `83.473 Hz`. This causally localizes the perceived slowdown to the
old capture/presentation boundary; movement, input and physics are not an active
remediation target.

Independent stereo, tracking, recenter, snap-turn and locomotion/jump parity
remain valid. The separate menu-pointer, arm-continuity and weapon-alignment
product gates remain open. Body, UI and weapon acceptance are
outside this transport run; normal-quit shutdown is now live-tested.

## Automatic menu-pointer acceptance

**Observed rejection:** holding L1/R1 activated the previous laser correctly,
but the game cursor still jumped and menu operation remained unusable. Passing
startup/closure verification did not accept that pointer.

**Headset-validated for first-level main-menu options:** the ray is automatic, with no shoulder
requirement. The presenter queues coordinates; only the game thread moves the
internal logical cursor, verifies readback and waits a game update before applying
a queued click. Accepted short taps survive trigger release; ownership/focus loss,
Back/Cross and menu-index changes cancel old clicks. No desktop mouse motion is injected. Same-hand L2/R2 selects;
a fresh trigger on the other hand chooses its ray. Physical mouse movement or
drag gives the mouse priority until 1.5 seconds after its latest activity.
Menu focus gates dispatch without starving flat publication, and held menu
shoulders cannot become gameplay weapon switches before release.

The operator confirmed correct first-level highlighting/selection and that mouse
input can take priority and hand control back to the laser. A later physical run
showed that a visible Yes/No dialog and the gameplay pause menu did not yet receive
the same behavior. The first modal fix then caused a broader regression by looking
up a nonexistent `m_bYesNoDlgVisible` field, poisoning JNI state before the proven
global route could run. Inspection of the shipped `MainMenuModule` confirms
`m_cYesNoDlg` exists and that visibility field does not. Modal observation is now
optional and exception-safe, while the global current UI remains preferred for
pause-menu delivery. The later native-click follow-up is now physically exercised
successfully: ordinary menu levels, the Yes/No dialog and gameplay pause all
accept the VR pointer in the latest candidate.

The next combined run keeps menu operation as a regression check while focusing
on Body IK restore continuity and the new 90-degree snap step. Startup/transport
verification confirms provenance and presentation only; the operator still
confirms the physical gestures.

## Combined menu and gameplay regression

For the next physical pass use normal `prepare -BodyIkAtStart`. In gameplay,
face forward and recenter with Create. The current **host-tested** candidate
retains torso/legs and tracks native hands independently; it does not require a
T pose or enlarge arm lengths. The last connected-arm candidate remained
physically rejected: extending reach helped distance but produced unacceptable
long limbs near the chest, and the post-swing orientation reference did not make
that anatomy comfortable. The separated meshes now load in game, but free wrist
rotation, hand/weapon cohesion and reload recovery remain pending gates.

Confirm that both player models load normally and that torso/legs remain visible
without arm triangles stretching toward the controllers. Bring hands close to
the chest, relax them, extend them and rotate each wrist fully. Fingers and weapon
must retain their native proportions and grip relation. During native reload the
original arms should return; afterwards the tracked hands should recover without
a jump or stale world position. Then
walk/run while moving both hands and verify that both hands stay attached to the
actor without stretched arm/body geometry and without needing reload to
re-synchronize. Sampled `body_hand_tracking` must prove both hands with
`reach_scale=1`, no upper-arm writes and twenty rigid elements including the
authored holding socket. Both-eye
`body_hand_render_probe` readback must match while the arm node is hidden, and
`body_hand_restore` must verify element and visibility restoration. For each
tracked weapon, both-eye `controller_weapon_render_probe` must match after the
final hand/socket pose; any failed probe rejects the technical gate. This does
not replace visual grip and actual-shot acceptance. `controller_fire_dispatch`
must show the deferred fire phase after muzzle publication; it is delivery
evidence, not proof that a shot consumed that ray. Confirm that
locomotion/performance retains the large improvement
from the last run. Turn the head substantially left/right without rotating the
body, press forward and confirm travel follows the viewed direction. Physically
crouch below the calibrated threshold and verify the native crouched body animation
engages and returns cleanly on standing, with both hands descending with the
controllers, without a second view-height jump when the native animation starts
or ends. Test explicit controller crouch separately. Step physically
forward/backward and sideways, including after a turn/recenter, and verify that
both the viewpoint's distance to a fixed scene object and the body respond in the
same physical direction. Lean forward and check for torso intrusion. Rotate
and bend each arm through the previously deformed pose and check recovery.
Exercise one right and one left snap turn
and confirm that each step is exactly 90 degrees. Aim/fire with both hands where
practical and confirm the gameplay reticle follows the visible weapon barrel.
Check first shots, held/repeated fire, weapon switching, reload recovery and firing
after crouching, physical steps and turning. The visible muzzle, tracer origin
and impacts must agree apart from native spread. Flash, smoke and shot light
must start at the visible muzzle, including while the hand is moving or held away
from the native idle gun pose. Cached-origin getter samples
are not actual-shot evidence. Then exercise an ordinary
menu, the Yes/No dialog and gameplay pause as regression checks; the gameplay
reticle must not alter menu-ray behavior. Recenter with Create, quit normally and
run `finish`. Hand/weapon/pelvis restore and archive integrity provide the host/live
evidence; visual continuity, crouch, movement direction, angle and weapon alignment
remain operator observations.

The transport profile verifies GPU capture, exact render-pose submission and
normal shutdown. Its summaries retain cadence, movement and dashboard observations;
modal/pause-menu hover/selection, post-dashboard usability, recenter visual quality
and Body IK continuity/anatomy still require operator confirmation. A transport
PASS does not accept those separate gestures.

## Production D3D9Ex startup/reset gate

**Passed for the exercised production build/host:** Ex startup, sustained
Present, flat capture/presentation and outer finalization are live-tested;
videos/menu visibility in PS VR2 is headset-validated. The earlier production
run separately established a successful reset. Shared stereo and inner shutdown
remain outside this completed startup acceptance.

Run one fresh `prepare -StartupOnly`, one Call of Juarez process, one `finish`.
Observe startup videos and the main menu, then close the game normally. Do not
enter gameplay in this run. `StartupOnly` disables native stereo and camera
tracking: the expected image is a flat virtual screen **inside the visor**.
Seeing only the desktop or Steam's generic theater does not establish the mod's
presentation. The startup verifier requires the
game-visible Ex factory/device identity, at least 90 successful Presents, flat content
capture and proxy finalization. It does not promote shared transport or headset
state. The separate transport acceptance gesture below has also passed for the
exercised build/host; retain these procedures as distinct regression gates.

### Bounded transport acceptance — passed

The accepted baseline established removal of the old readback bottleneck; its
approximately 136 Hz producer rate was not the configured headset refresh.
**Implemented / host-tested / live-tested bounded follow-up:** the producer now caps native eye-pair
production using the actual OpenVR HMD frequency and removes desktop Present
vsync when valid VR timing is available. The compositor remains independently
paced by `WaitGetPoses`. A fresh physical run reports HMD refresh/producer target
at the configured 90 Hz, eye-pair production around 87/s and total presenter
submissions around 89/s. The operator reports that it feels much better; normal
closure/drain still pass. Target cadence comes from the HMD, not the old
monitor-bound benchmark. Refresh changes and missing-property/reset recovery
remain host-tested only. The rate cap is not phase-lock or tail-latency validation.

Shared publication/copy and explicit-pose new-frame submission have been
live-tested in production gameplay, including resource drain and complete
normal-quit finalization. Measured update/stereo cadence and operator-confirmed
stereo depth/head-turn stability now pass for this exercised build/host.
The procedure below remains the regression gesture; another identical run is
not required to advance. Sustained pacing and pending-frame reset/device loss
remain separate renderer gates. Body/UI/weapon product work can proceed without
reopening the completed locomotion/input/physics diagnosis.

Use one fresh `prepare` without `-StartupOnly` or `-BodyIkAtStart`, load a save,
perform the short acceptance below, close normally and run `finish`. Body/UI
product acceptance and the locomotion comparison battery are outside this run.

For a future cadence regression, prepare normally, then enable the existing
`vr-full` trace before launching the game manually:

```powershell
pwsh -File E:\call_of_juarez_vr\tools\vr_test.ps1 prepare
pwsh -File E:\call_of_juarez_vr\tools\set_movement_diagnostic.ps1 -GameDirectory 'C:\Program Files (x86)\Steam\steamapps\common\Call of Juarez' -Mode vr-full
```

Load a save and spend 30 seconds in ordinary gameplay, checking each eye for a
current, distinct image and doing slow/fast head turns. Quit normally, then run
`pwsh -File E:\call_of_juarez_vr\tools\vr_test.ps1 finish`. The retained movement
summary must contain the `vr-full` phase with update/stereo-pair cadence; an empty
phase list cannot establish this gate. Body IK stays disabled for this profile.

| Area | Required observation |
| --- | --- |
| Startup | process reaches flat theater without a D3D9Ex/device crash |
| Native stereo | both eyes show distinct, correctly paired current images |
| Tracking/pose | normal head rotation/translation remains stable with no pull/snap-back |
| Minimal gameplay | a few seconds of ordinary movement remain responsive; do not repeat the locomotion battery |
| Cadence | reported HMD refresh and producer target match the configured visor rate; production pair rate approaches that target, with compositor new/total submission rates reported separately; game update Hz uses native simulation time |
| Transport | shared frames and D3D11 copies advance; producer/consumer wait stay zero; no classic fallback, readback, eye mismatch or ring exhaustion |
| Shutdown | close through the game's normal quit route; pre-exit hook installs/completes/restores, presenter reports `shutdown_complete=true`, pending GPU copies drain without abandoned leases, and transport/presenter summaries plus `run_end` are emitted |

Failure in one area does not promote that area, but independent observations may still be recorded.

## Closed locomotion diagnosis

The retained diagnostic tools may be used to report update/stereo cadence for a
transport run, but the comparative locomotion protocol is complete and must not
be repeated as an input-tuning exercise. Durable physical results are:

| Mode | Update Hz | Stereo-pair Hz | Normal / walk cm/s | Jump |
| --- | ---: | ---: | ---: | --- |
| Vanilla | 139.949 | n/a | 503.056 / 256.109 | 58.7-60.8 cm, 0.77-0.78 s |
| VR full, old CPU transport | 83.473 | 83.502 | 498.451 / 248.325 | 3 observed |
| VR input-off | 70.692 | 70.726 | 489.064 / 247.893 | 62.623 cm average, 0.781 s average |
| VR readback-off | 138.117 | 137.705 | 499.113 / 248.360 | 3 observed |

These measurements rule out native movement speed, native jump amplitude,
duplicated gameplay-input ownership and the second eye render as the primary
cause. The old per-frame GPU-to-CPU transport is the demonstrated cause.

## Current feature state

| Contract | State | Durable limit |
| --- | --- | --- |
| Exact camera -> view/projection -> renderer path | live-tested | camera path reaches the renderer used for physical stereo |
| Complete two-eye ChromeEngine render | live-tested | distinct eye rendering and SteamVR submission observed |
| Physical eye scale | headset-validated | validated at the game/XR unit boundary |
| HMD yaw/pitch/roll and positional offset | orientation headset-validated; translation follow-up host-tested after rejection | physical forward/backward and torso visibility remain rejected; camera Z sign and actor-yaw reference now match skeleton/reticle space |
| Explicit render-pose submission | headset-validated | removed the previous head-turn pull/snap-back artifact |
| Recenter | headset-validated | controller recenter exercised physically |
| Exact snap turn | mechanism headset-validated; 90-degree candidate host-tested | previous snap-turn behavior was exercised physically; the new 90-degree step requires one physical confirmation |
| Head-relative stick locomotion | implemented / host-tested | residual physical HMD yaw rotates the neutral stick before the unchanged native InputAnalog shaping; physical direction/feel is pending |
| Physical HMD-height crouch | live-exercised animation engagement; duplicate native-camera descent correction host-tested | comfort remains rejected; shared view/arm/aim correction awaits physical confirmation, explicit controller crouch retains native ownership |
| Flat-theater startup/load -> native stereo | headset-validated for exercised path | startup and level transition reached gameplay safely |
| Local head/hair suppression | headset-validated for HMD view | shadow behavior remains separate |
| D3D9Ex GPU-resident native-stereo transport | bounded acceptance passed; measured cadence and headset stereo validated | shared frames submit with explicit pose and drain on normal quit; production cadence approaches readback-off reference; sustained pacing/tail latency and pending-frame reset/device loss remain unproved |
| Classic-D3D9 resource pool census | live-tested research / bounded startup coverage | independent startup census records 1,552 successful creations and eleven MANAGED calls across texture/cube/VB/IB profiles with immediate hook restoration; broader gameplay/device-loss coverage remains open |
| Classic-D3D9 CPU transport fallback | implemented / unpromoted | retained only when D3D9Ex/device sharing is unavailable; any use during the physical gate is a failure |
| Inner presenter finalization | live-tested normal quit | exact-build pre-DestroyGame owner stop/join, complete GPU drain, capture cleanup, hook restoration and final summaries verified; abnormal exit/device loss remain unproved |
| Native analog locomotion | headset-validated diagnostically | normal/walk speed matches vanilla within the measured transport runs; do not retune input or movement |
| Native jump action | headset-validated diagnostically | measured apex/duration matches vanilla; do not retune jump or physics |
| Locomotion divergence instrumentation | host-tested and physically completed | retained for cadence reporting; causal investigation is closed |
| Room-scale visual body compensation | live-exercised / direction rejected; sign correction host-tested | physical forward/backward body displacement was inverted; corrected skeleton mapping and nested arm transaction await confirmation |
| Physical-walk visual animation | planned/open | should reuse native locomotion animation semantics without surrendering collision ownership |
| Sense tracking in game space | live-tested | left/right controller transforms reach the backend |
| Visible arm writer/restoration | live-tested | geometry changes and restoration are proven |
| Body IK continuity/anatomy | continuity physically improved; span calibration live-tested; anatomy rejected | calibration completes and arms are longer, but wrist/forearm orientation remains unnatural; solver target readback/restoration do not accept visual skinning |
| Independent native hands with retained torso/legs | native loading/presentation live-tested; attachment follow-up host-tested | operator reports substantial improvement but rejects weapon gap and wrist pose; twenty-element hand/socket map, parent-before-child application, per-eye weapon readback and transactional restoration covered; wrist comfort, grip cohesion and reload recovery pending |
| Native reload ownership | host-tested | VR writes yield during native reload state |
| Flat-menu pointer | headset-validated for exercised ordinary/Yes-No/pause paths | latest physical candidate reports complete VR-pointer menu operation; loading continuation remains separate |
| Native body yaw with Body IK disabled | headset-validated for exercised path | continuous 35-degree boundary correction removed the observed body/hand stepping during physical head turns |
| Cross/Circle/L2/R2 UI actions | exercised menu-selection path accepted; broader action coverage remains bounded | latest run accepts ordinary/Yes-No/pause pointer selection; retain per-action regression checks where relevant |
| Loading continuation | host-tested / latest run reached gameplay | an earlier physical load hang was not reproduced in the latest gameplay run; controller-only loading acceptance remains separate |
| Controller-origin UI beam | headset-validated for exercised menu paths | ordinary, Yes/No and gameplay-pause selection work in the latest physical candidate |
| Controller-owned weapon direction/visual origin | live-exercised technically; visual alignment rejected | rigid weapon-element map/cache publication and restoration observed; short clamped arms separate rendered hands from tracked weapons; actual shots and reload recovery remain pending |
| Sense tip direction convention | live-tested diagnostically | local `-Z` is the demonstrated pointing direction |
| Exact-frame gameplay weapon reticle | live-exercised / visible | impacts reported closer to the reticle; alignment with the visible weapon/muzzle remains rejected |
| Physical gun origin/direction | pending/rejected | production shots must originate from the visible weapon/barrel |
| Supported end-to-end VR release | planned | project remains pre-alpha |

## Evidence policy

Only conclusions that remain useful across sessions belong here. Raw headset-run manifests, hashes, local video paths, telemetry counts, process IDs and agent handoffs stay under ignored `work/` and may be discarded once their conclusions are represented by code, tests or the durable documents above.
