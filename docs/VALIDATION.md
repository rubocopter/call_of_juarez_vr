# Validation model

The project uses explicit evidence states:

`planned` -> `implemented` -> `host-tested` -> `live-tested` ->
`headset-validated` -> `supported`.

Current bootstrap checks:

1. Configure a Win32 CMake build.
2. Build with MSVC at `/W4`.
3. Run `cojvr_runtime_tests` to validate filename routing, catalog lookup and
   SHA-256 calculation.
4. Run `cojvr_d3d9_probe` to verify that a D3D9 interface can be created on the
   target machine.
5. Run `cojvr_d3d9_proxy_smoke` to load the proxy, forward `Direct3DCreate9`,
   attempt device creation, exercise `Clear`, `Present` and `Reset`, and verify
   identity/forwarding/device/frame-boundary log entries.
6. Test the Release proxy beside the exact known `CoJ.exe` build with VR disabled.
   Verify normal rendering, a known-build identity log, successful device creation
   logging and clean game shutdown before any VR-runtime or camera work.

The user performs all game and SteamVR launches manually. For the D3D9 live gate,
prepare the run with `tools/stage_d3d9_proxy.ps1`, let the user launch and close the
game, then inspect the resulting evidence with `tools/verify_d3d9_live_test.ps1`.
Use `tools/unstage_d3d9_proxy.ps1` to restore the game directory afterward.

The first forwarding-only live gate passed on the exact Call of Juarez D3D9 build:
the game rendered normally through a loaded save, the log identified the exact
known build and successful device creation, and the staged proxy was removed cleanly.

The later native-vtable `Present`/`Reset` observation hook also passed its flat-game
live gate on 2026-09-13. The user confirmed unchanged rendering and clean shutdown;
`tools/verify_d3d9_live_test.ps1` passed the known-build, forwarding, `CreateDevice`,
hook-installation and observed-`Present` checks. The proxy was then removed and the
game directory returned to an unstaged state. The hook is therefore **live-tested**.

After the later D3D9Ex-substitution crash, the proxy was removed and the user ran
the game again with no staged `d3d9.dll`, staging state or bridge marker. A save
loaded, movement remained normal and the game closed cleanly. That run is vanilla
stability evidence only; the stale `cojvr.log`/`callstack.txt` from the failed
D3D9Ex run do not belong to it.

OpenVR is now the initial SteamVR path. The pinned OpenVR 2.15.6 SDK, neutral
runtime adapter, D3D11 compositor backend and isolated `cojvr_openvr_probe.exe`
compile in Debug and Release.
Do not run the probe automatically. After the user starts SteamVR manually, run it
first without `--pose` to validate runtime/eye configuration, then with `--pose`
when physical PSVR2 tracking validation is explicitly being performed. `--submit`
adds one synthetic D3D11 stereo submission and therefore also requires the physical
runtime gate.

On 2026-09-14, with SteamVR started manually and PSVR2 connected, the Release
OpenVR probe passed all three isolated runtime gates. Normal mode initialized
OpenVR, selected DXGI adapter 0, created a D3D11 feature-level 11.0 device and
reported a 3400x3468 recommended target per eye. `--pose` returned a valid HMD
pose, and `--submit` completed with `submitted_one_stereo_d3d11_frame=true`.
This is **live-tested** OpenVR/SteamVR runtime, tracking and synthetic compositor
submission evidence. It is not yet `headset-validated`, because no headset-visible
image has been confirmed by the user.

For that next physical gate, `cojvr_openvr_probe.exe --submit-visible` submits the
same synthetic left/right eye textures for 180 paced frames. Use it only while the
user is looking through the visor; successful technical submission plus visible
confirmation is required before promoting this isolated compositor proof to
`headset-validated`.

For the more useful sustained gate, stage `d3d9_openvr_flat.dll` and its pinned
`openvr_api.dll` with `tools/stage_d3d9_openvr_flat.ps1` while the game is closed
and SteamVR is already running. The user then launches Call of Juarez manually,
tests menu, loading and gameplay for as long as needed, and closes it manually.
The first live attempt reached one genuine OpenVR scene submission but device
`Present` was observed only once. A second attempt installed an implicit-swapchain
`Present` hook, but that callback never fired. In both runs the game stayed on the
monitor and PSVR2 received audio but no game image. The current bridge therefore
submits the same flat game frame to both eyes after each successful `EndScene`;
this validates transport/presentation rather than stereo rendering. The latest
exact-build run reached `callback_count=3` and `submitted_frames=3` with no logged
OpenVR failure. Because that staged candidate logs every count from 2 through 10,
the missing count 4 means no fourth successful `EndScene` callback was observed.

The current diagnostic adds a successful-`BeginScene` counter and an `EndScene`
callback-return counter at the same 2-through-10, 60 and 300 milestones.
It also traces the bridge immediately before and after D3D9 readback, D3D11 upload,
`WaitForHmdPose`, and stereo submission so a stalled callback can be localized from
the final log line. Its Release DLL SHA-256 is
`A1420BFFA5B09C16CC586917491841BCC945ED858C00D232FC5667892E0A19B8`.
The five-cycle hook test passes for both scene boundaries, the Release target builds
with zero warnings/errors, and the Release suite passes 12/12 CTests. The verifier
reports BeginScene callbacks, EndScene callbacks, EndScene callback returns, OpenVR
submissions, and the last bridge phase/callback observed; its PowerShell syntax check
passes. This successor has now been exercised on the exact build. The live log reached
`BeginScene=3`, `EndScene=3`, `callback_return_count=3` and `submitted_frames=3`.
Frame 3 completed `after_readback`, `after_upload`, `after_wait_for_hmd_pose` and
`after_submit_stereo`, then returned from the EndScene callback. No fourth BeginScene
was observed and no OpenVR failure was logged. This rules out a stall inside the flat
bridge callback for the observed cutoff; sustained presentation/frame lifecycle is the
next diagnostic boundary. The >=300-frame live/headset gate remains incomplete.
Do not repeat the historical `A1420BFF...` diagnostic as a new evidence run. A later
run launched manually from SteamVR while it was staged again left the game visible
only on the monitor and produced the same `BeginScene=3`, `EndScene=3`, three callback
returns and three successful submissions. Its staged DLL hash was confirmed as
`A1420BFFA5B09C16CC586917491841BCC945ED858C00D232FC5667892E0A19B8`, so that run does
not exercise the newer Present telemetry.

Release candidate `DFFEC057CD41A65A6A529E776A89D6BF79F133A53595614B62B8E6D757252227`
has now been staged and exercised on the exact build. The user again observed normal
monitor rendering and no game image in PSVR2. The live log reached `Present=3` with
three Present returns, `BeginScene=3`, `EndScene=3`, three EndScene returns and three
successful OpenVR submissions. Frame 3 completed every bridge phase through
`after_submit_stereo`. This rules out a stall inside the original device `Present` as
well as inside the flat bridge callback for the observed cutoff.

Release candidate `6589B445A6033FABE3CB37C4014E034655CD52D8B1B08C4C4C6F70E962068621`
has now been exercised on the exact build. The installed hash was confirmed before log
inspection. The run again reached three device Present entries/returns, three successful
BeginScene/EndScene callbacks and three successful OpenVR submissions, then the continuity
observer reported `overwritten` with `present_callbacks=3`, `begin_scene_callbacks=3`
and `end_scene_callbacks=3`. The game continued rendering normally on the monitor while
PSVR2 showed no game image. This is live evidence that the installed D3D9 vtable hooks
are replaced after the third frame.

The next diagnostic candidate records the state of Reset, Present, BeginScene and EndScene
individually and resolves each replacement function address to its owning loaded module.
Release SHA-256 is
`369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF`. The complete
Release build succeeds and the Release suite passes 12/12 CTests. The >=300-frame
live/headset gate remains incomplete.

The D3D9/D3D11 transport boundary is now characterized by host evidence:

- D3D9Ex shared render target -> D3D11 `OpenSharedResource` passes in Debug and
  Release, including a pixel readback check: **host-tested**.
- A classic `Direct3DCreate9` device returns `D3DERR_INVALIDCALL` for the same
  shared render-target request on this host. This unsupported capability is itself
  **host-tested** and prevents treating the D3D9Ex result as direct evidence for
  the current game device.
- The proxy's opt-in classic-API -> D3D9Ex bridge passes synthetic `Clear`,
  `Present`, `Reset`, Ex-interface and shared-render-target checks in Debug and
  Release: **host-tested**. Its exact-build live-test failed after the first
  observed `Present`, so this mode is not a live-safe replacement for the classic
  D3D9 device path.
- Classic D3D9 render-target readback to system memory followed by D3D11 upload
  passes pixel verification in Debug and Release and has now passed the exact-build
  flat-game gate: **live-tested**. This preserves
  the original game device semantics and is the current fallback transport candidate.
  Live validation uses a separate `d3d9_readback.dll` diagnostic build that enables
  exactly one readback at the first observed `Present`; the normal `d3d9.dll` has no
  runtime environment-variable or marker switch for this diagnostic. Stage the
  diagnostic explicitly with `tools/stage_d3d9_proxy.ps1 -ProxyPath ...` and verify
  its log with `tools/verify_d3d9_readback_live_test.ps1`.

The separate readback diagnostic was exercised in the exact Call of Juarez build
on 2026-09-14. The live log passed all readback verifier conditions: exact build,
classic-D3D9 forwarding, successful `CreateDevice`, active `Present`/`Reset` hooks,
an observed `Present`, successful 2560x1440 D3D9 -> CPU -> D3D11 transfer, no
D3D9Ex substitution and no readback failure. The old `callstack.txt` retained its
2026-09-13 D3D9Ex-crash timestamp, so no new crash artifact was produced. The
diagnostic proxy was removed after the run. The user confirmed that menus and
gameplay were usable, videos and in-game animations worked, sound and controls
showed no noticed regressions, and no unusual visual artifacts were observed.
The classic-D3D9 readback gate is therefore **live-tested**.

The experimental OpenXR D3D11 probe has already been exercised against SteamVR.
After requesting OpenXR 1.0 compatibility and using an explicitly typed RTV for
SteamVR's typeless swapchain image, it created the runtime/session/swapchains and
submitted one valid projection frame. The session remained synchronized rather
than visible/focused, so this is live runtime evidence only and is not a headset
validation. A later wait-for-`shouldRender` change still needs a rebuild/runtime
recheck if OpenXR work resumes.

Host tests do not promote a game integration to `live-tested`. A live desktop
test does not promote it to `headset-validated`.

The initial development host passed the Release D3D9 availability probe and
reported two adapters. The D3D9 proxy smoke test, including the native-vtable
`Present`/`Reset` observation hook, passes in Debug and Release. The new optional
`EndScene` callback passes five consecutive real D3D9 scene cycles. The full
Release host suite currently passes **12/12 CTests**; the new EndScene hook and
OpenVR flat proxy targets also build successfully in Debug.

The exact installed `CoJ.exe` SHA-256 was rechecked as
`5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`.
The observed `ChromeEngine3.dll` contains `Direct3DCreate9` and no additional
common D3D9 bootstrap export names checked during host inspection.

The intended physical validation hardware is PlayStation VR2 on PC with both PS
VR2 Sense controllers. A passing desktop/live test does not imply PSVR2 validation.
