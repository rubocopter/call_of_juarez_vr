# Codex handoff — Call of Juarez VR

## Current checkpoint

Call of Juarez (2006), Direct3D 9, remains the reference implementation. OpenVR -> SteamVR is the initial PSVR2 path; OpenXR remains experimental/future.

The renderer/runtime transport proof is substantially complete. The current blocker is no longer D3D9 readback, D3D11 upload or OpenVR submission. Exact-build live evidence shows that the installed D3D9 device-vtable hooks are replaced after the third frame while the game continues rendering normally on the monitor.

Do not skip to camera, stereo or motion-controller work until sustained renderer interception is demonstrated.

## Architecture rules

- Keep game-neutral VR policy in the shared runtime.
- Keep D3D9/D3D10 device, frame, transport and submission behavior in renderer backends.
- Keep camera/player/weapon/UI/physics and exact-build knowledge in game backends.
- Call of Juarez is the reference implementation; do not generalize Chrome Engine behavior until another game demonstrates the same contract.
- Unknown executable builds fail closed for game-specific modifications.
- Preserve the original classic D3D9 device unless new evidence justifies a different path.

Read `AGENTS.md`, `ARCHITECTURE.md`, `ROADMAP.md` and `docs/VALIDATION.md` before changing behavior.

## Implemented and established

### Repository/runtime

- Win32 CMake project and presets.
- Exact executable SHA-256 catalog and host identity helpers.
- Renderer-neutral pose/eye types.
- D3D9 availability and runtime probes.
- Validation tooling and reversible staging scripts.

### D3D9 integration

- forwarding `d3d9.dll` proxy around system `Direct3DCreate9`;
- `CreateDevice` diagnostics;
- device-vtable observation hooks for `Reset`, `Present`, `BeginScene` and `EndScene`;
- host smoke coverage for forwarding, device creation, rendering and hook behavior;
- classic D3D9 -> CPU readback -> D3D11 upload bridge;
- D3D9Ex -> D3D11 shared-resource research harness;
- opt-in classic-API -> D3D9Ex substitution diagnostic;
- sustained flat-game OpenVR bridge packaged as `d3d9_openvr_flat.dll`.

### OpenVR

- pinned Valve OpenVR SDK 2.15.6;
- OpenVR runtime lifecycle and standing tracking space;
- eye configuration and HMD pose conversion;
- runtime-selected D3D11 adapter/device;
- D3D11 compositor submission;
- isolated runtime/pose/submission probe;
- same-game-frame-to-both-eyes flat submission path for the in-game transport gate.

### OpenXR

- pinned OpenXR.Loader 1.1.63;
- instance/system/session lifecycle;
- D3D11 graphics binding and adapter-LUID selection;
- frame timing, stereo swapchains and projection layers;
- experimental SteamVR runtime submission evidence.

## Validation summary

The authoritative detailed evidence trail is `docs/VALIDATION.md`. Current high-value facts:

- D3D9 forwarding/bootstrap is **live-tested** on the exact Call of Juarez build.
- Native D3D9 observation is **live-tested**.
- Classic D3D9 -> CPU readback -> D3D11 upload is **live-tested** in the exact game build and preserves normal menus/gameplay, videos/animations, sound and controls.
- Direct classic-D3D9 shared render targets are unsupported on the development host (`D3DERR_INVALIDCALL`).
- D3D9Ex -> D3D11 shared-resource transport is **host-tested**, but substituting the real game device with D3D9Ex failed its live test and is not the active path.
- OpenVR runtime/eye setup, valid PSVR2 HMD pose acquisition and isolated synthetic D3D11 submission are **live-tested** against manually started SteamVR.
- The in-game flat bridge has completed genuine game-frame submissions through D3D9 readback, D3D11 upload, `WaitForHmdPose` and stereo `Submit`.
- The exact-build runs consistently stop producing our callbacks after frame 3 while the monitor continues rendering.
- Candidate `6589B445...68621` proved that the device-vtable hook entries themselves are overwritten after exactly three `Present`, `BeginScene` and `EndScene` callbacks.
- This rules out a stall inside the bridge callback or original `Present` as the active blocking hypothesis.

## Active diagnostic

The active candidate inspects each installed device-vtable slot individually:

- `Reset`
- `Present`
- `BeginScene`
- `EndScene`

When a slot is replaced, it resolves the replacement address to the loaded module that owns it. It does not re-hook and does not issue rendering calls from the observer thread.

Release SHA-256:

`369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF`

The complete Release build succeeds and the Release suite passes 12/12 CTests.

## Next gate

Run the active candidate on the exact Call of Juarez D3D9 build with SteamVR/PSVR2 using the existing manual validation policy. The required evidence is the `d3d9 device hook continuity: overwritten ...` line showing the state and owning module for each slot.

Interpret the result as follows:

1. **Known overlay/hook module owns replacements** — decide whether to disable it for validation or chain after it.
2. **D3D9/engine-owned code owns replacements** — investigate the engine/runtime lifecycle that restores or replaces the native vtable.
3. **Native-vtable ownership is inherently unstable** — prefer a real owned `IDirect3DDevice9` forwarding wrapper returned to Chrome Engine instead of periodically re-installing hooks into the native/shared vtable.

Do not implement a re-hook polling loop as the default solution without evidence that this is the correct ownership model.

After a durable interception strategy is implemented, require at least **300 sustained captured/submitted game frames** before promoting the flat bridge. Then obtain explicit headset-visible confirmation.

Only after that gate should work proceed to:

- rotational HMD tracking / 3DOF camera proof;
- stereo per-eye projection;
- 6DOF/room-scale reconciliation;
- UI/cinematics/post-processing;
- PS VR2 Sense actions, weapon decoupling and motion interaction.

## Manual runtime policy

Never launch Call of Juarez or SteamVR automatically. The user performs all game and SteamVR launches manually.

Before requesting a runtime test:

1. build the exact candidate;
2. run the smallest meaningful host tests;
3. stage it reversibly;
4. provide or update the post-run verifier;
5. record the candidate hash;
6. after the user closes the game, inspect the evidence and update documentation without overstating the validation level.

Do not repeat historical candidates as new evidence unless the hypothesis or deployed artifact has materially changed.

## Renderer/game follow-up

Call of Juarez D3D10 remains a first-class follow-up because it provides visible rendering improvements. Bound in Blood and Gunslinger remain planned D3D9 backends, but only genuinely demonstrated shared contracts should move into the common layer.
