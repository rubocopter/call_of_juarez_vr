# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`.

## Current gate

**Immediate physical gate:** confirm per-hand firing and visible shot/impact
effects, then exercise explicit Triangle chords. The operator confirms recovered
pause selection, box pickup/carry/put-down with L1 and Triangle + R2 logs.
Shot effects and impact feedback remain invisible, and only L2 fires the right
pistol. The complete native controller route is now host-tested: action 10
converts to right-hand Attack(0), action 9 to left-hand Attack(1). Corrected
trigger ownership needs a fresh physical check. Native emitter creation/commit
is live-observed; visible effects remain rejected.
The candidate adds low-rate native FX global/draw-camera observations and last
committed handles. Emitter update, lifetime and actual pixels remain unverified;
this diagnostic addition does not fix or accept effect visibility.

The active presentation retains native torso/legs with independent native hands
at original proportions. Authored holding sockets share the hand rigid map;
parent hands move before child weapons, and restoration verifies both owners.
The connected-arm solver and enlarged reach fit remain diagnostic experiments
following physical anatomy rejection. Wrist comfort, grip cohesion and reload
recovery remain separate physical checks.

The current **host-tested** shot/effect follow-up retains native shot spread,
delivers fire after verified muzzle publication, and initializes detached
combustion/smoke with the complete measured barrel frame. Exact native research
shows the emitter create-call uses +Y/up while shipped barrel particles emit
along -X; the previous single-direction adaptation lost that relationship.
Actual bullets, tracers, flash, smoke and light still require headset acceptance.

The reversible graphics profile now preserves the chosen game resolution and
quality settings instead of forcing 1920x1080; FSAA 0 retains the demonstrated
shared-texture transport contract. Higher-resolution clarity and sustained HMD
cadence remain pending, including level loading. Repeated launches or graphics
changes within one run do not validate renderer reset or loading stability.

Native stereo, bounded configured-HMD cadence and normal-quit shutdown are
accepted for the reference build/host. Ordinary/modal/pause menus are accepted;
the operator confirms recovery of pause visibility and pointer selection.
Room-scale direction, physical crouch comfort, head-relative locomotion, the
90-degree snap step and hand recovery remain gesture-specific regression checks.
Follow the [combined validation procedure](VALIDATION.md#combined-menu-and-gameplay-regression).

## Stabilization baseline

Audit-remediation work established the foundation now used by the project:

| Area | Current state |
| --- | --- |
| Provenance and exact-build identity | host-tested and used by the physical workflow |
| Build/test validity | host-tested |
| Safe hook ownership and restoration | host-tested, physically exercised |
| Factory/device/generation identity | host-tested, physically exercised |
| Capture/presenter separation | host-tested, physically exercised |
| OpenVR state/lifecycle ownership | live-tested normal-quit owner shutdown and finalization; abnormal exit unproved |
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
- Frame pacing/transport cost: **performance-validated for bounded production cadence / sustained pacing open**; the shared-texture path approaches the readback-off reference with sampled native CPU readback/copy and producer/consumer waits zero. The operator confirms correct stereo depth and stable head turns. Pair production rate is separate from compositor submission/headset refresh, and ring drops/frame-age outliers remain. Production startup and normal shutdown are live-tested; pending-frame reset/device loss and sustained tail latency remain unproved. The shutdown-only D3D11 drain is outside steady-state presentation.
- Inner presenter shutdown/finalization: **live-tested normal quit**; exact pre-exit owner join, complete consumer-copy drain, capture cleanup, hook restoration and final summaries verified. Abnormal exit/device loss remain unproved.

## Milestone 2 — player body and comfort

- Exact campaign player discovery: **live-tested**.
- Room-scale camera translation with native actor position/grounding preserved: **host-tested follow-up after physical rejection**.
- HMD/body-yaw comfort ownership: continuous following at the existing 35-degree boundary is **headset-validated with Body IK disabled** for stable physical head turns.
- Exact snap turn mechanism: **headset-validated** at the previous step; the current 90-degree step is **host-tested** and awaits physical confirmation.
- Native analog locomotion through the shipped float-input path: **headset-validated diagnostically** for vanilla-equivalent normal/walk speed. No further movement/input changes are planned.
- Head-relative stick locomotion using residual HMD yaw before native shaping: **implemented / host-tested**, pending physical direction/feel confirmation.
- Native jump action: **headset-validated diagnostically** for vanilla-equivalent apex and duration. No further jump/physics changes are planned.
- Physical HMD-height crouch merged into the shipped crouch action: **live-exercised** for animation engagement; comfort remains rejected. Compensation of duplicate native camera descent, shared with arm/aim targets, is **host-tested** for the next candidate.
- Horizontal-only visual body room-scale overlay with native vertical actor/grounding/collision ownership: **live-exercised technically**. Physical displacement still needs stick-equivalent visual locomotion animation.
- Physical-walk visual animation from HMD horizontal movement: **planned/open**.
- Local Ray/Billy head/hair suppression: **headset-validated for HMD view**; shadow behavior remains unverified.

## Milestone 3 — tracked hands and body IK

- Independent native hands with retained torso/legs: **live-tested** native loading/presentation with substantial improvement; weapon gap and wrist pose rejected. The active **host-tested** follow-up adds authored holding sockets, parent-before-child weapon application, both-eye barrel verification and absolute unarmed grip orientation. Visual comfort, grip cohesion and reload recovery remain pending.

- Stable left/right Sense tracking in game space: **live-tested**.
- Exact absolute visible arm writer: **live-tested**; actor-relative restoration, stale-branch rebasing and pelvis/arm transaction nesting have physically improved continuity. Residual anatomy remains rejected.
- Correct head-relative controller target space: **live-tested**.
- FORETWIST/hand hierarchy correction: **live-tested technically; visual anatomy remains incomplete**.
- Connected-arm reach/anatomy/orientation: continuity is physically improved after actor rebasing, with hands no longer stranded in world space. Span calibration and the post-swing orientation reference are live-exercised, but enlarged limbs near the chest and unnatural wrists remain rejected. This presentation is superseded by independent native hands for the active candidate.
- Native reload-animation ownership: **host-tested follow-up**.
- Pelvis/legs full-body writing: **planned**, blocked on acceptable arm/body ownership first.

## Milestone 4 — controller UI and interactions

- PS VR2 Sense action/binding layer: **live-tested** for gameplay actions; recenter/snap have higher validation.
- Flat-menu pointer: native sprite-tree mouse events plus the exact `0xCC420` click route are **headset-validated for exercised menus and recovered Options pause**. The mono SYSTEMMEM reservation's allocation/Reset failure cases remain host-tested. Earlier Windows/sprite-only routes remain physically rejected.
- Cross accept / Circle UI-back / Options pause / L2-R2 ray-select: **headset-validated for exercised menus and recovered pause**; Circle kick execution still needs an eligible native context.
- Loading continuation through the native loading-input boundary: **host-tested**.
- Subtitle visibility: **operator-confirmed** for floating dialogue text; timing/readability and tutorial/context prompts remain separate checks.
- Controller-origin flat-theater beam: **live-exercised / visible in headset**; alignment and accurate controller-only UI operation remain open.
- Controller-owned per-hand weapon direction and visual origin: **host-tested**.
- Contextual F selection from the central HMD gaze: **headset-validated for box pickup/carry/put-down and pistol pickup**;
  exact local-player `CheckTriggers` getters preserve native range/permissions.
  Other devices and mounting still await physical acceptance.
- Ballistic-origin ownership across the native attack transition: **host-tested**.
- Sense tip direction convention: **live-tested diagnostically**; local `-Z` is the demonstrated pointing direction.
- Exact-frame per-eye gameplay reticle from the controller firing ray: **live-exercised / visible**. Impacts are reported closer to it; physical weapon/muzzle alignment remains rejected. It is independent from the menu pointer.
- Physical gun-origin/direction acceptance: **pending/rejected**.
- Shot/effect coherence: **physically rejected** for invisible effects/impact feedback. A host-tested follow-up commits the measured emitter starting frame into its active/previous world transforms via native FXDetach. Native attack/hit/FX counters add consumption evidence without replay or spread queries. Actual bullets, tracers, light and effects remain pending headset acceptance.
- Trigger-to-hand ownership: **host-tested follow-up after physical inversion**; R2 maps through PlayerController action 10 to Attack hand 0/right, L2 through action 9 to hand 1/left. Cross-hand retry is rejected only for a tracked one-hand firearm. Native two-hand/tool semantics remain separate acceptance checks.
- Motion-controlled reloads and richer world interactions: **planned**.
- Expanded PC campaign controls: **host-tested / physically incomplete**; L1 box pickup/carry/put-down, pistol pickup and Triangle + R2 logs are operator-confirmed. Focus and hands chords reached dispatch without accepted visible behavior. Broader interactions and other equipment remain unaccepted. Native one-shot delivery after the locomotion lock is a **host-tested** correction, preserving analog shaping and pending edges.
  The [exact PC catalogue](research/COJ_PC_CONTROLS_AND_HUD.md) records direct L1/F,
  separate Options/pause, the Triangle utility layer, focus toggle, alternate
  fire, put-away/discard, objectives/logs and six equipment intents. Save/load
  and Q/E lean remain deliberate custom bindings. The gesture selector has no
  visible inventory wheel or available-item filtering yet; XR focus magnification
  remains open.
- Gameplay HUD recovery: **incomplete**. Essential interaction/tutorial/subtitle
  text has a read-only native-owner route and cached finite-depth stereo panels,
  **host-tested**, with floating dialogue subtitles confirmed by the operator;
  tutorial/context visibility, pacing and broader headset readability remain open.
  Native PC key names remain in hint text. Exact sprite traversal and
  batch-flush boundaries are identified; their passive correlation/target probes
  are **host-tested**, with live route and usable coverage/alpha unproved.
  Native graphical capture remains planned. Restore health/ammo and native
  no-shoot feedback next. A native-inventory wheel and wrist compass follow essential
  feedback. Whip, horses, climbing, concentration and duels retain separate
  input/HUD gates; ordinary menus, gun firing and locomotion do not accept them.

## Milestone 5 — additional renderers and games

- OpenXR/frame-generation experiments: **future research only**; they do not replace validation of the experimental D3D9Ex/OpenVR shared-texture path.
- D3D10 renderer path for Call of Juarez: **planned**.
- Bound in Blood integration: **planned**.
- Gunslinger integration: **planned**.
- Promotion of shared Chrome Engine contracts: **blocked until a second game independently proves them**.

## Release direction

A public release requires the reference backend to clear the controller-only UI, body IK, weapon alignment, presentation-performance and shutdown gates in representative gameplay without regressing validated stereo, tracking or native locomotion behavior.
