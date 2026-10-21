# Roadmap

The active target is original Steam Call of Juarez (2006), Windows x86, native
D3D9 stereo through OpenVR/SteamVR. PS VR2 Sense is the development reference.
[Validation](VALIDATION.md) owns acceptance and unresolved physical gates;
[Architecture](ARCHITECTURE.md) owns integration contracts.

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`,
`headset-validated`, `supported`. A working build does not imply visor support.

## Current gate

The published `v0.1.0-prealpha.4` baseline has scoped PS VR2 Sense acceptance
for native stereo and head tracking, movement/smooth turning, independent hands,
physical Hands combat, manual revolver unit loading, controller menus, encountered
HUD and subtitles, two reticles, and the exercised Clyde and Ray mechanics.
Ordinary long-gun reload remains native and automatic. Release details and
specific acceptance limits live in [Validation](VALIDATION.md#current-feature-state);
the package is not qualified for full-campaign or other-headset support.

The operator has also accepted recovery after Hope and multiple exercised
cinematics, including hot theater/immersive switches and resumed gameplay.
The capture-ring reclamation that addressed the Hope report must be preserved.
Neither that declaration nor successful normal quit qualifies all cinematics,
unreported subtitle modes/timing, focus/dashboard/tracking loss or device resets.
Already accepted Hope and ordinary reload/menu behavior need no repeat test.

Current unpublished work includes a shared version-3 preference owner and native
Options > Game cinematic/turning/holster-guide selectors, a physical right-hand duel posture
guard, reload thumb/continuity corrections, a two-hand rifle support prototype
and improved launcher resolution reporting. These are implemented/host-tested,
with scoped operator acceptance of native option usability and one successful
physical duel draw/raise and rejection of tested incorrect/cheating postures.
Hand contact and unreported timing/recovery remain open. The solver is now connected to exact Winchester, Remington
and SawOff native rifle transactions, with host coverage and physical acceptance
pending. The published .4 download does not contain these later changes.

The offline batch additionally implements module-owned immersive subtitle text
and native-cache bow concentration magnification, both host-tested only. The
operator confirms subtitle appearance in exercised animations, with rendering
mode unspecified. Unreported movie timing/modes and bow alignment/comfort join the same naturally eligible
campaign session. The reload observer correlates stable native drum phase with
geometry; duel diagnostics observe original inventory holster sockets. Neither
diagnostic enables chamber mechanics or physical holster contact.
The read-only rifle support observer and evidence analysis are host-tested.
The prototype acquires its natural idle support socket on a fresh nearby L1,
retains that weapon-local reference through native fire/cycle, and cancels for
reload, invalid geometry or changed ownership. Placement, stability and recovery
join that same session; automatic native rifle reload is preserved.

The operator now reports the bow worked very well in exercised play, extending
the earlier substantial improvement after separating renderer capture drops
from the lifetime of a valid physical grab. Earlier trials
with no arrows or frequent tension cuts remain rejected historical trials; they
do not describe the current candidate. Sustained consistency, mesh/string/arrow/
reticle agreement, haptics and interruption recovery remain physical gates.
The exact WeaponBow-only root/+Z and DBowHolder correction, stable native hand
owner, right-hand contact/R2/retraction/release policy, native channel supervision
and controller vibration transport are implemented with host coverage. Slack or
off-axis motion relaxes tension without dropping capture. The held draw guards
only exact native concentration-expiry cancellation; original concentration
duration, zoom/slomo, ballistic behavior and release remain native. Restoration
failure, invalid ownership and tracking discontinuities still cancel the grab.
bHaptics is deferred at the operator's request: no owned hardware and no urgency.
Its existing foundation remains available without taking time from gameplay.
The [official C/C++ SDK 2.7.1](https://github.com/bhaptics/tact-cpp2/releases/tag/v2.7.1)
provides Windows x86 libraries; an optional provider must isolate blocking SDK
calls and fail independently of OpenVR controller haptics. Begin with neutral
event admission and a recording/fault sink, now host-tested with a QA intent only.
No SDK provider or gameplay producer is connected. A current virtual-device procedure
and native-confirmed gameplay event producers remain to qualify; Designer
preview or virtual output cannot qualify physical vest feedback.

A community secret-scroll report identifies a null ambient current level in
native mesh rendering. Conditional preservation of the same still-active render
level is implemented/host-tested; the exact clearing callback and campaign
recovery remain unobserved. Continuous HMD body following also replaces the
former 35-degree cone and has scoped operator headset acceptance. Physical
walking/turning still shows static sliding legs; presentation animation is
planned separately, preserving accepted body following and native movement. The
bounded render-fault reporter retains original exception propagation. Signed
32-bit native identity recovery has scoped functional acceptance, but actual high-address
owners remain untested. Real OneDrive operation, other controllers/headsets,
long-gun interactions, mounted play and campaign-specific mechanics retain
independent gates.
Mod-added DLC compatibility is deferred at the operator's request after an
unsatisfactory VR trial. Mounting/dismounting a horse worked in that exercised
context; mounted movement, camera/aim continuity and broader DLC behavior remain
unqualified and join later campaign coverage.

## Development backlog

The ordered work below continues offline where its exact-game prerequisite is
available. [Pending headset checks](VALIDATION.md#pending-headset-checks) owns the
physical checklist; an unobserved scene does not block unrelated development.

The [reference-project assessment](research/COJ_VR_REFERENCE_PROJECTS.md) records
the inspected sources, their concrete contribution and limits. Prioritize
measured cartridge contact, rigid support grip and exact reload ownership before
adding optional grip blending, virtual stock or device-aware contextual prompts.
No third-party renderer or physics framework is added by that research.

The audit's actor-replacement pelvis corruption, lost diagnostic-arm restore,
hidden-file recovery deletion and consumed-bow-trigger loss are corrected with
host regressions. Body transactions retain their original actor and verified
preimage, including partial writes and translated retries. Recovery preserves
unknown directory contents and journals; staging rejects an unrepresentable
original directory before mutation. These repairs are not open implementation
items and do not close the physical interruption gates.

The subsequent physical attempt did not acquire Remington support and did not
enter a manual pistol session. Live Remington `Holder` resolves to element zero;
the support admission's positive-only check rejected this valid root socket.
That filter is corrected with root-holder geometry/JNI regressions; physical
retest is pending. The trace identifies an unsupported Derringer, not every
pistol the operator tried. Existing dual/whole-clip/multiple-ammo exclusions
remain: the current run cannot qualify cartridge fit or reload feedback.
The operator also reports L2 firing the right-owned Remington. Native Attack's
empty-hand fallback explains this independently of support acquisition. A
host-tested ordinary-fire ownership filter now blocks that L2 route for exact
right-only rifles and retains consumed holds until physical release. Physical
absence of accidental shots, R2 firing and held-trigger recovery remain pending.
Basic holsters have positive operator feedback with occasional unwanted kicks.
Conflicting/stale zone presses are now consumed through release before native
selection admission; manual reload owns ammunition before holsters. Host tests
reproduce the earlier kick leakage; the corrected delivery still needs a retest.
Optional guides expose available/near/unavailable zones, while an anatomical
holstered weapon model and a distinct ammunition pickup cue remain future work.

Immediate work, in order:

1. **Remaining campaign continuity and ownership recovery.** Preserve accepted
   Hope/multi-cinematic recovery and hot switching. Broaden coverage only for
   unexercised campaign and ownership boundaries; accepted recovery needs no repeat.
   Preserve the chest/scroll diagnostic and qualify the current-level correction
   on the next naturally encountered scroll, including menu closure and resumed
   control/stereo; the latest session did not exercise chests. Preserve accepted
   continuous body following and investigate room-scale leg animation separately.
   Preserve the bow's improved capture lifetime; use remaining alignment and
   recovery evidence to choose its next correction. Basic firing is no longer
   recorded as wholly untested. Batch focus/tracking/dashboard and held-control recovery;
   an accepted ordinary menu or quit does not close those gates.
2. **Make the held cartridge convincing.** Preserve the accepted reload and
   textured CC0 .357. Its revised skin-derived anchor is host-tested and has
   bounded live delivery evidence, but the latest close-up does not establish
   finger contact; the operator still reports a floating cartridge. Offline skin
   replay finds slight pad/case overlap, so another position offset is unjustified.
   The requested short support-thumb gesture is implemented and host-tested:
   native-confirmed insertion starts a small flex and recovery while the cartridge
   stays hidden, retaining one native transfer and scoped finger recovery.
   The composed session/feedback path now distinguishes an unclaimed continuation
   token from a physically held cartridge, so the token cannot prematurely cancel
   that gesture. Active carries reject discontinuous translation of either hand
   while retaining the original trigger claim through release; normal sampled
   paths and initial/continuing transfers have host coverage.
   Judge its appearance alongside the revised axis in the consolidated candidate.
   Qualify a safe hand/scene-occlusion boundary and a measured
   cartridge axis relative to displayed fingers; avoid further arbitrary offset
   tuning. A measured tangent axis is now implemented and host-tested for both
   hands; its visual fit is still unobserved. A bounded read-only D3D9 depth
   observer is ready to collect the next batch, without enabling occlusion.
   The current compositor has no native scene depth. Batch one physical
   grip/roll/release/insertion/recovery check after a material correction.
3. **Qualify the native loading port.** Existing read-only root/barrel/gate/drum
   observations show changed gate pose and independent drum motion. Displayed
   six-mouth pivot rankings have bounded live evidence, but a pivot is not a
   socket. The operator accepts the port's visible presentation; next establish
   a measured phase-bound mouth frame/clearance before enabling precise insertion
   or any independent mechanical writer. The current grip zone stays active.
4. **Close relevant reload and interaction gates.** Square rearming across
   ownership/availability loss and conventional recovery after disable are
   strengthened and host-tested. Qualified zone-entry feedback is host-tested;
   native insertion retains its stronger independently gated pulse.
   Direct native completion/fresh opening clear old continuation tokens, and
   synchronous failed insertions yield presentation/supervision with accurate
   cancellation reasons; these corrections are implemented/host-tested.
   Batch manual tracking,
   held-control, save/load, death/retry and disable checks; broaden mirrored and
   Frontier coverage when available. The replacement left-Sense L1 ray and
   held-only cyan reference still need acceptance, independent of the earlier
   successful HMD-directed drawer interaction.
5. **Broaden campaign and performance coverage.** Mounted controls, other weapons,
   special optics/duels/climbing and optional HUD rows need eligible scenes.
   Current gameplay performance has positive operator feedback; defer tuning
   until evidence identifies a problem. The cause of recording-associated
   microskips is unresolved, including possible equipment/configuration limits.
   Measure recording and sustained transport recovery from evidence. Protected-target
   red-cross visibility is low priority.

No new test is required merely to reconfirm cartridge visibility, native unit
loading or already accepted Square/menu/weapon behavior. Horse mounting and
dismounting have scoped functional acceptance; mounted movement, gallop,
camera/aim and HUD remain open. Unavailable campaign actions remain
pending/inconclusive; Ray's exercised on-foot kick does not override native
permission restrictions elsewhere.

Additional implementation remains bounded by the following prerequisites:

| Development | Current boundary and next prerequisite |
| --- | --- |
| Physical-walk/turn leg animation | Planned. The inspected native root/SetFrame routes also advance child animation and notifications, including footsteps/MakeNoise; they cannot serve as a simple legs-only writer. Qualify an event-free sampler or complete reversible lower-body transaction for both models, preserving accepted continuous follow and native movement/permissions. |
| Rifle two-hand attachment | Exact Winchester/Remington/SawOff attachment is implemented/host-tested through one scoped paired-hand/weapon/aim transaction, acquired from the natural idle support socket. Physical placement, firing/cycling stability and interruption recovery remain pending; native automatic reload is preserved. |
| Physical holster contact | Physical duel posture guard exists. Native holster sockets are identified, but grasp volume and displayed-body alignment must be measured before socket-based contact enables drawing. Basic gesture zones below are independent of this geometry. |
| Basic virtual holsters | Ergonomic HMD-relative hip zones for pistols, right shoulder for rifle and left shoulder for bow are implemented/host-tested independently of model sockets or attached meshes. Optional colored guides share their geometry, default off in native comfort options. Physical zone fit, guide comfort and Action/Kick coexistence join the consolidated candidate; precise socket contact remains separate future work. |
| Real drum/chambers and ejection | Loading and drum rotation are separate native operations. Establish phase-bound mouth geometry, occupancy and native mechanical ownership before precise insertion or independent cylinder/ejection writers. |
| Remaining visual integration | Cartridge contact/occlusion and full graphical HUD follow G1/G3 below; existing visibility and text acceptance stand. |
| Deferred work | Mod-added DLC VR adaptation and bHaptics provider/event producers are deferred by operator priority. OpenXR, D3D10 and additional games remain separate future tracks. |

A pending visual/physical gate blocks promotion of that specific behavior only.
Continue independent offline work on controls, UI, recovery and other qualified
boundaries while waiting; accumulate relevant checks into one substantial
candidate rather than requesting repeated small physical sessions.

### Weapon implementation inventory

This table separates existing behaviour from work that still needs code or
research. Physical checklists remain in Validation. Exact constructor, capacity,
reserve and callback facts have one owner: [reload research](research/COJ_RELOAD_OWNERSHIP.md).

| Weapon/family | Existing implementation/evidence | Remaining development, in dependency order |
| --- | --- | --- |
| Peacemaker and admitted Frontier1878 | Persistent native unit-loading route with scoped acceptance. Changed cartridge tangent/thumb feedback and newer pad/mesh diagnostics have host coverage only where not previously exercised. | Resolve visible pinch/contact/occlusion; qualify real phase-bound loading mouth, chamber occupancy and spent-case/ejection ownership; insertion-specific sound requires an audited sound-only route. Do not duplicate native transfers. |
| Frontier, Walker, Cogswell | Exact single-ammo manual extensions are host-tested; Cogswell uses rifle reserve. | Qualify per-model hand/port geometry, visual ammunition and interruption behaviour; current .357 visual is not evidence of correct calibre for every weapon. |
| Winchester | Exact support grip and passive native reload observer are host-tested. Square still performs conventional automatic reload. | First observe native hand/animation/ammo/reserve through begin/loop/end; correlate the native ammo field with HUD and qualify a loading-port frame; then implement a separate supervised manual rifle session and deliberate regrip. Manual loading is not implemented. |
| Scoped Winchester | Native fallback; deliberately excluded from the support and manual whitelist. | Qualify exact class, optic alignment/zoom, both-hand ownership and its own reload/support adapter before enabling either interaction. |
| Remington and SawOff | Exact two-hand support prototype host-tested; native Square remains whole-clip/two-slot. | Demonstrate index-specific single-shell transfer and per-barrel readback/callback ownership before manual loading; separately qualify opening, port/ejection and either-hand SawOff recovery. |
| Volcanic | Native fallback; static single-index/per-round contract identified. | Observe native loading states/pose, qualify hand/port and effective ammo/reserve transaction, then implement its own manual profile. |
| Derringer | Native whole-clip/two-index fallback. | Establish per-chamber transaction, opening and ejection; one existing native call may fill both chambers, so it cannot represent one cartridge. |
| Lemant (archive spelling; LeMat) | Native fallback with separate rifle/pistol indices and reserve pools. | Selective index transfer and mode ownership, distinct loading geometry/gestures and both reserves; existing unit helper must not load both types for one gesture. |
| Schofield base/A/B | Native/hybrid whole-clip behaviour retained. | Qualify a deliberate full-clip gesture and readback, or establish selective transfer before a per-round design. Not part of the current manual session. |
| Bow | Physical string grab/pull/release and native shot-cache alignment host-tested; operator reports improved capture lifetime. | Use remaining mesh/string/arrow/reticle and interruption evidence to choose corrections. Keep native concentration duration, trajectory and ammunition. No new bow physics is required for the rifle/reload run. |
| Hands, Bible/whip, dynamite and mounted weapons | Physical Hands combat and Ray Bible/on-foot kick have scoped acceptance; conventional equipment/input routes exist. | Hands: volume/guard/AI anticipation. Whip/climbing, dynamite throws and cannon/mounted aim: discover exact native permissions/contact/ownership for physical interactions before adding gestures. An input binding alone is not implementation of a physical mechanic. |

### Other implementation inventory

| Area | Implemented boundary | Development still pending |
| --- | --- | --- |
| Hand visuals | Skin-derived pinch/tangent and optional proximity/axial measurements | Convincing contact and actual hand/scene occlusion; native loading-finger-pose reuse needs scoped ownership. |
| Body | Accepted continuous HMD following with native torso/legs and independent hands | Event-free room-scale walk/turn leg animation; root/SetFrame currently advances callbacks/footstep noise. Connected-arm IK remains rejected. |
| UI and controls | Native VR options, localized wheel, text HUD, optional holster guides and basic grip zones | Device/binding-aware transient glyphs; precise body/socket holsters; complete graphical HUD/sprite capture; unimplemented comfort options only after demonstrated need. |
| Rendering/performance | Active D3D9/OpenVR stereo and existing timing/transport diagnostics | Qualify independent target ownership, effects, memory and retirement before resolution prototype promotion; attribute recording stalls before optimization. New diagnostic runs do not qualify performance. |
| Campaign/recovery | Scoped accepted movement/menu/cinematic behaviour; owner/restore and input barriers have host coverage | Extend only unobserved tracking/focus/save/death/scroll/mounted boundaries. Use evidence to select fixes; unavailable scenes remain pending. |
| Deferred tracks | Foundations documented | bHaptics provider/event producers and DLC adaptation deferred by operator priority; OpenXR/D3D10 and additional games remain separate future work. |

## Parallel engineering tracks

These tracks are at different implementation and evidence stages. They can
advance offline without displacing campaign continuity, physical interactions
or reload work:

- **VR render resolution:** the [source trace and exits](research/COJ_VR_RENDER_RESOLUTION.md)
  identify the native target-family allocation/binding boundary before full eye
  renders. Qualify its color/depth/intermediate ownership before a restart-only
  independent-size prototype. Larger transport textures alone are not higher
  resolution rendering. Add a shared scale preference only after effective
  rendering, memory bounds and failure/restoration behavior are implemented.
  The [approved design](research/COJ_VR_INDEPENDENT_RESOLUTION_DESIGN.md)
  specifies same-size isolation before a fixed 2560x1920 experiment. Its opt-in
  bounded per-eye binding observer and conservative analyzer are implemented
  and host-tested; bounded gameplay collection now has live evidence and scoped
  operator confirmation of functional continuity at the original size. The
  analyzer now classifies the observed FourCC `NULL` placeholder/D16 depth layout
  and reads complete gameplay pairs from the existing trace, keeping unknown
  layouts rejected. Continue with native creation/selection/lifetime and actual
  effect/projection roles; binding classification alone does not qualify them.
  The [native ownership map](research/COJ_PPFX_TARGET_OWNERSHIP.md) now resolves
  destructive creation, alias-aware release, effect references and renderer
  internal/default/depth caches, including integer-buffer versus float-quad
  rounding. Qualify constants/history, complete sprite ordering and a private-bank
  transaction before same-size isolation; bounded native/COM correlation below
  supplies structural use evidence only.
  The collector separates device generation from its reset-attempt epoch and
  the analyzer checks both; older generation namespaces remain unqualified.
  Native descriptor/cache snapshots and their separate binding correlation are
  now implemented/host-tested, including exact classes, bounded lists/names,
  aliases, LAA and failed-read rejection. Route reservations now have live
  evidence: two complete pairs per gameplay/settings/immersive-movie route,
  matching native snapshots and use of all 14 `scene30.ppfx` descriptors.
  Default/internal surface and placeholder identities correlate as well. One
  settings frame qualifies sprite/flush order and matching device generation;
  this does not establish sprite coverage or generalize ordering to other frames.
  Native allocation/lifetime/effect/constant mapping and compositor retirement
  still block Q0. Original-size settings/movie structural correlation is now
  live-tested; independent-size continuity and headset A/B remain pending.
  The local default-off writer now implements same-size isolation and fixed
  2560x1920 private rendering, with real-GPU/synthetic-native host coverage.
  Complete its canonical B0 then B1 physical gates, effective geometry/optics,
  detail and performance measurements before acceptance. Fixed outputs retain
  bounded process-lifetime ownership; this is not compositor retirement proof.
  It remains default-off and unaccepted in the .2 consolidation.
  The first B0 operator run is rejected: queue admission prevented every stereo
  pair and the shared admission gate disabled gameplay input. Keep the prototype
  off, recover ordinary VR and resolve the native rejection and usable fallback
  before any B1 trial. Startup/global-input success does not close this gate.
  The ordinary native route now has measured 2804x2860 raster/capture/output
  extents and repeated scoped headset acceptance for clarity and observed pacing.
  Keep Manual 2804x2860 as the operator's local PS VR2 default; no repeat physical
  test is requested. The isolated MenuMain GPU correction resolves the severe menu
  slowdown, and the operator accepts Ray's 8 cm visual setback with the coat
  partition retained. Preserve the established hands, interactions and stereo.
  Earlier full-scene flashes have not recurred in subsequent exercised sessions,
  but their cause is unresolved; retain default-off observation tools.

  [Startup profiles](research/COJ_VR_RELEASE_RESOLUTION_PROFILES.md) now include
  persistent launcher selection, native bounds and layered video recovery, with
  host and isolated exact-game installation coverage. Other dimensions and general
  recommendation/memory limits remain unqualified. In .3, missing preferences use
  Auto, relative to the helper recommendation rather than panel-native pixels.
  Every explicit saved profile, including Manual and Preserve, survives updates;
  unavailable queries/incompatible caps preserve selected size with a visible
  explanation and retain intent for retry next preparation. Start SteamVR manually
  before Install/update or Prepare session for Auto/percentages. The .2/.1 archives
  and archived verifier verdicts remain unchanged. Next work follows
  unresolved campaign/profile gates; another Ray-fit or identical resolution
  candidate is not pending.
- **Native VR settings:** a shared versioned preference owner and three selectors
  in the existing Options > Game page are implemented, preserving original fonts,
  native OK/Cancel/Defaults and main/pause return routes. Cinematics and smooth/snap
  turning retain scoped acceptance. Optional holster guides default off; their new
  comfort row is host-tested. Version 1/2 choices migrate to version 3 with guides
  off. Labels follow the game's locale, including English/Spanish coverage.
  The [native bridge](research/COJ_UI_MOUSE_PATH.md#native-vr-settings-integration-boundary)
  consumes requests before side effects, rejects stale editors and reports save
  failure without closing the page. Host behavior, shipped-JVM bytecode and package
  inventory verification pass; native option usability has scoped operator
  acceptance. Unspecified persistence and held-input recovery remain open. The existing panel stays
  usable during development.
- **Physical duel draw:** the production mouse route now requires a fresh lowered
  right aim tip and centered stick before draw, then a physically raised tip
  before upward raise input. Host tests cover the pre-aimed-hand shortcut and
  tracking/owner recovery without changing native draw timing or permissions.
  Correct hand-at-hip draw/raise and rejection of exercised wrong/cheating
  positions have operator acceptance. Unreported early timing and broader native
  retry behaviour remain pending in the consolidated batch;
  this is not yet a measured holster interaction.
  Original model/inventory evidence identifies `HolsterR` for Ray/Billy and
  `HolsterL` for Ray. Natural pre-body frame diagnostics are available in the same
  duel; qualify grasp placement and visual-body alignment before enabling contact.
  The operator separately requested basic gesture zones without visible attached
  weapons. Those now reuse native pistol/rifle/bow toggles at the corresponding
  hip or right/left shoulder, with host-tested grip claims and native permission guards.
  Optional colored zone guides and procedural Western wheel styling have host
  coverage; fit and readability join normal equipment use in the same session.
  They are ordinary gameplay shortcuts and do not bypass duel timing/posture.
- **Two-hand long-gun support:** the host-tested prototype uses the naturally
  animated left holding socket relative to an exact right-owned Winchester,
  Remington or SawOff. The meshes have no named secondary holder; no arbitrary
  foregrip offset is introduced. Fresh nearby L1 acquires while idle; one rigid
  map preserves authored roll and scale across both hands, weapon and muzzle.
  Native fire/cycle retain the acquired local contact; reload/equip, changed
  object, invalid tracking or excessive separation cancel. Real bridge fixtures
  cover both-parent-before-child commit, same-ID replacement rejection and
  retained restoration. L1/L2 claims suppress competing consumers until release.
  Test grasp fit, fire/cycle coherence and interruption recovery together in visor.
  Scope variants and physical manual long-gun loading remain unimplemented.
  Original-archive reload research identifies Winchester's single-round rifle
  path as the first candidate for a separate manual session. Remington and
  SawOff use two-slot whole-clip native transfers, so they need indexed shell
  ownership before individual insertions. Volcanic is also unit-paced;
  Derringer and Lemant need distinct multi-slot/mixed-ammo handling.
  [Exact reload ownership](research/COJ_RELOAD_OWNERSHIP.md) records the
  evidence and prerequisites. No new weapon is manually admitted.
  The exact Winchester read-only observer is now implemented/host-tested,
  including offline same-owner clock/delta validation. Native phase,
  clip-count/HUD agreement and support motion await live correlation; a sparse sample or
  reserve decrement does not grant manual admission.
- **Automatic startup resolution:** preserve the current native startup requests
  and explicit saved profiles. The launcher now explains known fallback causes
  and reports the last identity-matched preparation separately from next-session
  intent. Configuration is a request; actual native raster remains unobserved
  by the launcher. No renderer change follows from reporting improvements.

## Next implementation gates

The reload sequence remains G1 then G2. G3-G5 are independent engineering work;
they can advance while native/physical evidence is unavailable. The exit criteria
below authorize implementation or host-tested state only. Runtime and visor
acceptance still require [Validation](VALIDATION.md).

| Gate | Actual starting state | Next implementation and prerequisite | Exit criteria |
| --- | --- | --- | --- |
| **G1 — cartridge contact and occlusion** | Skin-derived centre has bounded live evidence; displayed-finger tangent, native-confirmed support-thumb gesture and read-only depth observer are host-tested. Operator still reports floating contact. Offline pad/case overlap does not justify a position offset. Hand/scene occlusion is planned. | Physically judge the revised axis and scoped insertion gesture together. For occlusion, qualify exact per-eye depth contents, hand coverage, projection, format and lifetime before renderer-owned capture/draw/transport. If scene depth is unsuitable, qualify native-hand depth separately. | Contact/gesture host coverage preserves both hands, native unit transfer, rejection, owner/tracking loss and scoped restoration. Closing status retains native priority. Occlusion additionally requires both-eye ordering, exact capture/pose/generation correlation and reset/invalid-data rejection before a candidate. Physical pad contact, axis/scale, wrist roll and thumb feedback join the consolidated batch; metadata or offline skin replay cannot accept them. |
| **G2 — native port and precise tip insertion** | Visible port presentation has scoped operator acceptance. Gate/drum and pivot ranking are live-observed diagnostics. Neutral socket policy is synthetic-tested but unwired in CoJ; active insertion remains the 14 cm grip zone. | Establish a measured loading-mouth frame and clearance through actual drum phase. Only then supply a qualified game-adapter socket and cartridge tip/trajectory to the existing policy. Preserve false socket admission until qualified. | Stale/ambiguous owners, closed gate, wrong approach and misses reject. A deliberate aligned release calls native loading once, with loaded +1/reserve -1 and restored hand/weapon state. Do not infer chamber occupancy or enable mechanical writers/ejection from this gate. |
| **G3 — complete native graphical HUD** | Native sprite/flush observer is host-tested; text, compass and wrist cards are implemented. Independent countdown, encountered objective/journal notices and corrected green direction/red damage chevrons have scoped headset acceptance. Native graphical capture/composition is planned. | Qualify the naturally scheduled sprite pass, batch-flush lifetime, target ownership and usable alpha/coverage before renderer capture. Observe once; never replay traversal for the second eye. | Capture follows native visibility/settings, preserves callbacks/state, clears on owner/generation loss and composes coherently for both eyes. Synthetic raster tests precede live scheduling and visor readability acceptance. |
| **G4 — transport recovery and sustained pacing** | Normal transport/quit has scoped acceptance; operator reports good current gameplay performance without a new recording test. Independent ordered consumer-query recovery is implemented/host-tested, including multiple pending leases, ResetEx and source/consumer replacement; failed recovery proof retains quarantine. Original errors remain visible to the canonical verifier. | Qualify exact-game reset/device-loss and session teardown/recreation when the independent proof is unavailable. Reacquire transport only after old GPU work is proven retired or its owning session safely destroyed. Measure recording and sustained frame-age tails before choosing a performance fix. | Host faults retain uncertain leases, reject stale generations and mismatched contexts, and defer replacement until retirement. Healthy shutdown retains matching copy/completion totals and zero abandoned leases. Native hook restoration, transition/recovery and sustained recorded/unrecorded pacing remain separate physical gates. |
| **G5 — persisted validation verdicts** | Implemented/host-tested; canonical rejection persistence is live-tested through collection and original restoration. Inventoried results bind source/build/run/profile, requirements and exact input hashes; operator observations remain separate. Ordered exact run/build intervals and rejection of outside telemetry are host-tested only. | Preserve rejection and stale-input fences. Exercise successful full-profile workflow coverage during the next relevant candidate; no dedicated headset session is required. Collection without verification remains inconclusive. | Host fixtures cover pass/fail/inconclusive, partial runs, duplicate/foreign/reversed markers, outside telemetry, stale identity, ZIP/hash integrity and real `finish` restoration. Live collection retains a failed verifier despite a complete run and scoped operator acceptance. Missing required gestures cannot become a pass through collection or notes. The report always leaves headset acceptance unevaluated. |

G1 depth metadata collection and any needed G2 socket qualification join the next
candidate. Offline ordered eye-pair correlation is host-tested, but capture/depth
contents and hand coverage remain unqualified; no repeat port-visibility probe is required.
Native loading-pose reuse is a
measured research option, not an authorized animation-time writer. Loading sound,
spent-case ejection, independent cylinder/gate control and chamber occupancy need
their own exact-game contracts after port qualification. Bow concentration optics
are implemented/host-tested, awaiting physical alignment/comfort acceptance;
scoped Winchester optics and physical-walk animation remain planned integration.
Backend/game expansion remains Milestone 5.

## Next acceptance gates for implemented changes

The following work already exists and is host-tested. Include it in one fresh
canonical candidate instead of reimplementing it or requesting separate short
headset sessions:

- **Reload:** tangent fit, zone-entry cue, per-owner Square rearming, disable
  recovery, new-session waist pickup and synchronous insertion rejection. Check
  held triggers/Square across tracking/recenter/menu/weapon loss and available
  save/load/death; require no stale token, extra transfer, closing-press shot or
  permanent lock. Broaden left-hand/Frontier use when available.
- **Interaction and HUD:** left-Sense L1 ray independent of gaze, held-only cyan
  reference, pickup/carry/put-down, and wrist-meter changes after available damage
  or healing. Concentration state glyphs and paired horse health/fatigue meters
  are implemented/host-tested; include them only when those contexts are available
  in the combined session. Prior interaction/text-card acceptance does not qualify them.
- **Global ownership:** menu -> gameplay -> wheel -> pause -> dashboard ->
  gameplay with held mapped controls, tracking loss and eligible source resize.
  Require fresh release/press, no delayed action/haptic replay and coherent eyes.
- **Loading continuation:** the operator reports Circle/Options work. Cross and
  trigger selection now deliver the key-down required by the exact loading UI;
  the previous release-only call was ignored. Both routes and failure cleanup are
  host-tested. Check one fresh Cross/trigger continuation in the next relevant
  combined session, preserving held-input barriers and native post-load ownership.

The expanded Frontier/Walker/Cogswell manual profiles are implemented/host-tested,
including exact class admission and Cogswell's rifle-reserve readback. Add their
native unit transfer, closing and recovery to the same campaign batch when found.
Schofield base/A/B retain native whole-clip handling; LeMat's separate ammo types
remain outside manual loading. No new profile receives socket/geometry acceptance.
English/Spanish authored HUD/wheel/gesture text now follows the game's startup
locale, with consistent production UTF-8 and bounded English fallback. Wording,
accent glyphs and surface fit join that batch; native copied text is preserved.

Mounted routing, campaign permission transitions, special weapons/duels/climbing and optional
HUD rows need eligible campaign scenes; missing observations are pending or
inconclusive. General LAA high-address compatibility, broad allocation pressure,
abnormal exit and device loss remain open even though an exercised loading
transition and normal quit are accepted.

## Stabilization baseline

Preserve exact-build SHA-256 admission, native inventory/permissions and scoped
camera/body/weapon restoration. The presentation remains head/connected arms
hidden, independent hands, native torso/legs. No new IK, arm reconstruction,
ammo replica, full physics simulation or backend migration is part of the
current reload iteration.

Canonical `prepare`/`finish` remains the reversible physical workflow. Keep
configured SDKs and recovery data; consume local evidence into durable findings
and remove obsolete large copies. A fresh run identity is required for each new
candidate, and the operator starts/closes the game and SteamVR.

## Milestone 1 — native stereo and HMD

Achieved in exercised contexts: per-eye rendering, tracked head, recenter,
room-scale motion and stereo recovery after Hope and multiple cinematics,
including hot mode switches. Finish broad loading/reset/transition coverage,
sustained pacing and recording compatibility before public support claims.

## Milestone 2 — player body and comfort

Exercised native movement/jump parity, head-relative locomotion, physical crouch
and corrected snap turning are achieved. Mounted camera continuity and broader
ownership recovery remain open; physical-walk animation is planned.

## Milestone 3 — tracked hands and body IK

Independent native hands/weapons with original torso/legs are active and accepted
where exercised. Expand left/dual/special weapon and finger-pose coverage.
Connected-arm IK remains a rejected diagnostic presentation, not a production
requirement or completed milestone.

## Milestone 4 — controller UI and interactions

Exercised menus, wheel/haptics, Focus, Create timing, compass and HUD/subtitles
are achieved. Recovered countdown/notices, corrected attack directions and Ray's
Bible/on-foot kick have scoped headset acceptance. Persistent manual reload has
scoped acceptance; convincing grip,
precise chamber loading, measured mouth clearance and broader recovery remain open.
Spent-case ejection and complete graphical HUD capture are planned. Zone-entry
feedback is implemented/host-tested, awaiting physical acceptance.
Direct native unit loading is already implemented and scoped headset-validated
through the manual grip-zone gesture. Insertion-specific sound and precise
port/chamber loading remain pending; neither blocks that functional prototype.

Offline work outside the Peacemaker reload now includes a proportional wrist
health meter and low-health text colors, derived from the owned native HUD cache.
Both are implemented/host-tested with bounded invalid-data fallback; readability
and live percentage updates join the next consolidated gameplay acceptance batch.
The existing text card's headset acceptance does not qualify this new styling.
Discrete concentration glyphs and paired horse health/fatigue meters now extend
the same admitted native-cache route, preserving row priority and text fallback.
They are implemented/host-tested only; no native graphical pass is captured and
no horse or concentration mechanic receives additional acceptance.
Global-button release barriers and failed-input rollback, partial stereo-submit
state recovery and pointer source-size/in-flight cancellation are also
implemented/host-tested. Physical focus/dashboard/tracking and eligible resize
recovery remain in the combined menu/gameplay gate.

Current physical `WeaponHands` combat uses fresh displayed-knuckle contact and
adjacent-pose motion, retaining native attack permissions, cooldown and damage.
It has scoped headset acceptance in the exercised three-enemy fight; the earlier
completed-curve implementation is superseded. Broader hand/campaign and ownership
recovery remain pending. Full fist volume, guard/blocking and AI anticipation of
the physical wind-up remain planned. Dual pistol reload retains its existing
behavior. [Validation](VALIDATION.md#current-feature-state) owns acceptance limits.

## Milestone 5 — additional renderers and games

OpenXR is experimental; D3D10 is a separate future renderer. Bound in Blood and
Gunslinger are planned. Reuse game-neutral policy only; Chrome Engine behavior
requires a second independent game demonstration before generalization.

## Release direction

The latest published package is `v0.1.0-prealpha.4`, cumulative from .3/.2/.1.
[Release notes](../release/RELEASE_NOTES.md) own its changes and controls;
[Validation](VALIDATION.md#public-prerelease-policy) owns scoped PS VR2 acceptance,
community observations and unresolved risks. Native settings, physical duel
posture, reload feedback/continuity, two-hand solver, continuous body follow and
ambient render-context preservation work described above
remain unpublished and do not inherit the .4 acceptance.

Future distributions must preserve exact-build rejection, reversible archive
installation, independent saved resolution/reload preferences, local-only
diagnostics, original-asset exclusion, GPL-3.0-or-later corresponding source and
third-party notices. Keep the Windows 10/11 x64 offline package with its x86
game/runtime dependencies and PowerShell 5.1 launcher. Game and SteamVR lifecycle
remain manual; packaging or a draft prerelease cannot establish headset support.
The independent eye RT prototype and diagnostic probes stay default-off. No
automatic updater is planned.
