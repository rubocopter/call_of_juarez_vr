# Validation

Validation states are distinct:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

Builds, CTest, synthetic fixtures and static inspection do not promote physical
gates. Acceptance here is limited to exercised original Steam Call of Juarez
(2006), PS VR2 Sense and SteamVR contexts. It is not whole-campaign or public
release support. This document owns acceptance; [Architecture](ARCHITECTURE.md)
owns contracts and [Roadmap](ROADMAP.md) owns priorities.

## Public prerelease policy

`v0.1.0-prealpha.5` is cumulative from .4/.3/.2/.1, adding physical bow drawing,
bimanual rifle support, virtual holsters/guides, native VR options, physical duel
posture checks and continuous body following. Scoped PS VR2 operator acceptance
covers the corrected Remington grasp, ordinary holsters/guides, native option
usability, tested duel posture admission/rejection and continuous body following,
with very good exercised bow feedback. Winchester/SawOff support and broader
timing/interruption recovery remain separate gates. Earlier accepted functions,
including normal-rifle free-hand presentation and concentration, are retained.
Live OneDrive behavior remains unqualified. Long-gun manual reload is unimplemented;
Square retains conventional reload and the original animation. The archived
packages and canonical verdicts remain unchanged. No release claims
whole-campaign acceptance. Distribution, package verification and additional
controller bindings do not promote any validation state. Physical acceptance
remains limited to the exercised Steam 2006 D3D9/OpenVR scenes and PS VR2 Sense.
Other packaged bindings are experimental, with no support claim for other PCs
or headsets.

Disclose the unresolved chest/possible-scroll crash, incomplete campaign coverage
and open recovery gates. The operator explicitly confirms that stereo recovery
after the Hope cinematic is fixed and headset-validated on the exercised PS VR2
setup. Declared physical evidence also covers multiple exercised cinematics,
entry/exit, hot theater/immersive switches even mid-sequence, different weapons,
headset recenter and continued gameplay. This closes the exercised Hope recovery
gate; it does not qualify every campaign cinematic, unreported subtitle modes/timing,
extreme device/tracking loss or other headsets. No repeat physical test is requested.
Recommend save backups because crashes or blocked progression can lose progress.
Users start/close the game and SteamVR manually. Community diagnostics are
generated locally, inspected for private data and attached manually to issues;
they do not replace canonical evidence or operator acceptance below.

The .3 OpenVR projection correction has host regression coverage and scoped
community physical confirmation of a corrected Bigscreen Beyond gameplay view.
The published prealpha.2 converter reverses the vertical projection center when
raw vertical tangents are asymmetric. Regression tests now compare the complete
converted matrix and forward menu placement with Valve's independent projection
formula, covering asymmetric vertical optics in both directions and symmetric
PS VR2 controls. The corrected conversion preserves the symmetric controls.
An additional Quest report describes a good view and smooth play, without a
specified model. Menu gaze alignment, controller behavior and broader hardware
remain unqualified; these reports do not establish complete headset support.
See the owning
[projection contract](research/COJ_CAMERA_PATH.md#openvr-projection-conversion).

The .3 product policy uses Auto only when no resolution preference is saved;
every explicit choice, including Manual and Preserve, survives updates. Auto
uses the SteamVR helper recommendation, not panel-native pixels. Start SteamVR
manually before Install/update or PrepareSession for Auto/percentage profiles.
Unavailable queries or incompatible caps preserve the selected game size with a
visible explanation, retain intent and retry next preparation. Background/Scene
identity, memory headroom and broader headset qualification remain open.

Current Install/update enables ordinary stereo, independent hands, native torso/legs
and physical Hands combat. Manual reload defaults on for a fresh install;
upgrades retain its installed choice. Launcher option 8 switches manual/conventional
reload through transactional installation; explicit `-ManualReload:$false` remains
available. This supersedes .3's forced-on upgrade policy.
PrepareSession retains installed features without archive repatching and creates
fresh provenance for later plays. These product defaults add no physical acceptance;
theater remains default and the rejected independent eye RT/probes remain off.

Community reports remain observations, not support qualification. Chtiblue
reports startup with Pimax Dream Air, RTX 5090 and SteamVR; controller coverage,
tracking quality, sustained performance and campaign recovery are not established
by that report. The known source-resolution limitation also affects the PS VR2
development path: native stereo retains the game's RT0 extent, independently of
SteamVR's recommendation. Independent eye resolution remains a default-off,
rejected experimental prototype; the
[source investigation](research/COJ_VR_RENDER_RESOLUTION.md) adds no physical
acceptance and makes no claim that DSR works on a particular setup.

[Community issue #5](https://github.com/rubocopter/call_of_juarez_vr/issues/5)
reports rhythmic movement stutters with Pico 4/Virtual Desktop at 72 Hz and an
RX 7600. The available report tail shows shared GPU transport and disabled CPU
hashing during native stereo; its CPU theater route is separate and does not
establish failed gameplay interop. Native capture drops and occasional long
submissions are observed, without enough timing evidence to identify the
stutter's cause. The active source now gates the previously unconditional HUD
sprite/flush diagnostic by the resource-probe opt-in, with host coverage for
disabled/enabled/revoked callback delivery and native forwarding/restoration.
This corrects unwanted diagnostic work in ordinary sessions; it does not prove
the reported rhythmic stutter resolved. Reporter testing, sustained performance
and Pico controller/headset qualification remain open. The community tail is
not a complete canonical run and cannot promote support or performance gates.

Periodic FX-global/emitter walks and duplicate shot/barrel comparisons also
require the resource-probe opt-in; skeleton element discovery requires movement
tracing. These retired integration observations add no acceptance and are not
ordinary gameplay dependencies. Exact-build rejection, muzzle/hand safety,
ownership/restoration, fault reporting and transport counters remain active.

The [approved independent-resolution design](research/COJ_VR_INDEPENDENT_RESOLUTION_DESIGN.md)
has a startup-only binding observer and offline analyzer implemented and
host-tested. Real D3D9/Ex tests cover read-only state/pixels, release before Reset,
ResetEx invalidation, hook forwarding/continuity, scope cancellation and control
preservation. Synthetic analysis rejects incomplete/foreign traces. A finished
canonical session now exercises bounded gameplay eye collection without read
failures or truncation. The operator confirms the exercised functional checklist
still works at the original resolution, with occasional stutters. This is scoped
continuity acceptance with diagnostics active, not a performance or higher-detail
gate. A later bounded collection includes complete gameplay, settings and
immersive-movie resource scopes at the original size.
The offline analyzer classifies the observed FourCC `NULL` color placeholder
against its bound D16 depth extent, with explicit layout/Z and viewport checks.
It now reads the captured complete gameplay pairs; ordinary out-of-bounds color
and unqualified NULL/depth layouts still reject. This is bounded binding evidence,
not a qualified effect graph or verified shadow contents. Allocation/effect/constant
ownership and compositor retirement remain Q0 gates. Diagnostic sessions perturb
pacing and cannot establish performance improvement. Independent rendering,
actual detail and functional continuity at new sizes require the specified
native and headset evidence; complete bindings never admit resizing by themselves.
The [native PP ownership map](research/COJ_PPFX_TARGET_OWNERSHIP.md) now establishes
static allocator/release/alias and renderer-cache responsibilities. This is
research evidence only; it adds no live allocation, independent-size, shader
constant or recovery acceptance, and leaves Q0 open.
Sprite/flush analysis preserves distinct device-generation and probe-epoch
namespaces. Older gameplay samples cannot qualify their device-generation
identity retrospectively; fresh correlation uses both explicit counters.
Ordering metadata alone does not admit mirror mutation, alpha/coverage or
independent resolution.
The bounded native PP descriptor/cache reader, exact-build admission bridge and
separate offline native identity correlation are implemented and host-tested.
Synthetic fixtures cover LAA identities, aliases, class/list/count/name bounds,
root changes, failed reads, unallocated/dirty resources and the split sampler
caches. Serialization preserves byte names and unused 2D reciprocal-depth bits;
it does not call copied COM identities. Sampling now reserves two attempts per
gameplay/settings/movie route while retaining the six-pair total. Runtime
collection now supplies both reserved pairs for each route, complete native
begin/end snapshots and exact-value correlation for all 14 `scene30.ppfx`
descriptors, plus native default/internal color/depth and placeholder identities.
Five loaded effect classes/names match the bounded map. This is live-tested
structural correlation, not an allocation/lifetime proof; unmatched resources
remain explicit. One settings frame also supplies naturally scheduled sprite/flush
ordering against the original-size default color target with matching device
generation. Other sampled frames lack that ordering evidence. Pixel coverage,
programmable constants/history, private-family rollback and submitted OpenVR
output retirement remain unqualified; no resizing writer is enabled by default.
The local startup-only private-bank implementation now exists behind
`independentEyeResolution` (`off`, `2560x1440`, `2560x1920`). It remains
experimental: real D3D9Ex with synthetic native memory verifies new raster rows,
depth/viewport/scissor, separate histories, CPU/GPU rollback, mirror bars and
Reset retirement. Checked-write and graphics-restoration faults retain recovery
state; fixed-output allocation/authority and native-queue policies have host
coverage. This supersedes the earlier implementation-pending statement, without
promoting Q0/B0/B1 game or headset acceptance. The original-size collected runs
did not exercise this writer. The .2 consolidation does not accept or enable it.

The first private-bank B0 run is **rejected**. Its native queue admission failed
before the first eye mutation/capture; the run recorded zero fenced stereo pairs.
The session fault latch then blocked the combined stereo/gameplay sample,
leaving flat presentation without Sense gameplay input. The operator also reports
pause UI visible only on the monitor. Successful startup theater and global
recenter/menu buttons do not qualify gameplay fallback. The loading-continuation
change has positive operator feedback, separately from this rendering failure.
Keep independent resolution disabled and retain the ordinary VR path. Do not
advance to B1 or treat the synthetic native-memory GPU test as game acceptance.
The precise queue-install/read rejection still requires diagnosis; no guard may
be bypassed merely to admit the writer. Recovery must preserve usable controls
and current menus before another private-bank candidate is offered.

The ordinary native path at 2804x2860 is the accepted local PS VR2 reference
and selected Manual default. It has measured native raster/capture/output extents
and repeated positive operator feedback; no repeat physical test is requested.
The menu GPU correction and exercised Ray fit are retained. See the consolidated
[image-quality acceptance](#image-quality-profile-acceptance) for scope, remaining
limits and startup-profile validation. The independent bank stays off.

For the first private-bank run, verify menus/loading, gameplay, manual reload,
hands/melee, settings, immersive/theater cinematics and normal quit. B0 is the
same-size isolation gate; B1 adds 33.3% vertical samples at the same optics.
Accept B1 only with fresh per-eye native geometry/capture/pose provenance,
visible new vertical detail, unchanged angular geometry and no regressions.
Compare the same save/view, SteamVR scale/refresh and recording state against A:
record CPU/GPU frame times and pacing from SteamVR frame timing, process private
bytes/VA, capture/upload/frame-age tails and fallback/reset diagnostics. Host
GPU patterns cannot substitute for those measurements. Reject a run with stale
eye/theater sources, unmatched owner/generation, failed restoration, continuing
allocation growth, unacceptable pacing or missing effective-size evidence.
The canonical full-profile verifier retained rejection for an unobserved Sense
loading-continuation press. Complete collection, normal restoration and operator
acceptance do not overwrite that verdict or establish the missing input gesture.
The operator subsequently reports that only Circle and Options continue the
encountered loading screens; this does not establish Cross/trigger continuation.
The exact loading handler schedules its continuation only on key-down, while the
previous Cross/trigger adapter delivered key-up. Both adapter paths now send one
key-down, with JNI admission, single-delivery and exception cleanup host coverage;
physical continuation remains pending in the next combined candidate. Delivery
telemetry must not be equated with timer resume or mission continuation.
The general verifier also required the obsolete Circle gameplay-pause mapping.
It now requires Options with the same successful native-stereo Escape route;
host fixtures reject Circle-only, flat-only, failed, foreign-route and missing
evidence. The already archived rejection is preserved.

## Current feature state

| Boundary | Confirmed state | Remaining limits |
| --- | --- | --- |
| Native D3D9 stereo and head tracking | Headset-validated native per-eye rendering, 6DOF, recenter and room-scale movement in exercised scenes. Repeated Manual 2804x2860 clarity/stereo and no perceptible performance problems are operator-accepted on PS VR2, with prior actual raster/capture/output evidence. | Broad loading/device transitions, quantified sustained pacing and other headsets. |
| Cinematic stereo recovery | Headset-validated by declared operator physical evidence: recovery after Hope, multiple exercised cinematics, entry/exit, hot theater/immersive switching including mid-sequence, different weapons, headset recenter and continued gameplay. The operator reconfirms these recovery checks work and are complete in exercised play. | Secret-scroll interaction was not tested and retains its own campaign gate. Subtitle timing, broader comfort and extreme device/tracking loss remain separate limits. The new immersive skip-input defect does not reopen stereo recovery. No repeat of accepted recovery is pending. |
| Locomotion | Exercised native analog movement, jump parity, physical crouch and earlier snap turning accepted. The operator confirms careful-movement corrections, default smooth turn and continuous HMD body follow without the former cone in the exercised PS VR2 setup. Native option usability is accepted where exercised. | The drift report concerns the older published package; the current continuous-follow correction is accepted in scope. Other headsets/controllers, prolonged comfort and mounted/campaign contexts remain open. Physical-walk/turn leg animation is planned. Specific mode/held-input recovery is not separately qualified by selector usability. |
| Body and hands | Independent tracked native hands/weapons with original torso/legs accepted in exercised contexts; head and connected arms hidden. Corrected free-left-hand fingers with a normally held long gun have scoped operator acceptance. | Firing/alternate/reload retain native fingers. Broader finger, weapon and recovery coverage remains open. Connected-arm IK is diagnostic. |
| Weapons and whip | Right-hand pistol firing, muzzle/impact effects, water and destructible bottles accepted. Exercised whip use and approximate reticle alignment accepted. The combined candidate's dual reticles and Clyde draw/raise with tracked aim/fire have operator confirmation. | Broader weapons/duels, exact ballistic agreement, whip reach/climbing and special optics. Long-gun manual reload is unimplemented; native automatic reload/animation remain. |
| Indexed reload groundwork (post-.5 source) | Implemented/host-tested for exact Remington/SawOff/Derringer/Lemant two-index observation, native object/owner/hand stability, typed reserves counted once, JNI failure/null/cleanup and LAA-safe identities. A separate pure classifier checks a retained synchronous pair; sparse opt-in samples cannot qualify a transfer. | Native indexed phase/HUD correlation, selective shell transaction, mechanism/port and physical gesture integration remain pending. No new weapon enters manual reload; no ammo/animation/sound writer is added. |
| Physical bow | Exact owner/root ray, physical right-hand R2 grab/draw/release, native supervision, controller haptics and capture-drop separation are host-tested. Operator confirms the bow worked very well in the exercised session, extending the earlier improvement to scoped functional acceptance. | Mesh/string/arrow/reticle agreement, haptic quality, concentration optics and interruption recovery are not separately accepted. This does not qualify all scenes or sustained campaign support. |
| Physical duel posture | Correct hand-at-hip draw/raise and rejection of exercised incorrect/cheating positions have operator acceptance: actions were permitted only in the correct position. | Native early-draw timing, retry/recenter and ownership recovery remain separate where not exercised. Measured holster contact is not implemented. |
| Basic virtual holsters | Operator accepts exercised equip/stow gestures and the colored assistance after the zone-press consumption correction. Enabling/disabling the assistance works correctly in the exercised native options flow. | Earlier occasional kicks remain rejected history; broader conflicting/held-input and interruption recovery are unqualified. No attached weapon mesh or measured native socket contact; disabled during duels and mounted play. |
| Ammunition pickup assistance (post-.5 source) | Implemented/host-tested under the same default-off weapon-guide preference. Uses the real manual gesture source/threshold, fresh release, supervised native waiting and positive reserve; distinct cartridge icon replaces holster rings during the manual session. Real D3D11 host fixtures cover distinct binocular pixels, current-owner admission, disabled preference, invalid geometry and resource reset. | No live/headset acceptance of marker placement, readability or color semantics. Adds no physical pouch, ammunition permission, native transfer or manual rifle loading. Include it with the existing revolver contact/recovery check, without a dedicated run. |
| Binding-aware hints and reload coaching (post-.5 source) | Implemented/host-tested for bounded Unicode, semantic fallback, origin/cache invalidation, copied HUD translation, coaching and queued-text admission. Headless D3D11 tests also retire repeated-frame text after rebinding/owner loss/disabled guides/expiry, preserving subtitles and overlapping HUD. They cover reverse restoration, device/context/target mismatch, fresh-frame discard and BGRA/BGRX/RGBA UNORM/SRGB targets. Optional instructions follow actual support-hand/session state and yield to native hint/interaction text. | No live origin/rebinding, non-Sense presentation, resource-pressure, readability or contextual coaching acceptance. No arbitrary PC rebind/chord inference or graphical glyph replacement. Include one naturally encountered hint and pickup/carry/release sequence with assistance on/off in the existing combined session; preserve subtitles, current owner, expiry and release barriers. No extra run is required. |
| Two-hand rifles | **Remington bimanual grip is headset-validated in the exercised PS VR2 context:** the operator confirms it now works after the mesh-contact correction. This closes the reported remote-contact/acquisition defect for that model. Exact Winchester/Remington/SawOff transactions also have host coverage for paired hands, one weapon commit, muzzle/ray verification, replacement rejection, restoration and input/hemisphere guards. | Winchester and SawOff placement/grasp remain physically unqualified. Detailed finger fit, sustained fire/cycle stability and adversarial interruption recovery are not separately described by this confirmation. Scope variants and physical manual long-gun reload remain unimplemented; Remington still uses native Square reload. |
| Latest rifle/reload acceptance | The operator confirms the corrected Remington bimanual grip works and explicitly leaves reload pending. Earlier scoped free-L2 suppression acceptance remains. | Pending reload means physical manual loading, not a reported failure of the existing native automatic path. No manual reload acceptance is added. Earlier unsupported Derringer/no admitted pistol-session evidence does not identify every pistol tried; whole-clip, two-slot and dual-equipped exclusions and earlier admitted-revolver acceptance remain unchanged. |
| Physical melee | The current knuckle-contact route is headset-validated in an exercised fight: the operator reports defeating three enemies with Hands, and says the combat felt much more responsive. | Scoped to that fight and configuration; left/right parity, broader enemy/campaign contexts, and tracking/menu/recenter recovery remain unqualified. |
| Menus and Sense controls | Automatic hand pointer, same-hand trigger selection, Cross/Circle, pause, confirmation dialogs and mouse coexistence accepted. Wheel selection/haptics, on-foot crouch, ordinary Focus and Create tap/hold accepted where exercised. Ray's Bible selection/use and on-foot kick have scoped headset acceptance. | Dashboard/loading/held-input recovery, mounted routing, campaign permission changes and unavailable mechanics. |
| Native VR options | Operator reports native options work well, including enabling/disabling the holster assistance in exercised play. Shared preferences and cinematic/turning selectors retain scoped physical usability acceptance; version-3 migration has host coverage. | Unreported restart persistence, OK/Cancel/Defaults variants and held-input/recovery paths remain open. Ordinary selector operation, including the exercised holster assistance toggle, is not pending again. |
| Secret-scroll chests | Conditional preservation of the same still-active native render level is implemented/host-tested for the reported null-level crash. | No operator scroll pickup in the latest sessions. Next naturally encountered scroll must establish pickup, UI closure and continued control/stereo; the original callback cause is not fully observed. |
| Mounted play and mod-added DLC | Horse mounting/dismounting worked in the exercised DLC context. | Mounted movement/gallop, camera/aim, horse HUD and general DLC compatibility remain unqualified. DLC adaptation is deferred by operator priority. |
| HUD and subtitles | Encountered native health/ammo text, compass, equipment wheel, essential text and subtitles accepted. The operator additionally confirms subtitles appeared in exercised animations; rendering mode was unspecified. Independent mission countdown, encountered objective/journal notices and corrected green direction/red damage marks have scoped acceptance. | New Western wheel styling, wrist health styling, concentration glyphs and horse meters are host-tested only. Unreported movie modes, subtitle timing/hide recovery, special HUD contexts and complete graphical HUD capture remain open. |
| Interaction | Pickup/carry/put-down and drawers accepted through the previous HMD reference. | Latest left-Sense L1 ray and held-only cyan reference are host-tested, awaiting physical acceptance. The permanent HMD ring was rejected for comfort. |
| Persistent manual reload | Fresh-install default with launcher opt-out and preserved upgrade choice; scoped PS VR2 headset-validated: Square opens/closes without auto-loading, hands stay tracked, successive free-trigger cartridges load, insertion-only haptics work, weapon/menu recovery works where exercised. Native unit transfers/restoration have correlated live evidence. | Beyond community feedback reports difficult insertion and no perceived haptics; other controller feedback remains unqualified. Insertion sound, precise geometry, interruption recovery and prolonged comfort remain open. |
| Cartridge visual and pinch | Textured CC0 .357 visibility has scoped headset acceptance. Revised .7/.7 pinch and skin-derived anchor have host tests and bounded live delivery evidence. The later displayed-finger tangent is host-tested only. | The reviewed close-up predates that tangent and still appears superimposed rather than convincingly held. Contact, axis, scale, occlusion and comfort are unaccepted; scene depth and native lighting are unavailable. |
| Native loading port | Operator confirms the port looks correct in exercised reload; its visual presentation has scoped acceptance. Read-only gate/drum motion and six-mouth pivot rankings have bounded live evidence; generic tip/socket policy has synthetic host tests. | No qualified CoJ socket is connected to that policy. Active loading uses the 14 cm grip zone. Phase-bound mouth geometry, precise insertion, independent mechanics and ejection remain planned. |
| Death and save/load | Corrected failure-screen visibility and latest exercised boxing death/recovery have scoped operator acceptance. Exercised narrated loading after reversible LAA correction and save/load with the thumbnail workaround accepted. | Broader retry/back/held-input recovery, manual-session save roundtrip and allocation pressure remain open. |
| Transport and shutdown | Primary D3D9Ex GPU sharing and normal shutdown have bounded headset/live acceptance. Independent consumer-query recovery, retained quarantine on failed recovery proof, ResetEx/replacement admission and cleanup hardening are host-tested only. | Sustained pacing, abnormal exit, arbitrary reset/device loss and Steam recording-associated microskips. Automatic continuation after a native failure is not live/headset-validated. |
| Ownership recovery | Global input release barriers/failed-poll rollback, partial stereo invalidation, pointer resize cancellation and latest manual-session recovery have host tests. Camera-owned pelvis/diagnostic-arm restores retain the original actor through failures; consumed bow R2 survives repeated owner-proof loss and input-source replacement. | Physical focus/dashboard/tracking/held-control and eligible resize recovery remain open; earlier menu/reload acceptance does not qualify these changes. Host fault injection does not qualify a live actor-replacement failure. |
| Persisted validation verdicts | Host-tested reporting and rejection policy; live-tested canonical rejection remains inventoried through collection and original restoration. | A failed telemetry profile remains failed alongside scoped operator acceptance. No automatic headset acceptance; successful full-profile workflow coverage remains separate. |
| Protected-target warning | Red aiming-cross alternative implemented and host-tested. | Visor visibility/target transitions; original warning was absent in visor. |
| Other platforms | OpenXR experimental; D3D10 and additional games planned. | No physical support claim outside the active D3D9/OpenVR target. |
| Optional body feedback | Neutral QA-event admission and recording/fault-sink policy host-tested for freshness, duplicates, epoch changes, silent recovery and shutdown. | bHaptics provider, game event producers, software output and physical vest feedback remain deferred/unqualified at the operator's request. Controller haptics acceptance does not transfer to a vest. |

## Pending headset checks

Use one fresh canonical `prepare`/`finish` candidate when a useful set of eligible
scenes is available. Skip unavailable scenes as unobserved; no dedicated session
is requested for each row. Start and close the game/SteamVR manually. This list
tracks acceptance, while [Development backlog](ROADMAP.md#development-backlog)
tracks features that still need implementation.

| When naturally available | Remaining check and acceptance evidence |
| --- | --- |
| Bow | Left hand holds/aims; approach the string with the right hand, freshly hold R2, pull back and release R2. L2 is not required. Check several deliberate releases yield one arrow each, short/slack releases do not shoot and a held draw does not repeat or cut out at concentration expiry. Judge arrow/mesh/reticle agreement at different pitches, vibration and Focus/concentration comfort separately. |
| Bow interruption or a general recovery opportunity | Once, interrupt with wheel/pause or available tracking/focus/dashboard loss; release controls before resuming. Require no stale shot, click, round transfer or haptic replay and coherent resumed stereo. Recenter and save/load/death recovery remain distinct where naturally encountered. |
| A secret-scroll chest | Pick up the scroll, close its UI and continue moving/aiming. Require no crash and continued control/stereo. Chests were not exercised in the latest operator feedback. |
| An eligible single revolver with empty support hand | Check the changed cartridge tangent/contact, thumb gesture, zone-entry/insertion cues and tracking-jump rejection together. Keep the earlier native unit-loading acceptance; judge only new presentation/recovery behavior. Frontier/Walker/Cogswell and mirrored use remain eligible extensions when found. |
| Another duel | Only unexercised early timing, retry/recenter and ownership recovery remain. Correct posture and rejection of the tested incorrect/cheating positions are accepted and need no repeat merely for confirmation. |
| Weapons available during ordinary on-foot play | Fresh R1 at the right hip toggles its pistol; L1 at the left hip toggles its pistol; R1 above/behind the right shoulder toggles the rifle; L1 above/behind the left shoulder toggles the bow. Release between attempts, including to stow a drawn weapon. Enable/disable holster guides in native VR comfort options and judge zone reach, alignment and visual comfort. Check no repeats while held, no Action/Kick on leaving zones, no unavailable-slot selection and normal L1/R1 elsewhere. Judge the updated wheel's readability during normal selection. Use the grouped interruption check; attached meshes are not required. |
| Options or a loading transition | Specific unexercised save/reopen, Cancel/Defaults and held-control barriers; one fresh Cross/trigger loading continuation when encountered. Do not repeat ordinary selector/menu usability as a new gate. |
| Interaction/HUD, a movie or horse | Left-Sense L1 reference independent of gaze; revised wrist/concentration indicators. For unreported movie modes, broaden subtitle timing/clearance/settings recovery when naturally available; appearance in exercised animations is accepted. Mounted movement/gallop, camera/aim and paired horse meters remain separate from accepted mount/dismount. Broader DLC testing is deferred. |
| An untested Winchester or SawOff during ordinary play | Remington's exercised corrected grasp is accepted; do not repeat it solely for confirmation. For an available untested model, freshly hold L1 near the mesh fore-end, aim/roll and release. Check hand/mesh/reticle agreement. Sustained fire/cycle, crossed-hand cancellation, Square cancellation and pause/tracking/weapon recovery remain separate, naturally grouped checks where not reported. Native automatic reload remains; no physical loading is expected. Scoped Winchester is not admitted; skip absent weapons rather than adding sessions. |

Already accepted in scope: continuous body following, ordinary native options
and menus, subtitles appearing in exercised animations, the correct duel gesture, prior native unit reload and cartridge
visibility, physical Hands combat, exercised cinematic recovery/hot switching,
and the established PS VR2 resolution. The bow's latest improvement and horse
mount/dismount are recorded with their narrower limits above. None of these
needs a repeat merely because a newer candidate is prepared. Canonical telemetry
requirements and their recorded pass/fail/inconclusive verdicts remain separate
from operator acceptance; a skipped gesture does not satisfy a verifier gate.

### Focused rifle and reload candidate

Use this subset when preparing a candidate after support-grip/contact/reload
observer changes. The longer pending table remains the backlog, not a mandatory
list for one run. No new headset acceptance is recorded until the operator
reports these observations. Canonical full-profile telemetry is verified
separately; a focused or skipped action never changes its verdict.

Budget roughly 15-20 minutes in ordinary on-foot play with one unscoped
Winchester and one admitted revolver with empty support, if available. Missing
weapons remain unobserved; do not search the campaign or start extra sessions.
Keep the accepted local resolution and native D3D9/OpenVR path, the independent
eye bank off, and recording off. Enable reload tracing for the observation
segment; diagnostic overhead excludes performance qualification. Start/close
SteamVR and the game manually, with one fresh canonical prepare/finish identity.

| Block | Actions and evidence to retain |
| --- | --- |
| 1. Rifle support, about 4 minutes | Fresh L1 near the natural left support grip; aim up/down, roll and fire 2-3 deliberate shots. Release/regrip once. Cross support behind the dominant grip once: require cancellation, no backwards flip, stretch, extra shot, Action or holster leak. Reacquire only after release. Note hand/mesh/reticle agreement and any jump. |
| 2. Winchester native reload, about 3 minutes | With support held, press Square from a partially loaded weapon. Require support cancellation and original automatic reload; observe free-hand movement, native animation and visible ammo/reserve through its completion. Regrip after release. Repeat once with a menu/weapon interruption if safe. Correlate `winchester_native_reload` with these observations; raw states and `GetAmmoCount(0)` do not establish a valid phase or effective clip count. No physical rifle insertion is expected. |
| 3. Revolver contact and recovery, about 5 minutes | Fresh Square with a single admitted revolver and empty support; pick up a round, inspect pinch and wrist roll, carry into the zone, release for one round and inspect the brief thumb motion. Make 2-3 insertions and one release away from the gun. Once, pause or change weapon during a carry and return with controls held: require no stale round, queued insertion or repeated cue; release and deliberately reopen/recover. Judge axis/contact separately from the known missing hand/scene occlusion. |
| 4. Shared recovery and finish, about 3 minutes | Use the interruptions already in blocks 2/3 to check coherent resumed hands, controls and stereo; no extra recovery loop is required. Note any unavailable scene, naturally encountered loading continuation or new crash. Close normally and run canonical finish before analyzing the immutable evidence. |

Only if immediately available in the same scene: one untested SawOff grasp/fire/
native reload, or the opposite-hand revolver assignment. Remington's accepted
grasp needs no repeat; unreported fire/recovery checks can join natural play.
Bow, duels, scroll chest,
horse, DLC, additional renderers, resolution experiments and exhaustive control
coverage are not objectives of this focused candidate. Their separate pending
states remain unchanged. Already accepted ordinary menus, unit loading, body
following and resolution need no repeat solely to reconfirm them.

Record each block as observed success, observed failure or unobserved, with the
weapon/hand, action and visible symptom. Telemetry, host tests and absence of a
crash cannot substitute for an operator observation. Individual run IDs, source
range, deployment hashes, logs and the Spanish run checklist belong under
ignored `work/`, linked to that candidate's manifests.

The subsequent operator feedback accepts absence of Remington free-L2 fire,
ordinary holsters, their colored assistance and assistance on/off. Do not repeat
those accepted checks solely for confirmation. The later video/operator report
identifies a floating contact away from the Remington: grasping that point does
acquire and move it, corroborated by native acquisition/application events. This
accepts that limited functional observation while rejecting physical placement.
The old generic player left_hand translation is replaced by an original-mesh
fore-end point transformed by the same weapon's live named element. The operator
subsequently confirms that the Remington now works and requests validation of
its bimanual grip, leaving reload pending. The exercised corrected Remington
grasp is therefore **headset-validated**; no repeat placement/acquisition trial
is requested. This does not independently qualify every marker color, detailed
finger fit, sustained firing/cycling or adversarial recovery gesture. Winchester
and SawOff remain separate physical gates. Physical manual Remington reload is
still unimplemented; Square retains original automatic loading.

An outside held L1 must remain unqueued; owner/tracking loss must retain release
barriers. Broader interruption checks can join natural play. No wider radius is
introduced. The bounded trace retains contact element/source, ordinary contact,
context, spatial and preceding release data. Revolvers, Winchester observation,
bow, holsters and duel posture need no separate trial for this placement fix.

## Host verification boundary

An earlier combined candidate exposed a high-address identity rejection in
otherwise enabled manual reload and physical Hands. The operator reports native
reload even with one pistol and empty support, and full native body with Hands.
Runtime observation confirms invalid weapon identity in an otherwise coherent
single-pistol context and selected boxing with no admitted owner. Signed-positive
`GetThisID` predicates rejected LAA high-bit owners; preserving opaque 32-bit
identities is now host-tested across reload admission/recovery/readback/tickets
and native/JNI boxing identity, contact and diagnostics. Zero/all-ones and changed
owners still reject. The operator confirms that the corrected candidate restores
independent Hands presentation, manual reload and physical melee in the exercised
session. Inventoried evidence correlates manual insertions with native unit
readbacks and records scoped hand presentation and admitted physical boxing.
This is scoped headset acceptance of the restored functional baseline. The
collected samples do not establish a high-bit weapon owner in that successful
session, so actual high-address runtime coverage remains separate from the host
regressions. Cartridge contact/occlusion, broader weapons, campaign permissions
and tracking/menu/save/death recovery retain their own gates. The complete
collected run still retains its canonical telemetry rejection for the unobserved
loading-screen Sense continuation; operator acceptance does not change it.

The configured Win32 Release build and CTest suite qualify host behavior only.
The classic shared-texture test may be skipped when the environment does not
provide that capability. On the current host, classic shared render
targets remain unavailable (`d3d9_classic_d3d11_shared_texture`); the pool probe
passes. Transient classic-device creation unavailability has also been observed.
The skip is neither an accepted fallback nor a failure of the active D3D9Ex
transport. The suite
covers production policy, real host GPU interop, synthetic JNI/native fixtures,
deployment/restoration and evidence rejection; it does not exercise a campaign
or initialize SteamVR for the OpenVR input fixture.

Public distribution has host coverage for inventory/tamper rejection, unknown
builds, foreign staging, owned recovery with truncated stage JSON, Unicode video
paths and bounded local reports. An opt-in isolated copy of the exact game also
exercises install, tracked-head/body enablement, all five controller files,
fresh session identities/log rotation, conventional/manual updates, byte-identical
uninstall and injected deployment failure. Current coverage restores retained
reload choice and exercises launcher option 8; .3's forced-on upgrade policy is
historical. Redirected Documents/cloud-tag admission, real junction rejection,
missing ancestors, exact video restoration and recovery-path replacement rejection
have host coverage. Cloud metadata is simulated; real OneDrive still needs
community confirmation. These changes do not promote physical gates.
The extracted package uses Windows PowerShell 5.1 and bundled isolated Python.
These checks do not establish another PC, controller, campaign or headset gate.

Binding checks now reject unexpected or duplicate Sense source routes. Flat-menu
pointer uploads reuse presenter-owned scratch storage. These changes have host
coverage, without new headset binding acceptance or measured pacing improvement.
Building the optional OpenXR targets does not qualify an OpenXR game integration.
No feature is promoted to `supported`; the project remains pre-alpha.

The implementation gates and their dependencies are in
[Roadmap](ROADMAP.md#next-implementation-gates). Changes already host-tested need
the consolidated acceptance below, rather than being listed as missing code.

Scripted in-game cinematics now use native movie ownership to select flat theater,
including the naturally rendered module subtitle sprite. The running timer alone
previously admitted these movies as tracked stereo and omitted that subtitle path.
The movie getter/owner-loss/end and JNI fault cases are host-tested. Exercised
entry/exit and mode-switch recovery now have the declared operator headset
acceptance below. The operator separately confirms subtitle appearance in
exercised animations, with rendering mode unspecified. Broad comfort, unreported
movie modes/timing and adversarial held-control/loss cases remain separate;
ordinary dialogue acceptance alone does not qualify these broader movie cases.

Community feedback identifies failure to recover stereo after the cinematic on
arrival in Hope, including a Quest 3 report. Reviewed community telemetry contains
three captured-but-unsubmitted pairs per process, persistent capture-ring
saturation and no later return to stereo after the final logged movie entry. Normal
shutdown completes without shared-texture open/copy or compositor-submit errors.
Source inspection and a failing real-device host regression establish that an
owner change before `EndFrame` can strand each unfinished slot until the ring is
exhausted. The next capture now reclaims only its unfinished unpublished slot;
same-device GPU command ordering preserves writes, while pending fences and
consumer leases remain protected. Classic readback and D3D9Ex/D3D11 host tests
cover repeated one-/two-eye cancellations, a fresh recovered pair, retained
consumer content and independent pending-frame survival. These are host tests;
the operator separately declares physical headset validation that Hope recovery
is fixed, including continued gameplay. The community log spans multiple launches
under one public run identity and remains noncanonical; this declaration does not
retroactively qualify that log, invent telemetry or change an archived verdict.
The correction is not Quest-specific, and PS VR2 acceptance does not validate
Quest recovery. Theater during the movie remains the intentional default.

A campaign crash was reported while interacting near a chest/possible scroll.
The earlier canonical run remains interrupted/rejected and original staging was
restored. A later community secret-scroll report now supplies a precise native
fault: ChromeEngine3 `0x4609E` reads `current_level+0x378` with a null current
level. Production stops on the interaction that acquires native UI. Static native
inspection establishes the scheduler's ambient binding and its clearing on level
removal; the exact nested callback that cleared it is not observed.
The adapter now conditionally restores only the captured, still-active render
level under the same singleton at the injected input dispatch boundary, before
native continuation. Foreign/removed owners and unavailable write authority
reject without revival. This correction
is implemented/host-tested, including the production dispatch followed by native
fallback and checked-memory lifetime cases; it is not a demonstrated campaign
crash fix. Native secret credit,
menu/timer ownership and exception propagation remain unchanged. Picking up a
new scroll, viewing/closing its menu and resuming stereo/control remain a physical
gate in the next relevant combined session, without a separate immediate run.
The diagnostic source now also captures a bounded 16-byte window at the faulting
EIP when readable, explicitly marks unavailable instruction memory, and restores
the thread's Win32 last-error. Synthetic host tests confirm those properties and
unchanged exception propagation. The later community scroll report includes
these faulting instruction bytes from an actual escaping native fault. This is
diagnostic runtime evidence, without crash-recovery or headset acceptance.

Physical straight, curved and ascending punches are implemented/host-tested for exact `WeaponHands`.
An earlier operator trial showed independent Hands but no damage. A detector
regression reproduced fresh repeated
presenter poses cancelling the release/guard/collected path. Fresh duplicates now
retain that state and the original timestamp; stale, regressing and interrupted
poses still cancel. Bounded admission/gesture diagnostics distinguish trigger
availability, release/guard stages, owner/context loss and native attempts.
Available open-fist repeats retain a confirmed release while cancelling any old
stroke; a later deliberate close admits a new measured movement.
The later trial still produced no damage despite admitted native Hands. Recorded
time discontinuities and a deterministic 90 Hz cadence replay exposed coarse
adapter timestamps aliasing distinct poses. High-resolution steady time corrects
that boundary without relaxing gesture thresholds. No native attempt was logged
in that trial; contact/damage remain unaccepted.
The latest trial confirms intermittent physical damage, predominantly with a
straight extended arm. Natural hooks and repeated closed-fist combinations
remain unreliable and are not accepted. The intended interaction is sustained
one- or two-trigger fist closure plus natural motion, without trigger pumping
or repeatedly parking the hand in a stationary guard. Native permissions,
cooldown and damage remain authoritative; observed attempts do not establish
hit rate or contact alignment.
The subsequent completed-curve candidate was also rejected: the operator still
reports one effective punch in many attempts. That run contains 16 native melee
entries, nine requests with native `Hit` calls and seven without; neither these
counts nor synthetic gesture coverage establish reliable enemy damage. Long
curves and discontinuities also left closed fists behind release fences. The
active integration was subsequently replaced with current-contact evaluation and
measured displayed-fist geometry. Cinematic continuity was not exercised in
that rejected session; later declared physical acceptance is recorded separately.
Select native Hands, release the triggers once, then hold either or both
corresponding triggers to claim closed fists. The current implementation queries
contact on each fresh pose using measured displayed knuckle skin and only the
adjacent pose's movement. Jabs, hooks, uppercuts and short fast movements need no
completed curve, guard pause, minimum stroke length or timed rearming. Native
attack permission requires real grip and head-relative translation at the
existing speed threshold. A stationary or rotation-only fist cannot attack.
Contact latches the hand until a later clear query, preventing repeated attempts
while touching the same surface. Long movement no longer exhausts a stored path;
a discarded geometric jump rebaselines without requiring trigger release.
Native permissions, selected attack parameters, cooldown, collision and damage
remain authoritative. The native animation no longer schedules this VR attempt.
Four original skin witnesses follow the actual displayed finger frames and skin
weights for both hands and both player meshes. Queries use separate current
movement rails and a current knuckle cross-section, not a historical curve or
cross-finger movement shortcut. No arbitrary reach or radius is added. The
native query's endpoint mutation is isolated with copies, and owner exclusion
flags restore on success, miss and exceptions. One native attack/hit list is
invoked on eligible contact; query-only poses cannot spend cooldown or damage.
This is sampled skin contact, not complete fist-volume or guard/blocking support.
The latest visor trial validates useful physical contact in one exercised fight:
the operator defeated three enemies with Hands and reported a substantial
improvement. This promotes the current contact route to scoped headset-validated
for that context. It does not establish every punch shape, both-hand parity,
campaign-wide permissions, or recovery after tracking/menu/recenter loss. Native
attempt, melee-entry and `Hit` counters remain diagnostics rather than health
evidence; the operator's reported enemy defeats provide the physical outcome.
Dual pistol reload is unchanged.

## VR settings and cinematic choice

Post-.5 source retires the held-Options quick editor and its renderer resources.
Native Options > Game owns cinematic/turning/weapon-guide edits and retains the
existing saved version-3 preferences. Options acts on a fresh press, with no hold
activation or release replay. Startup/focus/owner release barriers remain separate
from editing. The retired overlay's earlier scoped acceptance is historical; it
does not require retesting or establish acceptance of the changed press timing.

The operator reports that buttons skip movies in theater but do nothing in
immersive presentation. Source review identifies both the proxy's cleared UI
edges and the camera-only early return. A separate skip intent now reaches the
original native Escape consumer while gameplay stays blocked. This correction is
implemented/host-tested for fresh input routing, one-sample consumption, native
eligibility, module/movie identity and JNI fault cleanup. Physical acceptance
remains pending; it must preserve
native unskippable scenes and cannot claim a completed fade from delivery alone.
Owner-stamped UI edges reject an earlier gameplay request on movie entry, and a
temporary natural-output fallback retains strict movie dispatch instead of generic
pause/navigation. No prior request is retried after a native rejection or fault.
Movie-to-movie owner replacement additionally invalidates pending input even when
presentation flags remain unchanged; its JNI identity epoch is tested separately.
The neutral release gate and fresh-press Options timing have offline coverage for
held startup/recovery, absent bindings, owner/time changes, unavailable trigger/
wheel history and nonfinite sticks. Preference persistence, native menu patch,
archive/JVM and bridge fixtures pass after quick-panel removal. These are host
contracts; the changed press timing has no new physical acceptance.

The boxing death report was an unresponsive native menu, not a demonstrated
renderer/process deadlock: fresh frames and normal shutdown continued. Exact
module bytecode identifies Space/ Escape as menu 38's native consumers; generic
UI Enter/mouse delivery does not reach that continuation. Visibility/type-gated
native dispatch and failure cleanup have host regressions. The latest operator
confirms death presentation/recovery worked in the exercised boxing context;
broader retry/exit and held-control transitions remain unaccepted.
Theater remains the default. Opt-in immersive movies use camera-only stereo:
gameplay/body/weapon mutation stays blocked, and invalid/dead/mission-end/frozen
owners retain theater. Immersive subtitle transport is now implemented/host-tested:
the exact module writer caches its normalized key/ID/movie, and the reader checks
active module, movie, native ID, visibility and subtitle setting before copying
localized Unicode into the captured pair. Wrong-ID hides, lifecycle/reentrant
callbacks, stale owners, JNI faults and the shipped JVM verifier have host coverage.
Theater retains its naturally rendered native pass. The operator confirms
subtitles appeared in exercised animations, without specifying the rendering
mode. That appearance is accepted in scope; broader mode coverage, timing,
readability and hide/skip/end behavior remain unqualified. Host tests do not
qualify immersive comfort or recovery.
An earlier immersive trial reported intermittent wrong-image flashes lasting
about a second. After the continuity corrections, the operator reports no problems
while toggling cinematic presentation and entering movies, and no persistent flat
presentation afterward. The operator now explicitly declares the Hope stereo
recovery fixed and **headset-validated**, across multiple exercised cinematics,
entry/exit, hot theater/immersive switches even mid-sequence, different weapons,
headset recenter and continued gameplay. This is declared operator physical
evidence, not a newly collected telemetry result. It supersedes the earlier
Hope-specific physical gate without changing archived rejections. All-campaign
cinematics, unreported subtitle modes/timing, full comfort coverage, extreme
device/tracking loss and other headsets remain open. Continued gameplay does not
independently qualify every hand/reload interruption. No repeat physical tests
are requested for these accepted cinematic checks.

Retain the following protocol for later changes or unexercised contexts; it is
not a request to repeat the accepted settings/cinematic baseline for .2:

1. **Settings:** press Options once for native pause; native Options > Game
   changes Cinematics. Holding Options must not open a second panel or replay a
   pause on release. Preserve available-release barriers after owner/focus loss;
   no wheel, shot or reload may leak through navigation.
2. **Physical melee:** the current contact route has scoped visor acceptance after
   defeating three enemies with Hands. On a later naturally available fight,
   broaden left/right hand, punch-shape and tracking/menu/recenter recovery
   coverage. Do not repeat the accepted fight solely to reconfirm basic damage.
3. **Movies:** set the mode before a naturally encountered scripted cinematic.
   Check theater and immersive when scenes are available, including native
   subtitles in theater, head tracking, coherent eyes and ordinary input after
   the movie. In unreported modes, broaden localized text timing, clearing at
   native hide/skip/end and subtitle-setting behavior; no previous movie's line
   may remain. Basic appearance and accepted recovery are not repeat gates.
   Do not replay a long campaign segment merely to repeat an accepted baseline.
   A fresh immersive Options/Circle/Cross/trigger selection should reach native
   skip when the scene permits it; a held entry/repeated frame must not skip again,
   fire or open pause after exit. This is a new input check, separate from accepted
   recovery. Frozen/unverified owners retain theater as the conservative fallback.
4. **Persistence:** after normal quit, prepare a fresh candidate for any later
   session and confirm the chosen mode was retained. A displayed save failure
   means the selection applies only to the current session.

Native Options > Game now includes cinematic and smooth/snap selectors using
the original game's widgets/fonts and original main/pause routes. This is an
implementation with host verification and scoped operator headset acceptance:
the native options work correctly in the exercised session. Individual widget
paths not described by that report remain unobserved.
An additional localized VR comfort selector controls holster guides, off by
default; the operator accepts the exercised guides and assistance on/off.
Unreported visual comfort, persistence and recovery remain separate gates. Labels
follow
the game's startup locale, with English/Spanish host coverage and English fallback.
The shared preference store preserves version-1 cinematics, defaults turning to
smooth, reads version-2 choices with guides off, and writes strict version 3.
Cinematic-only changes preserve other choices. Invalid/unreadable startup files
remain preserved; edits can apply for the session with visible save failure.
Host fixtures cover editor conflicts, native OK request consumption/completion,
Cancel/defaults, missing resources, failed saves, scoped JNI cleanup, per-Show
recovery and preference preservation after quick-panel removal.
The shipped Java 1.4 verifier also accepts the transformed menu without class
initialization and rejects a deliberately invalid helper. This is JVM bytecode
verification, not native widget rendering or game execution. The positive
operator usability report does not separately establish both callers, native
text/font fit, selection by pointer, OK/Cancel/Defaults, persistence, switching
turn modes or held-input recovery unless specifically exercised.
Resolution remains a launcher startup preference; no live size selector is added.

The launcher reports known startup-resolution fallback causes and the latest
identity-matched preparation outcome, separately from the saved next-session
profile. Reporting fixtures do not observe effective raster dimensions or add
headset/profile acceptance. Unsupported or unavailable requests still preserve
the selected game resolution through the existing reversible transaction.

Comfort options beyond cinematic presentation, turning and optional holster
guides remain planned. The new guide option is host-tested; physical comfort
and native row fit remain open.

## Current physical gate

Do not repeat an accepted baseline unless a change affects it. Batch related
checks into one meaningful candidate, recording unavailable cases as pending.

- **Community input:** the final candidate's exercised movement and default
  smooth turn have consolidated operator confirmation on PS VR2. Do not request
  another identical check solely for confirmation. Other headsets/controllers,
  sustained comfort and adversarial owner/input recovery remain separate gates.
- **Clyde duel:** the operator confirms successful right-stick down/up draw and
  raise followed by tracked-hand aiming and shooting. That exercised sequence
  is headset-validated and needs no repeat solely for confirmation. Native
  early-draw rejection, retry, recenter and return to ordinary turning remain
   separate gates. Do not force weapon/countdown state to pass them.
- **Dual weapon reticles:** the combined operator confirmation accepts the
  exercised two-marker presentation. Exact ballistic agreement, protected-target
  warning appearance and stale-mark recovery under pause/tracking loss retain
  separate gates. It does not qualify all countdown/UI campaign contexts.
- **Interaction:** point the left Sense at a drawer's usable part and press L1.
  Head turning must not retarget it. The cyan reference must exist only while
  L1 is held. Check opening, pickup/carry/put-down and release after tracking loss.
- **Long-gun support hand:** the bounded normal-state correction reuses controller
  curls with a matching applied right-rifle transaction. Its exercised free-hand
  appearance is operator-accepted after retesting. Broader equip/fire/reload
  transition coverage remains separate. Long-gun reload remains conventional
  Square/native animation, outside the manual whitelist; physical manual reload
  is not implemented or accepted.
  A separate host-tested Winchester-only read-only reload observer now records
  native hand states, flags, animation ID/time, rifle reserve, raw ammo readback
  and optional natural support pose. Its opt-in diagnostic is
  `winchester_native_reload` when `reloadTraceEnabled=true`. It tolerates
  unmeasurable natural pose without admitting or writing one and rejects changed
  owner/class/JNI reads. Readback is not independently proven clip occupancy.
  **Physical gate still pending:** use a fresh `vr_test.ps1 prepare`/`finish`
  run, enable trace via `tools/set_hmd_camera_control.ps1 -Mode reload-trace-enable`
  with the staged `-GameDirectory`, manually run SteamVR/game, then exercise
  an ordinary Winchester idle, fire, Square reload and interruption. Correlate
  recorded left/right state tuples, reserve/readback, support-hand movement and
  native animation before designing a rifle loading port or manual input.
  A read-only exact-class rifle support observer has host coverage for native
  high-bit identities, aliases, state/flag transitions, geometry and JNI failures.
  It samples natural root/Holder/left_hand frames before scoped writes, with
  bounded fire-intent bursts and measured gaps. The analyzer rejects overclaimed,
  malformed or inconsistent histories. Live source timing, support movement
  through fire/end remain unqualified. The support-grip prototype now uses a
  measured mesh fore-end reference, frozen in weapon space for native cycling;
  the native left socket retains only anatomical/verifier ownership.
  Paired real-bridge transactions, coherent same-ID replacement, restore retry,
  excessive separation and duplicate/stale samples have host regressions. These
  alone do not establish physical placement or stability. The operator separately
  accepts the corrected Remington bimanual grip; other models and unreported
  stability/recovery cases remain open as specified in the rifle row above.
  Additional host regressions cover same-numeric-ID Java replacement between
  completed frames, retained object identity through native firing states,
  failed cleanup/retry and clear/reset. Unexpectedly inactive or mismatched
  supported maps invalidate the whole stereo pair, including final verification
  and the legacy callback route; restoration cannot promote an invalid pair.
- **Cartridge grip:** the current anchor has already been observed in runtime;
  repeating it solely to reconfirm visibility adds no evidence. After a material
  contact/axis/occlusion correction, combine empty-hand waiting, trigger pickup,
  slow wrist roll, release away, successive insertion, closing/firing and
  menu/weapon recovery. Include both hand assignments when available.
- **Manual mechanics:** the port's visible presentation is operator-accepted;
  do not repeat that check alone. Precise insertion still requires a measured
  loading-mouth frame and clearance through native drum phase.
- **Recovery:** qualify tracking loss/recovery, held controls across ownership
  changes, death/retry, save/load and disable during loading when available.
  Require no stale round, delayed action, extra transfer or permanent lock.
- **Campaign coverage:** mounted controls, broader weapons/special optics, whip
  climbing and optional HUD content need eligible scenes. The protected-target
  red aiming cross remains a separate low-priority check.

Authored VR labels now follow startup English/Spanish game locale; production
UTF-8 compilation, UTF-16 accents, native text preservation, localized wheel
confirmation and bounded unknown-locale fallback are host-tested. Judge English
wording, Spanish accents and card/wheel fit in the next relevant combined session;
no immediate or dedicated language test is requested. Native copied text retains
its own localization and visibility settings.

## Persistent manual reload candidate

Use one eligible single Peacemaker/Frontier with an empty support hand, missing
rounds and reserve. Prepare with `-BodyIkAtStart -LargeAddressAware -ManualReload`.
The support **trigger**, not Grip, carries/releases the cartridge.

Additional Frontier, Walker and Cogswell exact profiles are implemented/host-tested
for the same native unit-loading route. Cogswell observes rifle reserve; the
other admitted profiles observe pistol reserve. Schofield base/A/B retain native
whole-clip reload; LeMat and unknown variants retain ordinary native handling.
Exercise each new model only when available in the combined campaign session:
Square opening/closing, one loaded round/matching reserve decrement per release,
proper capacity/no-reserve closure, both hand assignments and recovery. Existing
Peacemaker acceptance does not promote these profiles or their loading geometry.


The exercised flow is `READY -> OPENING -> MANUAL_LOAD -> CLOSING -> READY`.
Fresh left Square prepares; native opening enters a supervised wait without
automatic transfer. Weapon and independent hands retain controller ownership.
Pick up at waist with the free trigger, carry to the existing weapon-grip zone
and release. Each accepted insertion calls native `WeaponReload` once; native
inventory/capacity/reserve remain authoritative. A fresh pickup/claim can repeat
the process. Square again or the armed trigger closes early; that closing trigger
press must not fire. Release and press again to shoot. Full/no reserve closes
without another round. Unsupported positively identified contexts retain native
reload; observation failures do not authorize that fallback.

Operator acceptance covers Square preparation/closure, no automatic loading,
tracked hands, successive loading, insertion-only haptics and the exercised
weapon/menu interruptions. Complete inventoried right-hand Peacemaker captures
correlate accepted insertions with `loaded +1 / reserve -1`, sustained waiting
beyond the original bounded probe interval, coherent closing and scoped recovery.
The offline analyzer found no unexplained transfer in the reviewed captures.
These findings do not accept every weapon, interruption or comfort condition.

The operator still reports a floating cartridge rather than convincing finger
contact. Convincing grip remains the next visual priority. Offline contact replay
finds slight pad/case overlap, not a gap justifying another position offset;
this does not establish runtime skin contact or occlusion. The short support-thumb
gesture is implemented and host-tested, with no live/visor acceptance. It begins
only after a qualified native unit transfer, flexes from .7 to .75 and returns to
ordinary fingers by 180 ms while the cartridge stays hidden. Native closing
retains priority, so an immediate final-round close can suppress this gesture.
The adapter now distinguishes a continuation token from a physically held round;
composed host tests retain the full thumb gesture while that token is unclaimed,
and cancel it on a fresh pickup, owner/tracking loss or native finger rejection.
Physical acceptance must judge its timing and appearance on both support hands,
including interrupted insertion and immediate repickup; no replay or extra round
is permitted.

The active insertion zone is the existing approximately 14 cm grip-proximity
zone. It is not an individual chamber or measured loading port. The active
grip carry now rejects adjacent translation exceeding 12 cm or 3 m/s for either
hand, including release-only jumps. These are gesture UX bounds; sampled host
paths at 90/120/144 Hz retain ordinary transfers and continuation. Physical
fast/slow carry comfort and valid-pose tracking-relocation rejection join the
same batch, with no haptic/insertion replay or lost physical release barrier.
The read-only root/barrel/gate/drum observer has bounded live evidence of changed
native gate pose and independent drum motion. The six-mouth pivot-ranking diagnostic has
bounded live observations, but the gate element origin is an unverified pivot:
`gate_open_confirmed=false` and `socket_admission=false` remain mandatory.
The geometry observer now brackets its reads with the same weapon's finite
`m_fRotate` and rejects phase changes. It emits native degrees beside each source
frame; the analyzer preserves this optional scalar separately from chamber or
clearance claims. This correlation is host-tested only; native/displayed phase
composition still needs a correlated physical sample in the same reload batch.
No independent gate/drum writer, spent-case simulation or chamber occupancy is
implemented. See [exact reload research](research/COJ_RELOAD_OWNERSHIP.md).

Optional acceptance haptics require coherent native unit readbacks and fresh
capture/tracking/gameplay ownership. Host tests cover duplicate/stale captures,
replacement, release barriers and output faults; live submission records and
operator feedback accept insertion-only delivery in the exercised context.
The revised zone-entry cue is implemented and host-tested, without live/visor
acceptance; insertion-specific sound remains pending. The soft cue signals a
qualified held approach, grants no round, and does not delay the stronger
native-confirmed insertion cue. Withdrawal hysteresis and separate cadences
prevent boundary jitter from repeatedly pulsing.

`manual_reload_pinch` and `manual_reload_cartridge` have bounded applied/visible
and idle/hidden observations. The optional `manual_reload_finger_reference`
reader has bounded live snapshots of native/displayed distal fingers; its
`weapon_id=0` diagnostic field cannot independently prove weapon ownership.
The revised host-tested emitter uses the native reload snapshot instead of
ordinary admission (which is unavailable while waiting), with explicit snapshot
validity, armed hand and context. New correlation remains pending runtime.
Joint observations do not measure skin contact. The current compositor lacks
scene-depth occlusion; improved anchor position alone does not qualify the grip.
The new optional `skin_pad_gap_cm` samples verified displayed skin-pad separation
before restoration; host analysis rejects nonfinite/out-of-range values and
correlates snapshots conservatively. Whether the .357 visually fits between the
fingers remains pending headset observation.
The optional `case_span_cm` measures the displayed CC0 mesh's full local +Y
transverse envelope (centimetres). The offline signed gap-minus-envelope margin
is a geometry comparison, not an intersection test or headset contact verdict.
An optional, sparse pickup-only triangle-surface probe now samples both verified
displayed skin-pad points before hand restoration. The exact CoJ adapter maps
them into the same mesh-centred local basis used by the rendered CC0 cartridge,
including its axial centre offset, and measures each point's nearest unsigned
distance to the imported triangles (`thumb_mesh_surface_distance_cm`,
`index_mesh_surface_distance_cm`). The offline reader validates both together,
preserves older snapshots and reports the unsigned distance asymmetry. This is
sample-point proximity, not full finger skin/volume intersection, signed
penetration, visible occlusion or a physical contact verdict. Live correlation,
the visual grip and headset acceptance remain open; no anchor offset is tuned.
The same optional pickup sample now also reports each displayed skin-pad point's
signed axial station in centimetres relative to the imported mesh's rear base
(positive toward the tip), plus their longitudinal separation. These stations
distinguish a grip displaced along the cartridge from a lateral surface gap;
they do not establish casing contact or depth ordering. The offline analyzer
rejects partial/nonfinite station pairs and still accepts legacy distance-only
records. Physical interpretation remains pending a correlated headset sample.

## Consolidated candidate batch — host-tested, awaiting runtime and visor

This candidate combines a native-confirmed support-thumb gesture, a
displayed-finger tangent axis for the CC0 cartridge,
qualified zone-entry feedback, stricter Square rearming after ownership/input/
tracking loss and native Square recovery after disabling manual reload. Native
ammunition, the 14 cm insertion zone and body presentation remain unchanged.
Both-hand geometry and rotated-camera tests, fresh/duplicate/invalid gestures,
haptic priority/freshness and context/disable barriers have host coverage.
Direct native completion without an observed closing sample now discards pending
continuation tokens. Failed start/fresh preparation also clear them; a new
session requires its own initial waist pickup. Host regressions reproduce and
cover the former cross-session continuation and the former stale synchronous
ownership result on native insertion failure. Rejection versus invocation fault,
recenter, sample gap and regression have explicit cancellation reasons. No live
or visor acceptance is added by these regressions.
The read-only D3D9 depth observer is host-tested against a real host device,
including absent depth, attached D16 state and subsequent reset. It has no
exact-game observations yet and cannot authorize occlusion.
Offline same-frame depth pairing is host-tested against complete, missing,
reversed, duplicated, cross-generation/device and invalid-metadata fixtures.
It reports correlated/incomplete/rejected diagnostic groups without granting
capture correspondence, scene/hand coverage or a physical occlusion gate.

Use one fresh canonical candidate when the operator is available. There is no
request to repeat separate micro-tests or unavailable campaign contexts.

1. **Grip:** open an eligible revolver, wait with an empty support hand, take a
   cartridge with its trigger. Inspect close-up, wrist roll and palm-up/down.
   The port's accepted visual presentation needs no separate repeat check.
   Check the bullet follows the pinching fingers rather than cutting through
   them. The current compositor still cannot hide it behind fingers/walls;
   record that limitation separately from position/axis fit.
2. **Feedback and transfer:** carry into the grip zone while held. Expect one
   soft entry cue and no ammunition change. Small boundary oscillation must not
   repeatedly vibrate. Release for one stronger cue and exactly one native round.
   On a continuing load, the cartridge disappears and the support thumb makes a
   brief push before relaxing; it must not restart after an interruption or
   override a new pickup. Native final-round closing can suppress the gesture.
   Repeat several insertions; withdraw clearly to rearm entry feedback.
3. **Drop and closure:** release away from the gun, close with Square, reopen,
   close with the armed trigger, then release/press to fire. No extra insertion,
   retained pinch or firing on the closing press.
   After filling the gun, fire one round with a fresh press and reopen: the first
   cartridge of the new session must require pickup at waist. This extends the
   same batch, not a separate physical test.
4. **Held-control recovery:** carry a round, pause/change weapon/recenter or
   temporarily lose tracking, then recover with Square/triggers still held.
   No stale round, queued insertion or automatic reopening. Release and press
   deliberately to recover; also check returning from a menu while already
   `READY` with Square held.
5. **Lifecycle:** in eligible scenes, save/load, die/retry and use canonical
   `disable` during manual loading. Confirm restored control and conventional
   reload after releasing/pressing Square. Unavailable cases remain pending.
6. **Interaction and coverage:** left-Sense L1 drawer/pickup/carry/put-down while
   looking elsewhere; cyan reference only while held. Repeat reload with a
   left-hand assignment or Frontier if available in the same session.
7. **Menu/gameplay and HUD:** include the combined ownership regression below,
   holding mapped controls across pause/dashboard/tracking loss. If damage or
   healing is available, check the wrist meter against native health text,
   ammunition readability and hiding on blocking UI. Include source-size recovery
   only if a resolution transition occurs. If concentration or horse feedback is
   naturally available, check the discrete badge against its native text and
   the paired meters against health/fatigue percentages. Fatigue is not stamina;
   no cooldown duration/progress is inferred. Hidden owners must leave no stale
   glyph/bar, and essential health/ammo/countdown rows retain priority. These
   additions join the later combined session; no separate physical test is needed.

The default-off trace records zone entry/withdrawal, visual position/tip axis,
native insertions and submitted haptic type. Up to six `manual_reload_depth`
observations per opening record each eye's current target/depth/state at initial
held appearances; matching layout is not proven content. The offline analyzer
keeps these distinctions and never promotes a physical gate.

## Hybrid motion reload acceptance

This is the retained legacy path, separate from `-ManualReload`. A deliberate
free-trigger waist-to-gun journey requests ordinary native reload. Corrected
initial admission has scoped operator acceptance on the exercised Peacemaker.
Native animation still owns the original two-hand presentation.

## Cartridge-paced reload acceptance

The legacy Peacemaker/Frontier extension limits each natural completion to one
native round. Corrected recovery has bounded live evidence of replenishment and
unit count/reserve changes. It still takes the hands during each native cycle;
it is not the persistent manual flow and has no accepted physical chamber UX.
Keep it as a fallback/compatibility contract, not the next physical development
target. Schofield retains whole-clip native behavior.

## Bounded native wait/zero-transfer probe

The explicitly selected `-ReloadWaitProbe` derivative is host-tested and bounded
live-tested on a right-hand Peacemaker. It uses a roughly 1.5-native-clock wait,
grants zero rounds and closes through native callbacks; Square stays conventional
and presentation remains native-owned. It cannot combine with `-ManualReload`.
It established the logical veto/cancel seam used by the later manual candidate,
not mechanical clearance or persistent tracked-hand acceptance. No repeat probe
is needed merely to reconfirm that discovery.

## Revised Sense control acceptance

The [default Sense table](research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls)
is the sole binding reference. Exercised direct controls, wheel confirmation,
haptic highlights, snap rearm, on-foot crouch, ordinary R3 Focus and Create
timing have scoped operator acceptance. Ray's Bible selection/use and on-foot
kick are also operator-accepted in exercised gameplay. This does not qualify
concentration, duels or campaign permission transitions. Mounted contexts,
broader weapons and bow/scoped optics retain independent gates.
Bow concentration magnification now has host coverage through its original
module/BulletTime caches, including ordinary Focus combination, smoothing/decay,
equip/replacement/fault rejection, asymmetric tangents and unchanged IPD. In a
naturally eligible bow scene, check concentration with Focus off/on, both-eye
alignment, comfort and return to physical optics after exit or interruption.
This does not qualify physical string drawing or the separately excluded scope.
The exact bow aiming correction additionally admits its missing-Barrel native
root/+Z fallback on one stable actual/desired hand owner and its DBowHolder.
Host tests cover vertical rigid maps, alias rejection and scoped ray publication;
the community's pitch-lock/mixed-hand/crosshair report remains physically open.
In the same bow scene, move the native holding hand up/down/rolled while keeping
the other hand still, then reverse; verify mesh/arrow/reticle agreement in both
eyes, physical right-trigger draw/release, equip/pause/recenter recovery and concentration.
A straight aim marker is not a predicted arrow impact point.

The operator confirms that the bow worked very well in the later exercised
session after the earlier substantial improvement from separating renderer
capture drops from physical grab lifetime. Correlated diagnostics show
native attack entries and retained grabs during renderer-only backpressure;
attack entry alone does not count confirmed arrow spawns. This is scoped physical
functional acceptance in the exercised use. Sustained campaign consistency, interruption recovery,
visual alignment and haptic quality have not been separately confirmed.
Earlier no-arrow and frequent-cut trials remain rejected historical evidence.
The first aiming-only correction had no physical string-draw policy; a later
physical implementation still lost grabs when capture-ring backpressure occurred
despite successful attempted camera/resource restoration. That lifetime defect
is corrected with host regression coverage, retaining independent captured
feedback gating and cancellation on invalid gameplay, ownership or restoration.

The current interaction is: left holds/aims, right approaches the authored string
point, fresh R2 grabs, right retraction tensions, and R2 release requests one
original native shot. L2 is not required. Gesture, exact owner/state/JNI admission,
native channel supervision and captured controller haptics have host coverage.
No repeat of accepted body/options is requested to investigate this gesture.
Check one short/new draw does not fire, a deliberate pull/release fires once,
holding R2 does not repeat, and returning toward the bow relaxes tension.
Continuous sideways or slack movement must preserve the captured R2 hold while
removing shot tension; release outside valid alignment must not fire. A normal
held draw must survive the end of native concentration without extending its
zoom/slomo lifecycle. Ownership, tracking and time discontinuities still cancel
and require released input before another grab.
In the same naturally available context, interrupt once with the wheel/pause and
recover tracking/recenter with released input. Old release/haptics must not replay.
Judge grab/tension/release vibration separately from actual arrow appearance.
The original authored draw animation and ballistic speed/damage are retained;
physical draw distance is not a newly qualified native power or animation clock.

The operator also exercised mod-added DLC and considers it unprepared for VR;
compatibility work is deferred at their request. Mounting and dismounting a horse
worked in that exercised context, with native mounted gameplay observed. This
is scoped functional feedback, not acceptance of mounted movement, gallop,
camera/aim continuity, horse HUD or general DLC compatibility.

## Automatic menu-pointer acceptance

The automatic pointer uses same-hand L2/R2 selection; Cross accepts, Circle backs
out and Options pauses. Exercised ordinary menus, confirmation dialogs and mouse
coexistence are accepted. Dashboard, loading, resource replacement and held-click
recovery require broader coverage without stale clicks or haptic replay.

## Combined menu and gameplay regression

Changes to ownership need menu -> gameplay -> wheel -> pause -> dashboard ->
gameplay coverage plus available death/retry and save/load. Hold mapped controls
across transitions: no delayed fire, interaction, reload, snap or utility action
may escape on recovery. Native permissions remain final.

Global OpenVR button rearming, failed-poll rollback and partial stereo-submit
invalidation are implemented/host-tested. Host regressions reproduce an
unavailable button fabricating a press on held reacquisition and an old left-eye
confirmation completing across a tracking/lifecycle gap. The real input-poll
transaction is exercised with partial outputs and exception unwinding without
initializing the runtime. These checks do not qualify physical recovery.

Pointer source-size cancellation is implemented/host-tested for width and height
changes with unchanged hand/menu identity, stale dispatched snapshots, held/edge
packets, renewed hover and deliberate selection. A deterministic interleaving
also reproduces and covers an idle native cursor result returning after resize.
Include held L2/R2/Cross/Circle/Options recovery after available focus/dashboard/
tracking loss in the next combined session; check source-size recovery only if a
resolution transition occurs. No extra visual micro-test is requested.

## Production D3D9Ex startup/reset gate

Primary GPU transport and normal shutdown passed their bounded physical gate.
Arbitrary reset, abnormal shutdown, sustained pacing and campaign transitions
remain open. A skipped classic shared-texture environmental test is neither a
pass nor physical validation. Transient host D3D9 CreateDevice failures have also
been observed; their cause has not been established.
Implemented hardening retains producer leases on a failed D3D11 copy-fence poll
and moves lease bookkeeping ahead of copy submission. Sampled lease telemetry
tracks copy ordinals and accounts for shutdown-drain retirements. The Release
build and GPU transport host test pass. Test-only injected GetData failures on
real queued D3D9Ex/D3D11 copies verify retention through pending proof, ResetEx
and device replacement. A separate event query on the original immediate context
proves retirement of all quarantined copies before cache reopening and fresh
generation admission. The fixture covers multiple retained leases, repeated
recovery, D3D9Ex/D3D11 replacement, mismatched endpoints and stale generations.
If that independent query also fails, cache reset, later polls and shutdown keep
the uncertain lease; the fixture explicitly drains physical GPU work before
destruction. Destroying a failed bridge does not mark consumer_done: a real
producer fixture retains its occupied slot, rejects generation replacement and
defers invalidation/shutdown after the consumer disappears. Original errors
remain counted, and the canonical verifier persists a failure even when recovery
succeeds with matching copy/completion totals and
zero pending/abandoned leases. This is host-tested consumer-query recovery;
native device removal, resource teardown/session recreation and runtime/headset
performance remain unverified. No timeout or skipped frame grants lease reuse.
A separate test-only D3D9 producer GetData failure while a real GPU copy is
pending now verifies quarantine of the affected ring slot, successful collection
through a healthy successor, and a single invalidation count across shutdown.
The fixture drains its GPU queue before teardown. Both failure injections run
only in the dedicated host test target. Full Release build and CTest have zero
failures; the host verification section records environmental skips. Production
device-loss/reset behavior after an actual failed query remains unobserved.
The host startup contract additionally exposed partially restored factory slots
already pointing to retained originals. Cleanup now acknowledges those exact
targets without a write, preserving foreign conflicts and protection-failure
recovery. A deterministic regression and repeated host startup checks cover this
case; the external restorer and exact-game lifecycle effects remain unobserved.

## Closed locomotion diagnosis

Native per-axis shaping and float actions 4–7 remain the accepted movement seam.
Measured vanilla movement/jump parity closed the earlier diagnosis. Preserve
camera/body restoration; no speculative physics retuning is pending.

A later community gaze-direction report exposed two host-reproducible input
defects: rotating before axis shaping drops the lateral component of a 0.08
forward stick at 30 degrees, and a 0.005 dispatch tolerance suppresses small
gaze changes. Shaping before yaw and a 0.00001 tolerance have regression coverage.
The exercised PS VR2 candidate has operator confirmation; this does not prove
the complete other-headset community drift report is resolved. Native forward/
strafe speed coefficients retain their existing owners. Follow-up community
feedback reports remaining 20–30-degree drift and visible body/head separation.
The explicit 35-degree free-look cone retained that separation. Its replacement
tracks every HMD yaw change, keeping previous-commit camera/hand compensation and
committing the actor only after stereo restoration. Small/reversing/wrapped turns,
held-pose settling, world movement and fixed-hand continuity have host regressions.
The operator accepts continuous HMD body following in the exercised PS VR2
gameplay. Physical translation/rotation still leaves the legs static and visibly
sliding; locomotion/turn-in-place animation is a separate planned presentation
gate. This acceptance does not establish resolution of other-headset community
drift or broader movement/recovery contexts. No repeat of ordinary body following
is required solely to reconfirm this result.
Continuous default turning is host-tested at multiple sample rates and across
loss, duplicate-time, wheel and owner transitions. Ordinary smooth-turn behavior
has scoped operator acceptance; prolonged comfort and adversarial recovery remain
open. The operator reports native options work correctly in the exercised
session; unspecified menu paths, persistence and held-input recovery remain
separate gates.

## PC mechanics and gameplay HUD completeness

Critical countdown presentation, encountered objective/journal notifications and
corrected directional attack feedback have scoped operator headset acceptance.
They are implemented and host-tested independently of wrist visibility.
The countdown copies native localized title and exclusive numerical cache; green
direction/red damage chevrons preserve active native indicators, absolute bearing
and fade alpha. Reader faults/foreign owners, bearing conversion matching the
native camera-back convention (four cardinal directions, head and reference yaw),
real binocular GPU pixels, invalid second eye and stale/empty snapshots are tested.
The operator confirms the recovered countdown/notices and the corrected attack
marks in the exercised scenes, including the requested direction checks. The
camera-back conversion preserves native front/back/left/right placement under
head and reference yaw. Broader simultaneous-content readability, unencountered
notices and pause/loading/death or owner-loss transitions retain independent
gates. No timer/damage authority is recreated. This does not accept complete
graphical HUD coverage or require another run solely to reconfirm these visuals.

Accept only observed content. Horse, broader weapons, climbing, duel/special
modes and campaign indicators need coverage. Full graphical HUD capture and
complete physical cylinder reload are planned.

The operator confirms activating concentration, drawing and shooting both
enemies in an exercised window-entry/hostage-rescue scene. This is scoped
concentration acceptance; it does not accept duel quickdraw or campaign completion.
Ordinary concentration reactivation was initially reported unavailable while
Focus/zoom worked. After the native activation sequence was explained, the
operator retested and confirms correct operation in the exercised setup.
The exact native audit distinguishes alternate fire, Focus and concentration:
concentration requires two empty native hand machines, campaign eligibility
and an elapsed native cooldown before a fresh trigger quickdraw. Fists are not
empty hand machines. Correct controls are documented in the owning controls
research. The confirmed behavior does not establish a prior code regression
or justify claiming a new concentration implementation.
The first Clyde duel is a community-reported progression blocker. Its special
vertical mouse input was absent from the VR mapping. The separate host-tested
duel candidate verifies exact JNI ownership and forwards right-stick draw/raise/
aim to native mouse input. The empty-hand draw conversion uses the current native
threshold/delta/sensitivities; unchanged bytecode consumes actual C++ policy
output at 90/120/144 Hz, three mouse settings, both inversion settings and two
slow-motion factors. This bounded interpreter is not game or visor execution;
cadence differences between dispatch and the later native update remain a live
gate for broader cadence/ownership recovery. The operator subsequently confirms
the Clyde draw/raise gesture and shooting with tracked-hand aiming worked in
the headset. This closes that exercised progression blocker, not native early
draw/retry recovery, countdown visibility or all-campaign duel support.

The newer physical-posture guard is implemented/host-tested separately. A right
aim tip already pointing forward cannot draw through stick-only down/up. Lower
the physical tip at least 45 degrees and center the stick, then pull down; return
above -30 degrees before upward stick can raise the native gun. These gesture
thresholds are UX assumptions, not an anatomically qualified holster. Fresh-pose,
owner/context, stale-input and JNI failure checks do not prove headset usability.
The operator completed physical draw/raise with the hand at the hip and later
explicitly tried incorrect/cheating positions: only the correct position allowed
actions. Both the positive path and exercised posture rejection have scoped
headset acceptance. Native early timing, retry/recenter and ownership recovery
remain separate where not reported.
Original Ray/Billy `HolsterR` and Ray `HolsterL` are native inventory-placement
sockets. Read-only natural pre-body frames now accompany throttled duel diagnostics;
they grant no contact or draw permission. A mount origin alone does not establish
a reachable physical grasp volume, especially with the scoped visual body offset.
Combine its natural draw/raise, denied early draw and held-input/tracking/retry
recovery with the next substantial gameplay batch; prepare no separate duel run.

The new wrist health meter and low-health text colors are implemented and
host-tested, including 0/25/50/100 coverage, hidden/out-of-range cache fallback,
cache reuse and preservation of the ammunition row. Offline rendered images
have been inspected; this is not runtime or headset evidence. Existing accepted
health/ammunition text remains distinct from this new presentation gate.
Add one check to the next consolidated gameplay session: after available damage
or healing, confirm that the bar follows the displayed native percentage, remains
readable beside ammunition, and disappears with its health row or blocking UI.
No separate short headset run is required for this addition.

## Image-quality profile acceptance

**Headset-validated, scoped:** the operator accepts ordinary native stereo with
Manual 2804x2860 after repeated play with perceptibly improved clarity, coherent
stereo and no perceptible performance issues during the tested resolution scenes.
It is the chosen local Manual default for the PS VR2 reference
setup. Prior effective-size evidence supplies full-size color/depth/viewport draws
in both eyes, equal-size captures, distinct measured 2804x2860 D3D11 outputs and
successful matched-pose submission. This is actual world raster resolution,
not window/mirror enlargement. Do not require another identical physical test.

The isolated, visible and native-identified global MenuMain uses the 2560x1440
angular reference while preserving capture aspect and native cursor coordinates.
The operator accepts the main-menu GPU/angular correction and confirms that the
menu-only GPU replacement resolves the severe slowdown.
The rejected CPU resampling variant is superseded; it is not an alternate active
route. Pause, loading, movies and native stereo keep their established layouts.
See [menu ownership](research/COJ_UI_MOUSE_PATH.md) and
[resolution evidence](research/COJ_VR_RENDER_RESOLUTION.md#explicit-native-resolution-trial).

Ray's exercised 8 cm horizontal visual setback with the retained distal-coat
partition has operator acceptance. Retain that partition and scoped pelvis
restoration, without camera, actor/collision, hand-target or Billy adjustment.
This closes the requested fit check, not a cloth-physics or whole-campaign gate.

**Implemented/host-tested:** saved launcher profiles, startup-only application,
query/caps bounds, pixel-count scaling, separate session/install video journals,
conflict preservation and interrupted preparation/recovery. An isolated exact-game
host sandbox exercises profile reapplication, updates and original restoration.
These tests do not qualify other dimensions, headsets or GPU/CPU memory limits.
Missing or incompatible queries preserve the selected native settings with a
visible explanation and retain profile intent for retry next preparation. In .3,
a missing preference uses Auto; every explicit choice persists, including Preserve
and the operator's Manual 2804x2860. This product default does not qualify memory,
Background/Scene identity or other headsets, and publication does not extend the
physical acceptance carried forward from .2.

**Open:** quantified sustained frame timing, recording compatibility, reset/device
loss, memory/VA/loading headroom, general
Background/Scene recommendation equivalence, other profiles and broader campaign
continuity. Full-scene white flashes were reported earlier; subsequent exercised
sessions are symptom-free and sampled source data has no detected white jump.
The intermittent cause remains unknown, so retain the default-off luma observer
without declaring a root-cause fix. The independent private-bank prototype remains
rejected and off. Archived general-verifier failures retain their own verdicts.
Head-turn stability with Steam recording off and good current gameplay have
operator acceptance. Recording-associated microskips remain an observation without
an established cause; no additional recording test is requested here.

Resource/luma probes are off in the selected ordinary candidate; retain optional
diagnostic tools and recovery tests for those open boundaries. The existing bounded
reload trace remains separate from resource/luma sampling. A diagnostic prepare
with binding census can qualify new dimensions, but cannot establish performance.
Normal profile selection is not a request to replay the campaign or redo accepted
2804x2860/Ray checks. Original video bytes and installed assets retain their
transactional restoration, with intervening edits preserved separately.

## Evidence policy

Use the canonical reversible workflow:

```powershell
pwsh -File tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez" -BodyIkAtStart -LargeAddressAware -ManualReload
# Start SteamVR and Call of Juarez manually; exercise relevant gates.
# Close the game normally.
pwsh -File tools/vr_test.ps1 finish
```

Each physical run needs fresh source/build/deployment/run identity. Never launch
or terminate the game or SteamVR automatically. Preserve staging/recovery
journals and original backups until `finish` completes. A historical run manifest
marked `staged` is not evidence of an active deployment; inspect canonical status
and the current journal. Host recovery tests do not establish headset acceptance.

Evidence collection, verifier success and operator acceptance are separate facts.
`analysis/validation-result.json` now persists the canonical verifier's telemetry
outcome and rejection reason, source/build/run identity, profile, requirements and
input hashes. `evidence-manifest.json` inventories that result and the build
manifest; `incomplete` continues to describe only runtime termination. Changed
log/profile/build/deployment/verifier inputs reject a stale executed verdict.
Collection without verification explicitly records `inconclusive` and
`verifier.executed=false`; recollection cannot invent a verifier execution.
Native run/build markers now require one exact start and at most one matching
later end. Duplicate, foreign, malformed or reversed markers reject; profile
checks ignore telemetry before the start or after the end. Invalid intervals
remain incomplete during collection, even with a matching structured summary.
If invalid markers prevent the required performance summary, collection still
retains raw evidence and the non-passing verdict; a valid interval retains its
summary requirement.
These correlation corrections are host-tested only and need no separate visor run.

This boundary is implemented/host-tested for startup's early return, full and
transport checks, canonical rejection, partial runs, absent verification, stale
inputs and ZIP/hash integrity. An isolated real `finish` fixture verifies that a
complete collected run and successful original restoration still retain/report
the failed verifier. A fault after proxy removal retains the journal; a
restoration-only `finish` retry preserves the original verdict and ZIP hashes.
Result-publication failure also cannot prevent original restoration. Missing
required gestures remain canonical rejection, even
if the operator separately records them as unavailable. Optional run-local
`operator-observations.json` is separately inventoried and cannot change telemetry
outcome. `headsetAcceptance=not_evaluated` is mandatory: neither `incomplete=false`,
matching hashes nor a dashboard-cycle summary accepts recovery or a visual gate.
The canonical rejection path is now live-tested: an inventoried, complete
native run retains its rejected full-profile verdict through collection and
original restoration. The missing loading-screen Sense continuation observation
remains a telemetry rejection; separately accepted HUD and Ray controls do not
rewrite it. Successful full-profile workflow coverage remains separate. This
diagnostic boundary does not evaluate headset acceptance.

After `finish`, inspect collected reload evidence without modifying it:

```powershell
python tools/analyze_coj_manual_reload.py "<collected run directory>" --output "work/manual-reload-analysis.json"
```

Use an output outside the evidence directory. The analyzer verifies inventoried
hashes, deployment coverage and ordered native markers; it separates correlated
unit readbacks from rejection/inconclusive observations. It never accepts a
physical gate or replaces the canonical verifier.
The same report includes read-only rifle support observations,
`winchester_reload_observations` (bounded exact-class native hand/animation/owner
and reserve/readback witness with clock and identity validation), and bow diagnostics.
Winchester `unavailable` rows delimit observation loss. `window_end` rows record
exhaustion, expiry, idle limit or restart, and bind the final index/clock/owner
to the preceding sample. Bounded fire/Square/native reload windows restart at
index one; discovery continues at 350 ms even after the 220-sample emission cap.
The report never treats `GetAmmoCount(0)` as verified
clip occupancy, observed reserve changes as native transfer proof, optional
support poses as a loading port, or sparse animation clocks as a validated phase.
`bow_draw_observations` separates reported shot requests, native attack-counter
maximum, gesture/dispatch cancellation reasons and external reset reasons. Missing
bow events remain unobserved. An attack-counter baseline is not an arrow count;
confirmed spawns and headset acceptance remain unevaluated. Older logs without
the newer optional counter fields remain readable. No extra visor run is needed
to analyze already collected evidence.

Version durable conclusions and acceptance limits, not run chronology. Keep only
compact unresolved inputs and necessary local provenance under ignored `work/`.
Once consumed, remove duplicate videos/frames/logs/dumps, frozen game archives,
obsolete builds and SDK extraction/download caches. Keep configured dependency
roots, the pinned offline bootstrap-test archive and useful current build output;
these caches are not shipped. Never remove
active recovery data or user assets as evidence cleanup.
