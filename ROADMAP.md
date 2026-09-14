# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`.

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

- Pinned OpenVR SDK 2.15.6 bootstrap/build integration: **host-tested**.
- OpenVR runtime lifecycle, standing tracking space and neutral eye/HMD conversion: **live-tested** against SteamVR/PSVR2.
- OpenVR-selected D3D11 device and stereo compositor submission backend: **live-tested** for isolated synthetic submission.
- Isolated OpenVR runtime/eye/pose/submission probe: **live-tested**; headset-visible confirmation remains separate.
- D3D9Ex -> D3D11 shared render-target transport: **host-tested**.
- Direct classic-D3D9 shared render targets: **host-tested unsupported** on the development host (`D3DERR_INVALIDCALL`).
- Opt-in classic-API -> D3D9Ex proxy bridge: **host-tested; live-rejected** after an engine access violation in the exact game build.
- Classic D3D9 -> CPU readback -> D3D11 upload fallback: **live-tested**.
- Validate classic-D3D9 readback in Call of Juarez with unchanged flat rendering: **live-tested**.
- Validate OpenVR runtime/eye configuration against manually started SteamVR: **live-tested**.
- In-game classic-D3D9 -> CPU -> D3D11 -> OpenVR flat submission: **live-tested for the first three genuine game frames**. Readback, upload, `WaitForHmdPose` and stereo submission all complete successfully for those frames.
- Diagnose sustained D3D9 interception loss: **live-tested root symptom**. After exactly three `Present`, `BeginScene` and `EndScene` callbacks, the installed device-vtable hooks are overwritten while the game continues rendering normally on the monitor.
- Identify which D3D9 vtable slots are replaced and which loaded module owns each replacement target: **implemented / host-tested; next live gate**. Release candidate `369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF` builds cleanly and the Release suite passes 12/12 CTests.
- Choose a durable interception strategy from that evidence: **planned**. Prefer chaining after a known third-party hook when justified; otherwise investigate engine/runtime vtable restoration. If native-vtable ownership remains unstable, move to an owned `IDirect3DDevice9` forwarding wrapper instead of periodic re-hooking.
- Sustain at least 300 captured/submitted game frames before promoting the flat bridge: **planned validation gate**.
- Confirm sustained game image in the headset: **planned headset gate**.
- Rotational HMD tracking / 3DOF camera proof in Call of Juarez: **planned**.
- Stereo eye projection in Call of Juarez: **planned**.

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
