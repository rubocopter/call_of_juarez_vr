# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`.

## Current gate

The immediate physical gate is the host-tested D3D9Ex shared-texture transport.
It must preserve current stereo/tracking while replacing the old per-frame CPU
readback and raising native-stereo cadence above the approximately 83 Hz
baseline toward the 138 Hz readback-off reference.

Three separate playability areas remain open after that transport gate:

- menu pointer ownership and controller-only UI usability;
- continuous tracked-arm ownership without visible fallback or deformation;
- weapon/barrel origin and direction alignment.

Native movement speed and jump amplitude/duration now match vanilla in physical
measurements; locomotion/input/physics investigation is closed. Independent
stereo, tracking, recenter and snap-turn contracts remain the stable baseline.

## Stabilization baseline

Audit-remediation work established the foundation now used by the project:

| Area | Current state |
| --- | --- |
| Provenance and exact-build identity | host-tested and used by the physical workflow |
| Build/test validity | host-tested |
| Safe hook ownership and restoration | host-tested, physically exercised |
| Factory/device/generation identity | host-tested, physically exercised |
| Capture/presenter separation | host-tested, physically exercised |
| OpenVR state/lifecycle ownership | physically exercised; inner presenter shutdown still open |
| Transactional stage/finish workflow | host-tested and used for physical runs |
| Neutral VR math contracts | host-tested and exercised by stereo/recenter paths |

Historical phase-by-phase remediation and per-run chronology are intentionally not retained here.

## Milestone 1 — native stereo and HMD

- Camera -> view/projection -> renderer path: **live-tested**.
- Complete two-eye ChromeEngine render path with real color targets: **live-tested**.
- Correct physical eye scale: **headset-validated**.
- HMD yaw/pitch/roll and positional camera offset: **headset-validated**.
- Explicit render-pose submission for head-turn stability: **headset-validated**.
- Controller recenter: **headset-validated**.
- Startup/loading flat theater and transition to native stereo: **headset-validated for the exercised path**.
- Frame pacing/transport cost: **host-tested candidate / physical gate**; D3D9Ex shared DEFAULT textures and nonblocking steady-state D3D9/D3D11 queries eliminate native-stereo CPU readback on the preferred path. A bounded D3D11 completion barrier exists only for draining pending copies during shutdown.
- Inner presenter shutdown/finalization: **open regression**.

## Milestone 2 — player body and comfort

- Exact campaign player discovery: **live-tested**.
- Room-scale camera translation with native actor position/grounding preserved: **host-tested follow-up after physical rejection**.
- HMD/body-yaw comfort ownership: **host-tested follow-up**.
- Exact snap turn: **headset-validated**.
- Native analog locomotion through the shipped float-input path: **headset-validated diagnostically** for vanilla-equivalent normal/walk speed. No further movement/input changes are planned.
- Native jump action: **headset-validated diagnostically** for vanilla-equivalent apex and duration. No further jump/physics changes are planned.
- Horizontal-only visual body room-scale overlay with native vertical actor/grounding/collision ownership: **live-exercised technically**. Physical displacement still needs stick-equivalent visual locomotion animation.
- Physical-walk visual animation from HMD horizontal movement: **planned/open**.
- Local Ray/Billy head/hair suppression: **headset-validated for HMD view**; shadow behavior remains unverified.

## Milestone 3 — tracked hands and body IK

- Stable left/right Sense tracking in game space: **live-tested**.
- Exact visible arm writer and verified restoration: **live-tested**.
- Correct head-relative controller target space: **live-tested**.
- FORETWIST/hand hierarchy correction: **live-tested technically; visual anatomy remains incomplete**.
- Arm reach/anatomy/orientation: **live-exercised / physically rejected for continuity**. Safety must remain fail-closed without causing visible repeated fallback to default animation.
- Native reload-animation ownership: **host-tested follow-up**.
- Pelvis/legs full-body writing: **planned**, blocked on acceptable arm/body ownership first.

## Milestone 4 — controller UI and interactions

- PS VR2 Sense action/binding layer: **live-tested** for gameplay actions; recenter/snap have higher validation.
- Flat-menu pointer ownership: **live-exercised / physically rejected**. Replace or isolate the current model rather than stacking another competing cursor owner.
- Cross accept / Circle Escape-back / L2-R2 ray-select: **host-tested follow-up**.
- Loading continuation through the native loading-input boundary: **host-tested**.
- Subtitle visibility: **open physical check**; distinguish shipped setting state from presentation loss.
- Controller-origin flat-theater beam: **host-tested**.
- Controller-owned per-hand weapon direction and visual origin: **host-tested**.
- Ballistic-origin ownership across the native attack transition: **host-tested**.
- Sense tip direction convention: **live-tested diagnostically**; local `-Z` is the demonstrated pointing direction.
- Temporary controller-tip alignment ray: **planned diagnostic**. Use only to compare tracked direction with the visible weapon/barrel.
- Physical gun-origin/direction acceptance: **pending/rejected**.
- Motion-controlled reloads and richer world interactions: **planned**.

## Milestone 5 — additional renderers and games

- OpenXR/frame-generation experiments: **future research only**; they do not replace validation of the active D3D9Ex/OpenVR shared-texture path.
- D3D10 renderer path for Call of Juarez: **planned**.
- Bound in Blood integration: **planned**.
- Gunslinger integration: **planned**.
- Promotion of shared Chrome Engine contracts: **blocked until a second game independently proves them**.

## Release direction

A public release requires the reference backend to clear the controller-only UI, body IK, weapon alignment, presentation-performance and shutdown gates in representative gameplay without regressing validated stereo, tracking or native locomotion behavior.
