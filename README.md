# Call of Juarez VR

**Experimental PCVR framework for Techland's Call of Juarez games.**

Call of Juarez VR targets **Call of Juarez (2006)** and related Chrome Engine titles through a shared VR runtime, renderer backends and narrow per-game integrations.

The target experience for a supported game is **native stereo rendering, full-body IK and interactions rebuilt for VR**. Those are product goals; the current implementation is still progressing through the camera/tracking and renderer gates required to reach them safely.

> **Status: pre-alpha / stabilization.** Call of Juarez (2006) on Direct3D 9 is the reference implementation. Native ChromeEngine stereo, corrected centimetre-scale eye separation, PSVR2 head tracking and left-Sense-Create recenter are physically validated. The deferred capture/mailbox/presenter path hands SteamVR scene focus over only when a real stereo frame exists, shuts down cleanly, and submits each new or repeated texture with the exact HMD render pose; run `20260916T224239Z-e43b46698e5c` removed the previous head-turn snap-back. Phase 5 capture/presenter acceptance and Phase 6 OpenVR state/ownership/synchronization are host-tested. The current blocker is sustained frame pacing/performance. The flat menu still needs a separate VR presentation path, while positional 6DOF, full-body IK and rebuilt VR interactions remain later milestones.

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

Audit-remediation Phases 0-4 are host-tested and two run-bound observations reproduced the same three-frame loss of the four instrumented D3D9 device hooks. That Steam Overlay A/B remains a deferred renderer investigation. The HMD pose path reaches the visible first-person camera. Runs through `20260916T224239Z-e43b46698e5c` prove two complete ChromeEngine eye passes, distinct real-color capture, corrected metre-to-centimetre eye separation, physical Sense recenter, SteamVR scene-focus handoff, clean runtime teardown and explicit render-pose submission. The active transport uses a game-thread D3D9 capture ring, owned CPU stereo frames, a bounded mailbox and a dedicated D3D11/OpenVR presenter. Phase 5 and Phase 6 host acceptance are complete; fresh Debug and Release suites each produce 23 PASS plus the expected classic-D3D9 shared-texture capability SKIP. The next physical run is deliberately consolidated around sustained frame pacing/performance while rechecking runtime state, repeated presentation, one SteamVR dashboard focus-loss/reacquisition cycle, recenter and clean shutdown in the same process. Flat menu presentation is a separate later UI boundary.

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
