# Call of Juarez arm skinning and weapon ownership research

This document keeps the exact-game conclusions that still govern Body IK and aiming. Superseded writer experiments and raw video/log chronology were removed after their conclusions were captured.

## Measured arm contract

Live evidence measures the native upper-arm + forearm chain at about **49.843 game units**. Recent evidence shows strongly asymmetric reach clamp, so the data does not justify simply lengthening both bones. The user still reports visibly short arms.

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
fresh natural arm sample with the previous raw sample for the same actor generation
and recenter sequence. If the actor moved materially while shoulder/elbow/wrist
and all four sampled element positions stayed within float-scale world-position
tolerance, only those positions are translated by the actor delta before the next
IK solve; the current orientation bases are preserved. Any positional animation
change leaves the natural sample untouched. A per-arm cumulative rebase count is
sampled in tracking telemetry. This combined continuity contract is
**implemented / host-tested** and awaits physical validation.

## Target space and body yaw

A failed candidate mapped recentered controller positions around tracking-space origin while the live skeleton was far away in game world coordinates. Current mapping anchors hand targets to the live head/body neighborhood and transforms `(tracked hand - tracked head)` through the exact Call of Juarez camera basis.

Another physical rejection showed that applying actor-owned HMD yaw again to the controller/body reference rotates arms a second time. Current host source removes actor-owned yaw from that mapping. Run `20260920T235112Z-6b2d91cda4a5` then showed that leaving room-scale translation camera-only exposes the stationary local avatar during a physical step or crouch. The next candidate moved the local pelvis with full XYZ HMD translation, but non-promotable run `20260921T163309Z-481defca3401` measured up to `12.892973` game units of vertical pelvis offset while the body still entered the headset view. Current host source therefore applies only mapped horizontal room-scale translation to the local pelvis/skeleton for both eye renders and restores it afterwards, while vertical actor position, grounding and collision stay game-owned.

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

Controller `/pose/tip` supplies the exact per-hand direction and mapped visual origin after normal game look/aim update.

Run `20260920T204507Z-6db13f3107e0` ended immediately after the user's first shot attempt, before post-input telemetry recorded `fire_left/right=true`. The previous implementation already marked the pressed hand as cross-frame owner on that first press, so it could overwrite global `Being.m_vLookFromPoint` without first capturing/restoring the native value. That ownership was removed.

Further bytecode inspection established that `InputDigital.Translate` selects the requested hand/fire state but does not synchronously execute the final shot. The actual attack follows through `OnHandStateStarted_Attack -> WeaponAttack -> Weapon.Attack`, whose ordinary local origin reaches `GetFireOriginForWeapon -> Being.m_vLookFromPoint`. Current source therefore captures the native value and publishes the selected controller origin on the fire press transition so that the later native attack can consume it. If `Translate` fails the captured native value is restored immediately; after a successful transition, shipped `UpdateLookAndAimPoints` reclaims normal ownership. Unchanged held frames do not republish the field. The gameplay `LaserPointer` diagnostic path has also been removed.

The latest complete single-process run records 24 sampled fire states and 21 explicit fire-transition publications while reaching normal outer `run_end`; first-shot process termination is no longer the active problem. Aiming remains visibly wrong.

The controller-axis question is now mostly closed: 120 sampled `handgrip -> tip` comparisons put local `-Z` at `0.939388..0.939389`, far above the other local axes. The next aim investigation must therefore distinguish `m_avAimFromPoint[hand]` visual origin, the visible weapon/barrel transform, `Being.m_vLookFromPoint` ballistic origin and `m_avLookDirDevForHand[hand]` direction. Reintroduce a temporary controller-tip ray so the tracked direction can be seen beside the rendered weapon. The ray is instrumentation only; production shots must originate from the visible weapon barrel/muzzle.

## Latest anatomy rejection

The newest clip shows a different failure mode: safety blocks some extreme writes, but the arms repeatedly return to the native/default game pose as the Sense controllers move. Telemetry does not support solving this by scaling the upper/forearm chain globally:

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
