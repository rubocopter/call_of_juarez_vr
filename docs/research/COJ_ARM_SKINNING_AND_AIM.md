# CoJ arm skinning and shot ownership — 2026-09-19

## Evidence

Run `20260919T085408Z-327dd354bc4f` is finalized and unstaged. Source was `1805753`,
proxy SHA-256 `3BD0B476810B15FBD935FA96F539E7303439B372F6BC45DA57BB40595CCE693F`.
It recorded 5,050 applied arms and 5,050 successful restores, zero arm restore/write failures,
and one successful recenter recovery. The user's 26.84-second video
`C:\Users\onita\Videos\clip_1.789.819.360.960.mp4` still shows collapsed/twisted arm skin.
Video SHA-256: `0DC300AA8AC5FD30CFD8AACF948FEE2D69FFEFBBE837D683D123CAC1AF2AE3E2`.
Runtime log SHA-256: `E9F615515ADEDCE955EF97B515956E499A9D3E3A8972D03D9A791062893C7BB5`.

The video shows the deliberate T-pose, forward palms-down/up and lowering sequence. Controller
overlays help show physical placement; no precise video-to-log time synchronization is claimed.

## The previous hierarchy assumption is false

`EBones` assigns semantic IDs; it does not prove parentage. Earlier documentation described
`upper -> forearm -> FORETWIST -> hand`, which the live transformations contradict.
`tools/analyze_arm_hierarchy.py` pairs natural/application and immediate post-write probes by
frame/side. For each basis axis it applies Rodrigues rotations using logged world-space upper and
forearm deltas. It reconstructs the FORETWIST rotation's world axis from its observed final basis
and logged local axis (an axis is invariant under its own rotation), then undoes that rotation.
There are 58 paired arm samples, or 116 up/forward axis comparisons per hypothesis:

| Observed frame | Predicted propagation | Mean axis error | Maximum |
| --- | --- | ---: | ---: |
| Forearm | Upper + forearm | 0.000001 | 0.000008 |
| FORETWIST, own roll undone | Upper only | 0.000001 | 0.000007 |
| FORETWIST, own roll undone | Upper + forearm | 0.583441 | 1.733806 |
| Hand, unchanged by FORETWIST | Upper + forearm | 0.000001 | 0.000008 |
| Hand, hypothetical inherited FORETWIST undone | Upper + forearm | 0.825442 | 1.891457 |

This proves the effective writer propagation for the observed exact model: FORETWIST behaves as
a sibling of forearm beneath upper; hand follows forearm and is unaffected by FORETWIST writes.
It does not establish a reusable Chrome Engine skeleton layout. The old writer bent the forearm
while leaving its weighted FORETWIST frame behind, then rolled FORETWIST while leaving the hand
behind. Joint target reach alone cannot detect that skinning disagreement.

## Correction

`BuildArmSkinningPlan` transports the complete natural FORETWIST basis through the exact upper
and forearm rotations, then applies the requested axial roll. Its natural bind rotation survives.
The writer computes a relative basis delta from the *observed* post-IK FORETWIST frame to that
target, using the existing element-local `RotateElementWithChildren` path. The independently
parented hand receives the same axial roll; the remaining full-controller wrist residual stays
diagnostic. No absolute pivot/world-position replacement is introduced.

Before accepting an application, all four FORETWIST/hand axes must agree with the composed target
within 0.02 unit-vector distance. Telemetry records `hand_rotation_mode=sibling_shared_roll`,
`skinning_contract=foretwist_sibling_swing_hand_shared_roll`, `skinning_frames_reached` and the
measured `skinning_axis_error`. Elbow/wrist reach, both-eye persistence and complete natural
restoration remain required. Rollback and normal restore include the hand before FORETWIST,
forearm and upper. Host tests exercise missing 90-degree elbow swing, positive/negative shared
roll, native bind-roll preservation and invalid inputs. Physical visual acceptance remains pending.

## Aiming ownership recovered from shipped bytecode

Inspection uses `tools/inspect_java_bytecode.py` against the exact installed `code.pak`:

- `Weapon.GetOwnerAttackDir(Vector)` delegates to `PawnArmed.GetFireDirForWeapon(Weapon,Vector)`.
- `ArmedPlayerBeing.GetFireDirForWeapon(Weapon,Vector)` delegates to its `(Weapon,boolean,Vector)`
  overload, which reads `GetBeingLookDirDevForHand` and applies native accuracy/spread.
- `GetBeingLookDirDevForHand(int,Vector)` copies `m_avLookDirDevForHand[hand]`.
- `UpdateLookAndAimDirs(float)` recalculates both entries with `ComputeLookDirDevForHand`.
- `GetFireOriginForWeapon` uses `GetBeingLookFromPoint` in the ordinary player path. It does not
  obtain the visible barrel position. The network-forced branch is separate and must not be
  repurposed as a VR shortcut.

Therefore moving the visible weapon during the transient render overlay cannot aim its bullets.
A complete implementation needs a proven game-update/shot boundary feeding per-hand direction
and muzzle origin after native aim calculation, with appropriate weapon identity, spread,
focus/tracking-loss handling and restoration. The current render-only hook cannot guarantee that
ownership. Aiming is researched but **not implemented** in the sibling-skinning candidate.

## Other observations

Transport recorded 2,843 new and 2,274 repeated submissions, zero submit failures, CPU-copy
average 10.375 ms / p95 14.998 ms. Head/hair intrusion remains visible. The log reports
`native_stereo_factory_hook: status=incomplete` and `shutdown_complete=false` despite `run_end`;
no inner presenter shutdown markers appear. Evidence packaging being complete does not promote
clean shutdown or body anatomy. Those defects remain open.
