# Codex handoff — Call of Juarez VR

## Current checkpoint

Staging and process state are local runtime state; inspect them with `tools/vr_test.ps1 status` before continuing a physical run.

The latest physical evidence is run ID `20260920T214629Z-351c27f434d5`, from dirty source based on `e4e86eeb72a4e2cb2827a1952796a16b82094a4c`. The staged ID was reused by three CoJ processes (PIDs 12048, 8600 and 4688) after Circle/back failures forced relaunches, so this is **multiprocess diagnostic evidence only** and cannot promote a gate. Compact metadata is retained in `docs/research/evidence/20260920T214629Z-351c27f434d5.json`.

The user observed:

- Sense triggers did skip the intro videos;
- subtitles were not visible;
- menu hover still did not move the highlighted option with the Sense pointer; moving the physical mouse did, and the trigger activated the currently selected item rather than the ray target;
- Circle/back could hide/show UI layers, freeze the game and leave a flat frozen presentation;
- walking/running remained severely jerky and button jump looked extremely low/cut short;
- physical crouch exposed the full avatar in front of the camera instead of preserving a first-person leg view;
- arms still felt short and deformed, especially during reload;
- shots still had unacceptable origin/direction, but the gameplay process recorded 55 fire-pressed samples and reached normal `run_end` rather than terminating on first shot;
- image quality remained poor at a 1920x1080 source surface while SteamVR recommended 3400x3468.

Do not promote any gate from that run.

## Current untested candidate

The working tree contains follow-up source changes after that run. They have **not** received a new headset test.

### Flat UI and loading

- Pointer motion keeps `MainMenuModule.GetGlobalCursor()` -> `UICursorGame.SetPos(LVector;)V` -> `OnMouseMove(FFI)V` for logical synchronization and additionally mirrors the projected ray through Win32 `SetCursorPos` + `WM_MOUSEMOVE`. Static bytecode plus the physical mouse comparison show that cursor-sprite motion alone does not drive shipped hover hit-testing.
- Cross maps to global `ui_accept`.
- Circle maps to global `ui_back` and dispatches a normal Escape press/release through the active `GameUserInterface.CallOnInputKeyGlobal`; `MainMenuModule.ShowPrevUI()` is rejected for this purpose.
- L2/R2 remain left/right ray-select.
- The visible flat-theater beam now starts at the projected Sense-controller origin and ends at the screen hit.
- Startup skip remains a separate exact `IntroModule.OnInputKey` route.
- The blocking post-load prompt dispatches directly through `GameUILoading.OnInputKey(IZC)V`.
- Paused-hint dispatch now obtains the manager via shipped `LawmanModule.GetHintManager()` before `DisableCurrentHint`. The latest physical log contained 202 `NoSuchFieldError: m_cHintManager` exceptions from the previous direct inherited-field lookup.

Subtitles remain unresolved. `DialogSubtitle.Show` is gated by `Settings.bSubtitles`; the next diagnostic must establish that runtime value and whether `Show` executes before changing subtitle rendering/layering.

### Locomotion/playability telemetry

The earlier rejected run logged 11,192 successful `body_arm_tracking` and 11,192 successful `body_arm_restore` events, and the latest run still felt severely jerky after sampling/reuse changes. In the gameplay process, 70 sampled CPU copies measured 7.432 ms median, 8.994 ms p95 and 9.644 ms max at 1920x1080 per eye. This alone consumes most or more than a 120 Hz frame budget before the rest of the frame.

The native movement path still uses the shipped per-axis 0.04 shaping and float actions 4-7; jump input also reaches the game. Keep native movement/jump semantics and renderer stutter as separate investigations. Classic-D3D9 CPU readback remains the structural transport problem. OFXR-Bridge is an OpenXR optical-flow frame-generation layer and does not remove this active D3D9/OpenVR readback; retain it only as future OpenXR research.

### Recenter / horizon

The clip showed a visibly rolled world. `RelativePoseTracker` now stores a gravity-level recenter reference that preserves HMD yaw only, so pitch/roll present at recenter cannot become the tracking-space basis. A regression test recentering from a tilted HMD pose passes.

### Physical crouch

`CoJPhysicalCrouchState` detects calibrated HMD-height drop with hysteresis for state and telemetry, but physical HMD crouch does not automatically press the native crouch action. The latest run still exposed the full avatar while telemetry had `physical_crouch=true` and native `crouch=false`, so the old double-transform explanation is no longer sufficient. Treat this as first-person camera/local-mesh ownership; explicit controller crouch still uses the native action, while actor position and grounding remain game-owned.

### Weapon origin/direction

`/pose/tip` continues to drive per-hand direction and visual origin. Controller-derived `Being.m_vLookFromPoint` is now scoped to the synchronous fire `InputDigital.Translate` transition: the native value is read first, the controller origin is written for the call, and the native value is restored immediately afterwards. Held frames do not retain controller ownership.

The normal gameplay `LaserPointer` diagnostic path has been removed. The latest gameplay process emitted 55 fire-pressed samples and reached normal outer `run_end`, so the previous first-shot process termination was not reproduced. The user still rejected the shot trajectory/origin; stability evidence does not validate aiming.

### Body IK

The proven writer/restoration contract is unchanged. The measured native upper+forearm chain is still ~49.843 game units. Latest telemetry often reaches elbow/wrist targets with tiny positional error while hand orientation remains far from the controller target, and many poses are not reach-clamped. The clip still shows short/deformed arms and reload contortion. Do not extend both bones blindly; investigate clavicle/shoulder participation and native reload-animation conflict as controlled experiments.

## Host verification

Fresh current-tree verification on 2026-09-21:

- Debug build: pass.
- Debug CTest: 25 outcomes, **24 PASS + 1 expected SKIP**, 0 failures.
- Release build: pass.
- Release CTest: 25 outcomes, **24 PASS + 1 expected SKIP**, 0 failures.

The skipped test is the expected classic-D3D9 shared-texture capability check; the readback fallback test passes.

## Next headset gate

Prepare one fresh candidate and use one game process. Verify, in roughly this order:

1. Startup appears in flat theater and scene focus is stable.
2. Sense beam visibly starts at the controller and actual highlighted hover follows the ray without touching the mouse.
3. Cross accepts, Circle performs ordinary back without layer loss/freeze, L2/R2 select, and startup skip works.
4. The post-load “press a key” prompt continues from a Sense action and paused-hint handling produces no JNI field exception spam.
5. Confirm subtitles; if absent, capture whether `Settings.bSubtitles` is true before changing rendering.
6. Recenter while slightly tilted and confirm the world remains level afterwards.
7. Walk, run and jump with Sense; repeat with keyboard and compare smoothness. Retain producer/readback timing.
8. Physically crouch and confirm first-person local-mesh ownership; separately test explicit controller crouch.
9. Exercise ordinary and reload arm poses; record reach versus orientation failure separately.
10. Fire one shot, then repeated/held shots; judge origin/direction now that process stability has improved.
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
| `20260920T214629Z-351c27f434d5` | latest multiprocess diagnostic rejection: menu/subtitle/locomotion/crouch/body/aim still fail; repeated firing survived; measured readback cost remains severe |

Historical multiprocess and superseded runs remain useful only where a research document explicitly preserves a derived contract. Large local copies are not required after that derivation is recorded.

## Constraints to preserve

- Exact binary identity controls game-specific mutation.
- The game owns natural camera, actor position, grounding, collision and normal animation.
- HMD/eye/body/weapon changes are scoped transactions with restoration.
- Do not generalize exact Chrome Engine layouts to another title without independent proof.
- SteamVR and the game are launched/closed manually.
- One staged run ID equals one game process.
- Validation state advances only from evidence satisfying `docs/VALIDATION.md`.
