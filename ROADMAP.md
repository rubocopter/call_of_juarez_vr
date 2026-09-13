# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`,
`headset-validated`, `supported`.

## Milestone 0 — repository and evidence

- Win32 CMake project: **host-tested**.
- Exact-build SHA-256 catalog: **host-tested**.
- Runtime game classification: **host-tested**.
- SHA-256 implementation: **host-tested**.
- D3D9 availability probe: **host-tested**.
- Initial architecture/research documentation: **implemented**.

## Milestone 1 — transparent renderer bootstrap

- D3D9 forwarding bootstrap beside Call of Juarez: **planned**.
- Verify unchanged non-VR rendering and clean unload: **planned**.
- Log exact host build and D3D9 device creation: **planned**.
- Repeat forwarding-only test on Bound in Blood and Gunslinger: **planned**.

## Milestone 2 — OpenXR and first HMD proof

- Add OpenXR loader dependency and runtime session layer: **planned**.
- Create D3D9-compatible eye render path or justified translation boundary: **planned**.
- Rotational HMD tracking / 3DOF camera proof in Call of Juarez: **planned**.
- Stereo eye projection and headset submission: **planned**.

## Milestone 3 — full 6DOF and comfort

- Positional tracking and room-scale reconciliation: **planned**.
- Culling/visibility corrections: **planned**.
- HUD/menu strategy: **planned**.
- Cinematic and post-process handling: **planned**.
- Head/body/camera ownership and comfort validation: **planned**.

## Milestone 4 — controllers and interactions

- Logical OpenXR actions and controller profiles: **planned**.
- Decouple weapon aim from HMD view: **planned**.
- Motion-controlled guns/reload/interactions where game boundaries permit: **planned**.
- Per-game weapon/player adapters: **planned**.

## Milestone 5 — additional renderers/games

- Call of Juarez D3D10 backend: **planned**.
- Bound in Blood full VR backend: **planned**.
- Gunslinger full VR backend: **planned**.

## Parallel renderer research

- Determine whether engine passes expose reusable motion vectors/depth: **planned**.
- Evaluate modern renderer bridge suitable for DLSS/DLAA: **planned**.
- Evaluate DLAA per-eye quality/performance after stable stereo exists: **planned**.
