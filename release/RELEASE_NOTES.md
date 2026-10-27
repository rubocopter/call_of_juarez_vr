# v0.1.0-prealpha.7 - Graphical installer, experimental GOG and reload recovery

Community-testing prerelease for **Call of Juarez (2006), D3D9/OpenVR,
Windows 10/11 x64**. Steam remains the reference edition; the exact verified
GOG build is now admitted experimentally using the same VR payload. Back up
saves. This release includes all source changes since prealpha.6 and retains
its features. Publication does not establish full campaign or hardware support.

## Changes since prealpha.6

- **Graphical installer:** `CoJ-VR.cmd` opens a Western-themed Windows interface
  for install/update, preparing sessions, status, package verification, local
  reports and uninstall/recovery. Detect Steam libraries and appmanifest-defined
  folders, registered/default GOG locations, rescan, choose between multiple
  installations or Browse/type a custom path. Original executable, engine and
  archive SHA-256 identities decide admission; folder names do not.
- **Experimental GOG compatibility:** recognize the tested original and its exact
  LAA derivative, matching GOG Data0 archive, game hooks and restoration hashes.
  The operator successfully played and loaded existing Steam saves locally.
  Unknown/mixed files still reject. This is limited compatibility of one tested
  build, not parallel full support for a second edition.
- **Resolution and launcher recovery:** retain saved Auto/percentage/Manual
  profiles and manual-reload choice through updates and fresh sessions. Correct
  asynchronous GUI completion/error handling and PowerShell module inheritance.
  Source-checkout GUI actions prepare a verified local package automatically;
  incomplete player ZIPs cannot fall back to unverified files. Resolution
  preparation recognizes both exact edition-specific LAA executables.
- **Indexed reload feedback and mechanics:** integrate supervised native per-unit
  body replay and exact-profile Status1 feedback for Remington/SawOff, Derringer
  and Lemant paths. Preserve selected-slot reserve authority, native closure and
  interruption guards. These new mechanical paths have host coverage; model-
  specific physical playback, hand fit and recovery remain unqualified.
- **Held-input recovery:** keep physical trigger claims across changed input/
  player sources until a fresh available release; reject retired sources and
  stale weapon/owner observations. One final release sample cannot recreate a
  persistent claim. Extend interpreted/JNI regression and evidence analysis.
- **Winchester observation:** add measured mesh lip/gate/tip diagnostics bound to
  exact ordinary/scoped weapon identity, native reload phase and retained hand
  transaction. This gathers evidence; it does not replace the current insertion
  target or claim a precise functioning loading socket.
- **Horse peripheral effect:** replace equal-texture-UV masking with a common
  head-space angular aperture projected through each eye's optics/rotation.
  Preserve the clear center, alpha, disabled state and scope precedence. This
  corrects the observed binocular mask presentation in the exercised PS VR2
  session; native gallop speed and steering are unchanged.
- **Documentation:** consolidate the current progress, operator feedback and
  remaining physical gates without publishing local run metadata or recordings.

## Exercised results and known limits

The latest Steam operator session confirms ordinary Winchester R2 firing,
visible campfires/encountered environmental effects and the corrected horse
peripheral effect. Winchester loading remains awkward: insertion needs a very
precise point and the support hand sometimes takes an unnatural pose. Mounted
steering remains difficult and seems more responsive with R1, which also makes
the horse gallop faster. Visual mask acceptance does not establish reduced
nausea or overall mounted comfort.

Scoped Winchester reticle/actual impact agreement remains rejected; its later
head-yaw isolation correction still needs physical qualification. Other reload
families retain only their previously stated scoped acceptance; new animation,
finger contact, occlusion, sound and interruption recovery are not universally
validated. The latest canonical telemetry profile rejects the eye-baseline
comparison; collection and operator reports do not turn that verdict into PASS.

The graphical launcher and exact Steam/GOG install/update/session/uninstall and
failure-restoration paths have host integration coverage in isolated copies.
This does not qualify every Windows DPI, campaign, headset or controller. Broad
loading/device loss, sustained pacing, secret-scroll crashes, complete graphical
HUD capture, physical-walk leg animation and native runtime reload preference
remain open. DLAA/DLSS requests still leave game rendering native; OpenXR,
D3D10 and other Call of Juarez games remain outside this release.

## Install and update

Extract the complete **player ZIP**, open `CoJ-VR.cmd`, choose the intended game
folder and select **Install / update**. Start SteamVR manually first for Auto or
percentage resolution queries. Later launches use **Prepare play session**.
Use **Advanced > Manual reload Off** and Install/update for conventional reload.
Uninstall/recovery restores original game files; retain backups until successful.
Developer candidates must be restored by their own workflow before using the
public installer. [Package instructions](README.txt).

The ZIP contains no game archives, saves or developer symbols. GPL-3.0-or-later
source and notices remain linked through `SOURCE_COMMIT.txt`; optional developer
symbols are a separate download. Earlier release artifacts and notes are
unchanged. [Changes since prealpha.6](https://github.com/rubocopter/call_of_juarez_vr/compare/v0.1.0-prealpha.6...v0.1.0-prealpha.7).

# v0.1.0-prealpha.6 — Experimental firearm loading, scope and control updates

Incremental community-testing prerelease for **original Steam Call of Juarez
(2006), D3D9/OpenVR, Windows 10/11 x64**, cumulative from prealpha.5. Back up
saves. This release includes host-tested experiments alongside narrowly
exercised gameplay; publication does not establish full campaign or general
headset/controller support. PS VR2 Sense remains the physical reference.

## Manual firearm loading

- **15 exact firearm classes** now share Square preparation and the free-hand
  trigger pickup/carry/release gesture, with separate native ammunition rules.
  Native opening, closing, capacity and reserves remain authoritative. Release
  long-gun L1 support before loading; freshly regrip after closing. Unsupported
  weapons keep native handling. Launcher **8 — Manual reload on/off** restores
  conventional reload while the game is closed; fresh installs default on and
  updates preserve the installed choice.
- **Individual rounds:** retain Frontier1878_Regular, Peacemaker, Frontier,
  Walker and Cogswell profiles. Add Winchester/scoped Winchester rifle rounds
  and Volcanic pistol rounds, with a supervised native animation cycle between
  insertions. Winchester holds up to 12 rounds; Volcanic up to eight.
- **Selected chambers:** Remington/SawOff load one barrel from shared shotgun
  reserve; Derringer loads one chamber from pistol reserve. Lemant (LeMat) keeps
  its separate one-round rifle and eight-round pistol pools. Pickup retains the
  selected eligible slot until insertion/cancellation; one release cannot fill
  both slots or redirect a held cartridge. These weapons require Square
  preparation. Per-unit indexed mechanical animation is not implemented.
- **Schofield base/A/B:** one deliberate whole-clip insertion loads the missing
  portion of the six-round clip, limited by reserve, then closes. A new bundle
  repeats the imported textured cartridge to match the offered amount. It is
  provisional presentation, not a measured speedloader or exact-calibre model.
  A trigger held from empty-gun opening now requires release before it can close
  the session; a later press still closes deliberately.
- **Reload recovery:** empty/invalid reserves withdraw offers and carried rounds;
  optional coaching explains zero reserve. Tighten release requirements after
  tracking/input/ownership loss, discard stale continuation rounds across
  closure/reopening, and preserve conventional Square recovery after disabling
  manual loading. Native callback failures and changed weapons reject stale
  transfers. These corrections have host coverage; broad physical interruption
  and save/load recovery remain open.

**Acceptance boundary:** earlier single-round revolver loading is retained.
Base Schofield whole-clip loading has scoped functional operator feedback,
including offered missing-round amounts and matching native reserve debits.
Retry friction remains unresolved. That feedback predates the textured bundle
and opening-trigger correction; neither has a headset verdict. Schofield A/B
and the other newly integrated model paths have host tests only. No acceptance
extends to every revolver, the complete combined variant, precise loading mouths,
convincing finger/scene occlusion, calibre/shell visuals or ejection.

## Weapons, controls and assistance since .5

- **Scoped Winchester:** add measured two-hand support, native R2 firing and a
  revised large binocular scope/reticle. Exercised grip, discharge, scope view
  and controller aiming have scoped operator acceptance. Render-only isolation
  still left inverted horizontal head response; the subsequent correction also
  suspends head-driven body yaw while the native scope is visible, so angular
  aim follows the rifle. This new correction is host-tested only. Scope-exit
  recovery, precise shot agreement and manual reload remain separate gates.
- **Ordinary Winchester:** correct R2 requesting basic zoom instead of firing
  through the native rifle fire channel. This correction has host tests and was
  not retested in the latest headset session. Rifle reload usability remains
  physically unqualified for the new manual paths.
- **Mounted Gatling:** restore contextual R2 firing and right-stick horizontal/
  vertical traversal, with a directional reticle. Firing and X/Y progression
  work in one exercised section; precise aiming, reticle/impact agreement and
  exit/interruption recovery remain open.
- **Rifle trigger ownership:** protect either-hand single-rifle ownership from
  the empty hand's trigger, retaining consumed holds until physical release and
  rejecting changed equipment. Earlier Remington free-L2 acceptance does not
  accept every mirrored route, including a left-held SawOff/free-R2 case.
- **Pause and movie skip:** Options now pauses on a fresh press; holding it no
  longer opens a second settings panel. Native **Options > Game** is the VR
  settings editor and preserves saved preferences. Fresh Options/Circle/Cross
  or trigger selection can request the game's own skip during immersive movies,
  respecting native unskippable scenes. Changed press timing and immersive skip
  are host-tested; earlier cinematic stereo recovery remains accepted in scope.
- **Binding-aware help:** adapted floating hints can name the reported connected
  controller input, with semantic fallback for ambiguity. Optional glyphs are
  limited to recognized inputs. Native blocking tutorials still show PC keys;
  arbitrary chords/custom PC mappings are not fully described.
- **Reload assistance:** the default-off weapon-guide setting also enables
  supervised ammunition pickup markers and brief English/Spanish pickup, carry,
  insertion and empty-reserve coaching. Native hints retain priority. Floating
  reload hints were reported to flicker. Fix a reproduced case where an expired
  button icon erased a still-current instruction on repeated eye images: retain
  the semantic text until its own deadline, preserving native HUD and immediate
  ownership/rebinding invalidation. Align text expiry with the presenter clock.
  This is host-tested; physical stability/readability remain pending. Disable
  weapon guides/coaching in Options > Game if needed.
- **Interaction and HUD:** contextual L1 uses the left controller ray with a
  cyan reference only while held. Refine wrist health/concentration/horse
  indicators and protected-target reticle feedback. These latest presentations
  have host tests, not new blanket headset acceptance or complete graphical HUD
  coverage. Bow concentration optics are refined without changing the retained
  physical bow draw into a new .6 feature.
- **Horse control:** separate mounted forward/reins input and rider free look
  from HMD-directed on-foot movement/body yaw. Correct retained tiny steering
  after centering and analog recovery after partial delivery. These changes
  have host tests; straight riding, free look, gallop and broader recovery still
  need physical checks. Existing mount/dismount feedback remains narrowly scoped.

## Optional graphics and reliability

- Native **Options > Game** adds optional sharpening, diagonal-edge smoothing,
  contrast, saturation, gamma, reconstruction requests, startup scale and
  Off/Mild/Strong mounted peripheral comfort. Effects default off; spatial
  processing applies to fresh native stereo before VR overlays. Expanded layout,
  persistence and image controls have host tests only; visor quality, comfort,
  resource-loss fallback and sustained cost are unqualified. For the player
  package, launcher **7**, then **6**, remains the startup-resolution route;
  the native menu scale request does not replace that saved launcher profile.
- **DLAA/DLSS is not integrated into game rendering.** Those selections save a
  request and explicitly retain native output. Standalone GPU experiments do
  not establish game reconstruction, image quality or a performance gain. The
  rejected independent-eye-resolution prototype remains off.
- Correct the loading continuation key-down route and suppress optional pause/
  save-thumbnail capture paths while preserving native save serialization.
  Original-campaign New Game/loading and save/load have scoped positive feedback;
  other transitions, held-control cases and allocation pressure remain open.
- Correct exact-build high-address fire-definition handling. A distant campfire
  has scoped positive feedback; this does not prove the cause or resolution of
  every invisible-fire report. The retained chest/scroll mitigation still lacks
  campaign confirmation.
- Further gate successful allocation/resource and inactive body diagnostics
  behind opt-in probes. Preserve compatibility checks, restoration and failure
  diagnostics. This removes unwanted ordinary-session work; it does **not**
  establish a fix for Pico 4/Virtual Desktop rhythmic stutters, recording
  microskips, crashes or measured frame-rate gains.

## Installation and remaining limits

Extract the complete **Windows player ZIP**, close the game, start SteamVR for
Auto/percentage resolution profiles and choose **1 — Install/update** in
`CoJ-VR.cmd`. Saved resolution and manual-reload choices persist. Before later
plays, choose **6 — Prepare session** while closed; start/close SteamVR and the
D3D9 game manually. Virtual Desktop users need its SteamVR route, not VDXR.
The accepted local Manual 2804×2860 PS VR2 reference is retained, not imposed
on other users. Auto remains the missing-preference default.

Physical bow drawing, holsters/guides, duels, continuous body following, native
stereo and earlier cinematic recovery are retained from .5 and prior versions.
They are not newly introduced or broadly revalidated by this release. Open
limits include full campaign/hardware coverage; secret-scroll crashes; tracking,
dashboard, held-input, death/retry, save/load and device-loss recovery; manual
reload presentation/mechanics; scope/head stability; mounted comfort; physical-
walk leg animation; complete HUD capture; and sustained/recorded performance.
OpenXR, D3D10, Bound in Blood and Gunslinger remain outside this release.

Reports stay local until inspected and manually shared. Include the version,
weapon/checkpoint, hardware and reproduction steps. Original code remains
GPL-3.0-or-later; the ZIP retains licenses, notices and `SOURCE_COMMIT.txt`,
without redistributing game archives. Published .5 artifacts, historical notes
and canonical verdicts are unchanged. [All source changes since prealpha.5](https://github.com/rubocopter/call_of_juarez_vr/compare/v0.1.0-prealpha.5...v0.1.0-prealpha.6).

# v0.1.0-prealpha.5 — Physical bow, rifle support and virtual holsters

Experimental community prerelease for **original Steam Call of Juarez (2006),
D3D9/OpenVR, Windows 10/11 x64**. Cumulative from prealpha.4. Back up saves;
campaign completion and general headset/controller support remain unqualified.

**Remington bimanual grip is operator-validated on the exercised PS VR2 setup.
Physical manual reload for rifles/shotguns is still NOT implemented:** Square
keeps the original automatic reload. Winchester/SawOff support is experimental
and needs its own physical confirmation.

## Gameplay and controls added since prealpha.4

- **Physical bow draw:** hold/aim with the left hand; bring the right hand to the
  string, freshly hold R2, retract and release R2 to shoot. Returning toward the
  bow or moving off-axis relaxes tension without losing the held string. A short
  or slack release does not request a shot. The exact bow root/holding frame,
  native hand-state supervision and vibration transport are integrated. Native
  ammo, timing and concentration rules remain authoritative. The operator reports
  very good exercised bow use; sustained consistency, arrow/string alignment,
  haptic quality and interruption recovery remain separate checks.
- **Bimanual rifles/shotguns:** fresh L1 near the wooden fore-end acquires support
  on an idle, right-owned Remington, Winchester or SawOff. One rigid map keeps
  the weapon, hands, muzzle and aiming ray together without stretching. Release
  L1 to detach; Square cancels support for native reload. Crossing the support
  hand behind the dominant grip cancels instead of flipping the barrel. Native
  cycling retains the acquired local contact. Scoped Winchester is excluded.
- **Correct rifle contact:** acquisition and its optional L1 guide now use
  measured wood geometry on the live weapon element. This fixes the reported
  floating Remington grab point; the player's native support socket supplies
  anatomy rather than contact position. The corrected Remington grasp is
  headset-validated in exercised play. Other models and broad recovery remain
  unqualified.
- **Virtual holsters:** fresh R1 at the right hip toggles its pistol, L1 at the
  left hip toggles its pistol, R1 above/behind the right shoulder toggles the
  long weapon, and L1 above/behind the left shoulder toggles the bow. Release
  between gestures. These ergonomic zones require native inventory/permissions,
  operate on foot outside duels, and have scoped operator acceptance. They do
  not add visible holstered weapon models or precise body-socket grasping.
- **Optional weapon guides:** native Options > Game can show colored holster
  zones and the small rifle L1 marker. Cyan locates rifle contact, green indicates
  nearby released-hand admission, grey requests release; the marker hides when
  attached. Guides default off and share the real acquisition geometry. The
  operator accepts exercised holster guides and switching assistance on/off.
- **Equipment wheel presentation:** procedural Western styling and localized
  labels retain hold/select/release behavior and native equipment authority.
- **Native VR options:** Options > Game now exposes cinematic theater/immersive,
  smooth/snap turning and weapon guides using native widgets. Shared version-3
  preferences preserve older saved choices and are shared with the held-Options
  VR settings panel. A missing turning preference defaults to smooth. Explicit
  choices persist; a failed save remains a visible session-only change. EN/ES
  authored labels follow the game's startup locale.
- **Physical duel posture:** stick-down draw requires a fresh right hand at the
  hip and stick-up raise requires physically raising it. Native duel timing and
  permissions still apply. The operator confirms the correct gesture works and
  tested incorrect/cheating postures are rejected. Broader duel timing/retry and
  recenter recovery remain open.
- **Continuous body following:** normal gameplay follows every HMD yaw change,
  replacing the previous free-look cone while preserving scoped camera/body
  restoration. This has scoped operator acceptance. Legs still slide during
  physical walking/turning; room-scale leg animation is not implemented.
- **Immersive movie subtitles:** module-owned subtitle text can now be transported
  during immersive cinematics, respecting native subtitle ownership/settings and
  clearing stale text. The operator confirms subtitle appearance in exercised
  animations without specifying all modes. Other timing/mode cases remain open;
  theater stays the cinematic default and earlier accepted recovery is retained.
- **Bow concentration optics:** use the native cached concentration value for
  scoped world-view magnification, preserving physical HUD/menu optics and
  avoiding repeated native updates. This part is host-tested; visual alignment
  and comfort remain unqualified.

## Fixes and reliability

- **Free-hand rifle trigger:** L2 no longer falls through the native empty-hand
  attack route and fires a right-owned Remington. The operator confirms the fix.
  Consumed trigger holds require physical release across context changes; R2
  remains the normal firing trigger.
- **Grip arbitration:** rifle and holster claims consume conflicting Action/Kick
  input, including denied zone presses, so they cannot leak into delayed kicks
  or other interactions. Earlier unwanted kicks were corrected; exercised
  holsters are accepted, broader held-input recovery remains open.
- **Rifle regripping:** approaching with a released L1 preserves the release
  baseline; pressing outside contact never queues an automatic later grab.
  Named Remington Holder element zero is accepted as its valid root socket.
- **Rifle ownership and stereo:** retain the actual Java weapon object and epoch
  across restored frames, reject same-ID replacements and changed mesh anchors,
  recheck both hand maps before the single weapon commit, and verify muzzle/ray
  ownership through both eyes. A failed expected map invalidates the pair;
  successful restoration cannot publish images from a failed pair.
- **Bow continuity:** capture-ring misses alone no longer cancel an otherwise
  valid physical draw. Consumed R2 remains consumed through lost owner proof;
  real tracking/native owner loss still cancels and requires release. Holding a
  valid draw no longer inherits the unrelated concentration-expiry cancellation;
  native release/shot and concentration duration are retained.
- **Manual revolver reload:** preserve grip/feedback continuity across slow wrist
  motion, reject discontinuous carries, add a soft held-zone cue distinct from
  accepted-insertion feedback, and a short native-confirmed support-thumb gesture
  after insertion. Cartridge axis follows the measured displayed-finger tangent.
  Convincing cartridge grip, hand/scene occlusion, real chamber insertion,
  insertion-specific sound and ejection remain unfinished. The existing admitted
  profiles and native ammo/reserve authority are retained.
- **Body restoration:** keep the original actor, generation and complete preimage
  through pelvis/diagnostic-arm transactions, failed writes and translated restore
  retries. Restoration runs before replacement, disabled-tracking exits or new
  mutations; a failure retains ownership and stops dependent work.
- **Chest/scroll crash mitigation:** preserve only a verified, still-active native
  render-level context around injected input dispatch. Foreign, removed or
  read-only invalid contexts reject. This is host-tested mitigation, **not a
  confirmed fix of the reported campaign crash**; keep save backups.
- **Transactional recovery:** inventory hidden/system and nested directory contents
  before restoring/removing staged files. Preserve unknown contents and journals;
  reject fresh staging when the original directory cannot be represented safely.
- **Resolution reporting:** launcher output distinguishes saved intent, applied
  size and specific fallback causes, including unavailable SteamVR recommendations
  and incompatible native caps. No new resizing backend, automatic supersampling
  policy or live resolution switching is introduced.

## Diagnostics and engineering

- Fix [issue #5's](https://github.com/rubocopter/call_of_juarez_vr/issues/5) confirmed
  unwanted HUD sprite/flush diagnostic: ordinary sessions no longer install those
  hooks with resource probes disabled. Retained callbacks also honor permission.
  The report actually shows working GPU sharing during gameplay; CPU theater
  transfer is separate. **Resolution of the reported Pico 4/Virtual Desktop
  rhythmic stutter is still unverified; this release does not claim it fixed.**
- Retire automatic FX-global/emitter walks, duplicate shot/barrel comparisons and
  repeated skeleton discovery from ordinary play. They remain available only
  through explicit resource/movement traces. Keep exact-build rejection, muzzle/
  hand verification, rollback, crash reporting and lightweight transport/cadence
  counters active. This is reduced diagnostic work, not a measured FPS claim.
- Add opt-in, bounded rifle admission/native Winchester reload observations and
  offline bow/reload evidence analysis with owner/epoch/state correlation. These
  observers neither implement manual rifle reload nor prove chamber occupancy.
- Extend host regressions for settings/bytecode verification, gestures, geometry,
  same-ID replacement, failed restoration, staging recovery and packaging.
- Document native reload contracts for the remaining weapons and assess external
  VR reference projects. Development-only body-haptics admission/recording
  groundwork adds no vest SDK/provider or supported hardware. No external VR
  framework or physics runtime is added by this research.

## Install/update and focused community checks

Extract the complete **Windows player ZIP**, close the game, start SteamVR for
Auto/percentage profiles and select **1 — Install/update** in `CoJ-VR.cmd`.
Resolution and manual-reload preferences survive updates. Before later plays,
choose **6 — Prepare session** while closed. Start/close the game and SteamVR
manually. The GitHub automatic source ZIP is not the player package.

Please report your weapon/checkpoint, headset/controllers and any observed jump,
missed grab or input leak. Test an available unqualified Winchester/SawOff support
grip, bow consistency and native options/holsters during ordinary play. Existing
Remington grasp and accepted baselines need no repeat solely for confirmation.
For issue #5, compare movement pacing on the affected setup with ordinary probes
off; no additional PS VR2 run can establish the Pico result. Reports remain local
until inspected and manually attached.

Remaining limits: full campaign and other hardware; chest/scroll crash recovery;
tracking/dashboard/device-loss and held-input recovery; native PC tutorial labels;
mounted/climbing/scoped contexts; cartridge fit/occlusion and complete chamber/
ejection mechanics; physical long-gun reload; room-scale leg animation; sustained
and recording performance. Independent eye RTs, OpenXR and D3D10 remain separate
experimental/future tracks and are not enabled by this package.

Original project code is GPL-3.0-or-later. The player ZIP includes the license,
third-party notices and exact `SOURCE_COMMIT.txt`; no game archives/assets are
distributed. [All source changes since prealpha.4](https://github.com/rubocopter/call_of_juarez_vr/compare/v0.1.0-prealpha.4...v0.1.0-prealpha.5).

# v0.1.0-prealpha.4 — Duels, dual reticles and compatibility

Experimental community prerelease for the **original Steam Call of Juarez (2006),
D3D9/OpenVR**. Cumulative from .3; back up your saves. The operator confirms the
tested candidate works in the exercised PS VR2 Sense setup. This is scoped
functional acceptance, not full campaign coverage or general hardware support.

**Manual reload for long guns is not implemented. Rifles/shotguns still use the
original automatic reload and its native animation (Sense: Square).** Physical
cartridge loading remains limited to the admitted revolver profiles.

## Changes since prealpha.3

- **Duel controls:** center the right stick on entry, pull down to draw when the
  game permits, then up to raise. Horizontal stick input retains native duel aim;
  tracked-hand aiming and triggers remain available. Ordinary turning/body-follow
  yaw yield to the duel. Clyde draw/raise and tracked aiming/fire are operator-
  confirmed; native timing, permissions and early-draw rules are preserved.
- **Two simultaneous weapon reticles:** each verified firearm gets its own
  continuously displayed aiming mark, including while idle or firing either hand.
  Each mark has independent projections for both eyes and its own qualified
  native no-shoot flag. Missing weapon/tracking/context cannot reuse another ray.
- **Careful movement:** apply native stick shaping before head-relative rotation
  and retain smaller gaze-direction changes in native input dispatch, addressing
  the reported angular deadzone/drift during slow movement.
- **Smooth turning by default:** proportional turning up to 90 degrees/second
  in normal native look mode. Timing is independent of render sample rate;
  lost/stale input resets until the stick is centered. Native special-mode
  coefficients remain authoritative. A smooth/snap selector is still planned.
- **Long-gun free hand:** allow controller finger curls for the logically empty
  left support hand when a normally held right rifle owns a verified visual map.
  This corrects the inherited support-grip pose away from the gun. Firing,
  alternate-fire, reload and transitional states can retain native fingers;
  this does not add two-hand gripping or physical long-gun reload.
- **Manual reload choice restored:** launcher **8 — Manual reload on/off** changes
  the setting transactionally while the game is closed. Fresh installations
  default on; updates and Prepare session retain the installed choice.
- **Launcher help:** **9 — Controls and startup help** explains launch, pause and
  native save-menu access for each packaged controller profile. Updated guidance
  distinguishes R3 Focus/zoom, Circle alternate fire and Ray concentration.
- **OneDrive video-settings compatibility:** recognize Windows Cloud Files
  metadata for redirected `Video.scr` paths. The exception is limited to video
  settings; real links/junctions, unknown tags and linked game/package/backup
  paths remain rejected, including changed recovery targets. Host sandbox and
  simulated-cloud tests pass; live OneDrive operation still needs confirmation.
- **Documentation and validation:** current guides reflect the released features,
  operator confirmations and remaining limits. Ordinary Ray concentration is
  confirmed after holstering; its native activation rules were clarified, not
  replaced with a new concentration button or implementation.

## Controls to know

- **Ray concentration:** holster both weapons, allow the native cooldown/readiness
  and holster animations to finish, then freshly press L2 or R2. With both pistols
  drawn, hold Triangle, highlight right with the right stick and release to
  holster right; center, then repeat left for the left pistol. Selecting fists
  is not holstering. R3 remains Focus/zoom.
- **Blocking tutorial asking for Z:** Cross dismisses the hint. Release, then
  Circle requests alternate fire after gameplay resumes. Native tutorial windows
  still show PC-key labels; this limitation is not fixed in .4.
- **Reload:** Square opens/closes an admitted manual revolver session when enabled.
  The free-hand trigger carries/releases rounds. Long guns retain automatic
  native reload and animation. The manual system remains experimental.

## Install/update and remaining limits

Extract the complete **Windows player ZIP**, close the game, start SteamVR for
Auto/percentage profiles, then select **1 — Install/update** in `CoJ-VR.cmd`.
The first session is prepared automatically. Before later plays choose
**6 — Prepare session** while closed; start the game manually. Saved resolution
profiles and the reload on/off choice survive updates. **7** changes resolution,
**8** changes reload, **9** shows controls/help, and **2** restores original files.

The .3 asymmetric OpenVR projection fix and missing-preference Auto resolution
are retained. Beyond gameplay-view feedback and an unspecified Quest report are
positive; broader headset/controller compatibility and menu gaze alignment
remain unqualified. Only PS VR2 Sense is the exercised development reference.

Remaining work includes physical long-gun reload, native VR tutorial labels,
the smooth/snap selector, convincing cartridge contact/precise chamber loading,
ejection and broad interruption recovery. Full campaign, other weapons/duels,
mounted/climbing/scoped contexts, the reported chest/possible-scroll crash,
device loss and sustained/recorded performance remain open. Theater remains the
cinematic default; immersive movie subtitle transport is unfinished. Archived
packages and canonical verifier outcomes are unchanged by operator acceptance.

Original project code is GPL-3.0-or-later. The ZIP includes license/third-party
notices and `SOURCE_COMMIT.txt`; no game archives are distributed.

# v0.1.0-prealpha.3 — Experimental community release

Cumulative from **prealpha.2**, including the prior prealpha.1 baseline, for the
exact original Steam Call of Juarez (2006), D3D9/OpenVR build. Back up saves:
campaign coverage is incomplete and crashes can block progress. Physical
acceptance remains limited to exercised PS VR2 Sense scenes. Other hardware and
bindings are experimental; no feature is promoted to `supported`. Archived .2/.1
packages and verifier verdicts remain unchanged.

## New in .3

- **Correct OpenVR vertical projection conversion for asymmetric optics.** The
  .2 converter reverses the vertical projection center for asymmetric raw vertical
  tangents. Regression tests compare the complete converted matrix and forward
  menu placement with Valve's independent formula, covering asymmetry in both
  directions and symmetric PS VR2 controls. This fix is **HOST-TESTED only**:
  Beyond/Quest gameplay distortion and menu gaze alignment still need physical
  confirmation. It does not establish a complete fishbowl fix or headset support.
- **Auto resolution when no preference is saved.** Auto uses the SteamVR helper's
  recommended per-eye extent, not panel-native pixels. Every explicit saved
  preference, including Manual and Preserve, survives updates.
- **Manual reload enabled by default on fresh Install and Install/update**, even
  when a previous installation had it off. There is no interactive reload y/n
  prompt. Ordinary stereo, independent hands, native torso/legs and physical
  Hands combat remain enabled. Cinematic theater stays the default; the rejected
  independent eye RT prototype and investigation probes stay off.

## Install, update and later plays

Extract the complete player ZIP, close the game and keep original backups and
recovery data. **Start SteamVR manually before Install/update or Prepare session
for Auto/percentage profiles.** Choose **1 — Install/update** and the game folder.
Installation prepares the first session; resolution selection is optional.

To change resolution, choose **7 — Resolution profiles**, then **6 — Prepare
session**. Before every later play, close the game and choose **6** for fresh
provenance, with SteamVR already running for queried profiles. Prepare session
retains the installed feature set without repatching archives. Start the D3D9
game manually; no runtime or game lifecycle is automated.

Advanced noninteractive Install accepts `-ManualReload:$false`. Prepare session
retains that installed choice; a later default Install/update enables reload
again. Resolution preferences persist independently. **2 — Uninstall/recover**
restores originals; retain recovery data on errors. **3 — Diagnostics**, **4 —
Status** and **5 — Verify package** remain available. Verification does not prepare
a session. Reports stay local until inspected and manually shared.

No compiler, Git, system Python or PowerShell 7 is needed. The package uses
Windows PowerShell 5.1, bundled isolated Python and the VC++ x86 runtime on Windows
10/11 x64. Users supply the game, SteamVR and GPU/headset drivers. Avoid switching
desktop focus while playing: the original game can crash when it loses focus.

## Resolution and reload

- **Auto / 100%:** use the fresh helper recommendation. Auto is the missing-
  preference default. If the helper is unavailable or caps are incompatible,
  preparation preserves the selected game size with a visible explanation,
  retains profile intent and retries at the next preparation.
- **75% / 125%:** scale total pixel count relative to that recommendation; each
  axis scales by the square root of the ratio, approximately preserving aspect.
- **Manual:** saved width/height per eye, with no recommendation query.
- **Preserve:** explicitly retain the selected game size, with no query.

Profiles apply at startup, including menu/theater sources, and never resize a
running game. There is no new upscaler. Helper Background/game Scene identity
equivalence, complete native/theater allocation compatibility and x86 memory
headroom remain open. Caps checks and the Auto default do not establish support.
**Manual 2804×2860** remains the accepted saved local PS VR2 reference, backed by
actual raster/capture/output evidence and repeated positive operator feedback.
It is not imposed on other users or a sustained performance guarantee.

**Reload:** with an eligible revolver, missing rounds, reserve and an empty free
hand, press Reload (Sense: Square) to open; press/hold the free-hand trigger at
your hip to grab a round; bring it to the gun and release; press Reload to close.
Accepted insertions give haptic feedback. Native ammunition remains authoritative.
Convincing grip, precise chamber loading and broad interruption recovery remain
unfinished.

## Retained .2 baseline

- Headset-validated PS VR2 stereo recovery after Hope and multiple exercised
  cinematics, including entry/exit, hot presentation switches, weapons, recenter
  and continued gameplay. This does not qualify Quest recovery or every cinematic.
- Accepted main-menu GPU/angular correction at exactly 2804×2860, preserving
  proportions and native pointer coordinates; Ray's accepted 8 cm visual setback
  and coat partition.
- Safe reclamation of unfinished stereo capture slots and preservation of all
  32 native identity bits across JNI for reload and physical Hands ownership.
  General high-address compatibility remains separate.
- Host-tested loading-screen Cross/trigger key-down correction; fresh physical
  continuation remains pending. Persistent profiles, bounded query/caps helpers,
  layered video recovery and UTF-8 recommendation input handling are retained.
  Recovery preserves conflicting edits and restores session video before install
  originals.

## Known limitations

The chest/possible-scroll crash remains unresolved. Full campaign, mounted
contexts, other weapons, special optics, duels, climbing and complete graphical
HUD coverage remain open. The latest left-hand interaction ray and reload visual
refinements still await physical acceptance; cartridge occlusion, precise loading
and ejection remain unfinished.

Theater remains the cinematic default; immersive subtitle transport is unfinished.
Earlier intermittent full-scene flashes have not recurred in later exercised
sessions, but their cause is unresolved. Broader tracking/dashboard/held-input,
death/retry, save/load, abnormal-exit and device-loss recovery remain open, as do
sustained pacing, recording-associated microskips and other headsets/PCs. No repeat
of the accepted PS VR2 baseline is requested merely for release.

OpenXR, D3D10, Bound in Blood and Gunslinger are outside this release. Inspect
local diagnostics for private information before sharing; include version,
checkpoint, hardware and reproduction steps.

Original project code is GPL-3.0-or-later. The ZIP includes `LICENSE`,
`COPYRIGHT_AND_REUSE.md` and `SOURCE_COMMIT.txt` identifying its corresponding
source. Bundled dependencies retain their own terms in `THIRD_PARTY_NOTICES.txt`.
