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
