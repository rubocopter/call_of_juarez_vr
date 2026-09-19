# Validation model

## Latest arm evidence — 2026-09-19

Run `20260919T153546Z-705460dca03b` is the latest formal single-process physical run. It came from
clean source `32e979709bbb13780cf885c82770a0e8c1649631`, build-manifest ID
`9B5DDBEED941B6C1F5A1E45D39837F2B7BB491CE4C5F6AB8934F86DE255CD671`, proxy SHA-256
`8864D3DFEE42537D0A4E0CBCC230B4E27B48AC273287BFA9FAF508FA0323B97A`. The evidence manifest reports
`runtimeEnded=true` and `incomplete=false`; staging is now clear. Transport fenced 4,312 frames,
collected 4,311, submitted 4,309 new plus 3,828 repeated frames, and recorded zero capture-ring drops
or submit failures. Sampled CPU copy was 10.253 ms average / 13.974 ms p95. The inner presenter still
ended with `shutdown_complete=false`, so that shutdown sub-gate remains open.

The user physically confirmed right-stick snap turn works. Telemetry records 23 exact native snaps
through `PlayerBeing.RotateHorizontally(F)`: 13 left (-45 degrees) and 10 right (+45 degrees). This
promotes the exact 45-degree snap behavior to **headset-validated** for the tested PS VR2 Sense path.
The run also contains two successful Create recenter recoveries with `calibration_preserved=true` and
four immediate `orientation_calibration_mode=preserved_target_rebase` arm samples. The preservation
implementation is therefore **live-exercised**; a dedicated visual recenter gesture would be needed
to promote the absence of orientation jumps as a separate user-visible acceptance claim.

Body telemetry completed 8,452 applications and 8,452 restores with zero writer/restore failures.
The 52.678-second video `clip_1.789.832.536.546.mp4` shows a substantial anatomical improvement over
the earlier catastrophic twist/collapse, but full Body IK remains visually unpromoted. Reach was
still clamped 3,695/8,452 times (43.72%; left 45.41%, right 42.03%). Wrist/hand orientation is still
forced in some poses, local head/hair remains a major HMD intrusion, and the visible weapon/shot-aim
boundary remains unresolved. Video SHA-256:
`B132BD5709B79E8396CF4048B98A88EA70FF0FFFC88F4EE6C27255E5657ACB11`; evidence package SHA-256:
`0AF7723484528C368549F0DB46FE0C682F078B3120CCA3E67F7EA3DD4A1C4BE8`; packaged runtime-log SHA-256:
`57E56F94F963AA121B63BD4DD5C23644655A46C3285069ACC02471DBCA647BE7`.

Post-run source implements the four remaining corrections as the next physical candidate. The arm
adapter keeps the live-proven tracking axes and measured native bone lengths, but remaps ordinary
overreach by at most 12 game units before invoking the existing hard-clamped two-bone solver;
extreme targets still clamp. Telemetry now separates `raw_target_distance`,
`effective_target_distance`, `reach_adjustment`, `reach_adjusted` and the final `target_clamped`.
The measured sibling skinning contract remains authoritative, while shared FORETWIST/hand axial roll
is bounded to 100 degrees and the rejected full wrist residual remains diagnostic.

The exact local player visibility path is also implemented. Static mesh inspection identifies
`RayHead`, `RayHair`, `RayCap`, `BillyHead`, `BillyHair` and `BillyTress`; the bridge resolves them
with the shipped `GetElementID(String)`, records `IsElementHidden(int)`, hides only originally visible
elements and restores only VR-owned changes with `UnhideElement(int)`. Tracking loss and shutdown
explicitly restore the visibility transaction. This prevents whole-actor suppression, but HMD and
shadow behavior require live evidence.

Controller aiming now has separate OpenVR `/pose/tip` inputs while body hands continue to use
`/pose/handgrip`. Shipped `EnumInvHand.class` establishes right=0 and left=1. The exact CoJ bridge
writes the tip-derived world direction into `m_avLookDirDevForHand` after the native game update and
before rendering; `GetFireDirForWeapon` still applies native spread/accuracy and
`GetFireOriginForWeapon` still uses `GetBeingLookFromPoint`. No network-forced attack state is used.
This timing/ownership path is **implemented but not live-proven**; a physical firing test must show
that shots follow the corresponding Sense controller before aiming is promoted.

Fresh Debug and Release builds each complete the 25-outcome CTest suite with **24 PASS plus the one
expected classic-D3D9 shared-texture capability SKIP**, zero failures. Host tests cover within-reach,
small-overreach and extreme-overreach policy, the 100-degree rotation limit, neutral `/pose/tip`
direction mapping, invalid pose rejection and provenance of both tip bindings. JNI visibility and
shot ownership still require the exact running game and therefore remain below live-tested state.

Candidate `20260919T122155Z-c534d86926a9` was prepared from clean source
`075daf3cbeba8abbf6ac389978714d1d85092a9e`, build-manifest ID
`163BBD05C73327FADEEA3B50D4418D11A6C2B0ACAC5B49108389832B667A3702`, proxy SHA-256
`74FDEBE4C09C8D5AD6AE6EFAF6F8FDF80C2D900E4B60CF4AEEB59BBF384E9E1C`. It was subsequently
launched three times under the same run ID, so all resulting evidence is **diagnostic only** and
cannot promote a formal single-process validation state.

The recorded third process (PID 448) produced the user's eight-pose sequence and six recenter events.
It completed 11,406 arm applications and 11,406 restores with zero arm writer/restore failures.
`target_clamped=true` occurred 6,072 times (53.24%); sampled arm lengths averaged 26.6857 upper and
23.1577 lower, 49.8434 game units total. This quantitatively supports the reported short-arm feel and
requires an explicit shoulder/body-anchor or controlled reach-extension investigation rather than
another positional-axis change. Anatomy still fails visually and local head/hair intrusion remains.
The first two starts had the SteamVR dashboard/interface stuck over the game while the third did not,
so dashboard/focus stability remains unpromoted. Evidence manifest:
`docs/research/evidence/20260919T122155Z-c534d86926a9.json`; video SHA-256
`6BA5556AF95EFB3D598FB77BA900A8BE64065AF568EEF0FFB5B4A523289017F5`; runtime-log SHA-256
`4745C85DDA63CD7B7EACE93B49F71EA58DB465ABDA6F78D9E651402BD23348F6`; diagnostic package SHA-256
`09A1F3AC5B30E3238B35311CFD525FBD4443413D2793F0C0E3DEBEAAD189BF17`.

Post-run work fixes the recenter-induced hand-orientation jump by rebasing the new controller
reference against the last visible hand target. It also implements one exact +/-45-degree right-stick
snap per deflection through `PlayerBeing.RotateHorizontally(F)` and holds native analog turn actions
2/3 at zero. Fresh Release validation passes all 25 outcomes with **24 PASS plus the expected
classic-D3D9 shared-texture capability SKIP**, zero failures. The current run above supersedes the
previous host-only status for the exercised snap/recenter paths.

Run `20260919T085408Z-327dd354bc4f` is finalized/unstaged with matching deployed hashes and complete
evidence packaging. It produced 5,050 arm applications and restores, zero arm write/restore faults,
one recenter recovery, 2,843 new plus 2,274 repeated stereo submissions and zero submit failures.
The user's clip `clip_1.789.819.360.960.mp4` still fails anatomy. Inner shutdown is incomplete and
factory-hook restoration reports incomplete, so neither is promoted by the outer `run_end`.

Replay pairs 58 arm samples (116 up/forward axes): FORETWIST after undoing its own rotation agrees
with upper-only propagation to mean 0.000001 / max 0.000007, while upper+forearm propagation misses
by mean 0.583441 / max 1.733806. Hand agrees with upper+forearm without FORETWIST propagation to
mean 0.000001 / max 0.000008. These measurements invalidate the documented serial chain and
explain why joint-target checks missed inconsistent skinning frames.

The replacement candidate explicitly composes FORETWIST sibling swing and hand shared roll. The
writer verifies complete FORETWIST/hand targets within 0.02 unit-vector distance before accepting
an application. The live verifier requires `skinning_frames_reached=true` and finite measured
`skinning_axis_error <= 0.02`; full wrist residual remains diagnostic. Tests cover elbow swing,
both roll signs, native bind-roll preservation and invalid input. Physical improvement is pending.
The replay tool and exact shot-ownership findings are documented in
`docs/research/COJ_ARM_SKINNING_AND_AIM.md`.

Fresh complete Debug and Release builds and CTest suites each pass 24 tests plus the expected
classic-D3D9 shared-texture capability SKIP (25 outcomes, zero failures). This includes the new
skinning math and verifier success/rejection paths. These are host results only.

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

The adapter at this historical gate negated only `physical.yaw_degrees` before applying the CoJ
source transform. Pitch kept the live-confirmed sign and roll remained excluded. This correction was deliberately
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
the locked D3D9 surface pitch is already contiguous, CPU extraction now constructs the owned byte
range directly from the locked surface instead of first value-initializing the full destination
vector and then overwriting it. Padded surfaces reserve the final size and append only real pixel
rows. The verifier records `distinct_check=rgb_compare_every_frame`
and `hash_mode=sampled_telemetry` so the reduced diagnostic cost cannot be mistaken for weaker
stereo validation. These performance changes are host-tested only until another headset run
measures them.

The subsequent Phase 5 acceptance pass adds explicit host coverage for the remaining lifecycle
matrix. `d3d9_stereo_capture` now proves: source-size change on one device/generation rebuilds
capture resources; `InvalidateResources()` releases default-pool ownership before a classic D3D9
Reset and capture resumes on the post-Reset generation; and a second device with identical
dimensions/format receives newly owned resources rather than reusing the first device's objects.
`presentation_cadence` drives a controlled producer pause: after the first successful new-frame
submission, repeated presentation stays available without inventing a new capture; a failed submit
does not consume the pending-new state; producer resumption becomes `new` again; explicit
invalidation removes stale presentable content. Fresh full Debug and Release suites each produce
**21 PASS plus the one expected classic-D3D9 shared-texture capability SKIP out of 22 CTests**.
No Call of Juarez or SteamVR process was launched for this host validation.

The subsequent Phase 6 host pass formalizes OpenVR lifecycle/ownership without requiring a live
runtime. `openvr_state` simulates focus loss, one-eye submit failure, invalid tracking, runtime/HMD
disconnect and shutdown, and also proves that a second process-level owner is rejected until the
current owner releases the gate. `openvr_runtime` covers safe move construction/assignment for
uninitialized runtime objects, while production move-assignment now shuts down any currently owned
runtime before adopting another `Impl`. The real adapter drains relevant OpenVR events, tracks
initialized/connected/focused/tracking-valid/presenting/shutdown state, and the presenter stops
submitting while connection/tracking is invalid. State transitions are emitted as
`openvr_runtime_state` telemetry. Runtime methods that retain `noexcept` now contain
allocation-capable error/reporting paths and fail closed if reporting itself raises; moved-from
objects are also safe for non-live state/pose/eye queries.

The D3D11 upload/handoff policy is now explicit. Production remains
`UpdateSubresource -> gpu_sync=none -> Submit_TextureWithPose -> PostPresentHandoff`; no global GPU
wait was added. `d3d11_sync` exercises controlled `none`, `Flush` and bounded event-query fence
strategies on a WARP D3D11 device, including failure on invalid inputs. The isolated OpenVR probe
now accepts a configurable frame count, renders a recognizable animated/distinct eye pattern and
can select those synchronization strategies for later manual A/B evidence. This probe change is
implemented/host-built only; it was not launched against SteamVR or a headset in this pass.

Fresh full Debug and Release builds after the final Phase 6 owner-policy change each pass
**23 tests plus the one expected classic-D3D9 shared-texture capability SKIP out of 24 CTests**.
No Call of Juarez or SteamVR process was launched, so these Phase 6 changes remain host-tested.

The native-stereo verifier/evidence path now batches the next physical observations into one fresh
run. A run staged after this change requires: the production `gpu_sync=none` D3D11/OpenVR handoff;
connected, tracking-valid and presenting runtime state; at least one repeated-frame submission;
one deliberate SteamVR-dashboard focus loss followed by scene-focus reacquisition; and a final
`shutdown_complete` runtime state. The user should perform that dashboard cycle during stable
gameplay, then include slow and fast head turns, mouse rotation and the already established left
PS VR2 Sense Create recenter before disabling the candidate.

`tools/vr_test.ps1 finish` now performs the live verifier, generates a run-bound
`cojvr-native-stereo-summary.json`, collects the evidence package and restores the staged files.
The summary reports count/average/p50/p95/max for available GPU-copy queue, fence/readback,
CPU-copy, producer, diagnostic-hash, D3D11-upload, pose-wait and submit timing samples; it also
records transport counters, the OpenVR focus cycle and shutdown completion. Evidence collection
requires and copies that summary for native-stereo runs, preventing the next expensive physical
session from ending with only qualitative performance observations. These additions are
host-tested only until that fresh headset run occurs.

The initial development host passed the Release D3D9 availability probe and
reported two adapters. The D3D9 proxy smoke test, including native factory,
device and implicit-swapchain observation, passes in Debug and Release. The
optional `EndScene` callback passes repeated real D3D9 scene cycles. The current Debug and Release
suites each produce **23 PASS plus one explicit capability SKIP out of 24 CTests**; the OpenVR
flat, camera-probe and native-stereo proxies build successfully in both.

Final repository verification on 2026-09-17 rebuilt the complete current Phase 5/6 tree in Debug
and Release and reran both CTest presets. Each configuration produced **23 PASS plus the one
expected classic-D3D9 shared-texture capability SKIP out of 24 CTests**, with zero failures. No
manual game, SteamVR or headset launch was performed, so the performance/cadence and Phase 6
runtime-state changes remain host-tested. An earlier clean Debug rebuild with MSVC
`RunCodeAnalysis=true` completed without warnings before the final Phase 5/6 expansion; it is
retained as historical static-analysis evidence rather than claimed as a fresh analysis of the
final 24-test tree.

That staged run was later confirmed never to have started: only its stage/run manifests existed,
with no matching process log or evidence payload. It was transactionally unstaged before the
Phase 5 acceptance/copy changes above so that a later `vr_test.ps1 prepare` cannot accidentally
reuse the obsolete artifact.

The exact installed `CoJ.exe` SHA-256 was rechecked as
`5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`.
The observed `ChromeEngine3.dll` contains `Direct3DCreate9` and no additional
common D3D9 bootstrap export names checked during host inspection.

The intended physical validation hardware is PlayStation VR2 on PC with both PS
VR2 Sense controllers. A passing desktop/live test does not imply PSVR2 validation.

### Positional 6DOF and body/IK preflight — 2026-09-17

The user explicitly advanced positional/body work after the native-stereo camera path became
usable. Current source keeps the HMD as render authority while reconciling horizontal recentered
HMD translation into the native player actor at `100` CoJ units/metre. The adapter removes the
previously accepted room-scale component before applying the next one so normal locomotion is not
accumulated twice; vertical HMD translation remains camera/body-only. HMD and both controller-role
poses now originate from one OpenVR tracking sample and both Sense poses are transformed through
the same `RelativePoseTracker` recenter basis. Run `20260917T172007Z-e6232c4778d2` subsequently
live-proved the exact campaign actor route and successful player reconciliation; broader comfort
promotion still depends on the remaining presentation/body gates.

Exact-build body discovery now attaches to the already-created Java 1.4 VM and resolves the local
player/being without creating a second JVM. The bridge exposes fail-closed JNI wrappers for shipped
`GetBoneJointPos(BLVector;)Z`, `GetBoneDirVector(BLVector;)Z`,
`GetBonePerpVector(BLVector;)Z`, `GetMeshElemFromBoneID(B)I` and
`FromUpForwardPosElementWorld(ILVector;LVector;LVector;)V`. Static shipped-bytecode inspection of
`ArmedPlayerBeing.UpdateLookPointDead(F)V` independently shows `GetBoneJointPos(5)`,
`GetBoneDirVector(5)` and `GetBonePerpVector(5)` being combined as joint/up/forward geometry.
The inspected `ChromeEngine3.dll` registration/disassembly resolves the world-basis writer to the
exact native handler around RVA `0x0009A350`; production code still calls it through the Java
method instead of embedding native object offsets.

The runtime now contains a unit-agnostic two-bone solver with reach clamping and pole-vector
control. The CoJ adapter measures shoulder/elbow/wrist segment lengths from the animated skeleton,
derives the bend plane from the current elbow, and rotates the current upper-arm/forearm up/forward
bases by the shortest arc toward the tracked controller target. This preserves current animation
twist while solving the chain. Controller world targets use the natural camera basis and subtract
the room-scale world offset already absorbed by the actor before applying the controller's
recentered metre position. The exact arm writer is guarded by `bodyIkEnabled`; disabled mode still
emits sampled observation telemetry. Hand orientation remains game-natural, and pelvis/leg writes
are intentionally not promoted before the arm world-basis composition has physical evidence.

The lower-body preflight is now host-tested without enabling a pelvis/leg writer. Sampled frames
read the exact pelvis/thigh/shin/foot joints plus direction/perpendicular bases from the actor
contract, preserve the native actor-to-pelvis offset as the locomotion-rooted pelvis anchor, and run
both legs through the measured two-bone solver. Reach is clamped, the knee plane is derived from the
current animated chain, and foot orientation remains native. `body_lower_tracking` records the
pelvis target, hip/knee/ankle positions, measured segment lengths, clamp state and knee-plane
validity with `write_enabled=false`.

The native-stereo run manifest/verifier now includes `requireBodyIk`. Body/full-profile runs enable
IK before launch so the test does not depend on Alt+Tab or an in-process terminal toggle. The
verifier requires at
least one successful `body_arm_tracking result=applied` sample for each side with
`plan_valid=true`, `write_ok=true`, the measured bone basis and the exact world-basis writer. The
same body gate also requires one valid read-only `body_lower_tracking result=observed` sample for
each leg with `plan_valid=true`, `knee_plane_valid=true` and `write_enabled=false`. This gathers the
lower-body geometry/solver evidence in the same process without promoting a lower-body writer. The
body/full profile deliberately does **not** require a SteamVR dashboard cycle: that presentation
gate remains in the separate performance profile so a stuck system overlay cannot invalidate the
arm/body-composition test. The body run still requires positional 6DOF, recenter, repeated-frame
presentation, production `gpu_sync=none` and clean shutdown.

Fresh Release on 2026-09-17 builds the complete tree including `d3d9_native_stereo.dll`; all **25
CTest outcomes complete with 24 PASS and the one expected classic-D3D9 shared-texture capability
SKIP**, with zero failures. Debug `camera_probe`, `body_adapter`, `openvr_runtime` and provenance
coverage also pass. No Call of Juarez, SteamVR or headset process was launched for this increment,
so arm application and positional/body comfort remain **host-tested**, not live/headset-validated.

Run `20260917T153051Z-ac6a4be37d85` is diagnostic evidence for the next physical workflow, not a
body/IK promotion. The process produced sustained stereo submission and multiple dashboard
open/close transitions. The `body-enable` control update was accepted (`body_ik_enabled=true`), but
after that update the producer emitted no new game captures and the presenter only repeated the
last `capture_sequence=3455`; no valid arm application was therefore possible. The user reported
that returning through Alt+Tab commonly crashes or stalls the game, matching the observed loss of
new game frames. The run also ended with `native_stereo_presenter_stop: shutdown_complete=false`
even though the proxy reached `native_stereo_runtime: status=stopped` and `run_end`, so it does not
promote the shutdown-complete gate. Its evidence package SHA-256 is
`27C389D1801F79F5F8AEF6C365CAF4A1D45B25B16768489289987403F2536AB1`.

For the next body run, `tools/vr_test.ps1 prepare -BodyIkAtStart` enables the guarded arm overlay
before SteamVR/CoJ are launched. This avoids using Alt+Tab merely to toggle body IK while preserving
the existing `body-enable` / `body-disable` actions for runs where an in-process toggle is safe.

Run `20260917T154759Z-211723af9dc8` exercised that start-enabled path. The SteamVR interface was
usable and stereo/6DOF/recenter continued to run, but the user saw no body or Sense-driven arm
movement. Telemetry identified the blocker before any skeleton/write stage: every sampled body
update reported `stage=player;detail=local player is unavailable`. The body control itself was
accepted from frame 1, so this was not another toggle/Alt+Tab failure. The run was finalized and
packaged as SHA-256 `8431C07F85B96F9A9339680C90F76788902710ECE434174A96C7A45C50D9DC82`.
It again ended with `native_stereo_presenter_stop: shutdown_complete=false`, so shutdown remains a
separate failed gate.

Static inspection of shipped `Session.class` explains the discovery failure: `sm_LocalPlayer` is
assigned in `Session.PlayerCreated` only when `NetPlayer.GetNetIsOwner()` returns true, while all
players are independently retained in `sm_Players`. Current host source therefore keeps
`sm_LocalPlayer` as the primary exact-game route and falls back to the sole `sm_Players` entry only
when the vector size is exactly one. Zero or multiple players remain fail-closed. The next candidate
also emits sampled `body_tracking_input` telemetry containing left/right Sense position/orientation
validity before player/skeleton processing, and the live verifier requires both poses to be valid
for the body gate. Release builds successfully and the complete 25-test suite still reports 24 PASS
plus the expected classic-D3D9 shared-texture capability SKIP; focused provenance/camera/body tests
also pass after the verifier change. Run `20260917T172007Z-e6232c4778d2` later live-proved the
campaign discovery route; the Session-only fallback remains historical context for the earlier
failed runs.

Run `20260917T161917Z-909b63e114af` exercised that fallback. Sampled `body_tracking_input` proves
both Sense controllers delivered valid position and orientation and their positions changed as the
user moved them. Body processing still stopped before skeleton access because campaign discovery
reported `local player is unavailable and the session player list is empty`. The user's observation
that neither arm followed either Sense therefore matches the telemetry: tracked input is present,
but no native actor exists at that Session-based Java discovery boundary.

Static inspection of the shipped single-player classes now supplies a concrete campaign route.
`LawmanGame.class` owns static `sm_cActiveGameModule : LawmanModule`; `LawmanModuleSingle.class`
owns `MainPlayer : PlayerBeing` and exposes `GetMainPlayer() : Being`. `JavaPlayerBridge` keeps the
Session/NetPlayer routes first, but when the Session player vector is empty it reads the active game
module, verifies with JNI `IsInstanceOf` that it is specifically `LawmanModuleSingle`, and calls
`GetMainPlayer()`. Ambiguous Session state and non-single-player modules still fail closed.
`body_player_discovery` identifies this route as `lawman_module_single_main_player`.

Run `20260917T172007Z-e6232c4778d2` physically reached that path. Telemetry records
`player_source=lawman_module_single_main_player`, valid/changing position+orientation for both Sense
controllers, successful `body_player_reconciliation result=ok`, and left/right
`body_arm_tracking result=applied` with `write_ok=true` through
`FromUpForwardPosElementWorld`. The user also observed that both arms now moved with the physical
controllers. That proves controller tracking, campaign actor discovery, skeleton lookup and native
writer invocation in one exact-build process.

The same run **failed the arm-composition physical gate**. Both arms twisted sharply behind/above
the body and produced severe mesh corruption; the user's shadow made the reverse/overhead arm pose
visible even when the first-person geometry was an unreadable mass. The successful writer return
therefore cannot be used as evidence that the transform semantics were correct. The run finalized
with `run_end`; its evidence package SHA-256 is
`86CC35F6EAE2D8EB0C88C97D24723ACE10EE637D93A4368F55F18D09A7339EFB`.

Exact `ChromeEngine3.dll` registration/disassembly after that run identifies the composition bug.
`FromUpForwardPosElementWorld` consumes a complete element world transform: it normalizes forward,
derives the first axis as `up x forward`, rebuilds up and stores those axes together with the passed
world position. The previous arm path instead supplied the bone joint as the element position and
used `GetBoneDirVector`/`GetBonePerpVector` as the write frame. That discarded each mesh element's
real origin/bind offset from the joint and explains why mathematically valid IK positions could
still contort the rendered arm.

Current source now reads the natural element world position with `GetElementPos` and its stored +X
and up axes with `GetElementLeftVector`/`GetElementUpVector`. The shipped
`GetElementForwardVector` handler writes its output but returns false unconditionally, so forward is
reconstructed from the paired +X/up axes rather than accepted through that broken boolean contract.
`BuildArmIkPlan` rotates the complete natural element frame around the measured joint pivot; upper
arm preserves its offset from the shoulder, while forearm first moves the pivot from animated elbow
to solved elbow and then reapplies the rotated native offset. Focused host coverage now asserts both
pivot-radius preservation and that a no-op IK target leaves the exact natural element frame
unchanged. Fresh complete Debug and Release suites each produce **24 PASS plus the one expected
classic-D3D9 shared-texture capability SKIP out of 25**, with zero failures.

The same run kept the active comfort failure visible. Sampled `native_stereo_producer_timing`
reported approximately `15-25 ms` of `cpu_copy_ms` per stereo frame at the user's `2560x1440`,
`FSAA(8)` settings, before the cost of the two Chrome Engine eye passes. The user also confirmed that
small physical HMD translation could move the render camera away from the stationary character body,
forcing frequent recenter. Current host source now treats unresolved actor reconciliation as a
rotation-only fallback: physical HMD translation is zeroed while orientation validity and per-eye
IPD translation remain active. `camera_native_stereo_frame` reports the real
`positional_6dof` state plus `translation_mode` instead of unconditionally claiming 6DOF.

The next physical gate is therefore performance-focused. `tools/vr_test.ps1 prepare` uses a
`performance` validation profile unless `-BodyIkAtStart` is explicitly requested; that profile does
not require positional-6DOF/body promotion. It also applies a reversible `Video.scr` profile of
`1920x1080` and `FSAA(0)` while preserving every other setting, records the original/applied hashes
in the run manifest and restores the original file on `finish` or prepare rollback. `-KeepVideoSettings`
is available for diagnostics that must retain the user's native configuration. Fresh Release build
and CTest after these changes complete with **24 PASS plus the one expected capability SKIP out of
25, zero failures**. DLSS is deferred at this stage because the measured dominant cost is still the
classic-D3D9 CPU transport/double-render path; an upscaler would not remove that copy.

The resulting performance candidate was exercised and finalized as run `20260917T163732Z-03df947b8d50` with
build-manifest ID `52C4B1822E798CE84BBC8F878E80760AB97DCB729C5C22860BB99E044B935FFC` and proxy SHA-256
`287CC00832F64227DE4912BD7F945573355F0EE04A5A99A3E4B36850831957E3`. Its run manifest records
`profile=performance`, `requirePositional6Dof=false`, `requireBodyIk=false` and the active
`1920x1080`/FSAA0 video profile. The exact original `Video.scr` SHA-256 is
`9C7C51A2E46C60B775DDF1FB27022CD5179D53A039D1A8F64EB30C0286426191`; the temporary staged file is
`72A92EBCD87FF68D8E63AE138D6528AE5F215AA0B23A420CE2D3EC76CE0D4D49`. Preparation reran all 25
Release CTest outcomes with 24 PASS plus the expected capability SKIP. The finalized summary records
`cpu_copy_ms` average `10.246`, p50 `9.982`, p95 `12.937` and max `15.685`, with 3165 collected
frames, 3163 new submissions, 2042 repeated submissions, zero capture-ring drops and zero submit
failures. The user also confirmed that physical head movement no longer visibly leaves the native
body behind. Telemetry shows that this was the intended unresolved-actor fallback:
`positional_6dof=false;translation_mode=rotation_only_actor_unresolved` suppressed room-scale head
translation. Both Sense poses remained valid and changed with physical controller movement, but
`body_ik_enabled=false` by design in this performance profile, so this run cannot validate arm
motion. Presenter finalization still reported `shutdown_complete=false` before the outer runtime
reached `status=stopped` and `run_end`; do not promote the clean-shutdown sub-gate from this run.
Evidence package SHA-256:
`C16C889C155F9B1A7C09D7D06DEA6FF9D3B600A97C3D65C49BE46F026935C4CE`.

Candidate `20260917T175054Z-386939a46734` was later found to have no `cojvr.log`; it never supplied
run evidence. It was unstaged and the original `Video.scr` was restored. The next full-profile
candidate must therefore use a fresh run ID and the isolated body workflow above: IK enabled before
launch, no Alt+Tab requirement, and no SteamVR-dashboard cycle in the body gate.

A repository audit of the current corrected-arm tree found one additional host-side failure path:
upper-arm and forearm element writes were issued as two independent JNI calls, so a successful
upper-arm write followed by a failed forearm write could leave a partially applied arm for that
frame. The body adapter now treats the pair transactionally at its integration boundary. If either
write fails, it attempts to restore both upper-arm and forearm to the natural element world frames
read before the write. Telemetry records `rollback_attempted`/`rollback_ok` and distinguishes
`rollback_failed` from an ordinary `write_failed`; the live verifier accepts an `applied` arm sample
only when no rollback was required. Provenance coverage also rejects synthetic `applied` evidence
that claims a rollback occurred.

Fresh Debug and Release builds after this hardening complete all 25 CTests with **24 PASS plus the
one expected classic-D3D9 shared-texture capability SKIP**, with zero failures. The modified
`camera_probe`, `java_player_bridge` and `body_adapter` sources also produced empty MSVC native code
analysis defect sets during the repository audit. No Call of Juarez or SteamVR process was launched,
so the corrected element-frame composition and transactional arm-write behavior remain host-tested
until a fresh physical body run exercises them.

Run `20260917T220704Z-b57a36497e54` subsequently exercised the current tree with the separate
`performance` profile. Its build-manifest ID is
`9E2B4F538496B691CFE26663CE990306738D2B9CCBC1F4B17CB7DE2F75FBA615`, and the staged proxy SHA-256
was `E5A4487023E63873B161625C590C7ABD3944E017C63F0352D3A1694FCF850A5F`. The manifest explicitly
sets `requireBodyIk=false` and `requirePositional6Dof=false`; sampled `body_arm_tracking` remained
`result=observed` with `write_enabled=false`, so body/arm composition is outside this run's
acceptance scope. The summary records 4,471 collected frames, 4,469 new submissions, 2,075 repeats,
zero capture-ring drops, zero submit failures and only two mailbox replacements. Sampled
`cpu_copy_ms` is `9.412` average, `8.888` p50, `14.808` p95 and `17.313` max; sampled D3D11 upload
is `1.019 ms` average. Distinct-eye content, explicit render-pose submission, physical recenter and
focused/tracking-valid presentation were all observed.

The evidence package is complete at the outer provenance level (`runtimeEnded=true`,
`incomplete=false`) but the live acceptance gate still fails. The only recorded dashboard
open/close events precede the first `presenting=true` transition, so there is no valid post-active
dashboard cycle followed by resumed presentation. The run also lacks the explicit
disable-to-natural-passthrough events required by the verifier. Finalization emitted
`native_stereo_presenter_stop: shutdown_complete=false`; the outer runtime nevertheless reached
`native_stereo_runtime: status=stopped` and `run_end`. Do not promote performance, dashboard-cycle,
passthrough-disable or clean-presenter-shutdown acceptance from this run. Evidence package SHA-256:
`0078E0700F7177A361A2D8C95A051741B7D099AA9EA535E85D68D542B865B04A`.

Full/body run `20260917T222204Z-28ac69c31c56` then exercised the corrected natural-element-frame
arm writer. Build-manifest ID was
`48D7D4F31E96C477DD441B18F1DC50654CA40B3C3A41DF715A95C491ABEE6D50`. Both Sense poses were valid,
both arms reached `body_arm_tracking result=applied`, and the user reported that each arm followed
its corresponding controller without the previous unusable mesh corruption. The physical arm gate
still failed: the apparent forward/back relationship was reversed, so moving the real arms behind
the body brought the virtual arms forward.

Run telemetry identifies a world-space anchoring defect rather than sufficient evidence for a raw
axis-sign flip. At frame 1 the left/right recentered controller positions were approximately
`(0.012,-0.406,-0.160)` and `(0.069,-0.473,-0.268)` metres, but the generated controller targets
were only `(1.24,-40.64,15.99)` and `(6.94,-47.27,26.77)` game units while the animated shoulders
were around `(39718,3663,29407)` and `(39687,3663,29426)`. The arm solver therefore received targets
near the global origin and clamped both chains toward that distant point. `ReadNaturalCameraFrame`
provides the renderer-local camera translation at this boundary and cannot serve as the skeleton
world anchor.

Current host source maps each recentered hand around the exact native head joint instead. It uses
`tracked_hand - tracked_head` in the shared recenter space, maps that offset through the proven CoJ
right/up/forward basis at `100` units/metre, and adds it to bone 5's live world position. Telemetry
now records `head_anchor`, `tracked_head` and `tracked_hand` alongside `controller_target` so the next
physical run can verify that targets occupy the same world neighborhood as the skeleton. Debug and
Release each pass all 25 CTest outcomes with **24 PASS plus the expected classic-D3D9 shared-texture
capability SKIP**. This anchoring correction is host-tested only. Evidence package SHA-256 for the
failed physical run: `6B182844CAC4B5C9BAF9E993961C47836012F733C437CEE82EF9D31237741A43`.

Final repository verification before committing this state rebuilt both Debug and Release from the
current tree and reran both complete CTest configurations. Each produced **24 PASS plus the one
expected classic-D3D9 shared-texture capability SKIP out of 25**, with zero failures. No game or
SteamVR process was launched during this verification.

Run `20260917T223157Z-8061a216a065` physically exercised the head-anchored hand-target correction
(build-manifest ID `098655492B17DD9C255B4DE0643F477EAA8B1ECE107855F7D38ED941DD3BB50F`,
proxy SHA-256 `6E179177A9E197DF2A609D0348BA59C149359FA7AC0829261989984EDEEA20CC`).
The target-space correction worked: sampled left/right controller offsets and generated targets
remained on the corresponding shoulder sides and in the live actor/skeleton neighborhood. For
example, one extended-left sample had tracked hand-minus-head approximately
`(-0.941,-0.134,0.160)` m with target-minus-shoulder approximately
`(73.9,-2.2,-18.5)` game units; the right side showed the corresponding mirrored relationship.
This removes controller tracking, recenter-space anchoring and the previous global-origin clamp as
the primary explanation for the remaining visible reversal.

The same run still **failed the physical arm-composition gate**. With the user's real arms extended
in a T-pose, both virtual arms remained backward/contorted. The SteamVR interface/dashboard also
became stuck over the game again during the run. Record these as two separate failures: native arm
pose application remains invalid, and presentation/focus has regressed independently of the arm
target mapping. An earlier dashboard/focus pass therefore does not close the presentation issue
globally.

Exact-build static inspection after this failure found a better native boundary.
`SetBoneOrientation(BLVector;LVector;)V` is registered but its exact handler at RVA `0x00096D60`
does not provide a usable orientation write in this build. The registered
`BoneRotate(BLVector;FZ)V` handler at RVA `0x0009B6A0` performs real hierarchy work: it validates
the bone mapping, reads the supplied vector, converts the supplied degree angle using the native
degrees-to-radians constant, updates the bone path and refreshes affected descendants. The overload
`BoneRotate(BLVector;FZF)V` at RVA `0x0009B4C0` follows the same hierarchy route with one
additional float parameter.

Current host source replaces active arm mesh-element world writes with the simpler
`BoneRotate(BLVector;FZ)V` contract. It computes the shortest-arc upper-arm delta from the natural
shoulder->elbow segment to the solved segment, applies that parent delta to the natural lower segment
before deriving the forearm delta, and fails closed for degenerate rotations. A host regression test
covers a straight chain rotated 90 degrees and proves the forearm receives a zero delta when parent
rotation already places it correctly; mirrored T-pose no-op coverage prevents an unintended
left/right correction. Since `BoneRotate` is relative, the overlay exists only across the two stereo
eye draws and is restored forearm-before-upper-arm immediately after capture. A failed restore
blocks subsequent arm writes, and the live verifier now requires successful
`body_arm_restore result=ok` evidence for both sides together with `writer=BoneRotate`.

Fresh Debug and Release builds of this hierarchy-writer tree complete all 25 CTests with **24 PASS
plus the expected classic-D3D9 shared-texture capability SKIP**, zero failures. No game or SteamVR
process was launched for this implementation, so `BoneRotate` arm composition remains
**host-tested**, not live/headset-validated.

Run `20260917T230423Z-e63b9146cea7` then physically exercised that `BoneRotate` candidate. The
run is finalized and unstaged; its evidence package SHA-256 is
`1371671F7D9F5310A430D583D769AD91EEDBCF7B9896FC3274CBDD86BABC9D95`. Runtime provenance is
complete (`runtimeStarted=true`, `runtimeEnded=true`, `incomplete=false`). Transport remained
stable with 5,614 collected frames, 5,611 new submissions, 2,669 repeats, zero capture-ring drops
and zero submit failures. Sampled CPU copy was `9.838 ms` average / `9.600 ms` p50 /
`13.029 ms` p95. Presenter shutdown still reported `shutdown_complete=false` before the outer
runtime reached `status=stopped` and `run_end`.

The native arm calls did execute: telemetry contains 4,524 left and 3,332 right
`body_arm_tracking result=applied` samples, with the same number of successful post-capture
`body_arm_restore result=ok` records. Those records prove only that the JNI `BoneRotate` calls and
their inverse calls returned without an exception. They do **not** prove that the altered hierarchy
was consumed by the mesh rendered for either eye. Physical observation failed that gate: the
SteamVR interface became stuck over gameplay and the portion of the player body visible behind it
appeared to remain in the game's natural animation, with no observable Sense-driven arm response.
Do not promote Body IK from this run.

The same run independently reproduced the SteamVR dashboard failure. CoJ acquired and retained
scene focus (`scene_focus_process_id` remained the CoJ PID), while dashboard-visible state toggled
repeatedly and the final recorded `dashboard_opened` had no later close before the game exited.
The left-Sense Create recenter event occurred between an earlier dashboard close and a later open,
so this evidence does not support recenter as the trigger. Dashboard usability therefore remains a
presentation failure separate from the arm-composition gate.

That host source added two discriminating arm probes. `body_arm_write_probe` re-reads joints and
mesh-element world frames immediately after a successful `BoneRotate`; `body_arm_render_probe`
re-reads them after each complete eye render. This distinguishes a native call that returns success
but leaves geometry unchanged from one whose change is later overwritten before/during render.
The OpenVR presenter also treats the SteamVR dashboard as a system-owned presentation state: while
it is visible, CoJ stops compositor-paced scene submission and global game-action polling, retains
the newest scene frame, and resumes submission after the dashboard closes. Debug and Release both
complete all 25 CTest outcomes with **24 PASS plus the expected capability SKIP**, zero failures.
These changes are **host-tested only** until a fresh physical run.

### Visible arm-writer discrimination and liveness instrumentation — 2026-09-18

Run `20260918T160300Z-cd4137a48fca` physically exercised the `BoneRotate` candidate with the
write/render geometry probes active. It closes that writer question for the current exact build:
all 96 sampled `body_arm_write_probe` records reported `changed_from_natural=false`, and all 192
sampled `body_arm_render_probe` records across the two eye draws also reported
`changed_from_natural=false`. Tracking, head-relative controller targets and the measured two-bone
solution remained valid. Successful JNI calls and inverse calls therefore did not produce an
observable render-element mutation; `BoneRotate` is rejected as the active arm writer unless later
native evidence contradicts this run.

Exact-build registration/disassembly identifies `RotateElement(ILVector;F)V` at RVA `0x00099F90`,
`RotateElementWithChildren(ILVector;F)V` at RVA `0x0009A070` and `CopyXformAnimToElement(I)Z` at
RVA `0x00099E70`. The selected `RotateElementWithChildren` handler reads the current element world
matrix, composes the supplied relative axis/angle rotation into that live transform, writes it back
and refreshes descendants/attached child state. Shipped `ArmedPlayerBeing.UpdateBodyRotation`
bytecode also uses inherited element-rotation calls with degree-valued angles, supporting the
element-relative semantics. This boundary preserves the current animated element origin/bind state
instead of recreating the earlier broken absolute `FromUpForwardPosElementWorld` transform.

Current source keeps the existing controller target mapping and two-bone solver. Immediately before
the VR overlay it samples the natural upper-arm/forearm geometry, applies upper-arm then forearm
relative element rotations, and requires a measurable geometry change whenever the planned delta is
non-zero. The changed geometry must remain observable after the left-eye and right-eye render passes
and match the post-write sample. After stereo capture, inverse rotations run forearm before upper arm
and the bridge re-reads geometry to require an exact return to the natural sample. Failure to write,
observe a required mutation, preserve it through either eye or restore it disables further arm
writes. The live verifier now requires `writer=RotateElementWithChildren`, immediate
`changed_from_natural=true`, changed/matching geometry after both eyes, and
`geometry_restored=true` for both sides. Host provenance fixtures explicitly reject successful JNI
records with natural geometry, rollback-dependent `applied` samples and failed restoration.

The post-load freeze is instrumented separately. `post_load_liveness` telemetry observes
`GetLastInputInfo`, foreground/focus/GUI-thread state, `GetInputState` for pending keyboard/mouse
input and natural camera/frame progress when real input changes or periodic samples occur. Static
inspection also found a safe exact-game timer signal: `Module.IsTimerFreezed()Z` is inherited by
the already-proven `LawmanModuleSingle`, so the bridge reads it from
`LawmanGame.sm_cActiveGameModule` without guessing a timer object or mutating timer state. Samples
therefore include `game_timer_valid`/`game_timer_frozen`. The probe does not `PeekMessage`, consume
messages, hook the window procedure or synthesize keyboard/mouse input. The next physical run can
correlate the first real input with render, focus, queue-input and game-timer state without changing
the condition under test.

Fresh Debug and Release builds of this tree complete all 25 CTest outcomes with **24 PASS plus the
expected classic-D3D9 shared-texture capability SKIP**, zero failures. No game or SteamVR process
was launched for the `RotateElementWithChildren` implementation, so this writer remains
**host-tested** pending a fresh full/body run. Pelvis/leg writes remain disabled.

Candidate `20260918T164932Z-1daf676be48e` was prepared before the direct game-timer liveness signal
was added. It produced no `cojvr.log` and must not be used or reused for the next physical gate. A
fresh full/body candidate must be prepared after the current Debug/Release host suites pass so the
arm geometry probes and the complete post-load liveness instrumentation share one exact artifact.

That replacement preparation completed as run `20260918T165754Z-845101e7557b`. It rebuilt Release
and reran the complete suite with **24 PASS + 1 expected SKIP**, then staged build-manifest ID
`51EE466E23FFE5DC16DA502DF5078FFCAC9B3F7C56ED9EBF23CC6997E72B8670` and proxy SHA-256
`15AFB4220800DE7C0AF8F5743CB2C291D58A520926AF169525D076214929287E`. The manifest uses the `full`
profile, tracking/native stereo and Body IK are enabled before game start, and the reversible
`1920x1080`/FSAA0 video profile is active. This candidate contains both the
`RotateElementWithChildren` geometry-phase gate and the direct game-timer liveness signal. It is
now finalized physical evidence rather than candidate provenance. Evidence package SHA-256:
`02A6D8EA4DBEF452A01142AC66100F88B982C5B4D4BFB2022F86A2466360DFE4`.

The run proved that `RotateElementWithChildren` reaches the visible render mesh: both arms changed
immediately, remained at the same changed geometry through both complete eye draws and usually
inverse-restored to the natural sample. The user briefly saw controller-driven arm movement, but
the arms were wrongly oriented and deformed, so the body gate failed. Telemetry explains both that
failure and the later return to fixed game arms. The solver generated world-space shortest-arc axes,
but exact handler disassembly at helper RVA `0x001F5700` shows that the element matrix is
post-multiplied by the supplied axis-angle matrix; the Java axis is therefore element-local. Passing
the world coordinates as local coordinates reproduces the observed wrong pose numerically. At
frame 470 the right inverse calls returned success but the geometry comparison failed; frame 471
then disabled both writers by design and the native game animation resumed. The game had already
been out of foreground focus by frame 450 while arm writes continued, so focus loss coincided with
the observation but did not directly reset the writer.

Current host source converts each world-space solver axis into the live element-local frame before
calling `RotateElementWithChildren`. It converts the upper arm from its natural frame, applies it,
then re-reads the child forearm frame before converting/applying the forearm axis. Post-write
validation now additionally requires the rendered elbow and wrist to be within `0.5` game units of
their solved targets; a merely changed but misdirected mesh cannot pass. Restoration still attempts
the exact inverse child-before-parent, but a floating-point geometry mismatch now reapplies the
captured complete natural upper/forearm world frames and verifies that exact snapshot before the
next frame. Failure of both paths remains fail-closed. The verifier requires
`native_axis_space=element_local`, `targets_reached=true`, both-eye persistence and exact natural
restoration. Pelvis/leg writes remain disabled.

Fresh Debug and Release builds complete all 25 CTest outcomes with **24 PASS plus the expected
classic-D3D9 shared-texture capability SKIP**, zero failures. A first full-suite attempt while
SteamVR was still active made six native D3D9 device-creation tests return
`D3DERR_NOTAVAILABLE`; after SteamVR was manually closed, both configurations returned to the
expected clean result. No game process was launched for this correction.

The first post-fix prepare attempt, run `20260918T204452Z-d196ba97e34c`, was rejected before any
manual launch. `vr_test.ps1` built and tested `build-win32` but generated/staged its manifest from a
stale `build/win32-debug` DLL; the staged proxy therefore retained the previous candidate SHA-256
`15AFB422...9287E` instead of the corrected `build-win32` SHA-256
`561CE9FF...2ADE3`. The same script also recreated the explicitly rejected D3D9Ex marker. The
candidate was transactionally unstaged, its video profile restored and its run ID must not be used.
Current tooling binds the manifest and staging to `build-win32/Release/d3d9_native_stereo.dll`,
keeps the exact-game D3D9Ex marker absent, and has provenance tests that reject either regression.

Corrected candidate `20260918T204701Z-f561e493f4ab` was physically exercised and finalized. Its
`full` manifest required Body IK and positional 6DOF, used build-manifest ID
`E7AE926955FEFDC24A66B77ECC719FD35D3E02E4EA4F3C9729EF48C84ED8D697` and proxy SHA-256
`561CE9FFDCB97E174440FD265F56088AA23EA5383D2A0822A4B128EE4F12ADE3`. It fenced 4,265 frames,
submitted 4,263 new plus 3,924 repeated frames with zero ring drops/submit failures, and sampled
CPU copy at `9.835 ms` average / `12.678 ms` p95. It recorded 832 successful arm applications (416
frames, both sides) with element-local axes and solved-target agreement. At frame 416 the right-arm
inverse calls returned success, exact-frame fallback also returned success, but both were rejected
by the same natural-geometry comparison; all later writes correctly failed closed. Evidence package
SHA-256: `42998CBA9277C1B6729D3F062C364533699DFAD3579366CD6CB7B4E9A6A5DC9E`.

A second full/Body-IK run, `20260918T210459Z-b59448961f3c`, used build-manifest ID
`C121D9DEF1548FC0C3C97F000CB8DEAF2EE46DEE14473D013515D57D17D74FFA` and the same corrected proxy.
It fenced 4,819 frames, submitted 4,817 new plus 2,279 repeated frames with zero drops/failures, and
sampled CPU copy at `9.712 ms` average / `13.046 ms` p95. It produced 1,228 successful arm
applications (614 frames, both sides) before the equivalent left-arm restore rejection at frame
614, followed by fail-closed writes. The repeated late failure on the opposite side rejects a
controller-side or overlay-focus explanation. Evidence package SHA-256:
`06627C0B04CB3DCE92932B7DB068BADE7A81E6903CFF2CCDF27F13FBAA6C155E`.

The restore comparator used a `0.001` game-unit positional threshold. Around the observed
~39,700-unit world coordinates, one adjacent 32-bit float step is about `0.0039` game units, so a
correct inverse or exact-frame rewrite could not reliably satisfy that threshold. Current source
keeps the strict mutation check but uses a separate restore contract: joint and element positions
must be within `0.02` game units (0.2 mm at the proven centimetre scale), while each unit axis must
be within `0.001`. Telemetry reports all three maximum errors, and the verifier parses and enforces
those limits. A host test accepts one-ULP live-scale drift and rejects a `0.01` residual axis error.

Run `20260918T210713Z-99d292bd2c94` was prepared without `-BodyIkAtStart`. Its `performance`
manifest correctly set `requireBodyIk=false`; it recorded no arm applications or write failures and
48 natural observation samples. It fenced 1,469 frames, submitted 1,467 new plus 952 repeated frames
and sampled CPU copy at `10.695 ms` average / `15.073 ms` p95. This is not negative body evidence:
the writer was disabled by design. Evidence package SHA-256:
`FA063F2E25F98941F75CA1A67A27712DA86539C2983DC11FD913B322688FDAC0`.

All three manifests forced the reversible `1920x1080`/FSAA0 profile. That explains the user's
reported poor image resolution; it is the deliberate performance tradeoff that reduced CPU copy
from the earlier roughly `15-25 ms` at `2560x1440` to roughly `9.7-10.7 ms`, not a new compositor
resolution regression. All three runs still reported presenter `shutdown_complete=false`.

The user also identified severe nausea specifically when tilting the head. Inspection found a real
orientation contract mismatch: `ApplyCameraPoseOrientation` deliberately omitted roll, while the
frame mailbox and OpenVR presenter carried the complete raw HMD pose through
`Submit_TextureWithPose`. Current source derives physical roll from tracked forward/up, applies it
around the exact native camera forward axis with the sign inversion required by tracking `-Z` to
CoJ `+Z`, and continues validating a rigid right-handed basis. `camera_hmd_orientation_applied` now
records `roll_degrees` and `roll_mode=native_camera_basis`; the live verifier requires at least a
3-degree tilt sample. Debug and Release builds complete, and each sequential full suite passes all
25 outcomes with **24 PASS plus the expected capability SKIP**. This roll/comfort correction and
the float-aware arm restoration remain **host-tested**, pending one fresh physical run.

An immediate `vr_test prepare -BodyIkAtStart` attempt while SteamVR was still active was rejected
before staging: eight Release tests that require native classic-D3D9 device creation returned
`D3DERR_NOTAVAILABLE`. The script left `Staging: none` and no video-profile state. This attempt is
not candidate provenance; rerun preparation only after the user closes SteamVR so the exact staged
artifact receives a clean Release suite.

After SteamVR closed, that exact preparation path completed successfully. The complete Release
suite produced **24 PASS plus the expected classic-D3D9 shared-texture capability SKIP**, then
staged full/body run `20260918T213453Z-a789ac61ac91` with build-manifest ID
`C2FB43FC65A9CD0F0A848A64584322D1D5C70A49F3E3B4E10BE958F1A7B11C90` and proxy SHA-256
`ACF278F40C2C959BD40A8BE016E4F8B73C4DDA8BCC3DAC51845F61191DEDA5FC`. Tracking and Body IK are
enabled before process start, D3D9Ex is absent and the reversible `1920x1080`/FSAA0 profile is
active. The remaining evidence is physical: both arms must visibly follow modest Sense motion
without deformation or premature fail-close, and a slow small head tilt must move naturally without
the previous nauseating roll mismatch. Stop immediately on discomfort. Do not use the dashboard or
Alt+Tab merely to operate the body gate.

That candidate is now finalized evidence. The user reported that the previous head-tilt nausea no
longer occurred; telemetry recorded native camera roll from `-27.286` to `+40.762` degrees. This
promotes the roll/image-pose alignment to **headset-validated** for the tested tilt behavior. The
run fenced 6,930 frames, submitted 6,928 new plus 4,038 repeated frames with zero capture drops or
submit failures, and sampled CPU copy at `9.381 ms` average / `11.404 ms` p95. Presenter shutdown
still reported `shutdown_complete=false` despite a complete collected run.

The float-aware restore correction also passed live: all 13,860 left/right arm applications had a
matching successful restore, with no writer or restore failures. Maximum measured joint and element
position error was exactly one live-scale float step, `0.00390625` game units, and maximum axis
error was `4.05355e-7`, all within the guarded limits. Both eye probes retained changed geometry and
the elbow/wrist target errors remained near zero. This promotes writer persistence/restoration, but
not visual arm composition.

Physical body confirmation failed in a narrower way. Both arms moved and vertical hand motion
worked, but forward/back was reversed: a T-pose became visible in front only when the user moved
the controllers behind the head. The attached user screenshot also shows a visibly twisted/
malformed forearm and wrist. Telemetry proves that this is not target reach, mutation or restore
failure. The adapter still mapped tracking-space `-Z` forward to positive native camera-forward,
which the visual evidence now rejects. Current source maps tracked Z with the opposite sign while
preserving X/Y. Telemetry/verifier bind the new contract as
`tracking_forward=-z_to_negative_native_forward`. The hand remains `hand_orientation=natural`;
controller orientation/twist is a separate unresolved arm-composition requirement. Evidence package
SHA-256: `A8CE0C3BDDAD405FDC8EADA5CDF3C27E8D5B76AE8E2FDDFE1329ECB145E1B477`.

Fresh Debug and Release suites from the corrected-Z source each pass all 25 outcomes with **24 PASS
plus the expected capability SKIP**. Full/body run `20260918T215118Z-c46320012ff0` used build-manifest
ID `C3679E1866C80B388E557C8B5B76B6E144945469E8154DB16C5BECE3DB5476B4` and proxy SHA-256
`337EB29BE4000E4C85B0B2EAD30FC9843B84ED35E2DF1691BFAB3645EF824C34`. It is finalized and unstaged;
the collected evidence is complete (`runtimeStarted=true`, `runtimeEnded=true`, `incomplete=false`).
The user physically confirmed that moving both Sense controllers forward now moves both arms
forward, validating `tracking_forward=-z_to_negative_native_forward`. Telemetry recorded 9,296
successful `body_arm_tracking result=applied` records, 9,296 successful arm restores, zero restore
failures and one clean `run_end`. The transport fenced 4,648 frames, collected 4,647, submitted
4,646 new plus 2,934 repeated frames, and recorded zero capture-ring drops or submit failures.
Sampled CPU copy was `9.092 ms` average / `11.433 ms` p95 at the reversible `1920x1080`/FSAA0
profile. The package SHA-256 is
`CA3F758168AAE782E727EFCF0A24B6F46E821297538BB1A78883AA2CBA93DA74`.

This promotes the controller front/back positional mapping only. Visual body acceptance still
fails: the user reports that both arms are severely deformed/twisted despite moving in the correct
forward direction. The telemetry explicitly remains `hand_orientation=natural`; controller pose
orientation, forearm roll and wrist/hand twist are not yet composed into the native skeleton.
Do not revisit the now validated tracking-Z sign without contradictory evidence, and do not promote
full Body IK to headset-validated until the deformation is corrected physically.

### Controller orientation and Sense gameplay host increment — 2026-09-19

The next arm-composition slice is now **host-tested**. It preserves the physically validated
`tracking_forward=-z_to_negative_native_forward` position mapping and the live-proven
`RotateElementWithChildren` visible-mesh writer. Each side now captures a controller-orientation
reference together with the current animated hand basis, then maps subsequent Sense orientation as
a calibration-relative delta. The resulting hand target is decomposed into forearm twist around the
solved lower-arm axis plus a residual hand rotation, avoiding any hard-coded assumption about a
PS VR2 controller-local palm axis. The transaction now covers upper arm -> forearm -> forearm twist
-> hand and restores hand -> twist -> forearm -> upper arm after stereo capture. Natural-geometry
verification includes the hand element position/up/forward frame. Invalid controller orientation or
failed mutation/orientation/restore checks fail closed.

The live verifier now requires `hand_orientation=calibrated_controller_delta`, valid controller
orientation, calibration/recenter identity, `hand_orientation_reached=true`, element-local native
axes for both forearm twist and hand rotation, persistence through both eye renders, and successful
hand/twist/forearm/upper restoration. This is not a physical promotion: the last headset evidence is
still run `20260918T215118Z-c46320012ff0`, where the arms remained visibly deformed. A fresh
`-BodyIkAtStart` run must visually prove controller orientation, palm-up/palm-down motion and natural
forearm/wrist twist before Body IK can advance.

The same host increment adds a PS VR2 Sense gameplay profile without synthesizing Windows keyboard
or mouse input. The shared runtime exposes neutral move/turn/fire/jump/reload/run/crouch/interact/
weapon-cycle/kick state through `/actions/gameplay`; the exact CoJ bridge resolves the shipped
`GameInputController` action objects and calls their `InputAction.Translate` path using the game's
configured device/code/sign metadata. Current Sense layout is: left stick move + click run, right
stick turn + click crouch, L2/R2 left/right fire, L1/R1 previous/next weapon, Square reload,
Triangle interact/execute trigger, Cross jump, Circle kick; left Create remains global recenter.
Gameplay state is neutralized whenever dashboard/focus/tracking does not permit normal scene input,
preventing a held movement/fire state from sticking across focus loss.

The full/body run manifest now sets `requireGameplayInput=true`; its verifier requires an applied
`GameInputController.InputAction.Translate` sample plus at least one non-neutral Sense gameplay
action. Debug and Release each complete all 25 CTest outcomes with **24 PASS plus the expected
classic-D3D9 shared-texture capability SKIP**, zero failures. `body_adapter` covers calibrated
90-degree controller roll, invalid orientation, exact CoJ gameplay action mapping, stick deadzone
and neutral release; provenance coverage binds the critical Sense action paths and rejects the old
`hand_orientation=natural` body evidence for a new full/body candidate. No Call of Juarez, SteamVR
or headset process was launched for this increment.

### Physical Sense gameplay run and FORETWIST correction — 2026-09-19

Full/body run `20260918T233902Z-0cb2e565e886` physically exercised the controller-orientation and
Sense gameplay candidate built from commit `bdeb53a`. Build-manifest ID was
`6196BD90555521CFA35B82FFFC3AE41ABE5593D6CBB8D3CAF234F1D8075F0DD4`; staged proxy SHA-256 was
`F84719D72A5D28A888B7DEB8603BCF81F0EF1BC2D214B5CFB8637738946E14C4`. The finalized evidence is
complete (`runtimeStarted=true`, `runtimeEnded=true`, `incomplete=false`). It fenced 7,688 frames,
collected 7,687, submitted 7,686 new plus 4,875 repeated frames and recorded zero capture-ring drops
or submit failures. Sampled CPU copy was `10.343 ms` average / `14.449 ms` p95. Presenter telemetry
still did not observe its inner `shutdown_complete` marker even though the outer runtime reached its
normal end.

The user physically confirmed that using the Sense gameplay controls is a substantial functional
advance, and the run recorded gameplay input through the native
`GameInputController.InputAction.Translate` route. This promotes the gameplay-control route itself
to live physical evidence for the tested session; it does not imply that every individual binding
has been separately validated. Body IK still fails visual acceptance. Telemetry repeatedly reached
the hand position and orientation targets, but the user's exported 86-second capture
`C:\Users\onita\Videos\clip_1.789.775.964.664.mp4` shows severe wrist/forearm deformation while the
hands/controllers move. Representative frames also show the player's head/hair intruding deeply
into the HMD view. Those observations are consistent with an arm hierarchy/composition defect, not
with the already-validated tracked-hand position or front/back sign.

Static inspection of the shipped `EBones.class` then identified dedicated twist elements that the
previous controller-orientation candidate skipped: left upper/forearm/**FORETWIST**/hand are
`7/8/9/10`, and right upper/forearm/**FORETWIST**/hand are `12/13/14/15`. The previous implementation
applied pronation/supination to forearm elements `8/13`; current host source instead resolves twist
elements `9/14`, applies controller roll there with `RotateElementWithChildren`, leaves only the
residual orientation for the hand, includes FORETWIST in mutation/persistence/restore geometry and
restores the hierarchy child-first as hand -> FORETWIST -> forearm -> upper arm. Telemetry and the
live verifier bind this contract with `twist_owner=foretwist_element`. This correction is
**host-tested only** until a fresh physical run shows anatomically acceptable forearm/wrist motion.
Fresh Debug and Release builds both completed successfully; each full CTest suite reports **24 PASS
plus the expected classic-D3D9 shared-texture capability SKIP out of 25**, zero failures.

Fresh physical run `20260919T002540Z-fc19b8ae78a4` exercised that FORETWIST candidate but did not
produce visible body motion. The log explains the apparently inert body: on frame 1 the left arm
writer mutated the mesh and reached the solved positional targets (`elbow_target_error=0.000732422`,
`wrist_target_error=0.00201324`), but the composed hand basis missed the calibrated orientation
target (`hand_up_error=0.274044`, `hand_forward_error=0.272201`). The guarded path immediately
restored the complete natural arm successfully and set the existing writer fault latch, so all
subsequent arm writes failed closed with `prior element writer/restore validation failed`. This is
positive evidence for safe fail-close behavior and FORETWIST reachability, but not Body IK
promotion.

The failure exposed a specific composition assumption: the residual hand rotation had been
precomputed from ideal mathematical FORETWIST propagation. The native hierarchy's observed hand
basis after `RotateElementWithChildren` differs from that estimate. Current host source now re-reads
the hand element after the FORETWIST write and recomputes the final residual directly from that
observed post-FORETWIST basis to the calibrated target. Telemetry/verifier bind the new path as
`hand_residual_source=post_foretwist_observed_basis`. This correction is **host-tested only** until a
fresh physical run proves both orientation reach and acceptable anatomy. Fresh Debug and Release
builds pass the complete 25-outcome suite with **24 PASS plus the expected classic-D3D9
shared-texture capability SKIP**, zero failures.

Full/body run `20260919T003727Z-73f20e13cc1a` physically exercised that post-FORETWIST residual
candidate from clean commit `2213ae6a2903581f8c82c43bb01a1cc6705b0b00`. Build-manifest ID was
`172178E5ECDC340E563C4F0FD1412DC3E5AD477D142605472F87445B11FF0CA4`; staged proxy SHA-256 was
`E0093D6ACDDE08411B16045AC87709F22EAA0188B7A8D14E4C189A9A40A8926B`. The run fenced 7,615 frames,
collected 7,614, submitted 7,613 new plus 3,977 repeated frames and recorded zero ring drops or
submit failures. The visible writer remained stable for the whole run: 15,230 left/right
`body_arm_tracking result=applied` records were paired with 15,230 successful post-stereo restores.
The user nevertheless observed severe arm deformation. Representative telemetry reached essentially
zero hand-orientation error only by demanding extreme FORETWIST/hand rotations, including roughly
132 degrees of FORETWIST plus 155 degrees of residual hand rotation on one sampled left-arm pose.
This physically rejects the calibrated raw controller-frame target as an anatomical hand target;
it does not reject the live-proven writer, restoration, tracked-Z mapping or FORETWIST reachability.

The same run recorded ten successful recenter sequences and repeated SteamVR dashboard transitions
while stereo presentation continued. The user still observed the dashboard visibly stuck over the
game, so scene submission and the overlay problem are now treated separately. Inspection of Sony's
installed PS VR2 SteamVR controller profile exposes `/pose/raw`, `/pose/base`, `/pose/handgrip`,
`/pose/tip` and `/pose/openxr_aim`; SteamVR's compositor binding uses `/pose/tip` for its laser
pointer. Current host source therefore adds explicit left/right `handgrip` pose actions for body IK,
keeps `/pose/tip` for the later weapon-aim boundary and forbids raw-role fallback for anatomical hand
targets. Explicit recenter now invalidates hand-orientation calibration and can clear a latched arm
writer only when no write transaction is active and fresh natural arm geometry verifies healthy.
These handgrip/recenter-recovery changes are **host-tested only**. Fresh Debug and Release builds
each complete all 25 outcomes with **24 PASS plus the expected classic-D3D9 shared-texture
capability SKIP**, zero failures.

Full/body run `20260919T011421Z-bef5076cd07e` physically exercised that handgrip candidate from
clean source commit `1027a67392f1c5baa71bbba8b6c39c9b63444983`. Build-manifest ID was
`0AB9BCF9BE628504EA5961092681A9550978092D71C8CFF619FD035B304D906C`; proxy SHA-256 was
`CC03E4DD55929CC4154ABB8A88B836A7FE6C60E5880E67F0EAE9B82D481ECBB6`. Evidence finalization is
complete and staging is now clear. The run recorded 14,968 successful left/right arm applications,
14,968 successful restores, zero restore failures and seven successful arm recoveries after recenter.
Body input came from explicit `handgrip` actions with raw-role fallback disabled, so the handgrip
source/recenter contract is physically exercised. Visual anatomy nevertheless remained unacceptable.

Targeted pose-window analysis separates the remaining rotation defect from positional IK. In the
user's palms-up gesture, average FORETWIST was approximately 88.8 degrees left / 95.7 degrees right,
while the additional residual hand rotation was approximately 98.2 / 121.8 degrees. Elbow/wrist
target errors remained effectively zero in the same sampled windows. The next candidate therefore
keeps calculating the observed post-FORETWIST residual but records it as diagnostic only and does
not rotate the hand element. The visible writer is upper arm -> forearm -> FORETWIST, with the native
hand relationship preserved. The verifier now requires
`hand_orientation=calibrated_controller_delta_foretwist_only`,
`hand_residual_source=post_foretwist_observed_basis_diagnostic`,
`hand_rotation_mode=foretwist_only` and `hand_rotation_no_op=true`; positional target reach and exact
natural restoration remain mandatory. Fresh Debug and Release builds each complete all 25 outcomes
with **24 PASS plus the expected classic-D3D9 shared-texture capability SKIP**, zero failures. This
twist-only candidate is **host-tested only** pending one fresh physical visual gate.

The first preparation after this host work, `20260919T085249Z-0caf8c569979`, was intentionally
unstaged before launch because its manifest captured the still-uncommitted tree as `dirty=true`; no
game, SteamVR or headset process was launched and it carries no physical evidence. The host changes
were then committed as `18057534b960522613e30b8aa2dc20e02d35eb1e`.

Fresh clean candidate `20260919T085408Z-327dd354bc4f` is now staged from that exact commit with
`dirty=false`. Build-manifest ID is
`79EE4829D5076EB59A96F71609ACCFB37642E42788BDA30A9A28B6F4A5801213`; proxy SHA-256 is
`3BD0B476810B15FBD935FA96F539E7303439B372F6BC45DA57BB40595CCE693F`. Preparation rebuilt Release
and reran all 25 outcomes with **24 PASS plus the expected classic-D3D9 shared-texture capability
SKIP**, zero failures, then enabled Body IK from process start and applied the reversible
`1920x1080`/FSAA0 profile. This remains preparation/host evidence until the user performs the single
physical FORETWIST-only visual gate.

Three downstream VR ownership items are now explicitly tracked but remain **planned** and separate
from the arm-composition gate: suppress the local player's head/hair from the HMD view without
removing the body, derive physical crouch from calibrated HMD height and feed it through the native
game crouch action/state, and decouple firearm aim from camera/crosshair ownership so weapon/muzzle
aim follows tracked controller/weapon orientation. None of those items is claimed implemented by
this run or by the FORETWIST change.

### Phase 7 transactional deployment host acceptance — 2026-09-19

The deployment path now writes its recovery journal before managed game-file mutation and stages
replacement content through deterministic temporary paths. Proxy/OpenVR/camera-control files are
SHA-256 verified before their final move; the OpenVR input directory is fully assembled and checked
against its staged manifest before publication. The same journal records those temporary paths so
an interrupted later invocation can remove only verified staged/partial content or restore a proven
original. A changed temporary, destination or backup fails closed and leaves the journal available
for diagnosis.

`provenance_tools` exercises clean/no-original recovery, pre-existing original restoration,
verified temporary-file publication, interrupted temporary-file cleanup, known partial temporary
OpenVR-input cleanup, managed-directory restoration, missing-backup rejection, external-change
rejection and idempotent no-journal recovery. It now also launches a temporary test process named
`CoJ.exe` and proves both stage and unstage reject it before mutating the fixture.

`tools/test_deployment_transactions.ps1` closes the script-level interruption matrix without
touching the installed game: it creates isolated fixtures from the exact installed `CoJ.exe` and
`ChromeEngine3.dll`, current proxy/build manifest and synthetic originals, then drives every
scripted mutation checkpoint. The matrix passes **15 staging failure recoveries**, **10 unstaging
failure recoveries** and **two complete repeated stage/unstage cycles**, with every fixture original
restored at the end. No game, SteamVR or headset process is launched. Phase 7 therefore reaches its
host acceptance gate.

A subsequent real `vr_test prepare -BodyIkAtStart` exposed one script-level integration defect not
covered by the helper tests: `Select-Object -ExpandProperty stagedManifest` cannot expand the
`OrderedDictionary` used for the in-memory OpenVR-input journal asset. The prepare transaction
failed before a candidate remained staged and recovery returned the game directory to `Staging:
none`. Replacing that expansion with explicit single-asset selection plus normal `.stagedManifest`
access preserved the same manifest checks. Focused `provenance_tools` passed, and a fresh full
prepare then completed successfully as run `20260918T114514Z-8f16a3b98492` with build-manifest ID
`3EBA34EA0B3FCCEC92F1520E684CDE1D8B83BA90BCBB274585DEA053B63928AA` and proxy SHA-256
`3790A5997B1FBA62B499FDDFD642DAF3EE9FFC6CE05DF892C4F17DB3600A4BF8`. This remains useful historical
real-install recovery evidence; the isolated end-to-end matrix above now supplies the missing
repeat/failure/process acceptance coverage.

### Phase 8 neutral math/semantic host acceptance — 2026-09-19

The shared VR types now encode one meaning per field: `EyeView::eye_to_head` is static optical
eye-to-head data, `LocatedEyeView::tracking_from_eye` is a time-located tracking/reference-space
pose, and `EyeRenderRecommendation` contains only render-target extent. The neutral tracking
contract is explicitly right-handed `+X` right, `+Y` up, `-Z` forward, metres, quaternion
`(x,y,z,w)` with destination-from-source transform naming and composition.

`vr_math` host tests cover asymmetric `EyeFov` through a reference projection matrix and reject
non-finite/invalid FOV, near/far and rigid-transform inputs. OpenVR eye configuration fails closed on
invalid optics; OpenXR located views validate pose/FOV before returning them, and OpenXR render-size
recommendations no longer masquerade as `EyeView`. This reaches the Phase 8 neutral math/semantic
host gate. It does **not** promote the experimental OpenXR backend's runtime/session lifetime or
ownership, which remains tracked separately under A10.
