Call of Juarez VR 0.1.0-prealpha.7 - experimental player package
Experimental community-testing prerelease

These instructions describe a package containing cojvr-gui.ps1. Older downloaded
archives may still open the numbered launcher; use the README included with
your particular ZIP. This .7 player package opens the graphical installer.

Players run CoJ-VR.cmd from a completely extracted player release ZIP. Its
release-manifest.json binds every included file to its hash.
In the developer repository, the same release/CoJ-VR.cmd graphical entry also
works: its first action automatically prepares and verifies a local player
package using the existing native Release build. Progress stays in the window;
subsequent actions reuse the package after verifying it again. Changed installer
or native artifact bytes select a new package. No manual copying of manifests
or switching folders is needed. This developer route requires an existing
build from tools/package_release.ps1; it does not compile missing native code.
Local dirty packages are for testing and are not published releases.
Saving a resolution choice works immediately and applies it on the next
Install/update or Prepare session. Incomplete player ZIPs still reject safely.

The complete campaign is not tested. Crashes can block or lose progress.
Back up your saves before testing. This package does not promise support or
campaign completion. Read RELEASE_NOTES.md before installing.

This release is cumulative from .5 and earlier versions. It expands experimental
manual loading to 15 exact firearm classes, adds scoped Winchester support and
revised optics, binding-aware floating hints, reload assistance and optional
graphics/horse comfort controls. Pause/movie input, rifle triggers, mounted
steering and recovery also change. Bow drawing, holsters, duels and continuous
body following are retained from .5. Auto remains the missing resolution
preference; manual reload starts on for fresh installs and updates preserve it.
Earlier projection/stereo corrections and archived verdicts remain unchanged.
RELEASE_NOTES.md lists changes since .5 and their acceptance limits.

IMPORTANT: NEW MANUAL RELOAD PATHS ARE EXPERIMENTAL. Earlier single-round
revolver loading and base Schofield whole-clip loading have scoped functional
operator feedback. New model paths, the new textured Schofield bundle and the
latest scope/body correction have host tests only. In the graphical installer,
choose Advanced > Off, then Install/update with the game closed, to restore
conventional native reload. This release does not
establish general hardware or campaign support.

Current validation: the 27 October PS VR2 Steam session accepts encountered
campfires/environmental effects and the corrected horse peripheral effect.
Mounted steering remains difficult; holding R1 also invokes native gallop and
increases speed. This does not establish reduced nausea or general comfort.
Winchester insertion requires a precise small target and its support hand can
adopt unnatural poses; these ergonomics remain unresolved. Scoped shot agreement
and head-yaw isolation remain separate gates. The latest canonical telemetry
profile rejects its eye-baseline comparison; scoped operator feedback does not
override that verdict. See Validation and Roadmap in the corresponding source.

New in .7: graphical Steam/GOG installation selection, experimental exact GOG
build admission, indexed native per-unit reload feedback/cycles, stronger held-
trigger recovery, measured Winchester geometry diagnostics and corrected horse
mask projection. Experimental GOG compatibility includes one successful local
play/save-load test, not a second general support commitment.

REQUIREMENTS

- Windows 10/11 x64 host; the game and VR game DLL remain x86.
- Original Steam Call of Juarez (2006), or the verified GOG build (experimental),
  x86 game, D3D9 renderer. Other builds or modified originals are rejected.
  Installation checks exact game identities and rejects unknown builds.
- SteamVR (OpenVR), a PCVR headset and its drivers, and GPU drivers.
  You supply the game, SteamVR and drivers; they are not in this ZIP.
- System Windows PowerShell 5.1. PowerShell 7 is not required.

Virtual Desktop users must use the SteamVR route for this OpenVR build.
Selecting VDXR does not change the mod's backend. Direct OpenXR/VDXR gameplay
is not implemented; SteamVR initialization is expected.

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
Custom SteamVR bindings are allowed. Adapted floating prompts use reported
controller bindings where unambiguous, with semantic fallback otherwise.
Native blocking tutorials still show PC keys; arbitrary binding chords and
every controller label/glyph are not qualified in the headset.

DEFAULT CONTROLS CHEAT SHEET (L = left hand, R = right hand)

Controller       Objectives / Recenter       Pause
PS VR2 Sense     L Create                    R Options
Oculus/Meta Touch L stick UP + click          R stick UP + click
Valve Index      L thumbstick UP + click     R thumbstick UP + click
HTC Vive wands   L application-menu button   R application-menu button
WMR              L application-menu button   R application-menu button

UP + click means deflect upward AND press the stick, not a centered click.
Keep it upward while holding the click. For Objectives, tap and RELEASE before
800 ms in gameplay. Hold 800 ms to recenter once; releasing after the hold does
not also show Objectives. Pause acts once on a fresh PRESS;
holding it does not open a quick panel. Use native Options > Game for VR settings.
Release buttons and center sticks before resuming.

Controller       Reload / hold wheel         Jump / alternate fire
PS VR2 Sense     L Square / Triangle          R Cross / Circle
Oculus/Meta Touch L X / Y                     R A / B
Valve Index      L A / B                     R A / B
HTC Vive wands   L pad DOWN / UP click       R pad UP / DOWN click
WMR              L pad DOWN / UP click       R pad UP / DOWN click

Hold the wheel input, aim a sector with the right stick/pad, then release the
wheel input to confirm; center the selection to cancel. In menus, R Cross/Circle
(Sense), R A/B (Touch/Index), or R pad UP/DOWN click (Vive/WMR) accept/go back.

Sense tutorial help: Cross closes a blocking hint; release before acting again.
The PC Z prompt means Circle during gameplay (alternate fire / quick shooting).
R3 is Focus/zoom. Ray concentration requires both weapons holstered, native
readiness/cooldown, then a fresh L2/R2 quickdraw. Selecting fists is not holstering.
With both pistols drawn: hold Triangle, highlight right with the right stick,
release to holster the right pistol. Center, then repeat with the left sector
for the left pistol. Let both holster animations finish before quickdrawing.
Native blocking tutorial images still show PC keys; this remains a known limit.
Either hand's trigger selects with that hand's menu pointer.
To save/load, tap and release the profile's Pause control, then select Save/Load
in the native pause menu when the campaign permits. Quick Save/Quick Load have
no default controller binding; optional actions can be assigned in SteamVR.
The legacy command-line menu (cojvr.ps1 -Action Menu), option 9, provides
startup help. Custom SteamVR bindings may differ.

Move: L stick (Vive: L trackpad). Turn: R stick (Vive: R trackpad).
Ordinary turning is smooth and proportional, up to 90 degrees/second in native
normal-look mode. Center the stick after a menu, lost input or long frame gap.
Native Options > Game exposes cinematic presentation, smooth/snap turning,
weapon guides and experimental graphics/comfort choices. Saved preferences
survive removal of the held-Pause/Options quick panel. Guides default off and
also enable ammunition pickup markers and brief reload coaching. Floating
reload hints were reported to flicker. This version corrects a reproduced
icon-expiry case that removed still-current instruction text; physical
stability/readability still need confirmation.
Disable weapon guides/coaching if needed. Default smooth turning retains scoped
acceptance. In immersive cinematics, fresh Pause/Back/Accept or trigger selection
requests native skip when permitted; physical verification remains pending.
Experimental duel controls: center the right stick after entering the duel;
place the right hand at the hip and pull down to draw when the game allows,
then physically raise the hand and push up to raise the weapon. Left/right
stick aims in the native duel; triggers retain native firing. Ordinary
turning yields during duels. Clyde draw/raise and tracked aim/fire have scoped
operator confirmation, including tested rejection of incorrect/cheating
postures. Broader duel/retry behavior remains unqualified.
Concentration outside duels retains its original activation requirements.
Crouch / Focus: L / R stick click; Touch/Index require a CENTER click.
Vive uses L / R trackpad CENTER click. WMR uses stick clicks, not pad centers.
L grip = interact; R grip = kick (Sense: L1 / R1). Triggers retain the game's
left/right hand actions; the free-hand trigger carries/releases rounds in
manual reload, enabled by default.
Bow: aim with the left hand; bring the right hand to the string, freshly hold
R2, draw back, then release R2 to shoot. Slack/off-axis motion relaxes tension
without losing the grab; native ammo, timing and concentration rules apply.
Bimanual long guns: with an idle right-held Remington, Winchester, scoped
Winchester or SawOff
and an empty left hand, freshly hold L1 near the wooden fore-end. Release L1
to detach; Square cancels support and prepares the selected reload mode.
Manual loading requires the support hand empty and L1 support released;
freshly regrip after closing. Enable weapon guides to locate contact. Corrected
Remington grasp and scoped Winchester grip/fire have exercised acceptance.
Ordinary Winchester R2 now requests fire rather than basic zoom, but was not
retested in the latest session. The large R3 scope/reticle has scoped acceptance;
the new controller-only scope/body-yaw correction is host-tested only.
Mounted Gatling: R2 fires; right stick aims horizontally and vertically.
Exercised firing/traversal works; precise aiming and recovery remain open.
Horse: left stick supplies forward/backward and reins; right stick is rider
free look, R1 gallop. Head-gaze steering/retained-reins recovery remains physically unqualified. The
corrected peripheral effect has scoped visual acceptance; overall comfort does not.
Virtual holsters (on foot outside duels): freshly press R1 at the right hip
or L1 at the left hip for pistols; R1 above/behind the right shoulder for the
long weapon, L1 above/behind the left shoulder for the bow. Release between
gestures. Zones require native inventory permissions; guides are optional.
Visible holstered weapon models are not added.
Directional clicks can coincide with movement/turning: Touch/Index UP-clicks
deflect their sticks, Vive pads also move/turn, and WMR's separate movement/turn
sticks can remain deflected. Center those inputs when you want to stand still.

Sense controls: https://github.com/rubocopter/call_of_juarez_vr/blob/main/docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls

INSTALL / UPDATE

1. Extract the complete ZIP anywhere. Keep its folders together.
2. Back up your saves and close Call of Juarez.
3. Start SteamVR manually BEFORE Install/update for Auto/percentage profiles.
   Auto is the default when no resolution preference is saved.
4. Double-click CoJ-VR.cmd to open the graphical installer. It looks for the
   original Call of Juarez (Steam AppID 3020 or registered GOG installation), including additional Steam
   libraries and a different install-folder name recorded in its Steam manifest.
   If several installations are found, select the intended folder from the game
   directory dropdown. Use Find game to scan again, or Browse if no folder is
   detected. A folder containing CoJ.exe and ChromeEngine3.dll shows a green
   status hint; this is only discovery, and installation still validates the
   exact supported game build before changing any game files.
   Click Verify package, then Install / update. Ordinary stereo, independent
   hands, native torso/legs and physical Hands combat are enabled. Manual reload
   starts on for new installations; updates retain the installed on/off choice.
   The Western-style header depicts a drawn frontier landscape; the package does
   not include original game artwork. All six actions run in the same window,
   with progress and command output shown below the settings. The installer
   does not start SteamVR or Call of Juarez for you.
5. Installation prepares the first session using the saved resolution profile.
   To change it, select Preserve, Auto, 75%, 100%, 125% or Manual (per-eye
   width/height) under Resolution, then click Save resolution choice. Click
   Prepare play session with the game closed to apply the newly saved profile.
   No extra preparation is needed if you keep the profile used at installation.
   Start and close the game and SteamVR manually. Avoid switching desktop focus
   while playing: the original game can crash when it loses focus.

Before EVERY subsequent play session, close the game and click
Prepare play session. This creates a fresh diagnostic identity and applies the
saved resolution profile for that play session. It retains the installed feature
set without repatching game archives. Start SteamVR manually before preparation
for Auto/percentage profiles, then start the D3D9 game manually.

MANUAL RELOAD

In the installer, Advanced > manual reload installation mode offers Keep current,
On (motion-controlled reload) and Off (conventional native reload). With the game
closed, select the desired choice and click Install / update to apply and prepare
the session. Changing the dropdown alone does not change the installation.
Tracked head/body, independent hands and physical melee remain enabled.
Ordinary updates preserve the installed choice, and Prepare session respects it
without repatching the archives. Start SteamVR first for Auto/percentage profiles.
An in-game Options > Game toggle is not currently implemented. Safe runtime
enable/disable and cancellation of held cartridges require separate validation.

With an admitted firearm, missing rounds, matching reserve and an empty free hand:
press Reload (Sense: Square) to open; press/hold the free-hand trigger at your
hip to grab ammunition; bring it to the gun's generic grip zone and release.
Repeat for individual rounds as applicable, then press Reload to close. The
armed-hand trigger can also close; release it before a fresh press to fire.
Release a trigger used to open an empty gun before trying to close. For long
guns, release L1 support before loading and freshly regrip after closing.
Indexed weapons require Square preparation; do not rely on an empty trigger
to select a chamber. Native ammunition/reserves remain authoritative.

The 15 admitted exact classes are:
- Single-round revolvers: Frontier1878_Regular, Peacemaker, Frontier, Walker
  and Cogswell. Cogswell uses native rifle reserve; the others use pistol reserve.
- Winchester and WinchesterScope: up to 12 rifle rounds; Volcanic: up to 8
  pistol rounds. Each accepted insertion loads one, with a supervised native
  animation cycle between rounds.
- Remington and RemingtonSawOff: one selected barrel at a time, two shells total,
  from shared shotgun reserve. Derringer: one selected chamber at a time, two
  rounds total, from pistol reserve. These do not load both slots on one release.
- Lemant (LeMat): one rifle-reserve round in its first slot and up to eight
  pistol-reserve rounds in its second. Pickup chooses the first eligible slot
  and keeps that choice until insertion/cancellation; a held cartridge is not
  redirected if another pool becomes available.
  These indexed weapons replay the native loading cycle after each inserted
  unit when ammunition remains. Wait for loading to become available again,
  then use a fresh pickup; a held trigger cannot queue another unit.
- Schofield base, Schofield_A and Schofield_B: one deliberate bundle loads the
  missing portion of the six-round clip, limited by pistol reserve, then closes.
  The new textured bundle depicts that offered amount. This is whole-clip loading,
  not individual chamber insertion or a measured speedloader/calibre model.

Earlier single-round revolver loading and base Schofield whole-clip loading have
scoped functional operator feedback. Schofield retry friction remains unresolved;
its textured replacement and opening-trigger correction await headset checks.
A/B and the other newly integrated model paths have host tests only. These
results do not accept every revolver or the complete combined reload variant.
Accepted insertions retain native-confirmed feedback. Cartridge grip/occlusion,
precise loading mouths, per-model ammunition visuals and broader interruptions
remain unfinished. Indexed per-unit native replay has host tests; its mechanical
appearance and interruption recovery still need in-game/headset qualification.
Unsupported weapons retain native reload handling; choose Advanced > Off and
Install / update if experimental manual reload is unusable.

IMAGE RESOLUTION

The mod currently captures each eye at the game's own D3D9 render-target
resolution. SteamVR supplies headset optics, but increasing SteamVR's render
resolution percentage alone does not increase native game detail or make the
game render at SteamVR's recommended per-eye resolution.

Close the game, save the resolution choice in the graphical installer, then click
Prepare play session to apply it.
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

The existing engine renders at the applied native size; no upscaler is added.
Menu/theater sources also change. Dimensions are fixed for the session;
saving a profile never resizes a running game. Higher sizes can increase GPU and
32-bit game memory use. Caps do not qualify total memory or every headset.
Manual 2804x2860 per eye is the accepted local PS VR2 reference: repeated
operator tests report sharper detail, correct stereo and no perceptible
performance issues in the exercised scenarios, with prior measured native
raster/capture/output dimensions. It is not a universal default or sustained
performance guarantee. Inspect the reported applied size; configured dimensions
alone do not prove actual eye raster.

Optional native Options > Game controls add sharpening, diagonal-edge smoothing,
contrast, saturation and gamma on fresh native stereo images, plus Off/Mild/Strong
mounted peripheral comfort. Effects default off. Menu layout, image quality,
comfort and sustained cost have host tests only. Theater and VR overlays are
outside the spatial filter pass. DLAA/DLSS selections save a request but keep
native rendering active; there is no integrated game reconstruction or measured
performance gain. Save the launcher resolution choice and prepare a session:
the native menu's saved render-scale request does not replace that profile.

Profile intent persists across update/uninstall, independently of cinematic
preferences. Each preparation restores the prior session override first.
Uninstall/recovery restores the session and installation video originals in order;
intervening game/user edits are retained in verified adjacent conflict copies.
Retain recovery journals and backups on errors. Avoid live resolution changes.

If a protected game folder causes Access denied, right-click CoJ-VR.cmd,
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
the installed manual reload on/off choice. Use Advanced > Off/On and Install / update to change reload safely;
advanced noninteractive Install also accepts -ManualReload:$false or
-ManualReload:$true. Prepare session retains the installed choice.
Retain installation backups and recovery
data until uninstall has succeeded. Do not remove them to bypass an error.

VERIFY / STATUS / DIAGNOSTICS / UNINSTALL

Verify package checks package files without preparing a session. Installation
status reads the current installed state; Save diagnostics creates a local ZIP;
it does not upload it. Close the game before choosing Uninstall to restore
the installation's original files. If restoration fails, preserve the backups,
recovery data and error output, then generate diagnostics and report the issue.
Normal shutdown has scoped acceptance; arbitrary crashes, device loss and
recovery across all campaign contexts are not validated.

COMMAND LINE (from the extracted ZIP root)

Replace the example game path with your Steam or GOG game directory:

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
In the graphical installer, Advanced > On/Off and Install / update is the
equivalent action; the old numbered menu remains available with
powershell -File .\cojvr.ps1 -Action Menu.
PrepareSession keeps the installed feature set and does not repatch archives.
These are package-root commands; release\cojvr.ps1 is a source-repository path.

KNOWN LIMITATIONS

- The retained .3 OpenVR projection fix has scoped Beyond gameplay confirmation
  and a positive Quest report without a specified model. Menu gaze alignment,
  broader headset/controller support and other PCs remain unqualified.
- Background/Scene recommendation identity, other headsets and memory headroom
  remain unqualified. Auto is a product default, not a support guarantee.
- A chest/possible-scroll interaction has caused a crash. A render-context
  mitigation is host-tested; its campaign fix and recovery remain unconfirmed.
- Full campaign cinematic coverage remains open. Immersive subtitle transport
  is implemented, with broader mode/timing acceptance still pending.
  Stereo recovery after Hope and multiple other exercised cinematics is fixed
  and headset-validated, including hot mode changes, weapons and recentering.
  Theater remains the default. Earlier intermittent full-scene flashes have not
  recurred in later exercised sessions, but their cause remains unresolved.
- Tracking/dashboard/held-input recovery, broader death/retry, save/load,
  abnormal exit, device loss and sustained/recorded performance remain open.
- Mounted controls, other weapons, special optics, duels and climbing lack full
  campaign coverage. Horse steering/comfort and broader Gatling precision/recovery
  remain open. The new left-hand interaction ray also awaits physical acceptance.
- New manual reload families, per-model animations/ports, provisional ammunition
  visuals and the textured Schofield bundle remain physically unqualified.
  Retry friction, convincing contact/occlusion, precise chamber loading, ejection
  and broad interruption recovery are unfinished. The support-hand correction is
  limited to normal weapon states; firing/alternate/reload can keep native fingers.
- Render-only scoped head-yaw isolation still failed physically. The additional
  body-yaw freeze has host tests only; ordinary Winchester R2 firing was accepted
  in the 26 October PS VR2 session. Scoped zoom reticle vs actual bullet impact
  is misaligned and remains rejected. Scope-exit recovery is still open.
- Ordinary Winchester manual insertion misses the visible loading mouth and is
  awkward in the exercised session; it requires measured port/contact geometry.
- Horse riding caused severe nausea/speed/motion. The peripheral comfort mask
  looked unfused with the setting enabled, so the operator switched it Off.
  The saved diagnostics only sample Off and cannot explain the enabled mask.
- Floating reload hints have a host-tested icon-expiry correction. Physical stability,
  binding-aware readability and immersive skip require physical checks.
- Spatial graphics controls retain host coverage. The corrected horse effect
  has scoped visual acceptance; broader comfort remains open. DLAA/DLSS requests
  leave game rendering native; no reconstruction or performance gain is claimed.
- Native tutorial windows still show keyboard labels. Use the controls guide
  above; R3 zoom, Ray concentration and Circle/alternate fire are different actions.
- OpenXR, D3D10, Bound in Blood and Gunslinger are outside this prerelease.
  The rejected independent eye RT prototype and investigation probes remain off.
- Pico 4/Virtual Desktop rhythmic stutters reported in issue #5 remain
  unresolved. HUD/FX/skeleton diagnostic work is now opt-in; this does not prove
  a performance fix. Muzzle/hand safety, restoration and transport counters remain.

REPORTING

Inspect the local diagnostic report for private paths or other personal data
before manually attaching it to a GitHub bug issue. Include the package version,
chapter/checkpoint, headset, controllers, GPU and reproduction steps. Do not
attach saves, memory dumps or unrelated personal files without reviewing them.
https://github.com/rubocopter/call_of_juarez_vr/issues/new?template=bug_report.yml

PACKAGE CONTENTS

CoJ-VR.cmd, cojvr-gui.ps1, cojvr.ps1, README.txt, RELEASE_NOTES.md, THIRD_PARTY_NOTICES.txt,
LICENSE, COPYRIGHT_AND_REUSE.md, SOURCE_COMMIT.txt, payload\, tools\,
dependencies\ and release-manifest.json.

Unofficial project; not endorsed by Techland, Ubisoft, Valve or Sony.
Original project code is GPL-3.0-or-later; see LICENSE and COPYRIGHT_AND_REUSE.md.
Bundled dependencies retain their own terms in THIRD_PARTY_NOTICES.txt.

Steam / GOG selection
---------------------
The folder list detects Steam libraries and GOG registered/default locations.
If both copies are present, select the intended folder; Browse also accepts a
custom/offline GOG location. The installer verifies the original executable,
engine and archives by SHA-256 before changing files, then selects the matching
installation path automatically. Folder names and store labels are not trusted.
GOG compatibility is experimental, limited to the exact tested build; it does
not establish full campaign or hardware support. Both editions use the same VR
payload and reversible workflow. No game or SteamVR process is started for you.
