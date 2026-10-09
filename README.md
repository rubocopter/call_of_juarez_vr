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

> **Pre-alpha â€” no public release yet.** Core VR gameplay works in the exercised Steam build with PS VR2 through SteamVR. Broader campaign coverage, recovery and comfort still need validation.

## What works

- Native per-eye stereo, head tracking, recentering, room-scale movement, physical crouching and snap turning.
- Native movement and jump behavior, with head-relative controller locomotion.
- Tracked independent hands and weapons, retaining the original torso and legs. Pistol firing, visible muzzle/impact effects and exercised whip use work in-headset.
- Controller-operated menus and pause, an equipment wheel with haptic feedback, a wrist compass and the encountered health/ammo HUD and subtitles.
- Ordinary Focus, Create hold to recenter and Create tap for objectives. The corrected death screen is visible in the headset.
- Exercised pickup, carry, put-down and drawer opening. The latest candidate moves L1 selection to the left controller and removes the permanent head-gaze ring; that replacement still needs a headset check.

## Still in development

Manual reload is experimental, with scoped headset acceptance of the opt-in `-ManualReload` flow: left Square opens/closes without automatic loading, hands remain tracked, the free-hand trigger carries/releases successive cartridges, insertion haptics work and weapon/menu interruptions recover. Native one-round transfers have correlated live evidence on a right-hand Peacemaker. Exact loading-port clearance, individual chamber loading, broader recovery, prolonged comfort and mirrored/Frontier contexts still need qualification; ejection remains future work. See [validation and remaining gates](docs/VALIDATION.md#persistent-manual-reload-candidate).

The latest visual correction imports a [CC0 cartridge mesh](assets/models/README.md) and anchors it to verified support-finger joints. Its held-trigger pinch and stereo rendering have host tests; appearance/contact still await a fresh headset check after rejection of the previous proxy presentation.

Broader weapons, horses, duels, climbing and recovery across campaign transitions remain separate checks. Protected-target warning visibility and compatibility with Steam recording are unresolved. Full connected-arm/body IK is not the active presentation.

Only Call of Juarez (2006) is under active development. Bound in Blood and Gunslinger are planned; D3D9/OpenVR remains the primary path, with OpenXR and D3D10 separate future or experimental tracks.

## Documentation

[Validation and current limits](docs/VALIDATION.md) Â·
[Roadmap](docs/ROADMAP.md) Â·
[Architecture](docs/ARCHITECTURE.md) Â·
[Default Sense controls](docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls)

<details>
<summary><strong>Development</strong></summary>

Windows/x86 targets require Visual Studio 2022 or Build Tools with C++ support and CMake 3.25+. Use [OpenVR bootstrap](tools/bootstrap_openvr.ps1) before configuring a fresh checkout; [OpenXR bootstrap](tools/bootstrap_openxr.ps1) supplies the optional experimental runtime.

```powershell
cmake --preset win32-debug
cmake --build --preset release
ctest --preset release
```

For physical candidates use `tools/vr_test.ps1 prepare` and `finish`. SteamVR and the game are started manually. Read [AGENTS.md](AGENTS.md) before changing integration or validation state.

</details>

## Disclaimer

An unofficial community project, not affiliated with or endorsed by Techland, Ubisoft, Valve or Sony Interactive Entertainment.
