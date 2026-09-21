# Technical audit — current baseline

Last refreshed: 2026-09-21.

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

The newest physical diagnostic is complete single-process run `20260921T192639Z-1d70905cb4f8`, correlated with `clip_1.790.022.783.959.mp4`. The sole Win32 pointer is still unusable, locomotion/jump remain unacceptable after the shipped analog controller-state transaction, arm safety produces visible fallback to native poses, and weapon shots remain wrong. At the same time, the run proves two useful technical facts: horizontal-only visual-pelvis compensation now has zero sampled Y displacement, and Sense grip-to-tip geometry strongly identifies local `-Z` as the tip direction. These narrow the remaining failures without promoting playability.

The current source is the physically rejected candidate from that run and still passes Debug and Release at **24 PASS + 1 expected classic-D3D9 shared-texture capability SKIP**. Host success therefore cannot be treated as evidence of UI, locomotion, anatomy or aiming quality.

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

The dual-owner Java+Win32 route could select but was chaotic. The next candidate removed Java ownership and left only `SetCursorPos` + absolute `SendInput` + `WM_MOUSEMOVE`; run `20260921T192639Z-1d70905cb4f8` still found the pointer uncontrollable and required the physical mouse. Pointer instability therefore is not explained solely by competing owners. Treat the current projection/input ownership model as unresolved rather than stacking another cursor route on top.

Circle used normal Escape only while a current menu UI existed. Native-stereo gameplay had no current UI and therefore no usable exit path. Current host source retains `GameUserInterface.CallOnInputKeyGlobal(1, ...)` for existing UI and falls back to `LawmanGame.sm_cActiveGameModule.OnInputKey(1, ...)` so the shipped module creates the pause menu. Cross remains accept, L2/R2 remain ray-select, and `GameUILoading.OnInputKey(IZC)V` owns the blocking post-load continue path.

The same run produced 202 `java.lang.NoSuchFieldError: m_cHintManager` exceptions from direct inherited-field lookup in the paused-hint helper. Current source uses the shipped `LawmanModule.GetHintManager()` accessor before `HintManager.DisableCurrentHint`, avoiding that known exception route. These UI changes are host-tested only.

This remains an open UI architecture/debugging problem rather than a host-only gate.

### Subtitles

Retained diagnostic run `20260920T235112Z-6b2d91cda4a5` recorded 376 subtitle states with `Settings.bSubtitles=false`. `DialogSubtitle.Show(String)` exits when that setting is false, so that run explains its subtitle absence without evidence of a layering/capture defect. Enable subtitles through the shipped setting before judging presentation; do not force-enable or relocate them in the adapter.

### Shared playability stutter

The latest complete-process transport sample, `20260921T192639Z-1d70905cb4f8`, measured D3D9 CPU copy at 7.112 ms median, 9.056 ms p95 and 11.849 ms maximum while rendering/capturing 1920x1080 per eye. At 120 Hz the entire frame budget is about 8.33 ms, so classic-D3D9 GPU->CPU readback remains a measured structural bottleneck.

Locomotion again reached `0.999992`, and 126 sampled updates explicitly reported the shipped `LockApplyControllerState -> Translate -> UnlockApplyControllerState -> ApplyControllerState` transaction. That correction did not make locomotion acceptable, so missing transaction semantics are ruled out as the primary cause. Five sampled jump states also reach exact native digital action 11, yet the observed hop is only about 2 cm. The next investigation must measure game-space speed/jump displacement and renderer cadence separately instead of continuing guess-and-check input changes.

OFXR-Bridge is not a direct fix for this active path. It is an experimental OpenXR API layer that inserts optical-flow-generated frames between OpenXR submissions; the current backend is classic D3D9 + OpenVR and pays the CPU readback before compositor submission. OFXR therefore cannot remove this D3D9 readback or recover source detail that was never rendered. Keep it as future OpenXR/frame-generation research after a GPU-resident transport exists.

### Recenter level reference

Earlier evidence visibly showed a rolled horizon. The old recenter tracker captured the complete HMD basis, so pitch/roll present during recenter could become part of relative tracking space. Current host source derives the recenter reference from gravity plus HMD yaw only; a tilted-recenter regression test covers this boundary.

### Body anatomy

The exact visible writer is proven live, but current safety ownership is not usable. In the latest run, 43/128 sampled arm updates denied VR writes and 32 were reach-unsafe; 80 samples rejected hand residuals. The user sees this as the arms repeatedly snapping back to the default game pose while moving the Sense controllers. The safety guard is doing its fail-closed job, but the remaining anatomy problem is now continuity/ownership as much as raw rotation limits.

### Weapon origin and direction

Static bytecode and host tests establish distinct direction/visual-origin/ballistic-origin ownership. `InputDigital.Translate` selects the hand/fire state and the actual native attack later follows `OnHandStateStarted_Attack -> WeaponAttack -> Weapon.Attack`, where `GetFireOriginForWeapon` reads `Being.m_vLookFromPoint`. Full method-order inspection further establishes that `ArmedPlayerBeing.OnBeingsFrame()` executes `UpdateHandStates` before `OnPostBeingsUpdate()` later executes `UpdateLookAndAimDirs` and `UpdateLookAndAimPoints`; the native post-update recomputation therefore cannot erase the published fire transition before `WeaponAttack`. Translation failure rolls back the captured native value.

The newest run resolves the simple Sense-axis question: 120 grip-to-tip comparisons put local `-Z` at `0.939388..0.939389`, and 21 fire transitions successfully publish varying controller directions/origins. Shots are still visibly wrong, so a guessed controller-axis offset is no longer justified. Reintroduce a temporary controller-tip ray only as a visual diagnostic, then compare that ray with the visible weapon/barrel and trace the mismatch through weapon visual transform, muzzle origin and ballistic ownership. Production shots must originate from the visible barrel/muzzle.

### Physical crouch

The previous full-XYZ pelvis error is now technically corrected: 64 sampled room-scale writes in the latest run all had zero vertical world offset and restored after both eyes. The new gap is animation ownership. Physical horizontal HMD displacement moves the visual body anchor but does not trigger the walking animation expected from stick movement. Add this as a distinct body requirement while keeping native actor position, grounding and collision authoritative.

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
