# Codex handoff — Call of Juarez VR

## Current checkpoint

Audit-remediation Phases 0-4 remain **host-tested**, and their two run-bound manual Call
of Juarez observations remain authoritative. The user explicitly deferred repeating the
prepared Steam-Overlay-disabled A/B. The separate exact-build camera-path gate has now
passed live validation: `Camera -> View/Projection -> ChromeEngine3 renderer` and
reproducible external orientation/FOV control are proven in the exact game build without
SteamVR/headset involvement. The follow-on XR-neutral HMD orientation integration is now
**host-tested** and the next manual gate is physical HMD rotation driving the monitor
camera through that same proven renderer path.

No Call of Juarez, SteamVR or headset process was launched during the current HMD-camera
implementation/host-validation pass.

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
- Monocular HMD orientation injection is implemented/host-tested; stereo rendering,
  positional 6DOF and motion-controller gameplay are not implemented.

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
- Fresh Debug and Release suites each produce 17 PASS and one explicit capability SKIP out of 18 tests.
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

Follow-on HMD candidate, currently **host-tested**:

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
  passthrough; the natural basis is still restored immediately after each render update;
- Debug and Release builds pass all 18 CTest outcomes (17 PASS plus the existing explicit
  classic-D3D9 capability SKIP);
- `tools/set_hmd_camera_control.ps1` provides enable/recenter/disable and
  `tools/verify_hmd_camera_live_test.ps1` binds the eventual run to exact binaries,
  advancing pose samples, renderer-camera identity, restoration and shutdown;
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

The camera-boundary proof no longer needs another manual repeat. The next required evidence
is one exact-build `d3d9_hmd_camera` run with SteamVR/HMD started manually: enable tracking
at the physical forward pose, verify approximately 1:1 yaw/pitch on the monitor, return to
origin, optionally recenter, disable tracking and confirm immediate natural passthrough,
then exit normally and run `tools/verify_hmd_camera_live_test.ps1`. Do not begin stereo
work until this gate passes. Do not generalize the exact `CBaseCamera` layout/RVAs beyond
Call of Juarez until another game proves the same contract. The deferred Steam Overlay A/B
remains relevant only to the separate D3D9 presentation-hook finding.

## Constraints

- Never launch Call of Juarez or SteamVR automatically.
- Do not use blind periodic re-hooking.
- Do not reactivate D3D9Ex as the main path.
- Keep camera work limited to the current monocular HMD-rotation gate; do not add stereo,
  positional 6DOF, UI or controller gameplay yet.
- Do not equate submit count with new game content.
- Do not promote any Phase 0-4 result above `host-tested` until the exact manual run supplies live evidence.
