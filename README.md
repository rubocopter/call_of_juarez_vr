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

Call of Juarez (2006) is the reference implementation. The target is a native-feeling VR conversion with stereo rendering from the game engine, tracked head and hands, full-body IK and interactions rebuilt around motion controllers.

> **Pre-alpha — no public release yet.** Native stereo and the core HMD path work in-headset, but the current playability candidate is still physically rejected for controller UI, locomotion/jump, continuous arm IK and weapon alignment.

## Current state

- Native D3D9 stereo reaches SteamVR with real per-eye rendering, physical eye separation and explicit render-pose submission.
- HMD yaw/pitch/roll, positional camera offset, recenter and exact ±45° Sense snap turn have physical validation.
- Startup/menu/loading presentation can fall back to an anchored flat theater and return to native stereo gameplay.
- PS VR2 Sense poses reach the game-specific hand/body layer; local Ray/Billy head and hair suppression is physically validated in-headset.
- Latest complete physical run: `20260921T192639Z-1d70905cb4f8`. The menu pointer remained uncontrollable, walking/jump remained unacceptable, arm safety caused visible fallback to native poses, and weapon shots were still misaligned.
- That run also confirmed horizontal-only room-scale body compensation with zero sampled vertical pelvis offset and strongly confirmed local `-Z` as the Sense tip direction. Neither result promotes the failed playability gates.
- Classic-D3D9 CPU readback remains a measured performance blocker: 7.112 ms median / 9.056 ms p95 at 1920x1080 per eye in the latest complete run.
- Current Debug and Release host suites each pass **24 tests + 1 expected classic-D3D9 capability skip**.

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
