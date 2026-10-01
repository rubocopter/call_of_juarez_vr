# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`.

## Current gate

**Immediate physical gate:** one combined UI + Body IK run. First-level main-menu
laser hover/selection and physical-mouse coexistence are **headset-validated**.
Visible Yes/No-dialog and gameplay-pause input-root routing are **implemented /
host-tested** and need physical acceptance. The arm solver now preserves valid
hard-clamped positional ownership instead of dropping back to native animation;
that continuity change is also **implemented / host-tested**. Use normal
`prepare -BodyIkAtStart` and follow the combined gesture in [Validation](VALIDATION.md#combined-menu-and-gameplay-regression).

Native stereo, bounded configured-HMD cadence and normal-quit shutdown are
accepted for the exercised build/host. Continuous body/arm IK remains visually
rejected; weapon alignment, sustained tail latency and abnormal renderer
lifecycle remain separate gates. LTR research reset evidence does not promote
this mod's pending-frame transport reset.

The experimental D3D9Ex shared-texture production path is now **live-tested**
through native-stereo publication, D3D11 copy and explicit-pose new-frame
submission to both OpenVR eyes. Complete acceptance remains open. Its
compatibility/startup/reset subpath is **live-tested** on the
inspected Steam build/host: the manual run sustains Ex Present/capture, completes
a reset and reaches outer finalization. The corrected full startup gate now
passes: geometry-derived texture extents support upload/submission with the
actual asymmetric PS VR2 optics, and the operator confirms videos/menu plus a
visible pointer in the visor. Flat presentation is **headset-validated for this
startup path**; normal-quit inner presenter shutdown and finalization are also
**live-tested**. The bounded cadence target now passes near the readback-off
reference, and the operator confirms correct stereo depth and stable head turns.
Sustained pacing, tail latency and pending-frame reset/device loss remain separate.
The user clarified that the producer benchmark is constrained by the desktop
configuration and the visor is configured to 90 Hz. The **implemented /
host-tested / live-tested bounded** follow-up derives a render rate cap from the configured
OpenVR HMD refresh and removes desktop Present vsync when valid VR timing exists.
It supports refresh changes without a fixed 90 Hz constant. Compositor pacing
remains `WaitGetPoses`; a bounded physical run reports HMD/target at 90 Hz,
producer pairs around 87/s, total submissions around 89/s and complete closure.
The operator reports improved smoothness. Refresh changes, missing-rate recovery
and sustained tail latency remain separate gates.
Earlier production attempts
stopped after three Ex Presents, but an
independent LTR research path on the same game build has since demonstrated the
required startup compatibility pattern: semantic MANAGED adaptation plus
immediate `BeginStateBlock` hook restoration reaches a bounded 120-Present
startup window. Its reset probe keeps all nine tracked adapted resources alive,
preserves identical contents for four directly hashable textures and observes a
generation-1 texture still bound in generation 2. Its real two-slot transport
also survives an engine reset by cancelling the stalled old generation and
completing 12/12 submissions in the replacement generation with zero sampled
mismatches. These are **live-tested research results** and do not promote this
mod's candidate.

The production tree now implements the directly reusable compatibility findings:
the observed MANAGED WRITEONLY VB/IB profiles map to DEFAULT with usage and
metadata preserved, and `BeginStateBlock` immediately reacquires the owned hook
slots. Focused host tests cover allocation, Lock/Unlock and post-state-block hook
retention. The earlier clean-shutdown blocker is closed on this build/host:
the fresh physical run completes same-owner OpenVR shutdown, GPU-copy drain,
capture cleanup, hook restoration and final summaries. A corrected telemetry
verifier accepts genuine successful native submissions and strictly checks
the final drain counters. A bounded x86 host experiment reproduces owner cleanup
being skipped at DLL atexit and completing when stopped before process exit.
The candidate now installs an exact-build pre-`DestroyGame` callback, preserves
the original call and restores its protected import. Owner cleanup before
original forwarding/DLL atexit and producer reclamation after pending GPU copies
drain are **host-tested**, with normal-quit closure now **live-tested** in the
game. The ring-depth telemetry follow-up is now also **live-tested**, with zero
final depth after cancelling the last unpublished frame. Measured cadence and
operator-confirmed stereo depth/head-turn stability complete the bounded transport
gesture. Preserve that gesture as a regression requirement without repeating it
or restarting a broad resource census. Product body/UI/weapon work can proceed;
sustained pacing and abnormal renderer lifecycle remain independent gates.
Device-loss behavior, non-lockable resources and broader gameplay semantics
remain open. Menu, arm-continuity and weapon-alignment remain separate product
gates; abnormal shutdown recovery is unproved.

Three separate playability areas remain open after the bounded transport gate:

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
- Arm reach/anatomy/orientation: previous candidate **live-exercised / physically rejected for continuity**. Valid solver-clamped positional plans now retain ownership **implemented / host-tested** while rotation/hand/reload safety remains conservative; physical continuity/anatomy is the next gate.
- Native reload-animation ownership: **host-tested follow-up**.
- Pelvis/legs full-body writing: **planned**, blocked on acceptable arm/body ownership first.

## Milestone 4 — controller UI and interactions

- PS VR2 Sense action/binding layer: **live-tested** for gameplay actions; recenter/snap have higher validation.
- Flat-menu pointer: Windows injection and sprite-only internal-cursor delivery are **physically rejected**. Native sprite-tree mouse events are **headset-validated for first-level main-menu hover/selection and controller/mouse coexistence**. Visible Yes/No-dialog and gameplay-pause routing are **implemented / host-tested** pending physical acceptance.
- Cross accept / Circle Escape-back / L2-R2 ray-select: **host-tested follow-up**.
- Loading continuation through the native loading-input boundary: **host-tested**.
- Subtitle visibility: **open physical check**; distinguish shipped setting state from presentation loss.
- Controller-origin flat-theater beam: **live-exercised / visible in headset**; alignment and accurate controller-only UI operation remain open.
- Controller-owned per-hand weapon direction and visual origin: **host-tested**.
- Ballistic-origin ownership across the native attack transition: **host-tested**.
- Sense tip direction convention: **live-tested diagnostically**; local `-Z` is the demonstrated pointing direction.
- Temporary controller-tip alignment ray: **planned diagnostic**. Use only to compare tracked direction with the visible weapon/barrel.
- Physical gun-origin/direction acceptance: **pending/rejected**.
- Motion-controlled reloads and richer world interactions: **planned**.

## Milestone 5 — additional renderers and games

- OpenXR/frame-generation experiments: **future research only**; they do not replace validation of the experimental D3D9Ex/OpenVR shared-texture path.
- D3D10 renderer path for Call of Juarez: **planned**.
- Bound in Blood integration: **planned**.
- Gunslinger integration: **planned**.
- Promotion of shared Chrome Engine contracts: **blocked until a second game independently proves them**.

## Release direction

A public release requires the reference backend to clear the controller-only UI, body IK, weapon alignment, presentation-performance and shutdown gates in representative gameplay without regressing validated stereo, tracking or native locomotion behavior.
