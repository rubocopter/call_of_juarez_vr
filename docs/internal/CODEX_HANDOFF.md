# Codex handoff — Call of Juarez VR

## Current checkpoint

Run `20260919T162808Z-fb75cb34977a` is now diagnostic multiprocess evidence, not an active candidate.
The user launched it twice and both starts left the SteamVR interface stuck over the game. PID 28408
and PID 24032 both initialized with `scene_focus_process_id=0` and `dashboard_visible=true`; scene
focus moved to CoJ only after a native-stereo frame reached `scene_ready` and the first scene submit
completed. This directly rejects the previous policy of postponing compositor pacing until gameplay
stereo exists. Reusing the run ID for two processes also prevents formal promotion. Current tooling
reports `Staging: none`; the game directory has no staged project `d3d9.dll`, `openvr_api.dll`, stage
state or active video-profile state. The residual `.cojvr-run.json` and `cojvr.log` are diagnostic.

Current source implements a Penumbra-style startup/fallback presentation policy using CoJ's proven
classic-D3D9 boundaries. The implicit swap-chain `Present` hook captures the backbuffer into a
separate deferred ring when native stereo has been absent for 250 ms. Every second flat `Present` is
captured to limit CPU-readback pressure, while the OpenVR presenter repeats the latest image at
compositor cadence. Flat content is centered inside a larger black eye texture, submitted from a
stable HMD anchor and re-anchored by left-Sense Create. When the exact CoJ stereo producer resumes,
the presenter automatically switches to `native_stereo`; if it later stops for menus/loading, it can
return to `flat_theater`. Flat content may have identical eyes; native stereo still fails closed on
identical RGB. A shared transport sequence orders the two producer domains without conflating their
capture counters, and shutdown explicitly restores the swap-chain hook. Fresh Debug and Release
suites each pass 24 tests plus the expected classic-D3D9 shared-texture capability SKIP. This new
presentation path is host-tested only and requires a fresh one-process physical gate.

Fresh full/body candidate `20260919T170916Z-d9a22d24eb0c` is staged from clean source
`e341a32e0e0682b10e22c25e8835081fbba6d106`, build-manifest ID
`285F9DD22A6A7BDC8588A2EFC37BBE2DA8B22A06D89AA8613210E3DFD94C1C98`, proxy SHA-256
`25F197B813499C771F77F0614006AB263C049F21BFA6808E5C98376DC01D12F6`. Release preparation passed
24 tests plus the expected classic-D3D9 shared-texture capability SKIP. Body IK is enabled from
process start and the reversible `1920x1080`/FSAA0 profile is active. For the next one-process run,
first verify that intro/menu rendering appears as anchored `flat_theater` content and that SteamVR's
dashboard does not remain stuck. Then load gameplay and verify the automatic `native_stereo`
transition; Create should re-anchor the flat presentation while preserving the existing gameplay
recenter path. If startup fails, preserve the run as diagnostic evidence rather than repeating the
same run ID.

Run `20260919T153546Z-705460dca03b` is finalized and staging is clear. It is a clean single-process
full/body run from source `32e979709bbb13780cf885c82770a0e8c1649631`, build-manifest ID
`9B5DDBEED941B6C1F5A1E45D39837F2B7BB491CE4C5F6AB8934F86DE255CD671`, proxy SHA-256
`8864D3DFEE42537D0A4E0CBCC230B4E27B48AC273287BFA9FAF508FA0323B97A`. PID 7756 reached normal outer
runtime stop and `run_end`; the evidence manifest has `runtimeEnded=true` and `incomplete=false`.
The inner presenter still reports `shutdown_complete=false`.

The user's 52.678-second `clip_1.789.832.536.546.mp4` physically confirms the new snap-turn behavior.
Telemetry contains 23 exact steps through `PlayerBeing.RotateHorizontally(F)`: 13 at -45 degrees and
10 at +45 degrees. The screen jumps in the recording correspond to those deliberate snaps. Promote
this exact right-stick snap behavior to **headset-validated**. Two Create recenter recoveries report
`calibration_preserved=true`, with four immediate arm samples using
`orientation_calibration_mode=preserved_target_rebase`; this proves the new rebase path is live-
exercised, while visual recenter orientation stability remains a separate observation if needed.

Body writes are stable: 8,452 applications, 8,452 restores, zero writer/restore failures. The video
is visibly much better than the earlier collapsed/twisted candidates, but anatomy is not accepted
yet. `target_clamped=true` still occurs 3,695/8,452 times (43.72%; left 45.41%, right 42.03%), so the
short-arm/reach problem remains. Several wrist/hand poses are still forced, and the local hair/head
intrudes prominently (for example around the 18 s and 30 s sampled frames). Weapon presentation and
shot direction remain on the unresolved native aim boundary. Video SHA-256:
`B132BD5709B79E8396CF4048B98A88EA70FF0FFFC88F4EE6C27255E5657ACB11`; package SHA-256:
`0AF7723484528C368549F0DB46FE0C682F078B3120CCA3E67F7EA3DD4A1C4BE8`; packaged runtime-log SHA-256:
`57E56F94F963AA121B63BD4DD5C23644655A46C3285069ACC02471DBCA647BE7`.

Candidate `20260919T122155Z-c534d86926a9` was launched three times under the same run ID, so its
physical evidence is retained as **diagnostic multiprocess evidence** and cannot promote a formal
single-process gate. Candidate identity was clean source
`075daf3cbeba8abbf6ac389978714d1d85092a9e`, build manifest
`163BBD05C73327FADEEA3B50D4418D11A6C2B0ACAC5B49108389832B667A3702`, proxy SHA-256
`74FDEBE4C09C8D5AD6AE6EFAF6F8FDF80C2D900E4B60CF4AEEB59BBF384E9E1C`.

The third process (PID 448) is the user's 87.79-second video session. It recorded 11,406 successful
arm applications and 11,406 successful restores, zero arm writer/restore failures, and six recenter
events. Reach clamping occurred 6,072/11,406 times (53.24%). Sampled native chain length averaged
26.6857 upper + 23.1577 lower = 49.8434 game units, matching the user's observation that the arms
feel short. Visual anatomy remains rejected; local head/hair still intrudes and aiming still follows
the native per-hand look direction/origin path rather than the visible weapon transform. The first
two process starts had the SteamVR interface stuck over the game; the recorded third start did not,
so focus/dashboard stability is not promoted. Video SHA-256:
`6BA5556AF95EFB3D598FB77BA900A8BE64065AF568EEF0FFB5B4A523289017F5`; captured runtime-log SHA-256:
`4745C85DDA63CD7B7EACE93B49F71EA58DB465ABDA6F78D9E651402BD23348F6`; diagnostic package SHA-256:
`09A1F3AC5B30E3238B35311CFD525FBD4443413D2793F0C0E3DEBEAAD189BF17`.

Current source implements the next four corrections while preserving the live-proven axes and
writer. Local first-person visibility now resolves only the local player's exact Ray/Billy
head/hair elements (`RayHead`, `RayHair`, `RayCap`, `BillyHead`, `BillyHair`, `BillyTress`) through
the shipped `GetElementID(String)` route, records their original hidden state, uses
`HideElement(int)` only for elements that were visible, and restores only VR-owned changes through
`UnhideElement(int)` on tracking loss/shutdown. The whole player mesh is never hidden. The effect on
the exact game's shadow pass still needs physical observation.

Arm solving keeps the measured native segment lengths and the validated tracking axes, but now
absorbs up to 12 game units of ordinary target overreach before the hard two-bone clamp; extreme
targets still clamp. Telemetry records raw/effective distance and the applied adjustment. The
controller-driven shared FORETWIST/hand axial roll is limited to 100 degrees while the rejected full
wrist residual remains diagnostic. This is intended to reduce the short-arm clamp frequency and the
most forced wrist poses without reintroducing arbitrary full-hand rotation.

OpenVR now exposes separate left/right `/pose/tip` actions for weapon aim while `/pose/handgrip`
continues to own body hands. Static shipped data proves `EnumInvHand._RIGHT=0` and `_LEFT=1`; the CoJ
adapter writes the tip-derived world direction into `m_avLookDirDevForHand[0/1]` after the native
game update and before rendering. `GetFireDirForWeapon` remains responsible for native accuracy and
spread, `GetFireOriginForWeapon` remains on native `GetBeingLookFromPoint`, and the network-forced
branch is untouched. The direction ownership/timing is implemented but requires a live firing test
before it can be promoted beyond the host/static gate.

Fresh Debug and Release builds each pass all 25 CTest outcomes with **24 PASS plus the expected
classic-D3D9 shared-texture capability SKIP**, zero failures. These new paths are not physical
evidence yet.

Latest continuation, 2026-09-19: run `20260919T085408Z-327dd354bc4f` is finalized and unstaged.
The user's 26.84-second clip still rejects anatomy. It had 5,050 applications/restores, no arm
fault, and one recenter recovery. `tools/analyze_arm_hierarchy.py` proves the old ordinal-based
hierarchy assumption wrong: FORETWIST follows upper only; hand follows forearm without FORETWIST.
All 116 measured axes agree with those propagation models within 0.000008, whereas the assumed
serial chain has large errors. Current source composes full upper+forearm swing into the FORETWIST
sibling and shared axial roll into FORETWIST and hand. It checks all four output axes within 0.02,
retains positional/both-eye/restore checks and reports `hand_rotation_mode=sibling_shared_roll`.
The full wrist residual remains diagnostic. The old `foretwist_only` no-op-hand requirement is
superseded by this measured sibling contract. See `docs/research/COJ_ARM_SKINNING_AND_AIM.md`.
Aiming research proves bullets use native per-hand look directions and look origin. Current source
writes controller `/pose/tip` direction at the post-update/pre-render boundary while
leaving native fire origin, spread and network behavior intact; physical shot-direction ownership
still needs to be demonstrated.
Head intrusion and incomplete inner presenter/factory shutdown remain open. Do not promote body
anatomy from successful telemetry alone. The next candidate must include the host-tested flat-theater
startup/fallback path described at the top of this handoff.

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
- HMD orientation injection and native stereo are live-observed through the exact camera/render
  path. Positional 6DOF plus body/IK preflight are implemented and host-tested: tracked hands and
  upper-body writes are wired, while pelvis/leg geometry and two-bone solving are currently
  read-only. Physical body validation and motion-controller gameplay remain pending.

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

This run invalidated the prior static assumption that same-sign physical yaw is the visible
CoJ convention. Source at that gate kept the right-handed basis and changed only the exact-game
adapter to `-physical.yaw`; pitch remained unchanged and roll was still excluded. That correction
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
are logged. Contiguous D3D9 locks now construct the owned byte vector directly from the locked
range instead of value-initializing the full destination and immediately overwriting it; padded
locks reserve once and append only real pixel rows. The live
verifier now requires `distinct_check=rgb_compare_every_frame` and
`hash_mode=sampled_telemetry`. This optimization is host-tested only.

Phase 5 host acceptance is now complete at the component/policy boundary. The D3D9 capture test
exercises same-device source resize, explicit `InvalidateResources()` before classic D3D9 Reset
plus post-Reset generation recovery, and a second device with identical dimensions/format. The
exact CoJ candidate still does not depend on a Reset hook. A production `PresentationCadence`
state machine preserves the last valid frame across a controlled producer pause, classifies
subsequent successful submits as repeated, preserves `new` across a failed submit, returns to
`new` when the producer resumes and invalidates stale presentable content explicitly. Fresh full
Debug and Release suites each pass 21/22 CTests with only the expected classic-D3D9 shared-texture
capability SKIP.

Phase 6 host/simulated acceptance is now complete. `OpenVrRuntime` carries explicit lifecycle,
connection, focus, tracking-valid, presenting and shutdown state, processes relevant OpenVR
events and uses a process-level owner gate so a competing runtime fails closed. Move-assignment
now shuts down an existing owned runtime before taking another implementation. The presenter logs
runtime-state transitions, refuses submission while tracking/HMD connection is invalid, and
per-eye submit outcomes feed the presenting state. Host simulation covers focus loss, one-eye
submit failure, invalid tracking, disconnect, shutdown and owner conflict/release. The retained
`noexcept` runtime surface now contains allocation-capable error/reporting work behind failure
guards, and moved-from objects fail closed rather than dereferencing an empty implementation.

The D3D11 presentation order is explicitly `UpdateSubresource -> gpu_sync=none ->
Submit_TextureWithPose -> PostPresentHandoff`. A diagnostic seam and host test compare `none`,
`Flush` and a bounded D3D11 event-query fence without introducing a production-wide wait. The
isolated OpenVR visible probe now supports `--frames N`, an animated/distinct eye pattern and
`--gpu-sync none|flush|event` for later manual A/B evidence. That probe has not been rerun against
SteamVR/HMD, so no new live/headset claim is made. Fresh full Debug and Release suites each pass
23/24 CTests with only the expected classic-D3D9 shared-texture capability SKIP.

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

The previously staged performance candidate `20260916T225750Z-b2515c32a135` was confirmed never
to have started: only its stage/run manifests existed and no matching process log/evidence was
present. It was transactionally unstaged before the current Phase 5 acceptance/copy work. Do not
reuse it. A fresh candidate must be prepared from the current tree and compared for sustained
smoothness directly with `20260916T224239Z-e43b46698e5c`; the authoritative candidate identity is
the game-directory stage/run manifest created by `tools/vr_test.ps1 prepare`.

Repository closeout on 2026-09-17 now reflects the complete Phase 5/6 tree: fresh Debug and Release
builds each run all 24 CTests with 23 PASS plus the one expected classic-D3D9 shared-texture
capability SKIP. An earlier clean Debug `RunCodeAnalysis=true` pass completed without warnings
before the final Phase 5/6 expansion; do not treat that older analysis pass as stronger evidence
than the current build/CTest results. No SteamVR or game process was launched for these host-only
passes. Phase 5 is implemented, host-tested and live-exercised; Phase 6 runtime
state/ownership/failure simulation and controlled D3D11 synchronization are host-tested. The
animated physical OpenVR probe remains unexecuted for this increment. The active native-stereo
product gate remains sustained frame pacing/performance and must use a fresh run ID if resumed.

The next physical run has been deliberately consolidated so it can close more than the raw
performance observation. Native-stereo run manifests now require the production
`UpdateSubresource -> gpu_sync=none -> Submit_TextureWithPose -> PostPresentHandoff` policy,
connected/tracking/presenting OpenVR state, at least one repeated-frame submission, a deliberate
post-presentation focus-loss/reacquisition cycle and final `shutdown_complete` state. During stable
gameplay the user should open the SteamVR dashboard once, leave it visible briefly, then return to
the game; the same run should also include slow/fast head turns, mouse rotation and left-Sense
Create recenter. `tools/vr_test.ps1 finish` verifies those contracts, generates
`cojvr-native-stereo-summary.json`, packages it as `analysis/native-stereo-summary.json` with the
run evidence, and then restores staging. The summary records count/average/p50/p95/max timing for
the available capture/readback/CPU-copy/producer/upload/pose/submit samples plus transport counters,
focus-cycle observation and shutdown completion. Debug and Release still produce 23 PASS plus the
single expected capability SKIP after these changes. No game, SteamVR or headset process was
launched while preparing this consolidated gate.

Phase 7 has advanced without changing the current physical product gate. Staging rejects a running
CoJ process, a non-Win32 build manifest or non-x86 game/proxy/OpenVR/ChromeEngine PE before
mutation. Stage and unstage now journal managed assets before mutation and recover an interrupted
journal on the next invocation. Staging prepares proxy/OpenVR/camera-control/input assets under
deterministic temporary names, verifies SHA-256 or the complete directory manifest, then moves the
verified content into place. Recovery removes only recognized staged/temporary content or restores a
proven original; missing backups and externally changed paths fail closed while preserving the
journal. `provenance_tools` covers clean and pre-existing-original recovery, verified temporary
installation, interrupted temporary file/directory cleanup, partial managed-directory recovery,
missing-backup rejection, external-change rejection and idempotent no-journal recovery. Fresh Debug
and Release full suites pass 24 tests plus the expected classic-D3D9 shared-texture capability SKIP.
The previously missing end-to-end matrix is now host-tested. `tools/test_deployment_transactions.ps1`
uses isolated exact-binary fixtures and recovers all 15 staging failure checkpoints, all 10
unstaging failure checkpoints and two repeated complete stage/unstage cycles. `provenance_tools`
also proves a temporary active `CoJ.exe` blocks stage/unstage before mutation. Phase 7 host
acceptance is therefore complete without launching the real game or SteamVR.

Phase 8 neutral math/semantic acceptance is also host-tested. Static optical data is
`EyeView::eye_to_head`; runtime-located eyes use `LocatedEyeView::tracking_from_eye`; render-size
recommendations use `EyeRenderRecommendation`. The neutral convention is explicitly right-handed
`+X` right, `+Y` up, `-Z` forward, metres, `(x,y,z,w)` quaternions and destination-from-source
composition. Reference-projection tests cover asymmetric FOV and invalid inputs, and both OpenVR
optics and OpenXR located-view publication fail closed on malformed/non-finite data. This does not
promote OpenXR runtime/session lifetime ownership, which remains separate under A10.

The user then explicitly advanced the positional 6DOF/full-body direction. Current host source now
reconciles horizontal recentered HMD translation into `NetPlayer.m_Being` at the proven `100`
CoJ-units/metre scale while leaving vertical head movement camera/body-owned. The actor update
removes the previously absorbed physical offset first, preserving native locomotion and preventing
the model from being left behind when the user walks physically. HMD plus both Sense role poses
come from the same OpenVR sample; controller poses pass through the same recenter transform and are
stored only as neutral body anchors.

Exact shipped skeleton IDs remain game-specific. `JavaPlayerBridge` now has working fail-closed JNI
readers for `GetBoneJointPos`, `GetBoneDirVector` and `GetBonePerpVector`, plus the exact native
element writer `FromUpForwardPosElementWorld`. It attaches to the existing Java 1.4 VM and never
creates a second VM. `ArmedPlayerBeing.UpdateLookPointDead` bytecode confirms the engine uses bone
joint + direction + perpendicular as position/up/forward geometry; the world-basis native handler
was independently found around ChromeEngine RVA `0x0009A350`. No speculative object/bone offsets
were added to the runtime.

`SolveTwoBoneIK` is now a game-neutral measured-length solver with reach clamping. `BuildArmIkPlan`
uses the current animated shoulder/elbow/wrist positions and current upper-arm/forearm bases,
derives the bend plane from the live elbow and shortest-arc rotates the existing bases to the solved
segments so animation twist is retained. The CoJ target mapping removes the room-scale world offset
already absorbed by the actor before mapping the recentered Sense position through the natural
camera basis. `bodyIkEnabled` gates writes; observation remains available while disabled. Arm writes
set only upper-arm and forearm world bases; the hand follows the native chain and keeps its current
orientation. Pelvis/leg writes remain deferred until this composition is physically validated.

The host-only lower-body preflight now reads pelvis, thigh, shin and foot joint/basis data through
the same JNI contract. `BuildPelvisLocomotionAnchor` preserves the native actor/animated-pelvis
relationship; `BuildLegIkPlan` measures hip-knee-ankle lengths, derives the knee pole from the live
animated plane, clamps unreachable targets and preserves the native foot basis. Sampled
`body_lower_tracking` events record both sides with `write_enabled=false`. The consolidated
verifier now requires those read-only observations alongside successful left/right arm writes when
`requireBodyIk=true`, so the next physical run can collect lower-body evidence without adding a
second headset run. Lower-body element writers remain prohibited until the arm composition is
physically proven.

The body run stays one process, but IK is enabled before launch with
`tools/vr_test.ps1 prepare -BodyIkAtStart`; no Alt+Tab/terminal toggle is required during gameplay.
The native-stereo run manifest sets `requireBodyIk=true`; `finish` requires at least one successful left and right
`body_arm_tracking result=applied` sample using the measured basis and exact writer, plus one
read-only `body_lower_tracking result=observed` sample for each leg with a valid measured plan, in
addition to the existing stereo/6DOF/recenter/shutdown requirements. SteamVR dashboard cycling is
kept out of the full/body profile and remains a separate performance-profile requirement so a stuck
system overlay cannot invalidate the body-composition gate. Fresh Release builds all
current targets and runs 25 CTests with **24 PASS plus the expected classic-D3D9 shared-texture
capability SKIP**. No game/SteamVR/headset process was launched for this body increment.

Physical run `20260917T153051Z-ac6a4be37d85` did not validate the body writer. The user reported
that the SteamVR interface was again effectively stuck and that Alt+Tab commonly crashes or stalls
CoJ. Telemetry shows the live `body-enable` command was accepted, but from that point onward no new
game capture was produced: the presenter repeated `capture_sequence=3455` until shutdown, so there
was no frame on which either arm writer could be observed. The verifier failed specifically on the
missing left-arm application, while the dashboard cycle was observed. Finalization preserved the
evidence and restored staging; package SHA-256:
`27C389D1801F79F5F8AEF6C365CAF4A1D45B25B16768489289987403F2536AB1`. The presenter also reported
`shutdown_complete=false`, despite the outer proxy reaching `native_stereo_runtime: status=stopped`
and `run_end`; keep that as a failed shutdown-complete signal rather than promoting it.

To remove the Alt+Tab dependency from the next physical body gate, `tools/vr_test.ps1 prepare`
accepts `-BodyIkAtStart`. It writes the same guarded `bodyIkEnabled=true` control before SteamVR and
the game are launched. Prefer that option for the next body run; retain the live toggle only as an
optional diagnostic when switching windows is known to be safe.

Run `20260917T154759Z-211723af9dc8` has now been executed and finalized. Body IK was enabled before
launch, the SteamVR interface was usable, and the user completed the requested steps but observed no
body/Sense effect beyond the already-established recenter action. Telemetry shows why: every sampled
body update stopped at `body_player_reconciliation result=unavailable` with `stage=player;detail=local
player is unavailable`. This happened while fresh stereo frames continued, so it is a genuine JNI
player-discovery failure rather than the previous Alt+Tab/frame-stall failure. The evidence package
SHA-256 is `8431C07F85B96F9A9339680C90F76788902710ECE434174A96C7A45C50D9DC82`. Presenter shutdown again
reported `shutdown_complete=false` before outer `native_stereo_runtime: status=stopped` and `run_end`.

Shipped `Session.class` provides the correction basis. `Session.PlayerCreated` always appends to
`sm_Players`, but assigns `sm_LocalPlayer` only when `NetPlayer.GetNetIsOwner()` is true. Current
source therefore tries `sm_LocalPlayer` first and, when it is null, accepts the sole `sm_Players`
entry only if the vector contains exactly one player; zero or multiple entries fail closed. This is
exact-build campaign fallback, not a generalized Chrome Engine rule. Sampled `body_tracking_input`
telemetry now records validity/position for both transformed Sense poses, and the body verifier
requires valid position+orientation for both controllers before accepting arm evidence. Release
builds and the full 25-test suite pass with 24 PASS plus the expected capability SKIP; focused
`provenance_tools`, `camera_probe` and `body_adapter` tests also pass. This correction still needs a
fresh physical run.

Run `20260917T161917Z-909b63e114af` has now been physically exercised. Sense tracking itself is
good: sampled `body_tracking_input` has valid left/right position+orientation and changing controller
positions. The fallback did not resolve the actor; every body sample still stops at
`local player is unavailable and the session player list is empty`. The user observed no arm motion,
so body IK remains unvalidated.

Shipped single-player bytecode now provides the concrete actor-discovery fix. `LawmanGame.class`
holds static `sm_cActiveGameModule : LawmanModule`; `LawmanModuleSingle.class` owns
`MainPlayer : PlayerBeing` and exposes `GetMainPlayer() : Being`. Current `JavaPlayerBridge` keeps
the Session routes first, then uses this path only when the Session player vector is empty. It
verifies the active module with JNI `IsInstanceOf(LawmanModuleSingle)` before calling
`GetMainPlayer()`; ambiguous Session state and non-single-player modules still fail closed.
Successful telemetry identifies the source as `lawman_module_single_main_player`. Fresh Debug and
Release builds and complete 25-test suites both produce 24 PASS plus the expected classic-D3D9
shared-texture capability SKIP. Run `20260917T172007Z-e6232c4778d2` later live-proved this route,
skeleton discovery and both arm writers; the remaining failure from that run is transform
composition, described below.

That run also confirms the active comfort problem is still performance/frame pacing. Sampled
`cpu_copy_ms` remains roughly `15-25 ms` per stereo frame at `2560x1440` with `FSAA(8)`, before the
two Chrome Engine eye renders. Presenter submit time is comparatively small. The user also observed
that physical head translation leaves the native body behind. Current host source now suppresses
room-scale HMD translation whenever actor reconciliation is unavailable/invalid/write-failed while
preserving HMD rotation and per-eye IPD. Telemetry records `positional_6dof=false` and a precise
`translation_mode` in that fallback instead of claiming full 6DOF.

`tools/vr_test.ps1 prepare` now defaults to a performance validation profile when
`-BodyIkAtStart` is absent. It keeps the established stereo/runtime/dashboard/recenter/shutdown and
timing gates, but does not require body IK or positional-6DOF promotion. It also backs up the user's
`Documents\call of juarez\out\Settings\Video.scr`, changes only `Resolution` to `1920x1080` and
`FSAA` to `0`, records original/applied hashes in the run manifest and restores the exact original
file on `finish` or prepare rollback. `-KeepVideoSettings` skips that temporary profile. Fresh
Release build and all 25 CTest outcomes complete with 24 PASS plus the expected capability SKIP.

Do not prioritize DLSS yet. The measured hot path is still classic-D3D9 readback/owned CPU copy plus
two complete engine eye renders, which DLSS would not remove. Use the next performance run to compare
copy/frame pacing and subjective comfort at the lighter reversible profile. If that remains
insufficient, investigate removal of the CPU transport (or the later D3D10 path) before selecting an
upscaler insertion point.

Performance candidate `20260917T163732Z-03df947b8d50`, build-manifest ID
`52C4B1822E798CE84BBC8F878E80760AB97DCB729C5C22860BB99E044B935FFC`, proxy SHA-256
`287CC00832F64227DE4912BD7F945573355F0EE04A5A99A3E4B36850831957E3`, has now been physically
exercised, finalized and unstaged. At temporary `1920x1080`/FSAA0 its summary reports
`cpu_copy_ms` average `10.246`, p50 `9.982`, p95 `12.937`, max `15.685`; 3165 frames were
collected, 3163 submitted as new frames, 2042 repeated frames were presented, and there were zero
capture-ring drops or submit failures. The user reported that physical movement no longer leaves
the native body behind. Telemetry confirms this is the expected unresolved-actor fallback,
`positional_6dof=false;translation_mode=rotation_only_actor_unresolved`, so room-scale translation
was suppressed. Both Sense poses remained valid/changing, but `body_ik_enabled=false` in this
performance profile, so it carries no arm-writer evidence. Presenter shutdown still emitted
`shutdown_complete=false` before outer `native_stereo_runtime: status=stopped` and `run_end`.
Evidence package SHA-256:
`C16C889C155F9B1A7C09D7D06DEA6FF9D3B600A97C3D65C49BE46F026935C4CE`.

Run `20260917T172007Z-e6232c4778d2` has now been physically exercised, finalized and unstaged.
Telemetry closes several earlier unknowns in one process: `body_tracking_input` reports valid and
changing position+orientation for both Sense controllers; `body_player_discovery` resolves
`player_source=lawman_module_single_main_player`; `body_player_reconciliation result=ok` writes the
tracked horizontal displacement into the actor; and both sides reach
`body_arm_tracking result=applied` with `plan_valid=true`, `write_allowed=true`, `write_ok=true` and
`writer=FromUpForwardPosElementWorld`. The user also saw both arms respond to the physical
controllers. Actor discovery, controller tracking and the native arm writer are therefore no longer
the blocker.

That same physical run failed the arm transform semantics. The arms contorted sharply behind/over
the body and the first-person mesh became a graphical mass; the shadow exposed the reversed arm
pose. Exact native inspection then showed why. `FromUpForwardPosElementWorld` writes a complete
element world transform, but the previous adapter assigned `element.position = bone_joint` and
derived orientation from `GetBoneDirVector`/`GetBonePerpVector`. That destroys the native mesh
element origin/bind offset even when the IK joint solution is valid. The run remains diagnostic and
must not promote body IK. Evidence package SHA-256:
`86CC35F6EAE2D8EB0C88C97D24723ACE10EE637D93A4368F55F18D09A7339EFB`.

Current source corrects that boundary. `JavaPlayerBridge::TryGetElementWorldBasis` reads
`GetElementPos`, `GetElementLeftVector` and `GetElementUpVector`; exact disassembly shows the shipped
`GetElementForwardVector` writes its vector but returns false unconditionally, so forward is rebuilt
from the paired stored +X/up axes. `BuildArmIkPlan` now rotates each natural element frame around the
measured shoulder/elbow pivot and preserves the native element-origin offset. The forearm target
translates the source elbow pivot to the solved elbow before reapplying the rotated native offset.
Focused host tests assert pivot-radius preservation and a no-op target preserving the complete
natural element frame. `body_arm_tracking` now records natural and target element frames, and the
live verifier requires those fields with
`basis_source=GetElementPos/GetElementLeftVector/GetElementUpVector`.

Fresh full Debug and Release builds both complete all 25 CTests with **24 PASS plus the expected
classic-D3D9 shared-texture capability SKIP**. Corrected candidate
`20260917T175054Z-386939a46734` was prepared with build-manifest ID
`FFF7B749D0DCF1147CB1951F533973D3E7E7B6D92DEA13B24DD339FEB11511C8` and proxy SHA-256
`9BEFBFB2A7E91BF43ECC7E6635FBA21FC56A9F7C4D0C210112591D882BE678B5`, but it never produced a
`cojvr.log`; it was later unstaged and the original `Video.scr` restored. Do not reuse that run ID.
The next full/body candidate must be prepared fresh from the current tree with `-BodyIkAtStart` and
a new run ID. Its physical observation should specifically confirm that both arms follow their Sense
controllers without the previous reverse/overhead contortion or mesh corruption while collecting
the existing stereo/6DOF/recenter/shutdown body gates. Do not require a SteamVR dashboard cycle in
that body-composition run.

The latest physical observation is performance run `20260917T220704Z-b57a36497e54`, build-manifest
ID `9E2B4F538496B691CFE26663CE990306738D2B9CCBC1F4B17CB7DE2F75FBA615`, proxy SHA-256
`E5A4487023E63873B161625C590C7ABD3944E017C63F0352D3A1694FCF850A5F`. The manifest explicitly
excludes Body IK and positional-6DOF promotion (`requireBodyIk=false`,
`requirePositional6Dof=false`); arm samples are observation-only with `write_enabled=false`. The
transport itself was stable: 4,471 frames collected, 4,469 new submissions, 2,075 repeats, zero
ring drops and zero submit failures. Sampled CPU copy is `9.412 ms` average, `8.888 ms` p50,
`14.808 ms` p95 and `17.313 ms` max. Distinct-eye content, explicit render-pose submission, recenter
and active focused/tracking-valid presentation were observed.

This run remains diagnostic. Dashboard open/close telemetry exists only before active presentation,
so the required in-run dashboard cycle after `presenting=true` was not exercised. There is no
explicit `disable`/natural-passthrough event. Finalization again emitted
`native_stereo_presenter_stop: shutdown_complete=false`, although the outer runtime subsequently
logged `status=stopped` and `run_end`; the evidence manifest is therefore provenance-complete but
does not pass the stricter live verifier. Evidence package SHA-256:
`0078E0700F7177A361A2D8C95A051741B7D099AA9EA535E85D68D542B865B04A`.

A follow-up repository audit found and closed one additional host-side arm failure path before that
run. Upper-arm and forearm element writes are separate JNI calls; previously, a successful first
write followed by a failed second write could leave a partially mutated arm. `ApplyArmPlan` now
attempts to restore both natural element frames captured for the same plan whenever either write
fails. `body_arm_tracking` records `rollback_attempted` and `rollback_ok`, reports
`rollback_failed` separately, and the live verifier rejects an `applied` sample if rollback was
needed. Synthetic provenance coverage includes that rejection path. Fresh Debug and Release builds
and complete 25-test suites both produce 24 PASS plus the expected classic-D3D9 shared-texture SKIP.
The current game staging state is `none`; no staged project `d3d9.dll` or `openvr_api.dll` remains.
The temporary performance video profile has been restored, and the current user `Video.scr` SHA-256
is `B4B457D0CEF3DF6C257367AC770FAED11A9CB52343C0D046D705E020F4EE8B1C`.

Full/body run `20260917T222204Z-28ac69c31c56` is finalized and unstaged. It used build-manifest ID
`48D7D4F31E96C477DD441B18F1DC50654CA40B3C3A41DF715A95C491ABEE6D50`. Both arms followed the Sense
controllers and the prior catastrophic element-origin mesh corruption was no longer the observed
failure, but the user reported an apparent front/back inversion: extending the physical arms behind
the body brought the virtual arms forward. The run again ended with
`native_stereo_presenter_stop: shutdown_complete=false` before outer `status=stopped` and `run_end`.
Evidence package SHA-256:
`6B182844CAC4B5C9BAF9E993961C47836012F733C437CEE82EF9D31237741A43`.

The decisive telemetry is the controller target location. Recentered Sense positions were ordinary
sub-metre values, but `controller_target` was emitted near `(0,0,0)` while shoulders/wrists were near
the live actor at roughly `(39700,3600,29400)`. The existing arm path had used the natural renderer
camera translation as a world anchor; at this render boundary that translation is local/zero. This
made the two-bone solver clamp each arm toward the global origin and presented visually as a
front/back inversion. Do not fix this by globally flipping XR Z or by changing the established
camera/eye mapping.

Current source instead reads exact head bone 5 and builds each controller target from that live
world anchor plus `(tracked_hand - tracked_head)` mapped through the exact CoJ camera basis at
`100` units/metre. `body_arm_tracking` now includes `head_anchor`, `tracked_head`, `tracked_hand` and
the resulting `controller_target`. A host test covers both a hand in front of and behind the HMD so
longitudinal direction cannot regress independently of the anchor. Fresh Debug and Release complete
all 25 CTests with 24 PASS plus the expected classic-D3D9 shared-texture SKIP. The next body run must
confirm that controller targets are in the actor/skeleton world neighborhood and that physical
front/back hand motion maps correspondingly before body IK can promote.

That check is now physically resolved by run `20260917T223157Z-8061a216a065`. Its telemetry keeps
both controller targets in the correct live-skeleton neighborhood and preserves mirrored left/right
tracking, while the user still sees both arms reversed/contorted in a T-pose. The remaining blocker
is therefore downstream of IK target generation. The same run also reproduced the SteamVR
dashboard/interface stuck over gameplay; keep that as a separate presentation/focus failure.

Exact-build inspection identifies `BoneRotate(BLVector;FZ)V` as the next arm writer. Its native
handler at RVA `0x0009B6A0` performs real bone/hierarchy rotation and descendant refresh, while
`SetBoneOrientation(BLVector;LVector;)V` at `0x00096D60` is effectively a stub. Current source
converts the solved upper/lower chain into shortest-arc BoneRotate deltas. The forearm delta is
derived only after transforming the natural lower segment by the upper-arm delta, avoiding the
parent rotation being applied twice. Relative writes are scoped to the stereo eye draws and undone
forearm-before-upper-arm after capture. Restore failure fail-closes further arm writes.
`body_arm_tracking` reports both axes/angles and `writer=BoneRotate`;
`body_arm_restore` records the post-capture transaction. The verifier requires a clean restore for
both arms. Fresh Debug and Release suites each complete 25 outcomes with 24 PASS plus the expected
classic-D3D9 shared-texture SKIP.

The next full/body candidate must therefore be prepared from this hierarchy-writer tree with
`tools/vr_test.ps1 prepare -BodyIkAtStart` and a fresh run ID. Physical observation should start
with both arms in a T-pose, then move each hand forward/back/up/down. No dashboard cycle is required
for this body gate. Once arm composition passes, return separately to the recurrent dashboard/focus
failure and sustained frame pacing. The community DualSense mapping supplied by the user is useful
input for the later Sense gameplay-action milestone, but gameplay bindings remain outside this arm
gate.

Run `20260917T230423Z-e63b9146cea7` is now finalized and unstaged. Evidence package SHA-256:
`1371671F7D9F5310A430D583D769AD91EEDBCF7B9896FC3274CBDD86BABC9D95`. The run recorded 4,524
left and 3,332 right `BoneRotate` arm applications with matching successful inverse restores, but
the physical gate did not pass. The SteamVR dashboard/interface became stuck over gameplay, and the
visible portion of the player body appeared to remain on the natural game animation rather than
following the Sense controllers. Treat `result=applied` here as JNI-call success only; there is no
visual proof that the rendered mesh consumed the modified hierarchy. Presenter shutdown again
reported `shutdown_complete=false` before outer `status=stopped`/`run_end`.

The run also separates recenter from the dashboard recurrence: the one OpenVR global-action recenter
press was recorded between an earlier dashboard close and a later dashboard open, while CoJ retained
scene focus throughout the later dashboard transitions. Current source therefore advances two
diagnostic boundaries before the next physical run. The presenter suspends scene submission,
compositor `WaitGetPoses` pacing and game-action polling while SteamVR reports the dashboard visible,
then resumes from the retained newest frame after it closes. The body path emits
`body_arm_write_probe` immediately after `BoneRotate` and `body_arm_render_probe` after both complete
eye renders, so the next evidence can show whether native arm geometry changes and whether the
renderer subsequently overwrites it. Debug and Release each pass all 25 CTests with 24 PASS plus the
expected classic-D3D9 shared-texture capability SKIP. These changes are host-tested only.

Prepare the next full/body candidate fresh from this source with `tools/vr_test.ps1 prepare
-BodyIkAtStart`. Do not reuse `20260917T230423Z-e63b9146cea7`. The body observation remains T-pose
followed by left/right forward/back/up/down controller motion. No deliberate dashboard cycle is
required; if the dashboard appears incidentally, verify only whether it can now close and return to
the scene. The decisive telemetry is the write/render probe transition, not `write_ok` alone.

Candidate `20260918T114514Z-8f16a3b98492` belongs to the superseded `BoneRotate` tree. Do not use
or reuse it for the current body gate.

Run `20260918T160300Z-cd4137a48fca` physically resolved the `BoneRotate` ambiguity. Its 96 sampled
immediate arm-write probes and 192 sampled per-eye render probes all remained natural
(`changed_from_natural=false`) despite successful JNI calls. Tracking, head-relative targets and the
two-bone solution were still valid, so do not redirect investigation back to tracking, target-space,
recenter or renderer overwrite. `BoneRotate` is rejected as the current visible-mesh writer.

Exact-build inspection instead selected `RotateElementWithChildren(ILVector;F)V` at RVA
`0x0009A070`. The handler composes relative rotation onto the live element world transform and
refreshes descendants/attached children. Current source applies upper-arm then forearm rotations,
requires immediate geometry mutation for non-no-op deltas, keeps the changed geometry through both
eye draws, restores forearm before upper arm and verifies return to the natural pre-write geometry.
The verifier requires this full geometry-phase sequence for left and right arms; JNI success alone
is insufficient. Pelvis/leg writes remain disabled.

The separately reported post-load freeze now has non-invasive `post_load_liveness` telemetry. It
samples real input changes, foreground/focus/GUI-thread state, pending keyboard/mouse input through
`GetInputState`, natural camera/frame progression and read-only game-timer state through inherited
`Module.IsTimerFreezed()` on the already-proven active `LawmanModuleSingle`. This resolves the timer
instance safely through `LawmanGame.sm_cActiveGameModule`; no timer object is guessed. The probe does
not consume Win32 messages or synthesize input. Use the next physical run to collect that evidence,
but do not treat the freeze as the cause of arm mutation failure without telemetry.

Fresh Debug and Release completed all 25 CTest outcomes with 24 PASS plus the expected classic-D3D9
shared-texture capability SKIP before the physical run. Candidate
`20260918T164932Z-1daf676be48e` predates that addition, produced no `cojvr.log`, and is superseded;
do not use or reuse its run ID. Its cleanup removed staging and restored the original `Video.scr`;
evidence collection correctly failed because there was no runtime log. Fresh
`-BodyIkAtStart` candidate `20260918T165754Z-845101e7557b` was staged from that tree.
Build-manifest ID:
`51EE466E23FFE5DC16DA502DF5078FFCAC9B3F7C56ED9EBF23CC6997E72B8670`. Proxy SHA-256:
`15AFB4220800DE7C0AF8F5743CB2C291D58A520926AF169525D076214929287E`. Its `full` profile has
tracking/native stereo and Body IK enabled from process start plus the reversible
`1920x1080`/FSAA0 video profile.

Run `20260918T165754Z-845101e7557b` is now finalized and unstaged. Evidence package SHA-256:
`02A6D8EA4DBEF452A01142AC66100F88B982C5B4D4BFB2022F86A2466360DFE4`. It proves that
`RotateElementWithChildren` mutates the visible arm geometry and preserves that mutation through
both eye renders, but it fails body promotion. The user saw brief Sense-driven arm movement with
wrong orientation/deformation. At frame 470 the right inverse JNI calls succeeded but exact geometry
restoration failed; frame 471 fail-closed subsequent writes, explaining why the game returned to its
fixed native arm pose. `post_load_liveness` already reported `foreground_is_game=false` at frame 450
while writes continued, so leaving focus was not itself the reset mechanism.

Exact helper disassembly at RVA `0x001F5700` resolved the orientation defect. The handler copies the
current element matrix and post-multiplies it by the supplied axis-angle matrix, so the Java axis is
element-local. The failed candidate passed solver world-space axes unchanged. Current source
converts the upper world axis through the live upper-element frame, applies it, re-reads the
parent-adjusted forearm frame, and then converts/applies the forearm world axis. The frame-1 telemetry
sample maps `(-0.967098,-0.108973,-0.229883)` world to approximately
`(0,-0.918023,-0.396526)` local; the old interpretation reproduces the observed wrong upper-arm
direction exactly.

The post-write gate now requires elbow and wrist to reach their solved targets within `0.5` game
units, not merely that geometry changed. Inverse restore remains child-before-parent; if float drift
misses the captured sample, the bridge reapplies the exact complete natural element frames (including
their real origins) and verifies them before continuing. The verifier requires local-axis telemetry,
target agreement, both-eye persistence and exact restoration. Fresh Debug and Release builds plus
all 25 CTest outcomes now pass with 24 PASS and the expected classic-D3D9 shared-texture capability
SKIP in each configuration. While SteamVR was still active, six native D3D9 device-creation tests
temporarily returned `D3DERR_NOTAVAILABLE`; closing SteamVR manually restored the expected clean
host result. No game process was launched for the correction.

The first preparation attempt after the dashboard/render-probe changes exposed a Phase 7 script
defect before staging completed: `stage_d3d9_proxy.ps1` used `Select-Object -ExpandProperty` on an
`OrderedDictionary` journal asset, which PowerShell does not expose through that cmdlet even though
normal member adaptation works. Transaction recovery returned the game directory to `Staging:
none`. The script now selects the single `openvr_input` asset explicitly and reads
`.stagedManifest`; focused provenance validation passed and the subsequent real prepare completed
successfully. This supplies one additional successful real stage after recovery, but it does not by
itself constitute the Phase 7 matrix; the later isolated end-to-end transaction suite now completes
that host acceptance coverage.

Do not reuse run `20260918T165754Z-845101e7557b`; it is finalized failed body evidence. Prepare one
fresh full `-BodyIkAtStart` candidate from the corrected source and repeat the same compact physical
arm gate. Do not add pelvis/leg writes or gameplay controls.

Prepare run `20260918T204452Z-d196ba97e34c` was rejected before launch and transactionally
unstaged. The script built/tested `build-win32` but selected the stale artifact from
`build/win32-debug`, preserving old proxy SHA-256 `15AFB422...9287E`; it also recreated the rejected
D3D9Ex marker. Current `vr_test.ps1` now builds, manifests and stages the same
`build-win32/Release/d3d9_native_stereo.dll`, explicitly keeps the marker absent, and
`provenance_tools` asserts both contracts. Do not use or reuse that run ID.

Three new runs are finalized. Full/Body-IK run `20260918T204701Z-f561e493f4ab` recorded 832
successful arm applications before right restore rejection at frame 416; evidence package SHA-256
`42998CBA9277C1B6729D3F062C364533699DFAD3579366CD6CB7B4E9A6A5DC9E`. Full/Body-IK run
`20260918T210459Z-b59448961f3c` recorded 1,228 successful applications before the equivalent left
restore rejection at frame 614; evidence package SHA-256
`06627C0B04CB3DCE92932B7DB068BADE7A81E6903CFF2CCDF27F13FBAA6C155E`. The user saw only fixed
native arms in normal gameplay because each one-off rejection permanently fail-closed subsequent
writes. The local-axis correction itself worked: all pre-fault samples reached their elbow/wrist
targets and remained changed through both eyes.

The root cause was validation precision, not another pose-space failure. The comparator allowed
only `0.001` game-unit positional drift, while one float ULP near the live ~39,700-unit skeleton is
about `0.0039`. Current source separates mutation and restoration thresholds: restore positions
allow at most `0.02` game units and axes `0.001`, telemetry emits measured maxima, and the verifier
enforces them. Host coverage accepts one-ULP drift but rejects visible residual rotation.

Performance run `20260918T210713Z-99d292bd2c94` omitted `-BodyIkAtStart`, so its manifest correctly
had `requireBodyIk=false` and no writes were attempted; evidence package SHA-256
`FA063F2E25F98941F75CA1A67A27712DA86539C2983DC11FD913B322688FDAC0`. Do not treat that as a body
failure. All three runs used the reversible `1920x1080`/FSAA0 profile, directly explaining the soft
image. Their sampled CPU copies were approximately `9.7-10.7 ms`; higher resolution remains tied to
the provisional classic-D3D9 readback cost. All three still reported `shutdown_complete=false`.

The user's head-tilt nausea exposed a separate camera/compositor mismatch: camera roll was omitted
while OpenVR received the full HMD render pose. Current source applies extracted physical roll to
the exact CoJ right/up/forward basis with the tracking `-Z` -> native `+Z` sign conversion, logs
`roll_mode=native_camera_basis`, and requires a meaningful roll sample in the live verifier. Debug
and Release builds and sequential full suites pass 24 tests plus the expected capability SKIP.
No candidate is staged. The next candidate must use `-BodyIkAtStart`; the compact physical check
must include both arm motion and a slow, small head tilt, stopping immediately if discomfort or mesh
deformation appears. Use `-KeepVideoSettings` only if accepting the likely higher CPU-copy cost to
assess image quality; do not conflate that quality A/B with the arm/roll correctness gate.

The first preparation attempt from this source was made while SteamVR remained active. Eight
classic-D3D9 device-creation tests returned `D3DERR_NOTAVAILABLE`, so `vr_test.ps1` stopped before
manifest generation or deployment. Follow-up status confirmed `Staging: none` and no active video
profile. Do not treat that attempt as a candidate. Once the user closes SteamVR, rerun
`tools/vr_test.ps1 prepare -BodyIkAtStart`; it must pass the complete Release suite before staging.

The user then closed SteamVR and preparation succeeded. Full/body run
`20260918T213453Z-a789ac61ac91` is staged with build-manifest ID
`C2FB43FC65A9CD0F0A848A64584322D1D5C70A49F3E3B4E10BE958F1A7B11C90` and proxy SHA-256
`ACF278F40C2C959BD40A8BE016E4F8B73C4DDA8BCC3DAC51845F61191DEDA5FC`; the exact pre-staging
Release suite passed 24 tests plus the expected capability SKIP. Body IK/tracking are enabled from
start, D3D9Ex is disabled and the reversible `1920x1080`/FSAA0 profile is active. The user should
manually start SteamVR and CoJ, avoid dashboard/Alt+Tab, load gameplay, make modest movements with
both Sense controllers, then perform only a slow small head tilt. Stop immediately if deformation
or nausea recurs. Exit normally and run `tools/vr_test.ps1 finish`.

The run is finalized and unstaged. It provides two positive promotions: the user reported no
head-tilt nausea while roll telemetry ranged from `-27.286` to `+40.762` degrees, and all 13,860 arm
applications had successful natural restores with no writer/restore fault. Restore maxima were
`0.00390625` game units for joints/elements and `4.05355e-7` for axes. Transport fenced 6,930
frames with zero drops/submit failures; sampled CPU copy was `9.381 ms` average / `11.404 ms` p95.
Evidence package SHA-256: `A8CE0C3BDDAD405FDC8EADA5CDF3C27E8D5B76AE8E2FDDFE1329ECB145E1B477`.

The physical arm gate still failed. Arms moved and vertical motion behaved correctly, but front/
back was reversed: moving the controllers behind produced arms visible in front. The user screenshot
also shows forearm/wrist deformation. Because target reach, both-eye persistence and restoration all
passed for the complete run, the remaining position defect is the hand-target camera-basis Z sign,
not the native writer. The corrected-Z source for that run changed only `BuildTrackedHandTarget`
from `-forward*z` to `+forward*z`, i.e. tracking `-Z` forward -> negative native forward; X/Y stayed
untouched. Host tests and the verifier bound that exact mapping. Hand orientation remained
natural/uncontrolled in that artifact and was not claimed fixed by the sign change. The user closed
SteamVR; fresh complete Debug and
Release suites each passed 24 tests plus the expected capability SKIP. Full/body candidate
`20260918T215118Z-c46320012ff0` was staged with build-manifest ID
`C3679E1866C80B388E557C8B5B76B6E144945469E8154DB16C5BECE3DB5476B4` and proxy SHA-256
`337EB29BE4000E4C85B0B2EAD30FC9843B84ED35E2DF1691BFAB3645EF824C34`. The run is now finalized and
unstaged after the user ran `tools/vr_test.ps1 finish`. The evidence manifest is complete, the live
verifier passed, and the package SHA-256 is
`CA3F758168AAE782E727EFCF0A24B6F46E821297538BB1A78883AA2CBA93DA74`.

Physical evidence validates the corrected Z mapping: moving both Sense controllers forward now
moves both arms forward. The run recorded 9,296 successful arm applications, 9,296 successful
restores, zero restore failures, and the corrected
`tracking_forward=-z_to_negative_native_forward` marker on every application. Transport fenced
4,648 frames, collected 4,647 and submitted 4,646 new plus 2,934 repeated frames with zero ring
drops or submit failures; sampled CPU copy was `9.092 ms` average / `11.433 ms` p95. Do not revisit
the controller position Z sign unless new physical evidence contradicts this run.

The body gate still fails visually. The user reports both arms are now directionally correct but
"deformadísimos"; this is consistent with the `hand_orientation=natural` contract in that live
artifact. That failure defined the next body task: carry Sense orientation into the exact CoJ arm
composition and resolve forearm roll plus wrist/hand twist while preserving the already-live-proven
writer, target reach, both-eye persistence and exact post-capture restoration.

That next slice is now implemented and **host-tested only**. `BuildHandOrientationReference`
calibrates the current Sense quaternion against the animated native hand basis in camera space;
`BuildTrackedHandOrientationTarget` applies later controller orientation as a relative delta; and
`BuildHandOrientationRotationPlan` decomposes the target into forearm twist around the solved lower-
arm axis plus residual hand rotation. The arm transaction now applies upper arm -> forearm -> twist
-> hand with `RotateElementWithChildren`, then restores hand -> twist -> forearm -> upper after both
eye captures. Natural geometry and restore checks include the hand element, and telemetry/verifier
require `hand_orientation=calibrated_controller_delta`, valid controller orientation,
`hand_orientation_reached=true`, element-local twist/hand axes and clean hand/twist restoration.
Invalid orientation fails closed. No physical claim is made from this host work.

The requested PS VR2 Sense gameplay profile is also implemented and **host-tested only**. It adds
`/actions/gameplay` alongside the existing global recenter set and carries neutral move/turn/fire/
jump/reload/run/crouch/interact/weapon-cycle/kick state through the presenter. The exact CoJ bridge
does not call `SendInput`; it resolves `LawmanGame.sm_cInputController`, the shipped action/target
objects and their digital/analog `Translate` methods so the game's configured device/code/sign
contract remains authoritative. The binding is left stick move + click run, right stick turn +
click crouch, L2/R2 fire left/right, L1/R1 weapon previous/next, Square reload, Triangle interact,
Cross jump and Circle kick; left Create remains global recenter. Gameplay input is neutralized when
dashboard/focus/tracking is unavailable. Full/body manifests now require at least one applied,
non-neutral `gameplay_input` sample through `GameInputController.InputAction.Translate`.

Fresh Debug and Release builds/tests from this orientation/gameplay tree complete all 25 CTest
outcomes with **24 PASS plus the expected classic-D3D9 shared-texture capability SKIP**, zero
failures. Focused body/OpenVR/provenance coverage also passes. The next candidate must be committed
first, then prepared once with `tools/vr_test.ps1 prepare -BodyIkAtStart`; preparation must not
launch SteamVR or CoJ.

That candidate was committed as `bdeb53a` and physically exercised by full/body run
`20260918T233902Z-0cb2e565e886`. Build-manifest ID was
`6196BD90555521CFA35B82FFFC3AE41ABE5593D6CBB8D3CAF234F1D8075F0DD4`; proxy SHA-256 was
`F84719D72A5D28A888B7DEB8603BCF81F0EF1BC2D214B5CFB8637738946E14C4`. The finalized run is complete
(`runtimeStarted=true`, `runtimeEnded=true`, `incomplete=false`): 7,688 fenced / 7,687 collected
frames, 7,686 new plus 4,875 repeated submissions, zero ring drops/submit failures and sampled CPU
copy `10.343 ms` average / `14.449 ms` p95. The inner presenter still did not report
`shutdown_complete`, despite clean outer runtime finalization.

The Sense gameplay route is now physically proven usable through
`GameInputController.InputAction.Translate`; do not claim every individual binding separately
headset-validated. Body IK still fails visual acceptance. Position/orientation target telemetry was
successful, but the user-exported `C:\Users\onita\Videos\clip_1.789.775.964.664.mp4` shows severe
wrist/forearm deformation and repeated local head/hair intrusion into the HMD view.

Static shipped-code inspection after that run found the concrete arm hierarchy omission:
`EBones._L_UPPERARM=7`, `_L_FOREARM=8`, `_L_FORETWIST=9`, `_L_HAND=10` and
`_R_UPPERARM=12`, `_R_FOREARM=13`, `_R_FORETWIST=14`, `_R_HAND=15`. The failed candidate applied
controller pronation/supination on forearm `8/13`, skipping the dedicated twist elements. Current
host source resolves FORETWIST `9/14`, applies the twist there through the already-live-proven
`RotateElementWithChildren` writer, applies only residual orientation to hand `10/15`, includes
FORETWIST in natural/post-write/render/restore geometry and requires
`twist_owner=foretwist_element`. Restore remains child-first: hand -> FORETWIST -> forearm -> upper.
Focused Release `body_adapter`, `camera_probe`, `openvr_runtime` and `provenance_tools` tests pass.
Fresh full Debug and Release builds also pass all host acceptance: each CTest suite reports 24 PASS
plus the expected classic-D3D9 shared-texture capability SKIP out of 25, zero failures. The
FORETWIST correction is therefore ready to commit, but still requires physical visual acceptance.

Fresh full/body candidate `20260919T002540Z-fc19b8ae78a4` was prepared from clean source commit
`5f1dc8f765684af8980f7e98b76f6600222848e7`. Build-manifest ID was
`27FEC7017D7B4BDBC995830DD1FEFF207AEB94DA53E0CFD883A5B889FB8A5E99`; staged proxy SHA-256 was
`8A57AF169246BC98C9F7424E7B92B6340ED48461C738F1643DCE8A9FEF338EA7`. The run is now finished and
unstaged.

The user observed no body motion. Frame-1 telemetry identifies the cause: the first left-arm write
did mutate the mesh and reached the solved elbow/wrist targets, but the hand orientation check
failed (`hand_up_error=0.274044`, `hand_forward_error=0.272201`). The transaction restored the
natural arm successfully and set the existing writer fault latch; every later arm update therefore
failed closed with `prior element writer/restore validation failed`. Tracking, actor discovery,
position solving and FORETWIST element access were all active, so do not regress those paths to
explain the inert visual result.

The failed candidate precomputed the residual hand rotation from an ideal mathematical propagation
of the FORETWIST delta. Current host source instead lets `RotateElementWithChildren` propagate the
FORETWIST through the real hierarchy, re-reads the resulting hand basis, and recomputes the final
residual directly from that observed basis to the calibrated Sense target. Telemetry/verifier now
require `hand_residual_source=post_foretwist_observed_basis`. Focused Release body/camera/OpenVR/
provenance tests pass, and fresh full Debug/Release suites each complete all 25 outcomes with 24
PASS plus the expected classic-D3D9 shared-texture capability SKIP. Pelvis/leg writes remain
disabled.

Full/body run `20260919T003727Z-73f20e13cc1a` was physically exercised from clean source commit
`2213ae6a2903581f8c82c43bb01a1cc6705b0b00`. Build-manifest ID is
`172178E5ECDC340E563C4F0FD1412DC3E5AD477D142605472F87445B11FF0CA4`; proxy SHA-256 is
`E0093D6ACDDE08411B16045AC87709F22EAA0188B7A8D14E4C189A9A40A8926B`. `prepare -BodyIkAtStart`
reran the complete Release suite with 24 PASS plus the expected capability SKIP and staged the
reversible `1920x1080`/FSAA0 full profile with Body IK enabled before process start. The physical run
kept the writer active for its full 7,615-frame lifetime: 15,230 arm applications and 15,230 clean
restores completed with no writer/restore failure. Visual acceptance still failed because both arms
were heavily deformed. Telemetry showed that the mathematical target could be reached only through
extreme controller-frame rotations, with sampled values reaching about 132 degrees FORETWIST plus
155 degrees residual hand rotation. The remaining orientation defect is therefore upstream of the
proven element writer/restoration path.

The user's capture also showed SteamVR's dashboard visibly stuck while the Sense render models and
laser pointers were available. Sony's installed SteamVR profile exposes `raw`, `base`, `handgrip`,
`tip` and `openxr_aim`; the compositor's own laser binding uses `tip`. Current host source therefore
uses explicit left/right `handgrip` pose actions for body IK and fails closed when either action is
inactive/invalid; raw tracked-device-role poses are no longer anatomical body targets. `tip` remains
reserved for the later weapon-aim milestone. Dashboard visibility no longer suppresses scene
submission or global pose/recenter polling; gameplay actions are neutralized while the overlay owns
interaction. A successful explicit recenter invalidates hand-orientation calibration and clears a
latched arm-writer fault only when there is no active arm transaction and fresh natural geometry is
healthy. The live verifier now requires the handgrip source, no raw fallback and the safe recenter
recovery marker. Fresh Debug and Release suites each pass 24 tests plus the expected capability SKIP
out of 25. These changes are host-tested only and require one fresh combined body run.

That combined body candidate was physically exercised as run
`20260919T011421Z-bef5076cd07e` from clean source commit
`1027a67392f1c5baa71bbba8b6c39c9b63444983`. Build-manifest ID was
`0AB9BCF9BE628504EA5961092681A9550978092D71C8CFF619FD035B304D906C`; proxy SHA-256 was
`CC03E4DD55929CC4154ABB8A88B836A7FE6C60E5880E67F0EAE9B82D481ECBB6`. The evidence manifest is
complete and staging is clear. The run sustained 14,968 arm applications and 14,968 clean restores
with zero restore failures, and seven recenter recoveries succeeded. Explicit handgrip actions were
therefore live-exercised without raw-role fallback, but the arms still failed visual anatomy.

Analysis of the deliberate pose sequence isolates the next candidate. Elbow/wrist position solving
continued to reach its targets, and in the palms-up segment FORETWIST was already close between the
two sides (~89/~96 degrees average). The extra hand residual remained extreme (~98/~122 degrees
average) and is the next controlled variable. Current host source still computes the residual from
the observed post-FORETWIST hand basis, but records it as diagnostic only and never writes it to the
hand element. The verifier requires `hand_rotation_mode=foretwist_only`,
`hand_rotation_no_op=true`, `hand_orientation=calibrated_controller_delta_foretwist_only` and the
diagnostic residual marker while still requiring positional target reach, both-eye persistence and
natural restoration. Fresh Debug and Release suites each pass 24 tests plus the expected capability
SKIP out of 25. Preparation `20260919T085249Z-0caf8c569979` was deliberately unstaged before any
game/SteamVR launch because its build manifest recorded the then-uncommitted tree as `dirty=true`;
it has no physical evidence and must not be used for promotion. The host changes were committed as
`18057534b960522613e30b8aa2dc20e02d35eb1e` (`Harden VR math and deployment transactions`). A fresh
clean full/body candidate is now staged as run `20260919T085408Z-327dd354bc4f`, build-manifest ID
`79EE4829D5076EB59A96F71609ACCFB37642E42788BDA30A9A28B6F4A5801213`, proxy SHA-256
`3BD0B476810B15FBD935FA96F539E7303439B372F6BC45DA57BB40595CCE693F`. Its source identity is that
commit with `dirty=false`; preparation reran all 25 Release outcomes with 24 PASS plus the expected
capability SKIP, enabled Body IK at process start and applied the reversible `1920x1080`/FSAA0
profile. This is preparation/host evidence only. The next action is one physical visual run of this
twist-only composition; do not regress handgrip, tracked-Z, FORETWIST ownership or the visible writer
while evaluating it.

Three separate product milestones are now explicitly recorded for later work: suppress the local
head/hair from the HMD view while preserving the body, derive physical crouch from calibrated HMD
height through the game's crouch action/state, and decouple firearm/muzzle aim from the flat
camera/crosshair so tracked controller/weapon orientation owns shot direction. Do not bundle these
into the FORETWIST body correction; each needs its own exact-game ownership research and gate.

## Constraints

- Never launch Call of Juarez or SteamVR automatically.
- Do not use blind periodic re-hooking.
- Do not reactivate D3D9Ex as the main path.
- Positional 6DOF and the isolated body/arm IK preflight remain in scope. Campaign actor discovery,
  controller tracking and actor reconciliation are live-proven for the exact build. `BoneRotate` is
  physically rejected as a visible writer; `RotateElementWithChildren` is now live-proven to mutate
  the mesh. The latest physical orientation candidate still deformed the arm despite sustained
  FORETWIST/hand target reach. Explicit PS VR2 `handgrip` tracking and recenter recovery are now
  physically exercised; the next body candidate keeps that contract and disables only the residual
  hand-element writer (`foretwist_only`). Keep body semantics exact-build-first and fail closed; do
  not extend the unvalidated arm writer into pelvis/legs.
- UI rebuilding, local head suppression, physical crouch, weapon-aim ownership and interaction
  redesign remain outside the current FORETWIST gate. The exact-game Sense gameplay route has live
  physical evidence without implying those later interaction milestones are complete.
- Do not equate submit count with new game content.
- Preserve the validation-state boundary: host-only Phase 5/6 changes remain `host-tested` until a
  fresh run supplies the specific live/headset evidence required by the consolidated verifier.
