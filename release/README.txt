Call of Juarez VR - v0.1.0-prealpha.1
Experimental community-testing prerelease

The complete campaign is not tested. Crashes can block or lose progress.
Back up your saves before testing. This package does not promise support or
campaign completion. Read RELEASE_NOTES.md before installing.

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

Move: L stick (Vive: L trackpad). Turn: R stick (Vive: R trackpad).
Crouch / Focus: L / R stick click; Touch/Index require a CENTER click.
Vive uses L / R trackpad CENTER click. WMR uses stick clicks, not pad centers.
L grip = interact; R grip = kick (Sense: L1 / R1). Triggers retain the game's
left/right hand actions; the free-hand trigger carries/releases rounds in
optional manual reload.
Directional clicks can coincide with movement/turning: Touch/Index UP-clicks
deflect their sticks, Vive pads also move/turn, and WMR's separate movement/turn
sticks can remain deflected. Center those inputs when you want to stand still.

Sense controls: https://github.com/rubocopter/call_of_juarez_vr/blob/main/docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls

INSTALL / UPDATE

1. Extract the complete ZIP anywhere. Keep its folders together.
2. Back up your saves and close Call of Juarez.
3. Double-click CoJ-VR.cmd and choose Install/update.
4. Select the game directory when asked. ManualReload is optional: enable it
   only if you want the experimental manual revolver-loading interaction.
5. Installation prepares the first session. Start SteamVR and the D3D9 game
   manually. Close them manually afterward; the launcher does not start them.

Before EVERY subsequent play session, close the game and choose menu option
6 - Prepare session. This creates a fresh diagnostic identity for that play
session. Then start SteamVR and the D3D9 game manually.

IMAGE RESOLUTION

The mod currently captures each eye at the game's own D3D9 render-target
resolution. SteamVR supplies headset optics, but increasing SteamVR's render
resolution percentage alone does not increase native game detail or make the
game render at SteamVR's recommended per-eye resolution.

For a sharper source image, close the game and choose 2 - Uninstall/recover.
Run the unmodded D3D9 game, select a higher resolution supported by its video
settings, and exit. Choose 1 - Install/update again. The installer preserves
the chosen game resolution and stages FSAA 0. Higher resolutions can increase
GPU and 32-bit game memory use. Avoid live resolution changes during a VR
session; device-reset recovery remains unvalidated.

If a protected Steam game folder causes Access denied, right-click CoJ-VR.cmd,
choose Run as administrator and retry the operation.

To update, extract a newer complete ZIP and choose Install/update again.
There is no auto updater. Your manual-reload choice persists across updates
unless you explicitly change it. Retain installation backups and recovery
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
Use the menu to choose optional ManualReload during installation.
These are package-root commands; release\cojvr.ps1 is a source-repository path.

KNOWN LIMITATIONS

- A chest/possible-scroll interaction has caused a crash. Cause and recovery
  are unresolved; campaign progress is at risk.
- Immersive movies have shown wrong-image flashes. A correction awaits headset
  retesting. Theater is the default. Immersive movie subtitles are unavailable.
- Tracking/dashboard/held-input recovery, broader death/retry, save/load,
  abnormal exit, device loss and sustained/recorded performance remain open.
- Mounted controls, other weapons, special optics, duels and climbing lack full
  campaign coverage. Manual reload lacks convincing cartridge contact, precise
  chamber loading and broad interruption acceptance.
- OpenXR, D3D10, Bound in Blood and Gunslinger are outside this prerelease.

REPORTING

Inspect the local diagnostic report for private paths or other personal data
before manually attaching it to a GitHub bug issue. Include the package version,
chapter/checkpoint, headset, controllers, GPU and reproduction steps. Do not
attach saves, memory dumps or unrelated personal files without reviewing them.
https://github.com/rubocopter/call_of_juarez_vr/issues/new?template=bug_report.yml

PACKAGE CONTENTS

CoJ-VR.cmd, cojvr.ps1, README.txt, RELEASE_NOTES.md, THIRD_PARTY_NOTICES.txt,
payload\, tools\, dependencies\ and release-manifest.json.

Unofficial project; not endorsed by Techland, Ubisoft, Valve or Sony.
No project-code license has been selected. Third-party notices do not license
the project code.
