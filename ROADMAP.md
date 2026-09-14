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

- D3D9 forwarding bootstrap for Call of Juarez: **live-tested**.
- Verify unchanged non-VR rendering and clean unload: **live-tested**.
- Log exact host build and D3D9 device creation: **live-tested**.
- Observe D3D9 `Present` / `Reset` without replacing the native device object: **live-tested**.
- Repeat forwarding-only test on Bound in Blood and Gunslinger: **planned**.

## Milestone 2 — OpenVR/SteamVR and first HMD proof

- Pinned OpenVR SDK 2.15.6 bootstrap/build integration: **host-tested** for the build integration; standalone bootstrap execution still needs an independent run.
- OpenVR runtime lifecycle, standing tracking space and neutral eye/HMD conversion: **live-tested** against SteamVR/PSVR2.
- OpenVR-selected D3D11 device and stereo compositor submission backend: **live-tested** for isolated synthetic submission.
- Isolated OpenVR runtime/eye/pose/submission probe: **live-tested**; headset-visible confirmation pending.
- D3D9Ex -> D3D11 shared render-target transport: **host-tested**.
- Direct classic-D3D9 shared render targets: **host-tested unsupported** on the development host (`D3DERR_INVALIDCALL`).
- Opt-in classic-API -> D3D9Ex proxy bridge with shared render-target capability: **host-tested; live-test failed** (engine access violation after the first observed `Present`).
- Classic D3D9 -> CPU readback -> D3D11 upload fallback: **live-tested**.
- Validate the classic-D3D9 readback path in the exact Call of Juarez build with unchanged flat rendering: **live-tested**.
- Validate OpenVR runtime/eye configuration against manually started SteamVR: **live-tested**.
- Sustained in-game classic-D3D9 -> CPU -> D3D11 -> OpenVR flat submission bridge: **host-tested; live/headset gate incomplete**. The exact-build `6589B445...68621` run reproduced the same monitor-only result but closed the lifecycle question: after exactly three `Present`, `BeginScene` and `EndScene` callbacks, the installed device-vtable hook entries were observed as overwritten while the game continued rendering on the monitor. The OpenVR bridge is therefore losing its D3D9 interception rather than stalling inside submission or `Present`. Candidate `369754A6...A14A6AF` now records which individual slots are replaced and which loaded module owns each replacement target; it builds cleanly and passes 12/12 Release CTests. The >=300-frame live/headset gate remains incomplete.
- Rotational HMD tracking / 3DOF camera proof in Call of Juarez: **planned**.
- Stereo eye projection and headset submission in Call of Juarez: **planned**.

### Experimental OpenXR track

- Pinned OpenXR.Loader 1.1.63 bootstrap/build integration: **host-tested**.
- OpenXR instance/system/session, frame timing, D3D11 binding and stereo swapchains: **implemented**.
- SteamVR runtime/session/swapchain creation plus one projection-frame submission: **live-tested** runtime evidence only; no headset-visible/focused validation.

## Milestone 3 — full 6DOF and comfort

- Positional tracking and room-scale reconciliation: **planned**.
- Culling/visibility corrections: **planned**.
- HUD/menu strategy: **planned**.
- Cinematic and post-process handling: **planned**.
- Head/body/camera ownership and comfort validation: **planned**.

## Milestone 4 — controllers and interactions

- Logical OpenVR actions and controller profiles: **planned**.
- PS VR2 Sense OpenVR/SteamVR bindings and controller validation: **planned**.
- Decouple weapon aim from HMD view: **planned**.
- Motion-controlled guns/reload/interactions where game boundaries permit: **planned**.
- Per-game weapon/player adapters: **planned**.

## Milestone 5 — additional renderers/games

- Call of Juarez D3D10 backend: **planned**.
- Bound in Blood full VR backend: **planned**.
- Gunslinger full VR backend: **planned**.
