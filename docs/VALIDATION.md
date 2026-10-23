# Validation

States: `planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`.

Acceptance is scoped to exercised Steam 2006 D3D9/OpenVR and PS VR2 Sense contexts. Builds and synthetic tests do not promote physical gates. Published `v0.1.0-prealpha.5` and archived verdicts remain unchanged. Current post-.5 changes are host-tested; no new headset test has been declared. The accepted Manual 2804x2860 reference does not validate these changes. Candidate particulars remain under ignored `work/`.

## Public prerelease policy

No whole-campaign or other-headset support is established. Community projection/startup/gameplay feedback is scoped observation. Pico/Virtual Desktop stutters and reported crashes remain unresolved; diagnostic opt-in corrections establish no measured pacing gain. Real OneDrive synchronization, general high-address compatibility, device loss and allocation pressure remain unqualified. Preserve exact-build rejection and transactional restoration. Recommend save backups; users start and close the game and SteamVR manually.

## Current feature state

| Boundary | Confirmed state | Remaining limits |
| --- | --- | --- |
| Native D3D9 stereo and head tracking | Headset-validated native per-eye rendering, 6DOF, recenter and room-scale movement in exercised scenes. Repeated Manual 2804x2860 clarity/stereo and no perceptible performance problems are operator-accepted on PS VR2, with prior actual raster/capture/output evidence. | Broad loading/device transitions, quantified sustained pacing and other headsets. |
| Cinematic stereo recovery | Headset-validated by declared operator physical evidence: recovery after Hope, multiple exercised cinematics, entry/exit, hot theater/immersive switching including mid-sequence, different weapons, headset recenter and continued gameplay. The operator reconfirms these recovery checks work and are complete in exercised play. | Secret-scroll interaction was not tested and retains its own campaign gate. Subtitle timing, broader comfort and extreme device/tracking loss remain separate limits. The new immersive skip-input defect does not reopen stereo recovery. No repeat of accepted recovery is pending. |
| Locomotion | Exercised native analog movement, jump parity, physical crouch and earlier snap turning accepted. The operator confirms careful-movement corrections, default smooth turn and continuous HMD body follow without the former cone in the exercised PS VR2 setup. Native option usability is accepted where exercised. | The drift report concerns the older published package; the current continuous-follow correction is accepted in scope. Other headsets/controllers, prolonged comfort and mounted/campaign contexts remain open. Physical-walk/turn leg animation is planned. Specific mode/held-input recovery is not separately qualified by selector usability. |
| Body and hands | Independent tracked native hands/weapons with original torso/legs accepted in exercised contexts; head and connected arms hidden. Corrected free-left-hand fingers with a normally held long gun have scoped operator acceptance. | Firing/alternate/reload retain native fingers. Broader finger, weapon and recovery coverage remains open. Connected-arm IK is diagnostic. |
| Weapons and whip | Right-hand pistol firing, muzzle/impact effects, water and destructible bottles accepted. Exercised whip use and approximate reticle alignment accepted. The combined candidate's dual reticles and Clyde draw/raise with tracked aim/fire have operator confirmation. | Broader weapons/duels, exact ballistic agreement, whip reach/climbing and special optics. Long-gun manual reload is unimplemented; native automatic reload/animation remain. |
| Indexed reload groundwork (post-.5 source) | Implemented/host-tested for exact Remington/SawOff/Derringer/Lemant two-index observation, native object/owner/hand stability, typed reserves counted once, JNI failure/null/cleanup and LAA-safe identities. A separate pure classifier checks a retained synchronous pair. Inventoried finished-evidence analysis now retains raw indexed observations and rejects malformed claims, invalid counts/pools and repeated numeric-owner clocks; sparse samples cannot qualify a transfer. | Native indexed phase/HUD correlation, selective shell transaction, mechanism/port and physical gesture integration remain pending. No new weapon enters manual reload; no ammo/animation/sound writer is added. |
| Physical bow | Exact owner/root ray, physical right-hand R2 grab/draw/release, native supervision, controller haptics and capture-drop separation are host-tested. Operator confirms the bow worked very well in the exercised session, extending the earlier improvement to scoped functional acceptance. | Mesh/string/arrow/reticle agreement, haptic quality, concentration optics and interruption recovery are not separately accepted. This does not qualify all scenes or sustained campaign support. |
| Physical duel posture | Correct hand-at-hip draw/raise and rejection of exercised incorrect/cheating positions have operator acceptance: actions were permitted only in the correct position. | Native early-draw timing, retry/recenter and ownership recovery remain separate where not exercised. Measured holster contact is not implemented. |
| Basic virtual holsters | Operator accepts exercised equip/stow gestures and the colored assistance after the zone-press consumption correction. Enabling/disabling the assistance works correctly in the exercised native options flow. | Earlier occasional kicks remain rejected history; broader conflicting/held-input and interruption recovery are unqualified. No attached weapon mesh or measured native socket contact; disabled during duels and mounted play. |
| Ammunition pickup assistance (post-.5 source) | Implemented/host-tested under the same default-off weapon-guide preference. Uses the real manual gesture source/threshold, fresh release, supervised native waiting and positive reserve; distinct cartridge icon replaces holster rings during the manual session. Real D3D11 host fixtures cover distinct binocular pixels, current-owner admission, disabled preference, invalid geometry and resource reset. | No live/headset acceptance of marker placement, readability or color semantics. Adds no physical pouch, ammunition permission, native transfer or manual rifle loading. Include it with the existing revolver contact/recovery check, without a dedicated run. |
| Binding-aware hints and reload coaching (post-.5 source) | Implemented/host-tested for bounded Unicode, semantic fallback, origin/cache invalidation, copied HUD translation, coaching and queued-text admission. Headless D3D11 tests also retire repeated-frame text after rebinding/owner loss/disabled guides/expiry, preserving subtitles and overlapping HUD. They cover reverse restoration, device/context/target mismatch, fresh-frame discard and BGRA/BGRX/RGBA UNORM/SRGB targets. Optional instructions follow actual support-hand/session state and yield to native hint/interaction text. | No live origin/rebinding, non-Sense presentation, resource-pressure, readability or contextual coaching acceptance. No arbitrary PC rebind/chord inference or graphical glyph replacement. Include one naturally encountered hint and pickup/carry/release sequence with assistance on/off in the existing combined session; preserve subtitles, current owner, expiry and release barriers. No extra run is required. |
| Two-hand rifles | **Remington bimanual grip is headset-validated in the exercised PS VR2 context:** the operator confirms it now works after the mesh-contact correction. This closes the reported remote-contact/acquisition defect for that model. Exact Winchester/Remington/SawOff transactions also have host coverage for paired hands, one weapon commit, muzzle/ray verification, replacement rejection, restoration and input/hemisphere guards. | Winchester and SawOff placement/grasp remain physically unqualified. Detailed finger fit, sustained fire/cycle stability and adversarial interruption recovery are not separately described by this confirmation. Scope variants and physical manual long-gun reload remain unimplemented; Remington still uses native Square reload. |
| Latest rifle/reload acceptance | The operator confirms the corrected Remington bimanual grip works and explicitly leaves reload pending. Earlier scoped free-L2 suppression acceptance remains. | Pending reload means physical manual loading, not a reported failure of the existing native automatic path. No manual reload acceptance is added. Earlier unsupported Derringer/no admitted pistol-session evidence does not identify every pistol tried; whole-clip, two-slot and dual-equipped exclusions and earlier admitted-revolver acceptance remain unchanged. |
| Mirrored rifle trigger routing (post-.5 source) | Implemented/host-tested: a sole exact Winchester/Remington/SawOff in either native hand consumes the empty hand's trigger. Both release claims are independent; unavailable input, repeated poses and changed equipment cannot release a consumed hold. JNI fault and post-classification owner-replacement fixtures reject stale verdicts. After failed parent/child restoration, verified cleanup permits ordinary-fire recovery through available release while tracked writes remain quarantined. | Earlier Remington free-L2 acceptance does not qualify the new free-R2 route with a left-owned SawOff. Confirm that case only when naturally available; no new bimanual or manual-reload whitelist is admitted. Unknown ownership withholds both ordinary triggers until available release. |
| Physical melee | The current knuckle-contact route is headset-validated in an exercised fight: the operator reports defeating three enemies with Hands, and says the combat felt much more responsive. | Scoped to that fight and configuration; left/right parity, broader enemy/campaign contexts, and tracking/menu/recenter recovery remain unqualified. |
| Menus and Sense controls | Automatic hand pointer, same-hand trigger selection, Cross/Circle, pause, confirmation dialogs and mouse coexistence accepted. Wheel selection/haptics, on-foot crouch, ordinary Focus and Create tap/hold accepted where exercised. Ray's Bible selection/use and on-foot kick have scoped headset acceptance. | Dashboard/loading/held-input recovery, mounted routing, campaign permission changes and unavailable mechanics. |
| Native VR options | Operator reports native options work well, including enabling/disabling the holster assistance in exercised play. Shared preferences and cinematic/turning selectors retain scoped physical usability acceptance; version-3 migration has host coverage. | Unreported restart persistence, OK/Cancel/Defaults variants and held-input/recovery paths remain open. Ordinary selector operation, including the exercised holster assistance toggle, is not pending again. |
| Secret-scroll chests | Conditional preservation of the same still-active native render level is implemented/host-tested for the reported null-level crash. | No operator scroll pickup in the latest sessions. Next naturally encountered scroll must establish pickup, UI closure and continued control/stereo; the original callback cause is not fully observed. |
| Mounted play and mod-added DLC | Horse mounting/dismounting worked in the exercised DLC context. Post-.5 source separates mounted axes from HMD gaze and suspends automatic HMD rider-yaw commits with mount/generation guards. Further host regressions reproduce/fix retained tiny reins after centering and skipped-axis cache drift during forward updates; zero transitions and last-delivered axis values are preserved. | Community feedback reports zigzag when trying to ride straight. Static analysis confirms gaze could introduce lateral reins; physical correction, free-look/aim continuity, gallop, horse HUD and general DLC compatibility remain unqualified. DLC adaptation is deferred by operator priority. |
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

<a id="current-physical-gate"></a>
## Pending headset checks

The later manual-reload source batch host-tests empty/invalid native reserve
withdrawal of cartridge offers/carries and retained trigger-release barriers.
An optional brief EN/ES zero-reserve close hint uses the reported reload binding
with the existing guide, owner, expiry and native-text priority guards; its
physical readability remains unqualified.
It also injects player/generation changes and post-success JNI exceptions in
native heartbeat/insertion helpers; no stale Boolean success is credited.
Native ammunition, close callbacks and profile admission stay authoritative.
Physical presentation/recovery remains unqualified for these corrections; when
reserve naturally runs out, check that no extra cartridge or shot is offered.

The mounted offline audit also host-tests neutral recovery after a partial
native analog exception and rejects digital-only acknowledgement of pending
movement recovery. A direct player replacement regression compares actual aim
and pelvis mapping with the camera reference across mounted/unknown frames,
preserving the same-owner frozen reference. These failure fixtures establish
host behavior only; native exception recovery and replacement presentation in
the game remain unqualified.

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
| Interaction/HUD or a movie | Left-Sense L1 reference independent of gaze; revised wrist/concentration indicators. For unreported movie modes, broaden subtitle timing/clearance/settings recovery when naturally available; appearance in exercised animations is accepted. |
| Horse, when naturally available | After the mounted heading correction: hold straight forward while looking left/right, then deliberately steer both ways and center the stick; require no retained steering after centering, including while changing forward input. Request gallop; check rider free look/aim and paired horse meters. Dismount and confirm ordinary HMD body following resumes without a stale turn or camera/hand jump; include one mounted recenter if comfortable. Mount/dismount acceptance is retained; these corrections and broader mounted recovery remain unqualified. Broader DLC testing is deferred. |
| An untested Winchester or SawOff during ordinary play | Remington's exercised corrected grasp is accepted; do not repeat it solely for confirmation. For an available untested model, freshly hold L1 near the mesh fore-end, aim/roll and release. Check hand/mesh/reticle agreement. Sustained fire/cycle, crossed-hand cancellation, Square cancellation and pause/tracking/weapon recovery remain separate, naturally grouped checks where not reported. Native automatic reload remains; no physical loading is expected. Scoped Winchester is not admitted; skip absent weapons rather than adding sessions. |

Already accepted in scope: continuous body following, ordinary native options
and menus, subtitles appearing in exercised animations, the correct duel gesture, prior native unit reload and cartridge
visibility, physical Hands combat, exercised cinematic recovery/hot switching,
and the established PS VR2 resolution. The bow's latest improvement and horse
mount/dismount are recorded with their narrower limits above. None of these
needs a repeat merely because a newer candidate is prepared. Canonical telemetry
requirements and their recorded pass/fail/inconclusive verdicts remain separate
from operator acceptance; a skipped gesture does not satisfy a verifier gate.

## Host verification boundary

Host fixtures cover runtime policy, exact adapter rejection/restoration, archive patches and evidence tooling. Their results establish implementation behavior under the tested conditions, not physical acceptance, performance or campaign coverage. Preserve the existing scoped operator declarations separately; current post-.5 changes still await their physical checks.

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
not a request to repeat the accepted settings/cinematic baseline:

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
guides remain planned. The exercised guide option and assistance toggle retain scoped acceptance; broader comfort and recovery remain open.

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


<a id="hybrid-motion-reload-acceptance"></a>
<a id="cartridge-paced-reload-acceptance"></a>
<a id="bounded-native-waitzero-transfer-probe"></a>
<a id="revised-sense-control-acceptance"></a>
<a id="automatic-menu-pointer-acceptance"></a>
<a id="combined-menu-and-gameplay-regression"></a>
<a id="closed-locomotion-diagnosis"></a>
<a id="pc-mechanics-and-gameplay-hud-completeness"></a>
## Related acceptance boundaries

Legacy section anchors above are retained for existing links. Current acceptance is in the state table and pending checklist. Native controls and exact reload/weapon facts belong in focused research. Native permissions, original animation/ballistics and supervised inventory remain authoritative. Hybrid/cartridge-paced reload and the bounded zero-transfer wait probe retain their limited legacy evidence; none establishes physical chamber loading. Full graphical HUD capture, physical-walk animation, precise loading sockets, chamber occupancy and ejection remain unimplemented or unqualified.

For ownership changes, combine menu/gameplay/wheel/pause/dashboard with available tracking, save/load and death/retry. Hold then release controls before fresh actions; require no queued shot, click, transfer or haptic and coherent stereo. Eligible resize recovery is separate. Host fault injection cannot qualify physical recovery. Mounted corrections remain host-tested; mount/dismount acceptance does not qualify reins, gallop, free look or mounted HUD.

## Independent rendering isolation

Independent eye resolution remains rejected and default-off. The private-bank
attempt failed native queue admission before eye mutation/capture and did not
establish usable gameplay fallback; monitor-only pause is not accepted VR UI.
Do not bypass guards or advance B1 until same-size B0 isolation, usable controls
and menus, restoration and native ownership/retirement are qualified.
Read-only live binding/native descriptor correlation and synthetic GPU rollback
tests establish structure only, not allocation/lifetime, shader constants,
alpha/coverage, shadow contents or submitted OpenVR output retirement.
New sizes require fresh per-eye draw/capture/pose evidence, real additional detail
at unchanged optics, bounded memory/pacing and functional continuity. The accepted
ordinary native 2804x2860 route is separate. Reject stale/foreign generations,
failed restoration, uncontrolled growth or missing effective-size evidence.
