# Validation model

The project uses explicit evidence states:

`planned` -> `implemented` -> `host-tested` -> `live-tested` ->
`headset-validated` -> `supported`.

Current bootstrap checks:

1. Configure a Win32 CMake build.
2. Build with MSVC at `/W4`.
3. Run `cojvr_build_identity_tests` to validate filename routing, catalog lookup and
   SHA-256 calculation.
4. Run `cojvr_d3d9_probe` to verify that a D3D9 interface can be created on the
   target machine.
5. Run `cojvr_d3d9_proxy_smoke` to load the proxy, forward `Direct3DCreate9`,
   attempt device creation, exercise `BeginScene`, `EndScene`, device and
   swapchain `Present`, and `Reset`, then verify native identity, hook-module,
   device/generation and structured-finalization evidence.
6. Test the Release proxy beside the exact known `CoJ.exe` build with VR disabled.
   Verify normal rendering, a known-build identity log, successful device creation
   logging and clean game shutdown before any VR-runtime or camera work.

## Audit-remediation Phase 0 host evidence

Phase 0 provenance tooling is **host-tested** as of 2026-09-14:

- `tools/new_build_manifest.ps1` records the source commit/tree/dirty state, Win32 configuration, diagnostic mode, rebuild commands and SHA-256 identities for the proxy and required OpenVR runtime artifact. Clean source is required by default; `-AllowDirty` produces an explicitly non-commit-reproducible diagnostic manifest with tracked-diff and untracked-file identities.
- D3D9 staging now requires the exact matching build manifest, assigns one `run_id`, records `CoJ.exe`, `ChromeEngine3.dll`, proxy and `openvr_api.dll` identities, and moves prior logs/run markers into `.cojvr-evidence/historical` instead of deleting them.
- The proxy logs `run_start` with the staging-provided run/build-manifest IDs and PID. All D3D9 live verifiers require that identity to match the current run manifest, staging state, copied build manifest and deployed hashes.
- `tools/collect_run_evidence.ps1` packages the run/build/staging/log evidence without launching the game, reports deployed-file changes, and marks evidence incomplete when no matching `run_end` exists.
- The provenance host test exercises manifest generation, run/log binding, matching deployments, missing-final-summary state, finalized state and altered deployment detection.

At the Phase 0 checkpoint, fresh Win32 Debug and Release builds completed successfully with `/W4`; both suites then passed **13/13 CTests**. This is the historical Phase 0 evidence; the current larger suite is recorded below. No new game, SteamVR or headset run was performed.

## Audit-remediation Phases 1-4 host evidence

Phases 1-4 reached their **host-tested** acceptance gate on 2026-09-14. No Call of Juarez, SteamVR or headset process was launched for this evidence.

Phase 1 evidence:

- The neutral runtime, build/game identity, diagnostics, OpenVR and OpenXR targets are separate. OpenVR and OpenXR are independently selectable, and dependency checks are conditional on the enabled backend.
- Full Win32 Debug and Release builds completed at `/W4`. Each CTest suite produced 17 valid outcomes: 16 PASS and one explicit SKIP for direct classic-D3D9 shared-texture capability returning `D3DERR_INVALIDCALL` on this host.
- Separate OpenVR-only and OpenXR-only Debug configurations each built and produced the same 16 PASS / one SKIP suite while the disabled backend's SDK root was deliberately nonexistent.
- Native D3D9 tests assert the loaded system `d3d9.dll`; proxy tests execute from unique temporary directories and verify a normal process exit emits structured `run_end`.
- The classic readback test validates two changing 65x37 asymmetric patterns across complete visible rows and row pitch rather than a single pixel.
- CI configuration now bootstraps each enabled pinned dependency. Its workflow definition is implemented; a remote CI execution is not claimed as local evidence.

Phase 2 evidence:

- `VtablePatch`/`HookRegistry` use conditional replacement and explicit no-change, applied, protection-failure, conflict and incomplete-rollback results.
- Host tests cover failure before replacement, failure after replacement, retained ownership, foreign-hook-safe restore, two vtables, identical/conflicting reinstall, integrity loss, partial rollback and a callback reaching its retained original during installation.
- Factory, device and swapchain hooks share this ownership model, keep per-vtable originals, preserve native HRESULTs and pin the containing module while installed callbacks can remain reachable.
- The Release hook-conflict test also passes with distinct non-foldable function symbols, preventing linker identical-code folding from weakening the test premise.

Phase 3 evidence:

- The classic proxy returns native `IDirect3D9`; only the explicitly opted-in D3D9Ex laboratory path retains a forwarding wrapper. Classic staging rejects the D3D9Ex environment switch or marker.
- A native test creates a first device, verifies `GetDirect3D` canonical `IUnknown` identity, and creates a second observed device through the recovered factory. Both native `CreateDevice` HRESULTs remain `D3D_OK`.
- Factory/device/swapchain IDs and creation thread are assigned without retaining COM references. A successful Reset advances the device generation while preserving a stable swapchain ID when the native swapchain identity is unchanged.
- Proxy smoke coverage verifies structured native identity, factory/device/swapchain ownership, original/replacement/current hook target module names, implicit-swapchain `Present`, and generation 2 after Reset.

Phase 4 evidence:

- `COJVR_EVENT` JSONL records include run/build identity, PID/TID, monotonic timestamp, factory/device/swapchain/generation, callback/capture/content/upload/submit sequences, duration, HRESULT/runtime result and exact global/per-device counters.
- Callback and stage detail uses bounded milestone sampling while every observation updates exact counters and active-stage state. A separate observer thread emits per-device periodic summaries and reports any factory/device/swapchain slot whose current target is no longer owned.
- The flat bridge reports capture, frame publication, upload, `WaitGetPoses`, left submit and right submit separately. RGB hashing ignores the undefined X/alpha byte, so a repeated captured texture advances submit attempts without advancing new-content sequence.
- Failure counters and last-failed/active stage remain visible in summaries even when a high-frequency detail event is outside the sampling milestones.
- `tools/read_render_telemetry.ps1` reports exact device activity, generations, hook losses/conflicts, stage progress, repeated content, whether submit counters advanced while capture remained fixed, and the recorded original path for the native factory `CreateDevice` slot. Tests cover complete evidence, incomplete evidence, mixed-run rejection, exact synthetic clock progression and Steam Overlay/native factory-target discrimination.
- `tools/verify_d3d9_openvr_flat_live_test.ps1` is the prepared post-run gate. It requires matching Phase 0 provenance, native identity, owned hooks, callback/capture/content/upload/balanced-eye progression, no stage/runtime failure, no final active stage and inactive D3D9Ex substitution. A run manifest may additionally require Steam Overlay absence; when set, verification fails unless a factory `CreateDevice` original target was observed and none resolves to `gameoverlayrenderer.dll`. Headset visibility is not part of this first observation gate.

The first run-bound Phase 0-4 manual observation was completed on 2026-09-14 as run
`20260914T214318Z-fe71b222b664`, bound to build manifest
`28598DB93EBD1D0A0838445C0DDDCD39BE9B4F32B144BC2DACFF6913E65A8C40`.
The run completed normally with a unique `run_end`, and provenance/deployed hashes,
structured core events, native factory/device identity, `GetDirect3D` identity and
implicit-swapchain identity all passed verification. One factory, one device, one
implicit swapchain and generation 1 were observed. The device reached exactly three
`Present`, three `BeginScene` and three `EndScene` callbacks; all three captures reached
D3D11 upload and balanced left/right OpenVR submission with no capture, upload,
`WaitGetPoses` or submit failure. Only one unique RGB content hash was observed across
the three captures.

The observation gate failed because the same device vtable then lost ownership of all
four instrumented device slots: `Reset`, `Present`, `BeginScene` and `EndScene`. The
four losses were reported together at approximately 5.0 seconds, after callback counts
had stopped at 3/3/3. No new device, swapchain generation, hook-install conflict or
active pipeline stage explained the cutoff. This reproduces the historical three-frame
failure with the remediated `VtablePatch`/`HookRegistry` ownership model and rules out a
bridge-stage stall for this run.

The loss records identify concrete current function addresses, but module attribution
is still ambiguous because both the staged proxy and the Windows runtime have basename
`d3d9.dll`. The next diagnostic revision therefore records full module paths plus the
original, replacement and current target addresses so a subsequent run can distinguish
restoration to the native target from a foreign replacement without relying on basename.

That diagnostic revision was exercised as run `20260914T215121Z-9bac4e22cffd`, bound
to build manifest `6A0BE68C4C56B5B4ED21D30B4D317A7CC7480FA79F7CB5ECCC430B68FD7E95C3`.
It reproduced the same 3/3/3 cutoff and four device-hook losses. For `Reset`, `Present`,
`BeginScene` and `EndScene`, every `current_target` matched its recorded
`original_target`, and every `current_path` was `C:\WINDOWS\system32\d3d9.dll`.
The factory `CreateDevice` original target before the project hook was instead in
`C:\Program Files (x86)\Steam\gameoverlayrenderer.dll`. The factory and swapchain hooks
remained owned, leaving 2 of 6 installed hook slots owned at the final summary. The run
ended normally, was packaged successfully, and its evidence package SHA-256 is
`0FF39018DF5FF9FF6B7AAFC76672B89BD3F84A813B431E5D228A8CC83EE99420`.

There is no production callsite that restores the device `HookRegistry` during the live
run. The next observation is therefore an A/B test with Steam Overlay disabled. Its
telemetry must first confirm that the factory `CreateDevice` original target is no longer
`gameoverlayrenderer.dll`; only then can persistence or disappearance of the three-frame
restoration be used as evidence about overlay involvement.
The evidence package for the earlier run `20260914T214318Z-fe71b222b664` has SHA-256
`C453F3795E30860215F1C5135D4222A825FC56538E5566F7E7AC1F00217C6B5A`.

The A/B verifier was strengthened before the next manual gate. Synthetic provenance and
telemetry tests pass in Debug and Release, and the parser was also run against historical
run `20260914T215121Z-9bac4e22cffd`; it recovered exactly
`C:\Program Files (x86)\Steam\gameoverlayrenderer.dll` as the factory `CreateDevice`
original path and set `SteamOverlayFactoryIntercepted=true`, while retaining the four
known hook losses. Debug and Release were rebuilt successfully after these changes.

The earlier staged candidate `20260914T224423Z-3a4527d3e3ab` produced no runtime log and
was superseded before any game launch so the A/B expectation could be part of the run
manifest itself. The replacement manual gate is currently staged as run
`20260914T224904Z-overlay-ab` with `validation.requireSteamOverlayAbsent=true`, build
manifest ID `054813FAF3446544AC26F25D07D919EE2B893323736885F3701DBD7F877E4A93`
and staged proxy SHA-256
`8D5B93665E6E7067EAC02054EE2967ABB0DE6F8E3062386AB2C633B21CB9393B`.
Its source manifest is intentionally recorded as a dirty snapshot rooted at commit
`ef27ac7451370e54391534885a1bf8b59e6436d1`; it is not reproducible from that commit
alone and must not be mistaken for the repository HEAD after the stabilization work is
committed. No game or SteamVR process was launched while preparing these verifier
changes. The remaining evidence requires the user to disable Steam Overlay for Call of
Juarez, launch SteamVR and the game manually, exercise the same DX9 path and exit
normally before running the prepared verifier.

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
reported two adapters. The D3D9 proxy smoke test, including native factory,
device and implicit-swapchain observation, passes in Debug and Release. The
optional `EndScene` callback passes repeated real D3D9 scene cycles. The current
Debug and Release suites each produce **16 PASS plus one explicit capability
SKIP out of 17 CTests**; the OpenVR flat proxy builds successfully in both. This
was revalidated from the current working tree on 2026-09-15 with fresh Debug and
Release builds followed by both CTest presets.

The exact installed `CoJ.exe` SHA-256 was rechecked as
`5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`.
The observed `ChromeEngine3.dll` contains `Direct3DCreate9` and no additional
common D3D9 bootstrap export names checked during host inspection.

The intended physical validation hardware is PlayStation VR2 on PC with both PS
VR2 Sense controllers. A passing desktop/live test does not imply PSVR2 validation.
