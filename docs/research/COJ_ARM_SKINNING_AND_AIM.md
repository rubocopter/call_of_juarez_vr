# Call of Juarez arm skinning and weapon ownership research

This document keeps the exact-game conclusions that still govern Body IK and aiming. Superseded writer experiments and raw video/log chronology were removed after their conclusions were captured.

## Measured arm contract

The active physical candidate is a hybrid presentation: retained native torso/legs
with independently tracked native hands. The connected arm solver below remains
an exact-game diagnostic contract. Completing span calibration and correcting the
initial post-swing wrist reference did not produce acceptable anatomy; the operator
rejects the resulting long limbs near the chest. Further enlargement is not justified.

## Exact player geometry partition

The Ray/Billy player `MSH` assets contain a single skinned body node, so hiding
`RayBody`/`BillyBody` also removes hands and torso. A separate `CoJVRHiddenArms`
node is appended without relocating existing chunks or changing original element
indices. Triangles touching a vertex with upper-arm/forearm/twist influence but
no hand/finger influence move to that node. Mixed hand/forearm wrist vertices stay
with the hand. Complementary degenerate triangles preserve original index and
draw counts; positions, normals, UVs, weights and bone palettes stay unchanged.
The archive and both mesh entries are SHA-256 pinned; unknown inputs fail closed.
Only player mesh entries change, leaving NPCs and unrelated compressed records
untouched. Native loading and the separated hand presentation are now live-tested
for the observed player model;
the operator reports substantial improvement while still rejecting grip cohesion
and wrist orientation.

For each hand, the rigid overlay includes `Forearm`, `ForeTwist`, `ForeTwist1`,
`Hand`, all fifteen `Finger*` elements and the authored `left_hand`/`right_hand`
holding socket: twenty frames per hand. Moving all retained skin contributors
together avoids applying an isolated wrist twist to a connected sleeve. Armed
hands use the measured weapon rigid map, preserving native finger animation.
The separate arm node is hidden only during hand ownership and restores to its
previous visibility after both eyes; reload yields to native animation. Element
and visibility readback, failed-write rollback, retry retention and actor-relative
restore are host-tested. Mesh appearance, full wrist comfort and actual shots
remain physical gates.

## Native holding socket and child weapon

The exact `InvObject.GetOwnerElementID` chooses `Left_hand`/`Right_hand`, with
`Bip01 L/R Hand` as fallback. The Ray/Billy meshes contain the lowercase holding
socket as a child of the corresponding hand bone. `InvObjectInLeftHand`/
`InvObjectInRightHand` resolve that owner element; `ShowInvObject` inverts the
weapon's `HolderL`/`HolderR` (with native holder fallbacks) and calls
`owner.AttachChild(weapon, ownerSocket)`. The holding socket and wrist bone are
therefore separate exact-game ownership boundaries.

Live barrel observations disprove the previous order: a weapon verified before
the hand overlay changes position and direction after the parent hand is moved.
The current transaction captures the weapon before any parent writes, maps the
hand and socket rigidly, then applies the child weapon and verifies its muzzle
after both eye draws. Restore moves the parent hand/socket back first and then
restores the captured child, retaining both owners if parent restoration fails.
The tracked grip anchors the native holding socket rather than substituting the
wrist pivot. For an unarmed hand, the authored socket maps to absolute controller
grip orientation; it does not retain a fit to the first animated/controller pose.
Socket mapping, rollback and both-eye verifier rejection are host-tested.
Physical grip, left wrist comfort and actual-shot alignment remain pending.

## Shot visualization and emitter ownership

The holding-socket candidate now has live matching hand and weapon readbacks
through both views with successful pose/visibility restoration. The next
operator clip still rejects bullet/effect origin at the visible gun; successful
cache getters during rendering do not establish actual attack consumption.

`WeaponFire.AttackFire` separately reads ballistic origin/direction and visual
origin/direction, passes the visual origin to the shot light, and applies native
pellet spread to both rays. `GetFireDirVisualizationForHand` previously sourced
`m_avAimDir`, bypassing the tracked `GetBeingLookDirDevForHand` path. The current
patch substitutes only that base-vector selection; the original accuracy and
rotation suffix remains byte-for-byte intact.

`WeaponFire.ExecFXFire` originally attaches combustion and smoke to the
weapon/barrel element. A render-only weapon restores before native update, so
that attachment cannot own a persistent VR effect. The exact-class patch keeps
emitters detached, with null parent and element -1, and uses the verified muzzle.

The native `FXCreateParticleEmiter` handler (`ChromeEngine3 + 0x1102D0`) passes
its two world vectors through `+0x10EEC0` to `+0x10E850`. The second vector fills
the emitter's **+Y/up** row; it is not a barrel-forward vector. Shipped barrel
combustion/smoke definitions emit along **local -X**, agreeing with
`Weapon.GetBarrelDir = -GetElementLeftVector`. Supplying the shot direction as
emitter up loses the authored barrel axes and visibly misdirects the effect.

The current patch creates the same configured emitters with no direction
argument, then calls the shipped `FXSetStartXForm(handle, position, up, forward)`
(`+0x10F250`). That method stores up and derives +X from up cross forward, but
only writes the starting matrix at emitter offset +0x18. It does not commit
the active world transform or previous position. The successful null-parent
creation path also bypasses the attached emitter's link/update boundary.

The patch therefore follows the starting-frame write with shipped
`FXDetach(I)Z` (`+0x10EFF0`). Its valid-handle path calls `+0x10E420`, which
clears parent ownership, recomputes the active world transform through
`+0x10E0A0` and copies current position +0xE0 to previous position +0xF0.
That commit also applies to an already detached emitter. A failed frame write
or detach/commit deletes the newly owned nonzero handle once; no failed handle
is published as committed. Host bytecode tests model starting and active
transforms separately, including creation, frame and commit failures.

The bridge publishes measured barrel up/forward beside the shot origin/direction,
checks the complete barrel basis through both eyes, and clears all four vectors
per hand together on failed publication. No guessed muzzle translation or global
particle-definition change is involved. A failed full-frame effect write deletes
that emitter; a zero creation handle is never mutated. Other players, missing
caches, unknown hands and forced-network attacks retain the original method.
Fire actions 9/10 are delivered only after current muzzle publication; non-fire
actions and their native analog/snap/reload semantics keep their earlier order.
Bytecode execution, unchanged spread suffix, shipped Java verification and
deployment restoration are host gates; effect appearance and shot alignment
remain physical gates.

Exact `WeaponFire.AttackFire` and `OnHit` prefixes count local tracked consumer
entries without replaying either method or querying spread. The verified
`ExecFXFire` path additionally records configured, created, committed and failed
emitter masks. Periodic JNI observation reads five counters/status fields and
the last successfully committed comb/smoke handles as one complete observation
from each active weapon and clears partial reads on failure. Unavailable reads
are reported at the same bounded observation cadence. Counters belong to weapon
instances, can change on equipment replacement and can appear on both hands for
a shared two-hand weapon. `OnHit` entries are not one-to-one shot counts; FX masks
describe the latest verified FX call and can persist across a suppressed shot.
These counters
distinguish native consumption from input dispatch; they do not establish
visible muzzle effects, wall impacts or shot alignment.

The factory inserts detached emitters into the manager's update/draw membership;
null parent alone does not demonstrate a missing update. Native global FX enable
gates both paths. Particle draw chooses renderer camera +0x1B0, falling back to
+0x1B8, and can reject empty particle sets or their bounds. The full render-view
wrapper includes the FX stage and queue flush. Creation/commit masks cannot
establish first update, emission, lifetime, culling or actual pixels.

The exact-build observational probe copies and rechecks the global enable word
and particle camera before/after both eyes. Live samples show FX enabled and the
camera matching each eye. It does not force flags or call update/draw.
Emitter handles are manager-local. A fresh `Weapon.GetThisID` follows the
demonstrated handle+4 -> binding+4 -> GameObject chain. Native `FXIsEnabled`
at `0x10F120` then follows owner+0x24 -> module+0x370 -> manager. Its slot table
is manager+0x18, capacity+0x20 (grow `0xFCA80`), with 16-byte slots and a high-16
generation match. Active count+0x1C is not a capacity bound.
The read-only sampler rechecks that chain, slot and subtype. Only simple
vtable `0x2FDE7C` and rotated `0x2FDEB8` expose live/emitted/expired counters
at +0x40C/+0x410/+0x414. Their native update resets emitted/expired each tick.
Emitter+4 is its manager; clock manager+4, birth emitter+0xC, lifetime+0x10
and scale+0x14 govern expiry before update. Active/previous positions are
emitter+0xE0/+0xF0; enable/render bytes are +0x5C/+0x5D.
The bounded reader is host-tested; first update, destruction/culling and actual
pixels remain unobserved. Replacement and unreadable/unknown types produce
unknown observations, not zero particles. Last committed handles are cleared
at each tracked FX entry before creation and alone do not establish liveness.

## Legacy measured arm reach

Live evidence measures the native upper-arm + forearm chain at about **49.843 game units**. Asymmetric reach clamp did not justify an arbitrary fixed multiplier. The legacy render-only fit measures bilateral controller span in a stable T pose; fitted visual proportions were physically rejected.

The demonstrated indices are pelvis 0, spine 1, spine1 2, chest/spine2 3, neck 4, head 5, left arm 7/8/9/10 and right arm 12/13/14/15. Indices 6 and 11 remain unproven gaps; current evidence does not justify naming them clavicles/shoulders or mutating them. No global upper/forearm scaling is accepted. Shoulder/clavicle participation remains a future controlled experiment only after the exact elements and parentage are demonstrated.

## Effective hierarchy

`EBones` ordering is semantic numbering, not proof of parentage.

Replay of live natural/write probes established the effective visible propagation for the observed exact model:

- forearm inherits upper-arm motion;
- FORETWIST inherits upper-arm motion but not forearm swing;
- hand follows forearm;
- hand does not inherit FORETWIST roll.

The practical model is therefore FORETWIST as a sibling of forearm beneath upper, with the hand on the forearm branch. This is model/build evidence, not a reusable Chrome Engine skeleton rule.

## Visible writer

Earlier absolute-orientation and `BoneRotate` approaches did not produce reliable visible-mesh ownership. Exact-build `RotateElementWithChildren(ILVector;F)V` was then shown physically to change the rendered arm.

Disassembly established that the helper post-multiplies the current element matrix and consumes an **element-local axis**. Supplying a solver world axis directly caused incorrect orientation/deformation.
Further native inspection established a second property that matters to the
locomotion regression: after each rotation, the helper calls engine routine
`ChromeEngine3 + 0x82F00`, which recursively walks the owned scene/object child
hierarchy and invokes virtual notifications. Applying and then undoing several
arm rotations every frame therefore crosses a substantially broader engine
boundary than the geometry writer itself.

`FromUpForwardPosElementWorld` (`ChromeEngine3 + 0x9A350`) writes the requested
absolute world frame and metadata directly and does not call that recursive
`+0x82F00` path. The current host candidate therefore:

1. reads and retains the natural complete element frames;
2. solves shoulder/elbow/wrist targets from measured geometry;
3. computes complete absolute world frames for upper arm, forearm, FORETWIST and hand, including the bounded hand residual;
4. writes each final frame explicitly through `FromUpForwardPosElementWorld`;
5. verifies positional/orientation agreement;
6. restores the natural orientation bases after stereo capture while rebasing captured positions by any actor translation, then verifies the rebased geometry.

Partial apply failure rolls back through the same exact-frame restore path. A
failed verified restore disables further writes. The previous hierarchical
writer remains historically live-exercised. The absolute writer is now physically
exercised and produced a large recovery in locomotion/performance, supporting the
recursive-notification interference diagnosis.

That physical run exposed a separate restore-ownership defect. The actor could
translate between capture and restore while the stored natural arm frames retained
their old world positions; one observed shoulder stayed at its captured coordinates
while the actor moved, matching the stretched-arm/hand-left-behind result. Native
reload subsequently re-synchronized the branch. Storing actor position at capture
and rebasing the restored shoulder/elbow/wrist and element positions reduced the
visible failure in the next run, but did not remove it. The follow-up telemetry
then isolated an inter-frame case: the complete arm positional chain could remain
byte-for-byte effectively fixed across repeated samples while the actor continued
to translate, with actor-to-upper-arm distance growing from the normal roughly
150 game units to several hundred units before native animation snapped the branch
back into sync.

The current host candidate retains actor-relative restore and also compares each
fresh natural arm sample with the previous corrected/restored natural sample for
the same actor generation and recenter sequence. If the actor moved materially while shoulder/elbow/wrist
and all four sampled element positions stayed within float-scale world-position
tolerance, only those positions are translated by the actor delta before the next
IK solve; the current orientation bases are preserved. Any positional animation
change leaves the natural sample untouched. A per-arm cumulative rebase count is
sampled in tracking telemetry. This combined continuity contract is
**physically exercised with improved continuity**, but occasional recovery delay
and limb torsion still reject anatomy. Comparing against an unapplied raw sample
would not describe the actual next-frame state after the corrected frame is restored.

The absolute writer also contained an orientation composition mismatch: the
forearm used the direct positional-plan basis, while FORETWIST and hand used
upper-arm swing followed by elbow swing. Those rotations can reach identical
endpoints with different axial roll. The corrected forearm uses the same composed
rotations as the lower skinning elements, preserving native bind offsets. A host
regression demonstrates the disagreement and correction without changing bone
lengths or the controller-roll/residual limits.

## Target space and body yaw

Grip and tip targets now map their complete recentered positions around the
untracked natural camera plus physical-height compensation. The previous arm
anchor used the animated head while tip/view used the camera, allowing a model
animation offset to displace the tracked wrist. Horizontal pelvis translation
already appears in the observed shoulders; it is not added to targets again.
The generic head-relative mapping remains available for other consumers.

The elbow pole uses the head-to-shoulder offset rather than the current animated
elbow. A stable bilateral T pose measures controller span, subtracts measured
native shoulder width and fits upper/forearm lengths in their native ratio for
the render transaction. Ordinary aiming cannot calibrate this fit. All captured
native frames are restored; this is a measured user fit, not arbitrary global
bone scaling. Fitted FORETWIST and hand frames retain joint-relative offsets.
These anatomy changes are host-tested and await physical acceptance.

Physical follow-up disproves the original fixed recenter-X/Z T-pose criterion:
the operator extended both arms, but visor offset from the shoulder plane and
turning caused rejection, leaving the native chain clamped near the torso while
the tracked weapon stayed near the real hand. Extension is now measured in the
horizontal hand-to-hand frame, with the head between both hands, bounded vertical
and shoulder-plane offsets, the same measured-span bounds and stable-sample
requirement. Pending calibration telemetry retains the rejection reason and
sample count. Observed-pose and yaw-rotated host regressions pass. Physical
follow-up now completes calibration and confirms longer arms, while still
rejecting wrist/forearm anatomy. Successful solver-target readback coexists with
large hand-orientation residuals that the safety boundary does not apply. This
separates span detection from the remaining orientation/skinning problem; it
does not justify increasing reach again or lifting the residual limit without
demonstrating correct ownership.

The initial orientation reference previously paired a controller pose with the
native hand basis before upper/elbow IK. If controller position differed from
the native idle hand, positional swing alone could exceed the wrist residual
limit while controller orientation remained unchanged. The corrected reference
uses the composed post-IK hand basis. Recenter preserves the last read-back
displayed hand basis rather than a requested target that the residual gate
rejected. This is host-tested; visual wrist/forearm acceptance remains pending.

Another physical rejection showed that applying actor-owned HMD yaw again to the controller/body reference rotates arms a second time. Current host source removes actor-owned yaw from that mapping. Run `20260920T235112Z-6b2d91cda4a5` then showed that leaving room-scale translation camera-only exposes the stationary local avatar during a physical step or crouch. The next candidate moved the local pelvis with full XYZ HMD translation, but non-promotable run `20260921T163309Z-481defca3401` measured up to `12.892973` game units of vertical pelvis offset while the body still entered the headset view. Current host source therefore applies only mapped horizontal room-scale translation to the local pelvis/skeleton for both eye renders and restores it afterwards, while vertical actor position, grounding and collision stay game-owned.

The render-only pelvis translation uses the same horizontal sign convention as
tracked skeleton targets. The next physical pass still rejected inverted
forward/backward movement and exposed the whole avatar when leaning. The remaining
disagreement was in the camera: its position helper reflected tracking Z, and its
translation reference retained actor-owned yaw while skeleton targets removed it.
The camera follow-up now shares the skeleton/reticle convention and tracking
reference. This is host-tested, not physically accepted.

Pelvis application precedes arm reads/solving; arm restoration precedes pelvis
restoration. Earlier eye probes showed the pelvis moving already solved endpoints
again. Continuity excludes this temporary offset. This nesting is live-exercised
with further reported improvement, while residual anatomy remains rejected.

Native crouch lowers the camera independently of physical HMD descent. Applying
both caused a duplicate view-height drop. An actor-relative native camera-height
reference now compensates that drop for physical crouch and its pose recovery,
with the same correction in arm anchors and controller aim origins. Explicit
controller crouch remains native-owned; generation/recenter resets the reference.
No vertical actor or pelvis translation is introduced. Comfort remains a physical
gate.

## Telemetry cost

Run `20260920T161307Z-474f0b054338` emitted 11,192 successful arm-tracking and 11,192 successful restore records while the user observed jerky movement even with keyboard. Latest complete single-process evidence `20260921T192639Z-1d70905cb4f8` still rejects movement and measures classic-D3D9 CPU copy at 7.112 ms median and 9.056 ms p95, so renderer transport remains a measured performance problem independent of arm logging.

Routine arm telemetry remains sampled; write failures, rollback and restoration faults remain unconditional. Arm solving still has to be evaluated independently from renderer frame pacing.

The later Body-IK regression narrows this further. Direct arm apply/restore cost
was about 0.13 ms in the observed run, sampled body/actor boundaries did not show
Body IK directly writing `PlayerBeing` position, yet actor-position jump apexes
collapsed to only a few centimetres while native vertical speed still rose. That
evidence makes raw solver CPU cost and direct actor-position mutation poor primary
explanations; the recursive notification boundary above is the current static
hypothesis being removed by the absolute-frame candidate.

## Weapon direction and origins

Exact shipped bytecode establishes separate ownership paths:

- per-hand fire direction ultimately reads `m_avLookDirDevForHand[hand]` and native accuracy/spread remains downstream;
- visual origin uses `m_avAimFromPoint[hand]`;
- ordinary local ballistic origin reaches `Being.m_vLookFromPoint`;
- the network-forced branch uses separate network state and remains untouched.

The grip/tip axes now drive a rigid map of all measured weapon element world
frames, using `FromUpForwardPosElementWorld` rather than the general object
setter. `ControlObject.FromUpForwardPos` enters `+0x878F0`, which calls the
rejected recursive `+0x82F00` notification path. Measured wrist-to-muzzle offset
and internal animation geometry are preserved; barrel readback verifies the
result before cache publication. Restore is nested inside the arm/pelvis
transaction, actor-relative and verified. A shared two-hand weapon has one owner.

`InputDigital.Translate` chooses a hand state, while actual attack follows
later through `OnHandStateStarted_Attack -> WeaponAttack -> Weapon.Attack`.
The earlier trigger-edge global-origin publication is removed because native
updates reclaim the field. Nullable per-instance origin/direction vectors added
to the exact class are instead consumed by `GetFireOriginForWeapon`,
`GetFireOriginVisualizationForHand` and `GetBeingLookDirDevForHand`. Null vectors,
unknown hands and network-forced fire execute the unchanged original methods.
Native accuracy/spread remains downstream. Only the bridge-resolved player gets
verified muzzle vectors, cleared at invalid/menu/generation boundaries.

The class and original archive SHA-256 are pinned. A journaled staged archive
retains all other payloads; finish and interruption recovery restore the original
archive byte-for-byte. Host execution covers injected hand/fallback paths, and
the shipped Java 1.4 verifier accepts the patched class. Geometry/cache telemetry
does not prove actual-shot consumption or physical alignment. The current
gameplay reticle uses the measured muzzle ray when verified and stays isolated
from the menu pointer.

## Latest anatomy rejection

The latest operator clip reports further improvement but still rejects residual
anatomy, inverted physical displacement and whole-avatar intrusion on leaning.
Physical crouch comfort remains rejected. Impacts are closer to the visible
reticle, but weapon placement and muzzle origin remain rejected; live barrel
observations confirm a large positional/angular mismatch with the controller ray.
Sampled `Weapon.GetOwnerAttackOrigin/GetOwnerAttackOriginVisualization` now
distinguish ballistic and visual origins from the barrel. Those getters are
read-only; diagnostic calls do not query the accuracy/spread direction path.
The native hand residual limit remains unchanged. A measured bilateral-span
render fit is now implemented; its visual anatomy has not yet been accepted.

An earlier rejected clip showed a different failure mode: safety blocks some extreme writes, but the arms repeatedly return to the native/default game pose as the Sense controllers move. Telemetry does not support solving this by scaling the upper/forearm chain globally:

- retained formal evidence sampled controller targets up to 75.800 units against the ~49.843-unit measured native reach;
- the latest run denied 43/128 sampled arm updates, including 32 reach-unsafe samples, while 80 samples rejected the hand residual;
- elbow and wrist positional targets can still be reached while the resulting orientation is anatomically invalid;
- reload introduces a visible conflict between native animation and VR-driven element rotation.

The previous candidate applied a hand residual only when the original mismatch
was at most 30 degrees and also rejected positional arm writes more than 10%
beyond measured reach. That second rule was redundant with the two-bone solver's
existing hard clamp and caused valid unreachable targets to fall back visibly to
native animation. Current host code keeps ownership for every valid finite
positional IK plan, including a hard-clamped target at the measured native reach.
The conservative upper/forearm rotation plan, 30-degree hand residual, disabled
FORETWIST controller roll and shipped reload ownership remain independent safety
gates. This continuity change is **implemented / host-tested**; physical
anatomy/reach acceptance is still pending. Clavicle/shoulder participation
remains a measured experiment, with hand orientation kept separate from
positional reach.

## Current body acceptance boundary

Do not promote Body IK based on write counts alone. Acceptance requires visually plausible arms across representative poses, stable first-person ownership, verified restoration and no degradation of tracking/recenter behavior. Lower-body writing remains blocked until the upper-body/body-anchor contract is acceptable.
