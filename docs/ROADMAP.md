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
immersive subtitle transport, focus/dashboard/tracking loss or device resets.
Already accepted Hope and ordinary reload/menu behavior need no repeat test.

Current unpublished work includes a shared version-2 preference owner and native
Options > Game cinematic/turning selectors, a physical right-hand duel posture
guard, reload thumb/continuity corrections, a neutral two-hand long-gun solver
and improved launcher resolution reporting. These are implemented/host-tested,
with native widget usability, duel gesture comfort, hand contact and sustained
recovery awaiting headset evidence. The two-hand solver is not attached to
native rifles. The published .4 download does not contain these later changes.

A chest/possible-scroll interaction crash remains unexplained; its preserved
stack does not identify the faulting instruction. A bounded read-only
render-fault reporter is host-tested for recurrence. Signed 32-bit native
identity recovery has scoped functional acceptance, but actual high-address
owners remain untested. Real OneDrive operation, other controllers/headsets,
long-gun interactions, mounted play and campaign-specific mechanics retain
independent gates.

Immediate work, in order:

1. **Remaining campaign continuity and ownership recovery.** Preserve accepted
   Hope/multi-cinematic recovery and hot switching. Broaden coverage only for
   unexercised campaign and ownership boundaries; accepted recovery needs no repeat.
   Preserve the chest/possible-scroll fault diagnostic and record a recurrence without
   inventing its cause. Batch focus/tracking/dashboard and held-control recovery;
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
loading or already accepted Square/menu/weapon behavior. Unavailable campaign
actions and horse contexts remain pending/inconclusive; Ray's exercised on-foot
kick does not override native permission restrictions elsewhere.

A pending visual/physical gate blocks promotion of that specific behavior only.
Continue independent offline work on controls, UI, recovery and other qualified
boundaries while waiting; accumulate relevant checks into one substantial
candidate rather than requesting repeated small physical sessions.

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
- **Native VR settings:** a shared versioned preference owner and two selectors
  in the existing Options > Game page are implemented, preserving original fonts,
  native OK/Cancel/Defaults and main/pause return routes. Cinematics and smooth/snap
  turning are the first preferences; version-1 cinematic values migrate explicitly.
  The [native bridge](research/COJ_UI_MOUSE_PATH.md#native-vr-settings-integration-boundary)
  consumes requests before side effects, rejects stale editors and reports save
  failure without closing the page. Host behavior, shipped-JVM bytecode and package
  inventory verification pass; native navigation, persistence and held-input
  recovery still need one canonical physical candidate. The existing panel stays
  usable during development.
- **Physical duel draw:** the production mouse route now requires a fresh lowered
  right aim tip and centered stick before draw, then a physically raised tip
  before upward raise input. Host tests cover the pre-aimed-hand shortcut and
  tracking/owner recovery without changing native draw timing or permissions.
  Gesture feel and broader native retry/early-draw behavior remain pending in
  the consolidated batch; this is not yet a measured holster interaction.
- **Two-hand long-gun support:** investigate a measured support-hand frame before
  proximity/grip attachment. A neutral rigid solver and fresh-grip ownership policy
  are implemented/host-tested, including off-axis support, authored roll, unequal
  spacing without scale and grip/trigger release barriers. Native rifle attachment
  remains disabled. The four inspected Winchester/Remington meshes lack
  named secondary holder sockets. Existing free-hand fingers do not establish a
  foregrip or firing/cycling ownership. Preserve one scoped weapon/hand/aim map
  and arbitrate the claimed grip against interaction before enabling attachment.
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
their own exact-game contracts after port qualification. Bow/scoped optics and
physical-walk animation remain planned integration beyond the accepted ordinary
Focus and native locomotion seams. Backend/game expansion remains Milestone 5.

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
posture, reload feedback/continuity and two-hand solver work described above
remain unpublished and do not inherit the .4 acceptance.

Future distributions must preserve exact-build rejection, reversible archive
installation, independent saved resolution/reload preferences, local-only
diagnostics, original-asset exclusion, GPL-3.0-or-later corresponding source and
third-party notices. Keep the Windows 10/11 x64 offline package with its x86
game/runtime dependencies and PowerShell 5.1 launcher. Game and SteamVR lifecycle
remain manual; packaging or a draft prerelease cannot establish headset support.
The independent eye RT prototype and diagnostic probes stay default-off. No
automatic updater is planned.
