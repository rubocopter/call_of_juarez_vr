# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`.

## Current gate

The latest physical evidence, run ID `20260920T214629Z-351c27f434d5`, is a **multiprocess diagnostic rejection** because the same staged ID was reused across three CoJ launches after Circle/back failures. Intro skipping worked, but subtitles, native menu hover, safe Circle/back behavior, locomotion/jump, first-person crouch, arm/reload anatomy, aiming and image quality failed. Repeated firing no longer terminated the gameplay process, but no firing/aiming gate is promoted.

The current working tree contains follow-up fixes and passes Debug and Release with **24 PASS + 1 expected SKIP**. Those changes have not yet been tested in-headset.

The next physical candidate must prove in one process:

- startup/menu scene ownership and reliable startup skip;
- Sense-controller-origin beam and native menu hover through the real mouse/hit-test path;
- Cross accept, Circle normal Escape/back without UI corruption, and L2/R2 ray-select;
- post-load `GameUILoading.OnInputKey` continuation from Sense;
- visible subtitles with the `Settings.bSubtitles`/subtitle-layer cause established if still absent;
- level horizon after recenter;
- smooth walk/run/jump with native movement semantics separated from measured D3D9 readback/frame-pacing cost;
- physical crouch with correct first-person local-mesh ownership while native crouch remains unpressed;
- stable arm behavior with plausible shoulder/reach, hand orientation and reload poses;
- repeated-fire process stability plus correct controller/weapon visual origin, ballistic origin and direction;
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
- Frame pacing/transport cost: **open structural blocker**; latest gameplay evidence measured classic-D3D9 CPU copy at 7.432 ms median / 8.994 ms p95 for 1920x1080 per eye, while SteamVR recommends 3400x3468. Prioritize a GPU-resident/lower-copy transport before a large resolution increase.
- Inner presenter shutdown/finalization: **open regression** on recent runs.

## Milestone 2 — player body and comfort

- Exact campaign player discovery: **live-tested**.
- Room-scale camera translation with native actor position/grounding preserved: **host-tested follow-up after physical rejection**.
- HMD/body-yaw comfort-cone ownership: **host-tested follow-up after physical rejection**.
- Exact ±45° right-stick snap turn: **headset-validated**.
- Native analog locomotion using shipped 0.04 per-axis shaping: **host-tested / physically rejected for smoothness in the previous candidate**.
- Reduced synchronous Body IK telemetry overhead: **host-tested**, but latest physical evidence remained jerky; renderer transport and native movement semantics now need separate work.
- Physical crouch detection without automatic native crouch action: **host-tested**, but latest physical evidence still exposes the avatar; first-person mesh/camera ownership remains open.
- Local Ray/Billy head/hair suppression: **headset-validated for HMD view**; shadow behavior remains unverified.

## Milestone 3 — tracked hands and body IK

- Stable left/right Sense tracking in game space: **live-tested**.
- Exact visible arm writer and verified restoration: **live-tested**.
- Correct head-relative controller target space: **live-tested**.
- FORETWIST/hand hierarchy correction: **live-tested technically; visual anatomy still rejected**.
- Current arm reach/anatomy/orientation: **open**. Native chain is ~49.843 game units; latest telemetry can reach positional targets while hand orientation remains wrong and reload visibly deforms the arm. Do not simply lengthen both bones.
- Clavicle/shoulder participation and reload-animation ownership: **planned controlled investigations**.
- Pelvis/legs full-body writing: **planned**, blocked on acceptable arm/body ownership first.

## Milestone 4 — controller UI and interactions

- PS VR2 Sense action/binding layer: **live-tested** for gameplay actions; recenter/snap have higher validation.
- Flat-menu pointer via global `UICursorGame.SetPos` + `OnMouseMove` plus Win32 `SetCursorPos`/`WM_MOUSEMOVE`: **host-tested**.
- Cross accept / Circle Escape-back / L2-R2 ray-select: **host-tested**.
- Loading continuation via `GameUILoading.OnInputKey`: **host-tested**.
- Paused-hint lookup via `LawmanModule.GetHintManager`: **host-tested** after the latest run exposed 202 direct-field `NoSuchFieldError` exceptions.
- Subtitle visibility: **rejected / diagnostic pending**; distinguish runtime setting from UI/presentation loss.
- Controller-origin flat-theater beam: **host-tested**.
- Controller-owned per-hand weapon direction and visual origin: **host-tested**.
- Synchronous fire ballistic-origin override with immediate native restore: **host-tested**.
- Gameplay diagnostic `LaserPointer`: **removed** after the first-shot physical failure.
- Repeated fire process stability: **diagnostically improved** in the latest invalid multiprocess evidence; no promotion.
- Physical gun-origin/direction acceptance: **pending/rejected**.
- Motion-controlled reloads and richer world interactions: **planned**.

## Milestone 5 — additional renderers and games

- OFXR-Bridge / optical-flow frame generation: **future OpenXR research only**; it does not remove the active classic-D3D9/OpenVR CPU readback path.
- D3D10 renderer path for Call of Juarez: **planned**.
- Bound in Blood integration: **planned**.
- Gunslinger integration: **planned**.
- Promotion of shared Chrome Engine contracts: **blocked until a second game independently proves them**.
