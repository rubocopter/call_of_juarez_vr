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
the later holding-socket and wrist ownership follow-up has operator acceptance
for exercised grip cohesion, wrist comfort and reload/movement recovery.

For each hand, the rigid overlay includes `Forearm`, `ForeTwist`, `ForeTwist1`,
`Hand`, all fifteen `Finger*` elements and the authored `left_hand`/`right_hand`
holding socket: twenty frames per hand. Moving all retained skin contributors
together avoids applying an isolated wrist twist to a connected sleeve. Armed
hands use the measured weapon rigid map, preserving native finger animation.
The separate arm node is hidden only during hand ownership and restores to its
previous visibility after both eyes; reload yields to native animation. Element
and visibility readback, failed-write rollback, retry retention and actor-relative
restore are host-tested. The exercised independent-hand appearance, wrist/grip
cohesion and side-on/rolled pistol shots have operator acceptance. Broader
equipment/animation and sustained recovery remain separate gates.

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
Exercised independent-hand grip/wrist comfort and side-on/rolled pistol coherence
have operator acceptance. Broader equipment, anatomy and recovery remain separate
gates; this does not accept the legacy connected-arm solver.

## Shot visualization and emitter ownership

Matching hand/weapon readbacks through both views and successful restoration
establish transaction coherence, not actual attack consumption. Exercised pistol
shots, flash/smoke/light, impacts, water and destructible bottles are operator-
accepted. Actual missile birth/first-draw vectors remain unmeasured; a rearward
yellow streak alone is not evidence of an inverted ray.

`WeaponFire.AttackFire` separately reads ballistic origin/direction and visual
origin/direction, passes the visual origin to the shot light, and applies native
pellet spread to both rays. `GetFireDirVisualizationForHand` previously sourced
`m_avAimDir`, bypassing the tracked `GetBeingLookDirDevForHand` path. The current
patch substitutes only that base-vector selection; the original accuracy and
rotation suffix remains byte-for-byte intact.

The local yellow trail has a separate native missile-mesh consumer:
`AttackFire` copies the visual origin and velocity into `Missile`, loads the
configured mesh, then calls `Missile.UpdatePosition`, which sets world position
from `visualizationPosition` and direction from `visualizationVelocity`.
In the pinned original, `ControlObject.SetDir` (`+0x88540 -> +0x87B50`) builds
the world frame through `+0x1F3150`, placing the normalized input in its **+Z**
row. The shipped `BulletTrail_Glow` mesh has identity orientation and packed
positions spanning **-45.6856 to 0 cm along local Z**: an authored rearward tail,
confirmed by an isolated host geometry projection. It is independent of the
combustion/smoke emitter frame. A streak behind the muzzle therefore does not
by itself demonstrate reversed ray direction or a wrong launch origin.

`NewMissile` activates a pooled missile before the caller initializes its shot
vectors; `AttackFire` finishes with `UpdatePosition` before returning. Static
inspection does not establish whether any draw can observe the intermediate
state. Safe diagnosis must correlate the actual attack/hand and missile identity,
post-spread visual origin/velocity, birth time, first update and first rendered
world frame with the same visible muzzle pose. This distinguishes the authored
tail from stale launch data or premature visibility. No direction negation,
origin offset or forced FX update follows from the current evidence; actual-shot
birth/first-draw correlation remains open.

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
`+0x82F00` path. The retained connected-arm diagnostic therefore:

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

Restore captures actor position and rebases all captured element positions by
actor translation. Between frames, a same-owner/generation/recenter natural chain
that stayed positionally fixed while the actor moved is rebased by actor delta;
any positional animation change leaves it untouched. Compare against the previous
corrected/restored state, not an unapplied raw sample. Axes remain natural.
This improves continuity but does not accept connected-arm anatomy.

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
The diagnostic fit has host checks and physical calibration observations;
connected-arm visual anatomy remains rejected.

Span calibration measures extension in the horizontal hand-to-hand frame, with
the head between hands and bounded vertical/shoulder-plane offsets. A fixed
recenter-X/Z criterion rejected real T poses when the user turned. Stable samples
and measured-span bounds remain required. Calibration now succeeds, but longer
arms and reachable endpoints do not establish acceptable wrist/forearm skinning.
This fit belongs only to the connected-arm diagnostic, not independent hands.

The initial orientation reference previously paired a controller pose with the
native hand basis before upper/elbow IK. If controller position differed from
the native idle hand, positional swing alone could exceed the wrist residual
limit while controller orientation remained unchanged. The corrected reference
uses the composed post-IK hand basis. Recenter preserves the last read-back
displayed hand basis rather than a requested target that the residual gate
rejected. This is host-tested; visual wrist/forearm acceptance remains pending.

Actor-owned HMD yaw must be removed once from controller/body references.
The render-only pelvis overlay applies horizontal HMD translation for both eyes
and restores it afterward; vertical actor position/grounding/collision remain
native. Full XYZ pelvis translation and camera-only room-scale motion were
rejected. Camera translation now shares skeleton/reticle tracking-Z and yaw
conventions. The exercised physical direction, leaning/body-visibility and crouch
regressions are operator-accepted, independently of connected-arm anatomy.

Pelvis application precedes arm reads/solving; arm restoration precedes pelvis
restoration. Earlier eye probes showed the pelvis moving already solved endpoints
again. Continuity excludes this temporary offset. This nesting is live-exercised
with further reported improvement, while residual anatomy remains rejected.

Native crouch lowers the camera independently of physical HMD descent. Applying
both caused a duplicate view-height drop. An actor-relative native camera-height
reference now compensates that drop for physical crouch and its pose recovery,
with the same correction in arm anchors and controller aim origins. Explicit
controller crouch remains native-owned; generation/recenter resets the reference.
No vertical actor or pelvis translation is introduced. The exercised on-foot physical-crouch regression has operator acceptance;
mounted behavior and broader recovery remain separate gates.

## Telemetry cost

Routine arm telemetry is sampled; write failures, rollback and restoration faults
remain unconditional. The classic stereo CPU readback/copy was a measured renderer
bottleneck and has been replaced by the shared GPU path. It must not be confused
with arm-solving cost. Recursive arm notification also interfered with native
locomotion despite low measured solver cost; restoration must avoid that boundary.

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

The connected-arm experiment remains physically rejected despite improved
continuity, successful measured-span calibration and corrected post-swing wrist
reference. Near-chest enlarged limbs and unnatural wrists are not fixed by successful
solver readback. Independent native hands supersede this presentation for active
candidates; accepted movement/crouch/weapon regressions must not be described as
still rejected because an earlier arm experiment failed.

The diagnostic solver retains finite hard-clamped positional plans rather than
falling back to idle for every unreachable target. Native bone proportions,
conservative upper/forearm rotation, the 30-degree hand residual, disabled
FORETWIST controller roll and shipped reload ownership remain separate gates.
Indices 6/11 and clavicle/shoulder participation remain unproved. Further global
length enlargement or lifting wrist limits without ownership evidence is unjustified.

## Physical-walk animation ownership

The original `Creature.OnBeingsFrame` updates movement state, its state machine
and the root animation node in that order. `GetAnimSpeedState` normally reads
the real state machine; a render-time `m_iAnimSpeedState` write alone cannot
select walking for a standing actor. Earlier writes can feed native movement
state. The root's timing also supplies body/hand/weapon child nodes, so it is
not an independent legs-only owner.

Native direction/grade selection uses the actual velocity and `GetMoveLimit`.
The movement phase derives from the registered sequence's frame count, authored
distance normalization, mesh time and native animation/slomo policy. The script's
movement `FPS` value is distance in centimetres over a cycle in this path;
it is not an assumed fixed animation frequency. Replaying `UpdateMoveState`,
`UpdateMoveStateMashine` or `UpdateMoveAnimationNodes` advances gameplay/time
state and cannot provide a render-only sample.

The native `UpdateAnimations` route (`+0x97370 -> +0x44230 -> +0x21DFE0`)
enters mutable sequence processing at `+0x21D9A0`. Functional
`SetFrame(IFI)` at `+0x96540` reaches recursive child/notification processing
at `+0x82F00`. Neither is established as an event-free sampler, and restoring
bone matrices cannot restore all sequence, morph, cache and attachment state.
Shipped walking events can update the ground/material cache, play footsteps and
reach `MakeNoise`; a physical-only visual overlay must not duplicate those events.

Physical-walk animation remains planned. Implementation requires demonstrated
natural evaluation versus both-eye render order, an event-free sampler or a
complete reversible animation transaction, isolation of physical-only events,
fresh grounded/pending-state eligibility and both-model clip coverage. HMD
velocity must also distinguish lean/discontinuities from walking and define
coexistence with stick locomotion. These static findings do not validate a
runtime animation writer or physical walking.

## Current body acceptance boundary

The original Ray/Billy weighted finger hierarchy identifies `Finger0` thumb,
`Finger1` index, `Finger2` middle, `Finger3` ring and `Finger4` pinky. Resolve names
per observed owner/generation; their numeric mesh indices differ. Billy's authored
animation script includes the Ray first-person animation set. `Unarmed_Stand`
supplies relaxed rest and `Hands_Stand` supplies an authored fist; neither proves
general anatomical limits or maximum opening.

For ANM1 version `0x10015` type 3, scalar channels are compressed descriptors,
not float pointers. The original evaluator at engine `+0x253190` supplies XYZW
quaternions after the rotation-mode selector at `+0x251E90`; ignoring that mode
can rotate child fingers incorrectly. Isolated execution of the original reader
and independent authored 3DA comparisons establishes the endpoint quaternions.
The actual skeletal consumer's TRS conversion at `+0x219530` calls XYZW converter
`+0x1F8A20`. Isolated conversion and world getter `+0x41E40` establish row-major
axes and `local * parent_world`; the default affine multiply is `+0x1F4C20`.
These exact-game seams are research evidence, not new production hooks/offsets.
The similarly named `ElementSetWorldMatrixFromQuatPos` angle/axis route and
physics WXYZ converter are not equivalent replacements.

The host-tested initial retarget shortest-arc interpolates those authored local
orientations for the five available curls, preserving fresh parent-local
translations, Hand/socket and forearm/twist frames. All fifteen targets enter the
existing twenty-frame transaction before any write; no second writer is added.
Require active/desired weapon null, current/destination/desired hand states zero,
no other-hand desired two-hand weapon, alive player and
`HasSomethingInHand(hand)==false`, which includes carried objects and the other
hand's shared two-hand weapon. `SetDesiredWeapon` can queue an equip while the
current state is still empty: `GetDesiredWeapon`, `GetDesiredWeaponState`,
`GetHandStateMashineDestinyState` and `IsDesiredWeaponOperatedTwoHand` observe
those pending states without advancing them. Reload/context/pose loss retain native ownership.
Invalid curls or a failed optional lookup retain the original rigid map. Native
world-getter endpoint fixtures, independent fingers, transformed roots, length
preservation, rollback and restore/retry are host-tested; visible motion/contact
and equipment transitions remain live/headset gates. Curl zero deliberately
means authored relaxed rest. This does not claim complete finger tracking.

Billy's procedural whip uses a separate tracked holding-socket route. The
bridge maps the independent right hand with its authored socket and publishes
the exact native whip position/up/forward cache; `ComputeWhipPosition` consumes
it only for the owned in-hands object. Naturally scheduled `AdditionalSynchro`
owns the root/first-point placement and all cloth deformation. There is no
gun-barrel prerequisite, whole-rope rigid transformation or forced simulation
step. The cache/hand transaction and controller reticle are host-tested; the
operator accepts exercised whip use and approximately matching reticle alignment.
Exact reach/collision, all clutch/length/release gestures and broader climbing
remain separate gates. The exact control/axis contract is recorded in
[the PC controls boundary](COJ_PC_CONTROLS_AND_HUD.md#exact-procedural-whip-contract).

Do not promote Body IK based on write counts alone. Acceptance requires visually plausible arms across representative poses, stable first-person ownership, verified restoration and no degradation of tracking/recenter behavior. Lower-body writing remains blocked until the upper-body/body-anchor contract is acceptable.
