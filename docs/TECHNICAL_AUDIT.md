# Technical audit — current baseline

Last refreshed: 2026-09-20.

This document describes the current engineering baseline and the findings that still matter. Historical investigation details were intentionally removed once their conclusions became enforced contracts or retained evidence.

## Evidence policy

A claim advances only as far as its evidence:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

Exact game integration is SHA-256 gated. One staged run ID belongs to one game process. Physical promotion requires source/build/deployment/run correlation and the acceptance gesture for that gate.

## Current baseline

The repository has moved beyond the original blank-headset/bootstrap problem. Current verified capabilities include:

- exact Call of Juarez camera -> view/projection -> ChromeEngine renderer ownership;
- complete left/right native D3D9 game renders and SteamVR submission;
- correct 100 game-units/metre stereo baseline;
- HMD orientation and positional camera offset;
- explicit render-pose submission that removed the previous head-turn pull/snap-back artifact;
- flat-theater startup/loading presentation and return to native stereo;
- left-Sense Create recenter and exact ±45° snap turn;
- live campaign-player discovery and tracked Sense poses;
- exact visible arm mutation/restoration against the live skeleton;
- physically validated local head/hair suppression for the exercised Ray/Billy path.

The latest physical playability evidence is run ID `20260920T214629Z-351c27f434d5`. It is a **multiprocess diagnostic rejection** because the same staged run ID was reused by PIDs 12048, 8600 and 4688 after Circle/back failures forced relaunches. Sense input skipped the intro videos, but subtitles were invisible, menu hover still did not follow the controller ray, Circle could corrupt/freeze the menu presentation, locomotion and jump remained unacceptable, physical crouch exposed the full avatar, and arm/reload anatomy remained rejected. The gameplay process recorded repeated fire input and reached outer `run_end`, so the previous first-shot termination was not reproduced, but weapon origin/direction still looked wrong. No gate is promoted from this evidence.

The current source contains follow-up fixes for those observations and passes current Debug and Release suites at **24 PASS + 1 expected classic-D3D9 shared-texture capability SKIP**. These changes are host-tested only.

## Audit findings

| Finding | Current status | Remaining concern |
| --- | --- | --- |
| Provenance was too weak for promotion | Resolved baseline | Keep exact source/build/deploy/run correlation mandatory |
| Vtable/hook changes were not transactionally safe | Resolved baseline | Preserve foreign-hook detection and restoration checks |
| D3D9 factory/device identity could split or bypass hooks | Resolved baseline | Continue generation/device identity telemetry |
| Capture and OpenVR presentation were coupled in game callbacks | Resolved baseline | Maintain mailbox/presenter separation |
| Renderer resources were not tied strongly enough to device/generation | Resolved baseline | Reset/new-device recovery remains a required host test |
| Host tests did not prove the staged pipeline | Resolved baseline | `prepare` must build/test/provenance-bind the staged candidate |
| Multiple processes could reuse one run ID | Resolved in workflow | Historical multiprocess runs remain diagnostic only |
| Deployment restoration was happy-path oriented | Resolved baseline | `finish` must restore candidate and video settings transactionally |
| Source architecture and build dependencies drifted | Resolved baseline | Keep exact-game code in the game adapter |
| Runtime/lifetime recovery was incomplete | Partially open | Recent physical runs still report incomplete inner presenter shutdown |
| Neutral camera/math contracts were ambiguous | Resolved baseline | Keep right/up/forward source basis and exact unit conversion |
| Documentation accumulated chronological state and stale objectives | Addressed by current cleanup | Keep docs state-based; retain compact evidence metadata rather than narratives |

## Active technical risks

### UI ownership

The latest physical route drew a usable Sense beam but did not move the highlighted menu option with hover. Moving the real mouse immediately updated hover, proving that moving the `UICursorGame` sprite is not sufficient. Static bytecode inspection agrees: `UICursor.OnMouseMove(FFI)` moves the cursor visual, while window hit-testing/process-mouse paths consume the engine/Win32 mouse position. Current host source therefore keeps `UICursorGame.SetPos(LVector;)V` + `OnMouseMove(FFI)V` for logical synchronization and also publishes the projected ray through Win32 `SetCursorPos` + `WM_MOUSEMOVE` to the game window.

Circle was also bound to the wrong semantic route. `MainMenuModule.ShowPrevUI()` is not equivalent to normal Escape and matched the observed layer disappearance/frozen-flat behavior. Current host source dispatches an Escape press/release through the active `GameUserInterface.CallOnInputKeyGlobal(1, ...)` path instead. Cross remains accept, L2/R2 remain ray-select, and `GameUILoading.OnInputKey(IZC)V` owns the blocking post-load continue path.

The same run produced 202 `java.lang.NoSuchFieldError: m_cHintManager` exceptions from direct inherited-field lookup in the paused-hint helper. Current source uses the shipped `LawmanModule.GetHintManager()` accessor before `HintManager.DisableCurrentHint`, avoiding that known exception route. These UI changes are host-tested only.

This is the next physical UI gate.

### Subtitles

Subtitles were not visible in the latest headset test. `DialogSubtitle.Show(String)` first checks `Settings.bSubtitles`, then creates/positions the subtitle text and moves it to the top of its UI layer. The options menu writes `Settings.bSubtitles` and saves settings. The evidence does not yet establish whether the runtime flag was false or whether valid subtitle UI was lost by layering/capture. Do not force-enable or relocate subtitles until the next diagnostic distinguishes those cases.

### Shared playability stutter

Run `20260920T161307Z-474f0b054338` contained 11,192 successful `body_arm_tracking` and 11,192 successful `body_arm_restore` records. The latest gameplay process still felt severely jerky after telemetry/hash reduction and CPU-buffer reuse. Its 70 sampled D3D9 CPU copies measured 7.432 ms median, 8.994 ms p95 and 9.644 ms maximum while rendering/capturing 1920x1080 per eye; SteamVR recommended 3400x3468. At 120 Hz the entire frame budget is about 8.33 ms, so classic-D3D9 GPU->CPU readback is now a measured structural bottleneck rather than merely a logging hypothesis.

Locomotion input itself reaches the game as continuous native action values and jump presses are recorded, so movement semantics still need separate investigation. Do not remove walk animation or alter jump physics merely from appearance until their exact native route is established.

OFXR-Bridge is not a direct fix for this active path. It is an experimental OpenXR API layer that inserts optical-flow-generated frames between OpenXR submissions; the current backend is classic D3D9 + OpenVR and pays the CPU readback before compositor submission. OFXR therefore cannot remove this D3D9 readback or recover source detail that was never rendered. Keep it as future OpenXR/frame-generation research after a GPU-resident transport exists.

### Recenter level reference

The latest clip visibly showed a rolled horizon. The old recenter tracker captured the complete HMD basis, so pitch/roll present during recenter could become part of relative tracking space. Current host source derives the recenter reference from gravity plus HMD yaw only; a tilted-recenter regression test covers this boundary.

### Body anatomy

The exact visible writer is proven live, but visual anatomy is not. The latest clip still shows arms that feel short and visibly deform/contort, especially during reload. Telemetry frequently reaches elbow/wrist positional targets with tiny residuals while `hand_orientation_reached=false` and orientation errors remain large. Many sampled controller targets are within the measured ~49.843-unit upper+forearm reach, so the data does not support blindly scaling both bones. Clavicle/shoulder participation and conflict between the native reload animation and the VR writer are the next controlled anatomy questions.

### Weapon origin and direction

Static bytecode and host tests establish distinct direction/visual-origin/ballistic-origin ownership. The earlier run `20260920T204507Z-6db13f3107e0` ended on the first shot attempt before post-input fire telemetry was emitted, motivating transactional ownership of global `Being.m_vLookFromPoint`. Current source captures the native value, overrides it only for the synchronous fire `InputDigital.Translate` call and immediately restores it; the gameplay `LaserPointer` diagnostic path is removed.

In the latest gameplay process, 55 fire-pressed samples were recorded and the process reached normal outer `run_end`, so the previous first-shot termination was not reproduced. This is useful stability evidence but not an aiming promotion: the user still rejected shot origin/direction. The next investigation must identify whether the remaining error is visual origin, ballistic origin, per-hand direction or call ordering.

### Physical crouch

The latest physical run disproves the previous double-crouch explanation. Telemetry recorded `physical_crouch=true` while native `crouch=false`, yet lowering the HMD still exposed the full local avatar. The remaining problem is first-person camera/model ownership: room-scale vertical head motion can move the camera into/through body geometry while only the local head/hair are suppressed. Explicit controller crouch still uses the native action. Any mesh-visibility change must preserve tracked arms, shadows and the future full-body path.

### Presenter finalization

Several earlier runs achieved clean outer runtime stop and `run_end`, but recent body/playability runs still report `shutdown_complete=false` for the inner presenter. Do not collapse these signals into a single "clean shutdown" claim.

## Exact game findings that remain authoritative

- Full render-view wrapper: `0x00030FB0`; core `0x00030E00` alone does not produce a valid second-eye render.
- Camera source layout: right/up/forward/position with paired source/view state.
- World/player units are centimetres; XR tracking is metres.
- Call of Juarez `InputAnalog` uses 0.04 per-axis shaping for the native movement route.
- `RotateElementWithChildren(ILVector;F)V` composes element-local rotation and updates descendants/attachments.
- FORETWIST is effectively a sibling of forearm for the observed model; hand follows forearm and ignores FORETWIST roll.
- Ordinary local ballistic origin passes through `Being.m_vLookFromPoint`; per-hand visualization uses `m_avAimFromPoint`; per-hand direction uses the native look-direction array.

See `docs/research/COJ_CAMERA_PATH.md` and `docs/research/COJ_ARM_SKINNING_AND_AIM.md` for the compact research record.

## Current acceptance boundary

The repository is no longer blocked on discovering whether native stereo is possible. It is blocked on making the current exact-game VR ownership usable and physically correct: controller UI, subtitles, GPU/CPU presentation cost and smooth locomotion/jump, first-person crouch/body ownership, weapon origin/direction, arm/reload anatomy and lifecycle finalization.
