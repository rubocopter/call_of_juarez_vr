# Call of Juarez arm skinning and weapon ownership research

This document keeps the exact-game conclusions that still govern Body IK and aiming. Superseded writer experiments and raw video/log chronology were removed after their conclusions were captured.

## Measured arm contract

Live evidence measures the native upper-arm + forearm chain at about **49.843 game units**. Recent evidence shows strongly asymmetric reach clamp, so the data does not justify simply lengthening both bones.

If the next physical candidate still feels short, clavicle/shoulder participation is the preferred controlled experiment because shipped `EBones` exposes dedicated clavicle elements.

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

The current writer therefore:

1. reads the natural complete element frames;
2. solves shoulder/elbow/wrist targets from measured geometry;
3. converts each required world-space rotation axis into the live element-local frame;
4. applies upper rotation;
5. re-reads the parent-adjusted child frame before applying the forearm rotation;
6. transports FORETWIST/hand orientation according to the observed sibling contract;
7. verifies joint/axis agreement;
8. restores child-before-parent and verifies the captured natural state.

If inverse restoration drifts beyond tolerance, exact natural frames are reapplied. Failure of both restoration paths disables further writes.

The writer/restoration mechanism is live-exercised; visual Body IK remains unpromoted because anatomy/reach still fails acceptance.

## Target space and body yaw

A failed candidate mapped recentered controller positions around tracking-space origin while the live skeleton was far away in game world coordinates. Current mapping anchors hand targets to the live head/body neighborhood and transforms `(tracked hand - tracked head)` through the exact Call of Juarez camera basis.

Another physical rejection showed that applying actor-owned HMD yaw again to the controller/body reference rotates arms a second time. Current host source removes actor-owned yaw from that mapping and leaves room-scale translation camera-owned while native actor position/grounding stay game-owned.

## Telemetry cost

Run `20260920T161307Z-474f0b054338` emitted 11,192 successful arm-tracking and 11,192 successful restore records while the user observed jerky movement even with keyboard. Latest diagnostic evidence `20260920T214629Z-351c27f434d5` remained severely jerky after routine telemetry/hash sampling and CPU-buffer reuse. Its gameplay process measured classic-D3D9 CPU copy at 7.432 ms median and 8.994 ms p95, so the renderer transport is now a measured performance problem independent of arm logging.

Routine arm telemetry remains sampled; write failures, rollback and restoration faults remain unconditional. Arm solving still has to be evaluated independently from renderer frame pacing.

## Weapon direction and origins

Exact shipped bytecode establishes separate ownership paths:

- per-hand fire direction ultimately reads `m_avLookDirDevForHand[hand]` and native accuracy/spread remains downstream;
- visual origin uses `m_avAimFromPoint[hand]`;
- ordinary local ballistic origin reaches `Being.m_vLookFromPoint`;
- the network-forced branch uses separate network state and remains untouched.

Controller `/pose/tip` supplies the exact per-hand direction and mapped visual origin after normal game look/aim update.

Run `20260920T204507Z-6db13f3107e0` ended immediately after the user's first shot attempt, before post-input telemetry recorded `fire_left/right=true`. The previous implementation already marked the pressed hand as cross-frame owner on that first press, so it could overwrite global `Being.m_vLookFromPoint` without first capturing/restoring the native value. That ownership was removed.

Current source allows controller-derived ballistic origin only around a synchronous fire `InputDigital.Translate` transition: read native `Being.m_vLookFromPoint`, write the selected controller origin, translate the input, then restore the captured native value immediately. Unchanged held frames do not mutate the field. The gameplay `LaserPointer` diagnostic path has also been removed.

In the latest gameplay process under reused run ID `20260920T214629Z-351c27f434d5`, 55 fire-pressed samples were recorded and the process reached normal outer `run_end`. This shows that the earlier first-shot termination was not reproduced, but the user still judged the shots to originate/travel incorrectly. Because that run ID covered three processes, it is diagnostic stability evidence only and cannot promote firing.

The next aim investigation must distinguish four independently owned values/orderings: controller tip mapping, `m_avAimFromPoint[hand]` visual origin, `Being.m_vLookFromPoint` synchronous ballistic origin and `m_avLookDirDevForHand[hand]` direction. Do not treat “no crash” as proof that any of those values are correct.

## Latest anatomy rejection

The latest clip still shows arms that feel too short and visibly contort during reload. Telemetry does not support solving this by scaling the upper/forearm chain globally:

- many poses have controller targets inside the ~49.843-unit measured native reach;
- elbow and wrist positional targets are often reached with very small residual error;
- `hand_orientation_reached=false` remains common with large up/forward orientation error;
- reload introduces a visible conflict between native animation and VR-driven element rotation.

The next controlled experiments are clavicle/shoulder participation and animation ownership during reload, with hand orientation measured separately from positional reach.

## Current body acceptance boundary

Do not promote Body IK based on write counts alone. Acceptance requires visually plausible arms across representative poses, stable first-person ownership, verified restoration and no degradation of tracking/recenter behavior. Lower-body writing remains blocked until the upper-body/body-anchor contract is acceptable.
