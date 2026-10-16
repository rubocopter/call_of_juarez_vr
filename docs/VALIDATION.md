# Validation

Validation states are distinct:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

Builds, CTest, synthetic fixtures and static inspection do not promote physical
gates. Acceptance here is limited to exercised original Steam Call of Juarez
(2006), PS VR2 Sense and SteamVR contexts. It is not whole-campaign or public
release support. This document owns acceptance; [Architecture](ARCHITECTURE.md)
owns contracts and [Roadmap](ROADMAP.md) owns priorities.

## Public prerelease policy

`v0.1.0-prealpha.1` is published for experimental community testing before
whole-campaign acceptance. Distribution, package verification and additional
controller bindings do not promote any validation state. Physical acceptance
remains limited to the exercised Steam 2006 D3D9/OpenVR scenes and PS VR2 Sense.
Other packaged bindings are experimental, with no support claim for other PCs
or headsets.

Disclose the unresolved chest/possible-scroll crash, incomplete campaign coverage
and open recovery gates. Movie mode switching and return from exercised cinematics
now have scoped operator acceptance; the specific Hope transition remains separate.
Community testing also reports persistent flat presentation after the cinematic
on arrival in Hope. An unfinished-capture ring exhaustion defect is corrected
and host-tested locally; recovery in that campaign scene remains unvalidated.
Recommend save backups because crashes or blocked progression can lose progress.
Users start/close the game and SteamVR manually. Community diagnostics are
generated locally, inspected for private data and attached manually to issues;
they do not replace canonical evidence or operator acceptance below.

Community reports remain observations, not support qualification. Chtiblue
reports startup with Pimax Dream Air, RTX 5090 and SteamVR; controller coverage,
tracking quality, sustained performance and campaign recovery are not established
by that report. The known source-resolution limitation also affects the PS VR2
development path: native stereo retains the game's RT0 extent, independently of
SteamVR's recommendation. Independent eye resolution is planned; the
[source investigation](research/COJ_VR_RENDER_RESOLUTION.md) adds no physical
acceptance and makes no claim that DSR works on a particular setup.

The [approved independent-resolution design](research/COJ_VR_INDEPENDENT_RESOLUTION_DESIGN.md)
has a startup-only binding observer and offline analyzer implemented and
host-tested. Real D3D9/Ex tests cover read-only state/pixels, release before Reset,
ResetEx invalidation, hook forwarding/continuity, scope cancellation and control
preservation. Synthetic analysis rejects incomplete/foreign traces. A finished
canonical session now exercises bounded gameplay eye collection without read
failures or truncation. The operator confirms the exercised functional checklist
still works at the original resolution, with occasional stutters. This is scoped
continuity acceptance with diagnostics active, not a performance or higher-detail
gate. A later bounded collection includes complete gameplay, settings and
immersive-movie resource scopes at the original size.
The offline analyzer classifies the observed FourCC `NULL` color placeholder
against its bound D16 depth extent, with explicit layout/Z and viewport checks.
It now reads the captured complete gameplay pairs; ordinary out-of-bounds color
and unqualified NULL/depth layouts still reject. This is bounded binding evidence,
not a qualified effect graph or verified shadow contents. Allocation/effect/constant
ownership and compositor retirement remain Q0 gates. Diagnostic sessions perturb
pacing and cannot establish performance improvement. Independent rendering,
actual detail and functional continuity at new sizes require the specified
native and headset evidence; complete bindings never admit resizing by themselves.
The [native PP ownership map](research/COJ_PPFX_TARGET_OWNERSHIP.md) now establishes
static allocator/release/alias and renderer-cache responsibilities. This is
research evidence only; it adds no live allocation, independent-size, shader
constant or recovery acceptance, and leaves Q0 open.
Sprite/flush analysis preserves distinct device-generation and probe-epoch
namespaces. Older gameplay samples cannot qualify their device-generation
identity retrospectively; fresh correlation uses both explicit counters.
Ordering metadata alone does not admit mirror mutation, alpha/coverage or
independent resolution.
The bounded native PP descriptor/cache reader, exact-build admission bridge and
separate offline native identity correlation are implemented and host-tested.
Synthetic fixtures cover LAA identities, aliases, class/list/count/name bounds,
root changes, failed reads, unallocated/dirty resources and the split sampler
caches. Serialization preserves byte names and unused 2D reciprocal-depth bits;
it does not call copied COM identities. Sampling now reserves two attempts per
gameplay/settings/movie route while retaining the six-pair total. Runtime
collection now supplies both reserved pairs for each route, complete native
begin/end snapshots and exact-value correlation for all 14 `scene30.ppfx`
descriptors, plus native default/internal color/depth and placeholder identities.
Five loaded effect classes/names match the bounded map. This is live-tested
structural correlation, not an allocation/lifetime proof; unmatched resources
remain explicit. One settings frame also supplies naturally scheduled sprite/flush
ordering against the original-size default color target with matching device
generation. Other sampled frames lack that ordering evidence. Pixel coverage,
programmable constants/history, private-family rollback and submitted OpenVR
output retirement remain unqualified; no resizing writer is enabled.
The canonical full-profile verifier retained rejection for an unobserved Sense
loading-continuation press. Complete collection, normal restoration and operator
acceptance do not overwrite that verdict or establish the missing input gesture.
The operator subsequently reports that only Circle and Options continue the
encountered loading screens; this does not establish Cross/trigger continuation.
The exact loading handler schedules its continuation only on key-down, while the
previous Cross/trigger adapter delivered key-up. Both adapter paths now send one
key-down, with JNI admission, single-delivery and exception cleanup host coverage;
physical continuation remains pending in the next combined candidate. Delivery
telemetry must not be equated with timer resume or mission continuation.
The general verifier also required the obsolete Circle gameplay-pause mapping.
It now requires Options with the same successful native-stereo Escape route;
host fixtures reject Circle-only, flat-only, failed, foreign-route and missing
evidence. The already archived rejection is preserved.

## Current feature state

| Boundary | Confirmed state | Remaining limits |
| --- | --- | --- |
| Native D3D9 stereo and head tracking | Headset-validated native per-eye rendering, 6DOF, recenter and room-scale movement in exercised scenes. | Broad loading/device transitions and sustained pacing. |
| Locomotion | Exercised native analog movement, head-relative direction, jump parity, physical crouch and corrected snap turning accepted. | Mounted contexts and campaign-specific movement. |
| Body and hands | Independent tracked native hands/weapons with original torso/legs accepted in exercised contexts; head and connected arms hidden. | Left/dual/special weapons, broader finger and recovery coverage. Connected-arm IK is a diagnostic, not the accepted presentation. |
| Weapons and whip | Right-hand pistol firing, muzzle/impact effects, water and destructible bottles accepted. Exercised whip use and approximate reticle alignment accepted. | Broader weapons, ballistic birth vectors, whip reach/climbing and special optics. |
| Physical melee | The current knuckle-contact route is headset-validated in an exercised fight: the operator reports defeating three enemies with Hands, and says the combat felt much more responsive. | Scoped to that fight and configuration; left/right parity, broader enemy/campaign contexts, and tracking/menu/recenter recovery remain unqualified. |
| Menus and Sense controls | Automatic hand pointer, same-hand trigger selection, Cross/Circle, pause, confirmation dialogs and mouse coexistence accepted. Wheel selection/haptics, on-foot crouch, ordinary Focus and Create tap/hold accepted where exercised. Ray's Bible selection/use and on-foot kick have scoped headset acceptance. | Dashboard/loading/held-input recovery, mounted routing, campaign permission changes and unavailable mechanics. |
| HUD and subtitles | Encountered native health/ammo text, compass, equipment wheel, essential text and subtitles accepted. Independent mission countdown, encountered objective/journal notices and corrected green direction/red damage marks have scoped headset acceptance. Optional status rows accepted only where encountered. | Wrist health styling, concentration state glyphs and paired horse health/fatigue meters are host-tested only. Horse and special HUD contexts, newly translated prompts, scripted-movie comfort/subtitles and complete graphical HUD capture remain open. |
| Interaction | Pickup/carry/put-down and drawers accepted through the previous HMD reference. | Latest left-Sense L1 ray and held-only cyan reference are host-tested, awaiting physical acceptance. The permanent HMD ring was rejected for comfort. |
| Persistent manual reload | Opt-in `-ManualReload` is scoped headset-validated: Square opens/closes without auto-loading, hands stay tracked, successive free-trigger cartridges load, insertion-only haptics work, weapon/menu recovery works where exercised. Native unit transfers and restoration have correlated live evidence. | Measured loading-mouth geometry/precise insertion, tracking/save/load/death/disable recovery, held-control edges, mirrored/Frontier use and prolonged comfort. |
| Cartridge visual and pinch | Textured CC0 .357 visibility has scoped headset acceptance. Revised .7/.7 pinch and skin-derived anchor have host tests and bounded live delivery evidence. The later displayed-finger tangent is host-tested only. | The reviewed close-up predates that tangent and still appears superimposed rather than convincingly held. Contact, axis, scale, occlusion and comfort are unaccepted; scene depth and native lighting are unavailable. |
| Native loading port | Operator confirms the port looks correct in exercised reload; its visual presentation has scoped acceptance. Read-only gate/drum motion and six-mouth pivot rankings have bounded live evidence; generic tip/socket policy has synthetic host tests. | No qualified CoJ socket is connected to that policy. Active loading uses the 14 cm grip zone. Phase-bound mouth geometry, precise insertion, independent mechanics and ejection remain planned. |
| Death and save/load | Corrected failure-screen visibility and latest exercised boxing death/recovery have scoped operator acceptance. Exercised narrated loading after reversible LAA correction and save/load with the thumbnail workaround accepted. | Broader retry/back/held-input recovery, manual-session save roundtrip and allocation pressure remain open. |
| Transport and shutdown | Primary D3D9Ex GPU sharing and normal shutdown have bounded headset/live acceptance. Independent consumer-query recovery, retained quarantine on failed recovery proof, ResetEx/replacement admission and cleanup hardening are host-tested only. | Sustained pacing, abnormal exit, arbitrary reset/device loss and Steam recording-associated microskips. Automatic continuation after a native failure is not live/headset-validated. |
| Ownership recovery | Global input release barriers/failed-poll rollback, partial stereo invalidation, pointer resize cancellation and latest manual-session recovery have host tests. | Physical focus/dashboard/tracking/held-control and eligible resize recovery are still open; earlier menu/reload acceptance does not qualify these changes. |
| Persisted validation verdicts | Host-tested reporting and rejection policy; live-tested canonical rejection remains inventoried through collection and original restoration. | A failed telemetry profile remains failed alongside scoped operator acceptance. No automatic headset acceptance; successful full-profile workflow coverage remains separate. |
| Protected-target warning | Red aiming-cross alternative implemented and host-tested. | Visor visibility/target transitions; original warning was absent in visor. |
| Other platforms | OpenXR experimental; D3D10 and additional games planned. | No physical support claim outside the active D3D9/OpenVR target. |

## Host verification boundary

The latest combined candidate exposed a high-address identity rejection in
otherwise enabled manual reload and physical Hands. The operator reports native
reload even with one pistol and empty support, and full native body with Hands.
Runtime observation confirms invalid weapon identity in an otherwise coherent
single-pistol context and selected boxing with no admitted owner. Signed-positive
`GetThisID` predicates rejected LAA high-bit owners; preserving opaque 32-bit
identities is now host-tested across reload admission/recovery/readback/tickets
and native/JNI boxing identity, contact and diagnostics. Zero/all-ones and changed
owners still reject. The operator confirms that the corrected candidate restores
independent Hands presentation, manual reload and physical melee in the exercised
session. Inventoried evidence correlates manual insertions with native unit
readbacks and records scoped hand presentation and admitted physical boxing.
This is scoped headset acceptance of the restored functional baseline. The
collected samples do not establish a high-bit weapon owner in that successful
session, so actual high-address runtime coverage remains separate from the host
regressions. Cartridge contact/occlusion, broader weapons, campaign permissions
and tracking/menu/save/death recovery retain their own gates. The complete
collected run still retains its canonical telemetry rejection for the unobserved
loading-screen Sense continuation; operator acceptance does not change it.

The configured Win32 Release build succeeds. The public OpenVR Release tree registers 89
CTest tests: 88 passed, none failed and one classic shared-texture test was skipped
because the environment does not provide that capability. Classic shared render
targets remain unavailable (`d3d9_classic_d3d11_shared_texture`); the pool probe
passes. Transient classic-device creation unavailability has also been observed.
The skip is neither an accepted fallback nor a failure of the active D3D9Ex
transport. The suite
covers production policy, real host GPU interop, synthetic JNI/native fixtures,
deployment/restoration and evidence rejection; it does not exercise a campaign
or initialize SteamVR for the OpenVR input fixture.

Public distribution has host coverage for inventory/tamper rejection, unknown
builds, foreign staging, owned recovery with truncated stage JSON, Unicode video
paths and bounded local reports. An opt-in isolated copy of the exact game also
exercises install, tracked-head/body enablement, all five controller files,
fresh session identities/log rotation, conventional/manual updates, retained
reload preference, byte-identical uninstall and injected deployment failure.
The extracted package uses Windows PowerShell 5.1 and bundled isolated Python.
These checks do not establish another PC, controller, campaign or headset gate.

Binding checks now reject unexpected or duplicate Sense source routes. Flat-menu
pointer uploads reuse presenter-owned scratch storage. These changes have host
coverage, without new headset binding acceptance or measured pacing improvement.
Building the optional OpenXR targets does not qualify an OpenXR game integration.
No feature is promoted to `supported`; the project remains pre-alpha.

The implementation gates and their dependencies are in
[Roadmap](ROADMAP.md#next-implementation-gates). Changes already host-tested need
the consolidated acceptance below, rather than being listed as missing code.

Scripted in-game cinematics now use native movie ownership to select flat theater,
including the naturally rendered module subtitle sprite. The running timer alone
previously admitted these movies as tracked stereo and omitted that subtitle path.
The movie getter/owner-loss/end and JNI fault cases are host-tested. Camera comfort,
subtitle visibility and entry/exit with held controls remain physically unvalidated;
ordinary encountered dialogue acceptance does not accept movie subtitles.

Community feedback identifies failure to recover stereo after the cinematic on
arrival in Hope, including a Quest 3 report. Reviewed community telemetry contains
three captured-but-unsubmitted pairs per process, persistent capture-ring
saturation and no later return to stereo after the final logged movie entry. Normal
shutdown completes without shared-texture open/copy or compositor-submit errors.
Source inspection and a failing real-device host regression establish that an
owner change before `EndFrame` can strand each unfinished slot until the ring is
exhausted. The next capture now reclaims only its unfinished unpublished slot;
same-device GPU command ordering preserves writes, while pending fences and
consumer leases remain protected. Classic readback and D3D9Ex/D3D11 host tests
cover repeated one-/two-eye cancellations, a fresh recovered pair, retained
consumer content and independent pending-frame survival. This is **host-tested**
recovery, not confirmation that Hope is fixed in the headset or that the fault
is specific to Quest. The supplied log spans multiple launches under one public
run identity and does not qualify as canonical physical acceptance evidence.
Include the Hope transition and ordinary controls after it in the next relevant
campaign acceptance; theater during the movie remains intentional.

A new campaign crash was reported while interacting near a chest/possible scroll.
Collected native evidence ends after interaction acquires blocking UI, with no
normal runtime end. The native symbol-only callstack does not identify the fault
instruction, so the cause and recovery remain unresolved. Original staging was
restored; collection retains the canonical rejection rather than declaring this
interrupted run successful. The next candidate records precise escaping render
fault addresses without swallowing the native exception. Record any recurrence
in the ordinary combined session; no separate immediate crash reproduction is
required.
The diagnostic source now also captures a bounded 16-byte window at the faulting
EIP when readable, explicitly marks unavailable instruction memory, and restores
the thread's Win32 last-error. Synthetic host tests confirm those properties and
unchanged exception propagation. No new campaign fault has been observed and
this additional diagnostic has no live or headset acceptance.

Physical straight, curved and ascending punches are implemented/host-tested for exact `WeaponHands`.
An earlier operator trial showed independent Hands but no damage. A detector
regression reproduced fresh repeated
presenter poses cancelling the release/guard/collected path. Fresh duplicates now
retain that state and the original timestamp; stale, regressing and interrupted
poses still cancel. Bounded admission/gesture diagnostics distinguish trigger
availability, release/guard stages, owner/context loss and native attempts.
Available open-fist repeats retain a confirmed release while cancelling any old
stroke; a later deliberate close admits a new measured movement.
The later trial still produced no damage despite admitted native Hands. Recorded
time discontinuities and a deterministic 90 Hz cadence replay exposed coarse
adapter timestamps aliasing distinct poses. High-resolution steady time corrects
that boundary without relaxing gesture thresholds. No native attempt was logged
in that trial; contact/damage remain unaccepted.
The latest trial confirms intermittent physical damage, predominantly with a
straight extended arm. Natural hooks and repeated closed-fist combinations
remain unreliable and are not accepted. The intended interaction is sustained
one- or two-trigger fist closure plus natural motion, without trigger pumping
or repeatedly parking the hand in a stationary guard. Native permissions,
cooldown and damage remain authoritative; observed attempts do not establish
hit rate or contact alignment.
The subsequent completed-curve candidate was also rejected: the operator still
reports one effective punch in many attempts. That run contains 16 native melee
entries, nine requests with native `Hit` calls and seven without; neither these
counts nor synthetic gesture coverage establish reliable enemy damage. Long
curves and discontinuities also left closed fists behind release fences. The
active integration is being replaced with current-contact evaluation and
measured displayed-fist geometry. Cinematic continuity was not exercised in
that session and remains pending.
Select native Hands, release the triggers once, then hold either or both
corresponding triggers to claim closed fists. The current implementation queries
contact on each fresh pose using measured displayed knuckle skin and only the
adjacent pose's movement. Jabs, hooks, uppercuts and short fast movements need no
completed curve, guard pause, minimum stroke length or timed rearming. Native
attack permission requires real grip and head-relative translation at the
existing speed threshold. A stationary or rotation-only fist cannot attack.
Contact latches the hand until a later clear query, preventing repeated attempts
while touching the same surface. Long movement no longer exhausts a stored path;
a discarded geometric jump rebaselines without requiring trigger release.
Native permissions, selected attack parameters, cooldown, collision and damage
remain authoritative. The native animation no longer schedules this VR attempt.
Four original skin witnesses follow the actual displayed finger frames and skin
weights for both hands and both player meshes. Queries use separate current
movement rails and a current knuckle cross-section, not a historical curve or
cross-finger movement shortcut. No arbitrary reach or radius is added. The
native query's endpoint mutation is isolated with copies, and owner exclusion
flags restore on success, miss and exceptions. One native attack/hit list is
invoked on eligible contact; query-only poses cannot spend cooldown or damage.
This is sampled skin contact, not complete fist-volume or guard/blocking support.
The latest visor trial validates useful physical contact in one exercised fight:
the operator defeated three enemies with Hands and reported a substantial
improvement. This promotes the current contact route to scoped headset-validated
for that context. It does not establish every punch shape, both-hand parity,
campaign-wide permissions, or recovery after tracking/menu/recenter loss. Native
attempt, melee-entry and `Hit` counters remain diagnostics rather than health
evidence; the operator's reported enemy defeats provide the physical outcome.
Dual pistol reload is unchanged.

## VR settings and cinematic choice

The presenter settings panel, persistent theater/immersive preference, exclusive
input capture and release barriers are host-tested. Tests cover short/held
Options, held entry/exit controls, owner/focus/action loss, malformed configuration,
save failure, Unicode text and actual host GPU image upload/readback/reuse.
Never-bound optional actions and one-hand UI availability retain native input;
previously held unavailable sources still require an available release.
An earlier physical attempt rejected settings usability: holding Options caused
flicker/native-arm reappearance and no visible panel. Runtime logged repeated
overlay-publication failures; the earlier host image tests did not exercise
cross-process overlay sharing. Physical menu visibility and recovery remain
unaccepted for that earlier candidate. The latest corrected panel is visible
and functional in the exercised headset session; it has scoped operator
acceptance. This does not qualify every ownership/recovery transition.
The corrected surface passes real second-device shared-resource open and pixel
readback on host GPU/WARP. These tests do not identify the exact API that failed
in the old run or establish visible SteamVR presentation. Scoped settings-hand
rendering and natural-camera fallback now preserve the VR reference; the real
camera-hook regression reproduces and prevents the former automatic rebase.
Capture admission retains, rather than renews, the last successful pair's
freshness; transient and sustained capture failure have host coverage.

The boxing death report was an unresponsive native menu, not a demonstrated
renderer/process deadlock: fresh frames and normal shutdown continued. Exact
module bytecode identifies Space/ Escape as menu 38's native consumers; generic
UI Enter/mouse delivery does not reach that continuation. Visibility/type-gated
native dispatch and failure cleanup have host regressions. The latest operator
confirms death presentation/recovery worked in the exercised boxing context;
broader retry/exit and held-control transitions remain unaccepted.
Theater remains the default. Opt-in immersive movies use camera-only stereo:
gameplay/body/weapon mutation stays blocked, and invalid/dead/mission-end/frozen
owners retain theater. Movie subtitle sprites are not yet transported in
immersive; the settings panel discloses this limitation, and theater retains
their naturally rendered native pass. Neither host tests nor the operator's observation of a cinema
screen qualify immersive comfort, movie subtitles or transition recovery.
An earlier immersive trial reported intermittent wrong-image flashes lasting
about a second. After the continuity corrections, the operator reports no problems
while toggling cinematic presentation and entering movies, and no persistent flat
presentation afterward. This accepts mode switching and recovery in those exercised
scenes only; no specific Hope scene, immersive subtitle transport, full comfort
coverage or gameplay-hand/reload recovery is established by that observation.

Add these checks to the same ordinary session:

1. **Settings:** tap Options for native pause; hold it for one second to open
   VR settings. Release buttons and center sticks, then use right-stick left/right
   or Cross to change Cinematics; Circle closes. Check both native startup/pause
   menus and gameplay remain usable, with no wheel, snap, shot or reload leaking.
2. **Physical melee:** the current contact route has scoped visor acceptance after
   defeating three enemies with Hands. On a later naturally available fight,
   broaden left/right hand, punch-shape and tracking/menu/recenter recovery
   coverage. Do not repeat the accepted fight solely to reconfirm basic damage.
3. **Movies:** set the mode before a naturally encountered scripted cinematic.
   Check theater and immersive when scenes are available, including native
   subtitles in theater, head tracking, coherent eyes and ordinary input after
   the movie. Immersive subtitle transport remains a separate implementation gate.
   Do not replay a long campaign segment merely to repeat an accepted baseline.
   Frozen/unverified owners retaining theater are the conservative fallback.
4. **Persistence:** after normal quit, prepare a fresh candidate for any later
   session and confirm the chosen mode was retained. A displayed save failure
   means the selection applies only to the current session.

Comfort options beyond cinematic presentation remain planned.

## Current physical gate

Do not repeat an accepted baseline unless a change affects it. Batch related
checks into one meaningful candidate, recording unavailable cases as pending.

- **Interaction:** point the left Sense at a drawer's usable part and press L1.
  Head turning must not retarget it. The cyan reference must exist only while
  L1 is held. Check opening, pickup/carry/put-down and release after tracking loss.
- **Cartridge grip:** the current anchor has already been observed in runtime;
  repeating it solely to reconfirm visibility adds no evidence. After a material
  contact/axis/occlusion correction, combine empty-hand waiting, trigger pickup,
  slow wrist roll, release away, successive insertion, closing/firing and
  menu/weapon recovery. Include both hand assignments when available.
- **Manual mechanics:** the port's visible presentation is operator-accepted;
  do not repeat that check alone. Precise insertion still requires a measured
  loading-mouth frame and clearance through native drum phase.
- **Recovery:** qualify tracking loss/recovery, held controls across ownership
  changes, death/retry, save/load and disable during loading when available.
  Require no stale round, delayed action, extra transfer or permanent lock.
- **Campaign coverage:** mounted controls, broader weapons/special optics, whip
  climbing and optional HUD content need eligible scenes. The protected-target
  red aiming cross remains a separate low-priority check.

Authored VR labels now follow startup English/Spanish game locale; production
UTF-8 compilation, UTF-16 accents, native text preservation, localized wheel
confirmation and bounded unknown-locale fallback are host-tested. Judge English
wording, Spanish accents and card/wheel fit in the next relevant combined session;
no immediate or dedicated language test is requested. Native copied text retains
its own localization and visibility settings.

## Persistent manual reload candidate

Use one eligible single Peacemaker/Frontier with an empty support hand, missing
rounds and reserve. Prepare with `-BodyIkAtStart -LargeAddressAware -ManualReload`.
The support **trigger**, not Grip, carries/releases the cartridge.

Additional Frontier, Walker and Cogswell exact profiles are implemented/host-tested
for the same native unit-loading route. Cogswell observes rifle reserve; the
other admitted profiles observe pistol reserve. Schofield base/A/B retain native
whole-clip reload; LeMat and unknown variants retain ordinary native handling.
Exercise each new model only when available in the combined campaign session:
Square opening/closing, one loaded round/matching reserve decrement per release,
proper capacity/no-reserve closure, both hand assignments and recovery. Existing
Peacemaker acceptance does not promote these profiles or their loading geometry.


The exercised flow is `READY -> OPENING -> MANUAL_LOAD -> CLOSING -> READY`.
Fresh left Square prepares; native opening enters a supervised wait without
automatic transfer. Weapon and independent hands retain controller ownership.
Pick up at waist with the free trigger, carry to the existing weapon-grip zone
and release. Each accepted insertion calls native `WeaponReload` once; native
inventory/capacity/reserve remain authoritative. A fresh pickup/claim can repeat
the process. Square again or the armed trigger closes early; that closing trigger
press must not fire. Release and press again to shoot. Full/no reserve closes
without another round. Unsupported positively identified contexts retain native
reload; observation failures do not authorize that fallback.

Operator acceptance covers Square preparation/closure, no automatic loading,
tracked hands, successive loading, insertion-only haptics and the exercised
weapon/menu interruptions. Complete inventoried right-hand Peacemaker captures
correlate accepted insertions with `loaded +1 / reserve -1`, sustained waiting
beyond the original bounded probe interval, coherent closing and scoped recovery.
The offline analyzer found no unexplained transfer in the reviewed captures.
These findings do not accept every weapon, interruption or comfort condition.

The operator still reports a floating cartridge rather than convincing finger
contact. Convincing grip remains the next visual priority. Offline contact replay
finds slight pad/case overlap, not a gap justifying another position offset;
this does not establish runtime skin contact or occlusion. The short support-thumb
gesture is implemented and host-tested, with no live/visor acceptance. It begins
only after a qualified native unit transfer, flexes from .7 to .75 and returns to
ordinary fingers by 180 ms while the cartridge stays hidden. Native closing
retains priority, so an immediate final-round close can suppress this gesture.
Physical acceptance must judge its timing and appearance on both support hands,
including interrupted insertion and immediate repickup; no replay or extra round
is permitted.

The active insertion zone is the existing approximately 14 cm grip-proximity
zone. It is not an individual chamber or measured loading port. The read-only
root/barrel/gate/drum observer has bounded live evidence of changed native gate
pose and independent drum motion. The six-mouth pivot-ranking diagnostic has
bounded live observations, but the gate element origin is an unverified pivot:
`gate_open_confirmed=false` and `socket_admission=false` remain mandatory.
No independent gate/drum writer, spent-case simulation or chamber occupancy is
implemented. See [exact reload research](research/COJ_RELOAD_OWNERSHIP.md).

Optional acceptance haptics require coherent native unit readbacks and fresh
capture/tracking/gameplay ownership. Host tests cover duplicate/stale captures,
replacement, release barriers and output faults; live submission records and
operator feedback accept insertion-only delivery in the exercised context.
The revised zone-entry cue is implemented and host-tested, without live/visor
acceptance; insertion-specific sound remains pending. The soft cue signals a
qualified held approach, grants no round, and does not delay the stronger
native-confirmed insertion cue. Withdrawal hysteresis and separate cadences
prevent boundary jitter from repeatedly pulsing.

`manual_reload_pinch` and `manual_reload_cartridge` have bounded applied/visible
and idle/hidden observations. The optional `manual_reload_finger_reference`
reader has bounded live snapshots of native/displayed distal fingers; its
`weapon_id=0` diagnostic field cannot independently prove weapon ownership.
The revised host-tested emitter uses the native reload snapshot instead of
ordinary admission (which is unavailable while waiting), with explicit snapshot
validity, armed hand and context. New correlation remains pending runtime.
Joint observations do not measure skin contact. The current compositor lacks
scene-depth occlusion; improved anchor position alone does not qualify the grip.

## Consolidated candidate batch — host-tested, awaiting runtime and visor

This candidate combines a native-confirmed support-thumb gesture, a
displayed-finger tangent axis for the CC0 cartridge,
qualified zone-entry feedback, stricter Square rearming after ownership/input/
tracking loss and native Square recovery after disabling manual reload. Native
ammunition, the 14 cm insertion zone and body presentation remain unchanged.
Both-hand geometry and rotated-camera tests, fresh/duplicate/invalid gestures,
haptic priority/freshness and context/disable barriers have host coverage.
Direct native completion without an observed closing sample now discards pending
continuation tokens. Failed start/fresh preparation also clear them; a new
session requires its own initial waist pickup. Host regressions reproduce and
cover the former cross-session continuation and the former stale synchronous
ownership result on native insertion failure. Rejection versus invocation fault,
recenter, sample gap and regression have explicit cancellation reasons. No live
or visor acceptance is added by these regressions.
The read-only D3D9 depth observer is host-tested against a real host device,
including absent depth, attached D16 state and subsequent reset. It has no
exact-game observations yet and cannot authorize occlusion.
Offline same-frame depth pairing is host-tested against complete, missing,
reversed, duplicated, cross-generation/device and invalid-metadata fixtures.
It reports correlated/incomplete/rejected diagnostic groups without granting
capture correspondence, scene/hand coverage or a physical occlusion gate.

Use one fresh canonical candidate when the operator is available. There is no
request to repeat separate micro-tests or unavailable campaign contexts.

1. **Grip:** open an eligible revolver, wait with an empty support hand, take a
   cartridge with its trigger. Inspect close-up, wrist roll and palm-up/down.
   The port's accepted visual presentation needs no separate repeat check.
   Check the bullet follows the pinching fingers rather than cutting through
   them. The current compositor still cannot hide it behind fingers/walls;
   record that limitation separately from position/axis fit.
2. **Feedback and transfer:** carry into the grip zone while held. Expect one
   soft entry cue and no ammunition change. Small boundary oscillation must not
   repeatedly vibrate. Release for one stronger cue and exactly one native round.
   On a continuing load, the cartridge disappears and the support thumb makes a
   brief push before relaxing; it must not restart after an interruption or
   override a new pickup. Native final-round closing can suppress the gesture.
   Repeat several insertions; withdraw clearly to rearm entry feedback.
3. **Drop and closure:** release away from the gun, close with Square, reopen,
   close with the armed trigger, then release/press to fire. No extra insertion,
   retained pinch or firing on the closing press.
   After filling the gun, fire one round with a fresh press and reopen: the first
   cartridge of the new session must require pickup at waist. This extends the
   same batch, not a separate physical test.
4. **Held-control recovery:** carry a round, pause/change weapon/recenter or
   temporarily lose tracking, then recover with Square/triggers still held.
   No stale round, queued insertion or automatic reopening. Release and press
   deliberately to recover; also check returning from a menu while already
   `READY` with Square held.
5. **Lifecycle:** in eligible scenes, save/load, die/retry and use canonical
   `disable` during manual loading. Confirm restored control and conventional
   reload after releasing/pressing Square. Unavailable cases remain pending.
6. **Interaction and coverage:** left-Sense L1 drawer/pickup/carry/put-down while
   looking elsewhere; cyan reference only while held. Repeat reload with a
   left-hand assignment or Frontier if available in the same session.
7. **Menu/gameplay and HUD:** include the combined ownership regression below,
   holding mapped controls across pause/dashboard/tracking loss. If damage or
   healing is available, check the wrist meter against native health text,
   ammunition readability and hiding on blocking UI. Include source-size recovery
   only if a resolution transition occurs. If concentration or horse feedback is
   naturally available, check the discrete badge against its native text and
   the paired meters against health/fatigue percentages. Fatigue is not stamina;
   no cooldown duration/progress is inferred. Hidden owners must leave no stale
   glyph/bar, and essential health/ammo/countdown rows retain priority. These
   additions join the later combined session; no separate physical test is needed.

The default-off trace records zone entry/withdrawal, visual position/tip axis,
native insertions and submitted haptic type. Up to six `manual_reload_depth`
observations per opening record each eye's current target/depth/state at initial
held appearances; matching layout is not proven content. The offline analyzer
keeps these distinctions and never promotes a physical gate.

## Hybrid motion reload acceptance

This is the retained legacy path, separate from `-ManualReload`. A deliberate
free-trigger waist-to-gun journey requests ordinary native reload. Corrected
initial admission has scoped operator acceptance on the exercised Peacemaker.
Native animation still owns the original two-hand presentation.

## Cartridge-paced reload acceptance

The legacy Peacemaker/Frontier extension limits each natural completion to one
native round. Corrected recovery has bounded live evidence of replenishment and
unit count/reserve changes. It still takes the hands during each native cycle;
it is not the persistent manual flow and has no accepted physical chamber UX.
Keep it as a fallback/compatibility contract, not the next physical development
target. Schofield retains whole-clip native behavior.

## Bounded native wait/zero-transfer probe

The explicitly selected `-ReloadWaitProbe` derivative is host-tested and bounded
live-tested on a right-hand Peacemaker. It uses a roughly 1.5-native-clock wait,
grants zero rounds and closes through native callbacks; Square stays conventional
and presentation remains native-owned. It cannot combine with `-ManualReload`.
It established the logical veto/cancel seam used by the later manual candidate,
not mechanical clearance or persistent tracked-hand acceptance. No repeat probe
is needed merely to reconfirm that discovery.

## Revised Sense control acceptance

The [default Sense table](research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls)
is the sole binding reference. Exercised direct controls, wheel confirmation,
haptic highlights, snap rearm, on-foot crouch, ordinary R3 Focus and Create
timing have scoped operator acceptance. Ray's Bible selection/use and on-foot
kick are also operator-accepted in exercised gameplay. This does not qualify
concentration, duels or campaign permission transitions. Mounted contexts,
broader weapons and bow/scoped optics retain independent gates.

## Automatic menu-pointer acceptance

The automatic pointer uses same-hand L2/R2 selection; Cross accepts, Circle backs
out and Options pauses. Exercised ordinary menus, confirmation dialogs and mouse
coexistence are accepted. Dashboard, loading, resource replacement and held-click
recovery require broader coverage without stale clicks or haptic replay.

## Combined menu and gameplay regression

Changes to ownership need menu -> gameplay -> wheel -> pause -> dashboard ->
gameplay coverage plus available death/retry and save/load. Hold mapped controls
across transitions: no delayed fire, interaction, reload, snap or utility action
may escape on recovery. Native permissions remain final.

Global OpenVR button rearming, failed-poll rollback and partial stereo-submit
invalidation are implemented/host-tested. Host regressions reproduce an
unavailable button fabricating a press on held reacquisition and an old left-eye
confirmation completing across a tracking/lifecycle gap. The real input-poll
transaction is exercised with partial outputs and exception unwinding without
initializing the runtime. These checks do not qualify physical recovery.

Pointer source-size cancellation is implemented/host-tested for width and height
changes with unchanged hand/menu identity, stale dispatched snapshots, held/edge
packets, renewed hover and deliberate selection. A deterministic interleaving
also reproduces and covers an idle native cursor result returning after resize.
Include held L2/R2/Cross/Circle/Options recovery after available focus/dashboard/
tracking loss in the next combined session; check source-size recovery only if a
resolution transition occurs. No extra visual micro-test is requested.

## Production D3D9Ex startup/reset gate

Primary GPU transport and normal shutdown passed their bounded physical gate.
Arbitrary reset, abnormal shutdown, sustained pacing and campaign transitions
remain open. A skipped classic shared-texture environmental test is neither a
pass nor physical validation. Transient host D3D9 CreateDevice failures have also
been observed; their cause has not been established.
Implemented hardening retains producer leases on a failed D3D11 copy-fence poll
and moves lease bookkeeping ahead of copy submission. Sampled lease telemetry
tracks copy ordinals and accounts for shutdown-drain retirements. The Release
build and GPU transport host test pass. Test-only injected GetData failures on
real queued D3D9Ex/D3D11 copies verify retention through pending proof, ResetEx
and device replacement. A separate event query on the original immediate context
proves retirement of all quarantined copies before cache reopening and fresh
generation admission. The fixture covers multiple retained leases, repeated
recovery, D3D9Ex/D3D11 replacement, mismatched endpoints and stale generations.
If that independent query also fails, cache reset, later polls and shutdown keep
the uncertain lease; the fixture explicitly drains physical GPU work before
destruction. Destroying a failed bridge does not mark consumer_done: a real
producer fixture retains its occupied slot, rejects generation replacement and
defers invalidation/shutdown after the consumer disappears. Original errors
remain counted, and the canonical verifier persists a failure even when recovery
succeeds with matching copy/completion totals and
zero pending/abandoned leases. This is host-tested consumer-query recovery;
native device removal, resource teardown/session recreation and runtime/headset
performance remain unverified. No timeout or skipped frame grants lease reuse.
A separate test-only D3D9 producer GetData failure while a real GPU copy is
pending now verifies quarantine of the affected ring slot, successful collection
through a healthy successor, and a single invalidation count across shutdown.
The fixture drains its GPU queue before teardown. Both failure injections run
only in the dedicated host test target. Full Release build and CTest have zero
failures; the host verification section records environmental skips. Production
device-loss/reset behavior after an actual failed query remains unobserved.
The host startup contract additionally exposed partially restored factory slots
already pointing to retained originals. Cleanup now acknowledges those exact
targets without a write, preserving foreign conflicts and protection-failure
recovery. A deterministic regression and repeated host startup checks cover this
case; the external restorer and exact-game lifecycle effects remain unobserved.

## Closed locomotion diagnosis

Native per-axis shaping and float actions 4–7 remain the accepted movement seam.
Measured vanilla movement/jump parity closed the earlier diagnosis. Preserve
camera/body restoration; no speculative physics retuning is pending.

## PC mechanics and gameplay HUD completeness

Critical countdown presentation, encountered objective/journal notifications and
corrected directional attack feedback have scoped operator headset acceptance.
They are implemented and host-tested independently of wrist visibility.
The countdown copies native localized title and exclusive numerical cache; green
direction/red damage chevrons preserve active native indicators, absolute bearing
and fade alpha. Reader faults/foreign owners, bearing conversion matching the
native camera-back convention (four cardinal directions, head and reference yaw),
real binocular GPU pixels, invalid second eye and stale/empty snapshots are tested.
The operator confirms the recovered countdown/notices and the corrected attack
marks in the exercised scenes, including the requested direction checks. The
camera-back conversion preserves native front/back/left/right placement under
head and reference yaw. Broader simultaneous-content readability, unencountered
notices and pause/loading/death or owner-loss transitions retain independent
gates. No timer/damage authority is recreated. This does not accept complete
graphical HUD coverage or require another run solely to reconfirm these visuals.

Accept only observed content. Horse, broader weapons, climbing, duel/special
modes and campaign indicators need coverage. Full graphical HUD capture and
complete physical cylinder reload are planned.

The new wrist health meter and low-health text colors are implemented and
host-tested, including 0/25/50/100 coverage, hidden/out-of-range cache fallback,
cache reuse and preservation of the ammunition row. Offline rendered images
have been inspected; this is not runtime or headset evidence. Existing accepted
health/ammunition text remains distinct from this new presentation gate.
Add one check to the next consolidated gameplay session: after available damage
or healing, confirm that the bar follows the displayed native percentage, remains
readable beside ammunition, and disappears with its health row or blocking UI.
No separate short headset run is required for this addition.

## Image-quality profile acceptance

Canonical preparation preserves selected resolution and stages FSAA 0 unless
requested otherwise. Judge readability, aiming and motion stability separately.
Steam recording off produced accepted head-turn stability; recording-associated
microskips and sustained performance remain unresolved.
The operator also reports good current gameplay performance without another
recording test. Equipment capacity/configuration is a possible explanation for
recording-associated microskips; no cause is established or attributed to the mod.
This supports the exercised experience; recording compatibility,
measured sustained pacing and actual reset/device-loss recovery remain separate.

## Evidence policy

Use the canonical reversible workflow:

```powershell
pwsh -File tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez" -BodyIkAtStart -LargeAddressAware -ManualReload
# Start SteamVR and Call of Juarez manually; exercise relevant gates.
# Close the game normally.
pwsh -File tools/vr_test.ps1 finish
```

Each physical run needs fresh source/build/deployment/run identity. Never launch
or terminate the game or SteamVR automatically. Preserve staging/recovery
journals and original backups until `finish` completes. A historical run manifest
marked `staged` is not evidence of an active deployment; inspect canonical status
and the current journal. Host recovery tests do not establish headset acceptance.

Evidence collection, verifier success and operator acceptance are separate facts.
`analysis/validation-result.json` now persists the canonical verifier's telemetry
outcome and rejection reason, source/build/run identity, profile, requirements and
input hashes. `evidence-manifest.json` inventories that result and the build
manifest; `incomplete` continues to describe only runtime termination. Changed
log/profile/build/deployment/verifier inputs reject a stale executed verdict.
Collection without verification explicitly records `inconclusive` and
`verifier.executed=false`; recollection cannot invent a verifier execution.
Native run/build markers now require one exact start and at most one matching
later end. Duplicate, foreign, malformed or reversed markers reject; profile
checks ignore telemetry before the start or after the end. Invalid intervals
remain incomplete during collection, even with a matching structured summary.
If invalid markers prevent the required performance summary, collection still
retains raw evidence and the non-passing verdict; a valid interval retains its
summary requirement.
These correlation corrections are host-tested only and need no separate visor run.

This boundary is implemented/host-tested for startup's early return, full and
transport checks, canonical rejection, partial runs, absent verification, stale
inputs and ZIP/hash integrity. An isolated real `finish` fixture verifies that a
complete collected run and successful original restoration still retain/report
the failed verifier. A fault after proxy removal retains the journal; a
restoration-only `finish` retry preserves the original verdict and ZIP hashes.
Result-publication failure also cannot prevent original restoration. Missing
required gestures remain canonical rejection, even
if the operator separately records them as unavailable. Optional run-local
`operator-observations.json` is separately inventoried and cannot change telemetry
outcome. `headsetAcceptance=not_evaluated` is mandatory: neither `incomplete=false`,
matching hashes nor a dashboard-cycle summary accepts recovery or a visual gate.
The canonical rejection path is now live-tested: an inventoried, complete
native run retains its rejected full-profile verdict through collection and
original restoration. The missing loading-screen Sense continuation observation
remains a telemetry rejection; separately accepted HUD and Ray controls do not
rewrite it. Successful full-profile workflow coverage remains separate. This
diagnostic boundary does not evaluate headset acceptance.

After `finish`, inspect collected reload evidence without modifying it:

```powershell
python tools/analyze_coj_manual_reload.py "<collected run directory>" --output "work/manual-reload-analysis.json"
```

Use an output outside the evidence directory. The analyzer verifies inventoried
hashes, deployment coverage and ordered native markers; it separates correlated
unit readbacks from rejection/inconclusive observations. It never accepts a
physical gate or replaces the canonical verifier.

Version durable conclusions and acceptance limits, not run chronology. Keep only
compact unresolved inputs and necessary local provenance under ignored `work/`.
Once consumed, remove duplicate videos/frames/logs/dumps, frozen game archives,
obsolete builds and SDK extraction/download caches. Keep configured dependency
roots, the pinned offline bootstrap-test archive and useful current build output;
these caches are not shipped. Never remove
active recovery data or user assets as evidence cleanup.
