# Validation

Validation states are intentionally strict:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

A higher state requires direct evidence for that boundary. Static disassembly, source existence, CTest and a successful build cannot substitute for a physical acceptance gesture.

Per-run logs, process IDs, videos, raw telemetry and evidence packages are local working data and belong under ignored `work/`. This document records only durable acceptance state and the current physical gate.

Host recovery regressions cover altered original backups, empty input directories,
mutable camera control and `finish` retries with deployment/video journals after
stage state removal. Original identities remain mandatory; recovery-only completion
does not promote live or headset validation.

## Current physical gate

The previously failing narrated campaign transition is now **headset-validated
for the exercised Large Address Aware candidate**: the operator crossed it and
continued into playable whip content without crashing, followed by normal quit
and verified restoration. Bounded read-only samples show actual used virtual
address space above 2 GiB with no sampled allocation failures. WER process
counters from the earlier crashes were near the original non-LAA x86 ceiling.
Optional `prepare
-LargeAddressAware` stages only the recognized executable's one-bit PE derivative
on 64-bit Windows, allowing a 4 GiB user address space. Exact original/derived
hashes, executable backup, all-inventory recovery preflight and interrupted
stage/finish restoration are host-tested. A synthetic x86 process additionally
proves high-address reserve/commit/write/free with the bit set and rejection
without it. The exercised transition does not accept high-address compatibility
of every native/JVM path or fix the underlying allocation ownership.

Billy's whip is **headset-validated for the exercised use**: the operator reports
usable controls and approximately matching reticle alignment, with no other
problems in that session. Precise reach/collision, every attach/length/release
gesture and broader climbing contexts remain separate checks; this acceptance
does not tune native range or damage. The exact owned-whip pose cache and right-hand
ray are host-tested; the natural `AdditionalSynchro` remains the cloth simulation
owner. R2 (native RMB/action 10) grabs and, when attached, shortens; L2 (native
LMB/action 9) attacks and, when attached, lengthens. R2+L2 releases; Cross also
releases while hanging. The firearm empty-hand guard excludes this exact tool
so its two-input semantics survive. Read-only hint copies replace isolated
LMB/RMB/SPACE BAR/SPACEBAR with L2/R2/Cross; native text and dialogue stay intact.
The next physical gate is native protected-target feedback. The operator accepts
the corrected snap direction and the exercised left-wrist card, including
ammunition and damage-driven health updates. Broader recovery, pause/load
suppression or sustained pacing remain separate checks.
The protected-target red X is host-tested: native warning visibility, current
HUD/player/hand and weapon owner, bounded native age and traced/current ray
coherence are required. This read never forces native updates/traces or changes
firing permission. The corrected 45-degree snap maps right stick to right native
turn and left stick to left and is headset-validated for the exercised direction.
Transition save/load, repeated
loading and sustained stability remain separate unconfirmed checks. Keep
Steam recording off and graphics fixed. Retain the bounded address-space snapshots
at allocation boundaries and any crash evidence. The new 80% wheel/compass sizes
and revised compass dial also need a brief physical readability/attachment check;
previous acceptance describes their former sizes.

The latest operator confirms Options pause visibility/selection, L1 box pickup,
carry and put-down, pistol pickup, Triangle + R2 dialogue/hint logs, reload,
jump and physical/controller crouch. Pause and
those specific interaction gestures are **headset-validated**. The latest physical
report confirms visible shot FX, impact decals, water and destructible bottles
across exercised gameplay loads. Their visibility is **headset-validated** for
those contexts, superseding the earlier invisible-effect rejection. Exact
side-on/rolled muzzle origin, direction and flash/smoke/light coherence now have
operator acceptance for the exercised pistol. Native attack/hit entries and emitter
creation/commit telemetry do not expand that visual acceptance. The adapter's
missed `PlayerController` action-to-hand conversion is corrected and host-tested;
The exercised single right pistol now accepts R2/right fire and rejects the
empty left hand's trigger, matching R2 -> action 10 -> hand 0 and L2 -> action 9
-> hand 1. Left/dual weapons retain separate physical acceptance.
Triangle + Square has operator-confirmed blur but no perceived magnification;
focus on/off recovery and fresh fire after exit are now operator-confirmed.
Triangle + Circle hands/put-away remains unexecuted in the exercised context.
Selecting the equipped weapon again in the wheel puts it away successfully;
the chord is deferred and does not block this wheel-based workflow.
Triangle + R2 mission logs and exercised modifier/menu held-input regressions
are confirmed. Optical XR magnification remains planned.
The operator accepts the exercised movement/stereo/head-turn/recenter and
room-scale/crouch/head-relative regressions. A subsequent report identifies
inverted snap direction as the disorientation cause, superseding the previous
45/90-degree comfort conclusions. The sign correction at the retained 45-degree
step is headset-validated for the exercised direction.
Physical head-turn stability without recording remains separately accepted.
The operator reports that microskips start with Steam recording and disappear
when recording stops; the earlier association with snap is superseded by this
observation. Recording compatibility and sustained pacing remain open; this
correlation does not identify the source of recording overhead.
A read-only camera/actor trace records captured HMD/relative orientations,
native/eye bases and yaw ownership with a 180-stereo-frame burst after snap or
recenter, then bounded baseline samples. It changes no camera or FX policy.
A repeated crash during narrated/loading presentation, including a repetition
without input during loading, leaves sustained loading stability open. The
native texture-allocation failure and null dereference are correlated; their
resource-pressure cause remains unproved. Proximity to Create does not establish
causation.
Floating dialogue subtitles remain confirmed. Full graphical HUD is still
**planned**; protected-target red-X feedback is **host-tested**, pending visor acceptance.

Independent native hands with retained torso/legs are **headset-validated for
the exercised wrist/grip and reload/movement recovery checks**. This supersedes
the earlier weapon-gap and wrist rejection for the active independent-hand path.
The holding-socket follow-up includes each
native holding socket, commits the weapon after its parent hand, verifies the
muzzle through both eyes and uses absolute authored-socket grip orientation for
unarmed hands. Broader equipment and animation contexts remain separate gates.
The combined regression below is the acceptance
procedure; no T pose is required.

The holding-socket follow-up is now **live-tested technically**: both-eye hand
and weapon readbacks match, and pose/visibility restoration completes. The
earlier side-on clip showed a streak behind the gun. Exact mesh research shows
a rearward-authored trail, so that image alone does not prove an inverted shot.
The latest operator accepts exercised gun/effect coherence. The follow-up unifies the visual direction
with the tracked ballistic base while preserving native spread, defers fire
delivery until current muzzle publication, and creates combustion/smoke at the
verified world muzzle without attachment to the restored native weapon. Actual
missile birth/first-draw vectors remain a separate technical measurement gate;
physical acceptance does not prove the internals of that diagnostic.

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
accepted physical baseline confirms complete VR-pointer operation across the
ordinary menus exercised, the Yes/No dialog and the gameplay pause menu. The
current pointer-selection route is therefore **headset-validated for those
exercised menu paths**. The operator confirms recovery of pause visibility and
selection with the mono capture ring. Loading continuation remains separate.

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
accept the VR pointer. The allocation follow-up now also has operator-confirmed
Options pause visibility and usable selection after gameplay loading.

The next combined run checks correct trigger/weapon ownership and precise
muzzle/effect coherence, then explicitly exercises Triangle button chords.
Body restore continuity and the 45-degree snap step remain regression gestures. Startup/transport
verification confirms provenance and presentation only; the operator still
confirms the physical gestures.

## Combined menu and gameplay regression

For the next physical pass use normal `prepare -BodyIkAtStart`. In gameplay,
face forward and recenter with Create. The current **host-tested** candidate
retains torso/legs and tracks native hands independently; it does not require a
T pose or enlarge arm lengths. The last connected-arm candidate remained
physically rejected: extending reach helped distance but produced unacceptable
long limbs near the chest, and the post-swing orientation reference did not make
that anatomy comfortable. The active separated-hand path now has operator
acceptance for exercised free wrist rotation, hand/weapon cohesion and reload
recovery; broader equipment/animation contexts remain separate gates.

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
and confirm that each step is exactly 45 degrees and feels comfortable. Aim/fire with both hands where
practical and confirm the gameplay reticle follows the visible weapon barrel.
Check first shots, held/repeated fire, weapon switching, reload recovery and firing
after crouching, physical steps and turning. The visible muzzle, tracer origin
and impacts must agree apart from native spread. Flash, smoke and shot light
must start at the visible muzzle, including while the hand is moving or held away
from the native idle gun pose. Turn the weapon side-on and roll it through
several angles: particles must travel forward from the tip, independently of
weapon restoration. The complete-emitter-frame correction is host-tested; the
operator still rejects the preceding candidate's side-on effect direction. Cached-origin getter samples
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
pwsh -File .\tools\vr_test.ps1 prepare
pwsh -File .\tools\set_movement_diagnostic.ps1 -GameDirectory 'C:\Program Files (x86)\Steam\steamapps\common\Call of Juarez' -Mode vr-full
```

Load a save and spend 30 seconds in ordinary gameplay, checking each eye for a
current, distinct image and doing slow/fast head turns. Quit normally, then run
`pwsh -File .\tools\vr_test.ps1 finish` from the repository root. The retained movement
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
| HMD yaw/pitch/roll and positional offset | headset-validated for exercised regression without Steam recording | operator reports microskips during recording and recovery when it stops; recording compatibility, sustained loading/reset remain separate |
| Explicit render-pose submission | headset-validated | removed the previous head-turn pull/snap-back artifact |
| Recenter | headset-validated | controller recenter exercised physically |
| Exact snap turn | headset-validated for exercised corrected direction | retained 45-degree step, right stick turns right/left turns left; head-turn stability without recording separately accepted |
| Head-relative stick locomotion | headset-validated for exercised regression | operator accepts direction/feel; native InputAnalog shaping remains unchanged |
| Physical HMD-height crouch | headset-validated for exercised regression | operator accepts movement/crouch regression; explicit controller crouch retains native ownership |
| Flat-theater startup/load -> native stereo | headset-validated for exercised path | startup and level transition reached gameplay safely |
| Local head/hair suppression | headset-validated for HMD view | shadow behavior remains separate |
| D3D9Ex GPU-resident native-stereo transport | bounded acceptance passed; measured cadence and headset stereo validated | shared frames submit with explicit pose and drain on normal quit; production cadence approaches readback-off reference; sustained pacing/tail latency and pending-frame reset/device loss remain unproved |
| Classic-D3D9 resource pool census | live-tested research / bounded startup coverage | independent startup census records 1,552 successful creations and eleven MANAGED calls across texture/cube/VB/IB profiles with immediate hook restoration; broader gameplay/device-loss coverage remains open |
| Classic-D3D9 CPU transport fallback | implemented / unpromoted | retained only when D3D9Ex/device sharing is unavailable; any use during the physical gate is a failure |
| Inner presenter finalization | live-tested normal quit | exact-build pre-DestroyGame owner stop/join, complete GPU drain, capture cleanup, hook restoration and final summaries verified; abnormal exit/device loss remain unproved |
| Native analog locomotion | headset-validated diagnostically | normal/walk speed matches vanilla within the measured transport runs; do not retune input or movement |
| Native jump action | headset-validated diagnostically | measured apex/duration matches vanilla; do not retune jump or physics |
| Locomotion divergence instrumentation | host-tested and physically completed | retained for cadence reporting; causal investigation is closed |
| Room-scale visual body compensation | headset-validated for exercised regression | operator accepts the movement regression after the sign correction; wrist/grip/reload acceptance remains separate |
| Physical-walk visual animation | planned/open | should reuse native locomotion animation semantics without surrendering collision ownership |
| Sense tracking in game space | live-tested | left/right controller transforms reach the backend |
| Visible arm writer/restoration | live-tested | geometry changes and restoration are proven |
| Body IK continuity/anatomy | continuity physically improved; span calibration live-tested; anatomy rejected | calibration completes and arms are longer, but wrist/forearm orientation remains unnatural; solver target readback/restoration do not accept visual skinning |
| Independent native hands with retained torso/legs | headset-validated for exercised wrists/grip/reload and movement recovery | holding-socket follow-up supersedes the earlier gap/wrist rejection; original proportions retained; broader equipment/animation contexts remain separate |
| Native reload ownership | headset-validated for exercised recovery | VR writes yield during native reload state; broader equipment remains separate |
| Flat-menu pointer | headset-validated for exercised menus and recovered pause | Options pause appears correctly in the visor and permits pointer selection; allocation/Reset fault injection remains a host-only check |
| Native body yaw with Body IK disabled | headset-validated for exercised path | continuous 35-degree boundary correction removed the observed body/hand stepping during physical head turns |
| Cross/Circle/L2/R2 UI actions | exercised baseline menu selection accepted; Options pause recovery confirmed | retain per-action and loading regression checks |
| Loading continuation | exercised startup/load and previously failing narrated transition accepted with LAA | operator continued into playable whip content; normal quit/restoration and used VA above 2 GiB observed; transition save/load, sustained loading/resource-pressure and broader campaign remain separate |
| Controller-origin UI beam | headset-validated for exercised menus and recovered pause | operator confirms recovered pause visibility and pointer selection; retain loading/dashboard regression checks |
| Controller-owned weapon direction/visual origin | headset-validated for exercised pistol coherence | rigid weapon-element map/cache publication and restoration observed; active independent hands accepted; broader equipment remains separate |
| Sense tip direction convention | live-tested diagnostically | local `-Z` is the demonstrated pointing direction |
| Exact-frame gameplay weapon reticle | exercised pistol and approximate whip alignment headset-validated | firearm ray requires verified muzzle; procedural whip requires owned complete pose cache and tracked right hand; exact collision/reach and broader whip contexts remain separate |
| Physical gun origin/direction | headset-validated for exercised pistol/side-on and roll checks | operator accepts visible coherence; rearward-authored BulletTrail geometry prevents inferring ray inversion from the earlier clip alone; exact missile birth/first draw remains unmeasured |
| Native equipment wheel | headset-validated for exercised owned equipment and put-away | selecting the equipped weapon again puts it away; unavailable items and broader native permissions retain separate gates |
| Native-objective left-wrist compass | headset-validated for exercised guidance/readability/attachment and snap/recenter checks | bounded native visibility and captured binocular wrist geometry; broader objective/campaign contexts remain separate |
| Native health/ammo wrist card | headset-validated for exercised card, ammunition and damage-driven health updates | broader recovery/pacing retain their gates |
| Native protected-target red X | host-tested | actual native warning + current trace weapon owner + selected verified firearm ray; bounded age/origin/direction coherence; visor appearance/target transitions pending |
| Optional Sense finger sensing | live-tested technically, partial quality reported | independent skeletal-summary curls observed; native finger animation remains planned |
| Supported end-to-end VR release | planned | project remains pre-alpha |

## PC mechanics and gameplay HUD completeness

**Input expansion headset-validated for exercised controls; broader campaign incomplete. Essential stereo
text host-tested with operator-confirmed floating dialogue subtitles. Native
graphical HUD capture/composition: planned.** The latest operator reports basic
fire/recenter/crouch/jump/reload, usable pause selection, L1 box carry/put-down,
pistol pickup and Triangle + R2 logs. A later physical report accepts visible
shot FX, impact decals, water and destructible bottles, exercised muzzle coherence
and corrected single-pistol trigger ownership. Red no-shoot
feedback has a separate host-tested native-warning/trace-owner route, pending
visor acceptance; it is not inferred from text or hit distance. The log confirms
mapped F/weapon-cycle/modifier intents, not native mechanic execution.

The pause follow-up reserves and leases mono SYSTEMMEM buffers instead of
allocating transient stereo CPU images after loading. Host fault tests deny new
surface allocations after warmup, exercise ring exhaustion/release and same-size
and resized Reset. A separate native-dispatch fixture reproduces one-shots lost
under the locomotion lock and verifies delivery after analog commit, retry after
failure, held-edge suppression and the independent fire phase. These follow-ups
are **host-tested**; the operator separately confirms pause recovery and specific
pickup/carry gestures. Visible shot FX/decals now have operator acceptance;
exercised pistol origin/direction/light coherence is now accepted; left/dual and
broader equipment require their own available-context checks.
Interaction gaze checks additionally cover the translated/crouched central HMD
camera, actor-yaw compensation, invalid tracking/basis rejection, complete-pair
JNI publication and clearing after a write failure or context loss. The exact
archive changes only its five identified classes (including the procedural-whip
consumer); `CheckTriggers` retains its
native selection flow. Direct L1/F must still pick up and put down an available
object while standing, crouching and moving, including after pause/dashboard
transitions. Native interaction range and permissions must remain intact.

Fire ownership host checks distinguish right/left trigger roles, reject only the
tracked one-hand firearm's cross-hand retry, and preserve native two-hand modes,
non-firearm tools and network/untracked fallback. FX checks require a live-world
commit after writing the starting matrix; failed creation/frame/commit cases do
not retain a bad emitter. Read-only native entry/status counters distinguish
attack, impact, configured/created/committed FX and suppression. Additional host
checks cover clearing
stale/uncommitted FX handles, all-or-nothing JNI observation, copied native FX
global/camera reads and replacement/partial-read rejection. Live global samples
show FX enabled and the particle draw camera matching each eye. The bounded
owner-local emitter reader is **host-tested** for capacity/generation/subtype
validation, finite values, replacement, partial-read cleanup and observation
budget. Actual emitter samples, first update and expiry require their own
technical evidence; operator-confirmed FX visibility does not prove those
read-only observations. Physically test a
right pistol with R2, verify L2 cannot fire it, and repeat for left/dual pistols
when available. Check side-on muzzle flash/smoke and wall impact feedback.
The exercised runs include focus, hands and logs chords. Logs are operator-confirmed;
focus blur, toggle-off/recovery and fresh fire are operator-confirmed. Optical XR
zoom remains planned. Triangle + Circle remains unexecuted in the current
context and is deferred; wheel selection successfully puts the weapon away.
Acceptance covers equipment available in the exercised session.
Tutorial scripts disable kicking, so
absence of its animation there is not a binding rejection. XR optical focus
magnification remains planned independently of native focus state.

Pickup-triggered autosave exposed an unhandled native allocation exception
through `TakeScreenshot -> ForceRender`. A **host-tested** exact-class workaround
omits only quick/automatic-save GPU thumbnails while retaining native stale-preview
cleanup and the remaining save/restore flow. The shipped Java verifier accepts
the redirected private helper; this is not proof of successful save serialization
or a general memory-pressure fix. The operator now confirms that a new autosave
loads successfully in the exercised session: that save/load gesture is
**headset-validated**. Broader pickup contexts, repeated loading and resource
pressure retain their own gates. Repeat box/pistol/ammunition pickup through
autosave in further contexts and confirm pause
selection still works. Quick/automatic saves intentionally have no generated
preview while the patched archive is staged. After any crash, finish the current
run and prepare a fresh ID before restarting.

Menu operation and the alignment reticle
do not establish complete campaign control or HUD coverage. The authoritative action catalogue
and native feedback owners are recorded in
[the PC controls/HUD research](research/COJ_PC_CONTROLS_AND_HUD.md). Do not count
hidden/developer actions as mandatory missing buttons or substitute a third-party
Steam Input layout for official PC defaults. This gate adds acceptance requirements;
it does not promote any new physical gate.

Host checks cover pause/kick dispatch, the expanded native action catalogue,
manifest/default-binding consistency, modifier entry/exit and focus-loss release,
latched selection without delayed snap/fire, persistent focus with fresh fire,
and passive sprite/flush forwarding and owned restoration. The probe does not
capture HUD pixels and does not establish alpha/coverage or live render order.

For the next controller check, use direct L1 for F, Options for pause and the
documented Triangle layer for secondary controls. Test ordinary Circle kick, modifier focus on/off followed
by firing, alternate fire, put-away/discard, objectives/logs and available equipment
selection. Repeat after dashboard/menu transitions with held buttons. Inspect
sampled `native_hud_boundary` events for sprite/flush versus world capture/Present
ordering and render-target state. Preserve raw observations under ignored work.
The essential text candidate additionally reads localized interaction/tutorial
owners and enabled, visible dialogue subtitles without replaying HUD updates.
Host JNI fixtures cover player identity, native visibility, null/missing owners,
dialogue bounds, Unicode truncation and exception/reference cleanup. Real D3D11
WARP composition checks text pixels, binocular geometry, asymmetric/canted optics,
world preservation and removal after a fresh empty frame. Capture tests retain
text/optics with sequence and device generation across resource replacement.
These establish **host-tested** implementation only; the operator separately
confirms subtitle visibility. Verify tutorial/context text appears and
disappears with native prompts, remains legible in both eyes, does not obscure
aiming and adds acceptable pacing cost. The observation interval is at most
100 ms between sampled game frames; reading/dismissal latency needs acceptance.
Pausing hints and objectives/logs retain their existing native flat UI route.
Native hints currently retain PC key names; controller-label adaptation is open.
No complete graphical HUD or XR zoom is claimed. The visible equipment wheel
and native-objective left-wrist compass are **headset-validated for exercised
switching/put-away, direction, visibility, binocular readability, wrist attachment
and snap/recenter checks**. Broader campaign contexts retain the procedure below.
Hold Triangle and use the right stick; one selection is latched
per deflection, with neutral/release needed to rearm. Owned and cached-permitted
sectors are enabled; native selection retains ammunition/reload/context authority.
An unavailable held sector must remain blocked if permissions recover. Verify
all available slots, native hands/discard context, and no delayed fire/snap after
the layer, menu or dashboard. On the compass, verify native objective direction
and disappearance, snap/recenter consistency, both-eye readability, wrist roll,
attachment and pacing. Wrist geometry is captured each frame, while dial raster
updates are limited to 10 Hz. These checks do not accept the broader graphical HUD.

Optional Sense skeleton curls now retain their actual reported tracking quality
and clear on ownership loss. Native fingers are not animated by this input path.
Actual skeletal-summary availability and independent hand samples are
**live-tested technically**, with partial tracking quality reported by the
runtime. Future joint retarget/visible animation requires separate validation;
touch/curls do not demonstrate full tracking or bone animation.

When corresponding features are implemented, a controller-only campaign pass
must demonstrate:

- Readable tutorial/hint text, context-action cues, objectives/logs and subtitles,
  including dismissal/continuation of pausing hints without invisible prompts.
- Native no-shoot warnings for protected targets, coherent with the tracked
  weapon target; ordinary valid-target, empty-ammo and whip helpers remain
  distinct. The alignment cross alone does not satisfy this check.
- Health/ammo, stance/stealth, objective direction, concentration availability,
  duel countdown and horse condition visible when required by the native game.
- Contextual F pickup, carry, put-down, device use and mounting; focus enter/hold/
  exit with its native movement rules; alternate fire; native weapon selection,
  put-away and discard. Only owned/available items are selectable.
- Circle kicks without opening pause, Options pauses, and menu back/accept/select
  never leak into combat. Radial selection does not also turn/fire; focus loss,
  menus and native reload release or defer held actions correctly.
- Billy's climbing/stealth, whip attack/grab/length/release and bow; Ray's Bible
  and native put-away/quick-draw concentration; dynamite, duel draw/aim/fire and
  horse mount/steer/gallop/dismount. Verify their contextual input owners rather
  than extrapolating from gun firing or on-foot movement.
- Explicit slow-walk/accessibility crouch and deliberate save/load access,
  without accidental combat activation or changing accepted native shaping.

Fresh host checks cover action IDs, input ownership, failure/release behavior and
HUD probe forwarding; actual HUD frame correlation requires a live run. Each mechanic's live execution and visual/readability/
comfort gate remain separate; a binding, synthetic HUD or screenshot cannot
promote them to headset-validated.

## Evidence policy

Only conclusions that remain useful across sessions belong here. Raw headset-run manifests, hashes, local video paths, telemetry counts, process IDs and agent handoffs stay under ignored `work/` and may be discarded once their conclusions are represented by code, tests or the durable documents above.

## Image-quality profile acceptance

The preparation profile preserves selected render resolution and other game
quality settings and enforces FSAA 0; transactional restoration is host-tested.
The higher-resolution quality candidate remains pending physical validation.
Check fine distant detail, readability, head-turn clarity, sustained production
versus the configured HMD rate and representative level loading. Keep graphics
fixed for each fresh run; one run ID must describe one game launch. Graphics
changes and repeated launches cannot promote a stability or device-reset gate.
Native loading crashes observed after graphics changes do not establish their
exact cause or prove that a different quality setting has fixed them.
