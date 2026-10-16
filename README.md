# Call of Juarez VR

<p align="center">
  <img alt="Call of Juarez VR" src="assets/CoJ-VR.png" width="100%">
</p>

<p align="center">
  <img alt="Status: pre-alpha" src="https://img.shields.io/badge/status-pre--alpha-orange?style=flat-square">
  <img alt="Platform: Windows" src="https://img.shields.io/badge/platform-Windows-blue?style=flat-square">
  <img alt="Runtime: SteamVR" src="https://img.shields.io/badge/runtime-SteamVR-1b2838?style=flat-square">
</p>
<p align="center">
  <a href="https://ko-fi.com/onitaku"><img alt="Support me on Ko-fi" src="https://ko-fi.com/img/githubbutton_sm.svg"></a>
</p>

**A native PCVR conversion of Call of Juarez (2006), built around tracked head and hands.**

> **Experimental community-testing pre-alpha.** The campaign is not fully tested, crashes can block progress, and this is not a supported release. Physical validation is limited to exercised scenes with PS VR2 Sense through SteamVR; other packaged controller bindings are experimental.

## What works

- Native stereo, head tracking, recentering and room-scale movement.
- Controller locomotion, physical crouching and snap turning.
- Independent tracked hands and weapons, with the original torso and legs visible. Exercised pistol effects and whip use work in-headset.
- Controller menus, equipment wheel and haptics, wrist compass, encountered HUD and subtitles.
- Physical Hands combat with immediate knuckle contact; headset-tested in a fight against three enemies. Broader hand, campaign and recovery coverage remains open.
- Experimental manual revolver reload: Square opens/closes, the free-hand trigger carries and inserts rounds, hands remain tracked and accepted insertions give haptic feedback. The game owns ammunition.
- Exercised pickup/carry, drawers, Focus and corrected failure-screen presentation.

## Requirements and installation

The package requires **Windows 10/11 x64** and targets the **original Steam Call of Juarez (2006), x86 game, D3D9 and OpenVR/SteamVR**. Exact executable and archive identities are checked; unknown builds are rejected. You supply the game, SteamVR, and GPU/headset drivers. OpenXR, D3D10, Bound in Blood and Gunslinger are outside this prerelease.

Published player ZIPs appear on the [Releases page](https://github.com/rubocopter/call_of_juarez_vr/releases). Choose the **windows-x64.zip** asset; GitHub's automatic source-code ZIP is for development. Extract the complete player ZIP anywhere and double-click **CoJ-VR.cmd**. Its menu provides **Install/update**, **Uninstall/recover**, **Diagnostics**, **Status**, **Verify package** (5) and **Prepare session** (6). Bundled Python and the VC++ x86 redistributable allow offline installation without a compiler, Git or system Python. The launcher uses system Windows PowerShell 5.1; PowerShell 7 is not required.

Back up your saves before testing. Close the game before installing, updating or uninstalling. Select the game directory and optionally enable **ManualReload** during installation. That preference persists across updates unless explicitly changed. To update, extract a newer ZIP and choose Install/update; there is no automatic updater. Keep installation backups and recovery data until uninstall succeeds. If a protected Steam game folder causes **Access denied**, right-click **CoJ-VR.cmd**, choose **Run as administrator** and retry.

Installation prepares the first play session. Before **every subsequent play session**, with the game closed, choose **6 — Prepare session** to create a fresh diagnostic identity. Then start SteamVR and the D3D9 game manually; the launcher does not start them. Close them manually afterward. **5 — Verify package** checks package files; it does not prepare a play session.

PS VR2 Sense is the only physically validated controller reference. Additional packaged bindings are experimental and do not establish compatibility with another headset or PC. See the [controller cheat sheet](release/README.txt) for Sense, Oculus/Meta Touch, Valve Index, Vive wands and WMR, including Touch/Index directional stick-click pause/recenter. Custom SteamVR bindings are allowed; displayed prompts still use Sense labels. [Detailed Sense controls](docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls) remain available. No download is announced here yet.

## Known limitations

- A chest/possible-scroll interaction has caused a campaign crash. Its cause and recovery remain unresolved; progress can be lost.
- Immersive movies previously flashed the wrong image. A correction awaits headset retesting. Theater is the default; immersive movie subtitles are not transported.
- Tracking/dashboard/held-control recovery, broader death/retry and save/load, device loss and abnormal exit remain incompletely tested. Normal quit acceptance does not establish crash recovery.
- Other weapons, mounted controls, duels, climbing, special optics, sustained performance and recording compatibility need broader campaign coverage.
- Optional manual reload has scoped acceptance, but convincing cartridge grip, precise chamber loading and broader interruption recovery remain unfinished. The latest hand-directed interaction also awaits physical acceptance.

## Reporting a problem

Use **Diagnostics** to generate a local report. Inspect it for personal paths or other private information before manually attaching it to a [GitHub bug report](https://github.com/rubocopter/call_of_juarez_vr/issues/new?template=bug_report.yml). Include the package version, chapter/checkpoint, headset, controllers, GPU and reproduction steps. Reports are not uploaded automatically; do not attach saves or memory dumps without reviewing their contents.

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

The distribution build uses `powershell -NoProfile -ExecutionPolicy Bypass -File tools/package_release.ps1 -Version 0.1.0-prealpha.1` from clean committed source. It builds/tests the isolated OpenVR Release tree and emits the player ZIP plus SHA-256 checksum. The GitHub release workflow accepts a Version through `workflow_dispatch` and prepares a draft prerelease with separate developer symbols; review it and select Publish when ready. Update `release/RELEASE_NOTES.md` for each version. Package checks do not establish additional hardware validation.

</details>

## Disclaimer

An unofficial community project, not affiliated with or endorsed by Techland, Ubisoft, Valve or Sony Interactive Entertainment. Game assets are not redistributed; the imported cartridge is [CC0](assets/models/README.md).

No project-code license has been selected. Bundled Python, OpenVR and Microsoft VC++ runtime licenses and source notices are listed in [THIRD_PARTY_NOTICES.txt](release/THIRD_PARTY_NOTICES.txt); they do not license the project code.
