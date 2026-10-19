Call of Juarez VR 0.1.0-prealpha.3 - experimental player package
Experimental community-testing prerelease

The complete campaign is not tested. Crashes can block or lose progress.
Back up your saves before testing. This package does not promise support or
campaign completion. Read RELEASE_NOTES.md before installing.

This development candidate is cumulative from .3, including the earlier .2
baseline and OpenVR asymmetric vertical projection correction. Community
feedback confirms the corrected Beyond gameplay view; an unspecified Quest
report is also positive. These observations do not establish general support.
Auto remains the missing resolution preference. Manual reload starts on for
fresh installation and current updates preserve its choice. New movement,
smooth-turn and duel changes are host-tested, awaiting physical confirmation.
Archived .3/.2/.1 packages and verdicts are unchanged.

REQUIREMENTS

- Windows 10/11 x64 host; the game and VR game DLL remain x86.
- Original Steam Call of Juarez (2006), x86 game, D3D9 renderer.
  Installation checks exact game identities and rejects unknown builds.
- SteamVR (OpenVR), a PCVR headset and its drivers, and GPU drivers.
  You supply the game, SteamVR and drivers; they are not in this ZIP.
- System Windows PowerShell 5.1. PowerShell 7 is not required.

Python x64, OpenVR x86 and the VC++ x86 redistributable are bundled for offline
installation. No compiler, Git or system Python is required. Read
THIRD_PARTY_NOTICES.txt for dependency sources, licenses and notice locations,
including dependencies/python/LICENSE.txt and licenses/OpenVR-LICENSE.txt.
Microsoft's terms accompany the VC++ installer. Game files are not distributed
in this package.

COPYRIGHT AND SOURCE

Original Call of Juarez VR source code is GNU GPL version 3 or later;
see LICENSE and COPYRIGHT_AND_REUSE.md for the scope and exclusions.
SOURCE_COMMIT.txt identifies the exact corresponding source commit and URL
for this package. Redistributions of modified GPL-covered binaries require
GPL-compliant source availability and preservation of copyright notices.
Third-party code and original game assets remain under their own terms.

CONTROLLERS

PS VR2 Sense is the only physically validated reference, limited to exercised
scenes. Additional packaged bindings are experimental; supplying a binding
does not establish compatibility with another controller, headset or PC.
The included payload's OpenVR binding files identify the available profiles.
Custom SteamVR bindings are allowed. Known limitation: displayed prompts still
use Sense labels, including Square, Triangle, Create and Options.

DEFAULT CONTROLS CHEAT SHEET (L = left hand, R = right hand)

Controller       Objectives / Recenter       Pause / VR settings
PS VR2 Sense     L Create                    R Options
Oculus/Meta Touch L stick UP + click          R stick UP + click
Valve Index      L thumbstick UP + click     R thumbstick UP + click
HTC Vive wands   L application-menu button   R application-menu button
WMR              L application-menu button   R application-menu button

UP + click means deflect upward AND press the stick, not a centered click.
Keep it upward while holding the click. For Objectives, tap and RELEASE before
800 ms in gameplay. Hold 800 ms to recenter once; releasing after the hold does
not also show Objectives. For Pause, a short tap acts on RELEASE; hold 1 second
to open VR settings instead. Release buttons and center sticks before resuming.

Controller       Reload / hold wheel         Jump / alternate fire
PS VR2 Sense     L Square / Triangle          R Cross / Circle
Oculus/Meta Touch L X / Y                     R A / B
Valve Index      L A / B                     R A / B
HTC Vive wands   L pad DOWN / UP click       R pad UP / DOWN click
WMR              L pad DOWN / UP click       R pad UP / DOWN click

Hold the wheel input, aim a sector with the right stick/pad, then release the
wheel input to confirm; center the selection to cancel. In menus, R Cross/Circle
(Sense), R A/B (Touch/Index), or R pad UP/DOWN click (Vive/WMR) accept/go back.
Either hand's trigger selects with that hand's menu pointer.
To save/load, tap and release the profile's Pause control, then select Save/Load
in the native pause menu when the campaign permits. Quick Save/Quick Load have
no default controller binding; optional actions can be assigned in SteamVR.
Launcher option 9 - Controls and startup help shows pause/menu controls by
profile and the startup sequence. Custom SteamVR bindings may differ.

Move: L stick (Vive: L trackpad). Turn: R stick (Vive: R trackpad).
Ordinary turning is smooth and proportional, up to 90 degrees/second in native
normal-look mode. Center the stick after a menu, lost input or long frame gap.
A smooth/snap selector in VR settings is not implemented yet. This new default
has host coverage; comfort and behavior still need headset confirmation.
Experimental duel controls: center the right stick after entering the duel;
pull it down to draw when the game allows, then up to raise the weapon. Left/
right aims in the native duel; triggers retain native firing. Ordinary smooth
turning yields during a duel. Clyde progression and headset aim are not yet
physically validated. Concentration outside duels keeps its existing controls.
Crouch / Focus: L / R stick click; Touch/Index require a CENTER click.
Vive uses L / R trackpad CENTER click. WMR uses stick clicks, not pad centers.
L grip = interact; R grip = kick (Sense: L1 / R1). Triggers retain the game's
left/right hand actions; the free-hand trigger carries/releases rounds in
manual reload, enabled by default.
Directional clicks can coincide with movement/turning: Touch/Index UP-clicks
deflect their sticks, Vive pads also move/turn, and WMR's separate movement/turn
sticks can remain deflected. Center those inputs when you want to stand still.

Sense controls: https://github.com/rubocopter/call_of_juarez_vr/blob/main/docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls

INSTALL / UPDATE

1. Extract the complete ZIP anywhere. Keep its folders together.
2. Back up your saves and close Call of Juarez.
3. Start SteamVR manually BEFORE Install/update for Auto/percentage profiles.
   Auto is the default when no resolution preference is saved.
4. Double-click CoJ-VR.cmd, choose Install/update and select the game directory.
   Ordinary stereo, independent hands, native torso/legs and physical Hands combat
   are enabled. Manual reload starts on for a fresh installation; updates retain
   its installed on/off choice. Option 8 - Manual reload on/off changes it through
   Install/update while the game is closed, without scripting flags.
5. Installation prepares the first session using the saved resolution profile.
   Resolution selection is optional: to change it, choose 7 - Resolution profiles,
   then 6 - Prepare session before starting the D3D9 game manually. No additional
   preparation is needed if you keep the profile used during installation.
   Start and close the game and SteamVR manually. Avoid switching desktop focus
   while playing: the original game can crash when it loses focus.

Before EVERY subsequent play session, close the game and choose menu option
6 - Prepare session. This creates a fresh diagnostic identity and applies the
saved resolution profile for that play session. It retains the installed feature
set without repatching game archives. Start SteamVR manually before preparation
for Auto/percentage profiles, then start the D3D9 game manually.

MANUAL RELOAD

Choose 8 - Manual reload on/off with the game closed: 1 On enables the motion
reload below; 2 Off restores conventional reload; 0 Cancel leaves the installation
unchanged. Choosing On/Off immediately performs Install/update and prepares the
session. Tracked head/body, independent hands and physical melee remain enabled.
Ordinary updates preserve the installed choice, and Prepare session respects it
without repatching the archives. Start SteamVR first for Auto/percentage profiles.

With an eligible revolver, missing rounds, reserve and an empty free hand:
press Reload (Sense: Square) to open; press/hold the free-hand trigger at your
hip to grab a round; bring it to the gun and release; press Reload to close.
Accepted insertions give haptic feedback. Grip/contact and precise chamber
loading remain unfinished; unsupported weapons retain native reload handling.

IMAGE RESOLUTION

The mod currently captures each eye at the game's own D3D9 render-target
resolution. SteamVR supplies headset optics, but increasing SteamVR's render
resolution percentage alone does not increase native game detail or make the
game render at SteamVR's recommended per-eye resolution.

Close the game and choose 7 - Resolution profiles, then 6 - Prepare session.
Auto is used only when no preference is saved. Every explicitly saved choice,
including Preserve and Manual, survives updates. Preserve retains the selected
game size. Auto/100% use the SteamVR helper's recommended per-eye extent, NOT
panel-native pixels. Other experimental choices are 75%, 125% and Manual
width/height per eye. Percentages scale pixel count: 75% uses sqrt(0.75) on each
recommended axis. If the helper is unavailable or caps are incompatible, the
selected game size is preserved with a visible explanation. Profile intent stays
saved; the next preparation retries Auto/percentages. SteamVR must already be
running for those profiles; Manual needs no recommendation.
The query helper's application scaling equivalence to the game remains unqualified.

The existing engine renders at the applied native size; no post-capture upscaler
is added. Menu/theater sources also change. Dimensions are fixed for the session;
saving a profile never resizes a running game. Higher sizes can increase GPU and
32-bit game memory use. Caps do not qualify total memory or every headset.
Manual 2804x2860 per eye is the accepted local PS VR2 reference: repeated
operator tests report sharper detail, correct stereo and no perceptible
performance issues in the exercised scenarios, with prior measured native
raster/capture/output dimensions. It is not a universal default or sustained
performance guarantee. Inspect the reported applied size; configured dimensions
alone do not prove actual eye raster.

Profile intent persists across update/uninstall, independently of cinematic
preferences. Each preparation restores the prior session override first.
Uninstall/recovery restores the session and installation video originals in order;
intervening game/user edits are retained in verified adjacent conflict copies.
Retain recovery journals and backups on errors. Avoid live resolution changes.

If a protected Steam game folder causes Access denied, right-click CoJ-VR.cmd,
choose Run as administrator and retry the operation.

Redirected Documents folders, including OneDrive provider folders, are accepted
for Video.scr when their reparse metadata belongs to the Windows Cloud Files
family. This exception applies only to video settings. Symlinks, junctions,
unknown reparse types and linked deployment/package/backup paths remain rejected.
The error identifies the offending path component and tag. Keep recovery journals
and backups when a path check fails; restore the expected path before recovery.
Coverage uses simulated cloud tags and real junction/rollback sandbox tests;
live OneDrive operation has not been validated.

To update, extract a newer complete ZIP and choose Install/update again.
There is no auto updater. All explicit resolution preferences persist, as does
the installed manual reload on/off choice. Use option 8 to change reload safely;
advanced noninteractive Install also accepts -ManualReload:$false or
-ManualReload:$true. Prepare session retains the installed choice.
Retain installation backups and recovery
data until uninstall has succeeded. Do not remove them to bypass an error.

VERIFY / STATUS / DIAGNOSTICS / UNINSTALL

Menu option 5 - Verify package checks package files without preparing a session.
Use CoJ-VR.cmd for Status or Diagnostics. Diagnostics creates a local report;
it does not upload it. Close the game before choosing Uninstall to restore
the installation's original files. If restoration fails, preserve the backups,
recovery data and error output, then generate diagnostics and report the issue.
Normal shutdown has scoped acceptance; arbitrary crashes, device loss and
recovery across all campaign contexts are not validated.

COMMAND LINE (from the extracted ZIP root)

Replace the example game path with your Steam game directory:

powershell -NoProfile -ExecutionPolicy Bypass -File .\cojvr.ps1 -Action Install -GameDirectory "C:\path\to\Call of Juarez"
powershell -NoProfile -ExecutionPolicy Bypass -File .\cojvr.ps1 -Action Uninstall -GameDirectory "C:\path\to\Call of Juarez"
powershell -NoProfile -ExecutionPolicy Bypass -File .\cojvr.ps1 -Action Diagnose -GameDirectory "C:\path\to\Call of Juarez"
powershell -NoProfile -ExecutionPolicy Bypass -File .\cojvr.ps1 -Action Status -GameDirectory "C:\path\to\Call of Juarez"
powershell -NoProfile -ExecutionPolicy Bypass -File .\cojvr.ps1 -Action Verify
powershell -NoProfile -ExecutionPolicy Bypass -File .\cojvr.ps1 -Action PrepareSession -GameDirectory "C:\path\to\Call of Juarez"

Run PrepareSession with the game closed before every play after the first.
Verify checks package files; it does not establish game/headset compatibility.
Fresh Install enables manual reload; default updates preserve its installed
choice. Advanced noninteractive Install accepts -ManualReload:$false or
-ManualReload:$true to change it explicitly; menu option 8 provides the same choice.
PrepareSession keeps the installed feature set and does not repatch archives.
These are package-root commands; release\cojvr.ps1 is a source-repository path.

KNOWN LIMITATIONS

- The .3 OpenVR projection fix is HOST-TESTED only; Beyond/Quest gameplay
  distortion and menu gaze alignment still require physical confirmation.
  Physical acceptance remains scoped to exercised PS VR2 scenes.
- Background/Scene recommendation identity, other headsets and memory headroom
  remain unqualified. Auto is a product default, not a support guarantee.
- A chest/possible-scroll interaction has caused a crash. Cause and recovery
  are unresolved; campaign progress is at risk.
- Full campaign cinematic coverage and immersive subtitle transport remain open.
  Stereo recovery after Hope and multiple other exercised cinematics is fixed
  and headset-validated, including hot mode changes, weapons and recentering.
  Theater remains the default. Earlier intermittent full-scene flashes have not
  recurred in later exercised sessions, but their cause remains unresolved.
- Tracking/dashboard/held-input recovery, broader death/retry, save/load,
  abnormal exit, device loss and sustained/recorded performance remain open.
- Mounted controls, other weapons, special optics, duels and climbing lack full
  campaign coverage. Manual reload lacks convincing cartridge contact, precise
  chamber loading and broad interruption acceptance.
- OpenXR, D3D10, Bound in Blood and Gunslinger are outside this prerelease.
  The rejected independent eye RT prototype and investigation probes remain off.

REPORTING

Inspect the local diagnostic report for private paths or other personal data
before manually attaching it to a GitHub bug issue. Include the package version,
chapter/checkpoint, headset, controllers, GPU and reproduction steps. Do not
attach saves, memory dumps or unrelated personal files without reviewing them.
https://github.com/rubocopter/call_of_juarez_vr/issues/new?template=bug_report.yml

PACKAGE CONTENTS

CoJ-VR.cmd, cojvr.ps1, README.txt, RELEASE_NOTES.md, THIRD_PARTY_NOTICES.txt,
LICENSE, COPYRIGHT_AND_REUSE.md, SOURCE_COMMIT.txt, payload\, tools\,
dependencies\ and release-manifest.json.

Unofficial project; not endorsed by Techland, Ubisoft, Valve or Sony.
Original project code is GPL-3.0-or-later; see LICENSE and COPYRIGHT_AND_REUSE.md.
Bundled dependencies retain their own terms in THIRD_PARTY_NOTICES.txt.
