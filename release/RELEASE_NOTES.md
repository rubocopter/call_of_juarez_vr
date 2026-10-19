# Unreleased compatibility candidate

- Manual reload remains available and defaults on for fresh installs. Launcher
  option 8 switches to conventional reload or back; updates preserve the choice.
- Launcher option 9 explains startup and the per-profile pause/save menu controls.
- Recognized Cloud Files metadata is allowed for video settings only, addressing
  OneDrive's false-positive link rejection. Real junctions/symlinks and unknown
  tags still reject, including changed recovery targets. Host coverage uses
  simulated cloud metadata and real filesystem transactions; live OneDrive
  operation still needs community confirmation.
- Native controller-axis shaping runs before head-relative rotation, retaining
  small lateral components during careful movement. Native dispatch no longer
  drops small gaze changes. These fixes are host-tested; the complete community
  gaze-drift report still needs physical confirmation.
- Smooth turning is the ordinary default: proportional input up to 90 degrees/
  second in the native normal-look route, independent of rendering sample rate.
  Native special-mode coefficients remain authoritative. Stale/lost input cannot
  accumulate a catch-up turn. A VR settings smooth/snap selector is planned.
- A duel-only right-stick mapping forwards down/up to native draw/raise and
  horizontal input to native aim, retaining triggers and native permissions.
  Ordinary turning/body yaw yield during a duel. This is an experimental
  candidate; Clyde progression and headset aim still need physical confirmation.
- Scoped community feedback confirms .3's corrected Beyond gameplay view. An
  unspecified Quest report is also positive. The operator confirms concentration
  draw/fire in an exercised window-entry rescue; this does not accept duel draw.

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
