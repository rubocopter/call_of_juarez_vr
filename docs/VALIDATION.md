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
- Full Win32 Debug and Release builds completed at `/W4`. Before the camera-boundary work, each CTest suite produced 17 valid outcomes: 16 PASS and one explicit SKIP for direct classic-D3D9 shared-texture capability returning `D3DERR_INVALIDCALL` on this host. The current suite has 18 outcomes as recorded below.
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
run. A Steam-Overlay-disabled A/B remains the required controlled observation to determine
whether `gameoverlayrenderer.dll` contributes to the three-frame restoration. The user has
explicitly deferred that repeat run while the camera/render boundary is investigated, so
no conclusion about overlay involvement is drawn here.
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
manifest itself. The replacement A/B candidate was staged as run
`20260914T224904Z-overlay-ab` with `validation.requireSteamOverlayAbsent=true`, build
manifest ID `054813FAF3446544AC26F25D07D919EE2B893323736885F3701DBD7F877E4A93`
and staged proxy SHA-256
`8D5B93665E6E7067EAC02054EE2967ABB0DE6F8E3062386AB2C633B21CB9393B`.
Its source manifest is intentionally recorded as a dirty snapshot rooted at commit
`ef27ac7451370e54391534885a1bf8b59e6436d1`; it is not reproducible from that commit
alone and must not be mistaken for the repository HEAD after the stabilization work is
committed. No game or SteamVR process was launched while preparing these verifier
changes. That staged A/B has not been promoted by a game run and is superseded as the
active manual gate by the camera-boundary experiment below. Its historical staging record
remains useful if the A/B is resumed later.

## Camera -> view/projection -> renderer host evidence — 2026-09-15

The current user-directed gate investigates the game-camera boundary before investing
further in VR presentation. Static exact-build inspection establishes the following chain;
the full address-level record is in `docs/research/COJ_CAMERA_PATH.md`:

- `LawmanModule.SetViewFullscreen(Camera)` reaches native `Module.SetViewCamera` at
  `ChromeEngine3.dll` RVA `0x00039710`, binding the native camera to the active view.
- `CBaseCamera` RTTI identifies its primary vtable at RVA `0x0030AE1C`.
- vtable slot `+0x24` -> RVA `0x001C5BA0` calls the FOV/frustum slot `+0x28`, invokes
  the view/projection matrix update, then writes the current camera into the renderer at
  offsets `+0x1AC/+0x1B0` through renderer global RVA `0x00590074`.
- slot `+0x28` -> RVA `0x001C58E0`; matrix update RVA `0x0022BB10` and projection
  builder RVA `0x0022BC50` provide the View/Projection side of the chain.

The new `d3d9_camera_probe` diagnostic is **host-tested**. It requires both exact binary
hashes, validates the expected `CBaseCamera` vtable targets, uses the existing safe
`HookRegistry`, and forwards D3D9 directly to the Windows system runtime. It does not
initialize OpenVR and does not install D3D9 frame hooks. An external JSON command can
request FOV/yaw/pitch; yaw/pitch are applied only during the engine render-camera update
and the natural camera basis is restored immediately afterward.

`tools/set_camera_probe_control.ps1` updates the control atomically and rejects any staging
mode other than `d3d9_camera_probe`. `tools/verify_camera_probe_live_test.ps1` requires
exact run/build/deployment provenance, exact engine identity, successful camera hook
installation, an accepted external command, orientation/FOV application through the
renderer camera, basis restoration, hook restoration and a normal bound `run_end`.

Fresh full Win32 Debug and Release builds pass. Each current CTest suite produces
**17 PASS plus one explicit capability SKIP out of 18 CTests**. The added `camera_probe`
test now also covers XR-neutral recenter, 20-degree yaw mapping, pitch/roll basis mapping,
return-to-origin without accumulation, invalid-pose passthrough and tracking disable;
`provenance_tools` covers both manual camera control and the HMD enable/recenter/disable
control plus HMD build-manifest identity. No Call of Juarez or SteamVR process was launched
for this host evidence.

The exact-build camera/render boundary and external FOV control are **live-tested**. Candidate run
`20260915T150554Z-7e0d7da45949` used Release proxy
SHA-256 `0D221F41A28E18B07DED7582EA292AC7077ACF35391C5238945A524DE612E3DE`
and build manifest `5D14F545D3164C21AB430B439AA9E093253A94A21470CBFD2A6F1A9410DB0FD5`.
Staging reverified the exact game/engine hashes, installed an initially disabled camera
control, and removed the previously staged OpenVR-flat proxy/runtime without launching
Call of Juarez. These lines record post-staging state; the build manifest itself preserves
the source snapshot captured immediately before deployment.

During manual gameplay the user enabled FOV `110`, yaw `20` and pitch `-10` and confirmed
an obvious change in the rendered game view. The run log recorded the accepted control,
natural FOV `80` -> applied FOV `110`, the natural/applied camera basis,
`renderer_camera_match=true` and `restored=true`. The control was then disabled and the
probe returned to natural passthrough. Normal game exit restored both camera-vtable hooks
and emitted the bound `run_end`.

`tools/verify_camera_probe_live_test.ps1` passed all run/build/deployment, exact-binary,
camera-path, restoration and finalization checks. `tools/collect_run_evidence.ps1` marked
the run complete and produced package SHA-256
`2C41F3668EC4F7C1167C2BC7D3B88455119EC06E3FBB3E89C382074B52644866`.

A later physical-HMD attempt invalidated the stronger interpretation that this run had
independently proven yaw/pitch render control. The original orientation write targeted the
derived basis at `+0xC4/+0xD4/+0xE4`; exact-build disassembly shows matrix update RVA
`0x0022BB10` subsequently rebuilding that basis from the source transform beginning at
`+0x44`. The simultaneous FOV change therefore explains the obvious visual difference
without establishing that yaw/pitch survived to render consumption.

## HMD -> monocular game-camera rotation evidence — 2026-09-15

The next gate now has a **host-tested** candidate, `d3d9_hmd_camera`. It reuses the exact
two-slot camera hook and restoration path proven above, but camera orientation is sourced
through the backend-neutral `runtime::PoseSource` / `RelativePoseTracker` boundary.
OpenVR is only the current producer: its existing x86 `WaitForHmdPose` path runs in
standing tracking space and publishes neutral `PoseSample` values. ChromeEngine-specific
code does not depend on OpenVR/OpenXR types.

The neutral orientation contract is right-handed `+X` right, `+Y` up, `-Z` forward,
quaternion `(x,y,z,w)`. Enabling tracking captures the current physical orientation as
base; subsequent orientation is recomputed as `R_base^T * R_current`, not integrated from
frame deltas. CoJ receives this relative rotation as a transient offset on the natural
camera basis and the basis is restored after the render-camera update. Invalid/missing XR
pose or disabled tracking leaves the game camera untouched.

The dedicated proxy only forwards `Direct3DCreate9`; it does not install D3D9 `Present`,
`BeginScene`, `EndScene` or `Reset` hooks. Staging binds both `d3d9_hmd_camera.dll` and
`openvr_api.dll` to the run manifest, verifies exact CoJ/ChromeEngine hashes, and backs up
and restores both runtime deployment and the camera-control file transactionally.

Before the first manual attempt, Win32 Debug and Release builds completed with `/W4` and
both CTest suites produced **17 PASS plus the existing explicit classic-D3D9 capability
SKIP**. `tools/verify_hmd_camera_live_test.ps1` was prepared to require OpenVR pose-source
startup, exact camera profile, recenter, advancing HMD pose sequences, meaningful yaw and
pitch samples, renderer-camera identity, restoration, passthrough, clean shutdown and the
bound `run_end`.

The first physical-HMD attempt failed the visual gate. Telemetry showed a valid OpenVR HMD
source, successful recenter, advancing pose sequences and substantial yaw/pitch changes,
while the user observed no camera motion. Tracking was disabled and natural game control
returned immediately. This is useful live failure evidence, not a successful rotation gate.

Static disassembly then identified the source world/camera basis at
`+0x44/+0x54/+0x64`; `0x0022BB10` derives the downstream `+0xC4/+0x144` path from it.
The implementation was corrected to inject there, and fresh Debug/Release suites passed.

That Release candidate was staged as run `20260915T211240Z-7d1c65d07a43` with build
manifest `409C8A9A78C1276649F8CCE9C4CF817370E045FD2EFC8768051D1CED6C36889F` and proxy
SHA-256 `6471140FB12FE8695C33B5F1553A9F66DB3B2153165FABB505780A8F9A0ED4A3`.
The manifest records the current dirty source snapshot explicitly. Staging reverified the
exact CoJ/ChromeEngine identities, the deployed proxy/OpenVR hashes, and created the HMD
control with tracking, manual diagnostic orientation and recenter all disabled by default.

The second physical-HMD attempt also failed the visible camera-rotation gate, but produced
more specific evidence. Pose/recenter advanced and telemetry proved the `+0x44 -> +0xC4`
path changed. The user observed that HMD direction instead controlled which distant world
objects were rendered: pointing away from the monitor could make the scene nearly empty,
while the player camera, body and weapon continued looking in the original direction.
Tracking was disabled immediately and generation 3 returned to natural passthrough.
The run then exited normally, restored both camera hooks and stopped the OpenVR pose source.
Its collected evidence package is SHA-256
`8E015C2B39326EE2E9C52804E9C6E2E03262088B1B3FFB05E5BC92747B45A5D2`. The hardened HMD
verifier rejects this run because it cannot prove the culling/view pair changed together.

Exact-build disassembly then established the missing paired transform. Native camera
setters write `+0x44` and call RVA `0x001F3F80` to rebuild its inverse at `+0x04`.
`0x0022BB10` copies `+0x04 -> +0x84 -> +0x104`, and uses `+0x104` with projection
`+0x184` to produce view-projection `+0x204`. The new implementation mirrors that native
sequence and restores both source matrices after the render update. The verifiers now
require observed changes in `+0x104` and `+0x204`, in addition to the world/culling path.

After CoJ closed, fresh Debug and Release builds plus both full CTest suites passed with
**17 PASS plus the existing explicit classic-D3D9 capability SKIP out of 18**. The earlier
suite attempted while CoJ remained open remains invalid host evidence, but the synchronized
world/view implementation is now **host-tested** and ready for a newly staged manual retry.

The synchronized candidate was staged as run `20260915T212754Z-a12fcb10f11a`, build
manifest `0B73CD2A748762A6A8FBE7A47E610534B0033C08B5B49FDE13BE23DC810F6180`, proxy
SHA-256 `CAD1A6153F81026FD90D43570F6E370F9B8DE2AD9790C180B67C770965FAFCA0` and OpenVR
SHA-256 `AB696E4F218A95B3E396BC310F9FE6485DF48C99C0969762083212B1E1F025A6`.
Deployed hashes matched staging state and the exact CoJ/ChromeEngine identities were
accepted. During the physical HMD test, telemetry showed advancing pose samples together
with `render_basis_changed=true`, `view_matrix_changed=true`,
`view_projection_changed=true`, `restored=true` and `renderer_camera_match=true`.
The user confirmed that the first-person visual camera now rotated, proving the synchronized
world/view path reaches the visible renderer. The motion was inverted relative to physical
head movement and many environment elements disappeared, so the 1:1 gate failed and the
run must not be promoted to `live-tested`.

Tracking was then disabled in the live process. The log accepted control generation 3 with
`tracking_enabled=false` and emitted `camera_probe_passthrough`, returning immediately to
the natural game camera. After normal user shutdown, `verify_hmd_camera_live_test.ps1`
passed run/manifest/deployment identity, HMD pose/recenter progression, render/view changes,
passthrough, hook restoration, XR shutdown and `run_end`. This structural verifier pass
does not override the failed manual direction/visibility gate. Evidence package SHA-256:
`FDEDBFD5427892A16208C678EA5145AE22D72BE83DBFD37CED2A4BE3915F888D`.

The `-yaw/+pitch` candidate was manually exercised as run `20260915T214329Z-3d9f66ae064e`.
Pitch direction was correct, but yaw remained reversed; most world geometry disappeared or
rendered white and the right-hand weapon appeared on the left side of the camera. Tracking
was disabled immediately and control generation 3 returned to natural passthrough. After
normal shutdown the structural verifier passed all run/manifest/deployment, pose/recenter,
render/view, restoration, XR shutdown and `run_end` checks. Evidence package SHA-256:
`AE5B87AE5F54F68D6EAFE6D6E68C41F737DB9134A0FF443E5C122C477BF2D965`. The structural
pass does not override the failed manual visual gate.

Follow-up exact-build disassembly identified a concrete handedness bug: native
`FromForwardUpPos` RVA `0x001F3150` stores `right = up x forward` at camera `+0x44`, while
the hook wrote `forward x up` (left). Current source now carries and writes the native right
axis explicitly. Run `20260916T104036Z-24b3e3010d4c` then tested that corrected basis with
the physical HMD. The user reported good overall camera feel, the character model on the
expected side, no obvious recurrence of the disappearing/white environment, and correct
up/down pitch. Left/right remained inverted. The run telemetry retained determinant `+1`,
valid source world/view homogeneous layouts, advancing pose samples, changed derived
render/view/view-projection data, restoration and `renderer_camera_match=true`; tracking was
disabled and normal shutdown emitted `run_end`. This is live evidence that the handedness
and scene-visibility failure is no longer reproduced, but the overall monocular gate still
fails on yaw direction.

The adapter now negates only `physical.yaw_degrees` before applying the CoJ source transform.
Pitch keeps the live-confirmed sign and roll remains excluded. This correction is deliberately
game-specific: it does not change the neutral OpenVR pose convention or the native
`right = up x forward` matrix contract.

A second static pass against the exact `ChromeEngine3.dll` SHA-256
`DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8` verified the complete
source-matrix contract. `FromForwardUpPos` writes right/up/forward/position as four 16-byte
rows, sets the three orientation-row `w` components to `0` and position `w` to `1`.
`0x0022C510` writes and normalizes the three source axes at `+0x44/+0x54/+0x64`, preserves
position at `+0x74`, recomputes `+0x04 = inverse(+0x44)` through `0x001F3F80`, then enters
`0x0022BB10`. No additional transpose or hidden axis swap was found.

The camera integration now fails closed before injection unless both source world `+0x44`
and source view/inverse `+0x04` preserve that native homogeneous layout and the natural
basis is rigid/right-handed. After native inverse generation, the injected view matrix is
validated again before the engine update consumes it. The applied basis must remain unit,
orthogonal and right-handed with determinant approximately `+1`; a reflected basis such as
the previous left-axis reconstruction has determinant `-1` and is rejected to natural
passthrough with restoration of the original source state. Host tests sweep the allowed
diagnostic yaw/pitch range and reject reflected, scaled, skewed and non-finite bases.
Successful HMD telemetry records both matrix-layout checks and natural/applied determinants.
`verify_hmd_camera_live_test.ps1` now requires both determinants within `0.02` of `+1` and
the complete world/view homogeneous contract. `provenance_tools` executes the verifier
against synthetic valid evidence and proves rejection of a non-rigid natural determinant and
an invalid source-view layout. Fresh complete Debug and Release suites pass with **17 PASS
plus one expected capability SKIP out of 18**. This remains host evidence only; stable scene
visibility and the corrected physical yaw direction still require the next manual game observation.

## First native-stereo candidate host evidence — 2026-09-16

The user explicitly combined the next yaw confirmation with the first native-stereo gate.
Static inspection identified the exact ChromeEngine view boundary at vtable target RVA
`0x00030FB0` and render-view core RVA `0x00030E00`. The first `d3d9_native_stereo`
implementation rendered the left eye through the original view method and invoked only the
core for the right eye. Later live evidence proved that asymmetry incomplete; the current
candidate replays the complete `0x00030FB0` wrapper for both eyes as documented below.

OpenVR eye configuration is consumed through `CameraStereoRuntimeCallbacks`; the game adapter
does not depend directly on an XR API. `EyeView::pose` is explicitly eye-to-head for this
backend. Host tests prove the eye translation mapping into the native right/up/forward basis
and asymmetric per-eye FOV mapping into the engine frustum. The per-eye camera update keeps
the natural game camera authoritative and restores the source world/view pair and frustum.
After each complete eye render, the integration additionally restores derived matrices at
`+0x84`, `+0xC4`, `+0x104`, `+0x144`, `+0x184` and `+0x204` transactionally, preventing the
right eye from leaking state into the normal engine path.

For this first proof, each eye result is synchronously read back from classic D3D9, uploaded
to its own D3D11 texture and submitted through the existing OpenVR D3D11 compositor backend.
The readback cache is keyed by both D3D9 device identity and backbuffer description and is
invalidated on device changes/readback failures. The candidate observes the device from the
factory `CreateDevice` path only and installs no `Present`, `BeginScene`, `EndScene` or
`Reset` hooks.

`tools/verify_native_stereo_live_test.ps1` requires two or more submitted stereo frames with
both native eye camera/projection passes, both captures, successful transactional state
restoration after each eye, plausible eye separation, at least one distinct left/right pixel
hash pair, renderer-camera identity, HMD matrix invariants, disable-to-natural passthrough and
clean camera/render-view/factory/runtime shutdown. Synthetic provenance tests prove both the
success path and rejection of duplicated left/right eye content.

The first physical native-stereo attempt used run `20260916T113600Z-native-stereo`. The user
started the game more than once under that same staged run identity, so the collected package
is deliberately **incomplete** and cannot be promoted as one clean live run. The log contains
four process `run_start` records, four successful OpenVR/native-D3D9 initializations, advancing
HMD orientation samples, **13,558 incomplete stereo-frame records, zero successful stereo
frames, zero eye captures and zero OpenVR stereo submissions**. The user therefore continued
to see SteamVR's flat/theater presentation. No additional SteamVR action was missing.

The failure is specific: every attempted eye frame reported `left_camera_applied=true` but
`left_projection_applied=false`. Live PSVR2 optics showed OpenVR raw vertical tangents with
negative `top` and positive `bottom`; the adapter had passed those signs directly into the
neutral `EyeFov` contract, which requires positive `up` and negative `down`. The OpenVR
adapter now converts that API-specific convention explicitly and a host test reproduces the
observed sign pattern.

The same run also reproduced a visibility mismatch: HMD motion rotated the visual camera,
while geometry outside the mouse/game-facing direction was not selected consistently. Exact
render-view disassembly shows `0x00030E00` performs the camera update and then continues scene/
visibility work. The stereo hook had restored source world/view and frustum state immediately
after the camera update, before that later work. Stereo eye state is now held for the complete
render-view pass and the source world/view pair, frustum and derived matrices are restored
transactionally only after each eye finishes. The game-controlled character/body orientation
remains a separate later body/IK ownership problem; it is not coupled directly to head yaw in
this gate.

Fresh Debug and Release suites after these corrections pass all 19 CTest outcomes with
**18 PASS plus the expected classic-D3D9 shared-texture capability SKIP**. Provenance now also
rejects reuse of one `run_id` across multiple process starts. The corrected native-stereo
candidate is **host-tested only** until a fresh one-launch physical run proves eye projection,
capture/submission, visible stereo depth and stable HMD-driven scene visibility.

The next physical run, `20260916T123049Z-8d977bb5b439`, was bound to build manifest
`E9816AC9943DB3B572A458F28B937EFFC9ED64D9DDCB1CDAD37924972F7D2F00`. It used one process
start and correct exact-build/deployment provenance. OpenVR initialized with the PSVR2 eye
configuration (`3400x3468`, eye X offsets `-0.0325/+0.0325`), HMD pose/recenter advanced, and
both eye camera/projection passes reported successful capture, transactional restoration and
submission. The user saw gameplay in the headset after loading a save, whereas the pre-game
presentation remained flat/theater-like.

This is not native-stereo acceptance evidence. Every sampled submitted frame had identical
left/right RGB hashes (`distinct_eye_content=false`), including frames `1..8`, `90`, `180`,
`270`, `360` and `450`. The user reported severe discomfort, images that did not combine
properly and inadequate frame pacing. The old synchronous capture path always read
`GetBackBuffer(0,0)` even though capture occurs inside ChromeEngine's render-view boundary.
The following candidate instead captured active D3D9 render target 0, recorded whether it was
the backbuffer and refused to submit equal left/right hashes. The old frame-level
`renderer_camera_match=false` was also sampled after the eye pass; per-eye correlation is now
latched at the camera update itself and the live verifier requires both eye values.

Shutdown evidence also failed. Tracking disable returned to natural passthrough and the
camera/render-view plus factory hooks were restored, but the log stopped before
`native_stereo_runtime: status=stopped` and `run_end`; the collected package is therefore
`runtimeEnded=false`, `incomplete=true`. Package SHA-256 is
`C45162E2CB9584BBA0454319E62B1AA5438A6C973193FD64F86A377DA047CEEE`. The precise CRT-teardown
failure point is not proven. The next candidate shuts down readback/OpenVR and records
`run_end` before abandoning its retained process-lifetime D3D9 pointers, avoiding explicit
final COM releases in the atexit path.

Run `20260916T130852Z-1438628c90c6` tested that active-render-target candidate. The user
reported correct left/right and up/down HMD camera motion but a flat presentation throughout.
Telemetry explains the failure precisely: the left eye captured RT0 successfully as the
`2560x1440` `D3DFMT_A8R8G8B8` backbuffer, while every right-eye capture failed because RT0 was
`D3DFMT_NULL` (`FOURCC 'NULL'`). Camera/projection application and renderer-camera correlation
were true for both eyes, but `right_captured=false` and `submitted=false`, so no complete
stereo pair reached OpenVR. The run is diagnostic only: the same run ID contains three
`run_start` process IDs and no clean `run_end`, so it cannot be promoted.

Exact-build disassembly resolves the left/right asymmetry. RVA `0x00030FB0` is the complete
render-view wrapper: it checks and sets the per-view byte at `view+0xD7`, stores the active view,
calls core RVA `0x00030E00`, then performs additional post-core work. The failed candidate used
the complete wrapper for the left eye but called only `0x00030E00` for the right eye. Current
source now replays `0x00030FB0` for the right eye by clearing `view+0xD7` only for that call and
restoring its natural post-left value afterwards. `D3DFMT_NULL` is explicitly rejected as an
auxiliary target rather than treated as another color format. The verifier requires
`right_full_view_pass=true`, `right_view_guard_restored=true`, wrapper RVA `0x30fb0` and core RVA
`0x30e00` in addition to the existing distinct-eye/camera/restoration checks.

Fresh Debug and Release builds after this correction both passed the complete host suite with
**18 PASS plus the expected classic-D3D9 shared-texture capability SKIP out of 19**. At that
point the full-wrapper replay candidate was **host-tested only**; the following run supplied the
missing live eye-render evidence.

Run `20260916T133322Z-36c287cc43d8`, build manifest
`346D6C806E1DA86E28A605E72AC10238FE0FBBBBCC6B83DCA91BC5ED06CF2C15`, supplied that next
single-process live observation. OpenVR recommended `3400x3468` per eye with eye-to-head X
offsets `-0.0325/+0.0325` m. Both complete `0x30FB0` eye passes captured the real
`2560x1440` `D3DFMT_A8R8G8B8` RT0; sampled left/right hashes were distinct through frames
`1..8`, `90`, `180`, `270`, `360`, `450`, `540`, `630`, `720`, `810` and `900`, both
`renderer_camera_match` values were true, transactional state restoration passed and each pair
was submitted to OpenVR. The user observed binocular gameplay, correct yaw/pitch direction,
camera placement at the character head and no obvious missing scene geometry. Native distinct
eye rendering is therefore **live-tested**.

The visual gate still failed: the user reported that the binocular image combined poorly and
performance was mediocre. Static inspection of the shipped gameplay definitions then found a
concrete scale mismatch: `Data/Player/PlayerProperties.def` documents movement as `cm/s` and
acceleration as `cm/s^2`, while the neutral XR pose contract is metres. The live candidate had
inserted OpenVR eye offsets directly into CoJ camera coordinates, making the physical eye
baseline 100x too small. Current source converts eye translation by `100` in the CoJ adapter,
adds per-eye runtime/applied-position and frustum telemetry plus the D3D9 viewport, and adds
before/after markers around readback and OpenVR shutdown. The native-stereo verifier now rejects
the obsolete 1:1 scale, a missing capture viewport, an applied baseline inconsistent with the
runtime eye transform and invalid/non-asymmetric frusta. Debug and Release both pass 18 tests
plus the expected shared-texture SKIP after the code change; the strengthened synthetic
provenance/verifier gate also passes. The transport also records per-eye `GetRenderTargetData`
stall time, CPU/hash/D3D11 upload time, total capture time and OpenVR submit time on sampled
frames so the next run can quantify the proof path's performance cost. This scale/diagnostic
revision is **host-tested only**.

The same candidate now removes the need to Alt-Tab for normal headset recentering. A minimal
OpenVR global action manifest binds `/actions/global/in/recenter` to the left PS VR2 Sense
Create click (`/user/hand/left/input/create`). `OpenVrRuntime` resolves and polls that action,
emits one request on the press edge, and the native-stereo boundary feeds it into the existing
XR-neutral `RelativePoseTracker::RequestRecenter()` path before consuming the corresponding HMD
pose. The action manifest and Sense binding are build/run provenance artifacts and staging
verifies/restores them transactionally. The native-stereo verifier now requires evidence that
the OpenVR input set initialized, a recenter press was observed and the logical request reached
the camera pose boundary. Both Debug and Release suites pass 18 tests plus the expected
classic-D3D9 shared-texture capability SKIP after this change. Controller behavior itself is
**host-tested only** until a physical Sense press confirms it in-headset.

Run `20260916T153109Z-8976b8f77775`, build manifest
`65BC08F2024C2FC975DD915E4A752F611F19CE597D8067E6B532FD3AD57648B4`, supplied that physical
confirmation. The staged deployment hashes matched the proxy, OpenVR runtime, action manifest
and PS VR2 Sense binding. OpenVR input initialized, the log recorded
`openvr_input_event: action=recenter result=pressed source=global_action`, and the same request
reached `camera_hmd_recenter_requested` followed by `camera_hmd_recentered`. The user confirmed
that left Sense Create recenters while remaining in the headset. This promotes the minimal
Create/recenter action path itself to **headset-validated**; it does not validate tracked
controller gameplay.

The run also supplied live evidence for the corrected Call of Juarez world scale. Runtime eye
X offsets of `-0.032/+0.032` metres were applied as approximately `-3.2/+3.2` game units at the
neutral camera, and later world-oriented samples retained the same centimetre-scale baseline.
Submitted frames kept distinct left/right real-color hashes, renderer-camera correlation, the
complete `0x30FB0` wrapper for both eyes and asymmetric per-eye frusta. The geometry/scale path
is therefore **live-tested structurally**, but visual acceptance still failed: the user reported
very poor image quality/frame pacing and discomfort.

The timing evidence explains why the current path remains a proof transport. Sampled
`copy_upload_ms` values are commonly in the mid-30 ms range **per eye**, with higher samples in
the 40-50+ ms range, before accounting for the two engine view passes and the rest of the game.
OpenVR submit itself is usually small after startup, so the dominant observed cost is the
synchronous capture/CPU/hash/D3D11-upload path rather than compositor submission. No FPS value is
inferred from these sampled stage timings alone.

The same physical run exposed a separate presentation boundary for flat UI. Once the game entered
its menu, that menu was not visible in the headset; returning to gameplay resumed HMD-driven VR
movement. Treat this as live evidence that the current native gameplay render-view hook is not a
complete HUD/menu/video presentation path. A later UI milestone must handle flat/modal content
explicitly instead of assuming it is part of the gameplay stereo pass.

An additional host-only maintenance pass on 2026-09-16 hardened the current candidate without
launching Call of Juarez, SteamVR or the headset. `PoseFromRigidTransform3x4` now rejects
non-finite, scaled, non-orthogonal and reflected rotation bases instead of marking every 3x4
matrix valid; finite position validity is tracked independently. `OpenVrRuntime::WaitForHmdPose`
now combines that numeric validation with OpenVR's `bPoseIsValid` rather than overwriting it.
The VR-math host test covers NaN rotation, infinite position, scale and reflection rejection.

The D3D9 stereo transport also now keys cached resources by multisample type/quality in addition
to size/format, avoiding stale readback assumptions if RT0 changes to a surface with otherwise
matching dimensions. The duplicated flat/stereo RGB content hash was replaced by one packed BGRX
diagnostic hash that still ignores undefined alpha/X and row padding while processing two pixels
per hash iteration. Host coverage verifies alpha/padding invariance, RGB-change detection and
invalid-pitch rejection. This reduces CPU work in the equality/change diagnostic path; no
headset-frame-time improvement is claimed without a live timing run.

MSVC `/analyze` was run across the Debug tree. It identified several 32/64 KiB automatic buffers
in Win32 runtime/proxy paths and temporary-string `string_view` telemetry arguments. The large
path/hash buffers now use heap-backed storage and telemetry materializes strings for the duration
of each synchronous emit. Targeted `/analyze` reruns for the affected D3D9 proxy/readback/flat,
native-stereo and VR-math targets complete without warnings. Fresh complete Debug and Release
suites each pass **18 tests plus the expected classic-D3D9 shared-texture capability SKIP out of
19**. These changes are **host-tested only** and do not alter the pending physical stereo gate.

Shutdown also remains unresolved in the live evidence. The run package records
`runtimeStarted=true`, `runtimeEnded=false`, `incomplete=true`; the log stops before
`native_stereo_runtime: status=stopped` and `run_end`. Package SHA-256:
`C878419E81C305EB616E32A6E0C1FC0BFE110C8853FA6119936AE9930A5CE91C`.

The later `20260916T153109Z-8976b8f77775` evidence package is also incomplete: readback shutdown
completed, then the log stopped at `native_stereo_shutdown: stage=runtime_begin`. Its evidence
manifest records `runtimeStarted=true`, `runtimeEnded=false`, `incomplete=true`. The candidate
was nevertheless unstaged afterward and the game directory is no longer staged.

The user performs all game and SteamVR launches manually. For the D3D9 live gate,
prepare the run with `tools/stage_d3d9_proxy.ps1`, let the user launch and close the
game, then inspect the resulting evidence with `tools/verify_d3d9_live_test.ps1`.
Use `tools/unstage_d3d9_proxy.ps1` to restore the game directory afterward.

For repeated HMD/native-stereo runs, `tools/vr_test.ps1` is the user-facing wrapper.
`prepare` builds Release, runs the Release CTest suite, creates a dirty-aware exact build
manifest, stages `d3d9_native_stereo` and enables tracking without launching SteamVR or the
game. Normal in-headset recenter uses left PS VR2 Sense Create; `recenter` remains a terminal
diagnostic fallback. `enable`, `recenter`, `disable` and `status` operate on the same staged candidate;
`finish` runs the native-stereo verifier, collects the run evidence and restores staging
after the user has closed the game. The local game-directory preference is stored only in
ignored `work/vr-test.json`. The wrapper is included in the provenance-tools parser gate;
the current Release suite passes with 18 PASS plus the expected classic-D3D9 capability
SKIP. A staged run ID is valid for one game-process start only; launch a fresh prepared run
for every separate observation.

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

On 2026-09-16, before the deferred presenter candidate was staged, repeated manual
launches of the unstaged exact game build crashed immediately after the startup movies.
Windows recorded `0xc0000005`; the generated HotSpot crash report identifies
`ogg.dll+0x274e` from `Sprite.PlayAVI -> IntroModule.PlayIntroMovie ->
IntroModule.MovieFinishedNvidia`. The process module list contains the system D3D9 DLL,
not a CoJ VR proxy, so this crash is not evidence against the deferred transport.
Inspection of the shipped Java bytecode shows `IntroModule.StartIntro()` bypasses the
movie/logo path when `Game.LogosDisabled()` is true, and `Game.ReadExecuteParams()`
recognizes the literal `NoLogos` argument. Physical testing later showed that supplying
`NoLogos` on this installation did not visibly bypass the startup videos, so it is no longer
part of the VR validation procedure. The legacy intro crash remains a separate intermittent
baseline issue rather than evidence against the VR transport.

The first physical deferred-presenter observations then exercised run ID
`20260916T215746Z-362752fe6362` twice without preparing a fresh run between process starts, so
that run ID is not valid formal acceptance evidence. It is still useful defect evidence. In both
processes SteamVR recognized `CoJ.exe` as `VRApplication_Scene` / `steam.app.3020`; the presenter
submitted distinct-eye frames successfully and both processes reached clean OpenVR shutdown plus
`run_end`. The longer process recorded 2,589 new-frame submissions, 5,125 repeated submissions and
zero submit failures. The user reported that once gameplay was active, toggling the game's Escape
menu open and closed restored normal sound and much better perceived performance/camera response,
but SteamVR's dashboard layer remained visibly stuck over the scene.

SteamVR client logs show a lifecycle mismatch relevant to that defect: the presenter entered
`WaitGetPoses` and SteamVR logged `Capturing Scene Focus` roughly 20-45 seconds before the first
D3D11 scene textures existed. Current host-tested source therefore uses
`IVRSystem::GetDeviceToAbsoluteTrackingPose` while no stereo frame is present, starts compositor
`WaitGetPoses` pacing only after a real presentable stereo frame exists, and calls
`PostPresentHandoff` explicitly after each successful stereo pair. It also logs scene-focus PID,
`CanRenderScene`, input availability, dashboard visibility and SteamVR pause/reduce-work hints at
the initialization/frame-ready/first-submit transitions. Debug and Release both pass 20 tests with
the expected classic-D3D9 shared-texture capability SKIP out of 21. This focus-transition change is
host-tested only and requires one fresh run ID for physical validation.

Fresh run `20260916T221254Z-861f3c15abd4`, build manifest
`DE21ABE171A1482AD9B899A5662297346995DED43C91C00704CA5661E7248A65`, then physically exercised
that lifecycle change in one CoJ process. The evidence package is complete (`runtimeEnded=true`,
`incomplete=false`) and shutdown reached `native_stereo_runtime: status=stopped` plus `run_end`.
SteamVR scene focus was still `0` while the dashboard was visible at initialization/frame-ready,
then changed to the CoJ PID on first submit. The user confirmed that the SteamVR dashboard no
longer remained stuck over gameplay and perceived performance was better. The run produced 5,139
collected stereo frames, 5,136 new-frame submissions and 5,439 repeated submissions, with one
submit failure. The user still observed strong head-turn ghosting and an elastic feeling that the
view wanted to return toward its prior orientation.

That symptom exposed a presentation contract defect in the deferred transport. Each engine frame
is rendered from the HMD pose sampled by the presenter/game boundary, then reaches OpenVR later
through the D3D9 ring, CPU mailbox and D3D11 upload. The presenter continued calling
`WaitGetPoses` while that older image was in flight and submitted the texture with
`Submit_Default`, which tells SteamVR to associate the image with the latest compositor pose rather
than the pose actually used for rendering. Repeated frames make the mismatch larger during head
motion. Current source now carries the exact HMD render pose and pose sequence with each captured
stereo frame and uses OpenVR `VRTextureWithPose_t` / `Submit_TextureWithPose` for both new and
repeated submissions. This lets SteamVR reproject from the image's real render orientation instead
of a newer unrelated pose. Fresh Debug and Release builds after this correction each pass 20/21
CTests with only the expected classic-D3D9 shared-texture capability SKIP. The correction remains
host-tested until a fresh physical run validates it.

Fresh run `20260916T224239Z-e43b46698e5c` then physically exercised explicit render-pose
submission. It ended cleanly with `native_stereo_runtime: status=stopped` and the matching
`run_end`, collecting 3,129 stereo frames and producing 3,127 new plus 6,099 repeated submissions;
one compositor submit failure was recorded. The user tested slow and fast head turns plus mouse
rotation and reported that the previous backward pull/snap-back was gone and overall comfort was
substantially improved. Remaining discomfort is now primarily performance/frame pacing rather than
the prior pose-registration defect.

That run also quantified an avoidable hot-path cost. Sampled frames spent roughly `15-22 ms` in
the owned CPU copy and another `11-14 ms` computing full-frame diagnostic hashes before the D3D11
upload. Current source therefore keeps a fail-closed RGB equality check on every eye pair but only
computes the expensive full-frame hashes for the telemetry samples that are actually logged. When
the locked D3D9 surface pitch is already contiguous, CPU extraction now uses one bulk `memcpy`
instead of one copy call per row. The verifier records `distinct_check=rgb_compare_every_frame`
and `hash_mode=sampled_telemetry` so the reduced diagnostic cost cannot be mistaken for weaker
stereo validation. Fresh Debug and Release suites pass 20/21 CTests with the expected capability
SKIP. These performance changes are host-tested only until another headset run measures them.

The initial development host passed the Release D3D9 availability probe and
reported two adapters. The D3D9 proxy smoke test, including native factory,
device and implicit-swapchain observation, passes in Debug and Release. The
optional `EndScene` callback passes repeated real D3D9 scene cycles. The current Debug and Release
suites each produce **20 PASS plus one explicit capability SKIP out of 21 CTests**; the OpenVR
flat, camera-probe and native-stereo proxies build successfully in both.

Repository closeout verification on 2026-09-17 rebuilt the complete current tree in Debug and
Release and reran both CTest presets. Each configuration again produced **20 PASS plus the one
expected classic-D3D9 shared-texture capability SKIP out of 21 CTests**. No manual game or SteamVR
launch was performed. A full clean Debug rebuild with MSVC `RunCodeAnalysis=true` also completed
without code-analysis warnings. The staged performance candidate remained run
`20260916T225750Z-b2515c32a135`; this verification does not promote its host-only performance
changes to live/headset status.

The exact installed `CoJ.exe` SHA-256 was rechecked as
`5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`.
The observed `ChromeEngine3.dll` contains `Direct3DCreate9` and no additional
common D3D9 bootstrap export names checked during host inspection.

The intended physical validation hardware is PlayStation VR2 on PC with both PS
VR2 Sense controllers. A passing desktop/live test does not imply PSVR2 validation.
