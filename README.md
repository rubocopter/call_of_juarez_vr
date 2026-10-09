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

> **Pre-alpha — no public release yet.** Tested in exercised Steam-build scenes with PS VR2 Sense through SteamVR. Broader campaign coverage is still in progress.

## What works

- Native stereo, head tracking, recentering and room-scale movement.
- Controller locomotion, physical crouching and snap turning.
- Independent tracked hands and weapons, with the original torso and legs visible. Exercised pistol effects and whip use work in-headset.
- Controller menus, equipment wheel and haptics, wrist compass, encountered HUD and subtitles.
- Experimental manual revolver reload: Square opens/closes, the free-hand trigger carries and inserts rounds, hands remain tracked and accepted insertions give haptic feedback. The game owns ammunition.
- Exercised pickup/carry, drawers, Focus and corrected failure-screen presentation.

## In progress

Manual reload works in the exercised context; convincing cartridge grip, precise chamber loading and broader recovery still need work. Other weapons, mounted/special mechanics, campaign coverage and sustained performance remain open. The latest hand-directed interaction also awaits a headset check.

Call of Juarez (2006), D3D9 and OpenVR are the active target. OpenXR is experimental; D3D10, Bound in Blood and Gunslinger are future tracks.

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

</details>

## Disclaimer

An unofficial community project, not affiliated with or endorsed by Techland, Ubisoft, Valve or Sony Interactive Entertainment. Game assets are not redistributed; the imported cartridge is [CC0](assets/models/README.md).
