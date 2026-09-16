# Codex objective — Call of Juarez VR

Use this objective for the current implementation pass.

## Product goal

Turn Call of Juarez (2006) into a native-feeling PCVR game. The supported end state must provide:

- native stereo rendering from the game/engine render path;
- full-body IK driven by validated VR tracking anchors;
- interactions rebuilt for VR, including tracked weapons/hands and motion-controller interaction semantics instead of a direct flat-input remap.

Call of Juarez (2006) remains the reference implementation. Shared contracts may be promoted only after another game independently demonstrates the same boundary.

## Current gate

The exact `Camera -> View/Projection -> ChromeEngine3 renderer` path is proven. HMD yaw/pitch direction, the native right/up/forward basis, two complete `0x30FB0` eye passes, distinct real-color left/right captures and OpenVR stereo submission are all live-tested in the exact Call of Juarez build. Run `20260916T133322Z-36c287cc43d8` supplied the distinct-eye proof, but binocular fusion/comfort and frame pacing failed acceptance and the process did not emit clean runtime finalization.

Static game data subsequently proved that Call of Juarez movement/world units are centimetres while the shared XR runtime uses metres. The live stereo run therefore used an eye baseline 100x too small. Current source fixes that conversion only in the Call of Juarez adapter, records per-eye applied position/frustum/viewport and transport timing, adds shutdown-stage telemetry, and exposes a minimal OpenVR global recenter action bound to left PS VR2 Sense Create. These corrections are host-tested only.

The active task is one fresh single-process physical gate proving the corrected `100` game-units-per-metre eye baseline, usable binocular fusion, valid per-eye geometry, left-Sense-Create recenter without Alt-Tab, distinct eye submission and clean runtime shutdown/`run_end`. Positional 6DOF, controller gameplay, full-body IK and rebuilt interactions remain subsequent product milestones.

## Execution order

1. Pass the corrected-scale native-stereo manual gate with usable binocular fusion, valid per-eye geometry, in-headset recenter and clean finalization.
2. Use the live evidence to harden/optimize per-eye rendering, culling, render-target transport and frame pacing while preserving temporary engine state.
3. Add positional HMD tracking and room-scale/player-body reconciliation.
4. Add tracked controller input, weapon/hand ownership and interaction rebuilding.
5. Add full-body IK after the head/controller/body-anchor contracts are stable.

Keep D3D10 as a later first-class Call of Juarez renderer target. Do not generalize exact Chrome Engine 3 layouts or RVAs to Bound in Blood or Gunslinger without independent proof.

## Local manual VR-test workflow

The user must be able to run the current candidate without waiting for an agent. Use `tools/vr_test.ps1` as the front door:

```powershell
pwsh -File tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez"
# Start SteamVR and Call of Juarez manually.
# Normal recenter while wearing the headset: press Create on the left PS VR2 Sense.
# Terminal fallback for diagnostics only:
pwsh -File tools/vr_test.ps1 recenter
pwsh -File tools/vr_test.ps1 disable
# Close the game normally.
pwsh -File tools/vr_test.ps1 finish
```

The first explicit game directory is stored under ignored `work/` state so later commands can omit it. `prepare` must build/test, create exact provenance and stage the current native-stereo candidate. The script must never launch or terminate SteamVR or Call of Juarez.

## Engineering constraints

- Inspect the actual repository/working tree before continuing; do not assume documented work is still pending.
- Exact binary identity controls game-specific integration. Unknown builds fail closed.
- Preserve the existing audit-remediation evidence and validation-state vocabulary.
- Keep OpenVR -> SteamVR as the primary PSVR2 path for the current implementation; keep OpenXR independently selectable and experimental/future.
- Camera orientation and per-eye state are transient: the game camera remains authoritative, HMD/eye pose is an offset, and source plus derived camera state is restored around the relevant engine/render-view work.
- The exact source matrix is right/up/forward/position. Never reconstruct the first axis as `forward x up`; reflected or non-rigid camera bases must fail closed.
- Do not use `Present`, `BeginScene`, `EndScene` or `Reset` as the HMD/native-stereo ownership boundary.
- Do not use blind periodic re-hooking or reactivate D3D9Ex as the primary path without new evidence.
- Never launch Call of Juarez or SteamVR automatically.
- Treat one staged run ID as one game-process execution. Prepare a fresh run before relaunching the game; provenance rejects multiple `run_start` records for the same run ID.

## Evidence discipline

For every meaningful change, inspect the owning code, make the smallest coherent change, run the relevant host tests, build the exact candidate, preserve source/build/deployment/run identity and update `docs/VALIDATION.md` plus `docs/internal/CODEX_HANDOFF.md` with actual evidence. A host-tested candidate is not live-tested until the corresponding manual run passes its visual/physical acceptance gate.
