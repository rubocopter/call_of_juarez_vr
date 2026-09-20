# Validation

Validation states are intentionally strict:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

A higher state requires direct evidence for that boundary. Static disassembly, source existence, CTest and a successful build cannot substitute for a physical acceptance gesture.

## Current repository gate — 2026-09-21

The current working tree is the follow-up candidate after multiprocess diagnostic rejection `20260920T214629Z-351c27f434d5`. It has not been tested in-headset.

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
| Menu pointer | beam starts at Sense controller; hovered item follows ray through the real mouse/hit-test path without physical mouse movement |
| Menu actions | Cross accept, Circle normal Escape/back without layer loss/freeze, L2/R2 ray-select |
| Blocking UI | post-load continue responds to Sense through `GameUILoading.OnInputKey` without actor/body mutation |
| Subtitles | visible when enabled; if absent, establish runtime `Settings.bSubtitles` state and subtitle-show execution before changing layering |
| Recenter/horizon | recenter leaves the world level even if the HMD has pitch/roll at the recenter instant |
| Locomotion | walk/run/jump smoothness with Sense and keyboard; preserve separate evidence for native input semantics versus D3D9 readback/frame pacing |
| Crouch | physical HMD drop preserves first-person local-mesh ownership while native crouch remains false; explicit controller crouch still works |
| Body IK | arms remain stable/restorable; verify shoulder/reach, hand orientation and reload pose separately |
| Weapon | repeated shots remain process-stable and controller/weapon visual origin, ballistic origin and direction are correct |
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
| Analog locomotion through native float actions | host-tested; physical smoothness rejected in previous candidate | current source retains 0.04 per-axis native shaping |
| Reduced routine Body IK telemetry | host-tested | introduced after `20260920T161307Z-474f0b054338` |
| Sampled eye distinction + recycled CPU frame storage | host-tested | reduces current CPU path cost; physical smoothness still pending |
| Yaw-only/gravity-level recenter reference | host-tested | regression covers tilted-HMD recenter; physical horizon check pending |
| Native flat-menu cursor route | host-tested | logical global `SetPos` + `OnMouseMove` plus Win32 `SetCursorPos`/`WM_MOUSEMOVE` for shipped hover hit-testing |
| Cross accept / Circle back / L2-R2 select | host-tested | Circle now uses active-UI Escape press/release; destructive `ShowPrevUI` route removed |
| Loading continue via `GameUILoading.OnInputKey` | host-tested | current post-run source only |
| Paused-hint manager lookup | host-tested | shipped `LawmanModule.GetHintManager()` accessor replaces direct `m_cHintManager` lookup that emitted 202 Java exceptions |
| Controller-origin flat beam | host-tested | current post-run source only |
| Subtitle visibility | rejected / unresolved | latest test showed none; runtime setting versus UI/presentation loss not yet distinguished |
| Physical crouch detection without automatic native crouch | host-tested; physical ownership rejected | latest run still exposed full avatar with `physical_crouch=true`, native `crouch=false` |
| Visible arm writer/restoration | live-tested | exact writer changes geometry and restores; visual anatomy still fails |
| Body IK anatomy/reach/orientation | rejected / unpromoted | native chain ~49.843 units; positional targets can be reached while hand orientation/reload still fail |
| Controller-owned weapon direction/visual origin | host-tested | physical firing still required |
| Scoped ballistic-origin override | host-tested | native `Being.m_vLookFromPoint` is captured and restored around fire input translation only |
| Gameplay diagnostic shot ray | removed | no runtime `LaserPointer` object is created by the firing path |
| Outer runtime shutdown | live-exercised | recent runs reach `run_end` |
| Inner presenter finalization | open | recent body/playability evidence reports `shutdown_complete=false` |
| Supported end-to-end VR release | planned | pre-alpha |

## Latest physical evidence

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
