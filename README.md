# Call of Juarez VR

**Experimental PCVR framework for Techland's Call of Juarez games.**

Call of Juarez VR targets **Call of Juarez (2006)** and related Chrome Engine titles through a shared VR runtime, renderer backends and narrow per-game integrations.

> **Status: pre-alpha.** Call of Juarez (2006) on Direct3D 9 is the reference implementation. D3D9 capture, D3D11 upload, OpenVR/SteamVR initialization, PSVR2 pose acquisition and real compositor submission are live-tested. The current blocker is sustaining the D3D9 interception: after three frames, the installed device-vtable hooks are replaced while the game continues rendering normally on the monitor.

[Roadmap](ROADMAP.md) · [Architecture](ARCHITECTURE.md) · [Validation](docs/VALIDATION.md) · [Research notes](docs/RESEARCH_NOTES.md)

## Scope

| Game | Engine | Renderer focus | State |
| --- | --- | --- | --- |
| **Call of Juarez (2006)** | Chrome Engine 3 | D3D9 first; D3D10 retained | Active reference implementation |
| **Call of Juarez: Bound in Blood** | Chrome Engine 4 | D3D9 | Planned |
| **Call of Juarez: Gunslinger** | Later Chrome Engine branch | D3D9 | Planned |

Shared contracts are promoted only when evidence from more than one game supports them. Camera, player, weapon and exact-build behavior remain game-specific until proven otherwise.

## Current checkpoint

The reference game has demonstrated the transport chain:

`D3D9 backbuffer -> CPU readback -> D3D11 texture -> OpenVR -> SteamVR`

The isolated OpenVR runtime initializes against SteamVR, acquires a valid PSVR2 HMD pose and submits D3D11 eye textures. The in-game flat bridge has also completed real submissions from captured game frames.

The current evidence shows that after exactly three `Present` / `BeginScene` / `EndScene` callbacks, the installed D3D9 vtable hooks are overwritten while monitor rendering continues. The active diagnostic identifies which slots are replaced and which loaded module owns each replacement target. Camera hooks, stereo rendering and motion controls remain behind this renderer/runtime gate.

See [Validation](docs/VALIDATION.md) for the evidence trail and [Roadmap](ROADMAP.md) for the next gates.

## Architecture

The project is split into:

- **Shared runtime** — poses, eye data, tracking-space policy, logical input, haptics and VR runtime lifecycle.
- **Renderer backends** — D3D9/D3D10 device and frame boundaries, transport and compositor submission.
- **Game backends** — camera, player, weapon, UI, physics and exact-build knowledge.

Call of Juarez (2006) is the reference implementation used to discover the smallest reusable contracts. See [Architecture](ARCHITECTURE.md) for the design rules.

## Development

Native targets are currently **Windows x86/Win32**. Development requires Visual Studio 2022 or Build Tools with the C++ workload and CMake 3.25+.

```powershell
cmake --preset win32-debug
cmake --build --preset debug
ctest --preset debug
```

Runtime validation is evidence-driven. Successful builds and host tests are distinct from live-game and headset validation. Call of Juarez and SteamVR are launched manually; staging, diagnostics and post-run verification are handled by the repository tooling.

Developer workflow is documented in [AGENTS.md](AGENTS.md). The detailed continuation state lives in [`docs/internal/CODEX_HANDOFF.md`](docs/internal/CODEX_HANDOFF.md).

## Documentation

- [Architecture](ARCHITECTURE.md) — project boundaries and renderer/runtime design.
- [Roadmap](ROADMAP.md) — milestone status and next gates.
- [Validation](docs/VALIDATION.md) — host, live-game and headset evidence.
- [Research notes](docs/RESEARCH_NOTES.md) — inspected Chrome Engine builds and cross-game observations.
- [Codex handoff](docs/internal/CODEX_HANDOFF.md) — detailed internal implementation checkpoint.

## Support

If you want to support continued development, see [Ko-fi](https://ko-fi.com/onitaku).
