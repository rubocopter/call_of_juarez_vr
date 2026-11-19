# Roadmap

The active target is original Steam Call of Juarez (2006), Windows x86, D3D9
native stereo through OpenVR/SteamVR. PS VR2 Sense is the development reference.
[Architecture](ARCHITECTURE.md) owns contracts; [Validation](VALIDATION.md) owns
acceptance. Implementation, host tests and publication do not establish physical
acceptance or supported hardware.

## Current gate

The active target is the original Steam CoJ (2006), Windows x86, native D3D9
stereo through OpenVR/SteamVR. The exact tested
GOG build has experimental functional acceptance; it is not a second fully
supported target. Physical acceptance remains scoped to exercised PS VR2 Sense
contexts. [Validation](VALIDATION.md#current-state) owns verdicts;
the [pending checklist](VALIDATION.md#pending-headset-checks) owns physical tests.

Current source already implements and host-tests these changes:

- Sixteen exact manual firearm profiles, including the pinned Henry 1863 alias;
  unit, retained-index and native clip accounting remain separate.
- One optional insertion-policy target guide for every admitted manual profile,
  including Schofield. The provisional grip zone is the default. The optional
  Winchester candidate places the guide at the measured receiver lip and
  permits nearby releases on either side of its plane.
- Recovery of idle-rifle support fingers after positively confirmed native reload
  completion, while preserving retained cleanup responsibility. This does not
  resolve the reported skin pose during active loading.
- Stable camera-only stereo after a refused skip with unchanged movie ownership;
  actual native key attempts still retire the old image. Native permission,
  sequence identity and blockers remain authoritative.
- Native desktop output for screenshots, with the blackout pixel transaction
  removed after it caused black captures and desktop flicker.
- Bounded source-only temporal diagnostics with native final-color/depth/optics
  association, same-eye source identity, persistent worker retirement, and an
  opt-in two-burst consecutive-frame capture mode. At 2560×1440 the collected
  bursts match color/depth optics and exercise offline NGX history reuse.
  Preparation also admits a separate source run at native 2804×2860.
- Captured world anchors for native hints and mission notices with binocular
  fitting.
  Zoom-to-pause and ordinary reading stability now have scoped local headset
  acceptance; the separate Quest report does not change the accepted local pose.
- Contextual EN/ES duel coaching, a pinned EN paused-tutorial overview, a small
  native-nock bow guide, and separate Movie/skippable-sequence subtitle ownership.

These changes are already implemented; remaining physical checks and scoped
acceptance are recorded in Validation. The revised larger, lower centered
subtitles retain their exercised readability acceptance. Functional manual
transfers, bow firing,
correct duel posture, ordinary HUD/menus, secret-scroll pickup, Gatling and
cinematic recovery remain accepted only in their recorded contexts. Later
skip/flicker, finger fit, exact insertion contact, and Hunt01 subtitle fusion
remain unresolved. The exercised mounted steering and bow now have scoped
headset acceptance.

Source and a locally staged candidate may be ahead of published prealpha.7.
Run IDs, exact build/test results, installed hashes and operator checklists stay
under ignored `work/`; this roadmap does not identify the active installation.
Public release notes retain the behavior of their original packages.

## Development backlog

The next implementation work is distinct from testing the changes above:

| Priority | Remaining implementation | Required boundary / completion condition |
| --- | --- | --- |
| 1 — campaign gameplay and reloads | Use the bounded Schofield classifier and native-result traces to measure remaining misses on both hands. Preserve the exercised Winchester insertion and verify early lever closure after an intermediate cartridge. Exercise other firearms during campaign play. | Qualify native transfer, miss and close/regrip, usable native fallback, and uninterrupted campaign progression. Establish cavity clearance and indexed port correspondence before admitting an interior or selected-barrel target. |
| 2 — rendering stability | Isolate translucent flicker under capture pressure. Keep temporal reconstruction in the experimental track. | Use the callback route witness to correlate accepted and rejected native passes with desktop Present, then qualify the pixel-content witness. |
| 3 — movie ownership and stability | Validate the movie-local positional reference and isolate Hunt01 subtitle fusion. | A recorded gameplay displacement reached the authored movie camera. Captured affected/control crops show one composed panel per eye and no visible native duplicate. Broader skip and theater transitions retain separate gates. |
| 4 — hand fit and loading pose | Resolve remaining long-gun grip intersections and the support-like index pose during active reload. | Correlate final hand/socket/distal-index frames, actual finger mapping and native phase with visible skin. The terminal cleanup fix covers recovery after completion. Qualify contact and occlusion separately. |
| 5 — scoped shot agreement | Correlate native fired trajectory/impact with optical ray and qualify head-yaw/Focus isolation. | Reticle/actual impact remains rejected; no guessed reticle offset or native spread change. Controller-only rendering and body-yaw freeze are host-tested, not a physical solution. |
| 6 — recovery and presentation | Complete graphical HUD capture, event-free physical-walk/turn leg motion, broader transport/retry recovery and measured pacing. | Per-eye alpha/depth/material ownership, scoped native write/restore and exact capture/device generation remain prerequisites. Resolve the canonical eye-baseline discrepancy without overriding evidence gates. |

The source-only mounted spatial extension, equipment observers and depth probes
are diagnostics, not finished mechanics. Continue independent engineering when
campaign scenes are unavailable; batch physical checks during ordinary play.

### Mechanics still requiring native boundaries

- Independent right-stick-back hammer cocking: hammer meshes/fire tracks do not
  establish a separate cock/hold/release callback. Native alternate fire is a
  different operation; duel aiming and ordinary turning already own this stick.
- The exact base Schofield has an opt-in development route for opening,
  downward or rearward unloading with a tipped muzzle, retained bundle insertion and a wrist
  flick to close. Qualify count conservation, closing/recovery, rear-view
  guide visibility and gesture comfort using the combined headset gate.
  Ordinary clips and variants retain automatic close on insertion.
- Per-chamber occupancy, ejection, independently driven lever/gate/cylinder and
  per-calibre loose ammunition are not implemented by the manual transfer routes.
- Explicit LeMat mode/port selection, dual-wield manual reload and legacy `_Old`
  classes require their own exact storage, reserve and ownership contracts.
- Motion throws and broader special-weapon/climbing mechanics require proven
  native action/trajectory ownership. A binding or observer alone is insufficient.
- Visible holstered meshes and physical-walk/turn leg policy/write restoration
  remain incomplete despite offline model/animation coverage.
- The native Manual Reload selector, verified install capability, stored
  preference and disable release barriers have host coverage. Live switching
  during loading, fresh-release recovery and the expanded native layout remain
  physical gates.

Exact mechanical findings belong in [reload research](research/COJ_RELOAD_OWNERSHIP.md)
and [arm/weapon research](research/COJ_ARM_SKINNING_AND_AIM.md). Neural/DirectNR
remains deferred/default Off with rejected live quality/cadence. DLAA/DLSS,
independent resolution, OpenXR and D3D10 remain separate experimental/future
tracks; they are not prerequisites for these core-VR tasks.

### Weapon implementation inventory

Exact capacities, classes, reserve types and callback ownership belong in
[reload research](research/COJ_RELOAD_OWNERSHIP.md).

| Family | Current boundary | Remaining development/evidence |
| --- | --- | --- |
| Peacemaker / admitted Frontier1878 | Persistent native unit-loading route with scoped acceptance. | Contact/occlusion, precise mouth, sound, mechanics and broader interruption recovery. |
| Additional Frontier, Walker, Cogswell | Manual profiles host-tested. Cogswell presents the elongated rifle round from its rifle reserve. | Per-model physical unit transfer, geometry, ammunition appearance and recovery. |
| Winchester | Receiver proximity has scoped final-round insertion acceptance on the ordinary rifle, followed by native close. The opt-in lever remains available while waiting with more ammunition to load. | Qualify intermediate transfer and replay, a missed release, early lever close, support regrip and visible clearance. |
| Remington / SawOff | Remington grasp accepted. Current manual source integrates selected-index shell accounting, shared shotgun reserve, distinct shell presentation, guarded body replay and supervised close. SawOff retains its either-hand policy. | Confirm shell appearance, per-barrel loading contact, native hand ownership and mechanical playback/recovery in the headset. The grip-zone loading target remains provisional. |
| Scoped Winchester | Grip/discharge and the revised large scope/reticle have scoped acceptance. Current source shares Winchester manual unit/body-cycle accounting with separate exact-class admission; controller-only scope aim remains a host candidate. | Zoom reticle and actual shot impact were misaligned in .6 and are rejected. Correlate optical ray with native fired trajectory/impact/spread by pose and distance; separately check Focus exit/head tracking, manual loading, support regrip/recovery and exact mouth. |
| Volcanic | Current manual source integrates the exact 8-round native unit/body-cycle route with pistol reserve and the shared pickup/carry/release gesture. | Physical animation, hand/port, per-model cartridge and recovery qualification. |
| Henry 1863 | Exact pinned constructor-only Winchester alias now shares its 12-round right-owned rifle-reserve unit/body21 transaction; host regressions cover accounting and rejection. | Campaign reach, physical playback/recovery and model geometry remain unqualified; the shared provisional target is available, but no Winchester mouth geometry is inherited. |
| Derringer | Current manual source integrates captured-index unit loading for two 1-round slots and one pistol reserve pool, preserving original whole-clip parameters. | Physical port/barrel presentation and recovery. Original setters implement a guarded selected-index composite; the native whole-clip loop is not used for each cartridge. |
| Lemant / LeMat | Current manual source integrates captured-index loading, capacities 1/8 and distinct rifle/pistol presentation. Fresh pickup chooses the lowest eligible index and retains it until insertion or cancellation. | Explicit mode and port selection, physical visibility of both cartridge types, native playback and recovery. |
| Schofield base/A/B | Base has exercised manual whole-clip functional acceptance and correlated native readback. Source replaces the rejected proxy bundle with imported textured cartridge instances and fixes held empty-trigger opening/closure. A/B remain host candidates. | Retry friction, textured bundle appearance/contact, native animation and broader recovery. No per-chamber insertion, exact-calibre model or measured speedloader is claimed. |
| Bow | Physical grab/pull/release has scoped functional acceptance. | Mesh/string/arrow/reticle, haptics, concentration optics and interruption evidence. Preserve native trajectory, ammo and concentration lifecycle. |
| Duel instruction | Optional contextual EN/ES coach and exact paused EN overview are implemented/host-tested; existing native draw/raise/aim/fire controls remain authoritative. | Validate text timing, lower/raise sequence, binding labels and readability without promoting new duel mechanics or other localized resources. |
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
| Horse comfort | Corrected steering, default Walk and optional R1 acceleration have scoped operator acceptance. Wider terrain, deliberate reverse, rider-camera ownership and sustained comfort retain separate physical gates. |
| Installer / gameplay preference | Western-styled Windows GUI over existing transactions with Steam library/manifest and GOG registry/default-path detection, multi-install choice, rescanning, six actions, saved resolution and advanced reload selection. Source checkout actions automatically prepare a verified local package. Host tests, ZIP inventory and isolated exact-game all-action/update/recovery integration passed. The runtime separates installed manual reload capability from the persisted choice. Native Options > Game includes the host-tested Install default/Off/On selector; live UI/cleanup validation and wider interactive Windows/DPI confirmation remain pending. |
| Compatibility | The Quest 3 tester confirmed pause via right stick up + click in the reported setup. Correlate the free left hand tilt with its controller grip and authored hand socket. The newer reading panel still needs Quest confirmation for the holster hint. Real OneDrive hydration/synchronization, high-address native owners, broader allocation pressure, campaign and hardware coverage remain open. |
| Deferred | DLC adaptation and bHaptics provider/game-event producers by operator priority; existing neutral haptic QA policy is host-tested only. |

## Parallel engineering tracks

Optional spatial image processing is implemented and host-tested: original
adaptive sharpening, diagonal-edge smoothing and contrast/saturation/gamma on
fresh D3D11 native eye images before VR overlays. Ten native Options > Game
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
the backend is not connected to CoJ presentation. The opt-in hardware-depth
run exposed a capability rejection with valid PPFX/device identity. The
game's requested `PUREDEVICE` mode reproduces that rejection. The corrected
startup now has bounded live matched-eye hardware-depth delivery. Joint
color/depth previews at `1B_16` now have bounded live correspondence for the
exercised outdoor world, native hands/weapons, foliage and sky. An offline
history-reset NGX run accepts these reduced actual CoJ inputs. The separate
default-off renderer transport now host-tests immutable full-size FP16 color
and packed depth under one lease/fence, including worker consumption and real
NGX controlled pairs. Exact-scene packet export, the bounded worker handoff and
the persistent D3D11/NGX service now have host coverage for source identity,
asynchronous startup, early source retirement, authority cancellation and stale/
expired-result rejection. The explicit sampled-source probe now connects the
game to a bounded copy-only worker audit. Collected evidence now qualifies its
sampled full-size live copies and source retirement, separately from the full
canonical profile and headset reconstruction. Continuous transport and full-image
equivalence remain unqualified.
The bounded source diagnostic has live same-eye final native color previews
alongside HDR and depth, including reviewed matching silhouettes in interior
and exterior gameplay. Those reduced final-color inputs evaluate offline in
NGX DLAA and DLSS Quality with reset history. Host-tested per-eye optics
export preserves native depth projection and history resets follow native
source continuity. The completed-eye observer now supplies matched final-color
and depth optics in collected consecutive stereo previews. Offline NGX replay
exercises six genuine transitions with camera reprojection and independent
per-eye history. A measured depth-disagreement bias mask reached NGX and
returned identical pixels to the full-current mask in that replay. Protocol 2
now preserves fully marked current pixels explicitly after inference, with GPU
and recorded-source replay coverage. An isolated GPU depth-disagreement pass
also covers moving-occluder/exposed-background fixtures, both sampling jitters
and explicit native exclusions, with recorded reduced-source NGX replay. An
opt-in paired-session/worker owner now commits private depth only after complete
stereo inference, seeds reset pairs with current color and retires old mask
storage before new workloads. The active game adapter leaves this disabled.
The bounded native scene-copy witness now has live program and constant
observations for skinning, wind deformation and animated sky. The opt-in
paired draw witness has bounded live coverage of exact vertex/pixel programs
and render state, including draws outside the original first-program budget
and vegetation cutout. Captured pixel programs preserve primary color with a
secondary marker in the standalone GPU fixture, including representative
RGB-only additive and source-alpha draws. A separate host fixture verifies
one leading full clear, opaque occlusion, both blend modes in the same
producer scope, and bounded sidecar retirement at the native-copy boundary.
The read-only witness records incoming typed blits, device writes and secondary
bindings at the HDR copy boundary. A standalone D3D9Ex/D3D11 fixture verifies a
common lease for both eye colors and masks through consumer retirement. One
native-size slot with lossless mask packing skips busy frames and reuses the
same allocations after both fences and private reader retirement. A duplicate
color pool still makes the shared and private storage exceed the sampled-depth
capture budget. Ten live stereo pairs now correlate the native allocations,
initialization and bounded write/copy receipts. Integrate the established
source boundaries with existing color ownership. Water and other transparency
remain separate cases.
Camera/depth reprojection and blend state do not establish that coverage.
Next qualify native
dynamic/material exclusions, scoped game jitter and dynamic motion, connect
continuous reconstruction scheduling, then atomically present reconstructed
pairs with native fallback. DLAA can
precede independent resizing; DLSS performance still requires reduced native
raster work and a measured net benefit. Preserve the rejected independent bank's
ownership gates. FarCry2VR also supplies a sun-shadow research pattern; independently
establish Chrome shadow equivalence before skipping any pass. Native video
filtering already supports anisotropic trilinear; forced 16x is not established.

The active OpenVR route includes bounded measurements of the full wrapper per
eye with a live CPU wrapper baseline. Center-head audio returns to research
because the retired camera hook could change the inputs to native spatial
queries. Qualify an audio-only interception point before another physical
audio session. Visibility and draw attribution
remain necessary before reducing culling or render work. A Quest-only hand
correction requires measured controller frames. Hand and weapon contact requires
the exact native collision and firing seams.
The early experimental OpenXR probes do not establish gameplay parity or justify
a runtime migration. Runtime replacement and external frame generation remain
secondary research, not prerequisites for these improvements.

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
belong to [human targeting](research/COJ_NEURAL_HUMAN_TARGETING.md); the external
Penumbra/DirectNR comparison is in ignored local `work/research/VR_REFERENCE_RESEARCH.md`.
[Validation](VALIDATION.md#standalone-neural-laboratory) owns the rejected verdict.

The operator has renewed autonomous offline DLSS/Neural research. One/two-pass
model comparisons and separate GPU-layer timing are available in the image lab;
source-correct whole-eye settings supersede the old requested-only metadata.
This resumes feasibility measurements, without reopening the rejected retained-
frame game candidate. A useful moving stereo source and fresh same-source GPU
completion are prerequisites for integration; Super Resolution retains its
independent native depth/motion/jitter and reduced-raster gates.

G1 and G2 share current weapon/tip ownership and contact prerequisites; renderer
occlusion is not proof of a mouth or a prerequisite for a coherent target.
G3–G5 can proceed independently. Rendering research is
separate from ordinary stereo and must preserve usable menus/controls on failure.
Independent resolution remains default-off and rejected; live structural binding
correlation and synthetic GPU tests do not qualify resizing or output retirement.
See [resolution research](research/COJ_VR_RENDER_RESOLUTION.md) and the
[independent-resolution design](research/COJ_VR_INDEPENDENT_RESOLUTION_DESIGN.md).

## Next implementation gates

| Gate | Actual state | Next prerequisite / exit boundary |
| --- | --- | --- |
| G1 — cartridge contact and occlusion | Revised tangent/thumb and depth observer host-tested; convincing contact unaccepted; occlusion planned. | Judge contact/axis/scale/roll and native-confirmed gesture. Qualify exact per-eye depth contents, coverage, format, lifetime and pose/generation before renderer-owned occlusion. Preserve single native transfer and scoped recovery. |
| G2 — port and precise insertion | Native port appearance accepted where exercised. Grip-centred contact rejected. The read-only pre-insertion mouth/tip preview has host coverage for four exact rifle and shotgun classes. | Correlate predicted and rendered geometry, qualify phase clearance and indexed correspondence, then require a deliberate aligned release with one native loaded +1/reserve -1. Reject stale owners, closed gates, wrong approaches and misses. |
| G3 — complete graphical HUD | Text/countdown/notices/directions accepted where exercised; sprite/flush observer and separate bounded per-draw HUD metadata collector have host fixture coverage. Native collection and capture remain unqualified. | Qualify naturally scheduled target, alpha/coverage and retained lifetime. Observe once; never replay traversal for the second eye. Metadata does not admit capture/occlusion. |
| G4 — transport recovery and pacing | Normal transport/quit accepted in scope; independent consumer-query recovery host-tested. | Qualify native reset/device-loss and teardown/recreation when retirement proof fails. Retain uncertain leases, reject stale generations; measure sustained frame-age/recording tails. |
| G5 — persisted verdicts | Host-tested; rejection persistence live-tested through collection/restoration. | Exercise successful full-profile collection during a relevant candidate. Preserve input hashes, ordered run/build intervals and stale-input rejection. Collection cannot accept missing gestures or physical gates. |

## Next acceptance gates for implemented changes

Use [the pending checklist](VALIDATION.md#pending-headset-checks) as the single
physical checklist. It prioritizes desktop output, refused-skip stability, the
shared provisional
reload marker and fingers after completion. Broader model, duel-coach, bow-guide
and interruption checks remain conditional on naturally available scenes.
Unavailable campaign scenes remain pending/inconclusive; do not split them into
repeated small sessions. Host-tested revisions remain unaccepted physically;
earlier scoped acceptance
is preserved and must not be reclassified as untested.

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

The official v1.0.0 target is a playable original campaign from New Game
through the ending on the supported Steam/OpenVR configuration. Prioritize
core controls, combat and equipment. The operator reports working chapter
transitions, checkpoints, recovery after death and secret scrolls in the
exercised campaign. Preserve those paths during the end-to-end playthrough.
Temporally reconstructed output, DLSS and DLAA remain experimental and outside
this gate.
See the [campaign acceptance checklist](VALIDATION.md#pending-headset-checks).
The package workflow accepts stable and pre-alpha versions and creates drafts.
The planned public configuration uses the existing manual routes on by default
for new installs, retains installed choices and offers native reload via Off.
The Schofield motion and Winchester receiver development options remain
outside this configuration until their physical gates and the campaign support
including them. Clean install/update/recovery on a player PC remains required
before publication. Qualify pacing with a representative gameplay run and
record the active transport, display mode, resolution and enabled probes.

Keep published .5 artifacts and verdicts unchanged. Future distributions retain
reversible installation, saved resolution/reload choices, local-only diagnostics,
original-asset exclusion, GPL-3.0-or-later corresponding source and notices.
Windows 10/11 x64 packaging retains the x86 game/runtime and PowerShell 5.1
transaction engine. The .7 package includes the graphical front end, with
verified local packaging and isolated exact-game integration; broader interactive
operator acceptance remains separate. Manual reload should
retain separate installed capability and runtime native Options > Game intent;
the selector, persistence and safe transitions have host coverage and await live
qualification. No automatic updater is
planned. Release notes own package history;
publication cannot promote hardware, campaign, rendering or recovery acceptance.
