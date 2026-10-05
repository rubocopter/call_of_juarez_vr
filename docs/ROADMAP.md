# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`.

## Current gate

**Immediate gate:** native protected-target red-X feedback, **host-tested** and
pending visor acceptance. Positive right-stick X now maps to negative native yaw
(right); left maps to positive yaw, and that corrected direction is
**headset-validated for the exercised run**. The step remains 45 degrees with
unchanged engage/release and held-stick latching. The operator accepts the
exercised wrist card, ammunition updates and damage-driven health updates.
Broader recovery, pause/load and pacing checks retain their own gates.
Protected-target feedback requires the native warning,
natural trace weapon owner and a fresh trace matching the current tracked ray;
no native trace/update is forced and native firing permission is unchanged.
Billy's whip now has operator acceptance for exercised use and approximately
matching reticle alignment, without other reported problems. Exact reach/collision,
every attach/length/release gesture and broader climbing retain separate gates.
Its procedural-tool pose cache and right-hand attack ray are host-tested.
Hint presentation translates native mouse-button names to Sense labels.
The previously failing narrated campaign transition is **headset-validated for
the exercised one-bit Large Address Aware candidate**: the operator continued
into whip content without crashing and quit normally; restoration is verified.
Actual used VA exceeded 2 GiB, with no sampled allocation failures. Transition
save/load, repeated loading and general high-address compatibility remain open.
Transactional staging/restoration and x86 high-address capacity are host-tested. A bounded
read-only VA map now records address capacity/free regions separately from
private commit at texture-allocation boundaries. No resource pool or resolution
change is inferred from this evidence. Wheel/compass sizes are reduced to 80%
with a revised compass dial; readability at these new sizes remains pending. The
operator accepts exercised head-turn stability with Steam recording off,
pistol muzzle/FX coherence, wrist/grip/reload recovery, wheel switching/put-away,
objective wrist compass and focus exit/fresh fire. Recording compatibility remains
open: microskips start with recording and recover when it stops; this observation
does not identify the underlying overhead. Triangle + Circle remains unexecuted
in the current context and is deferred while wheel put-away works.
The operator confirms recovered
pause selection, box pickup/carry/put-down with L1 and Triangle + R2 logs.
Visible shot FX, impact decals, water and destructible bottles are now accepted
for exercised contexts. Earlier right-pistol trigger inversion motivated the
complete native controller route, now host-tested: action 10
converts to right-hand Attack(0), action 9 to left-hand Attack(1). Corrected
single-right-pistol trigger ownership is now operator-confirmed, including the
empty left trigger not firing it; left/dual acceptance remains separate. Native emitter creation/commit
is live-observed; those counters do not establish precise visual coherence.
Live native FX samples show the global enable word set and the particle camera
matching each eye. A host-tested bounded owner-local reader adds clock/lifetime,
positions, flags and known-subtype particle counts with generation/replacement
checks. First update, expiry and measured emission still need technical evidence;
operator-observed visible FX do not validate those read-only diagnostics.

Pickup-triggered autosave crashed in optional screenshot rendering with a native
allocation exception. A host-tested exact-class workaround retains the save flow
and stale-preview cleanup while omitting quick/automatic-save GPU thumbnails.
The exercised new-autosave/load gesture is now operator-confirmed;
the underlying resource pressure remains unresolved.

The active presentation retains native torso/legs with independent native hands
at original proportions. Authored holding sockets share the hand rigid map;
parent hands move before child weapons, and restoration verifies both owners.
The connected-arm solver and enlarged reach fit remain diagnostic experiments
following physical anatomy rejection. The independent-hand path now has
operator acceptance for exercised wrist comfort, grip cohesion and reload/movement
recovery; broader equipment/animation contexts remain separate checks.

The current **host-tested** shot/effect follow-up retains native shot spread,
delivers fire after verified muzzle publication, and initializes detached
combustion/smoke with the complete measured barrel frame. Exact native research
shows the emitter create-call uses +Y/up while shipped barrel particles emit
along -X; the previous single-direction adaptation lost that relationship.
Visible shot FX and impact decals now have operator acceptance, alongside water
and destructible bottles. Side-on/rolled pistol muzzle coherence and flash/smoke/light
now have operator acceptance. Exact missile birth/first-draw vectors and left/dual
firing in available contexts retain separate technical/physical gates.

The reversible graphics profile now preserves the chosen game resolution and
quality settings instead of forcing 1920x1080; FSAA 0 retains the demonstrated
shared-texture transport contract. Higher-resolution clarity and sustained HMD
cadence remain pending, including level loading. Repeated launches or graphics
changes within one run do not validate renderer reset or loading stability.

Native stereo, bounded configured-HMD cadence and normal-quit shutdown are
accepted for the reference build/host. Ordinary/modal/pause menus are accepted;
the operator confirms recovery of pause visibility and pointer selection.
The exercised room-scale, crouch and head-relative movement regressions are now
operator-accepted. The later operator report identifies reversed snap direction
as the disorientation cause; earlier 45/90-degree comfort conclusions are
superseded. The 45-degree default is retained, with corrected direction
headset-validated for the exercised run. Head-turn stability without Steam recording remains scoped
acceptance; recording-associated microskips remain a compatibility gate.
A bounded read-only pose/camera/actor
trace is implemented to investigate that boundary; it changes no turn policy.
The previously failing transition passes with LAA; sustained loading stability
remains open. Earlier crashes correlated failed native texture allocation with
the same null dereference, including a repeat without input during loading.
The resource-pressure source is unproved; proximity to Create does not establish
a recenter cause.
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
- HMD yaw/pitch/roll and positional translation: **headset-validated for exercised
  movement regressions**; hand/weapon attachment and loading stability remain separate.
- Explicit render-pose submission for head-turn stability: **headset-validated**.
- Controller recenter: **headset-validated**.
- Startup/loading flat theater and transition to native stereo: **headset-validated for the exercised path**.
- Frame pacing/transport cost: **performance-validated for bounded production cadence / sustained pacing open**; the shared-texture path approaches the readback-off reference with sampled native CPU readback/copy and producer/consumer waits zero. The operator confirms correct stereo depth and stable head turns. Pair production rate is separate from compositor submission/headset refresh, and ring drops/frame-age outliers remain. Production startup and normal shutdown are live-tested; pending-frame reset/device loss and sustained tail latency remain unproved. The shutdown-only D3D11 drain is outside steady-state presentation.
- Inner presenter shutdown/finalization: **live-tested normal quit**; exact pre-exit owner join, complete consumer-copy drain, capture cleanup, hook restoration and final summaries verified. Abnormal exit/device loss remain unproved.

## Milestone 2 — player body and comfort

- Exact campaign player discovery: **live-tested**.
- Room-scale camera translation with native actor position/grounding preserved: **headset-validated for exercised regression**.
- HMD/body-yaw comfort ownership: continuous following at the existing 35-degree boundary is **headset-validated with Body IK disabled** for stable physical head turns.
- Exact snap turn: corrected direction **headset-validated for exercised regression** at the retained 45-degree step. Held-stick latching remains unchanged; head-turn stability without recording is separately accepted and recording compatibility remains open.
- Native analog locomotion through the shipped float-input path: **headset-validated diagnostically** for vanilla-equivalent normal/walk speed. No further movement/input changes are planned.
- Head-relative stick locomotion using residual HMD yaw before native shaping: **headset-validated for exercised regression**.
- Native jump action: **headset-validated diagnostically** for vanilla-equivalent apex and duration. No further jump/physics changes are planned.
- Physical HMD-height crouch merged into the shipped crouch action: **headset-validated for exercised movement regression**. Duplicate native camera descent is compensated without changing collision ownership; wrist/grip/reload checks remain separate.
- Horizontal-only visual body room-scale overlay with native vertical actor/grounding/collision ownership: **live-exercised technically**. Physical displacement still needs stick-equivalent visual locomotion animation.
- Physical-walk visual animation from HMD horizontal movement: **planned/open**.
- Local Ray/Billy head/hair suppression: **headset-validated for HMD view**; shadow behavior remains unverified.

## Milestone 3 — tracked hands and body IK

- Independent native hands with retained torso/legs: **headset-validated for exercised wrists/grip/reload and movement recovery**. Authored holding sockets, parent-before-child weapon application, both-eye barrel verification and absolute unarmed grip orientation supersede the earlier gap/wrist rejection for this active path. Broader equipment/animation contexts remain separate.

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
- Controller-origin flat-theater beam: **headset-validated for exercised menus and recovered pause**; loading/dashboard regressions remain separate.
- Controller-owned per-hand weapon direction and visual origin: **host-tested**.
- Contextual F selection from the central HMD gaze: **headset-validated for box pickup/carry/put-down and pistol pickup**;
  exact local-player `CheckTriggers` getters preserve native range/permissions.
  Other devices and mounting still await physical acceptance.
- Ballistic-origin ownership across the native attack transition: **host-tested**.
- Sense tip direction convention: **live-tested diagnostically**; local `-Z` is the demonstrated pointing direction.
- Exact-frame per-eye gameplay reticle from the controller firing ray: **live-exercised / visible**. Impacts are reported closer to it; exercised side-on/rolled pistol alignment is now accepted, while broader weapon contexts retain separate gates. It is independent from the menu pointer.
- Physical gun-origin/direction acceptance: **headset-validated for exercised side-on/rolled pistol coherence**. The earlier clip's rearward streak alone does not demonstrate ray inversion because the exact mesh has a rearward-authored trail. Actual missile birth/first-draw vectors retain their own technical gate.
- Shot/effect visibility: **headset-validated for exercised contexts**, including side-on/rolled pistol coherence, flash/smoke/light, impacts, water and destructible bottles. The measured emitter frame is committed through native FXDetach. Exact missile birth/first-draw vectors remain unmeasured; native counters do not expand visual acceptance.
- Trigger-to-hand ownership: **headset-validated for the exercised right pistol and empty left trigger**; R2 maps through PlayerController action 10 to Attack hand 0/right, L2 through action 9 to hand 1/left. Cross-hand retry is rejected only for a tracked one-hand firearm. Left/dual and native two-hand/tool semantics remain separate acceptance checks.
- Motion-controlled reloads and richer world interactions: **planned**.
- Expanded PC campaign controls: **headset-validated for exercised controls / broader campaign incomplete**; L1 box pickup/carry/put-down, pistol pickup, Triangle + R2 logs, Triangle + Square blur/toggle-off/fresh fire and exercised modifier/menu held-input regressions are operator-confirmed. Triangle + Circle remains unexecuted and deferred; wheel put-away works. Optical XR magnification, broader interactions and unavailable equipment retain separate gates. Native one-shot delivery after the locomotion lock is a **host-tested** correction, preserving analog shaping and pending edges.
  The [exact PC catalogue](research/COJ_PC_CONTROLS_AND_HUD.md) records direct L1/F,
  separate Options/pause, the Triangle utility layer, focus toggle, alternate
  fire, put-away/discard, objectives/logs and six equipment intents. Save/load
  and Q/E lean remain deliberate custom bindings. The visible wheel and
  ownership/cached-permission filtering are host-tested, including denial
  retention until gesture release. Native selection retains final ammunition,
  reload and context eligibility. Switching/put-away with available equipment has
  operator acceptance; XR focus magnification
  remains open.
- Gameplay HUD recovery: **incomplete**. Essential interaction/tutorial/subtitle
  text has a read-only native-owner route and cached finite-depth stereo panels,
  **host-tested**, with floating dialogue subtitles confirmed by the operator;
  tutorial/context visibility, pacing and broader headset readability remain open.
  Isolated LMB/RMB/SPACE BAR/SPACEBAR labels in hint copies now map to
  L2/R2/Cross (host-tested, exercised whip usability accepted); broader PC-key translation remains open. Exact sprite traversal and
  batch-flush boundaries are identified; their passive correlation/target probes
  are **host-tested**, with live route and usable coverage/alpha unproved.
  Native graphical capture remains planned. The native health/ammo wrist card is
  operator-accepted for exercised ammunition and damage-driven health updates. It
  copies visible cached health, active/actual slot totals and reserve
  counts without gameplay updates. Protected-target red-X feedback is host-tested
  with native warning/trace-owner/ray coherence and pending physical acceptance.
  The native-inventory wheel and objective wrist compass
  are host-tested with bounded JNI reads, native rotor-ID resolution, real D3D11
  composition and captured wrist geometry. Exercised direction, attachment,
  readability, switching and pacing are operator-accepted with recording off;
  broader campaign objectives remain separate. Optional Sense skeletal curls
  are **live-tested technically**, with independent samples and partial reported
  quality; native finger retarget/animation is still planned. Exercised whip use
  and approximate reticle alignment are accepted; broader whip/climbing, horses,
  climbing, concentration and duels retain separate
  input/HUD gates; ordinary menus, gun firing and locomotion do not accept them.

## Milestone 5 — additional renderers and games

- OpenXR/frame-generation experiments: **future research only**; they do not replace validation of the experimental D3D9Ex/OpenVR shared-texture path.
- D3D10 renderer path for Call of Juarez: **planned**.
- Bound in Blood integration: **planned**.
- Gunslinger integration: **planned**.
- Promotion of shared Chrome Engine contracts: **blocked until a second game independently proves them**.

## Release direction

A public release requires the reference backend to clear the controller-only UI, body IK, weapon alignment, presentation-performance and shutdown gates in representative gameplay without regressing validated stereo, tracking or native locomotion behavior.
