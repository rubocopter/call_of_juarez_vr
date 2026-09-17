# Call of Juarez VR

<p align="center">
  <img alt="Status: pre-alpha" src="https://img.shields.io/badge/status-pre--alpha-orange?style=flat-square">
</p>
<p align="center">
  <a href="https://ko-fi.com/onitaku"><img alt="Support me on Ko-fi" src="https://ko-fi.com/img/githubbutton_sm.svg"></a>
</p>

**Experimental PCVR conversion project for Techland's Call of Juarez games.**

The project currently focuses on **Call of Juarez (2006)** as the reference game, with the longer-term goal of supporting other games in the series through shared VR systems and game-specific integrations.

The target experience is **native stereo rendering, tracked head and hands, full-body IK and interactions rebuilt for VR**. This is still a development project rather than a finished, user-ready mod.

> **Status: pre-alpha / stabilization.** Native stereo gameplay is visible in PSVR2 with working head rotation, correct eye separation, SteamVR scene handoff, render-pose submission and left-Sense-Create recenter. The main blocker is still frame pacing/performance. Both Sense controllers are tracked, and positional/body IK groundwork is implemented and host-tested, but campaign player discovery currently prevents the body/arm path from being validated in-game. Flat menus and VR-native gameplay interactions are also still unfinished.

[Technical audit](docs/TECHNICAL_AUDIT.md) · [Remediation plan](docs/AUDIT_REMEDIATION_PLAN.md) · [Roadmap](ROADMAP.md) · [Architecture](ARCHITECTURE.md) · [Validation](docs/VALIDATION.md) · [Research notes](docs/RESEARCH_NOTES.md)

## Scope

| Game | Engine | Renderer focus | State |
| --- | --- | --- | --- |
| **Call of Juarez (2006)** | Chrome Engine 3 | D3D9 first; D3D10 retained | Active pre-alpha reference implementation |
| **Call of Juarez: Bound in Blood** | Chrome Engine 4 | D3D9 | Planned |
| **Call of Juarez: Gunslinger** | Later Chrome Engine branch | D3D9 | Planned |

Shared contracts are promoted only when evidence from more than one game supports them. Camera, player, weapon and exact-build behavior remain game-specific until proven otherwise.

## Current state

What already works in the reference game:

- Native left/right ChromeEngine rendering reaches SteamVR and produces visible binocular gameplay in PSVR2.
- Head yaw/pitch, physical eye separation and render-pose reprojection are validated in-headset.
- Left PS VR2 Sense Create recenters the VR view without using the keyboard.
- SteamVR scene focus can hand over correctly to the game and the runtime has completed clean shutdown in validated runs.
- Both Sense controller poses are available to the body-tracking layer.

What still prevents a usable release:

- The current classic-D3D9 transport is too expensive for consistently comfortable frame pacing. The active performance candidate temporarily tests the game at `1920x1080` with FSAA disabled and restores the user's original settings afterwards.
- Campaign player discovery currently exposes no usable actor through the known Java session paths, so positional body reconciliation and arm IK cannot yet be promoted from host-tested code to a working in-game feature. When the actor cannot be resolved, physical head translation is suppressed instead of allowing the VR camera to drift away from the character body.
- The flat game menu is not yet presented through the native-stereo gameplay path.
- Motion-controlled weapons, reloads and other VR-native interactions are still planned.

Fresh Debug and Release validation each report **24 PASS plus one expected classic-D3D9 shared-texture capability SKIP out of 25 CTests**. Detailed run evidence and validation levels are tracked in [Validation](docs/VALIDATION.md); the [Roadmap](ROADMAP.md) gives the product-level status.

## Architecture

The intended product is split into:

- **Shared runtime** — game-neutral VR types and policy.
- **Renderer backends** — D3D9/D3D10 device/frame ownership, capture/transport and compositor integration.
- **Game backends** — camera, player, weapon, UI, physics and exact-build knowledge.
- **Diagnostics/evidence infrastructure** — hook ownership, device generations, structured run telemetry and source-to-run provenance.

Call of Juarez (2006) is the reference implementation used to discover the smallest reusable contracts. See [Architecture](ARCHITECTURE.md) for the design rules.

## Development

Native targets are currently **Windows x86/Win32**. Development requires Visual Studio 2022 or Build Tools with the C++ workload and CMake 3.25+.

```powershell
cmake --preset win32-debug
cmake --build --preset debug
ctest --preset debug
```

Runtime validation is evidence-driven. Successful builds and host tests are distinct from live-game and headset validation. Call of Juarez and SteamVR are launched manually; agents must prepare the exact artifact, deployment path, verifier and expected evidence before requesting a runtime test.

For repeated local HMD/native-stereo testing, the repository provides one front-door command. The first invocation stores the local game directory under ignored `work/` state; later invocations can omit it:

```powershell
pwsh -File tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez"
# Start SteamVR and Call of Juarez manually.
# Recenter while wearing the headset: press Create on the left PS VR2 Sense.
# During stable gameplay, open and close the SteamVR dashboard once.
# Exercise slow/fast head turns and mouse rotation for the performance/comfort gate.
# Terminal fallback for diagnostics only:
pwsh -File tools/vr_test.ps1 recenter
pwsh -File tools/vr_test.ps1 disable   # while the game is still running
# Close the game normally.
pwsh -File tools/vr_test.ps1 finish
```

`prepare` rebuilds Release, runs the host suite, creates an exact dirty-aware build manifest, stages the current `d3d9_native_stereo` candidate, its OpenVR action manifest/PS VR2 Sense binding and enables tracking. It never launches SteamVR or the game.

Every staged diagnostic now requires a build manifest for the exact artifact. For example:

```powershell
pwsh -File tools/new_build_manifest.ps1 -DiagnosticMode d3d9_openvr_flat -Configuration Release
pwsh -File tools/stage_d3d9_openvr_flat.ps1 -GameDirectory "C:\path\to\Call of Juarez"
pwsh -File tools/verify_d3d9_openvr_flat_live_test.ps1 -GameDirectory "C:\path\to\Call of Juarez"
pwsh -File tools/collect_run_evidence.ps1 -GameDirectory "C:\path\to\Call of Juarez"
```

Staging assigns a unique run ID, preserves older logs under `.cojvr-evidence`, records the exact game/engine/proxy/runtime hashes, and writes the run identity into the proxy log. The structured verifier rejects stale, mixed, malformed or incomplete evidence and reports exact activity per device/generation, hook ownership, stage failures/stalls, repeated captured content and capture-versus-presentation clock progression.

Developer workflow is documented in [AGENTS.md](AGENTS.md). The detailed continuation state lives in [`docs/internal/CODEX_HANDOFF.md`](docs/internal/CODEX_HANDOFF.md), and the current implementation objective is recorded in [`docs/internal/CODEX_OBJECTIVE.md`](docs/internal/CODEX_OBJECTIVE.md).

## Documentation

- [Technical audit](docs/TECHNICAL_AUDIT.md) — authoritative stabilization findings and their current status.
- [Audit remediation plan](docs/AUDIT_REMEDIATION_PLAN.md) — ordered implementation phases and acceptance gates.
- [Architecture](ARCHITECTURE.md) — project boundaries and target ownership model.
- [Roadmap](ROADMAP.md) — product-level milestone status.
- [Validation](docs/VALIDATION.md) — host, live-game and headset evidence trail.
- [Research notes](docs/RESEARCH_NOTES.md) — inspected Chrome Engine builds and cross-game observations.
- [Codex handoff](docs/internal/CODEX_HANDOFF.md) — immediate continuation checkpoint.
- [Codex objective](docs/internal/CODEX_OBJECTIVE.md) — current audit-driven implementation objective.

## Support

If you want to support continued development, see [Ko-fi](https://ko-fi.com/onitaku).
