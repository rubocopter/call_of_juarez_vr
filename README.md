# Call of Juarez VR

<p align="center">
  <img alt="Status: pre-alpha" src="https://img.shields.io/badge/status-pre--alpha-orange?style=flat-square">
</p>
<p align="center">
  <a href="https://ko-fi.com/onitaku"><img alt="Support me on Ko-fi" src="https://ko-fi.com/img/githubbutton_sm.svg"></a>
</p>

**An experimental native-PCVR conversion for Techland's Call of Juarez games.**

The project currently uses **Call of Juarez (2006)** as its reference game. The goal is a proper VR experience with native stereo rendering, tracked head and hands, full-body IK and interactions rebuilt around VR.

> **Pre-alpha — no public release yet.** Native stereo gameplay is already visible in PS VR2 with validated head rotation, eye separation, SteamVR presentation and left-Sense recenter. The current priority is making presentation smooth and comfortable enough for sustained play.

## What works today

- Native left/right ChromeEngine rendering reaches SteamVR.
- PS VR2 head tracking and physical eye separation are working in-game.
- Left PS VR2 Sense **Create** recenters the VR view.
- Both Sense controller poses reach the body-tracking layer.
- Runtime focus handoff, explicit render-pose submission and clean shutdown have live validation.

## What is still missing

- Comfortable sustained frame pacing on the current D3D9 transport.
- Full positional/body integration: the code exists, but campaign actor discovery currently blocks in-game IK validation.
- VR presentation for flat menus.
- Motion-controlled weapons, reloads and other rebuilt VR interactions.

## Games

| Game | Status |
| --- | --- |
| **Call of Juarez (2006)** | Active reference implementation |
| **Call of Juarez: Bound in Blood** | Planned |
| **Call of Juarez: Gunslinger** | Planned |

## Project documentation

[Roadmap](ROADMAP.md) · [Architecture](ARCHITECTURE.md) · [Validation](docs/VALIDATION.md) · [Technical audit](docs/TECHNICAL_AUDIT.md) · [Research notes](docs/RESEARCH_NOTES.md)

<details>
<summary><strong>Developer quick start</strong></summary>

Native targets are Windows x86/Win32 and currently build with Visual Studio 2022 / Build Tools plus CMake 3.25+.

```powershell
cmake --preset win32-debug
cmake --build --preset debug
ctest --preset debug
```

Runtime testing is evidence-driven. SteamVR and Call of Juarez are always launched manually. See [AGENTS.md](AGENTS.md) and the [validation record](docs/VALIDATION.md) before promoting a runtime result.

</details>
