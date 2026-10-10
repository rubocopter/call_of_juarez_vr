# Exact CoJ reload ownership

These contracts apply to the original Steam Call of Juarez (2006) archive
`code.pak`, SHA-256
`F9DB47C166E03F23E37CBCDFD5344E4AD4C5C9134F35E8F6DCDF66DB7E71CE12`.
Game hand indices are 0/right and 1/left.

## Active scope

The opt-in `-ManualReload` candidate has scoped headset acceptance for Square
opening/closing, tracked hands, successive trigger-carried native rounds,
insertion-only haptics and exercised weapon/menu recovery. The legacy hybrid,
paced and bounded-wait routes below remain compatibility/research contracts;
they are not the current persistent manual UX. [Validation](../VALIDATION.md)
owns acceptance. Finger contact, loading-port clearance and broader recovery
remain unresolved; do not infer them from callback or geometry observations.

## Native request and completion

`PlayerController.ExecuteInput(IFLjava/lang/Object;)V` action 31 calls
`ApplyReloadWeapon()` on a positive input edge. Its controller eligibility and
public `ArmedPlayerBeing.ReloadWeapon()` remain authoritative. The public method
rejects existing reload, computes reloadable ammunition for both desired weapons
and selects one hand. A genuinely empty support hand makes that choice
unambiguous. The protected `ReloadWeapon(I)V` only sets a desired reload state;
calling it directly would bypass public checks.

The pistol state machine has ordinary move/idle node 1, empty node 0 and reload
begin/body/end nodes 20/21/22. Alternate states carry bit 128 and remain outside
the initial gesture contract. `IsWeaponReloading()` observes desired states,
so it cannot alone establish current/destination readiness.

Reload begin/body/end callbacks own weapon skin, sounds and animation. Actual
round transfer occurs through `OnHandStateFinished_Reload` and
`PawnArmed.WeaponReload`: it resolves reserve, caps the transfer, writes both
weapon ammunition and reserve, and participates in reload-loop completion and
the weapon-reloaded game event. Frontier/Peacemaker logic defaults to single-round
loops; Schofield defaults to whole-clip transfer. Parameters remain authoritative.
`ForceReloadWeapon`, direct ammunition/reserve writes or direct completion calls
are not a demonstrated gesture-paced reload boundary.

## Initial hybrid gesture

The initial implementation requests the ordinary native reload after a deliberate
free-hand ammunition-zone-to-gun journey. It leaves native ammunition, automatic
reload, animation, sounds and events intact, and retains Square. The baseline route delegates the whole reload to native animation. The
experimental paced route below adds a visual token, not physical cylinder/ejection
ownership.

The passive reader admits only the exact concrete classes
`WeaponPistolFrontier1878_Regular`, `WeaponPistolSchofield_A`,
`WeaponPistolSchofield_B` and `WeaponPistolPeacemaker`. The shipped Home01 opening `ColtPeacemaker` is the Peacemaker class;
omitting it would reject the tutorial pistol before gesture policy. Current, destination, desired and actual states must
all be 1 for the armed hand and 0 for the empty support hand. Actual, active and
desired weapon objects must match by JNI identity; the support hand has none.
Both hands reject attack and actual/desired/state-machine two-hand flags. Death,
carrying, reload, alternate states, replacement and unavailable observation fail
closed. A second bounded observation rechecks coherence before publishing the
positive `GameObject.GetThisID()` identity. The caller supplies current local
player generation, native gameplay epoch and input context ownership.

These getters read shipped state. No ammunition-capacity, reserve-eligibility or
weapon-parameter helper is called: `GetWeaponMaxAmmoToReload` and parameter
getters can create native parameter objects on a cache miss. No native update,
trace, state change or reload is invoked by the reader.

Gesture policy is game-neutral, in XR metres, with physical trigger claims
retained until their own available release even after weapon-hand changes.
Native epoch loss/reacquisition, menu/wheel/equipment/interaction conflicts,
recenter, unavailable input/pose and stale samples cancel the journey. Completion
is a one-sample intent without a retry queue. Native reload continues to own
hand/weapon animation and the existing restoration path.

Policy, passive JNI and adapter checks are host-tested for the four exact classes;
the corrected hybrid gesture has operator headset acceptance for the exercised
Peacemaker context. Other contexts and interruption/recovery gates remain open.

## Cartridge-paced native completion

The experimental extension is limited to exact Frontier 1878 Regular and
Peacemaker classes. A fresh pickup/insertion arms a bridge-owned pending weapon
reference immediately before public action 31. The protected native request,
after public eligibility, validates the selected weapon, ordinary reload state,
single-round parameter, network ownership and non-multiplayer context before
any state request. Rejection clears pending and returns without starting reload.
Accepted state delivery transfers pending into a native-owned active reference
and hand. The bridge clears pending immediately after dispatch; renderer/context
cancellation cannot remove the accepted native limiter.

The ordinary reload completion still calls `WeaponReload` and retains native
round/reserve accounting and `_WEAPON_RELOADED`. The scoped limiter selects the
existing move/end route after one natural completion. Native end/replacement
reconciles active ownership. There are no direct ammo writes, forced completion
calls or parameter changes. Square, automatic reload, non-opted players and
Schofield retain native paths. Unknown class/archive hashes fail closed.

The pickup publishes a bounded cartridge visual token in the same captured
head-space geometry as both eye images. Fourteen bounded surfaces form a
controller-oriented brass/copper solid with per-eye back-face rejection. Its
tip pose matches the rendered apex and exposes local +Z inward. It is compositor
geometry rather than an individual native cartridge mesh and has no scene depth
occlusion.
An optional explicit insertion pose is supported by neutral policy; the active
adapter retains the validated grip-proximity gesture until a model loading-port
anchor is demonstrated. Native reload continues to own arm/weapon animation.

After a cartridge-paced request is dispatched, the adapter does not mint the
next token immediately. It first requires observation of the native reload
interval and then the same player, weapon, armed hand, input generation and
native gameplay context returning to ordinary eligible state. Only then does a
new presentation-only cartridge appear in the empty support hand. It does not
claim the trigger until a fresh press; a release near the gun requests exactly
one further native round, while a missed release leaves the token in hand for a
fresh retry. Context loss, owner/weapon replacement, native faults or lack of an
observed reload interval cancel the continuation. Tracking or support-trigger
availability loss during that interval also cancels it, as do non-finite poses,
regressing sequence/time, a gap over 250 ms or failed native observation.
Duplicate render polls cannot establish native progress or consume replenishment.
Native recovery can retain reload states and temporary two-handed flags after
`IsWeaponReloading` becomes false. A separate passive reader admits only the same
exact single-round pistol, actual/active/desired weapon object identity, empty
support hand and idle/reload-only native states. It retains an already accepted
continuation; it cannot start a new journey or replenish before ordinary admission
returns. Attack, replacement, observation failure and foreign states cancel it.
`HasSomethingInHand` conflates a carried object with the armed state machine's
two-hand animation ownership. Recovery therefore separately reads the pure
`HumanBeing.IsCarrying` predicate (`m_cCarriedObject != null`) twice. Positive
support occupancy is tolerated only when the armed current state and its own
state-machine two-hand flag explain that fallback, with no carried object,
zero support states and absent actual/active/desired support weapons. Initial
gesture admission still uses the original occupancy rejection. This distinction
is not permission to load while holding a world object.
During that verified interval, a held support trigger is consumed until a fresh
available release, including across subsequent context loss. This prevents native
cross-hand attack fallback while tracked muzzle ownership yields to reload animation.
This keeps ammunition,
capacity, reserve checks and completion timing native-owned while avoiding a
new waist pickup for every chamber.

The corrected paced recovery distinguishes temporary two-hand animation
occupancy from a carried object. Bounded live Peacemaker observations establish
replenishment and unit count/reserve changes. Native animation still takes the
hands in this legacy route. Persistent tracked loading is implemented separately
below; individual chamber geometry, clearance and ejection remain unresolved.

## Offline cylinder and loading-gate geometry

The shipped D3D9 meshes have separate animated loading parts and six modeled
rear chamber-mouth rings, but no named insertion sockets or individual cartridge
nodes. Mesh identity remains mandatory before interpreting these model frames:

| Model | Mesh SHA-256 | Cylinder / gate elements |
| --- | --- | --- |
| ColtPeacemaker | `0df76de44edefcba8ea640b85f119d5dbd20a31c1d2df7d1ab1e1c909aff63f0` | `ColtDrum` / `ColtLock` |
| ColtFrontier | `6d4577416a2c775783bed8294c3d72b3cf966381a5c2e6779a23761f3ded6044` | `GunDrum` / `GunLoader` |

Cylinder-local centimetre candidates measured from rear-face geometry are
Peacemaker `(-1.234718, -0.712462, -2.383278)` and Frontier
`(-1.185826, -0.010422, -2.010485)`. Their mouth-edge radii are about 0.593 cm
and 0.519 cm. These are geometric observations, not interaction tolerances or
proof of gate clearance. The insertion approaches along cylinder-local +Z from
the rear. A future target must use the actual displayed cylinder frame, including
native accumulated phase, and a cartridge-tip trajectory rather than grip distance.

Ordinary reload scripts select begin frames 0–8, loop 8–16 and end 16–25 at
30 fps. Isolated native ANM sampling uses frame-domain time: frame 8 opens
`ColtLock` approximately -50 degrees about local Z and `GunLoader` approximately
-40 degrees. The Peacemaker drum also rotates in its ANM, while Frontier's drum
ANM stays identity; native `ReloadRotate` and `RotateElementWithAnim` add a
separate accumulated rotation. Peacemaker's animated gate translation differs
from its mesh bind pivot, so a bind translation combined with the opening
quaternion is not a valid demonstrated pose.

Gate accessibility, final phase composition, reload-skin/attachment coherence
and both-eye restoration require native observation before enabling physical
gate manipulation. Offline ring and animation measurements do not accept those
boundaries or establish occupied-chamber/ejection semantics.

The displayed-frame observer resolves the drum and gate by native element name,
checks readback against the current controller-mapped child transaction and
rechecks JNI weapon identity after the reads. Six local mouth observations are
transformed through that actual drum frame in centimetres. This is read-only
diagnostic geometry; matching a weapon class/name does not verify mesh content
or permit a new physical loading mode.

An optional **implemented, host-tested** `post_overlay` probe now ranks the six
rederived Peacemaker/Frontier mouth centres by distance to the observed native
gate/loader **element origin**, after independent before/after native weapon/hand
identity reads. The origin may be a hinge/pivot and is **not** an established
loading-port centre. `manual_reload_geometry` records `mouth0`–`mouth5`,
`port_reference=gate_element_origin_unverified`, `port_owner_qualified`,
`port_rank` (`pivot_only`, `ambiguous`, `invalid`), `port_nearest_mouth`,
distance, runner-up, separation, axial/radial offsets in cm, and the explicit
`gate_open_confirmed=false;socket_admission=false`. Invalid, ambiguous or stale
owner/model/frame measurements never authorize a socket. The offline analyzer
parses these optional fields conservatively into `port_probe`, counts unique
pivot rankings, reports malformed combinations as `invalid_mechanical_geometry`,
and remains compatible with older evidence. Both C++ geometry and Python
analyzer have host tests; the ranking also has **bounded live observations**,
without headset validation of a usable socket.
No insertion-zone change, independent gate writer or chamber selection is
enabled. The previously accepted ~14 cm grip zone stays active.

## Persistent-session boundary

Original reload continuation traverses 21 -> 22 -> 20 -> 21. Pair-state reversal
can shorten visible closing/reopening, but still invokes end/begin/body callbacks.
An early zero-credit return from `OnHandStateFinished_Reload` prevents transfer
but still allows sounds, important-state bookkeeping and native cylinder rotation;
it also skips capacity/reserve termination. The completion callback ignores its
third boolean argument for transfer, so cancellation needs a no-transfer guard.

The original `StateMashine.Update()Z` transition boundary at bytecode offsets
54–63 precedes `SetCurrentState` clearing the old state, after transition
eligibility. It is the demonstrated logical seam used by the bounded-wait and persistent
manual derivatives below, not a general mesh pause API. The owner-scoped veto,
zero-transfer close and scoped tracked presentation are implemented and tested
at their stated boundaries; broader engine save/load/recovery remains open.

## Audited input, animation and observation boundary

The default Sense binding is left Square (`/user/hand/left/input/square`) to
`/actions/gameplay/in/reload`. OpenVR publishes `GameplayInputState.reload`;
the control mapper and command queue retain gameplay ownership/release barriers.
`JavaPlayerBridge` dispatches native action 31 after releasing the analog state
lock. `PlayerController.ExecuteInput` recognizes its positive edge;
`ApplyReloadWeapon` checks `CanApplyState` and invokes
`IControlledPlayer.ReloadWeapon()`. The public armed-player implementation
selects a reloadable hand before the protected state request. Holding Square is
not permission to repeat successful delivery.

Ordinary right/left Ray clips are `Ray_Peacemaker_Reload_R/L.anm` and
`Ray_Frontier_Reload_R/L.anm`. Their script tracks select 0–8/8–16/16–25
at 30 fps. Alternate tracks use different ranges and remain excluded. The weapon
tracks and player hand clips are separate resources, not a proven independent
runtime playback API. `PlayerStateMashine.NextFrame` advances current time and
delta; `GetMinAnimAdvance`/`GetMaxAnimAdvance` derive normalized advance from
current/start time, play time and duration. Morphing/pair reversal and weapon
state-time parameters also participate; clip frame ranges do not establish exact
wall-clock opening or transfer times. Completion tests normalized maximum
advance, then `StateMashine.SetCurrentState` clears the departing state and
dispatches the callback. The third callback boolean does not guard transfer.

`MeshObject` does expose native `PauseAnim(IZZ)V`, `SetFrame(IFI)V`,
`StopAnimSingle(IZ)V`, `PlayAnimAtTime(IFIZ)I`, `GetAnimFrame(I)F` and
`SetAnimSpeed(IFZ)V` (with additional overloads). These are real animation API
declarations, not a demonstrated reload pause API. The script state clock and
`UpdateStateTime`/`AnimationNode.PlayAnimation` continue controlling playback
independently. Pausing or setting a mesh frame does not establish that state
completion, ammunition callbacks or subsequent animation writers also stop.
The identifier/slot correspondence, scope of element animation blocks and
transactional resume/restore require observation before invoking these writers.

The ordinary Peacemaker/Frontier ammunition constructors assign six-round
capacity, null begin/end reload sounds and a body reload `SoundBuffer` for
`Data/Sounds/weapon/Frontier`, `Frontier_Ammo_Open`. The native reload-body route
owns this sound. The declaration is not proof of an insertion-specific sound or
of audible timing; no additional sound call or haptic event is introduced here.

Conventional and legacy-paced reload retain native hand/weapon presentation.
The persistent candidate below applies an owner-qualified exception for the
existing restorable independent-hand/weapon maps. This has scoped tracked-hand
acceptance without establishing a mechanical writer or exact loading socket.

The legacy-paced cartridge remains a value-only compositor token, not an engine object:
fresh free-hand **trigger** pickup at the waist, controller-oriented fourteen-face
solid, held travel and trigger release within 14 cm of the armed grip after the
minimum journey. Grip buttons retain their existing actions. Mere proximity
does not insert. A valid release produces a one-shot request; disappearance
signals token consumption, not successful native ammunition transfer. The ticket
and native callback own acceptance/accounting; the next token requires observed
reload and ordinary same-owner recovery. Invalid release, drop, timeout,
tracking/context loss and owner changes follow existing cancellation and release
barriers. There is no persistent `MANUAL_LOAD` state in this route.

### Default-off trace

`reloadTraceEnabled` in the existing camera control file enables the passive
`reload_trace` event. `tools/set_hmd_camera_control.ps1 -Mode reload-trace-enable`
and `reload-trace-disable` preserve all other control fields. Default false
performs no additional JNI reads. No hook, archive patch, animation clock write,
ammunition write, sound replay or presentation ownership change is introduced.

The exact-game reader admits coherent actual/active/desired identity for one
Peacemaker/Frontier, empty support weapon references, positive weapon ID and
native `cOwner` identity. It copies both current/destination/desired/actual state
IDs, animation IDs, current/start/play/duration time, minimum/maximum advance,
`m_fRotate`, pistol reserve and raw `GetAmmoCount(0)` readback. All JNI references
remain local; missing methods/fields, non-finite values, exceptions or replacement
discard the whole snapshot. The bridge rechecks player generation.

`InventoryAmmoCounterPistol.GetAmount/SetInternalAmount` explicitly back reserve
with `PawnInventory.nAmmoPistol`. In contrast, stock Java `Weapon.GetAmmoCount`
is a zero-return stub and `SetAmmoCount` calls the skin update. Observing the
native transfer *call boundary* does not establish the loaded runtime's final
count implementation. The trace deliberately labels this value `ammo_readback`,
never confirms transfer, and never treats a constant zero as an empty gun.
Contrast it with the native HUD and reserve before claiming exactly-one-round
runtime acceptance; authoritative runtime count storage still needs correlation.

Bounded right-hand Peacemaker observations establish useful virtual
`GetAmmoCount(0)` values: admitted paced reloads change loaded +1/reserve -1,
full-gun requests leave counts unchanged and conventional Square can load the
remaining capacity. This does not locate final count storage or generalize to
other weapon classes. Sampled begin/body/end play/duration is 0.3 native-clock
units, while transitions may traverse inside one update and reset the state
start time. State IDs alone do not identify every callback or actual ANM frame.

Events report old/new states, cartridge presence, entry/exit of the legacy grip
zone, reload intent, context/recenter/tracking/conflict cancellation and owner
policy. Ownership is labeled `vr_if_restorable`/`native_reload`/`unavailable`,
with `actual_draw_owner=unobserved`: dispatch policy is not proof of a rendered
pose. Before/after numbers are comparable only with the same player, weapon,
hand and native context; lost/reacquired observations establish a new baseline.
Unchanged polls and continuous time changes are silent; a pending input intent
emits once, and same-frame blocking-UI acquisition may emit one terminal loss.
Pre-dispatch render sampling can miss transitions traversed in one native update.
It cannot prove callback order, sound playback, an exact ANM frame or a safe hold.

Reader fault/replacement/reference-cleanup, event filtering and control parsing
are host-tested; read-only trace has bounded Peacemaker live evidence. Persistent
manual-session acceptance is separate and described below. No insertion credits
or second ammunition authority are used.

### Bounded pre-Clear wait probe — implemented, host-tested, bounded live-tested

`tools/coj_reload_wait_probe.py` is an explicitly selected archive derivative,
not the default reload path. `patch_coj_weapon_consumers.py --reload-wait-probe`
requires the same original archive/player identity plus original
`StateMashine.class` SHA-256
`d8bc41b303ee2b419c42dd3f93318c1f76e1c4ba1b51d2df824c4b846d2af8f8`.
Only those two classes differ from the existing five-class derivative. An unknown
class, missing closing state or foreign veto rejects experimental admission
before the native reload setter. An ordinary request without a paced ticket
retains the existing native route, including Square.

After the existing exact-class/local-owner admission, transient instance fields
bind the admitted weapon, hand and exact `PlayerStateMashine` to a nullable
owner reference on that machine. At the audited `Update()` offset 60, after
native `CanChangeState` and before `SetCurrentState`/`Clear`, that owner may veto
only its current body state 21. The first approved exit records a deadline 1.5
native-clock units ahead; unchanged waiting polls produce no telemetry. Deadline,
clock rollback/non-finite time, death, network-owner loss or changed
actual/desired weapon permits cancellation. The held logical state is not proof
that the actual mesh ANM or loading gate remains open.

For the admitted hand, the probe skips only the original `WeaponReload` call
and stores false in its native result local. Capacity/desired-state closing and
the rest of the callback still execute; the native result guard consequently
suppresses `_WEAPON_RELOADED`. No ammunition field is written or copied into a
second authority. `ReloadEnd` detaches the owned veto through its actual callback.
Machine replacement closes an obsolete body under zero-transfer ownership before
releasing it. Failed admission cannot fall through to a real paced transfer.

Explicit cancellation uses the demonstrated native `FindStateByID(22)` and
`SetCurrentState` boundary, retaining old `Clear` and new `Set`. A save prefix
cancels before the original `InventoryBeing.SGSaveChunk`; load cancels/clears the
old owner before original deserialization. Save layout is unchanged and transient
probe fields are not added to its native chunks. Native `OnHandStateStarted`
still performs `UpdateHandState` reconciliation. Exact engine save/load recovery
has not been run; prefix order and zero-transfer cancellation are host evidence,
not a claim of physical or full save-roundtrip validation.

Host tests execute the patched protected request, `Update`, `SetCurrentState`,
body/end callbacks and injected helpers. They cover timeout through ready and
another request, explicit cancellation, full/no-reserve counters, repeated
callbacks, other-hand isolation, replacement, death/network loss, invalid clock,
missing closing state/foreign veto and original-build rejection. All new symbols
resolve against the shipped class hierarchy and relocated branches target actual
instructions. Engine scheduling/skin and native transfer are stand-ins: this is
not a Chrome JVM verifier, actual animation playback or headset evidence.

Canonical preparation accepts `-ReloadWaitProbe` only with `-BodyIkAtStart`.
The staged archive, run manifest and `reloadWaitProbeInstalled` observation
marker record opt-in identity. Trace reads status 1/2/3/4 (armed/waiting/closing/
cleared) plus deadline only for that marked derivative; missing symbols discard
the observation. The marker does not enable/disable native behavior at runtime.
The archive probe is removed by normal `finish` restoration. Disabling tracking
does not accelerate its native clock; the bounded zero-transfer close resumes
with native updates. Immediate tracking/menu/mod-disable cancellation remains a
gate for the future persistent session.

Bounded right-hand Peacemaker runtime evidence establishes armed -> waiting ->
closing -> cleared status, approximately 1.5-native-clock deadline exits and
earlier desired-attack exits, with no reload transfer under probe ownership.
Native Square, automatic reload and shooting recover. A first cleared sample
can already include a shot; compare reload accounting within owned intervals,
not count equality across a whole cleanup frame.

The clip shows original hand/arm movement. This probe grants zero rounds and
has no persistent `MANUAL_LOAD` or Square preparation; mechanical hold,
save/load and broader interruption behavior remain unqualified. The logical
veto seam is reused by the persistent candidate below. Repeating the probe is
not necessary to establish that already observed seam.

## Persistent manual candidate — scoped headset acceptance

`tools/coj_manual_reload.py` extends the exact zero-transfer derivative only when
selected with `--manual-reload` / canonical `-ManualReload`. It cannot combine
with `-ReloadWaitProbe`. Default and probe payloads retain their previous routes;
unrelated archive entries remain identical. Installation markers in control/run
manifests identify the derivative; toggles preserve that identity.

Fresh **left Sense Square** (`/user/hand/left/input/square`) is mapped to logical
reload. In the admitted single Peacemaker/Frontier context it arms the existing
immediate ticket before action 31, retaining the original controller/public reload
checks and native begin 20. The scoped native body 21 veto opens `MANUAL_LOAD`
when status 2 is observed. There is no whole-animation pause or guessed frame.
The original animation still drives internal mechanical frames; the existing
restorable rigid map then places the weapon and independent hands at tracked
controllers. Closing status 3/end 22 retains this presentation only while the
same recovery owner remains positively coherent. Mapping failure cancels and
restores; native animation remains the fallback for unsupported contexts.

`cojvrKeepManualReloadAlive()Z` validates the cached weapon, current hand machine,
its exact veto owner, actual/active/desired identities, empty support, desired 21,
local alive/non-carried/non-network owner and current reload phase. It rejects
non-finite time, rollback and expired waiting deadlines before renewal. Only
coherent waiting extends the demonstrated 1.5-native-clock watchdog. Missing
updates do not create an indefinite hold; paused native time resumes on unpause.

The free-hand **trigger** picks up the existing compositor token at waist. Its
current visual frame follows the verified displayed skin-pad midpoint and distal
finger tangent described below; gesture admission still follows tracked input.
A valid approach uses the existing 14 cm grip-distance
zone, and a fresh release inside it produces one immediate insertion intent.
`cojvrInsertManualRound()Z` repeats the native validity checks, requires status 2
and calls the shipped `PawnArmed.WeaponReload(LWeapon;)Z` **exactly once**. The
native method alone resolves inventory/capacity, transfers a unit for the admitted
non-whole-clip/single-ammo weapon and returns acceptance. The original
`_WEAPON_RELOADED` event is issued only on acceptance. No ammunition setter,
replicated inventory, deferred insertion credit or native action 31 is used per
insertion. Full/no reserve invokes zero-transfer native closing; failed invocation
or rejection cancels without retry. The visual token disappears on intent;
acceptance permits a fresh token/trigger release. A missed release transfers
nothing and leaves the token available for a new deliberate attempt.
Continuation is scoped to that manual session: direct native completion to
status 0/4 without an observed closing frame, failed preparation and fresh
preparation clear a pending token. Host regression fixtures demonstrate that
the former direct-completion path could leak a continuation into a later
opening; the corrected later session requires a waist pickup of its own.
This is presentation/admission state, not a native ammunition refund or grant.
The synchronous native result now resolves policy/output together before frame
publication. A rejected call reports `native_insertion_rejected`; invocation
failure reports `native_insertion_failed`. Both stop requested VR presentation
and supervision, close without retry and retain only the original intent in
diagnostics. Pending-operation cancellation also preserves its reason after
clearing the flag; recenter, sample gap and regression have distinct reasons.
These corrections have host coverage, with live/physical recovery still pending.

Policy phases are `READY -> OPENING -> MANUAL_LOAD -> CLOSING -> READY`, owned
by the exact adapter. Native states, ammunition and mechanical animation remain
game authority; policy contains only presentation/admission identities and
release barriers. Square again or armed trigger closes; the closing trigger
press cannot shoot. Invalid context/player/weapon/input generation, UI/equipment
or save/load intents, recenter, tracking/input loss, stale samples, disabled
control and failed presentation cancel. Held physical controls remain consumed
until available release. Rejected Square admission cannot fall through to a full
native reload; native fallback requires a positively observed unsupported
context. Native closing restores parent hands and child weapon first; failed
restoration retains captures and invokes no closing callback. Closing policy
never renews the watchdog, including if the native cancellation call fails.
Failed JNI cancellation retains player ownership for a retry; native
watchdog and save/load prefixes supply additional cleanup, not physical proof.

Event diagnostics are disableable and filtered by state/gesture/zone/ammunition
changes rather than continuous clocks. Native insertion records validity and
before/after readbacks, no retry; mapped presentation records weapon and both
hand ownership separately from the existing per-eye draw guards. Geometry
observations include a host-tested optional `pre_native` snapshot of natural
root (element zero), the exact weapon's observed barrel element, and named
gate/drum from the same captured weapon before scoped overlay writes.
`post_overlay` uses JNI readback to check mapped root/barrel and named elements
against the tracked overlay targets. Both sources are observational, require
current weapon identity and rigid frames, and retain their provenance in the
event. An unavailable snapshot cannot establish mechanical motion. Same-frame
native reload status must be correlated before treating a `pre_native` sample
as eligible evidence of native mechanics; `post_overlay` and legacy events do
not qualify as native-motion evidence. No independent gate writer is enabled.

`tools/analyze_coj_manual_reload.py` reads collected evidence after `finish` and
writes a new JSON report outside that directory. Its host-tested correlation
requires valid native traces with the same player/context/weapon/hand before and
after insertion, increasing frame order, matching insertion/readback counts and
a completed native call. The preceding trace must observe manual waiting; the
following trace must still observe the manual wait or closing. Native closing
can follow the last accepted round synchronously. Disabled, unobserved, opening
or cleared follow-up states leave the insertion inconclusive, even with matching
counts and owner. Duplicate insertion frames remain inconclusive. An
uncorrelated accepted event cannot explain a transfer during waiting or closing. Loaded
count +1/reserve -1 is a unit readback observation, not physical acceptance.
Gate/drum distance and relative axis cosines remove global rigid motion from
mechanical comparisons without claiming a fixed open pose or independent writer.
The analyzer verifies inventory, build hash/identity, exact deployment coverage
and runtime marker order; it never writes control files or invokes native code.
Missing samples and incomplete runs retain their limitations.

Host tests execute emitted helpers for watchdog, single/multiple transfers,
full/no reserve, invalid/replaced/carried/dead/network owners, clock/veto faults
and zero-transfer cancellation. They resolve symbols against the installed
hierarchy and retain native save/load prefix order. Independent JNI fixtures
cover missing slots, exceptions, cleanup and cancel-before-player-release.
Pure policy tests cover transitions, gesture drop/retry, cancel/reopen,
unavailable/stale poses/input, held-button barriers and fail-closed Square.
These are stand-ins for native scheduling/ammunition and do not constitute
Chrome JVM, live animation, engine save roundtrip or physical validation.

Complete inventoried right-hand Peacemaker captures establish persistent
waiting beyond the original watchdog interval, correlated native unit transfers,
Square/armed-trigger/full-gun closing and coherent scoped presentation/restoration.
Operator acceptance includes independent hands, successive loading,
insertion-only haptics and exercised weapon/menu recovery. The analyzer accepts
runtime `(x,y,z)` vectors and fixture vectors, rejecting malformed/non-finite
data; reviewed captures have no unexplained transfer. None of this establishes
every interruption, mirrored/Frontier use or prolonged comfort.

Gate-to-drum metrics alone conflate both parts' motion and cannot isolate gate
opening. Source-qualified root/barrel `pre_native` observations are required;
`post_overlay` verifies mapping, not native mechanical advance. Native mechanics
remain the only writer until visible clearance and safe ownership are qualified.

The read-only diagnostic supplies a **bounded natural `READY` reference** before
the scoped weapon/hand overlay, with at most three geometry attempts and one
emitted reference per eligible player-generation/native-context/weapon/hand
identity. It requires manual policy `READY`, native observation and probe validity,
non-reloading native status 0 or 4, a recognized current weapon, matching armed
hand and an allowed gameplay context. A fresh native readback must still match
after the geometry snapshot. The event carries `reference_pose=1`,
`manual_session=0`, `source=pre_native` and native identity/status;
`hand_owner=pre_overlay_unverified` does not assert that the later VR hand write
succeeded. Phase change or disabled diagnostics clears the in-process attempt
cache. This observer performs **no game mutation**.

The offline analyzer accepts only rigid eligible reference geometry, keyed by
player generation, native context, weapon ID, armed hand and weapon model. It
compares that baseline with later same-owner, same-model, coherent same-frame
`pre_native` reload observations; `post_overlay`, a different hand/weapon/model
or uncorrelated traces cannot supply a comparison. Its `*_change_from_ready`
metrics include `gate_root_up_cosine_change_from_ready`; every row retains
`gate_open_confirmed=false`. Analyzer fixtures cover valid/rejected references
and owner mismatches. The observer/consumer are host-tested and have bounded
live same-owner
Peacemaker reference/comparison evidence. Natural `READY` gate-to-root up cosine
is approximately 1.000 versus 0.642788 while waiting, with a relative position
change around (-0.107, -0.145, -0.298) cm. Independent drum motion is observed.
The video does not establish visible port clearance. Keep
`gate_open_confirmed=false` and native mechanical ownership.

The offline consumer now also projects the complete gate/drum up and forward
axes into each weapon-root/barrel reference frame, orthonormalizes the sampled
bases, and reports a 3D rotation angle plus local translation relative to an
eligible same-owner natural `READY` pose. Synthetic host tests distinguish a
global rigid weapon motion from independent component rotations. Derived
same-owner live vectors measure approximately 50 degrees of gate
rotation relative to root/barrel and 0.346–0.356 cm local translation; the drum
rotates roughly 30–150 degrees with under 0.007 cm relative displacement.
These are offline calculations from bounded live telemetry, not proof of a hinge
axis, visible clearance or a safe writer. Per-sample data stays local.

Unresolved: port clearance, convincing cartridge contact/axis/occlusion,
tracking/save/load/death/disable and held-input recovery, mirrored/Frontier use,
prolonged comfort and insertion sound. Mesh elements and offline ANM sampling
are useful observations; no `PauseAnim`/frame setter, new IK, connected-arm
change, physics simulation or proprietary asset is added. Retain the textured
CC0 visual and accepted flow while qualifying these contracts through
[Validation](../VALIDATION.md#persistent-manual-reload-candidate).

### Optional acceptance haptic — host-tested with scoped headset acceptance

The exact-game adapter qualifies feedback only after a completed, accepted
`TryInsertManualRound` call, using synchronous native observations from the same
player: matching nonzero weapon ID and armed hand, valid manual waiting before
the call and waiting/closing afterward, and exactly loaded +1/reserve -1. This
observation does not authorize a transfer, retain ammunition or retry the call.
Inconclusive readbacks produce no acceptance feedback. A failed scoped weapon/
hand restoration or lost presentation suppresses the captured event.

Only bounded event/hand/capture/input-epoch values cross to the presenter. The
existing OpenVR output can emit an 18 ms, amplitude 0.25, 120 Hz pulse in the free
hand after accepting the corresponding fresh stereo upload. Current focus,
tracking and native/input/resource epochs gate delivery; silent baselines,
100 ms maximum age and 80 ms cadence prevent old or duplicate pulses. Output
failure stays optional and cannot change native loading. Event diagnostics are
controlled by `reloadTraceEnabled`. Pure host tests cover unit readback rejection,
hand mapping, duplicate/stale captures, context/resource recovery and output
failure. A later correlated capture records successful free-hand output submissions,
and the operator confirms insertion-only haptics in the exercised manual context.
This establishes scoped delivery acceptance; broader recovery and prolonged comfort
remain pending. [Validation](../VALIDATION.md#persistent-manual-reload-candidate)
owns the current acceptance state, including weapon/menu recovery.
An independent opaque ownership epoch is authored in the game adapter from
latest valid player generation, native context, weapon ID and armed hand.
Replacement, missing observation, cancellation and failed presentation invalidate
queued events even when gameplay availability stays unchanged; the presenter
rechecks the published atomic epoch before output. Tests cover native replacement
without a controller intent and same-owner reacquisition without epoch reuse.
This guard follows sampled ownership; it cannot detect an unsampled native change.
The later host-tested zone cue is described below. No insertion-specific sound
call or independent mechanical writer is admitted.

### Installed ammunition resources — pickup meshes, not loose cartridges

Read-only inspection of the installed `Data0.pak` identifies
`Data/Objects/ammoshort.msh`, `ammolong.msh` and `ammogauge.msh` as ammunition
**pickup objects**: their respective `.def` files declare `JavaClass("AmmoPistol")`,
`JavaClass("AmmoRifle")` and `JavaClass("AmmoShotgun")`. Each mesh is 2,160 bytes,
has one named AmmoShort/AmmoLong/AmmoGauge geometry node with 24 vertices and 36
indices (12 triangles), and references a corresponding material; the pistol pickup
material declares `SRF_WOOD`. These are not evidence of an individual round of
revolver ammunition suitable for a free-hand VR cartridge. The installation also
contains `Data/Player/Equipment/Ammo/ShortAmmo.dds` and other ammunition textures,
but a texture alone is neither a standalone mesh nor a proven runtime attachment.

The revolver meshes expose the mechanical elements above, with no demonstrated
individual cartridge node. The first CC0 derivative was a **16-surface visual
proxy**, not an imported mesh. Its physical presentation was rejected: the operator
observed no convincing asset change/contact and a loading pinch on opening.
Correlated runtime events report the requested estimated overlay as unavailable.
The ordinary empty-hand gate rejects the reload's temporary two-hand animation
occupancy, even though the existing reload-recovery reader already distinguishes
it from an actual carried object.

The ownership correction has **host tests and bounded live delivery evidence**:
subsequent capture events report the estimated pinch applied and the full .44
geometry captured, while the operator still rejects the cartridge's appearance.
These events establish neither pixel quality nor finger-pad contact. A
bridge-owned manual opening/wait admits support fingers only after
matching native recovery and probe observations for the same weapon/armed hand.
Failed observation, foreign equipment, carrying, death and armed-hand requests
remain rejected. The pinch requires a held physical trigger claim; waiting with
no claimed round uses live curls or estimated authored rest. A replenished-ready
token alone does not request the pinch or a visible manual cartridge.

The manual visual centre now follows the midpoint of two **skin-derived offsets
on the verified displayed distal thumb/index bones** after the scoped finger write.
The offsets were estimated offline from the exact-game Ray skin at authored
thumb/index curl .7/.7 (roughly 1.1 cm between estimated pads for a 1.2 cm case).
Native cm are projected through the current camera basis into XR metres.
Invalid owner/bone frames, a pad gap outside 0.6–1.8 cm and failed restoration
hide the round. This is a **host-tested** contact estimate with **bounded live
delivery evidence**. The accompanying close-up does not establish convincing
finger contact. The accepted free-hand trigger pickup/release insertion gesture and
native ammo are unchanged.

The source reference is Pichuliru's [CC0 Flat Ammunition](https://opengameart.org/content/cc0-flat-ammunition)
(2022), `OBJ/Loose Ammo/44 Magnum.obj`: the inspected 122 vertices / 224
triangles have bounds X,Z +/-0.00653 m and Y -0.010396 to +0.030494 m.
The prior manual visual retained all **122 source positions / 224 triangles**,
with a right-handed OBJ-axis conversion and the same brass/copper/primer swatches
as the proxy. The source changed, but the material presentation remained flat;
the full-mesh candidate was also physically rejected for appearance.
The checked-in CC0 OBJ and generated runtime arrays are bound by source SHA-256
and a deterministic bake check. Attribution/license and regeneration instructions
are in [assets/models](../../assets/models/README.md). The old six-sided proxy
remains available for the separate legacy route and its regression fixtures.
The compositor batches bounded geometry into one upload/draw per eye, with
back-face rejection and far-to-near face ordering. Host WARP checks produce
binocular front/side/rear pixels and suppress malformed optional geometry without
losing the rest of the HUD. It does not read game scene depth, create an engine
ammunition object or establish visually correct contact. The source nose centre
retains the exported local Z -0.0475 m offset; the manual attachment translates
the complete visual around the displayed skin-derived midpoint. No proprietary game
asset is embedded.

The current correction uses [Revolver Game Asset](https://opengameart.org/content/revolver-game-asset)
by loafbrr_1 (CC0, 2021): only the loose `357Bullet` node from
`RevolverAmmo.gltf`, 83 positions / 92 triangles. A SHA-256-pinned offline bake
retains source normals and UVs with a cropped 128x160 albedo. Other scene nodes,
rigs and original placements do not enter the runtime. The renderer adapts the
orange case palette to brass and adds authored head-space preview light/reflection;
these are not native environment light or complete PBR. Source normal/roughness
maps are not used. There is no runtime glTF parsing or asset-file dependency.

This material correction has **scoped operator acceptance for visibility**;
the operator explicitly reports that the round still does not look held between
the fingers. Correlated live events identify the textured mesh and native unit
readbacks. WARP front/side/rear draws produce binocular pixels, distinguish
lead/brass colors and reject corrupted optional UVs/normals in both eyes. Optional
reference-image output uses the actual compositor; it is not a headset capture.
The old proxy retains its separate raster/cache; the accepted trigger/release
insertion and native ammunition are unchanged. The skin-derived anchor has host tests and bounded live delivery evidence;
visual grip remains unaccepted.
Neither imported model
is a demonstrated historical/calibre match. Scene-depth occlusion, native lighting,
visual finger fit, orientation/scale, appearance quality and comfort remain open.

### Finger-contact boundary — visibility accepted, grip unresolved

The current estimated pinch interpolates the shared authored rest/fist endpoints
at thumb/index .7/.7; it is not the original loading finger pose. The last
live-observed attachment used bone-local skin-derived reference points and
retained controller grip orientation. The current host-tested candidate replaces
that orientation with the displayed contact tangent in the next section; its
visual fit has no fresh live evidence. Offline vertex measurements are not
runtime proof of skin contact. The compositor draws the cartridge after the
captured scene without scene depth: native fingers cannot correctly occlude it, even with a better
geometric attachment. A complete grip correction therefore needs qualified finger
pose/contact/axis and a demonstrated hand-occlusion or scene-depth boundary.
The operator's visibility acceptance does not accept this grip.

A read-only offline replay using the recognized Ray mesh, existing skin weights,
authored rest/fist endpoints and the audited native ANM sampler shows that the
native Peacemaker loading finger pose differs from this estimate. This is a
candidate source of finger motion, not proof of a stable runtime loading pose or
successful cartridge contact. Temporary meshes, sampled poses and replay images
stay local under ignored `work/`; no proprietary asset is distributed.

The default-off `manual_reload_finger_reference` event is **host-tested and
bounded live-tested**. Reviewed native/displayed snapshots accompany
applied/visible and idle/hidden transitions and coherent native unit insertions.
The supplied close-up still appears superimposed rather than demonstrating
finger-pad contact.
At a newly visible physical pickup, it snapshots the
pre-overlay native and verified displayed distal thumb/index frames, each
expressed in its own Hand-local centimetre basis. It includes weapon/player,
hand and frame identity, positions and up/forward axes. The observed diagnostic
`weapon_id` is zero and cannot independently establish weapon ownership.
Capture precedes scoped
restoration, emission uses value-only data after successful presentation recovery,
and a mismatched capture sequence produces `unavailable`. It neither writes a
pose nor measures pads. Tests cover a rotated/translated hand map, separate
native/displayed references, changed frame rejection and post-restore rejection.
Keep the current accepted reload flow and visible mesh while qualifying native
finger-pose reuse plus a measured contact/axis reference; there is no new IK,
animation-time setter, ammunition state or independent mechanical writer.

### Displayed contact tangent and depth inspection — host-tested only

The revised visual frame retains the verified skin-pad midpoint and constructs
up from thumb toward index. Native distal +X axes point along each digit;
their mean projected perpendicular to the pad gap supplies projectile direction.
Mesh -Z follows it. The offline Ray replay at .7/.7 yields hand-local directions
approximately left `(-.05292,.99517,.08269)` and right
`(.09508,-.95800,.27053)`. These are derived pose measurements, not socket axes
or a new native animation. A degenerate mean/gap, changed live bone or failed
scoped restoration hides the visual. Native centimetres and orientation convert
through the current exact camera basis; the mesh centre meets the midpoint.
Tests cover both hands, camera rotation, invalid basis and post-restore rejection.
Runtime visibility/contact/comfort for this axis remain unobserved.
The revised finger-reference emitter also takes weapon/armed-hand/context from
the current native reload snapshot, not the ordinary idle admission reader.
This removes the source of the historical zero ID while waiting. Snapshot
validity is explicit; the changed emitter has no fresh live correlation yet.

The renderer's read-only `manual_reload_depth` observer records target/depth
surface identities/descriptions, viewport/depth states and available fixed-
function view/projection at full eye completion. It samples at most six times
per manual opening around held appearances. Real host D3D9 tests establish
non-mutating observation and resource release before reset. This does not prove
Chrome's eye-depth lifetime, hand contents, shader matrices, sampleable format
or cross-API transport. `occlusion_admission=false` remains mandatory; no new
draw/depth-copy path is enabled.

Qualified held entry into the existing grip zone now supplies a separate 9 ms,
amplitude .10, 90 Hz cue. Initial pickup uses the insertion journey/time guards;
replenished cartridges require a fresh press. Withdrawal beyond 18 cm rearms
the 14 cm entry, with a separate 180 ms output cadence. The native-confirmed
18 ms/.25/120 Hz insertion pulse retains its own 80 ms cadence and priority.
Freshness/ownership/restoration fences apply to both. Host tests cover boundary
jitter, immediate acceptance after entry and reacquisition. No ammunition is
granted on entry and physical delivery of the new cue remains pending.

Square's released baseline is now tied to usable gameplay and observed player,
context/input generation and weapon. Lost availability or ownership invalidates
it. Fresh physical release still discharges an old claim after disabling manual
reload so the conventional path can recover. Host tests reproduce held recovery
boundaries; live/menu/disable coverage for this strengthening is pending.
