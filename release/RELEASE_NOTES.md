# v0.1.0-prealpha.1 — experimental community prerelease

Prepared for community testing before whole-campaign acceptance. No feature is promoted to `supported`, and no complete
campaign or compatibility with other PCs/headsets is promised. Back up saves:
crashes and blocked progression can lose progress.

## Target and exercised features

- Windows 10/11 x64 host; exact original Steam Call of Juarez (2006), x86 game,
  D3D9 and OpenVR/SteamVR.
  Unknown executable/archive builds are rejected.
- Native stereo/head tracking, locomotion, independent hands/weapons, controller
  menus, encountered HUD/subtitles and physical Hands combat have scoped
  acceptance in exercised scenes with PS VR2 Sense.
- Optional manual revolver reload has scoped acceptance for opening/closing,
  successive native unit loading, tracked hands and insertion haptics. It is not
  a complete physical cylinder/chamber simulation.
- Additional packaged controller bindings are experimental. Only PS VR2 Sense
  has physical validation; a binding does not establish hardware support.

## Package workflow

Extract the complete ZIP anywhere and double-click `CoJ-VR.cmd` for Install/update,
Uninstall, Diagnostics and Status. System Windows PowerShell 5.1 is used; no
PowerShell 7, compiler, Git or system Python is needed. Python and the VC++ x86
redistributable are bundled for offline installation. Users supply the game,
SteamVR and GPU/headset drivers and start/close the game and SteamVR manually.

ManualReload is an optional installation choice that persists across updates
unless explicitly changed. Updates use a newer extracted ZIP; there is no auto
updater. Preserve original backups and recovery data until uninstall succeeds.
See `README.txt` for package-root CLI commands and reporting instructions.

## Known issues and incomplete coverage

- **Chest interaction crash:** an interaction near a chest/possible scroll has
  crashed the game. Cause and recovery are unresolved; added fault diagnostics
  are not a fix.
- **Movies:** immersive mode previously flashed the wrong image. The correction
  awaits headset retesting. Theater remains the default; immersive movie subtitle
  transport is unfinished. Movie transitions and comfort need further testing.
- **Recovery:** broader tracking/dashboard/held-control, death/retry, manual-session
  save/load, disable and abnormal-exit/device-loss recovery remain open. Acceptance
  of normal quit or an exercised retry does not close these gates.
- **Campaign:** mounted contexts, other weapons, special optics, duels, climbing
  and complete graphical HUD coverage are incomplete. The latest left-hand
  interaction ray still awaits physical acceptance.
- **Reload:** cartridge contact/occlusion, precise port/chamber insertion, ejection,
  additional profiles and broader interruption/comfort coverage remain unfinished.
- **Performance:** sustained pacing and recording-associated microskips remain
  unresolved. Current exercised gameplay feedback does not establish recording
  compatibility or performance on another PC.
- **Other tracks:** OpenXR is experimental; D3D10 and other CoJ games are outside
  this prerelease.

Generate Diagnostics locally, inspect the report for private information, then
manually attach it to a GitHub bug issue with version, chapter, headset,
controllers, GPU and reproduction steps. Reports are not uploaded automatically.

No project-code license has been selected. `THIRD_PARTY_NOTICES.txt` records
bundled Python, OpenVR and Microsoft VC++ runtime sources, licenses and notice
locations; those notices do not license the project code.
