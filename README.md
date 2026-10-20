# Call of Juarez VR

<p align="center">
  <img alt="Call of Juarez VR" src="assets/CoJ-VR.png" width="100%">
</p>

<p align="center">
  <img alt="Status: pre-alpha" src="https://img.shields.io/badge/status-pre--alpha-orange?style=flat-square">
  <img alt="Platform: Windows" src="https://img.shields.io/badge/platform-Windows-blue?style=flat-square">
  <img alt="Runtime: SteamVR" src="https://img.shields.io/badge/runtime-SteamVR-1b2838?style=flat-square">
</p>

**A native PCVR conversion of Call of Juarez (2006), built around tracked head and hands.**

> **Experimental community-testing pre-alpha.** The campaign is not fully tested, crashes can block progress, and this is not a supported release. Physical validation is limited to exercised scenes with PS VR2 Sense through SteamVR; other packaged controller bindings are experimental.

**[Download v0.1.0-prealpha.4 — Windows player ZIP](https://github.com/rubocopter/call_of_juarez_vr/releases/download/v0.1.0-prealpha.4/CoJ-VR-0.1.0-prealpha.4-windows-x64.zip)** · [Release notes and checksums](https://github.com/rubocopter/call_of_juarez_vr/releases/tag/v0.1.0-prealpha.4)

**New in .4:** native duel draw/raise controls, simultaneous reticles for both firearms, careful-movement corrections, smooth turning by default, improved free-hand fingers with a normally held long gun, and launcher reload on/off and controls/help menus. Updates retain the manual-reload choice. Recognized OneDrive/Cloud Files video-setting paths are admitted; live OneDrive operation still needs community confirmation.

**Long-gun manual reload is not implemented.** Rifles/shotguns retain the original automatic reload and its native animation (Sense: Square). Manual cartridge loading remains limited to the admitted revolver profiles. The operator confirms the final candidate's exercised functions on PS VR2, including the corrected support-hand presentation and ordinary concentration activation. This is scoped functional acceptance, not full campaign or hardware support.

## Quickstart

You need **Windows 10/11 x64**, the **original Steam Call of Juarez (2006), D3D9 mode**, SteamVR and GPU/headset drivers. Unknown game builds are rejected. OpenXR, D3D10, Bound in Blood and Gunslinger are outside this prerelease.

1. **Back up your saves.** Run the original game in D3D9 mode once, then close it.
2. Download and extract the complete **Windows player ZIP** above. Keep its folders together; GitHub's automatic source-code ZIP is for development.
3. Start **SteamVR manually before Install/update** for Auto or percentage profiles. **Auto** is used when no resolution preference is saved.
4. Double-click **CoJ-VR.cmd**, choose **1 — Install/update** and select the game folder containing `CoJ.exe`. Stereo, independent hands, native torso/legs and physical Hands combat are enabled. Manual revolver reload defaults on for a fresh installation; **8 — Manual reload on/off** changes it while the game is closed.
5. Installation prepares the first session using the saved profile. Resolution selection is optional: choose **7 — Resolution profiles**, then **6 — Prepare session** only if you change it afterward. Start the **D3D9 game manually**.
6. Before **every subsequent play session**, close the game, start SteamVR manually for Auto/percentages, optionally change the profile with **7**, then choose **6 — Prepare session** for fresh provenance. It retains installed features. Start the game manually; the launcher never starts, closes or changes its focus.

No compiler, Git, system Python or PowerShell 7 is needed. The package bundles Python and the VC++ x86 runtime for offline installation and uses Windows PowerShell 5.1.

To update, close the game, extract a newer player ZIP, start SteamVR manually for Auto/percentages and choose **1 — Install/update**. Explicit resolution preferences and the installed manual-reload on/off choice persist. **8** changes reload transactionally; **9 — Controls and startup help** explains launch, pause and save-menu controls for each packaged profile. Installation prepares the updated session. Use **2 — Uninstall/recover** to restore original files and keep backups/recovery data until it succeeds. For **Access denied** in a protected game folder, run **CoJ-VR.cmd** as administrator and retry. **5 — Verify package** checks package files; it does not prepare a session. [Full package instructions](release/README.txt).

### Image quality

**Image resolution:** The player package includes [saved startup profiles](docs/research/COJ_VR_RELEASE_RESOLUTION_PROFILES.md): **Preserve, Auto, 75%, 100%, 125% and Manual** width/height per eye. With the game closed, use **CoJ-VR.cmd → 7**, then **6** to apply a change. Missing preferences default to **Auto**; all explicit saved choices persist. Auto and 100% use the **SteamVR helper's recommended per-eye extent, not panel-native pixels**. Percentages scale total pixel count while approximately preserving aspect ratio. Start SteamVR manually before Install/update or Prepare session queries it. If the helper is unavailable or caps are incompatible, preparation keeps the selected game size and explains why; it retains profile intent and retries on the next preparation. Background/Scene recommendation identity remains unqualified.

**Validated local reference:** Manual **2804×2860 per eye** has measured native raster/capture/output dimensions and repeated positive PS VR2 operator feedback. It is that setup's saved choice and remains intact across updates; no repeat test is pending. Auto does not impose that size on other users. Other profiles remain experimental. Higher sizes can increase GPU and x86 memory pressure; recommendation equivalence, sustained timings and broad recovery remain separate [validation limits](docs/VALIDATION.md#image-quality-profile-acceptance).

Profiles change native rendering at startup, including menu/theater sources, and never resize a running game. There is no new upscaler. At **2804×2860**, the accepted main-menu GPU presentation preserves its angular reference, proportions and native pointer coordinates; this targeted correction is not enabled for other sizes. SteamVR's slider alone does not enlarge the game's native render targets.

## What you can try

These features have scoped headset validation in exercised scenes, not full campaign coverage:

- Native stereo, tracked head, recentering and room-scale movement; controller locomotion, physical crouch and default smooth turning. The published .4 build has no in-game smooth/snap selector.
- Independent tracked hands/weapons with the original torso and legs; exercised pistol effects, whip use and physical Hands combat.
- Controller menus, equipment wheel/haptics, wrist compass, encountered HUD/subtitles, pickup/carry, drawers and Focus.
- Experimental manual revolver reload, enabled by default: Square opens/closes, the free-hand trigger carries/inserts rounds and accepted insertions give haptic feedback. Cartridge grip and precise chamber loading remain unfinished.
- Simultaneous independent aiming reticles with two firearms; Clyde duel draw/raise with the right stick and tracked-hand aiming/fire; corrected free-left-hand fingers with a normally held long gun.

Compared with prealpha.1, this release also improves Ray's body/coat presentation, including the accepted 8 cm horizontal visual setback. Stereo recovery after cinematics, including Hope, is fixed and headset-validated across multiple exercised sequences. The operator also accepted hot presentation-mode changes, different equipped weapons, recentering during sequences and continued gameplay; this is not complete campaign coverage.

## Controller guide

Current PS VR2 Sense defaults:

| Control | Action |
| --- | --- |
| Left stick / L3 | Move / crouch toggle |
| Right stick / R3 | Smooth turn (duel: draw/raise/aim) / Focus zoom |
| L2 / R2 | Left/right native hand action; manual reload uses the free-hand trigger |
| L1 / R1 | Interact along left-hand ray / kick or mounted gallop |
| Square / Cross / Circle | Reload / jump / alternate fire |
| Hold Triangle | Equipment wheel; select with right stick and release to confirm |
| Create tap / hold 800 ms | Objectives / recenter |
| Options tap / hold 1 s | Pause on release / VR settings |

PS VR2 Sense default bindings. **Hold Options for 1 second** to open VR settings; a short tap pauses on release. For manual reload with an eligible revolver and empty support hand: **press Square to open → press/hold the free-hand trigger at your hip to grab a round → bring it to the gun and release → press Square to close**. For the other experimental controller profiles, see the [controls cheat sheet](release/README.txt).

The .4 VR settings panel is an **experimental interface** for cinematic presentation. Later native Options > Game and smooth/snap work is described in the [roadmap](docs/ROADMAP.md#parallel-engineering-tracks) and is not in the download.

**Duels:** center the right stick on entry, pull down to draw when the game permits, then up to raise; aim with your tracked hand and use the triggers to fire. **Ray concentration:** holster both weapons and wait for native readiness, then make a fresh L2/R2 press. R3 is Focus/zoom; selecting fists is not holstering. With both pistols drawn, hold Triangle, highlight the right sector with the right stick and release to holster the right pistol; center and repeat left for the left pistol. Let both animations finish. **Blocking hints:** Cross dismisses; release before the next action. A PC Z prompt means Circle/alternate fire once gameplay resumes. Native tutorial windows still display PC keys.

PS VR2 with Sense through SteamVR is the only physically validated reference, limited to exercised scenes. Packaged Touch, Index, Vive wand and WMR bindings are experimental and do not establish support for another headset or PC. Custom SteamVR bindings are allowed; displayed prompts still use Sense labels. [Detailed Sense controls](docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls).

## Known limitations

- A chest/possible-scroll interaction has caused a campaign crash. Its cause and recovery remain unresolved; progress can be lost.
- Full campaign cinematic coverage remains open. Theater is the default; immersive movie subtitles are not transported. Earlier intermittent full-scene flashes have not recurred in later exercised sessions, but their cause remains unresolved.
- Tracking/dashboard/held-control recovery, broader death/retry and save/load, device loss and abnormal exit remain incompletely tested. Normal quit acceptance does not establish crash recovery.
- Other weapons, mounted controls, duels, climbing, special optics, sustained performance and recording compatibility need broader campaign coverage.
- Manual reload has scoped revolver acceptance, but convincing cartridge grip, precise chamber loading and broader interruption recovery remain unfinished. Long guns retain automatic native reload and its original animation. The latest hand-directed interaction also awaits physical acceptance.
- Native tutorial windows still show PC keys. The long-gun support-hand correction applies in normal weapon states; firing/alternate/reload/transitions can retain native fingers.
- Community feedback confirms the retained .3 projection correction on Beyond and reports a good Quest view without specifying its model. Menu gaze alignment and broader headset/controller support remain unqualified. The rejected independent eye RT prototype and investigation probes remain off.

The latest published release is **v0.1.0-prealpha.4**, cumulative from .3, .2 and .1. See [release notes](release/RELEASE_NOTES.md) for the changes since .3 and [Validation](docs/VALIDATION.md) for acceptance limits. Unpublished source changes have their own validation gates. Archived packages and canonical verifier outcomes remain unchanged.

## Feedback

- [Reproducible bug](https://github.com/rubocopter/call_of_juarez_vr/issues/new?template=bug_report.yml): what happened, where and how to repeat it.
- [Compatibility, image quality or performance](https://github.com/rubocopter/call_of_juarez_vr/issues/new?template=compatibility_report.yml): your setup and what you observed, including successful tests.
- [Suggestion](https://github.com/rubocopter/call_of_juarez_vr/issues/new?template=suggestion.yml): what would improve your experience.

Logs are **optional**. **3 — Diagnostics** creates a local report for you to inspect and manually share; there is no automatic upload or remote telemetry. Remove private information before attaching reports, screenshots or clips. Do not share unreviewed saves or memory dumps.

## Documentation

[Validation and limits](docs/VALIDATION.md) ·
[Roadmap](docs/ROADMAP.md) ·
[Architecture](docs/ARCHITECTURE.md) ·
[Sense controls](docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls)

<details>
<summary><strong>Development</strong></summary>

Windows/x86 requires Visual Studio 2022 or Build Tools with C++ support and CMake 3.25+. Run [OpenVR bootstrap](tools/bootstrap_openvr.ps1) before configuring a fresh checkout; [OpenXR bootstrap](tools/bootstrap_openxr.ps1) supplies the optional experimental loader.

```powershell
cmake --preset win32-debug
cmake --build --preset release
ctest --preset release
```

Physical candidates use `tools/vr_test.ps1 prepare` and `finish`; add `-BodyIkAtStart -LargeAddressAware -ManualReload` for the manual-reload candidate. The operator starts/closes SteamVR and the game. Read [AGENTS.md](AGENTS.md) and the [validation workflow](docs/VALIDATION.md#evidence-policy) before integration work.

The distribution build uses `powershell -NoProfile -ExecutionPolicy Bypass -File tools/package_release.ps1 -Version 0.1.0-prealpha.4` from clean committed source. It builds/tests the isolated OpenVR Release tree and emits the player ZIP plus SHA-256 checksum. The GitHub release workflow accepts a Version through `workflow_dispatch` and prepares a draft prerelease with separate developer symbols; review it and select Publish when ready. Update `release/RELEASE_NOTES.md` for each version. Package checks do not establish additional hardware validation.

</details>

## Disclaimer

An unofficial community project, not affiliated with or endorsed by Techland, Ubisoft, Valve or Sony Interactive Entertainment. Game assets are not redistributed; the imported cartridge is [CC0](assets/models/README.md).

Original project code is licensed under **[GPL-3.0-or-later](LICENSE)**. See [copyright, reuse and provenance](COPYRIGHT_AND_REUSE.md) for scope, attribution and third-party exclusions. Bundled Python, OpenVR and Microsoft VC++ runtime notices are listed in [THIRD_PARTY_NOTICES.txt](release/THIRD_PARTY_NOTICES.txt) and retain their own terms.

## Support development

If you would like to help fund development, [support the project on Ko-fi](https://ko-fi.com/onitaku).

<p align="center">
  <a href="https://ko-fi.com/onitaku"><img alt="Support me on Ko-fi" src="https://ko-fi.com/img/githubbutton_sm.svg"></a>
</p>
