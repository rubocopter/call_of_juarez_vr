# Call of Juarez VR

<p align="center">
  <img alt="Status: pre-alpha" src="https://img.shields.io/badge/status-pre--alpha-orange?style=flat-square">
</p>
<p align="center">
  <a href="https://ko-fi.com/onitaku"><img alt="Support me on Ko-fi" src="https://ko-fi.com/img/githubbutton_sm.svg"></a>
</p>

**An experimental native-PCVR conversion for Techland's Call of Juarez games.**

Call of Juarez (2006) is the reference implementation. The target is a native-feeling VR conversion with stereo rendering from the game engine, tracked head and hands, full-body IK and interactions rebuilt around motion controllers.

> **Pre-alpha — no public release yet.** The game already renders native stereo to SteamVR and is playable in-headset, but controller UI, locomotion comfort, weapon ownership and body IK are still being validated.

## Current state

- Native D3D9 stereo reaches SteamVR with real per-eye rendering, physical eye separation and explicit render-pose submission.
- HMD yaw/pitch/roll, positional camera offset, recenter and exact ±45° Sense snap turn have physical validation.
- Startup/menu/loading presentation can fall back to an anchored flat theater and return to native stereo gameplay.
- PS VR2 Sense poses reach the game-specific hand/body layer; local Ray/Billy head and hair suppression is physically validated in-headset.
- Body-arm writing works against the live skeleton and restores cleanly, but anatomy/reach still fails the visual acceptance gate.
- The latest menu, physical-crouch, telemetry, and controller-owned shot-origin fixes pass the full host suites but have **not** had a new headset test yet.

The active physical gate is documented in [Validation](docs/VALIDATION.md). The current implementation checkpoint is in [Codex handoff](docs/internal/CODEX_HANDOFF.md).

## Games

| Game | Status |
| --- | --- |
| **Call of Juarez (2006)** | Active reference implementation |
| **Call of Juarez: Bound in Blood** | Planned; no Chrome Engine assumptions promoted yet |
| **Call of Juarez: Gunslinger** | Planned; no Chrome Engine assumptions promoted yet |

## Documentation

[Roadmap](ROADMAP.md) · [Architecture](ARCHITECTURE.md) · [Validation](docs/VALIDATION.md) · [Technical audit](docs/TECHNICAL_AUDIT.md) · [Research notes](docs/RESEARCH_NOTES.md)

<details>
<summary><strong>Developer quick start</strong></summary>

Windows x86/Win32, Visual Studio 2022 / Build Tools and CMake 3.25+ are currently required.

```powershell
cmake --preset win32-debug
cmake --build --preset debug
ctest --preset debug
```

For a physical candidate use `tools/vr_test.ps1`. SteamVR and Call of Juarez are always launched manually. Read [AGENTS.md](AGENTS.md) before changing runtime integration or promoting validation state.

</details>