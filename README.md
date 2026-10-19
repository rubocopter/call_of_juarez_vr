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

**[Download v0.1.0-prealpha.3 — Windows player ZIP](https://github.com/rubocopter/call_of_juarez_vr/releases/download/v0.1.0-prealpha.3/CoJ-VR-0.1.0-prealpha.3-windows-x64.zip)** · [Release notes and checksums](https://github.com/rubocopter/call_of_juarez_vr/releases/tag/v0.1.0-prealpha.3)

**New in .3:** corrected OpenVR vertical projection conversion for asymmetric optics, Auto resolution when no preference is saved, and manual reload enabled by default on install/update. A community user confirms the corrected gameplay view on Bigscreen Beyond; an unspecified Quest report is also positive. This is scoped feedback, not general headset support. This release includes the .2 baseline; archived .2/.1 packages and verdicts remain unchanged.

**Unreleased development candidate:** launcher **8 — Manual reload on/off** with the choice retained across updates; **9 — Controls and startup help**; video-settings compatibility with recognized Cloud Files metadata; careful-movement corrections and smooth turning by default. These changes need physical/community confirmation. The published .3 download and its instructions below retain their original behavior.

## Quickstart

You need **Windows 10/11 x64**, the **original Steam Call of Juarez (2006), D3D9 mode**, SteamVR and GPU/headset drivers. Unknown game builds are rejected. OpenXR, D3D10, Bound in Blood and Gunslinger are outside this prerelease.

1. **Back up your saves.** Run the original game in D3D9 mode once, then close it.
2. Download and extract the complete **Windows player ZIP** above. Keep its folders together; GitHub's automatic source-code ZIP is for development.
3. Start **SteamVR manually before Install/update** for Auto or percentage profiles. **Auto** is used when no resolution preference is saved.
4. Double-click **CoJ-VR.cmd**, choose **1 — Install/update** and select the game folder containing `CoJ.exe`. Stereo, independent hands, native torso/legs, physical Hands combat and manual reload are enabled; there is no reload y/n prompt.
5. Installation prepares the first session using the saved profile. Resolution selection is optional: choose **7 — Resolution profiles**, then **6 — Prepare session** only if you change it afterward. Start the **D3D9 game manually**.
6. Before **every subsequent play session**, close the game, start SteamVR manually for Auto/percentages, optionally change the profile with **7**, then choose **6 — Prepare session** for fresh provenance. It retains installed features. Start the game manually; the launcher never starts, closes or changes its focus.

No compiler, Git, system Python or PowerShell 7 is needed. The package bundles Python and the VC++ x86 runtime for offline installation and uses Windows PowerShell 5.1.

To update, close the game, extract a newer player ZIP, start SteamVR manually for Auto/percentages and choose **1 — Install/update**. Every explicitly saved resolution choice, including Manual and Preserve, persists. Default Install/update enables manual reload even if it was previously off. Advanced noninteractive installation can use `-ManualReload:$false`; Prepare session retains that installed choice, but a later default Install/update enables it again. Installation prepares the updated session. Use **2 — Uninstall/recover** to restore original files and keep backups/recovery data until it succeeds. For **Access denied** in a protected game folder, run **CoJ-VR.cmd** as administrator and retry. **5 — Verify package** checks package files; it does not prepare a session. [Full package instructions](release/README.txt).

### Image quality

**Image resolution:** The player package includes [saved startup profiles](docs/research/COJ_VR_RELEASE_RESOLUTION_PROFILES.md): **Preserve, Auto, 75%, 100%, 125% and Manual** width/height per eye. With the game closed, use **CoJ-VR.cmd → 7**, then **6** to apply a change. Missing preferences default to **Auto**; all explicit saved choices persist. Auto and 100% use the **SteamVR helper's recommended per-eye extent, not panel-native pixels**. Percentages scale total pixel count while approximately preserving aspect ratio. Start SteamVR manually before Install/update or Prepare session queries it. If the helper is unavailable or caps are incompatible, preparation keeps the selected game size and explains why; it retains profile intent and retries on the next preparation. Background/Scene recommendation identity remains unqualified.

**Validated local reference:** Manual **2804×2860 per eye** has measured native raster/capture/output dimensions and repeated positive PS VR2 operator feedback. It is that setup's saved choice and remains intact across updates; no repeat test is pending. Auto does not impose that size on other users. Other profiles remain experimental. Higher sizes can increase GPU and x86 memory pressure; recommendation equivalence, sustained timings and broad recovery remain separate [validation limits](docs/VALIDATION.md#image-quality-profile-acceptance).

Profiles change native rendering at startup, including menu/theater sources, and never resize a running game. There is no new upscaler. At **2804×2860**, the accepted main-menu GPU presentation preserves its angular reference, proportions and native pointer coordinates; this targeted correction is not enabled for other sizes. SteamVR's slider alone does not enlarge the game's native render targets.

## What you can try

These features have scoped headset validation in exercised scenes, not full campaign coverage:

- Native stereo, tracked head, recentering and room-scale movement; controller locomotion, physical crouch and snap turning.
- Independent tracked hands/weapons with the original torso and legs; exercised pistol effects, whip use and physical Hands combat.
- Controller menus, equipment wheel/haptics, wrist compass, encountered HUD/subtitles, pickup/carry, drawers and Focus.
- Experimental manual revolver reload, enabled by default: Square opens/closes, the free-hand trigger carries/inserts rounds and accepted insertions give haptic feedback. Cartridge grip and precise chamber loading remain unfinished.

Compared with prealpha.1, this release also improves Ray's body/coat presentation, including the accepted 8 cm horizontal visual setback. Stereo recovery after cinematics, including Hope, is fixed and headset-validated across multiple exercised sequences. The operator also accepted hot presentation-mode changes, different equipped weapons, recentering during sequences and continued gameplay; this is not complete campaign coverage.

## Controller guide

[![Current PS VR2 Sense controls](assets/CoJ_sensevr.png)](assets/CoJ_sensevr.png)

PS VR2 Sense default bindings. **Hold Options for 1 second** to open VR settings; a short tap pauses on release. For manual reload with an eligible revolver and empty support hand: **press Square to open → press/hold the free-hand trigger at your hip to grab a round → bring it to the gun and release → press Square to close**. For the other experimental controller profiles, see the [controls cheat sheet](release/README.txt).

The VR settings panel is a **provisional development interface**, not the final settings experience; more comfort options are planned.

PS VR2 with Sense through SteamVR is the only physically validated reference, limited to exercised scenes. Packaged Touch, Index, Vive wand and WMR bindings are experimental and do not establish support for another headset or PC. Custom SteamVR bindings are allowed; displayed prompts still use Sense labels. [Detailed Sense controls](docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls).

## Known limitations

- A chest/possible-scroll interaction has caused a campaign crash. Its cause and recovery remain unresolved; progress can be lost.
- Full campaign cinematic coverage remains open. Theater is the default; immersive movie subtitles are not transported. Earlier intermittent full-scene flashes have not recurred in later exercised sessions, but their cause remains unresolved.
- Tracking/dashboard/held-control recovery, broader death/retry and save/load, device loss and abnormal exit remain incompletely tested. Normal quit acceptance does not establish crash recovery.
- Other weapons, mounted controls, duels, climbing, special optics, sustained performance and recording compatibility need broader campaign coverage.
- Manual reload has scoped acceptance, but convincing cartridge grip, precise chamber loading and broader interruption recovery remain unfinished. The latest hand-directed interaction also awaits physical acceptance.
- The .3 projection correction is host-tested only. Beyond/Quest gameplay distortion and menu gaze alignment still need physical confirmation; other headset support is unqualified. The rejected independent eye RT prototype and investigation probes remain off.

The current release target is **v0.1.0-prealpha.3**, cumulative from .2 and .1. Later changes on `main` require another release before they reach that ZIP. See [release notes](release/RELEASE_NOTES.md) and [Validation](docs/VALIDATION.md) for changes and acceptance limits.

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

The distribution build uses `powershell -NoProfile -ExecutionPolicy Bypass -File tools/package_release.ps1 -Version 0.1.0-prealpha.3` from clean committed source. It builds/tests the isolated OpenVR Release tree and emits the player ZIP plus SHA-256 checksum. The GitHub release workflow accepts a Version through `workflow_dispatch` and prepares a draft prerelease with separate developer symbols; review it and select Publish when ready. Update `release/RELEASE_NOTES.md` for each version. Package checks do not establish additional hardware validation.

</details>

## Disclaimer

An unofficial community project, not affiliated with or endorsed by Techland, Ubisoft, Valve or Sony Interactive Entertainment. Game assets are not redistributed; the imported cartridge is [CC0](assets/models/README.md).

Original project code is licensed under **[GPL-3.0-or-later](LICENSE)**. See [copyright, reuse and provenance](COPYRIGHT_AND_REUSE.md) for scope, attribution and third-party exclusions. Bundled Python, OpenVR and Microsoft VC++ runtime notices are listed in [THIRD_PARTY_NOTICES.txt](release/THIRD_PARTY_NOTICES.txt) and retain their own terms.

## Support development

If you would like to help fund development, [support the project on Ko-fi](https://ko-fi.com/onitaku).

<p align="center">
  <a href="https://ko-fi.com/onitaku"><img alt="Support me on Ko-fi" src="https://ko-fi.com/img/githubbutton_sm.svg"></a>
</p>
