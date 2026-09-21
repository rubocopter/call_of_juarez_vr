# Validation

Validation states are intentionally strict:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

A higher state requires direct evidence for that boundary. Static disassembly, source existence, CTest and a successful build cannot substitute for a physical acceptance gesture.

## Current repository gate — 2026-09-21

The current source was physically exercised by complete single-process run `20260921T192639Z-1d70905cb4f8`. That run rejects menu-pointer usability, locomotion/jump, continuous Body IK and weapon aiming. It does exercise several technical safeguards successfully: horizontal-only body compensation has zero sampled vertical pelvis displacement, unsafe arm writes fail closed, and Sense tip geometry strongly identifies local `-Z` as the pointing axis.

Fresh current-tree host verification:

| Configuration | Build | CTest |
| --- | --- | --- |
| Debug | pass | 24 PASS + 1 expected SKIP, 0 failures |
| Release | pass | 24 PASS + 1 expected SKIP, 0 failures |

The expected skip is `d3d9_classic_d3d11_shared_texture`; the classic-D3D9 readback fallback remains covered and passes.

Staging is local runtime state; inspect it with `tools/vr_test.ps1 status` before preparing or finishing a physical run.

## Next physical acceptance gate

One fresh `prepare`, one Call of Juarez process, one `finish`. The run should exercise:

| Area | Required observation |
| --- | --- |
| Startup/presentation | flat theater visible, stable scene ownership, startup skip works |
| Menu pointer | beam starts at Sense controller and hover is controllable without touching the physical mouse; the current sole Win32 path is already known to fail this gate |
| Menu actions | Cross accept, Circle normal Escape/back without layer loss/freeze, L2/R2 ray-select |
| Blocking UI | post-load continue responds to Sense through `GameUILoading.OnInputKey` without actor/body mutation |
| Subtitles | enable the shipped subtitle setting first, then verify visible text; latest evidence already established `Settings.bSubtitles=false` |
| Recenter/horizon | recenter leaves the world level even if the HMD has pitch/roll at the recenter instant |
| Locomotion | walk/run speed is acceptable and jump has useful native-scale amplitude; distinguish movement semantics from D3D9 frame pacing rather than changing controller routing again |
| Room-scale body | physical horizontal displacement keeps actor collision native but drives a plausible visual walk animation comparable to stick locomotion |
| Crouch | physical HMD drop preserves first-person local-mesh ownership with horizontal-only visual-body compensation while native crouch remains false; explicit controller crouch still works |
| Body IK | arms track continuously without unsafe deformation or repeated fallback to the default game pose; verify reload separately |
| Weapon | temporary controller ray and visible weapon barrel can be compared directly; final shots originate from the weapon/barrel and follow the intended controller-aligned direction |
| Mode transition | flat theater and native stereo switch safely where the game state requires it |
| Shutdown | outer `run_end` plus inner presenter finalization are both inspected |

Failure of one area does not invalidate independent observations, but it must not promote the failed gate.

## Current feature state

| Contract | State | Evidence/limit |
| --- | --- | --- |
| Exact camera -> view/projection -> renderer path | live-tested | physical camera-path gate passed |
| Complete real-color two-eye ChromeEngine render | live-tested | distinct eye surfaces and SteamVR submission observed |
| 100 game-units/metre eye baseline | headset-validated | `20260916T153109Z-8976b8f77775` |
| HMD yaw/pitch/roll and positional camera offset | headset-validated | repeated physical stereo runs |
| Explicit render-pose submission | headset-validated | `20260916T224239Z-e43b46698e5c` removed pull/snap-back artifact |
| Left-Sense Create recenter | headset-validated | physical recenter exercised |
| Exact ±45° snap turn | headset-validated | `20260919T153546Z-705460dca03b` |
| Flat-theater startup/load -> native stereo | headset-validated for exercised path | `20260920T090448Z-b1f54e3cb38e` |
| Local Ray/Billy head/hair suppression | headset-validated for HMD view | same run; shadow behavior unverified |
| Analog locomotion through native float actions | live-exercised / rejected for playability | current run observed 126 shipped lock/dispatch/unlock/apply transactions and `0.999992` input magnitude; routing is no longer the leading explanation for terrible movement |
| Native jump action 11 | live-exercised / rejected | 5 sampled jump states reached gameplay, but the user reports only about a 2 cm hop |
| Reduced routine Body IK telemetry | host-tested | introduced after `20260920T161307Z-474f0b054338` |
| Sampled eye distinction + recycled CPU frame storage | host-tested | reduces current CPU path cost; physical smoothness still pending |
| Yaw-only/gravity-level recenter reference | host-tested | regression covers tilted-HMD recenter; physical horizon check pending |
| Native flat-menu cursor route | live-exercised / rejected | sole Win32 `SetCursorPos` + absolute `SendInput` + `WM_MOUSEMOVE` still produces an uncontrollable pointer; the user had to use the mouse |
| Cross accept / Circle back / L2-R2 select | host-tested follow-up | Circle uses active-UI Escape and falls back to `LawmanModule.OnInputKey(Escape)` in gameplay |
| Loading continue via `GameUILoading.OnInputKey` | host-tested | current post-run source only |
| Paused-hint manager lookup | host-tested | shipped `LawmanModule.GetHintManager()` accessor replaces direct `m_cHintManager` lookup that emitted 202 Java exceptions |
| Controller-origin flat beam | host-tested | current post-run source only |
| Subtitle setting diagnosis | live-tested | all 376 latest samples reported `Settings.bSubtitles=false`; presentation remains untested with subtitles enabled |
| Visual body room-scale ownership | live-exercised technically | 64 sampled render-only offsets all had `world_offset.y=0` and restored; physical walking still lacks stick-equivalent locomotion animation |
| Visible arm writer/restoration | live-tested | exact writer changes geometry and restores; visual anatomy still fails |
| Exact gameplay target routing | host-tested | each action now selects only `m_Targets[InputSettings.GetTargetTypeForAction(action)]`; actions 4-7 keep the direct float path |
| Body IK anatomy/reach/orientation | live-exercised / rejected for continuity | 43/128 sampled arm updates denied VR writes; the fail-closed policy prevents some unsafe takeover but visibly resets arms to native animation during controller motion |
| Native reload ownership | host-tested | VR arm writes yield while `ArmedPlayerBeing.IsWeaponReloading()` is true; observation failure leaves normal IK active |
| Controller-owned weapon direction/visual origin | host-tested | physical firing still required |
| Ballistic-origin attack-transition ownership | host-tested | fire press publishes controller `Being.m_vLookFromPoint`; native `WeaponAttack` consumes it before `UpdateLookAndAimPoints` reclaims the field |
| Sense tip-axis/fire-transition diagnostics | live-tested diagnostically | 120 sampled axis values put local `-Z` at `0.939388..0.939389` against grip-to-tip direction; simple axis selection is no longer the main aiming hypothesis |
| Temporary gameplay controller alignment ray | planned diagnostic | reintroduce a visible controller-tip ray only to compare tracked direction with weapon/barrel geometry before changing ballistic ownership |
| Gameplay diagnostic shot ray | removed | no runtime `LaserPointer` object is created by the firing path |
| Outer runtime shutdown | live-exercised | recent runs reach `run_end` |
| Inner presenter finalization | open | recent body/playability evidence reports `shutdown_complete=false` |
| Supported end-to-end VR release | planned | pre-alpha |

## Latest physical evidence

### `20260921T192639Z-1d70905cb4f8` — complete single-process diagnostic rejection

One process (PID 24736), matching `run_start`/`run_end`, intact deployment hashes and a collected evidence package. Video: `C:\Users\onita\Videos\clip_1.790.022.783.959.mp4`, 140.75 s, SHA-256 `D4AC624A58783431E842D0AAB961D3EFD88F1A6A2514CAE1F9B5232CE1421055`.

The sole Win32 menu-pointer route still behaved uncontrollably and the user returned to the physical mouse. Locomotion remained terrible even though movement reached `0.999992` and 126 sampled gameplay updates reported the shipped analog transaction. Five sampled jump states reached gameplay, but the visible jump was only about 2 cm. Classic-D3D9 CPU copy measured 7.112 ms median and 9.056 ms p95.

Horizontal-only room-scale body compensation is technically exercised: all 64 sampled visual-pelvis writes had zero Y offset and restored after both eyes. That fixes the previous full-XYZ mistake, but physical horizontal walking still does not trigger the visual walk animation expected from stick locomotion.

Arm safety is also exercised but not accepted: 83 sampled arm updates applied VR writes while 43 denied them, including 32 reach-unsafe cases; 80 samples rejected the hand residual. The user sees this fail-closed behavior as the arms repeatedly snapping back to the game's default pose when moving the Sense controllers. Body IK remains rejected.

Aim diagnostics narrow the weapon problem. Across 120 grip-to-tip axis samples, `dot(-Z)` stays at `0.939388..0.939389`, strongly confirming local `-Z` as the Sense tip convention. Twenty-one fire transitions successfully published changing controller directions/origins, yet the user still saw nonsensical shots. The remaining fault is therefore downstream of simple tip-axis selection. The next diagnostic must temporarily render a controller-tip ray and compare it directly with the visible weapon/barrel; production shots are required to originate from the weapon barrel rather than from an arbitrary controller point.

Inner presenter finalization still reported `shutdown_complete=false`. No playability gate advances. Metadata: `docs/research/evidence/20260921T192639Z-1d70905cb4f8.json`.

### `20260921T163309Z-481defca3401` — reused-run diagnostic rejection

The first launch had a stuck SteamVR UI and the game was started again without preparing a new run ID, so the staged identity spans PIDs 2372 and 14492 and cannot promote any gate. PID 14492 is the useful gameplay process. Video: `C:\Users\onita\Videos\clip_1.790.013.828.839.mp4`.

The VR menu pointer could finally select items but moved chaotically and was difficult to control. Walking and jumping remained unacceptable even though movement again reached `0.999992`. The local model still entered the headset view, including after recenter; shooting remained impossible to aim reliably; and the right arm still contorted visibly.

Telemetry in the useful process reached `119.548°` upper-arm rotation, `175.008°` forearm rotation, `179.47°` hand residual and `12.892973` game units of vertical visual-pelvis room-scale displacement. These observations drove the current single-owner pointer path, native analog transaction, horizontal-only visual-body compensation, arm rotation-plan safety and tip-axis/fire-transition diagnostics. No validation state advances.

Metadata: `docs/research/evidence/20260921T163309Z-481defca3401.json`.

### `20260920T235112Z-6b2d91cda4a5` — single-process diagnostic rejection

Complete dirty-source run tied to build manifest `DE2AEC43078653EC49E5A7CF686B5C472594D1F07E423273265C5AD6E73D1E93`, one process (PID 19572), matching `run_start`/`run_end`, and the supplied 126.629-second video. It is valid diagnostic evidence but rejects every exercised playability gate.

Menu hover changed only while the physical mouse moved, despite logical cursor, `SetCursorPos` and posted `WM_MOUSEMOVE` routes. Circle could not open an exit/pause path from native-stereo gameplay. Physical steps and crouch exposed the local avatar. Walking and jump remained slow and poor; however, movement reached 0.999623/0.999992 maximum axis magnitude, so input range was not clipped. All 376 subtitle observations reported `Settings.bSubtitles=false`.

The right arm visibly deformed. Telemetry observed targets up to 75.800 units against the 49.843-unit native chain and hand residuals up to 175.967 degrees. Arm transactions still restored cleanly in the sampled telemetry. D3D9 CPU copy measured 6.890 ms median and 9.692 ms p95 at 1920x1080 per eye, and inner presenter shutdown still reported `shutdown_complete=false`. No validation state advances.

Metadata: `docs/research/evidence/20260920T235112Z-6b2d91cda4a5.json`.

### `20260920T214629Z-351c27f434d5` — multiprocess diagnostic rejection

Dirty-source evidence tied to build manifest `88DA5F3070D4244D49AD41A2636AE6820F97222A4FCD9F7205CC898405BD16DF`. The same staged run ID was reused by three CoJ processes (PIDs 12048, 8600 and 4688) after Circle/back failures forced relaunches, violating the one-process run-identity rule. It is diagnostic only and cannot promote a physical gate.

The user confirmed startup video skipping. Subtitles were invisible; the Sense pointer moved but did not drive menu hover until the real mouse moved; Circle could hide/show layers, freeze the game and leave a flat frozen presentation; locomotion and jump remained visibly unacceptable; physical crouch exposed the full avatar; and arms remained short/deformed, especially on reload. The gameplay process rendered/captured 1920x1080 per eye while SteamVR recommended 3400x3468, with 70 CPU-copy samples at 7.432 ms median, 8.994 ms p95 and 9.644 ms max.

The gameplay process recorded 55 fire-pressed samples and reached normal outer `run_end`, so the previous first-shot process termination was not reproduced. The user still rejected shot origin/direction. Telemetry also recorded physical crouch with native crouch false while the avatar remained visible, ruling out native double-crouch as the sole explanation. Inner presenter shutdown still reported `shutdown_complete=false`.

Metadata: `docs/research/evidence/20260920T214629Z-351c27f434d5.json`.

### `20260920T204507Z-6db13f3107e0` — diagnostic rejection

Single-process dirty-source run that reached outer `run_end`. The Sense triggers successfully skipped the intro videos. The user still found that the menu pointer did not move the highlighted option, the post-load “press a key” prompt required the keyboard, locomotion was severely jerky, physical crouch exposed the full avatar in front of the HMD, and the horizon appeared rolled/tilted. The user also reported that the first shot terminated the game.

The runtime recorded controller aim and normal gameplay immediately before exit but no `fire_left/right=true` gameplay-input record, so the exact first-shot failure site is not proven by telemetry. This run drove the current global-cursor/loading route, yaw-only recenter reference, physical-crouch ownership change, fire-origin transaction and transport-cost reductions. No physical gate is promoted from it.

Metadata: `docs/research/evidence/20260920T204507Z-6db13f3107e0.json`.

### `20260920T161307Z-474f0b054338` — diagnostic rejection

Clean single-process run from source `e4e86eeb72a4e2cb2827a1952796a16b82094a4c`. It reached normal outer `run_end`; evidence is complete. The user rejected playability because controller menu semantics were still unreliable, the ray origin was wrong, startup skip remained difficult, movement/jump stuttered even with keyboard, arms felt short, shots remained native/gaze-origin and crouch exposed the body.

Telemetry included 11,192 successful arm tracking records and 11,192 successful arm restores, with 27.09% left and 70.82% right target clamp and ~49.843 game-unit native two-bone chain. This drove the current telemetry sampling and anatomy direction; it does not validate them.

Metadata: `docs/research/evidence/20260920T161307Z-474f0b054338.json`.

### `20260920T152316Z-48dab54366d5` — diagnostic rejection

Single-process dirty-source run. Startup skip/menu selection, locomotion, body yaw and shot-origin behavior failed. It established concrete UI route and ownership problems later addressed in source. No gate promotion.

Metadata: `docs/research/evidence/20260920T152316Z-48dab54366d5.json`.

### `20260920T090448Z-b1f54e3cb38e` — useful full/body run

Clean single-process run that physically exercised startup VR presentation through a level load. The user saw the game in VR from startup, Create recenter worked, the SteamVR dashboard did not remain stuck, loading survived and gameplay reached native stereo. Local Ray/Billy head/hair suppression worked visually.

The same run rejected the then-current menu activation path and Body IK anatomy. It also continued to show incomplete inner presenter shutdown.

Metadata: `docs/research/evidence/20260920T090448Z-b1f54e3cb38e.json`.

### `20260919T153546Z-705460dca03b` — snap/body evidence

Clean single-process run. The user confirmed exact right-stick snap turning; telemetry recorded 23 exact native ±45° turns. Body IK was substantially better than earlier catastrophic deformation, but reach/wrist anatomy remained unacceptable. This run is the physical authority for snap turn, not for completed Body IK.

Metadata: `docs/research/evidence/20260919T153546Z-705460dca03b.json`.

### Older retained stereo evidence

The following historical runs remain authoritative only for specific contracts:

- `20260916T153109Z-8976b8f77775`: correct 100 units/metre eye baseline and Create recenter.
- `20260916T221254Z-861f3c15abd4`: scene-focus/lifecycle correction and clean outer runtime stop/run end.
- `20260916T224239Z-e43b46698e5c`: explicit render pose removed the previous head-turn pull/snap-back.

Their large local logs/videos do not need to remain in the repository workspace once these conclusions are preserved.

## Evidence rules

### Physical candidate

Use:

```powershell
pwsh -File tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez" -BodyIkAtStart
# Start SteamVR and Call of Juarez manually and perform the gate.
pwsh -File tools/vr_test.ps1 finish
```

The first game directory is remembered in ignored local state. `prepare` builds/tests and stages a provenance-bound candidate. `finish` verifies/collects available evidence and restores staging/video settings.

Never launch or terminate SteamVR or Call of Juarez automatically.

### Run identity

A staged run ID may contain only one game process. If the game is relaunched, finish/clear the previous candidate and prepare a new one. Historical multiprocess runs are diagnostic only.

### Evidence retention

Keep compact formal metadata in `docs/research/evidence/` when it still supports a current contract. Large runtime logs, packaged evidence, extracted video frames and copied test media are disposable after their useful conclusions are recorded in source/docs and no unresolved analysis depends on them.

### Promotion

Do not describe a feature as physically validated merely because a verifier accepts telemetry. For UI, comfort, anatomy and interaction gates, the user's direct in-headset observation is part of acceptance.
