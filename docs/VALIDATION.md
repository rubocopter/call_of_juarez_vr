# Validation

Validation states are intentionally distinct:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

Builds, CTest, synthetic fixtures and static inspection do not promote physical gates. Acceptance below is limited to the exercised original Steam Call of Juarez (2006), PS VR2 and SteamVR contexts; it is not whole-campaign or public-release support.

## Current feature state

| Boundary | Actual state and limits |
| --- | --- |
| Native D3D9 stereo and tracked head | Headset-validated in exercised scenes: native per-eye rendering, 6DOF, recenter and room-scale movement. Broader transitions and device recovery remain open. |
| Locomotion | Native analog shaping, head-relative movement, jump parity, physical crouch and corrected 45-degree snap direction accepted in exercised contexts. |
| Body and hands | Independent tracked native hands with original torso/legs are active. Connected arms/full-body IK are not the accepted presentation. Skeletal fingers have partial live evidence; authored rest/fist retargeting is host-tested. |
| Weapons | Exercised right-hand pistol firing, visible muzzle/impact effects, water and destructible bottles accepted. Left-hand, dual and special weapons need separate coverage. |
| Menus | Automatic controller pointer, same-hand trigger selection, Cross accept, Circle back and pause accepted in exercised menus, including confirmation and mouse coexistence. |
| Sense controls | Direct controls, equipment wheel confirmation, snap rearm, haptics, on-foot crouch, ordinary Focus and Create tap/hold accepted in exercised available contexts. Horse unavailable; kick inconclusive. |
| Gameplay HUD | Encountered health/ammo, wrist compass and subtitles accepted. Full graphical native HUD capture remains planned. |
| Death presentation | Corrected failure screen visibility accepted by operator. Retry/back and held-control recovery remain separate checks. |
| Interaction | Pickup/carry/put-down and drawers accepted through the previous HMD reference. Permanent cyan ring rejected for comfort. Latest left-controller L1 route is host-tested, awaiting headset acceptance. |
| Legacy cartridge-paced reload | Hybrid gesture starts native Peacemaker reload in exercised context. Corrected continuation is live-tested on a right-hand Peacemaker: repeated replenishment and HUD round/reserve changes observed. Its native animation still takes the hands; this legacy route is distinct from the opt-in persistent manual session below. |
| Passive reload observation | Default-off legacy trace is host-tested for faults/filtering and live-tested on one right-hand Peacemaker. It observes native time, unit round/reserve changes, full-gun no-op and Square's repeated loading. The later active wait/manual candidates have separate callback-level evidence below. |
| Native reload wait probe | Host-tested and bounded live-tested on a right-hand Peacemaker: 13 zero-transfer cycles, nine deadline exits and four early exits with attack desired; native reload/discharge recover. Gate/drum hold, tracked presentation during waiting and save/load remain unresolved; no persistent manual-load or headset acceptance. |
| Persistent manual reload candidate | Scoped **headset-validated** by operator report: Square opens/closes without automatic loading, both hands remain tracked, sustained preparation allows successive manual rounds, insertion haptics occur only on loading, and removing the weapon or entering/returning from menus recovers correctly. Correlated right-hand Peacemaker telemetry confirms native unit transfers, waiting and recovery. Exact loading-port clearance, chamber insertion, explicit tracking/save/load/death/disable recovery, held-control edge cases and mirrored/Frontier contexts remain pending; this is not whole-feature support. |
| Cartridge pinch presentation | The first implementation was **physically rejected** for native loading-pose takeover; its requested overlay was unavailable. The correction is **host-tested with bounded live delivery evidence**: a same-owner recovery exception admits only the free hand, waiting uses resting/current fingers and a held physical cartridge claim requests the pinch. Correlated events now report the overlay applied. Bone-anchor identity, release, tracking and ownership guards have host tests. Actual finger contact/normal-pose recovery remain unaccepted. |
| Imported CC0 cartridge visual | Both the old proxy and full .44 OBJ with flat swatches were **physically rejected** for appearance. The current textured .357 correction is **implemented and host-tested**: one 83-position/92-triangle source node, UVs/normals and cropped albedo with authored brass/metal shading. WARP renders front/side/rear pixels in both eyes, checks lead/brass materials and rejects malformed UVs/normals; source/bake identities are checked. The centre follows verified distal thumb/index joints; native ammunition and grip insertion stay unchanged. Scene depth/native environment lighting are unavailable. Appearance, contact, scale and comfort need a fresh physical test. |
| Protected-target warning | Original red warning visible on monitor, absent in visor. Red aiming-cross alternative implemented and host-tested; visor acceptance pending. |
| Whip | Exercised controls and approximate reticle alignment accepted. Broader reach, attachment, climbing and release remain open. |
| Loading/save | Exercised narrated transition after reversible LAA correction and save/load with optional thumbnail workaround accepted. Broad campaign loading and allocation pressure remain unresolved. |
| Transport | Primary D3D9Ex GPU sharing and bounded normal shutdown accepted. Sustained pacing, reset and abnormal shutdown require broader coverage. Steam recording-associated microskips remain unresolved. |
| Other platforms | OpenXR experimental. D3D10 and other games planned; neither is the active support target. |

## Current physical gate

Left-hand L1 selection still awaits operator acceptance. The opt-in persistent
manual reload candidate now has scoped operator acceptance for its basic flow,
independent hands, acceptance haptics and weapon/menu interruption recovery.
Remaining mechanical and interruption gates are listed below; do not require
repetition of accepted behavior unless a later change affects it. The earlier
cartridge-paced path remains a separate fallback.

- Aim the left Sense at a drawer's usable part and press L1. Turning the head must not retarget interaction. The cyan reference appears only while L1 is held. Check opening, pickup, carry and put-down; losing hand tracking/publication must require an available release before another action.
- With `-ManualReload`, finish the remaining [persistent manual reload gates](#persistent-manual-reload-candidate): exact loading-port clearance, tracking/save/load/death/disable recovery, held-control behavior and mirrored/Frontier contexts. Basic Square preparation/closure, repeated manual loading, tracked hands, acceptance haptics and weapon/menu recovery are accepted in the exercised context.
- During the next reload session, first open and wait without pickup: the free hand must not retain a cartridge pinch. Pick up with its trigger and inspect the imported cartridge between thumb/index, including roll and both hand assignments when available. Release away, insert, close and pause: the visual round/pinch must clear appropriately. `manual_reload_pinch` reports overlay ownership transitions; `manual_reload_cartridge` reports visible/hidden, mesh and verified joint anchor. Neither event establishes physical finger contact.
- After death/failure, Back/retry must restore native gameplay cleanly. Held controls must remain suppressed until release; underfloor tracked presentation or continued dead-player interaction rejects recovery.
- Check the red aiming cross on a native protected target. This is a low-priority visor check, independent of ordinary aiming acceptance.

## Persistent manual reload candidate

This is one combined acceptance session, not a repetition of the observation-only
or bounded wait probes. Prepare with `-BodyIkAtStart -LargeAddressAware
-ManualReload`, start SteamVR/game manually, close normally and run `finish`.
The derivative is **host-tested, live-tested and scoped headset-validated**.
The operator explicitly confirms opening/closing with Square, no automatic
reload, both hands following the controllers, keeping the weapon open while
collecting/inserting successive cartridges, haptics only on insertion, and
correct recovery after removing the weapon while open and entering/returning
from menus. This accepts the basic manual flow and those exercised interruptions.
It does not establish an individual chamber socket, exact visible port clearance,
independent mechanical control, prolonged comfort, or unreported recovery cases.

The corresponding complete inventoried right-hand Peacemaker capture adds four
accepted insertions, all with coherent `loaded +1 / reserve -1` native readbacks,
four sampled waiting entries and no offline-analyzer issues. Native full-gun
closing returns to ready; input-conflict and native-gameplay-owner cancellations
also recover. Three optional acceptance pulses have successful left-hand output
submission records; qualification does not guarantee output for every capture.
The physical haptic acceptance comes from the operator report, not those records.
Raw identity, telemetry and the report correlation remain under ignored `work/`.

Earlier bounded evidence remains useful for unexercised gates. A prior
correlated capture contains 16 preparations returning to ready,
13 observed waits, 11 Square closures and five loading sessions with 4/4/6/6/1
accepted rounds. All 21 insertion readbacks are exactly loaded +1/reserve -1;
waiting lasts up to 8.464 native-clock units under supervision. No unexplained
loading during waiting/closing or failed sampled presentation/restoration guard
is reported. The clip shows separate hands, cartridge pickup/travel and firing
after loading, including HUD 0/36 followed by 4/32. This accepts observation of
that bounded behavior, not the whole physical gate. No active-load pause,
tracking loss, armed-trigger close, save/load, death/retry or replacement is
established by this capture. The exit menu occurs after reload has closed.

A later complete, inventoried physical run adds **12/12 correlated native
unit transfers** (`loaded +1 / reserve -1`), ten sampled native waiting entries,
and no offline-analyzer issues. The session trace also records Square closure,
armed-trigger `fire_close`, native full-gun closure and return to ready. There
are four logged `manual_admission_rejected` transitions, which returned to ready;
they are distinct from cartridge insertion rejection. The operator reports that
the tested flow appears to work. This is bounded live evidence and general
operator feedback; it does not establish each interruption, audible feedback,
comfort or independent mechanical opening gate.

The preceding capture's 41 same-frame mechanical samples include waiting and
closing, but predate source-qualified root/barrel observation. They compare gate
against drum, whose own rotation changes relative axes; they cannot isolate
gate opening or establish port clearance. A newer **host-tested and bounded
live-tested** optional sampler captures root, native barrel element, gate and
drum from the same weapon's `pre_native` frames before tracked overlay writes.
Its `post_overlay` observations verify the mapped root/barrel and named element
readbacks through JNI. The offline analyzer treats only coherent same-frame
`pre_native` samples as eligible evidence of native mechanical motion and
rejects non-rigid reference/part frames. Neither source alone confirms an
open gate or a usable loading port. Its first live capture contains 26 coherent
same-frame `pre_native` samples, including 19 in native waiting: the gate's
orientation relative to the root stays effectively constant in those waiting
samples (up cosine approximately 0.642787-0.642788), while the drum's relative
orientation changes. The gate is **not demonstrated open**; opening may precede
these sparse waiting samples, and the visual port remains unverified. A focused
physical observation must establish actual clearance before enabling a
mechanical writer or loading-port interaction. Use one eligible
Peacemaker/Frontier, empty support hand and some missing rounds/reserve.

A subsequent complete inventoried run validates the **natural `READY` reference
observer in runtime**. Twelve eligible `READY` references and 24 later coherent
same-owner, same-model `pre_native` comparisons were recorded; 13/13 accepted
insertions have native unit readbacks (`loaded +1 / reserve -1`), seven waiting
entries were sampled, and the offline analyzer reports no issues. For the
sampled Peacemaker, gate-to-root up-axis cosine changes from approximately
1.000 in natural `READY` to 0.642788 during manual waiting, while the gate's
relative position shifts by roughly (-0.107, -0.145, -0.298) cm. This is
evidence of a different native gate pose, with the drum also moving separately.
An additional host-tested, full-frame offline calculation on those same 24
comparisons measures approximately 50 degrees of gate rotation relative to
root and barrel, 0.346–0.356 cm of local gate translation, and 30–150 degrees
of independent drum rotation with under 0.007 cm of displacement. This is
derived from the existing recorded runtime vectors, not another physical run.
These measured rotations do not establish the hinge axis, visible
clearance, an insertable socket or safe native pose control. The available
video does not resolve the loading port well enough to accept those gates.
The observer/analyzer are host-tested; the geometry difference is bounded
**live-tested**, while mechanical clearance and full headset acceptance remain
pending. Raw run metrics, trace and video remain in ignored local evidence.

The optional displayed `post_overlay` chamber-mouth **ranking** is
**implemented and host-tested only**. After native identity checks it compares
six model-specific mouth centres to the gate/loader element origin in cm; that
origin may be a pivot rather than the physical opening. The associated offline
analyzer reports `port_probe` and `port_pivot_rank_samples` only for coherent
optional diagnostics, rejects malformed data, and always retains
`gate_open_confirmed=false` and `socket_admission=false`. No new headset run
has validated this ranking, visible clearance, or an accurate insertion socket.
The headset-accepted free-hand **trigger** gesture and the existing 14 cm
weapon-grip proximity zone remain unchanged; this diagnostic cannot yet select
an individual chamber or physical loading-port socket.

- For any further focused mechanical test, show the actual loading port clearly
  in natural `READY` and sustained `MANUAL_LOAD`, preferably at a similar viewing
  angle. The reference observer and paired native geometry are already sampled
  in runtime; inspect whether the visible port is accessible, since the recorded
  gate orientation delta alone cannot prove clearance.
- Press/release Square once. Preparation must not load automatically, and waiting
  longer than the old 1.5-native-unit probe interval must not close while coherent
  supervision continues. Observe whether the gate is actually open. Move both
  controllers independently: weapon and hands must follow, connected arms hidden.
- At waist level hold the **free-hand trigger**, carry the placeholder near the
  gun's grip and release. Require exactly `loaded +1 / reserve -1` for each accepted
  fresh release, no repeated native reload cycle and no delayed transfer. Repeat
  twice and, when practical, to full. Accepted insertion replenishes the token;
  a missed release adds nothing and can be retried. It uses the existing 14 cm
  grip zone, not an accepted individual chamber or loading-gate socket.
- Close early with fresh Square, reopen after release, and close once with the
  armed trigger. That closing press must not fire; release and a new press must
  recover shooting. Full/no reserve must close without an extra round.
- In the same session exercise pause/resume and available weapon change, save/load
  and death/retry. Briefly lose/recover controller tracking and, if practical,
  disable while loading. No stale cartridge, held-button replay, extra transfer
  or permanent reload/input lock may survive. Unavailable cases remain pending.

The candidate now also implements **host-tested optional acceptance haptics**.
A completed, accepted native call with coherent loaded +1/reserve -1 readbacks
qualifies an 18 ms, amplitude 0.25, 120 Hz pulse in the free hand. Delivery requires
a fresh accepted stereo capture and current tracking/gameplay/native ownership;
repeated/stale captures, reacquisition and output faults cannot replay it. A
separate game-owned epoch binds the latest observed player/weapon/armed hand, so
native replacement invalidates queued feedback without a Sense intent. Sampling
cannot establish that an unobserved transition never occurred. This change
was absent in the earlier captures, but now has **live-tested delivery and scoped
headset acceptance**: the operator reports a cue only when a cartridge loads.
Explicit missed-insertion tests, prolonged comfort and haptic replay checks across
tracking/resource recovery remain pending; successful API submission alone does
not validate them. Capture loss may silently drop optional feedback. Zone-entry
haptics and a new insertion sound remain unimplemented.

`reloadTraceEnabled` controls event diagnostics: `manual_reload_session`,
`manual_reload_insertion`, `manual_reload_presentation`, `reload_trace` and mapped
`manual_reload_geometry`. Compare native ammunition/reserve only with the same
player/weapon/hand/context and valid observations. Applied scoped mapping is
separate from the per-eye draw guards and operator visual acceptance. The trace
observes state/animation IDs and native time/advance, not every rendered ANM frame
or actual sound playback. Geometry events distinguish `source=pre_native` from
`source=post_overlay`; legacy events without source remain inconclusive for
independent gate motion. `reloadTraceEnabled` also controls the optional
`openvr_reload_haptic` submission/drop event. A submitted pulse records an API
result, not a physically observed vibration. No new insertion sound is claimed.

Host coverage includes opening/waiting/closing/ready, repeated inserts, full and
empty reserve, drop/rejection, cancel/reopen, owner/context/input-generation loss,
menu/save/load intents, death/network/carried/replaced native owners, unavailable
or stale tracking/input, held controls and disable. Native helper tests execute
emitted bytecode with native scheduling/ammunition stand-ins; JNI faults and
reference cleanup are tested independently. Original save/load prefix order is
checked, not an engine save roundtrip. Deployment/restoration fixture checks do
not validate physical interactions.

## Hybrid motion reload acceptance

The hybrid gesture is **headset-validated for the exercised Peacemaker context**
by operator report: the corrected gesture successfully starts the original
reload. Mirrored hands, cancellation, reserve limits and the other exact pistol
classes remain separate gates. This report does not validate cartridge-paced
loading or physical cylinder manipulation.
With a recognized Frontier 1878 Regular, Schofield A/B or Peacemaker pistol in one hand,
the other genuinely empty, on foot and in ordinary firing mode:

- Fire one round and wait for native shot recovery; retain ammunition in reserve.
  An entirely empty gun may already auto-reload through the native game path.
- Bring the empty hand to its side of the waist with its trigger released.
  Press and hold that hand's trigger, bring it near the armed hand, then release.
  The initial zone is approximately 55 cm below the HMD, 20 cm to the empty-hand
  side and 10 cm forward, with 20 cm radius. Release within 14 cm of the armed
  grip after at least 20 cm of hand travel and 100 ms; finish within five seconds.
  These are gesture UX defaults, not native physics or a measured body fit.
- Verify one native reload, actual ammunition/reserve changes, original reload
  animation ownership and recovery of tracked hands. No trigger shot, duplicated
  reload or virtual cartridge insertion is expected from the empty hand.
- Release away from the weapon to cancel. Repeat interruption by pause (including
  keyboard Escape), dashboard, wheel, recenter, tracking loss, switching/equipping
  and Action. No interrupted gesture may replay after recovery or reload a newly
  selected weapon. A trigger that acquires a weapon while still held must remain
  suppressed until an actual available release.
- Test mirrored hands where the native inventory permits, Square fallback,
  full/empty reserve and native automatic reload. Unsupported weapons, dual guns,
  alternate firing states, carried objects and mounted/unknown owners must retain
  their original controls without starting the gesture.

Assess zone reach/readability, accidental activation, control coexistence and
frame pacing. The hybrid intentionally leaves transfer timing to the game;
full manual per-round/cylinder interaction remains planned. See
[reload ownership](research/COJ_RELOAD_OWNERSHIP.md).

## Cartridge-paced reload acceptance

The earlier no-replenishment failure has bounded positive live evidence after the
carrying/animation distinction: a right-hand Peacemaker capture shows repeated
tokens and consecutive HUD changes from 3/43 through 4/42 and 5/41 to 6/40
(gun/pistol reserve). Correlated gesture/recovery logs reach replenished-ready
after repeated native cycles. The original animation still moves the hands;
this is cartridge-paced native reload, not a persistent manual-load session.
Comfort and the full interruption/mirrored/Frontier gate remain pending; video
evidence is not a new operator acceptance of those boundaries.
With a single Peacemaker or Frontier 1878 Regular, empty support hand, on foot,
ordinary state and ammunition in reserve:

- Fire at least two rounds, then perform one waist pickup and release near the
  armed grip. A small brass/copper cartridge marker should follow the empty hand
  during the held journey and disappear on release or cancellation.
- Wait for the original reload begin/body/end cycle. Exactly one round should
  enter the gun, with exactly one round removed from reserve. The gun must stop
  reloading while still partially empty. If the same gun can still continue,
  another cartridge marker should reappear in the same empty support hand only
  after that native reload interval completes. A fresh trigger press claims it;
  release near the gun requests one further round. A miss keeps that cartridge
  available for another fresh press/release attempt. Holding the trigger through
  replenishment or repeatedly releasing cannot queue additional rounds.
- Release away from the gun, pause, dashboard, recenter or switch during pickup.
  No cartridge or reload request may return after the interruption. If the native
  reload was already accepted, its single-round completion remains native-owned;
  cancellation must not turn it into a full reload loop.
- Check full gun, zero reserve, weapon replacement, death, quicksave/load and
  mirrored hands separately. Multiplayer is excluded from this route. Square,
  native automatic reload and Schofield remain ordinary native reload paths.

This candidate uses the established grip proximity gesture and original reload
animation. The cartridge is a captured stereo solid with fourteen surfaces,
controller orientation, brass/copper shading and per-eye self-occlusion. It has no
scene depth occlusion; its visibility and comfort require headset testing. It does not
provide a measured chamber/loading-port socket, gate opening, spent-case ejection
or independent occupied chambers. Those model/pose boundaries and save/load
recovery remain open before full physical cylinder loading can be accepted.

The explicit insertion policy is host-tested separately: the captured cartridge
tip must approach an adapter-authored socket from behind, stay aligned, enter
while held and release inside. Discontinuous tip/socket motion cancels the claim.
The active game adapter does not enable that mode yet. Named displayed drum/gate
frame observation and six measured chamber-mouth transforms are host-tested
observations only; they do not establish gate clearance or occupied chambers.
`manual_reload_geometry` records sampled current mapped frames in centimetres
with `interaction_enabled=false`. A persistent open/load/close native session
remains unresolved. Repeating the earlier paced/trace capture cannot establish
the new wait boundary; the optional probe below exercises different behavior.

The optional `reloadTraceEnabled` observer adds transition events to the existing
runtime log without changing reload behavior. Enable/disable with
`tools/set_hmd_camera_control.ps1 -GameDirectory "C:\path\to\Call of Juarez" -Mode
reload-trace-enable` / `reload-trace-disable` after canonical preparation. Trace
toggles preserve presentation/gameplay fields. Use a fresh correlated run for any
new observation; a previously installed candidate does not contain later source
changes. The next diagnostic capture needs only:

- Compare ordinary Square and gesture-paced reload on a partially empty
  Peacemaker/Frontier. Correlate native HUD, pistol reserve and state/animation
  events, especially 20/21/22 recovery. Raw `GetAmmoCount(0)` may expose a stub;
  it must agree with observed native ammunition before interpreting count deltas.
- Check a fresh second insertion, a missed release and full/no-reserve rejection;
  intent/token disappearance must not be recorded as successful transfer.
- Interrupt by pause, weapon change, tracking loss and save/load/death/retry when
  available. Require clean recovery and no input/token replay. Record which
  transitions the render sampler misses rather than assuming callback cadence.

These checks validate observation and the existing paced path. Quiet waiting,
zero unauthorized transfer on closing and tracked hands through an open session
remain development gates; host state-sequence tests do not accept them physically.

The bounded Peacemaker trace capture now observes three paced cycles with count
3 -> 4 -> 5 -> 6 and reserve 50 -> 49 -> 48 -> 47, followed by a full-gun request
without count/reserve change or a new reload interval. A fresh Square press later
loads 2 -> 6 while reserve decreases 47 -> 43. Sampled begin/body/end states each
report 0.3 native-clock duration; intermediate Square transfers can retain state
21 while the animation start time resets. This validates the reader in that
context, not an exact ANM frame, callback order, wait or cancellation. No new
clip accompanies this capture, and interruption/tracking/other-weapon gates
remain open.

## Bounded native wait/zero-transfer probe

This explicitly selected probe is **host-tested and bounded live-tested on a
right-hand Peacemaker**. It replaces
the paced gesture's one-round completion with a bounded logical wait and a
zero-transfer close. Hands still follow the original reload presentation;
Square stays conventional. The normal prepared candidate does not include it.
No repetition of the earlier observation-only capture is needed.

The correlated capture records 13 admitted begin/body/end cycles with status
armed -> waiting -> closing -> cleared. Nine deadline exits are observed
1.501–1.524 native-clock units after entering waiting; four earlier exits have
attack state 31 desired. Loaded count and reserve remain unchanged while each
probe owns the reload, and reserve remains unchanged through cleanup. One
cleared observation already includes a native shot (3 -> 2), so whole-frame
count equality is not a valid reload-transfer assertion when firing resumes.
Fresh Square loads 0 -> 6 with reserve 50 -> 44; a later native automatic reload
also loads six. Subsequent probe requests and shots demonstrate bounded recovery.

The 46.70-second clip shows native hand/arm takeover and later recovery. It does
not resolve loading-gate clearance or a fixed drum/gate pose during the wait.
The existing `manual_reload_geometry` sampler runs after successful tracked
weapon application, which is suppressed during native reload; its ordinary-state
samples cannot prove mechanics in the held state. Pause, tracking, weapon-change,
death/retry and save/load cancellation during an owned wait remain unproven.
This evidence promotes the bounded logical probe to live-tested, not the full
manual session or headset acceptance. The finished staging is restored.

For a new test of this distinct boundary, prepare a fresh candidate with
`tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez"
-BodyIkAtStart -LargeAddressAware -ReloadWaitProbe`. The canonical process remains:
start SteamVR/game manually, exercise the gate, close normally, then `finish`.
Tracing starts enabled and the manifest identifies the experimental archive.

- On a partially empty Peacemaker with reserve, perform one waist pickup using
  the free hand's **trigger**, hold the cartridge near the gun and release. Require
  native 20 -> 21, status 2 waiting for approximately 1.5 native-clock units after
  the first attempted body exit, then 22 -> coherent ready. **Neither loaded
  count nor reserve may change.** Repeat once to establish release/rearm; a
  consumed marker is intent and must not be reported as a loaded round.
- Compare one fresh Square press: it must retain conventional native loading.
  Then interrupt a probe by pause, weapon switch or save/load when available.
  No unintended transfer or permanent reload/input lock is allowed. Paused native
  time may retain the bounded wait until resuming; immediate menu/tracking/mod
  cancellation is not implemented by this probe.
- Record whether the loading gate/drum visually remains prepared during logical
  wait and whether native closing restores it. Hand takeover is expected here;
  this capture cannot accept tracked hands throughout `MANUAL_LOAD`.

The independent partial reload/comfort and broader recovery gates above remain
separate. Unknown observations, skipped steps and absent video stay pending.

## Revised Sense control acceptance

The [default Sense table](research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls) is the single binding reference. Exercised direct buttons, Triangle wheel release confirmation, haptic highlights, snap rearm, on-foot crouch and ordinary R3 Focus have operator acceptance. Create held for 800 ms recentres; a short press opens objectives, with operator acceptance.

Remaining coverage: mounted transitions/gallop when a horse is available; kick once campaign permissions allow it; newly acquired weapons, bow/scoped contexts and alternate HUD rows. Unavailable or inconclusive actions are pending, not failed. Check that wheel/menu/dashboard ownership suppresses gameplay and recovery requires release without replay.

## Automatic menu-pointer acceptance

The pointer appears automatically in native menus without L1/R1. Use the same hand's L2/R2 to select; verify Cross, Circle, Options, confirmation dialogs and mouse coexistence. These exercised cases are accepted. Loading, dashboard and resource replacement recovery require broader physical coverage with no stale click or haptic replay.

## Combined menu and gameplay regression

After each change affecting ownership, exercise menu -> gameplay -> wheel -> pause -> dashboard -> gameplay, plus death/retry and save/load. Hold mapped controls across transitions: no delayed fire, interaction, reload, snap or utility action may escape on recovery. Native permission and exact-build admission remain authoritative.

## Production D3D9Ex startup/reset gate

### Bounded transport acceptance — passed

The exercised primary D3D9Ex GPU transport and normal shutdown passed their bounded physical gate. This does not accept arbitrary device resets, abnormal shutdown, sustained pacing or every campaign transition. The classic shared-texture environmental test can be skipped when the host cannot supply its device capability; that skip is not a pass for that boundary.

## Closed locomotion diagnosis

Native per-axis analog shaping and actions 4–7 remain the accepted boundary. Measured vanilla movement/jump parity closed the earlier locomotion diagnosis; no speculative physics retuning is pending. Camera/body writes must retain scoped restoration.

## PC mechanics and gameplay HUD completeness

Encountered HUD rows and ordinary controls are accepted only for observed content. Horse, broader weapons, climbing, duel/special modes and all campaign-specific indicators still require coverage. Full graphical HUD capture and full physical reload are future work, not hidden completed features.

## Image-quality profile acceptance

Use the repository's prepared profile for comparable headset checks. Record rendering/readability, aiming effects and motion stability separately. Steam recording off produced accepted head-turn stability; recording-associated microskips remain unresolved. Higher resolution or successful startup alone does not establish sustained performance.

## Evidence policy

Use the canonical reversible workflow:

```powershell
pwsh -File tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez" -BodyIkAtStart
# Start SteamVR and Call of Juarez manually; exercise the relevant gates.
# Close the game normally.
pwsh -File tools/vr_test.ps1 finish
```

Every physical run needs a fresh local identity and source/build/deployment correlation. Never launch or terminate the game or SteamVR automatically. Keep active staging, original backups and deployment/video recovery journals until finish completes. Host recovery checks cover backup identity, interrupted journals and finish retries; they do not establish headset acceptance.

After `finish`, collected reload evidence can be inspected without modifying it:

```powershell
python tools/analyze_coj_manual_reload.py "<collected run directory>" --output "work/manual-reload-analysis.json"
```

Use a new output path outside the evidence directory. The host-tested analyzer
checks run/build identity, inventoried hashes, deployment coverage and ordered
runtime markers. It distinguishes correlated unit readbacks, rejections,
inconclusive insertions and unexplained transfers while waiting or closing.
Insertion correlation requires observed manual waiting before the call and
observed manual waiting or closing afterward; a cleared or unobserved follow-up
does not confirm a unit transfer. Relative gate/drum
metrics do not establish an open gate. Incomplete runtime and sparse observations
remain explicit; this report never accepts a physical gate or replaces the
canonical verifier. Hash consistency is not external authentication.

Store run IDs, logs, videos, raw telemetry and handoffs under ignored `work/`. Once conclusions are durable in source/tests/research and validation, discard consumed clips, extracted frames, duplicate binaries, obsolete frozen packages and SDK extraction/download caches. Preserve necessary unresolved inputs and concise findings. Versioned documentation records contracts and acceptance, not debugging chronology.
