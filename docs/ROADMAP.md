# Roadmap

The active target is original Steam Call of Juarez (2006), Windows x86, D3D9
native stereo through OpenVR/SteamVR. PS VR2 Sense is the development reference.
[Architecture](ARCHITECTURE.md) owns contracts; [Validation](VALIDATION.md) owns
acceptance. Implementation, host tests and publication do not establish physical
acceptance or supported hardware.

## Current gate

As of **2 November 2026**, development remains focused on the native VR adaptation.
Neural/DirectNR is deferred at the operator's request: retained as an optional,
default-Off developer experiment, with no worthwhile visual gain accepted and
live cadence/quality rejected. Neither facial tuning nor another Neural headset
candidate is on the active sequence. Ordinary preparation stages no Neural
dependencies. DLAA/DLSS and independent resolution remain separate, unqualified
rendering tracks; none is a prerequisite for core VR progress.

The 26 October public `.6` PS VR2 session accepts ordinary Winchester R2
firing, but rejects its current manual insertion ergonomics, scoped Winchester
zoom reticle/actual bullet-impact agreement and mounted comfort. The operator
reported an unfused horse mask in .6. The later 27 October Steam session
accepts the corrected peripheral effect and encountered campfires/world effects;
steering remains difficult and appears more responsive with R1 at higher speed.
The reload clip confirms awkward precise insertion and intermittent unnatural
support-hand poses. The completed telemetry profile still rejects its eye-baseline
comparison. Experimental compatibility of the exact tested GOG build now has
operator functional acceptance, including Steam saves; it shares the same mod
and does not establish a second fully supported target. [Validation's snapshot and
checklist](VALIDATION.md#pending-headset-checks) own these physical boundaries.
The next combined candidate should target measured horse motion/comfort
ownership, a properly located Winchester loading mouth and scope ballistics,
alongside outstanding head-yaw isolation, reload recovery and eligible fixes.
Host verification cannot promote these headset verdicts.

The latest extended PS VR2 session additionally confirms functional manual
reload, one arrow per bow gesture, skippable movies, correct subtitle and
continuation behavior, intact encountered HUD/effects, and successful
secret-scroll chest pickup. A sustained upward-pointing/free LEFT index pose
intermittently recurs with long guns. Horse control still feels too fast,
occasionally turns opposite the intended input and induces substantial nausea;
immersive movie selection sometimes falls back to theater. The inventoried
mounted trace records high cached native velocities and turn persistence but
does not prove real rider displacement or a sign error. Finished reload traces
observe accepted native transfers while visible insertion contact remains
offset. The source now includes an optional, host-tested native-`DArrow`-nock
bowstring locator; its real string alignment and comfort await a new physical
candidate. Small firearm loading-mouth circles still require measured,
phase/owner-qualified opening anchors. Do not require replay of accepted scroll,
Gatling, duel, bow basic, HUD or death gates.

Continue offline engineering by dependency: native owner and phase evidence,
then measured weapon geometry/hand contact, followed by narrowly admitted
mechanics. The exact-family inventory below distinguishes runtime-integrated
features from observers and separately verified laboratory derivatives.

For DLAA/DLSS, verify source-owned per-eye color/depth and material coverage
before further reconstruction work; no game image reconstruction is active.
Independent-resolution B0 remains rejected/default-off. The current incremental
package line is `v0.1.0-prealpha.7`, carrying the accumulated source batch with
its documented experimental limits. Archived `.5` artifacts remain unchanged.
Accepted functionality stays scoped to the evidence in Validation. Candidate
identities and run histories remain in ignored `work/`.

## Development backlog

The next implementation sequence is:

| Order | Concrete next step | Current boundary / completion condition |
| --- | --- | --- |
| 1 — mounted controls | Correlate R1/gallop and lateral reins with native gait, cached velocity, actual rider heading and camera yaw before changing steering or speed. | A bounded read-only native observer and inventoried offline correlation analyzer have 658 earlier mounted observations, including large cached velocity and persistence after neutral. A source-only optional spatial extension now captures player/camera world endpoints and natural camera yaw for later evidence. No physical run has exercised it; actual rider displacement and right/left sign convention remain unresolved. The operator again rejects comfort. Preserve accepted peripheral-mask presentation and native permissions. |
| 2 — reload ergonomics | Use Winchester and Remington/SawOff observers to establish displayed opening clearance and cartridge-tip approach in each native reload phase. Add optional small reload-mouth circles only after this anchoring is verified. | Finished run observes 100 accepted insertion events across unit/indexed/clip modes; that does not establish visible contact. Geometry is host-tested but tip/mouth correlation is sparse; the generic 14 cm grip zone remains provisional. Preserve unit native writes and safe miss/retry/close recovery. |
| 3 — scoped aiming | Correlate the fired native trajectory/impact with the optical ray and qualify the existing HMD-yaw isolation correction. | Finite-depth optical parallax is characterized; the cause of the reported miss remains unobserved. No arbitrary reticle offset or changed native spread. |
| 4 — hand and movie ownership | Verify the prepared-rifle left-finger fix in headset, correlate any remaining intermittent free-left-index freeze with native/VR ownership and long-gun phases, then investigate intermittent theater during selected Immersive movies. | Offline code inspection found that the attached long-gun path omitted left finger data and the existing eligibility gate required an already committed child weapon while parent hands were mapped first. The prepared, retained-rifle exception and left finger forwarding now pass host/JNI checks, including native reload/alternate and identity-loss rejection; no new headset acceptance. Finished movie evidence shows natural stale-stereo and native-blocking fallbacks. Fallback entry diagnostics now distinguish observation, blocker, release, settings and queue freshness with selected mode/owner epoch; host checks pass, and causality still needs real movie evidence. |
| 5 — stability and presentation | Resolve canonical eye-baseline evidence, then batch eligible held-input/focus/loading/reload recovery and graphical HUD work. Physically assess the optional native-nock bowstring locator in the current combined candidate. | Native `DArrow` nock owner/pose capture and head-relative projection now produce a bounded 3.6 cm source-only locator, with preference, pose, owner, gameplay/stereo and restoration gates. Bow gesture, raster, compositor and geometry host checks pass. The current staged candidate includes the marker; optical placement, readability and comfort remain unqualified in headset. Host corrections and accepted gameplay stay distinct. |

Workstreams below distinguish remaining implementation from physical checks of
existing code. Continue independent engineering while eligible campaign scenes
are unavailable; collect related physical checks in one meaningful candidate.

1. **Convincing cartridge contact.** Preserve native unit loading and the accepted
   cartridge/port visibility. Judge the host-tested displayed-finger tangent and
   native-confirmed thumb gesture before further changes. Offline overlap does
   not justify another arbitrary anchor offset. Qualify depth contents and hand
   coverage before implementing scene/hand occlusion.
2. **Measured loading mouth.** Establish a phase-bound port frame and clearance
   before connecting tip/trajectory insertion. The current 14 cm grip zone stays
   active; pivot rankings and drum phase observations do not admit a socket,
    chamber occupancy, ejection or an independent mechanical writer. The .6
    Winchester trial physically rejects the existing narrow/misaligned gesture;
    The exact ordinary/scoped receiver lip is measured offline; the decoded
    gate blocks its diagnostic access ray in bind geometry. A bounded source
    observer now correlates the full transformed lip, current displayed
    GunBody/GunLoader/Holder and textured cartridge nose with bracketed native
    phase, retained ownership and verified restoration. Host fixtures cover the
    observer; collect live displayed clearance/approach evidence before replacing
    the rejected grip zone. Pivot/radius tuning remains insufficient.
    Remington/SawOff now have a bounded observer for both rear mouths following
    the animated barrel, with retained ownership, native phase/count brackets and
    displayed mesh nose. Host tests cover transforms, indexed state, JNI faults
    and publication rejection. Collect open-chamber approach and native slot
    correspondence before enabling mouth insertion or snapping the support hand.
    The reported grip-centred shotgun gesture remains unresolved.
3. **Campaign correctness.** Secret-scroll chest pickup now has scoped operator
   acceptance; retain without a dedicated replay. Validate mounted reins and rider
    free look without changing accepted on-foot body following. Instrument
    mounted R1 gallop, native velocity, steering and rider/camera movement;
    identify the source of extreme speed/motion before proposing comfort modes.
    The completed mounted observer run supplies cached native velocity and turn
    but lacks measured rider-heading/camera response and same-frame causality.
    The mask's equal-UV binocular geometry defect now has a physical-optics
    projection correction with WARP regression coverage and scoped operator
    visual acceptance. Preserve that result; steering/overall comfort and
    broader On/Off recovery remain separate.
    The immersive skip route now has operator acceptance. Inconsistent theater
    presentation under an Immersive selection remains a distinct issue; accepted
    cinematic stereo recovery stays closed in its exercised scope.
4. **Recovery and pacing.** Resolve the canonical eye-baseline discrepancy by
   correlating runtime and applied eye data; do not infer a physical regression
   or grant a PASS from operator feedback. Broaden held-input, ownership, tracking, save/death and
   device-loss coverage. Attribute recording microskips and community issue #5
   stutters before optimizing. Diagnostic gating fixes remove known unwanted
   work but establish no measured performance gain or crash resolution.
5. **Complete presentation.** Qualify native graphical HUD capture, remaining
    weapon geometry/optics (including scoped Winchester actual shot agreement)
    and event-free physical-walk/turn leg animation.
   Bindings, static research and passive observers do not implement a physical
   mechanic.
6. **Operator setup and options.** A first WinForms GUI is implemented with Steam
   AppID 3020 manifest/library and exact GOG installation autodetection, a rescan button, multi-install
   selection and manual Browse, package verification, install/update, prepare session, status,
   uninstall/recovery, local report creation and saved resolution profiles. Its
   Western parchment/leather palette and runtime-drawn frontier illustration are
   implemented without distributed game artwork. Scripted actions retain exact
   hashes, rollback, known-build rejection and manually controlled game/SteamVR
   lifecycle. The repository GUI now prepares a verified local package
   automatically, superseding the source-folder blanket rejection.
   Launcher/discovery fixtures, local ZIP inventory checks and isolated
   exact-game integration cover all actions, updates, session preferences,
   uninstall and injected-failure restoration for Steam and the experimental GOG
   build. The .7 distribution includes this GUI; wider interactive Windows/DPI
   confirmation remains separate from automated installation coverage. Manual reload On/Off remains an advanced Install/update choice. A
   future native Options > Game
   selector requires splitting installed capability from persisted runtime policy
   and safely terminating held cartridges and native reload ownership on change.

### Weapon implementation inventory

Exact capacities, classes, reserve types and callback ownership belong in
[reload research](research/COJ_RELOAD_OWNERSHIP.md).

| Family | Current boundary | Remaining development/evidence |
| --- | --- | --- |
| Peacemaker / admitted Frontier1878 | Persistent native unit-loading route with scoped acceptance. | Contact/occlusion, precise mouth, sound, mechanics and broader interruption recovery. |
| Additional Frontier, Walker, Cogswell | Manual profiles host-tested; Cogswell uses rifle reserve. | Per-model physical unit transfer, geometry, ammunition appearance and recovery. Existing acceptance does not promote these extensions. |
| Winchester | Production manual-option source integrates the exact 12-round native unit/body-cycle transaction and shared hand gesture. Its corrected guarded button1/R2 firing passed a .6 PS VR2 session. | Current insertion is physically awkward and misses the visible port. Measure exact loading mouth, tip approach and clearance, then qualify transfers/callbacks, retries, support release/regrip and recovery; provisional cartridge look is low priority. |
| Remington / SawOff | Remington grasp accepted; current manual source integrates captured-index single-shell accounting, shared shotgun reserve, input, guarded per-unit native body replay and supervised close. SawOff retains its exact single-hand/either-hand policy. | Physical shell appearance, per-barrel port, native hand ownership and mechanical playback/recovery. The integrated replay/Status1 feedback is host-tested only; generic grip-zone loading is provisional. |
| Scoped Winchester | Grip/discharge and the revised large scope/reticle have scoped acceptance. Current source shares Winchester manual unit/body-cycle accounting with separate exact-class admission; controller-only scope aim remains a host candidate. | Zoom reticle and actual shot impact were misaligned in .6 and are rejected. Correlate optical ray with native fired trajectory/impact/spread by pose and distance; separately check Focus exit/head tracking, manual loading, support regrip/recovery and exact mouth. |
| Volcanic | Current manual source integrates the exact 8-round native unit/body-cycle route with pistol reserve and the shared pickup/carry/release gesture. | Physical animation, hand/port, per-model cartridge and recovery qualification. |
| Derringer | Current manual source integrates captured-index unit loading for two 1-round slots and one pistol reserve pool, preserving original whole-clip parameters. | Physical port/barrel presentation and recovery. Original setters implement a guarded selected-index composite; the native whole-clip loop is not used for each cartridge. |
| Lemant / LeMat | Current manual source integrates captured-index loading, capacities1/8 and rifle/pistol reserves. Fresh pickup chooses the lowest eligible index and retains it until insertion/cancellation. | Explicit mode/port UX, distinct ammunition presentation, native playback and recovery. No automatic redirect of a held cartridge to the other pool; no empty-trigger index inference. |
| Schofield base/A/B | Base has exercised manual whole-clip functional acceptance and correlated native readback. Source replaces the rejected proxy bundle with imported textured cartridge instances and fixes held empty-trigger opening/closure. A/B remain host candidates. | Retry friction, textured bundle appearance/contact, native animation and broader recovery. No per-chamber insertion, exact-calibre model or measured speedloader is claimed. |
| Bow | Physical grab/pull/release has scoped functional acceptance. | Mesh/string/arrow/reticle, haptics, concentration optics and interruption evidence. Preserve native trajectory, ammo and concentration lifecycle. |
| Hands / Bible / whip / dynamite / mounted weapons | Scoped Hands and Ray Bible/on-foot kick acceptance; other native routes retained. | Fist volume/guard/AI anticipation and exact permissions/contact for additional physical mechanics. |

### Other implementation inventory

Remaining-weapon animation work has an original-archive inventory and audited
native graph. Naive replay remains rejected because completion changes the
desired state to movement. The isolated Winchester/Volcanic derivative now
implements a scoped per-cartridge body cycle with retained ownership, unit
readback, one guarded self-Clear and exception quarantine. Interpreted fixtures
exercise original callback bytecode; the combined production derivative now integrates native cycle, selected-index and clip paths. Real JVM scheduling/playback, mechanical presentation and loading mouths remain physically unqualified.
The indexed laboratory retains a non-installable body21-cycle opt-in for
Remington/SawOff/Derringer/Lemant. Production now composes that guarded emitter
with its canonical endpoint, exact-profile Status1 typed feedback and gesture
recovery. Original Started/storage-bytecode fixtures, shared family exclusion
and shipped JVM verification establish host behavior. Loading mouths, native
mechanical playback and physical recovery remain separate gates.

| Area | Remaining work |
| --- | --- |
| Body | Event-free offline ANM sampling and hash-bound Ray/Billy eight-leg recomposition are implemented and interpreted-tested. Runtime physical-motion policy and retained native write/restore transaction remain pending. Native root/SetFrame advances events/footsteps. |
| Holsters | Four original model profiles and 12 Ray/Billy mount recompositions have interpreted coverage and C++ geometry host tests. Live attachment/identity/contact and attached meshes remain pending. Accepted ergonomic zones/guides remain usable. |
| HUD / hints | Complete graphical capture and device/binding-aware visual qualification; current text and host-tested coaching retain native visibility/priority. |
| Horse comfort | The corrected peripheral effect has scoped visual acceptance. Mounted steering remains difficult and seems more responsive with R1, which also increases speed. Bounded native gait/request/velocity/turn observation is host-tested; correlate live gallop/reins before a behavior change. Rider-camera ownership and reduced nausea remain open. |
| Installer / gameplay preference | Western-styled Windows GUI over existing transactions with Steam library/manifest and GOG registry/default-path detection, multi-install choice, rescanning, six actions, saved resolution and advanced reload selection. Source checkout actions automatically prepare a verified local package. Host tests, ZIP inventory and isolated exact-game all-action/update/recovery integration passed. Included in the .7 package; wider interactive Windows/DPI confirmation remains pending. Native Options > Game Manual Reload selector still requires capability/preference separation and safe runtime deactivation. |
| Compatibility | Real OneDrive hydration/synchronization, high-address native owners, broader allocation pressure, campaign and hardware coverage. |
| Deferred | DLC adaptation and bHaptics provider/game-event producers by operator priority; existing neutral haptic QA policy is host-tested only. |

## Parallel engineering tracks

Optional spatial image processing is implemented and host-tested: original
adaptive sharpening, diagonal-edge smoothing and contrast/saturation/gamma on
fresh D3D11 native eye images before VR overlays. Nine native Options > Game
selectors edit the dedicated versioned graphics preferences and publish changes
at fresh captures without per-frame file reads. Color/comfort changes are live;
resolution selection applies at the next preparation. Defaults remain off;
theater/repeats/the private bank remain excluded. Expanded menu layout, cancel/
defaults, live application and persistence have host coverage only. Visor
quality, comfort, sustained cost and real resource-loss fallback remain pending;
batch these with a relevant opted-in candidate, without reopening accepted
ordinary resolution checks. No measured gain follows from host pixels.

The original x86/x64 NGX D3D11 backend now evaluates real DLAA/DLSS Quality on
paired controlled GPU inputs. Depth/camera-motion/bias guide generation and
history/reset policy have host coverage. Native Options > Game saves Native,
DLAA or DLSS Quality requests and explicitly reports pending/native-active state;
the backend is not connected to CoJ presentation. Next: use the bounded opt-in
HDR-alpha probe to qualify the exact `1B_16` material/hand coverage, implement
same-eye/frame/pose/generation depth transport and scoped jitter/dynamic masks,
then atomically present/commit reconstructed pairs with native fallback. DLAA can
precede independent resizing; DLSS performance still requires reduced native
raster work and a measured net benefit. Preserve the rejected independent bank's
ownership gates. FarCry2VR also supplies a sun-shadow research pattern; independently
establish Chrome shadow equivalence before skipping any pass. Native video
filtering already supports anisotropic trilinear; forced 16x is not established.

**Deferred Neural experiment:** real-model inference and committed game pairs
are demonstrated; useful quality and acceptable cadence are not. The standalone
image lab retains exact scale/settings comparisons, including the reported
Deus Ex 60%/90%/one-pass profile, and the supplied-mask fixture retains ownership
tests. No human-only game path, animated per-eye bounds or visible-face mask is
integrated. The operator finds the still-image benefit insufficient to justify
continuation. Preserve the optional code and default-Off native menu request;
do not allocate the next core-VR candidate to this experiment.
Reopening requires a renewed development decision, useful representative quality
and fresh native delivery with exact result/source identity. Stale-result blending
and longer frame holds do not satisfy that boundary. Retained technical findings
belong to [human targeting](research/COJ_NEURAL_HUMAN_TARGETING.md) and the
[DirectNR comparison](research/COJ_VR_REFERENCE_PROJECTS.md#penumbra-directnr-comparison);
[Validation](VALIDATION.md#standalone-neural-laboratory) owns the rejected verdict.

Reload G1 precedes G2. G3–G5 can proceed independently. Rendering research is
separate from ordinary stereo and must preserve usable menus/controls on failure.
Independent resolution remains default-off and rejected; live structural binding
correlation and synthetic GPU tests do not qualify resizing or output retirement.
See [resolution research](research/COJ_VR_RENDER_RESOLUTION.md) and the
[independent-resolution design](research/COJ_VR_INDEPENDENT_RESOLUTION_DESIGN.md).

## Next implementation gates

| Gate | Actual state | Next prerequisite / exit boundary |
| --- | --- | --- |
| G1 — cartridge contact and occlusion | Revised tangent/thumb and depth observer host-tested; convincing contact unaccepted; occlusion planned. | Judge contact/axis/scale/roll and native-confirmed gesture. Qualify exact per-eye depth contents, coverage, format, lifetime and pose/generation before renderer-owned occlusion. Preserve single native transfer and scoped recovery. |
| G2 — port and precise insertion | Port appearance accepted; pivots/phase are diagnostics; generic socket policy unwired. | Measured phase-bound mouth/clearance, then deliberate aligned release with one native loaded +1/reserve -1. Reject stale owners, closed gate, wrong approaches and misses. No chamber/ejection promotion. |
| G3 — complete graphical HUD | Text/countdown/notices/directions accepted where exercised; sprite/flush observer and separate bounded per-draw HUD metadata collector have host fixture coverage. Native collection and capture remain unqualified. | Qualify naturally scheduled target, alpha/coverage and retained lifetime. Observe once; never replay traversal for the second eye. Metadata does not admit capture/occlusion. |
| G4 — transport recovery and pacing | Normal transport/quit accepted in scope; independent consumer-query recovery host-tested. | Qualify native reset/device-loss and teardown/recreation when retirement proof fails. Retain uncertain leases, reject stale generations; measure sustained frame-age/recording tails. |
| G5 — persisted verdicts | Host-tested; rejection persistence live-tested through collection/restoration. | Exercise successful full-profile collection during a relevant candidate. Preserve input hashes, ordered run/build intervals and stale-input rejection. Collection cannot accept missing gestures or physical gates. |

## Next acceptance gates for implemented changes

Use [the pending checklist](VALIDATION.md#pending-headset-checks) as the single
physical checklist. It groups reload contact/feedback/reserve/recovery, hints and
pickup guides, left-hand interaction, mounted controls, immersive skip/loading
continuation, mirrored rifle triggers, HUD styling and global ownership checks.
Unavailable campaign scenes remain pending/inconclusive; do not split them into
repeated small sessions. These changes have no new physical acceptance.

## Milestones and stabilization

The [architecture invariants](ARCHITECTURE.md#safety-and-provenance-invariants)
govern every milestone: recognized SHA-256 build, scoped/restorable native
mutations, D3D9/OpenVR as the active path and a manually controlled game/SteamVR
lifecycle. The independent-eye bank and diagnostics remain opt-in or disabled.

| Milestone | Delivery boundary | Still open |
| --- | --- | --- |
| 1. Native stereo and HMD | Stereo, tracking, recenter and cinematic recovery in exercised scenes. | Device/loading transitions and sustained pacing. |
| 2. Player body and comfort | Native movement, jump, crouch, turning and body follow in exercised scenes. | Mounted steering/comfort, physical-walk leg animation. |
| 3. Hands and body | Independent tracked hands and native torso/legs. | Weapon/finger fit and recovery; connected-arm IK remains diagnostic. |
| 4. UI and interactions | Exercised menus, wheel, HUD/text, combat, bow, holsters and revolver loading. | Per-weapon mechanics, full graphical HUD and wider campaign validation. |
| 5. Other backends/games | Experimental OpenXR research. | D3D10, Bound in Blood and Gunslinger; independent proof before Chrome Engine generalization. |

These milestones identify implemented scope. [Validation](VALIDATION.md)
provides the exact physical acceptance boundaries and pending checks.

## Release direction

Keep published .5 artifacts and verdicts unchanged. Future distributions retain
reversible installation, saved resolution/reload choices, local-only diagnostics,
original-asset exclusion, GPL-3.0-or-later corresponding source and notices.
Windows 10/11 x64 packaging retains the x86 game/runtime and PowerShell 5.1
transaction engine. The .7 package includes the graphical front end, with
verified local packaging and isolated exact-game integration; broader interactive
operator acceptance remains separate. Manual reload should
become a runtime native Options > Game preference once installed capability,
persistence and safe session transitions are verified. No automatic updater is
planned. Release notes own package history;
publication cannot promote hardware, campaign, rendering or recovery acceptance.
