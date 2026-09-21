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

Another physical rejection showed that applying actor-owned HMD yaw again to the controller/body reference rotates arms a second time. Current host source removes actor-owned yaw from that mapping. Run `20260920T235112Z-6b2d91cda4a5` then showed that leaving room-scale translation camera-only exposes the stationary local avatar during a physical step or crouch. The next candidate moved the local pelvis with full XYZ HMD translation, but non-promotable run `20260921T163309Z-481defca3401` measured up to `12.892973` game units of vertical pelvis offset while the body still entered the headset view. Current host source therefore applies only mapped horizontal room-scale translation to the local pelvis/skeleton for both eye renders and restores it afterwards, while vertical actor position, grounding and collision stay game-owned.

## Telemetry cost

Run `20260920T161307Z-474f0b054338` emitted 11,192 successful arm-tracking and 11,192 successful restore records while the user observed jerky movement even with keyboard. Latest complete single-process evidence `20260921T192639Z-1d70905cb4f8` still rejects movement and measures classic-D3D9 CPU copy at 7.112 ms median and 9.056 ms p95, so renderer transport remains a measured performance problem independent of arm logging.

Routine arm telemetry remains sampled; write failures, rollback and restoration faults remain unconditional. Arm solving still has to be evaluated independently from renderer frame pacing.

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

The current candidate applies a hand residual only when the original mismatch is at most 30 degrees, rejects arm writes more than 10% beyond measured reach, requires a conservative upper/forearm rotation plan, keeps FORETWIST roll disabled and yields to shipped reload ownership. Unsafe plans return ownership to native animation. That fail-closed behavior is technically safer but visually discontinuous; the next solver work must maintain plausible continuous ownership across ordinary controller motion before adding more body scope. Clavicle/shoulder participation remains a measured experiment, with hand orientation kept separate from positional reach.

## Current body acceptance boundary

Do not promote Body IK based on write counts alone. Acceptance requires visually plausible arms across representative poses, stable first-person ownership, verified restoration and no degradation of tracking/recenter behavior. Lower-body writing remains blocked until the upper-body/body-anchor contract is acceptable.
