# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`.

## Current gate

The newest physical evidence, run ID `20260921T192639Z-1d70905cb4f8`, is a complete **single-process diagnostic rejection**. The sole Win32 menu-pointer path remains uncontrollable and the user had to use the mouse. Movement still reaches `0.999992` after the shipped analog controller-state transaction was restored, but walking is unacceptable and jump height is only about 2 cm. Horizontal-only visual-body compensation now keeps all sampled pelvis Y offsets at zero, yet physical room-scale walking does not drive the same walk animation as stick locomotion. Arm safety frequently returns control to native animation, producing visible arm resets. Sense grip-to-tip geometry strongly confirms local `-Z`, but shots still do not originate/travel correctly from the weapon. No playability gate is promoted.

The current source was the candidate exercised by that run. Host verification remains **24 PASS + 1 expected SKIP** in Debug and Release; headset evidence now overrides any host-only optimism for the rejected areas.

The next physical candidate must prove in one process:

- startup/menu scene ownership and reliable startup skip;
- a stable menu-pointer ownership model that does not require the physical mouse and does not oscillate under Sense motion;
- Cross accept, Circle normal Escape/back without UI corruption, and L2/R2 ray-select;
- post-load `GameUILoading.OnInputKey` continuation from Sense;
- visible subtitles with the `Settings.bSubtitles`/subtitle-layer cause established if still absent;
- level horizon after recenter;
- correct walk/run speed and useful jump amplitude, with native movement semantics separated from measured D3D9 readback/frame-pacing cost;
- physical room-scale displacement with horizontal body compensation **and** stick-equivalent visual locomotion animation while actor grounding/collision remain native;
- continuous tracked-arm ownership without unsafe deformation or repeated snap-back to the default game pose;
- repeated-fire process stability plus correct weapon-barrel visual/ballistic origin and direction; use a temporary controller-tip gameplay ray to compare tracked direction against the visible weapon before changing ballistics;
- normal transition between flat theater and native stereo;
- normal shutdown, with inner presenter finalization specifically checked.

## Stabilization baseline

Audit-remediation phases 0-8 established the foundation now used by the project:

| Area | Current state |
| --- | --- |
| Provenance and exact-build identity | host-tested and used by physical workflow |
| Build/test validity | host-tested; current Debug/Release suites green |
| Safe hook ownership and restoration | host-tested, physically exercised |
| Factory/device/generation identity | host-tested, physically exercised |
| Structured telemetry/run correlation | host-tested, physically exercised |
| Capture/presenter separation | host-tested, physically exercised |
| OpenVR state/lifecycle ownership | physically exercised; inner presenter shutdown still open |
| Transactional stage/finish workflow | host-tested and used for physical runs |
| Neutral VR math contracts | host-tested and exercised by stereo/recenter paths |

The old phase-by-phase implementation backlog is complete enough to serve as baseline; new work is tracked by product gates below.

## Milestone 1 — native stereo and HMD

- Camera -> view/projection -> renderer path: **live-tested**.
- Complete two-eye ChromeEngine render path with real color targets: **live-tested**.
- Correct 100 game-units/metre physical eye baseline: **headset-validated**.
- HMD yaw/pitch/roll and positional camera offset: **headset-validated**.
- Explicit render-pose submission for head-turn stability: **headset-validated**.
- Left-Sense Create recenter: **headset-validated**.
- Startup/loading flat theater and transition to native stereo: **headset-validated for the exercised path**.
- Frame pacing/transport cost: **open structural blocker**; latest gameplay evidence measured classic-D3D9 CPU copy at 7.112 ms median / 9.056 ms p95 for 1920x1080 per eye. Prioritize a GPU-resident/lower-copy transport before a large resolution increase.
- Inner presenter shutdown/finalization: **open regression** on recent runs.

## Milestone 2 — player body and comfort

- Exact campaign player discovery: **live-tested**.
- Room-scale camera translation with native actor position/grounding preserved: **host-tested follow-up after physical rejection**.
- HMD/body-yaw comfort-cone ownership: **host-tested follow-up after physical rejection**.
- Exact ±45° right-stick snap turn: **headset-validated**.
- Native analog locomotion using shipped 0.04 per-axis shaping, exact `InputSettings.GetTargetTypeForAction` target selection and shipped `LockApplyControllerState -> dispatch/Translate -> UnlockApplyControllerState -> ApplyControllerState`: **live-exercised / physically rejected for playability**. The transaction is present, so more blind input-routing changes are not justified.
- Jump digital action 11: **live-exercised / physically rejected for amplitude**; input reaches the game but the observed hop is only about 2 cm.
- Reduced synchronous Body IK telemetry overhead: **host-tested**, but latest physical evidence remained jerky; renderer transport and native movement semantics now need separate work.
- Horizontal-only visual body room-scale overlay with native vertical actor/grounding/collision ownership and post-stereo restoration: **live-exercised technically**; sampled vertical offset is zero. Physical displacement still lacks the walk animation expected from stick locomotion.
- Physical-walk visual animation from HMD horizontal velocity/displacement: **planned/open**; should reuse the game's locomotion animation semantics without moving the native collision actor solely for room scale.
- Local Ray/Billy head/hair suppression: **headset-validated for HMD view**; shadow behavior remains unverified.

## Milestone 3 — tracked hands and body IK

- Stable left/right Sense tracking in game space: **live-tested**.
- Exact visible arm writer and verified restoration: **live-tested**.
- Correct head-relative controller target space: **live-tested**.
- FORETWIST/hand hierarchy correction: **live-tested technically; visual anatomy still rejected**.
- Current arm reach/anatomy/orientation: **live-exercised / physically rejected for continuity**. Safety gates prevent some unsafe writes, but 43/128 sampled arm updates fell back to native animation and the user sees repeated arm resets while moving the Sense controllers. The solver must become continuously plausible instead of alternating between VR and default poses.
- Native reload-animation ownership: **host-tested follow-up**; VR arm writes yield while `ArmedPlayerBeing.IsWeaponReloading()` is true. Clavicle/shoulder participation remains a planned controlled investigation.
- Pelvis/legs full-body writing: **planned**, blocked on acceptable arm/body ownership first.

## Milestone 4 — controller UI and interactions

- PS VR2 Sense action/binding layer: **live-tested** for gameplay actions; recenter/snap have higher validation.
- Flat-menu pointer via sole Win32 `SetCursorPos` + absolute `SendInput` + `WM_MOUSEMOVE` ownership: **live-exercised / physically rejected**. It reaches the menu but remains uncontrollable; the user returned to the mouse.
- Cross accept / Circle Escape-back / L2-R2 ray-select: **host-tested follow-up**; Circle now falls back to `LawmanModule.OnInputKey(Escape)` from gameplay when no UI is open.
- Loading continuation via `GameUILoading.OnInputKey`: **host-tested**.
- Paused-hint lookup via `LawmanModule.GetHintManager`: **host-tested** after the latest run exposed 202 direct-field `NoSuchFieldError` exceptions.
- Subtitle visibility: **rejected / diagnostic pending**; distinguish runtime setting from UI/presentation loss.
- Controller-origin flat-theater beam: **host-tested**.
- Controller-owned per-hand weapon direction and visual origin: **host-tested**.
- Ballistic-origin ownership across the native attack transition, with rollback on failed input translation and later `UpdateLookAndAimPoints` reclamation: **host-tested**.
- Gameplay diagnostic `LaserPointer`: **removed** after the first-shot physical failure.
- Repeated fire process stability: **diagnostically improved** in retained evidence; no promotion.
- Grip-to-tip aim-axis and fire-transition telemetry: **live-tested diagnostically**. Across 120 sampled axis comparisons, local `-Z` scores `0.939388..0.939389` against grip-to-tip displacement, strongly confirming the Sense tip direction convention.
- Temporary controller-tip gameplay alignment ray: **planned diagnostic**. Reintroduce it only to compare tracked direction with the visible weapon/barrel; it must not become the production ballistic path.
- Physical gun-origin/direction acceptance: **pending/rejected**.
- Motion-controlled reloads and richer world interactions: **planned**.

## Milestone 5 — additional renderers and games

- OFXR-Bridge / optical-flow frame generation: **future OpenXR research only**; it does not remove the active classic-D3D9/OpenVR CPU readback path.
- D3D10 renderer path for Call of Juarez: **planned**.
- Bound in Blood integration: **planned**.
- Gunslinger integration: **planned**.
- Promotion of shared Chrome Engine contracts: **blocked until a second game independently proves them**.
