# Codex handoff — Call of Juarez VR

## Current checkpoint

Audit-remediation Phases 0-4 remain **host-tested**, and their two run-bound manual Call
of Juarez observations remain authoritative. The user explicitly deferred repeating the
prepared Steam-Overlay-disabled A/B. The separate exact-build camera-path run established
`Camera -> View/Projection -> ChromeEngine3 renderer`, reproducible external FOV control,
renderer-camera correlation and clean restoration. The first physical-HMD rotation attempt
then showed that the original orientation injection point was wrong: OpenVR tracking and
recenter telemetry advanced, but head motion produced no visible camera movement.

Read in order before continuing:

1. `AGENTS.md`
2. `docs/TECHNICAL_AUDIT.md`
3. `docs/AUDIT_REMEDIATION_PLAN.md`
4. this handoff
5. `docs/VALIDATION.md`
6. `ARCHITECTURE.md`
7. `ROADMAP.md`

## Evidence that remains authoritative

- Call of Juarez (2006) DX9 is the reference implementation.
- Classic D3D9 -> CPU readback -> D3D11 upload has succeeded in the exact game build.
- OpenVR has initialized against manually started SteamVR, returned a valid PSVR2 HMD pose and accepted D3D11 submissions.
- The historical in-game flat bridge completed genuine captured-game-frame submissions.
- Historical diagnostics observed exactly three project `Present`, `BeginScene` and `EndScene` callbacks, then detected loss of installed device-vtable hook integrity while monitor rendering continued.
- Run `20260914T214318Z-fe71b222b664` reproduced that cutoff with the remediated Phase 0-4 ownership/telemetry candidate: 3/3/3 device callbacks, three successful capture/upload/balanced-eye submissions, then simultaneous loss of `Reset`, `Present`, `BeginScene` and `EndScene` ownership on the same device vtable.
- Run `20260914T215121Z-9bac4e22cffd` resolved the target ambiguity: all four lost device slots returned exactly to their recorded original addresses in `C:\WINDOWS\system32\d3d9.dll`. The factory and swapchain hooks remained owned. The factory `CreateDevice` entry was already intercepted by `C:\Program Files (x86)\Steam\gameoverlayrenderer.dll` before the project installed its factory hook.
- Historical replacement-owner candidate `369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF` remains baseline evidence only.
- HMD orientation injection is live-observed through the exact camera path; the remaining
  horizontal sign correction is host-tested. The first native-stereo render-view candidate is
  also host-tested. Positional 6DOF, full-body IK and motion-controller gameplay are not yet
  implemented.

## Phase 0 — host-tested

- Build manifests bind commit/tree/dirty state, diagnostic mode and artifact SHA-256.
- Staging requires a matching manifest, assigns a unique `run_id`, records exact game/engine/deployment identities and preserves older logs.
- Verifiers bind log, run manifest, staging state, copied build manifest and deployed hashes.
- Evidence collection recognizes structured `run_end`; absence of finalization remains explicitly incomplete.
- Provenance tests cover matching, altered-deployment and complete/incomplete paths.

## Phase 1 — host-tested

- Neutral runtime, build identity, diagnostics, OpenVR and OpenXR targets are separated.
- OpenVR/OpenXR are independently selectable and CI prepares only enabled pinned dependencies.
- Integration artifacts explicitly require Win32/x86.
- Native D3D9 tests assert the system runtime; proxy tests are isolated.
- HAL/capability unavailability is CTest SKIP, not PASS evidence.
- Readback validates full asymmetric frames, pitch and temporal changes.
- Fresh Debug and Release suites each produce 18 PASS and one explicit capability SKIP out of 19 tests.
- OpenVR-only and OpenXR-only Debug configurations produce the same outcomes with the disabled SDK root deliberately absent.

## Phase 2 — host-tested

- `VtablePatch`/`HookRegistry` provide conditional replacement, explicit outcomes, retained originals/ownership, per-vtable records, conflict-safe restore, synchronization and module pinning.
- Factory, device and swapchain hooks use the same registry model.
- Failure injection covers before/after replacement failures, conflicts, two vtables, reinstall, integrity loss, partial rollback, foreign-hook-safe restore and callback reentrancy.
- Native hook tests preserve original HRESULT behavior.

## Phase 3 — host-tested

- Classic `Direct3DCreate9` returns the native factory; native factory vtables observe all reachable `CreateDevice` calls.
- `GetDirect3D` canonical COM identity and device creation through a recovered factory are tested.
- Factory/device/swapchain IDs, creation thread and device generation are recorded; successful Reset advances generation.
- Hook events record original/replacement/current target modules.
- The D3D9Ex wrapper remains laboratory-only; classic staging rejects its environment switch and marker.

## Phase 4 — host-tested

- `COJVR_EVENT` JSONL binds every record to run/build, PID/TID and monotonic time.
- Events carry factory/device/swapchain/generation, sequences, duration, HRESULT/runtime result and exact process/per-device counters.
- Callback detail is sampled at bounded milestones; exact counters and active-stage state update for every observation.
- An observer thread emits per-device summaries and factory/device/swapchain hook integrity without re-hooking.
- Flat bridge stages are split diagnostically into capture, content publication, upload, pose wait, left submit and right submit.
- RGB content hashes ignore the D3D9 X/alpha byte; repeated captures do not advance content sequence even when submit attempts advance.
- Normal process exit emits `run_end`; smoke tests verify it. Missing finalization is rejected as incomplete.
- `tools/read_render_telemetry.ps1` and `tools/verify_d3d9_openvr_flat_live_test.ps1` report exact ownership/progression and reject malformed, mixed-run, failed-stage or incomplete evidence. The parser also exposes the factory `CreateDevice` original paths; a run can bind `validation.requireSteamOverlayAbsent=true`, which makes the verifier reject `gameoverlayrenderer.dll` automatically.

## Camera-path checkpoint

Exact static evidence is recorded in `docs/research/COJ_CAMERA_PATH.md`. The relevant
inspected `CBaseCamera` vtable is RVA `0x0030AE1C`; render-camera update slot `+0x24`
targets `0x001C5BA0`, FOV/frustum slot `+0x28` targets `0x001C58E0`, and the render
update calls the matrix path before storing that camera on the renderer at `+0x1AC/+0x1B0`.

Implemented/host-tested artifacts:

- `d3d9_camera_probe.dll`: system-D3D9 forwarding bootstrap only; no OpenVR or D3D9
  frame hooks;
- exact `CoJ.exe` and exact `ChromeEngine3.dll` rejection gates plus expected-vtable
  target validation;
- safe `HookRegistry` ownership of only the two camera slots;
- transient yaw/pitch basis modification around the engine render update, followed by
  immediate natural-basis restoration;
- FOV override only inside that same render-camera update;
- `tools/set_camera_probe_control.ps1` for atomic external control;
- `tools/verify_camera_probe_live_test.ps1` for run-bound exact-build/live evidence;
- camera control is transactionally created/backed up/restored by the existing D3D9
  staging/unstaging scripts.

Follow-on HMD candidate:

- `runtime::PoseSource` / `RelativePoseTracker` separate absolute XR pose acquisition from
  base-orientation/recenter policy and the ChromeEngine adapter;
- runtime convention is right-handed `+X` right, `+Y` up, `-Z` forward, metres,
  quaternion `(x,y,z,w)`; relative orientation is recomputed from the captured base rather
  than accumulated frame by frame;
- `d3d9_hmd_camera.dll` reuses `OpenVrRuntime::WaitForHmdPose` on a dedicated acquisition
  thread and exposes only neutral `PoseSample` data to the camera integration;
- the HMD proxy forwards system `Direct3DCreate9` only and installs no D3D9
  `Present`/`BeginScene`/`EndScene`/`Reset` hooks;
- missing/invalid pose data and tracking disable produce immediate natural-camera
  passthrough; the source basis is restored immediately after each render update;
- the first staged HMD candidate passed the previous Debug/Release host suites, but its
  derived-basis injection failed the manual visible-motion gate; the corrected source-basis
  candidate now passes fresh Debug/Release builds and all 19 CTest outcomes in each suite
  (18 PASS plus the expected classic-D3D9 capability SKIP);
- `tools/set_hmd_camera_control.ps1` provides enable/recenter/disable and
  `tools/verify_hmd_camera_live_test.ps1` binds the eventual run to exact binaries,
  advancing pose samples, an observed engine-derived render-basis change, renderer-camera
  identity, source restoration and shutdown;
- HMD staging now binds/backups/restores both `d3d9.dll` and `openvr_api.dll` plus the
  camera-control file.

The historical Steam-Overlay-disabled candidate `20260914T224904Z-overlay-ab` was never
run. Preserve its record as deferred A/B evidence; it is no longer the active manual gate.

The camera candidate ran as `20260915T150554Z-7e0d7da45949`, build manifest
`5D14F545D3164C21AB430B439AA9E093253A94A21470CBFD2A6F1A9410DB0FD5`, proxy
SHA-256 `0D221F41A28E18B07DED7582EA292AC7077ACF35391C5238945A524DE612E3DE`.
The run manifest required exact `ChromeEngine3.dll`; the initial external-control file was
disabled. The previous OpenVR-flat proxy/runtime were transactionally unstaged before
this candidate was installed. The user then enabled FOV `110`, yaw `20`, pitch `-10` and
confirmed a clearly changed gameplay view. Telemetry recorded renderer-camera correlation,
orientation/FOV application, immediate natural-basis restoration, a later disabled/natural
passthrough state, clean restoration of both hooks and bound `run_end`.

The dedicated verifier passed every condition. The collected run package SHA-256 is
`2C41F3668EC4F7C1167C2BC7D3B88455119EC06E3FBB3E89C382074B52644866`.

The first HMD run failed the visible-motion gate even though pose/recenter telemetry was
valid. Exact-build disassembly showed `0x0022BB10` overwriting the old derived-basis
`+0xC4/+0xD4/+0xE4` injection from source transform `+0x44`.

The second HMD attempt used run `20260915T211240Z-7d1c65d07a43`. Tracking/recenter and
large yaw/pitch samples were valid, and telemetry proved the `+0x44 -> +0xC4` path changed.
The user observed no visual camera/body/weapon rotation; instead HMD direction controlled
which distant world geometry remained visible, with the scene becoming nearly empty when
the headset pointed away. Tracking was disabled and generation 3 returned to passthrough.
Normal exit restored both hooks and stopped the pose source. Evidence package SHA-256:
`8E015C2B39326EE2E9C52804E9C6E2E03262088B1B3FFB05E5BC92747B45A5D2`. The hardened
verifier rejects this run because it predates the synchronized view-source fix.

Further exact-build disassembly established the missing native contract: camera setters
`0x0022C4D0`/`0x0022C510` update world/camera matrix `+0x44` and call matrix inverse RVA
`0x001F3F80` to rebuild inverse/view source `+0x04`. `0x0022BB10` copies those to
`+0xC4/+0x144` and `+0x84/+0x104` respectively, then combines `+0x104` with projection
`+0x184` into view-projection `+0x204`. `camera_probe.cpp` now mirrors that exact sequence:
apply HMD orientation to `+0x44`, recompute `+0x04`, run the original update, then restore
both source matrices. Telemetry and both live verifiers now require actual `+0x104` view
and `+0x204` view-projection changes.

After CoJ closed, fresh Debug and Release builds and both full CTest suites passed with
18 PASS plus the expected classic-D3D9 capability SKIP. The synchronized-transform revision
is therefore host-tested. Generate/stage a new manifest before the next manual test; run
`20260915T211240Z-7d1c65d07a43` and its artifact are obsolete for that retry.

The next required evidence is one combined `d3d9_native_stereo` run with SteamVR/HMD
started manually. It must confirm the corrected horizontal direction/scale, retained correct
pitch, stable scene/model placement, two native eye camera/projection passes, distinct left/
right rendered content and visible stereo depth. Tracking disable must return immediately to
the natural game camera and normal shutdown must restore camera/render-view/factory hooks and
OpenVR state. Do not generalize the exact `CBaseCamera`/render-view layout or RVAs beyond
Call of Juarez until another game proves the same contract. The deferred Steam Overlay A/B
remains relevant only to the separate D3D9 presentation-hook finding.

Obsolete second-attempt candidate (preserved only as failure evidence):

- run `20260915T211240Z-7d1c65d07a43`;
- build manifest `409C8A9A78C1276649F8CCE9C4CF817370E045FD2EFC8768051D1CED6C36889F`;
- proxy SHA-256 `6471140FB12FE8695C33B5F1553A9F66DB3B2153165FABB505780A8F9A0ED4A3`;
- OpenVR SHA-256 `AB696E4F218A95B3E396BC310F9FE6485DF48C99C0969762083212B1E1F025A6`;
- exact game/engine identities reverified and deployed hashes match staging state;
- initial control is `trackingEnabled=false`, `enabled=false`, `recenter=false`.

Synchronized world/view candidate exercised in the current manual run:

- run `20260915T212754Z-a12fcb10f11a`;
- build manifest `0B73CD2A748762A6A8FBE7A47E610534B0033C08B5B49FDE13BE23DC810F6180`;
- proxy SHA-256 `CAD1A6153F81026FD90D43570F6E370F9B8DE2AD9790C180B67C770965FAFCA0`;
- OpenVR SHA-256 `AB696E4F218A95B3E396BC310F9FE6485DF48C99C0969762083212B1E1F025A6`;
- exact CoJ/ChromeEngine identities and deployed hashes matched staging state;
- OpenVR pose/recenter advanced and the render path reported
  `render_basis_changed=true`, `view_matrix_changed=true`,
  `view_projection_changed=true`, `restored=true` and `renderer_camera_match=true`;
- the user confirmed visible first-person camera rotation, but physical movement was
  inverted and substantial environment geometry disappeared;
- tracking was disabled in-process; control generation 3 was accepted with
  `tracking_enabled=false` and `camera_probe_passthrough` restored natural control.

The gate therefore failed on orientation convention/environment visibility and remains below
`live-tested`. After normal user shutdown, the HMD verifier passed identity, pose/recenter,
render/view changes, passthrough, hook restoration, XR shutdown and `run_end`; evidence
package SHA-256 is
`FDEDBFD5427892A16208C678EA5145AE22D72BE83DBFD37CED2A4BE3915F888D`. That structural
pass does not override the failed manual direction/visibility observation.

Run `20260915T214329Z-3d9f66ae064e` has now been manually exercised. Pitch was correct,
yaw remained reversed, most environment geometry disappeared/turned white, and the user's
right-hand weapon appeared on the left side of the view. Tracking was disabled in-process
and generation 3 restored natural passthrough. Normal shutdown produced `run_end`; the
structural HMD verifier passed and the evidence package SHA-256 is
`AE5B87AE5F54F68D6EAFE6D6E68C41F737DB9134A0FF443E5C122C477BF2D965`. The manual visual
gate still failed. Do not treat this staged binary as a candidate for another run.

Exact-build disassembly of `FromForwardUpPos` RVA `0x001F3150` proved the native source
matrix at `+0x44` is right/up/forward/position and computes `right = up x forward`. The hook
had written `forward x up` (left), creating a horizontal reflection. The corrected source
therefore carries/writes the native right axis explicitly and keeps the paired world/view
update.

That correction was physically exercised as run `20260916T104036Z-24b3e3010d4c`, build
manifest `D9FC1DEFFDD019643DE882F675AB3051478D0AD7CABCF91CE9F523083BB17905`, proxy
SHA-256 `2287B6C83BFECD4D7A2DCE4C271F2CEC5824F9AC0416A85F3CB7A50990281DEB`. The user reported
good overall camera feel, correct character/model placement, no obvious recurrence of the
previous disappearing/white scene, and correct pitch. Yaw was still reversed. Runtime
telemetry kept determinant `+1`, preserved source world/view homogeneous layouts, changed
the render/view/view-projection paths, restored the source state, matched the renderer camera,
accepted tracking disable and emitted `run_end` on normal shutdown.

This run invalidates the prior static assumption that same-sign physical yaw is the visible
CoJ convention. Current source keeps the right-handed basis and changes only the exact-game
adapter to `-physical.yaw`; pitch remains unchanged and roll remains excluded. That correction
will be confirmed in the same physical run as the first native-stereo candidate.

After this yaw-only correction, fresh Debug and Release builds completed successfully and
both full CTest suites passed with 18 PASS plus the expected classic-D3D9 capability SKIP.
No game or SteamVR process was launched for that host validation. Generate a fresh build
manifest from this post-run tree before staging the next manual confirmation candidate.

Before another live run, the exact engine copy was re-hashed and the native matrix contract
was checked again. `FromForwardUpPos` sets right/up/forward/position rows and homogeneous
`w = 0/0/0/1`; `0x0022C510` normalizes the three axes, recomputes the inverse at `+0x04`,
then calls `0x0022BB10`. Current code now validates both source world `+0x44` and source
view/inverse `+0x04` homogeneous layouts before injection, validates the newly generated
inverse again before engine consumption, and fails closed to restored natural passthrough on
any invalid matrix. Natural/applied bases must remain rigid/right-handed with determinant
approximately `+1`. Camera host tests reject reflected, scaled, skewed and non-finite bases
and sweep the allowed yaw/pitch range while requiring `right = up x forward`. The HMD live
verifier now requires the complete world/view layout plus natural/applied determinants within
`0.02` of `+1`; `provenance_tools` executes synthetic success and failure evidence for these
guards. Fresh Debug and Release builds completed, and both full 19-test suites pass with
18 PASS plus the expected classic-D3D9 capability SKIP.

No new game or SteamVR run was launched for this refinement.

The first native-stereo implementation used the exact ChromeEngine view boundary at vtable
target RVA `0x00030FB0` and render-view core RVA `0x00030E00`: the left eye used the original
view method and the right eye invoked the core once more. Later live evidence proved the
core-only right path incomplete; current source replays the complete wrapper for both eyes.
Per-eye data is supplied through `CameraStereoRuntimeCallbacks`; OpenVR `EyeView::pose` is
explicitly treated as eye-to-head, keeping the game integration independent of a concrete XR
API.

Run `20260916T113600Z-native-stereo`, build manifest
`C2C866ACA33AA1C54E6D3D9D135A9F24809DF84CC78AB089DD5305D63823EB13`, was manually
exercised but is failed/incomplete evidence and must never be reused or promoted. Its log
contains four `run_start` records for the same run ID and no `run_end`. It produced 13,558
incomplete stereo-frame events, zero successful stereo frames, zero eye captures and zero
OpenVR stereo submissions. The representative failure was
`left_camera_applied=true;left_projection_applied=false;left_captured=false;right_rendered=false;submitted=false`.
The HMD camera path itself remained valid: pose application changed the engine view and
view-projection matrices and retained `renderer_camera_match=true`. The user's flat SteamVR
cinema view was therefore expected: no game eye textures reached the compositor.

The projection failure was traced to OpenVR's vertical `GetProjectionRaw()` convention. The
observed PSVR2 values use negative top and positive bottom tangents, while neutral `EyeFov`
requires positive up and negative down angles. `OpenVrProjectionRawToEyeFov()` now maps
`left, right, -top, -bottom`; a host test covers the exact observed asymmetric PSVR2 values.

The same manual run exposed a second issue consistent with the user's culling observation.
Static inspection of render-view core RVA `0x00030E00` shows camera update occurs early and
scene/visibility work continues afterward. Stereo mode had restored source world/view/frustum
inside `HookRenderCameraUpdate`, so later visibility work could see the natural game camera
even though derived matrices reflected the HMD. Stereo source world/view/frustum restoration
is now deferred through the complete eye pass. `HookRenderView` snapshots source/frustum plus
derived matrices and restores them transactionally after each eye. Stereo orientation
telemetry records `restore_deferred=true;stereo=true`; frame telemetry still requires
`left_state_restored=true` and `right_state_restored=true`.

The first transport remains classic-D3D9 CPU readback into two distinct D3D11 eye textures
followed by OpenVR stereo submission. The live run below proved that reading the swap-chain
backbuffer from inside the engine render-view boundary can duplicate one image into both eyes.
Current code captures `GetRenderTarget(0)` at each eye boundary, records whether it aliases the
backbuffer, keys resources to that capture-surface description and refuses OpenVR submission
when left/right RGB hashes are equal. The candidate observes `CreateDevice` through the factory
hook only and installs no `Present`, `BeginScene`, `EndScene` or `Reset` hooks.

`verify_native_stereo_live_test.ps1` requires both eye camera/projection applications,
captures and transactional restores, plausible eye separation, at least one distinct left/
right content-hash pair, per-eye renderer-camera identity sampled during the camera update,
HMD matrix invariants, passthrough on disable and clean camera/render-view/factory/runtime
shutdown. Synthetic provenance tests reject duplicated eye content, a missing per-eye renderer
match and reused run IDs. `get_run_provenance.ps1` requires exactly one matching `run_start`
for the staged run/build manifest.

Run `20260916T123049Z-8d977bb5b439`, build manifest
`E9816AC9943DB3B572A458F28B937EFFC9ED64D9DDCB1CDAD37924972F7D2F00`, was the next physical
candidate. It used one process start with matching exact-build/deployment provenance. OpenVR
started with PSVR2 recommended eye size `3400x3468` and eye offsets `-0.0325/+0.0325`; pose,
recenter and HMD camera matrices advanced. Both left/right camera/projection passes, captures,
state restores and OpenVR submission reported success. The user saw the actual game in the
headset once gameplay loaded; before loading a save the presentation remained flat/theater.

The stereo gate nevertheless failed. Every sampled pair (`1..8`, `90`, `180`, `270`, `360`,
`450`) had identical left/right RGB hashes and `distinct_eye_content=false`. The user reported
severe discomfort, images that did not combine correctly and poor frame pacing. This is live
evidence of headset-visible in-game submission, but not native stereo. The frame-level
`renderer_camera_match=false` in that artifact was sampled after the eye pass; orientation
telemetry at camera-update time was true. Current source now latches the renderer-camera match
independently for each eye at that correct moment.

The same run failed finalization evidence. Tracking disable returned to passthrough and camera/
render-view plus factory hooks restored, but the log ended after
`native_stereo_factory_hook: status=restored`: there was no
`native_stereo_runtime: status=stopped` or `run_end`. Evidence manifest therefore records
`runtimeStarted=true`, `runtimeEnded=false`, `incomplete=true`. Package SHA-256:
`C45162E2CB9584BBA0454319E62B1AA5438A6C973193FD64F86A377DA047CEEE`.

Explicit COM release of the retained D3D9 factory/device during CRT teardown is only a
suspected boundary, not a proven root cause. Current `Finalize()` restores hooks, shuts down
the readback/OpenVR runtime and emits final telemetry before dropping process-lifetime D3D9
pointers; Windows reclaims those references as the process exits.

Run `20260916T130852Z-1438628c90c6` exercised the active-RT candidate. The user reported
correct left/right and up/down tracking while the headset remained flat. Telemetry confirms
that the left eye captured RT0 as a `2560x1440` `D3DFMT_A8R8G8B8` backbuffer, while every
right-eye capture encountered `D3DFMT_NULL` (`1280070990`, FOURCC `NULL`). Both eye camera/
projection updates and renderer-camera correlation were true; no right capture completed and
no stereo frame was submitted. This run cannot be promoted because the same run ID contains
three `run_start` PIDs and no clean `run_end`.

Exact-build disassembly establishes the missing boundary detail. `0x30FB0` is the complete
render-view wrapper: it guards the view at `view+0xD7`, stores the active view at owner `+0x3B8`,
calls `0x30E00`, then runs additional post-core work. The failed implementation used the full
wrapper for the left eye and core-only `0x30E00` for the right eye. Current source replays the
full `0x30FB0` wrapper for the right eye by clearing only `view+0xD7` for that call and restoring
the natural post-left value afterwards. `D3DFMT_NULL` remains a fail-closed auxiliary target.
The verifier now requires `right_full_view_pass=true`, `right_view_guard_restored=true`,
`render_view_rva=0x30fb0` and `render_core_rva=0x30e00`.

Fresh Debug and Release builds/tests after this correction both passed **18 tests plus the one
expected classic-D3D9 shared-texture capability SKIP**. At that point the full-wrapper candidate
was host-tested and ready for the live run recorded next.

Run `20260916T133322Z-36c287cc43d8` subsequently live-tested the full-wrapper candidate with one
process start. Both eyes finished on real `2560x1440` `D3DFMT_A8R8G8B8` RT0, sampled hashes
were distinct, both renderer-camera correlations were true and OpenVR submission succeeded.
The user saw binocular gameplay with correct yaw/pitch, camera at the character head and no
obvious missing scene geometry. Fusion/comfort was poor and performance mediocre. Finalization
still failed to emit `native_stereo_runtime: status=stopped` or `run_end`; package SHA-256 is
`C878419E81C305EB616E32A6E0C1FC0BFE110C8853FA6119936AE9930A5CE91C`.

Static inspection of shipped `Data0.pak` found the first concrete visual-scale defect:
`Data/Player/PlayerProperties.def` explicitly defines `MoveSpeed` in `cm/s` and acceleration/
deceleration in `cm/s^2`; Billy/Ray use `MoveSpeed(550)`. The shared XR runtime is metres, but
the live candidate inserted eye-to-head metres directly into the CoJ camera. A 65 mm physical
IPD was therefore represented as 0.065 game units instead of 6.5 cm. Current source converts
eye translation using `kGameUnitsPerMeter = 100` only in the CoJ adapter. It also logs full
runtime eye positions, applied left/right eye positions and frusta, the D3D9 viewport, and
teardown markers before/after readback and OpenVR shutdown. The live verifier now checks the
100-units-per-metre contract and the resulting baseline distance, requires valid capture
viewports and valid asymmetric frusta, and retains the existing distinct-content/full-wrapper/
restoration checks. Synthetic tests reject the old 1:1 scale and missing viewport. Debug and
Release suites pass 18 tests plus the expected classic-D3D9 shared-texture capability SKIP.
The proof transport now also emits sampled `gpu_readback_ms`, `copy_upload_ms`,
`capture_total_ms` and `submit_ms` telemetry so the next physical run can quantify the cost of
the synchronous D3D9 GPU->CPU->D3D11 path before Phase 5 redesign. These changes are host-tested
only.

The latest host-only increment adds a minimal OpenVR controller-action seam specifically to
remove Alt-Tab from recenter. The native-stereo build ships a provenance-bound
`cojvr_openvr_input/actions.json` plus PS VR2 Sense binding; left Create maps to
`/actions/global/in/recenter`. `OpenVrRuntime` owns IVRInput/action handles and rising-edge
polling, while `camera_probe` sees only `recenter_requested` and reuses the existing
`RelativePoseTracker` recenter math. Terminal recenter remains available only as a diagnostic
fallback. Staging/unstaging hashes and transactionally manages both input assets, and the live
verifier requires the input-start, controller-press and camera-boundary events. Debug and
Release both pass 18 tests plus the expected shared-texture capability SKIP. No physical Sense
validation has occurred yet.

The repeated manual workflow now has a user-facing front door: `tools/vr_test.ps1`.
`prepare` rebuilds Release, runs the Release host suite, creates a dirty-aware exact manifest,
stages the current `d3d9_native_stereo` artifact plus its OpenVR action assets and enables tracking.
Left PS VR2 Sense Create is the normal in-headset recenter control; `recenter` remains a terminal
fallback. `disable`, `status` and `finish` let the user operate and close out a run without waiting for an agent;
the script never launches or terminates SteamVR/Call of Juarez. The first explicit
`-GameDirectory` is persisted only under ignored `work/vr-test.json`.

While physical testing was unavailable, a host-only hardening/cleanup pass was completed. The
neutral rigid-transform converter now fails closed for non-finite, scaled, skewed or reflected
rotation bases, and OpenVR HMD validity no longer overrides those numeric checks. Stereo readback
resource reuse now includes MSAA type/quality in its surface identity. Flat and native-stereo
capture share one packed BGRX content hash that ignores alpha/X and row padding, with tests for
those semantics; it cuts diagnostic hash-loop work without changing the distinct-eye acceptance
contract. No live performance claim is made from this host change.

A full Debug MSVC `/analyze` sweep found large 32/64 KiB stack buffers in file/path helpers and
temporary-string `string_view` telemetry arguments. The buffers were moved to heap-backed
storage and the telemetry strings now have explicit lifetimes. Rerunning `/analyze` on all
affected D3D9 proxy/readback/flat/native-stereo and VR-math targets is warning-free. Fresh full
Debug and Release builds each pass 18 tests plus the expected classic-D3D9 shared-texture SKIP.
No Call of Juarez, SteamVR or headset process was launched, so the validation state remains
host-tested for these corrections.

The subsequent physical run is `20260916T153109Z-8976b8f77775`, build manifest
`65BC08F2024C2FC975DD915E4A752F611F19CE597D8067E6B532FD3AD57648B4`. It physically validates
the minimal PS VR2 Sense input seam: left Create emitted the OpenVR global recenter action,
reached `camera_hmd_recenter_requested`, and the user confirmed recenter worked in-headset.
The corrected `100` game-units-per-metre eye baseline was also live-observed in submitted
distinct-eye frames (`-0.032/+0.032` m -> approximately `-3.2/+3.2` CoJ units at the neutral
camera) with valid asymmetric frusta and renderer-camera correlation.

Visual acceptance still failed. The user described image quality/frame pacing as very poor and
nauseating. Sampled synchronous `copy_upload_ms` is generally in the mid-30 ms range per eye and
can exceed 40-50 ms, confirming that the classic-D3D9 CPU/readback/upload proof path is not a
viable final transport. The menu is also outside the current VR presentation path: opening the
game menu removes it from the headset, while returning to gameplay resumes HMD-driven movement.
Record this for the later HUD/menu/video milestone; do not expand the present stereo gate into
full UI work yet.

Shutdown remains unresolved. The log restored camera/factory hooks, completed readback shutdown,
then stopped at `native_stereo_shutdown: stage=runtime_begin`; the collected evidence marks
`runtimeEnded=false` and `incomplete=true`. Staging is now clean (`vr_test.ps1 status` reports
`Staging: none`).

The next manual attempt initially exposed an unrelated baseline crash before the new transport
was staged: the game directory contained no staged `d3d9.dll`/OpenVR runtime, and the process
loaded system D3D9. Repeated exact-build launches crashed after the startup movies. Windows/HotSpot
evidence places the fault in `ogg.dll+0x274e` on the `MovieFinishedNvidia -> PlayIntroMovie`
path. Static bytecode inspection shows a `NoLogos` argument path, but physical testing on this
installation did not visibly bypass the startup videos. `NoLogos` is therefore not part of the
VR test procedure. Treat the intro-codec crash as a separate vanilla/baseline issue unless new
run-bound evidence ties it to the VR candidate.

The first deferred-presenter headset observations used staged run
`20260916T215746Z-362752fe6362` for two separate CoJ process starts, so strict provenance rejects it
as an acceptance run. It nevertheless establishes useful live behavior: both processes reached
clean presenter/runtime shutdown and `run_end`, distinct-eye frames reached OpenVR with zero submit
failures, and the longer process recorded 2,589 new plus 5,125 repeated submissions. The user
reported good camera response, sound and substantially better perceived performance after opening
and closing the CoJ Escape menu once, but SteamVR's dashboard remained permanently visible over the
game.

Local SteamVR evidence narrowed that dashboard issue. `vrserver` recognizes CoJ as
`VRApplication_Scene` with app key `steam.app.3020`; our PS VR2 Sense binding only maps left Create
to recenter and does not bind the system/dashboard button. `vrclient_CoJ.txt` shows `Capturing Scene
Focus` immediately after the presenter begins calling `WaitGetPoses`, while the first D3D11 scene
textures arrive roughly 20-45 seconds later when gameplay becomes renderable. Current source now
keeps pre-scene HMD tracking on non-blocking `GetDeviceToAbsoluteTrackingPose`, switches to
`WaitGetPoses` only once the first real stereo frame is ready, explicitly calls
`PostPresentHandoff` after successful stereo submission, and emits `openvr_scene_state` telemetry at
initialization, frame-ready and first-submit. Fresh formal run
`20260916T221254Z-861f3c15abd4` physically validated that correction: SteamVR scene focus moved
from PID `0` to the CoJ PID on first submit and the dashboard no longer remained stuck over
gameplay. The evidence package completed cleanly with `runtimeStarted=true`, `runtimeEnded=true`,
`incomplete=false`, `native_stereo_runtime: status=stopped` and `run_end`. The run recorded 5,139
collected frames, 5,136 new submissions, 5,439 repeated submissions and one submit failure. The
user also reported improved perceived performance.

The remaining physical defect in that run is strong head-turn ghosting/elastic reprojection, as
if the displayed view tries to return toward an older orientation. The asynchronous path is
`HMD pose -> ChromeEngine stereo render -> D3D9 ring -> CPU mailbox -> D3D11 upload -> OpenVR`.
Previously the presenter continued advancing `WaitGetPoses` while it could submit an older image
with `Submit_Default`, so SteamVR had no record of the pose that actually produced that image.
Current source now carries the exact HMD render pose and its sequence with every `StereoCpuFrame`
and submits both new and repeated textures through `VRTextureWithPose_t` /
`Submit_TextureWithPose`. Telemetry records `render_pose_sequence` and
`pose_mode=explicit_render_pose`, and presentation fails closed if an exact valid render pose is
missing. Run `20260916T224239Z-e43b46698e5c` physically validated the effect: the user tested slow
and fast head rotation and mouse rotation and reported that the backward pull/snap-back was gone
and the sensation improved substantially. The run ended cleanly with `run_end`, collected 3,129
frames, submitted 3,127 new plus 6,099 repeated frames and recorded one submit failure. Treat the
explicit-render-pose correction as live-tested for the snap-back defect; remaining work is now
primarily frame pacing/performance.

Telemetry from that run exposes two large CPU costs on sampled 2560x1440 stereo frames: roughly
`15-22 ms` for owned CPU copying and `11-14 ms` for full-frame diagnostic hashing, before the
roughly `1-4 ms` D3D11 upload. Current source removes the hash from the normal presentation path:
every frame still performs a fail-closed RGB equality comparison that ignores the undefined alpha
byte, while the expensive full hashes are computed only for the first/sample telemetry frames that
are logged. Contiguous D3D9 locks also use one bulk `memcpy` rather than per-row copies. The live
verifier now requires `distinct_check=rgb_compare_every_frame` and
`hash_mode=sampled_telemetry`. Fresh Debug and Release each pass 20/21 CTests with the one expected
classic-D3D9 shared-texture capability SKIP. This optimization is host-tested only.

The product target is explicitly native stereo rendering, full-body IK and VR-rebuilt
interactions. The current combined HMD/native-stereo gate controls progression into positional
tracking and later interaction/IK work.

The next candidate must use a fresh automatically generated run ID. `tools/vr_test.ps1
prepare` rebuilds/tests Release, creates a dirty-aware exact manifest, and
`stage_d3d9_proxy.ps1` generates a timestamp-plus-GUID run ID while rejecting an existing
evidence directory. Do not reuse `20260916T113600Z-native-stereo`,
`20260916T123049Z-8d977bb5b439`, `20260916T130852Z-1438628c90c6`,
`20260916T133322Z-36c287cc43d8`, `20260916T153109Z-8976b8f77775`,
`20260916T221254Z-861f3c15abd4` or `20260916T224239Z-e43b46698e5c`. The corrected
100-units-per-metre eye baseline, distinct real-color eye captures and left-Sense-Create recenter
now have live evidence, and clean presenter/runtime shutdown plus SteamVR scene-focus handoff are
also live-tested. Explicit render-pose submission removed the reported snap-back in the latest
physical run. The active gate is now comfortable sustained frame pacing with the cheaper
per-frame distinction path; synchronous D3D9 readback/CPU copying remains the largest known
transport cost after removing full-frame hashing from nearly every frame.

The next physical candidate must be prepared from the current performance changes. Its decisive
observation is sustained smoothness during slow/fast head turns and mouse rotation, while
confirming the snap-back remains absent and stereo geometry/recenter/dashboard behavior do not
regress. Normal launch is the procedure; `NoLogos` is not required. Recenter remains left PS VR2
Sense Create. Before closing the game use `tools/vr_test.ps1 disable`; after closing use
`tools/vr_test.ps1 finish` so the verifier can bind the evidence to that fresh run.

That performance candidate is now staged without launching SteamVR or the game. Run ID
`20260916T225750Z-b2515c32a135`, build manifest
`46837519D71BBAB58B5B97EDD26F9D87B8093CD40640BBE2B3E5D513687151A0`, staged proxy SHA-256
`AAE80D73AC7C77F2F77988FA63B4F345A66323CBF8FE88D46A036BAF6844C229`. Release preparation passed
20/21 CTests with only the expected classic-D3D9 shared-texture capability SKIP. Use this run for
exactly one CoJ process start and compare sustained smoothness directly with
`20260916T224239Z-e43b46698e5c`.

Repository closeout on 2026-09-17 independently rebuilt the full current tree in Debug and Release
and reran both CTest presets: each produced 20 PASS plus the one expected classic-D3D9
shared-texture capability SKIP. A clean full-tree Debug build with MSVC
`RunCodeAnalysis=true` completed without code-analysis warnings. `tools/vr_test.ps1 status`
confirmed the game was not running and the staged candidate still pointed at run
`20260916T225750Z-b2515c32a135`. No SteamVR or game process was launched during this closeout.
Phase 5 is now documented as implemented, host-tested and live-exercised, with explicit
reset/resize/new-device and controlled paused-producer acceptance coverage still pending; the
active physical gate remains sustained frame pacing/performance.

## Constraints

- Never launch Call of Juarez or SteamVR automatically.
- Do not use blind periodic re-hooking.
- Do not reactivate D3D9Ex as the main path.
- Keep work limited to the current HMD/native-stereo gate; do not add positional 6DOF, UI,
  full-body IK or controller gameplay yet.
- Do not equate submit count with new game content.
- Do not promote any Phase 0-4 result above `host-tested` until the exact manual run supplies live evidence.
