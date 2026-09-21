# Codex handoff — Call of Juarez VR

## Current checkpoint

Staging and process state are local runtime state; inspect them with `tools/vr_test.ps1 status` before continuing a physical run.

The newest physical diagnostic is complete single-process run `20260921T192639Z-1d70905cb4f8` (PID 24736), correlated with `C:\Users\onita\Videos\clip_1.790.022.783.959.mp4`. It has one `run_start`, one matching `run_end`, intact deployment hashes and a collected evidence package. It is valid formal evidence, but the user rejects all major playability areas exercised in this run.

The user observed:

- the sole Win32 VR menu pointer is still uncontrollable; the user had to use the physical mouse;
- walking remains terrible and jump height is only about 2 cm even though gameplay input reaches full `0.999992` magnitude;
- moving physically should produce the same visual locomotion animation as stick movement, but the current render-only body offset does not drive that animation;
- moving the Sense controllers repeatedly returns the arms to the game's default pose;
- shots still occur in visibly nonsensical directions/origins; the user requires the final shot to leave the visible weapon barrel and wants a temporary controller ray restored to align the systems.

The new run changes several diagnoses. Horizontal-only body compensation is working technically: all 64 sampled room-scale pelvis writes had `world_offset.y=0`. The native analog controller-state transaction is also present in 126 sampled gameplay updates, so its absence no longer explains locomotion. Arm guards denied 43/128 sampled writes, explaining the repeated visible fallback to native animation. Grip-to-tip geometry produced 120 `-Z` dot samples tightly clustered at `0.939388..0.939389`, strongly confirming the Sense tip-axis convention even though weapon aiming remains wrong.

Do not promote menu pointer, locomotion/jump, Body IK or weapon aiming from this run. Preserve earlier validated stereo/tracking/recenter/snap contracts.

## Current tested state

The current code was the candidate exercised by `20260921T192639Z-1d70905cb4f8`. No new playability fix has been implemented after that run; current edits are documentation/evidence only.

### Flat UI and loading

- Pointer motion has one owner: `SetCursorPos` + absolute `SendInput` + `WM_MOUSEMOVE`. This route is now physically rejected for usability. Do not layer another owner on top without first identifying why projected Sense motion becomes uncontrollable.
- Cross maps to global `ui_accept`.
- Circle maps to global `ui_back`, dispatches normal Escape through an active `GameUserInterface`, and falls back to `LawmanGame.sm_cActiveGameModule.OnInputKey(Escape)` when gameplay has no current UI.
- L2/R2 remain left/right ray-select.
- The visible flat-theater beam now starts at the projected Sense-controller origin and ends at the screen hit.
- Startup skip remains a separate exact `IntroModule.OnInputKey` route.
- The blocking post-load prompt dispatches directly through `GameUILoading.OnInputKey(IZC)V`.
- Paused-hint dispatch now obtains the manager via shipped `LawmanModule.GetHintManager()` before `DisableCurrentHint`. The latest physical log contained 202 `NoSuchFieldError: m_cHintManager` exceptions from the previous direct inherited-field lookup.

Subtitle absence was diagnosed in retained run `20260920T235112Z-6b2d91cda4a5`: all 376 samples reported `Settings.bSubtitles=false`, and `DialogSubtitle.Show` exits in that state. Enable the shipped setting before judging presentation; do not add adapter-side forcing.

### Locomotion/playability telemetry

The newest run contains 266 sampled gameplay states, reaches `0.999992` movement magnitude, records 27 run samples and 5 jump samples, and still feels terrible. Its classic-D3D9 CPU copy measures 7.112 ms median, 9.056 ms p95 and 11.849 ms max at 1920x1080 per eye.

The native movement path uses shipped per-axis 0.04 shaping, exact `InputSettings.GetTargetTypeForAction(action)` routing and the shipped `LockApplyControllerState -> dispatch/Translate -> UnlockApplyControllerState -> ApplyControllerState` transaction. The latest run physically exercised that transaction in 126 samples and still rejected locomotion, so do not continue changing input routing without new root-cause evidence. Jump remains exact native digital action 11 and is observed in gameplay telemetry, but visible amplitude is only about 2 cm. Separate game-space locomotion/jump semantics from renderer pacing before another fix.

### Recenter / horizon

The clip showed a visibly rolled world. `RelativePoseTracker` now stores a gravity-level recenter reference that preserves HMD yaw only, so pitch/roll present at recenter cannot become the tracking-space basis. A regression test recentering from a tilted HMD pose passes.

### Physical crouch

`CoJPhysicalCrouchState` still keeps physical HMD crouch separate from the native crouch action. Horizontal-only body compensation is now physically exercised: 64 sampled visual-pelvis writes all had zero Y offset and restored after stereo capture. That fixes the previous vertical-pelvis mistake. A separate missing requirement is now explicit: horizontal HMD room-scale movement must drive a plausible visual walk animation comparable to stick locomotion while native actor grounding/collision remain authoritative.

### Weapon origin/direction

`/pose/tip` continues to drive per-hand direction and visual origin. Bytecode establishes that `InputDigital.Translate` only selects the hand state; the actual shot is created later by `OnHandStateStarted_Attack -> WeaponAttack -> Weapon.Attack`, whose origin reaches `GetFireOriginForWeapon -> Being.m_vLookFromPoint`. `ArmedPlayerBeing.OnBeingsFrame()` runs `UpdateHandStates` before `OnPostBeingsUpdate()` later runs `UpdateLookAndAimDirs`/`UpdateLookAndAimPoints`, so the native post-update recomputation cannot overwrite the published fire transition before `WeaponAttack`. Translation failure still rolls back the captured native value.

The newest run now answers the controller-axis question: 120 sampled grip-to-tip comparisons place local `-Z` at `0.939388..0.939389`, while 21 fire transitions successfully publish changing controller directions/origins. The user still sees nonsensical shots, so the remaining failure is downstream of simple Sense axis selection. Do not guess a new controller offset.

Reintroduce a **temporary diagnostic** gameplay ray from the controller tip along the proven tracked direction so controller orientation can be compared directly with the visible weapon/barrel. Keep it visually diagnostic only. The production contract is that the actual shot origin matches the visible weapon barrel/muzzle and its direction agrees with the intended controller-aligned weapon direction.

### Body IK

The writer/restoration contract remains transactional, but the safety policy is now visibly discontinuous. Of 128 sampled arm updates, 83 applied VR writes and 43 denied them; 32 were reach-unsafe and 80 rejected the hand residual. The user sees these denials as the arm resetting to the game's default pose while moving the Sense controllers. The fail-closed guard prevents some deformation, but Body IK is still unusable because ownership alternates between VR and native animation instead of maintaining a continuous plausible pose.

## Host verification

Fresh current-tree verification on 2026-09-21:

- Debug build: pass.
- Debug CTest: 25 outcomes, **24 PASS + 1 expected SKIP**, 0 failures.
- Release build: pass.
- Release CTest: 25 outcomes, **24 PASS + 1 expected SKIP**, 0 failures.

The skipped test is the expected classic-D3D9 shared-texture capability check; the readback fallback test passes.

## Next headset gate

Do not prepare another headset candidate until the next code change addresses at least one diagnosed boundary. The next implementation/verification sequence should prioritize:

1. Startup appears in flat theater and scene focus is stable.
2. Replace or isolate the current pointer ownership model; the sole Win32 path is already physically rejected.
3. Cross accepts, Circle performs ordinary back without layer loss/freeze, L2/R2 select, and startup skip works.
4. The post-load “press a key” prompt continues from a Sense action and paused-hint handling produces no JNI field exception spam.
5. Enable subtitles in the shipped settings, then confirm visible text and capture runtime subtitle state.
6. Recenter while slightly tilted and confirm the world remains level afterwards.
7. Measure actual game-space walk/run speed and jump displacement separately from frame pacing; the input transaction itself is already proven present.
8. Add physical-walk visual animation driven by horizontal HMD motion while preserving actor/collision ownership.
9. Make arm ownership continuous across reachable controller motion; a safety rejection must not create visible default-pose snapping.
10. Reintroduce the temporary controller-tip ray, compare it against the visible weapon barrel, then trace the exact mismatch to visual weapon transform, muzzle origin or ballistic ownership. `-Z` no longer needs another axis experiment.
11. Exercise flat-theater -> native-stereo -> flat-theater transitions if available.
12. Close normally and run `finish`; inspect both outer `run_end` and inner presenter shutdown state.

Do not add more speculative anatomy or weapon changes before this gate unless the current candidate cannot build/run.

## Physical evidence still used as authority

| Run | What it proves |
| --- | --- |
| `20260916T153109Z-8976b8f77775` | corrected 100 units/metre eye baseline and physical Create recenter |
| `20260916T221254Z-861f3c15abd4` | scene-focus/lifecycle correction and clean outer runtime stop/run end |
| `20260916T224239Z-e43b46698e5c` | explicit render pose removed previous head-turn pull/snap-back |
| `20260919T153546Z-705460dca03b` | exact ±45° snap turn; body IK improved but still visually rejected |
| `20260920T090448Z-b1f54e3cb38e` | startup flat theater, load to native stereo, recenter, local head/hair suppression |
| `20260920T152316Z-48dab54366d5` | diagnostic UI/locomotion/body-yaw/shot-origin failures driving follow-up work |
| `20260920T161307Z-474f0b054338` | clean playability rejection that exposed heavy synchronous telemetry and remaining body/UI problems |
| `20260920T204507Z-6db13f3107e0` | single-process rejection: intro skip passed; menu hover/loading continue, smoothness, horizon, crouch and first-shot stability failed |
| `20260920T214629Z-351c27f434d5` | earlier multiprocess diagnostic rejection: menu/subtitle/locomotion/crouch/body/aim still fail; repeated firing survived; measured readback cost remains severe |
| `20260920T235112Z-6b2d91cda4a5` | earlier single-process rejection: hover still required physical mouse motion, gameplay back missing, subtitles disabled, full-range movement still poor, body exposed by room scale and right arm deformed |
| `20260921T163309Z-481defca3401` | non-promotable reused-run diagnostic: pointer selectable but chaotic; walking/jump, local-body visibility, aiming and right-arm anatomy all rejected; second process isolated full-range movement plus extreme arm/vertical-pelvis values |
| `20260921T192639Z-1d70905cb4f8` | latest complete single-process rejection: sole Win32 pointer still uncontrollable; analog transaction present but movement/jump still wrong; horizontal-only pelvis compensation works technically but physical-walk animation is missing; arm guards cause native-pose resets; `-Z` tip axis is confirmed while weapon muzzle/ballistics remain wrong |

Historical multiprocess and superseded runs remain useful only where a research document explicitly preserves a derived contract. Large local copies are not required after that derivation is recorded.

## Constraints to preserve

- Exact binary identity controls game-specific mutation.
- The game owns natural camera, actor position, grounding, collision and normal animation.
- HMD/eye/body/weapon changes are scoped transactions with restoration.
- Do not generalize exact Chrome Engine layouts to another title without independent proof.
- SteamVR and the game are launched/closed manually.
- One staged run ID equals one game process.
- Validation state advances only from evidence satisfying `docs/VALIDATION.md`.
