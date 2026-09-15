# Call of Juarez VR

**Experimental PCVR framework for Techland's Call of Juarez games.**

Call of Juarez VR targets **Call of Juarez (2006)** and related Chrome Engine titles through a shared VR runtime, renderer backends and narrow per-game integrations.

> **Status: pre-alpha / stabilization.** Call of Juarez (2006) on Direct3D 9 is the reference implementation. D3D9 capture, D3D11 upload, OpenVR/SteamVR initialization, PSVR2 pose acquisition and initial compositor submission have all been demonstrated. A current diagnostic also confirmed loss of integrity of the installed D3D9 device-vtable hooks after three observed frame callbacks while monitor rendering continued. That is a confirmed failure mode of the present interception design, not yet a complete root-cause explanation for the blank headset. The project is now executing an audit-driven stabilization pass before camera, stereo or controller work.

[Technical audit](docs/TECHNICAL_AUDIT.md) · [Remediation plan](docs/AUDIT_REMEDIATION_PLAN.md) · [Roadmap](ROADMAP.md) · [Architecture](ARCHITECTURE.md) · [Validation](docs/VALIDATION.md) · [Research notes](docs/RESEARCH_NOTES.md)

## Scope

| Game | Engine | Renderer focus | State |
| --- | --- | --- | --- |
| **Call of Juarez (2006)** | Chrome Engine 3 | D3D9 first; D3D10 retained | Active reference implementation |
| **Call of Juarez: Bound in Blood** | Chrome Engine 4 | D3D9 | Planned |
| **Call of Juarez: Gunslinger** | Later Chrome Engine branch | D3D9 | Planned |

Shared contracts are promoted only when evidence from more than one game supports them. Camera, player, weapon and exact-build behavior remain game-specific until proven otherwise.

## Current checkpoint

The reference game has demonstrated the basic flat transport chain:

`D3D9 backbuffer -> CPU readback -> D3D11 texture -> OpenVR -> SteamVR`

The isolated OpenVR runtime initializes against SteamVR, acquires a valid PSVR2 HMD pose and accepts D3D11 eye submissions. The in-game flat bridge has also completed real submissions from captured game frames.

Audit-remediation Phases 0-4 are host-tested and have now been exercised in two run-bound manual observations. Both reproduced the same three-frame loss of the four instrumented device hooks; the second proved that those slots return to their original Windows D3D9 targets while the factory and swapchain hooks remain owned. Because Steam's `gameoverlayrenderer.dll` owned the factory `CreateDevice` target before the project hook, the next evidence gate is an otherwise identical run with Steam Overlay disabled and telemetry confirmation that the overlay factory hook is absent. Headset-visible confirmation is not required for this gate.

See the [technical audit](docs/TECHNICAL_AUDIT.md) for the current findings and the [audit remediation plan](docs/AUDIT_REMEDIATION_PLAN.md) for the required execution order.

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
