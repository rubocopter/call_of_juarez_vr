# Codex handoff — Call of Juarez VR

## Current checkpoint

The repository has completed the Milestone 1 D3D9 forwarding/frame-boundary live
gates and is moving through the host-only OpenVR/renderer-transport gate. OpenVR ->
SteamVR is now the initial PSVR2 path; OpenXR remains experimental/future. The architecture follows the useful separation
learned from Penumbra VR: shared game-neutral runtime, renderer backends and
per-game adapters, with exact-build evidence kept out of shared policy.

Implemented:

- Win32 CMake project/presets;
- game/renderer IDs and exact executable SHA-256 catalog;
- CNG SHA-256 helper and current-host identity boundary;
- renderer-neutral pose/eye types;
- D3D9 availability probe;
- forwarding-only D3D9 proxy around the system `Direct3DCreate9` interface;
- `CreateDevice` diagnostics and native-vtable `Present`/`Reset` observation hooks;
- D3D9 proxy smoke coverage for loading, forwarding, device creation, `Clear`,
  `Present`, `Reset` and logs;
- pinned OpenVR SDK 2.15.6 dependency plus build integration;
- OpenVR runtime lifecycle, standing tracking space, recommended eye size,
  renderer-neutral eye configuration and HMD pose conversion;
- OpenVR-selected D3D11 device/compositor submission backend;
- isolated OpenVR probe with runtime/eye, optional pose and optional one-frame
  D3D11 submission gates;
- synthetic D3D9Ex-to-D3D11 shared-texture interoperability harness;
- explicit classic-D3D9 shared-texture capability test;
- opt-in classic-API -> D3D9Ex proxy bridge with synthetic smoke coverage;
- pinned OpenXR.Loader 1.1.63 bootstrap with exact package hash verification;
- renderer-neutral OpenXR instance/system/session, event, timing and stereo-view layer;
- D3D11 OpenXR adapter-LUID binding, stereo swapchains and projection layers;
- isolated D3D11 compositor probe that attempts 180 rendered projection frames;
- runtime host tests;
- initial architecture, roadmap, validation and research documents.

Validation on 2026-09-13:

- CMake Win32 configuration generated successfully;
- Debug Win32 MSBuild completed with 0 warnings and 0 errors;
- Release Win32 MSBuild completed with 0 warnings and 0 errors;
- `cojvr_runtime_tests.exe` passed;
- `cojvr_runtime_tests.exe` also passed in Release;
- the Release D3D9 availability probe executed successfully and detected two
  adapters;
- `cojvr_d3d9_proxy_smoke.exe` passed in Debug and Release, including assertions
  for host identity, forwarding activation, `CreateDevice`, `Present` and `Reset`;
- the installed `CoJ.exe` SHA-256 was rechecked against the known-build catalog;
- the observed `ChromeEngine3.dll` symbol scan found `Direct3DCreate9` and no
  additional common D3D9 bootstrap export names in the inspected set.
- the forwarding-only proxy was live-tested manually in the exact CoJ D3D9 build:
  a save loaded and played with normal flat rendering through the connected PSVR2,
  logging showed the exact build and successful `CreateDevice`, and unstage completed
  cleanly;
- the Release host suite now passes 12/12 CTests; the `EndScene` hook traverses
  five consecutive `BeginScene`/`EndScene` cycles in its host test;
- D3D9Ex -> D3D11 shared-resource transport passes with pixel verification;
- direct classic-D3D9 shared render-target creation returns `D3DERR_INVALIDCALL`
  on this host;
- the opt-in D3D9Ex proxy bridge passes `Clear`, `Present`, `Reset`, Ex-interface
  and shared-render-target smoke coverage in Debug and Release, but its exact-build
  live-test crashed after an observed `Present`; do not promote or restage it;
- classic D3D9 -> CPU readback -> D3D11 upload passes pixel verification in Debug
  and Release and is now **live-tested** on the exact game build; the live diagnostic
  is a separate `d3d9_readback.dll` artifact rather than a marker/env toggle;
- the separate readback diagnostic has now run in the exact live game: all log
  checks passed, including a successful 2560x1440 classic-D3D9 -> CPU -> D3D11
  transfer, and no new crash artifact appeared; the proxy was removed afterward.
  The user confirmed menus/gameplay, videos/animations, sound and controls remained
  normal and no unusual visual artifacts were noticed;
- the OpenVR D3D11 backend/probe compiles in Debug and Release and has now been
  live-tested against manually started SteamVR with PSVR2: runtime/eye setup passed,
  a valid HMD pose was returned, and one synthetic stereo D3D11 frame submitted
  successfully; headset-visible confirmation is still pending;
- the first exact-build `d3d9_openvr_flat.dll` run reached SteamVR as a scene app
  and completed one real stereo submission, but device `Present` fired only once;
  the user saw no game image in PSVR2, only normal monitor output and visor audio;
- the repeat run installed an implicit-swapchain `Present` hook, but no swapchain
  callback was observed, so that frame-boundary hypothesis is rejected for this
  exact engine path;
- `IDirect3DDevice9::EndScene` at vtable index 42 is now the active sustained-frame
  candidate. Its callback is host-tested with five consecutive real D3D9 scene
  cycles, and the OpenVR flat diagnostic now submits only from successful `EndScene`;
- the current exact-build diagnostic (`A1420BFF...`) reproduces the 3/3 cutoff with
  stronger evidence: `BeginScene=3`, `EndScene=3`, callback returns `=3`, and OpenVR
  submissions `=3`. The third callback completes D3D9 readback, D3D11 upload,
  `WaitForHmdPose`, stereo submit and the callback return. No fourth `BeginScene` is
  observed and no OpenVR failure is logged. The bridge callback is therefore no
  longer the active blocking hypothesis; the next investigation is the D3D9 frame/
  presentation lifecycle after that completed third callback;
- the earlier OpenXR SteamVR experiment created its runtime/session/swapchains and
  submitted one projection frame, but never reached headset-visible/focused evidence.

The forwarding-only bootstrap and native-vtable `Present`/`Reset` observation hook
are both **live-tested** from their earlier staged runs. After the D3D9Ex-substitution
crash, the proxy was removed and the user performed another manual vanilla run: a
save loaded, movement remained normal and the game exited cleanly. Inspection found
no staged `d3d9.dll`, backup, staging state or bridge marker, so that second run is
base-game stability evidence rather than proxy evidence. The D3D9Ex substitution is
**host-tested but live-rejected** and has not replaced the live-tested classic path.

Runtime validation policy: never launch a game or SteamVR automatically. The user
performs those launches manually. Prepare deployment and automated post-run checks
before handing off each runtime gate. The primary physical target is PlayStation
VR2 on PC with both PS VR2 Sense controllers.

## Next gate

The classic-D3D9 CPU-readback -> D3D11 gate is **live-tested**. Preserve the original
D3D9 device and do not retry the D3D9Ex substitution unless new evidence justifies it.

The isolated OpenVR runtime/pose/one-frame submission gate is now **live-tested**.
The older `EndScene`-driven diagnostic that first produced the 3/3 evidence was
cleanly unstaged before `A1420BFF...` was deployed and live-exercised on the exact
build. That candidate is now historical evidence rather than the active diagnostic.

A narrower diagnostic successor was built, host-tested, staged and live-exercised. Its Release DLL
SHA-256 is
`A1420BFFA5B09C16CC586917491841BCC945ED858C00D232FC5667892E0A19B8`.
It hooks successful `BeginScene` calls and records `EndScene` callback returns, using
the same 2-through-10, 60 and 300 milestones. It also traces the potentially blocking
bridge phases around D3D9 readback, D3D11 upload, `WaitForHmdPose` and stereo submit
for those milestones. The Release target builds with zero warnings/errors and the
Release suite passes 12/12 CTests. The post-run verifier reports BeginScene callbacks,
EndScene callbacks, EndScene callback returns, OpenVR submissions, and the last bridge
phase/callback observed; its PowerShell syntax check passes. The exact-build live run
confirmed all bridge phases through `after_submit_stereo` and the EndScene callback
return for frame 3, but did not reach frame 4.

Do not repeat the current `A1420BFF...` run: it would only reproduce evidence already
captured. Sustained device `Present` entry/return telemetry is now implemented in the
OpenVR-flat diagnostic using the existing device-vtable hook. A Release build produced
candidate SHA-256
`DFFEC057CD41A65A6A529E776A89D6BF79F133A53595614B62B8E6D757252227`; the verifier
summarized Present entry/return counters for that live run.
The device-hook host test now exercises five Present entry/return cycles; the complete
Release build succeeds and the Release suite passes 12/12 CTests. DFFE is therefore
host-validated. A later manual run launched from SteamVR while A1420 was still staged
again showed the game only on the monitor and no image in PSVR2, with the log repeating
the same 3/3 cutoff. The installed DLL hash was confirmed as A1420, so that run adds
repeatability evidence for the old cutoff but does not test DFFE.

DFFE has now been staged and live-exercised on the exact build. Its installed hash was
confirmed as `DFFEC057CD41A65A6A529E776A89D6BF79F133A53595614B62B8E6D757252227`.
The user again saw normal monitor rendering and no game image in PSVR2. The live log
reached three device Present entries and three Present returns, `BeginScene=3`,
`EndScene=3`, three EndScene returns and three successful OpenVR submissions. Frame 3
completed every bridge phase through `after_submit_stereo`; neither the bridge callback
nor the original device `Present` is blocking at the cutoff.

The `6589B445...68621` continuity candidate has now been live-exercised on the exact
build. Its installed hash was confirmed. The run reached `Present=3`, `BeginScene=3`,
`EndScene=3` and three successful OpenVR submissions, then logged
`d3d9 device hook continuity: overwritten present_callbacks=3 begin_scene_callbacks=3 end_scene_callbacks=3`.
The game remained normal on the monitor and PSVR2 still showed no game image. This
confirms that the OpenVR path stops because the installed D3D9 vtable hooks are replaced
after the third frame.

The active diagnostic now identifies exactly which Reset/Present/BeginScene/EndScene
slots are overwritten and resolves each replacement target to the loaded module that
owns it. It does not re-hook or issue rendering calls from the observer thread. Release
candidate SHA-256 is `369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF`.
The complete Release build succeeds and the Release suite passes 12/12 CTests. The next
live run should use this candidate. If the replacement resolves to a known overlay/hook
module, decide whether to chain after it or remove that interference; if the targets
remain in D3D9 itself, investigate the engine/runtime transition that restores the native
vtable. Continue to require at least 300 submitted frames before promoting the bridge.
Do not infer stereo rendering or camera tracking from this same-image-to-both-eyes proof.

Do not skip directly to camera hooks, stereo rendering or motion controls.
Do not treat the D3D10 path as disposable; it remains a first-class follow-up for
the first game because it provides visible rendering improvements.
