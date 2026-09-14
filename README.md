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

The current engineering priority is not another headset experiment. The repository is being hardened so one future execution can prove which factory/device/generation renders, whether hooks remain valid, how many **new** game frames are captured, where each bridge stage progresses or stalls, and how that activity maps to OpenVR submissions.

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
