# Call of Juarez arm skinning and weapon ownership research

This document keeps the exact-game conclusions that still govern Body IK and aiming. Superseded writer experiments and raw video/log chronology were removed after their conclusions were captured.

## Measured arm contract

Live evidence measures the native upper-arm + forearm chain at about **49.843 game units**. Asymmetric reach clamp did not justify an arbitrary fixed multiplier. The current render-only fit instead measures bilateral controller span in a stable T pose; fitted visual proportions still require physical acceptance.

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
sample count. Observed-pose and yaw-rotated host regressions pass; fitted anatomy
remains a physical gate.

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
