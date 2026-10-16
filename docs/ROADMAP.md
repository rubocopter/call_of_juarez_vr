# Roadmap

The active target is original Steam Call of Juarez (2006), Windows x86, native
D3D9 stereo through OpenVR/SteamVR. PS VR2 Sense is the development reference.
[Validation](VALIDATION.md) owns acceptance and unresolved physical gates;
[Architecture](ARCHITECTURE.md) owns integration contracts.

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`,
`headset-validated`, `supported`. A working build does not imply visor support.

## Current gate

Native stereo/head tracking, exercised locomotion, independent hands/weapons,
menus, available Sense controls and encountered HUD/subtitles have scoped
operator acceptance. The opt-in persistent manual reload now also has scoped
headset acceptance for opening/closing, tracked hands, successive native unit
loading, insertion-only haptics and exercised weapon/menu recovery.
Recovered mission countdown, encountered notices and corrected green/red attack
directions now have scoped headset acceptance. Ray's Bible use and on-foot kick
are accepted in exercised gameplay; concentration and special mechanics remain
separate gates.

A reported chest/possible-scroll interaction crash is a current campaign stability
issue. Evidence is preserved but the symbol-only native stack lacks the actual
instruction; do not claim a root cause from its export names. A bound, non-handling
render-fault reporter is implemented/host-tested for the next ordinary candidate.
Scripted in-game movies now yield stereo/body ownership to native flat theater,
including the module subtitle pass, even with a running timer. Movie detection,
end/loss and exception cleanup are host-tested; comfort and subtitle/held-input
entry/exit acceptance join the combined session. These corrections take precedence
when exercising the affected campaign scenes.

The current knuckle-contact combat route now has scoped headset validation: the
operator reports defeating three enemies with Hands and says it felt substantially
better. Earlier intermittent-hit and rejected completed-curve reports remain
useful history; the current route resolves that gate for the exercised fight.
Broader hand parity, campaign contexts and recovery remain open. The VR settings
panel and exercised boxing death/recovery are also accepted. Immersive movies
previously showed intermittent wrong-image flashes. The latest operator trial
accepts toggling and entering cinematics without reported problems or persistent
flat presentation in the exercised scenes; Hope-specific recovery remains separate.
That same combined candidate exposed signed-positive native-identity rejection
with high-address objects, blocking single-pistol manual reload and physical Hands.
Opaque 32-bit identity preservation is implemented/host-tested. The operator
confirms that the corrected candidate restores independent Hands, manual reload
and physical melee in the exercised session. That functional baseline has scoped
headset acceptance; its successful capture does not establish a high-bit weapon
owner, so actual high-address runtime coverage remains a separate gate. No new
test is needed solely to reconfirm those restored functions.
The functional settings panel remains a provisional development tool; future
native settings should share its preference policy.

Community testing reports stereo remaining flat after the arrival-in-Hope
cinematic. An unfinished capture could permanently consume a ring slot when
ownership cancelled submission; repeated transitions exhausted the ring.
Reclaiming only the current unfinished unpublished slot is implemented and
host-tested on classic D3D9 and D3D9Ex/D3D11, preserving pending/leased frames.
Verify recovery through the affected campaign transition before treating that
report as resolved. This campaign stability gate takes priority over visual
reload refinement; it does not establish a Quest-specific cause or acceptance.

Immediate work, in order:

1. **Campaign continuity and ownership recovery.** Check the already implemented
   movie-continuity correction in the next relevant campaign candidate, including
   return to stereo after the Hope cinematic when available. Preserve the
   chest/possible-scroll fault diagnostic and record a recurrence without
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

## Independent-resolution and native-settings preparation

These are planned engineering tracks that can advance offline without displacing
campaign continuity, physical interactions or reload work:

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
  No independent-resolution writer is admitted and the published
  prerelease remains unchanged.
- **Native VR settings:** retain the existing single-panel preference path now.
  Before another editor, extract its coordination into one shared preference
  owner, preserving version-1 values. Qualify the
  [native command/resource/input bridge](research/COJ_UI_MOUSE_PATH.md#native-vr-settings-integration-boundary)
  for both Options variants before a native page; no new overlay or fictitious
  settings are required. The existing panel stays usable during development.

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

Achieved in exercised contexts: per-eye rendering, tracked head, recenter and
room-scale motion. Finish broad loading/reset/transition coverage, sustained
pacing and recording compatibility before public support claims.

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

`v0.1.0-prealpha.1` is published as an experimental community-testing prerelease.
Public testing may begin before full campaign coverage; it does not promote any
validation state or promise campaign completion, recovery, comfort or support.
The chest/possible-scroll crash remains unresolved. Local cinematic switching
and recovery now have scoped acceptance; the published package and unexercised
campaign transitions retain their own limits. Restored hand/reload/melee behavior
has scoped operator acceptance; actual high-address coverage remains separate.
Publish those limitations and recommend save backups alongside the package.

The distribution target remains the exact original Steam 2006 D3D9/OpenVR build.
Only PS VR2 Sense has scoped physical validation. Additional packaged controller
bindings are experimental; other PCs/headsets require independent evidence.
Preserve exact-build rejection, reversible installation and local-only diagnostics
that users inspect before manually sharing. Manual reload is optional and its
installation preference persists across explicit ZIP updates; no auto updater
is planned for this prerelease.

Prepare an offline ZIP for Windows 10/11 x64 hosts and the x86 game, with a
double-click Windows PowerShell 5.1 menu, bundled Python and VC++ x86 runtime,
package inventory, dependency licenses/source notices, install/update, uninstall,
status and diagnostics. Users supply the game, SteamVR and GPU/headset drivers
and start/close the game and SteamVR manually. Packaging and draft-prerelease
automation remain separate from publication and physical acceptance. No
project-code license has been selected; third-party notices remain separate.
Detailed acceptance limits remain in
[Validation](VALIDATION.md#public-prerelease-policy).
