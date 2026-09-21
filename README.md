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

**An experimental native-PCVR conversion for Techland's Call of Juarez games.**

Call of Juarez (2006) is the reference implementation. The goal is a native-feeling VR conversion with stereo rendering from the game engine, tracked head and hands, full-body integration and interactions rebuilt around motion controllers.

> **Pre-alpha — no public release yet.** Native stereo and the core HMD path are working in-headset. Current development is focused on the remaining playability and comfort blockers before the first backend can be considered usable end to end.

## Current state

Native D3D9 stereo reaches SteamVR with real per-eye rendering, tracked 6DoF head movement, positional camera offset, recentering and VR presentation transitions between menus/loading and gameplay.

PS VR2 Sense tracking already reaches the game-specific hand/body layer. The active work is now concentrated on controller-driven UI, locomotion and jump behavior, continuous arm IK, weapon alignment and the remaining performance cost of the classic-D3D9 presentation path.

Detailed physical-test evidence belongs in [Validation](docs/VALIDATION.md); the active implementation checkpoint belongs in [Codex handoff](docs/internal/CODEX_HANDOFF.md).

## Games

| Game | Project state |
| --- | --- |
| **Call of Juarez (2006)** | Active reference implementation and current physical-test target. |
| **Call of Juarez: Bound in Blood** | Planned. No Chrome Engine assumptions are promoted until the reference backend is mature enough to justify reuse. |
| **Call of Juarez: Gunslinger** | Planned. Game-specific work has not started. |

## Documentation

[Roadmap](ROADMAP.md) ·
[Architecture](ARCHITECTURE.md) ·
[Validation](docs/VALIDATION.md) ·
[Technical audit](docs/TECHNICAL_AUDIT.md) ·
[Research notes](docs/RESEARCH_NOTES.md)

The README is intentionally a project landing page. Detailed measurements, run identifiers, temporary debugging conclusions and implementation handoffs are kept in the versioned technical documents instead of being duplicated here.

<details>
<summary><strong>Development</strong></summary>

Native targets are Windows/x86. Development currently requires Visual Studio 2022 / Build Tools with C++ support and CMake 3.25+.

```powershell
cmake --preset win32-debug
cmake --build --preset debug
ctest --preset debug
```

For a physical candidate use `tools/vr_test.ps1`. SteamVR and Call of Juarez are launched manually. Read [AGENTS.md](AGENTS.md) before changing runtime integration or promoting validation state.

</details>

## Disclaimer

Call of Juarez VR is an unofficial community project and is not affiliated with or endorsed by Techland, Ubisoft, Valve or Sony Interactive Entertainment.
