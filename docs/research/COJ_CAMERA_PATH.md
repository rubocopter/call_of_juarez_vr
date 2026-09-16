# Call of Juarez camera -> renderer path

Evidence state: the static path is recorded from exact-binary host inspection; the
diagnostic implementation is **live-tested** on the exact game/engine build.

This note records exact-build reverse-engineering evidence for Call of Juarez (2006).
These addresses and layouts belong to the inspected game backend only and must not be
promoted to a generic Chrome Engine contract without evidence from another game.

## Exact binaries

- `CoJ.exe` SHA-256: `5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`
- `ChromeEngine3.dll` SHA-256: `DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8`

The camera probe rejects every other executable/engine combination before applying any
game-specific patch.

## Java camera entry points

`code.pak` exposes camera operations that provide useful reverse-engineering anchors:

- `Camera.SetFOV`, `SetAlternativeFOV`, `SetBaseFOV`, `GetFOV`;
- `Camera.Rotate(FFFF)`;
- `Camera.FromForwardUpPos(Vector,Vector,Vector)`;
- position, left/up/forward vector getters and setters;
- clip near/far control.

`FreeCamera` bytecode shows `Rotate(FFFF)` is used as axis-angle rotation for roll, yaw
and pitch, while FOV is updated through `SetFOV`.

## Camera -> active view

`LawmanModule.SetViewFullscreen(Camera)` creates/activates the main view and calls native
`SetViewCamera`. In the inspected engine, `Module.SetViewCamera` is RVA `0x00039710`.
It resolves the Java camera to the native camera object, detaches the previous camera,
stores the new one on the view and performs the corresponding attach callbacks.

This establishes:

```text
Java Camera
  -> native CBaseCamera
  -> active CLevelView
```

## CBaseCamera profile

MSVC RTTI (`CBaseCamera`, `CCamera`, `IBaseCamera`) identifies the primary
`CBaseCamera` vtable at RVA `0x0030AE1C`.

Relevant exact slots are:

| Slot | Target RVA | Evidence |
| --- | ---: | --- |
| `+0x04` | `0x0022C4D0` | forward/up/position transform update |
| `+0x10` | `0x0022C510` | basis/position update reached by `Camera.Rotate` |
| `+0x24` | `0x001C5BA0` | render-camera/FOV/matrix update boundary |
| `+0x28` | `0x001C58E0` | FOV/frustum virtual used by the render update |

The exact build exposes a paired camera-transform contract. The world/camera transform
begins at right `+0x44`, up `+0x54`, forward `+0x64`, position `+0x74`; its inverse/source
view matrix begins at `+0x04`. Native setters at `0x0022C4D0` and `0x0022C510` update
`+0x44` and then call matrix-inverse RVA `0x001F3F80` to rebuild `+0x04`.

`0x0022BB10` copies those two synchronized inputs into two downstream paths: `+0xC4`
from `+0x44`, and `+0x84` from `+0x04`. It then produces the effective view matrix at
`+0x104`, a second world/culling transform at `+0x144`, projection at `+0x184`, and
view-projection at `+0x204`.

The adjacent `+0x24/+0x28` target pair occurs once in the inspected DLL, and the camera
constructor at RVA `0x001C65BD` assigns this exact primary vtable to the native camera.

## View/projection -> renderer

RVA `0x001C5BA0` is the most useful functional boundary found so far. It:

1. calls the camera virtual at slot `+0x28` with the current FOV/frustum inputs;
2. calls RVA `0x0022BB10`, which consumes the synchronized `+0x04` inverse/view source
   and `+0x44` world/camera source, then builds `+0x104` view, `+0x144` world/culling,
   `+0x184` projection and `+0x204` view-projection matrices;
3. updates projection-related renderer globals;
4. loads the renderer through global pointer RVA `0x00590074`;
5. stores the current camera pointer at renderer offsets `+0x1AC` and `+0x1B0`.

RVA `0x0022BC50` visibly constructs the projection matrix at camera `+0x184` from
frustum bounds and near/far values. The engine also contains the renderer transform
descriptors `VIEW_XFORM`, `MODELVIEW_XFORM`, `VIEWPROJ_XFORM` and `PROJECTION_XFORM`.

The resulting static chain is therefore:

```text
Camera / CBaseCamera
  -> active level view
  -> CBaseCamera render update (+0x24)
  -> FOV/frustum (+0x28)
  -> view + projection + combined matrices
  -> ChromeEngine3 renderer camera (+0x1AC/+0x1B0)
```

## Functional probe design

`d3d9_camera_probe.dll` uses D3D9 only as a load/bootstrap and forwards
`Direct3DCreate9` directly to the Windows system runtime. It does not initialize OpenVR
and does not hook D3D9 `Present`, `BeginScene`, `EndScene` or `Reset`.

After validating both exact binary hashes and the expected vtable targets, it installs
two `HookRegistry` patches:

- slot `+0x24`: when external orientation control is enabled, snapshot both source
  transforms, apply the transient orientation at `+0x44/+0x54/+0x64`, recompute the
  inverse at `+0x04` with the engine's own `0x001F3F80`, let `0x0022BB10` derive culling,
  view and view-projection from the synchronized pair, then restore both source matrices
  immediately after the original function returns;
- slot `+0x28`: while that same render update is active, replace only the FOV argument with
  the externally configured diagnostic FOV and call the engine's original FOV/frustum work.

The probe records the natural/applied basis, camera position, FOV values, whether the
renderer references that camera after the update, and hook restoration on normal exit.
The control file is `cojvr-camera-control.json`; `tools/set_camera_probe_control.ps1`
updates it atomically while the game is running.

## First camera-path live run — 2026-09-15

One ordinary DX9 gameplay run is sufficient. SteamVR and the headset are unnecessary.
The gate passes only when all of the following are true:

1. run provenance matches the exact `CoJ.exe`, exact `ChromeEngine3.dll` and staged
   `d3d9_camera_probe` artifact;
2. the engine profile/vtable targets are accepted and both hooks install safely;
3. an external command produces an obvious FOV plus yaw/pitch change in the game view;
4. telemetry reports `camera_probe_orientation_applied` with
   `renderer_camera_match=true` and `restored=true`;
5. telemetry reports `camera_probe_fov_applied` for the same controlled path;
6. disabling the command returns the game to its natural camera behavior;
7. normal exit restores the camera-vtable hooks and produces the bound `run_end`.

The run established the exact camera/render path, external FOV control, renderer-camera
correlation, passthrough and clean hook restoration. A later HMD live attempt exposed that
the original yaw/pitch injection point was downstream of the authoritative source basis and
was overwritten by `0x0022BB10` before render consumption. The earlier simultaneous
FOV/yaw/pitch observation therefore does not independently prove rendered yaw/pitch.

Run `20260915T150554Z-7e0d7da45949` remains valid live evidence for the path and FOV
boundary. The user observed an obvious view change with FOV `110`, yaw `20` and pitch
`-10`; telemetry recorded the command, `camera_probe_fov_applied`,
`camera_probe_orientation_applied`, `renderer_camera_match=true` and `restored=true`.
Disabling returned to natural passthrough; normal exit restored both camera-vtable slots
and emitted the bound `run_end`. Because the FOV change was simultaneous, this run no
longer promotes yaw/pitch render control by itself.

`tools/verify_camera_probe_live_test.ps1` passed exact run/build/deployment identity,
exact `CoJ.exe`/`ChromeEngine3.dll`, renderer-camera correlation, FOV/orientation
application, basis restoration and hook restoration. The collected evidence package is
SHA-256 `2C41F3668EC4F7C1167C2BC7D3B88455119EC06E3FBB3E89C382074B52644866`.

## HMD orientation integration boundary — host-tested 2026-09-15

The live-tested camera path is now split into four explicit responsibilities:

- **exact-build discovery/hooking:** `camera_probe.cpp` owns the inspected CoJ/ChromeEngine
  hashes, `CBaseCamera` RVAs/field offsets, renderer-camera correlation and the two
  `HookRegistry` patches. These details remain Call of Juarez specific;
- **reusable pose/recenter policy:** `runtime::PoseSource`, `PoseSample` and
  `RelativePoseTracker` carry backend-neutral pose data and base-orientation capture;
- **diagnostic external control:** JSON yaw/pitch/FOV remains available only through the
  original `d3d9_camera_probe` path for repeatable engineering diagnostics;
- **VR camera integration:** `d3d9_hmd_camera` supplies an OpenVR HMD pose source to the
  same transient camera override. ChromeEngine code sees only the neutral `PoseSource`
  boundary and has no OpenVR/OpenXR dependency.

The neutral tracking convention is right-handed `+X` right, `+Y` up, `-Z` physical
forward, metres, quaternion `(x,y,z,w)`. `OpenVrRuntime::WaitForHmdPose` supplies the
device-to-standing-tracking transform already used by the live-tested OpenVR probe.
`RelativePoseTracker` captures the current orientation as the forward/base pose and
computes `R_relative = R_base^T * R_current`; it never integrates frame deltas.

Call of Juarez world/gameplay translation uses centimetres. This is explicit in shipped
`Data0.pak`: `Data/Player/PlayerProperties.def` documents `MoveSpeed` as `[cm/s]` and movement
acceleration/deceleration as `[cm/s^2]`; Billy and Ray both use `MoveSpeed(550)`. XR poses stay
in metres across the shared runtime, so the exact CoJ adapter converts positional eye offsets
with `100` game units per metre. The pre-fix native-stereo run `20260916T133322Z-36c287cc43d8`
therefore used an eye baseline 100x too small even though the two eye renders themselves were
distinct.

The inspected CoJ camera basis uses natural right = `cross(up, forward)` and local
`+forward`. The adapter maps the neutral XR basis into that local basis by reflecting the
XR Z coordinate (`-Z` physical forward -> CoJ `+forward`) and composing the resulting
right/up/forward vectors onto the natural game camera. Host tests verify identity/recenter,
approximately `+20°` physical yaw -> `+20°` camera yaw, pitch direction/scale, roll basis
preservation, return to the physical origin without accumulation, invalid-pose passthrough
and immediate disable passthrough.

The first implementation applied orientation to the derived basis at
`+0xC4/+0xD4/+0xE4` from the hooked FOV boundary. The first physical HMD attempt proved
that this was too late: pose acquisition/recenter/sequence telemetry advanced correctly,
but head motion produced no visible camera movement. Static inspection then showed
`0x0022BB10` rebuilding that derived basis from the source transform beginning at `+0x44`.

The first source-basis correction still changed only `+0x44`. Run
`20260915T211240Z-7d1c65d07a43` proved why that is insufficient: OpenVR/recenter and
orientation telemetry were valid, and the `+0xC4/+0x144` path changed, but the user saw
the player camera/body/weapon remain fixed while distant world geometry appeared or
disappeared according to HMD direction. Turning the headset away could make the scene
nearly empty. This is direct live evidence that this half of the pair drives world/frustum
visibility without rotating the final view. The run exited cleanly and its evidence package
is SHA-256 `8E015C2B39326EE2E9C52804E9C6E2E03262088B1B3FFB05E5BC92747B45A5D2`; the hardened
verifier intentionally rejects it because the final view path was not proven.

Static inspection then showed the missing half of the native contract. `0x0022C4D0` and
`0x0022C510` always rebuild `camera+0x04 = inverse(camera+0x44)` through `0x001F3F80`.
`0x0022BB10` copies `+0x04 -> +0x84 -> +0x104`, and projection multiplication uses
`+0x104` to produce `+0x204`. The implementation now mirrors that exact native sequence:
apply HMD orientation to `+0x44`, recompute `+0x04`, run the original update, then restore
both source matrices. Telemetry/verifiers require changes in both the world/culling path
and `+0x104/+0x204` before orientation can count as render-view evidence.

`d3d9_hmd_camera.dll` uses D3D9 only for load/bootstrap and system `Direct3DCreate9`
forwarding. It installs no `Present`, `BeginScene`, `EndScene` or `Reset` hooks. OpenVR is
used for this gate because its existing x86 pose path is already live-tested with PSVR2;
OpenXR remains substitutable behind `PoseSource` after its existing lifetime/semantic debt
is resolved.

With CoJ closed, the synchronized view/world correction passed fresh Debug and Release
builds and both full CTest suites with **17 PASS plus one expected capability SKIP** and was
therefore **host-tested**. It was staged as run `20260915T212754Z-a12fcb10f11a`, build
manifest `0B73CD2A748762A6A8FBE7A47E610534B0033C08B5B49FDE13BE23DC810F6180`, proxy
SHA-256 `CAD1A6153F81026FD90D43570F6E370F9B8DE2AD9790C180B67C770965FAFCA0`.

That run supplied the missing visible-view proof: advancing HMD samples produced
`render_basis_changed=true`, `view_matrix_changed=true`, `view_projection_changed=true`,
`restored=true` and `renderer_camera_match=true`, and the user saw the first-person camera
rotate on the monitor. The direction was inverted relative to physical head motion and
large parts of the environment disappeared, so the 1:1 camera gate failed. This is useful
live evidence that direct device-to-tracking orientation composition has the wrong sign
contract at the exact CoJ source-basis boundary.

Tracking was disabled in the same process and generation 3 returned to
`camera_probe_passthrough`. After normal user shutdown the HMD verifier passed identity,
pose/recenter progression, render/view changes, passthrough, hook restoration, XR shutdown
and `run_end`; the package SHA-256 is
`FDEDBFD5427892A16208C678EA5145AE22D72BE83DBFD37CED2A4BE3915F888D`. The structural
verifier pass does not promote the run because the manual direction/visibility gate failed.

The `-yaw/+pitch` candidate was then exercised as run `20260915T214329Z-3d9f66ae064e`.
Pitch followed physical up/down, but yaw was still reversed; most environment geometry was
missing/white and the normally right-hand weapon appeared on the left side of the camera.
Tracking was disabled in-process and generation 3 returned to natural passthrough. After
normal shutdown the structural HMD verifier passed identity, pose/recenter progression,
render/view changes, passthrough, hook restoration, XR shutdown and `run_end`; evidence
package SHA-256 is `AE5B87AE5F54F68D6EAFE6D6E68C41F737DB9134A0FF443E5C122C477BF2D965`.
The run remains failed manual evidence because direction, visibility and handedness failed.

That observation led to a more specific static check. Exact-build disassembly of native
`FromForwardUpPos` helper RVA `0x001F3150` proves the matrix beginning at camera `+0x44` is
**right/up/forward/position**: it normalizes forward, computes `right = up x forward`, stores
that right vector at matrix `+0x00` (`camera+0x44`), then derives the corrected up vector.
The hook had instead reconstructed `camera+0x44` as `forward x up`, i.e. the left vector.
That single sign error reflected the camera horizontally and directly explains the mirrored
weapon side; it is also consistent with the reversed yaw and broken culling.

Current source carries `right` explicitly in `CameraProbeBasis` and writes that native right
axis at `+0x44`. Run `20260916T104036Z-24b3e3010d4c` exercised that corrected basis in the
physical HMD. The user reported that the character model was back on the expected side and
did not observe the previous disappearing/white environment; pitch also followed physical
up/down correctly. Horizontal movement was still inverted. Telemetry simultaneously kept
natural/applied determinants at `+1`, preserved the complete source-world/source-view
homogeneous contract, changed the render/view/view-projection paths and matched the renderer
camera. This separates the old reflection/culling bug from the remaining yaw convention.

`HeadAngles` continues to expose positive physical yaw for tracking-space `-Z` forward
turning toward `+X`, but the exact CoJ paired source world/view path has now shown its visible
horizontal response empirically. The game-specific HMD adapter therefore uses
`-physical.yaw` while retaining the native right-handed `right = up x forward` basis. Pitch
keeps its live-confirmed sign and roll remains excluded.

The exact engine copy in `work/analysis/ChromeEngine3.dll` was re-hashed to the expected
`DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8` and disassembled again
before another live attempt. The remainder of `FromForwardUpPos` confirms a conventional
rigid homogeneous source matrix: right at row `+0x00`, corrected up at `+0x10`, normalized
forward at `+0x20`, position at `+0x30`, row `w` values `0/0/0/1`. `0x0022C510` independently
normalizes the three source axes, then calls `0x001F3F80` to rebuild the inverse and
`0x0022BB10` to derive the render/culling/view paths. This rules out an undiscovered
row/column transpose as the explanation for the observed mirror.

The implementation now treats the complete paired source transform as an explicit invariant.
Both the source world matrix at `+0x44` and source view/inverse matrix at `+0x04` must have
the native homogeneous layout before injection. After native inverse generation the new
view matrix is validated again before the engine update consumes it. The natural and
applied bases must be unit, orthogonal and have determinant approximately `+1`; invalid or
reflected transforms immediately use natural passthrough and restore the original bytes.
Host tests sweep yaw `[-60,+60]` and pitch `[-45,+45]`, require `right = up x forward`, and
reject reflected, scaled, skewed and non-finite bases. The HMD live verifier is also executed
against synthetic run evidence in host tests: the valid contract passes, while a non-rigid
natural determinant and an invalid source-view homogeneous layout are rejected. Fresh full
Debug and Release suites pass with **17 PASS plus one expected capability SKIP out of 18**.

The first native-stereo implementation combined the outstanding yaw confirmation with the
stereo proof. Exact-build inspection identified render-view method RVA `0x00030FB0` and core
RVA `0x00030E00`. That implementation rendered the left eye through the original method and
invoked the core once more for the right eye, with distinct eye-to-head translation and
asymmetric frustum values at the proven `CBaseCamera` boundary. Later live evidence showed
that the right eye also needs the complete wrapper; the current full-wrapper replay is
documented below.

The stereo bridge keeps XR/runtime details outside this file's game adapter through
`CameraStereoRuntimeCallbacks`. The current OpenVR producer defines `EyeView::pose` as
eye-to-head. After each complete eye render, the candidate restores the derived camera blocks
at `+0x84`, `+0xC4`, `+0x104`, `+0x144`, `+0x184` and `+0x204`; the source world/view pair
and native frustum are also restored by the camera update path. This prevents a completed
right-eye pass from becoming persistent engine state.

The first transport is deliberately simple: classic-D3D9 CPU readback of each engine eye,
upload into two separate D3D11 textures, then OpenVR stereo submission. No `Present`,
`BeginScene`, `EndScene` or `Reset` hooks participate in the candidate.

The first physical attempt, run `20260916T113600Z-native-stereo`, proved that HMD orientation
still reaches the visible camera but did not reach stereo submission. All 13,558 attempted
stereo frames stopped at `left_projection_applied=false`; there were no eye captures or
OpenVR stereo submissions, matching the user's observation that SteamVR still showed a flat
theater screen. The captured OpenVR eye data exposed the cause: raw vertical projection
tangents use negative top/positive bottom, opposite to neutral `EyeFov` up/down signs. The
OpenVR adapter now flips only those API-specific vertical signs before neutral conversion.

The same observation showed geometry selection remaining tied to the game's natural facing
direction while HMD view rotation continued. Re-inspection of `0x00030E00` confirms that the
camera virtual update at `0x00030ECA` occurs before later scene/visibility calls. The first
stereo hook restored source/frustum state as soon as the camera update returned. The corrected
implementation keeps the eye-specific source world/view pair, frustum and derived matrices
through the complete render-view pass and restores the whole snapshot afterward. This is the
smallest change consistent with the observed culling failure and the exact render ordering.

Fresh Debug and Release host suites now pass with **18 PASS plus one expected capability
SKIP**. At that point the candidate remained **host-tested**, not live-tested or headset-validated. A fresh
single-process HMD run must prove corrected projection, eye capture/submission, visible stereo
depth, stable scene visibility, corrected yaw/pitch, disable-to-natural passthrough and clean
restoration/shutdown.

The next run, `20260916T123049Z-8d977bb5b439`, progressed substantially further. Both eye
camera/projection passes executed, both captures reported success and OpenVR accepted the
submissions; after a gameplay save loaded, the user saw the game in the headset. The result
was uncomfortable and did not fuse correctly. Telemetry explains why: every sampled stereo
frame (`1..8`, then `90`, `180`, `270`, `360`, `450`) reported exactly equal left/right RGB
hashes, despite distinct eye translations and asymmetric FOV. The old capture path called
`GetBackBuffer(0,0)` from inside the render-view hook. At that point ChromeEngine may still be
rendering the eye into a bound intermediate target, so the backbuffer can remain the same for
both captures. This run therefore proves headset presentation, not native stereo.

The next transport revision captured `IDirect3DDevice9::GetRenderTarget(0)` at the exact eye
boundary, recorded whether that surface aliases the swap-chain backbuffer, and keyed readback
resources to the active capture-surface description. It also refused OpenVR submission when
the two eye hashes were equal. Renderer-camera identity is sampled during each eye's camera
update rather than after the render pass has restored/moved renderer state.

The same run is incomplete for shutdown evidence: after tracking disable it restored the
camera/render-view and factory hooks, but no `native_stereo_runtime: status=stopped` or
`run_end` was emitted. Final D3D9 COM release during CRT teardown is a suspected boundary, not
a proven root cause. The candidate now shuts down readback/OpenVR and emits final runtime
telemetry before dropping its process-lifetime retained D3D9 pointers; the operating system
reclaims those remaining references on process termination. Package SHA-256 for the failed
run is `C45162E2CB9584BBA0454319E62B1AA5438A6C973193FD64F86A377DA047CEEE`.

Run `20260916T130852Z-1438628c90c6` then isolated a different boundary defect. The user saw
correct horizontal and vertical HMD camera motion, but the headset remained flat throughout.
For more than one thousand attempted frames, the left eye captured a `2560x1440`
`D3DFMT_A8R8G8B8` RT0 that also matched the swap-chain backbuffer. The right eye reached the
same camera/projection path and renderer-camera identity, but capture saw decimal format
`1280070990`, which is `D3DFMT_NULL` (`'NULL'`), and therefore produced no right hash or OpenVR
submission. This is not a missing format implementation: `D3DFMT_NULL` identifies an
auxiliary/null target and must fail closed. The run is diagnostic only because its log contains
three `run_start` process IDs under the same run ID and no clean `run_end`.

The exact `ChromeEngine3.dll` disassembly explains that asymmetry. `0x00030FB0` is a full
render-view wrapper, not merely a thin entry point: it tests `view+0xD7`, sets that byte to mark
the view rendered, stores the active view at owner `+0x3B8`, calls core `0x00030E00`, and then
executes additional post-core routines. The failed candidate used this full wrapper for the
left eye but invoked only `0x00030E00` for the right. Current source now runs both eyes through
`0x00030FB0`; before the right eye it temporarily clears the exact-build `view+0xD7` guard and
restores the post-left guard value after the wrapper returns. Camera/frustum state remains
transactional per eye. Telemetry and the live verifier require proof of the full right-eye view
pass and guard restoration, while the readback path reports `D3DFMT_NULL` explicitly as a wrong
capture boundary.

Fresh Debug and Release host suites after the full-wrapper correction both pass **18 PASS plus
one expected classic-D3D9 shared-texture capability SKIP out of 19**. At that point the implementation
remained **host-tested** until a fresh one-process headset run proved a real right-eye color
surface, distinct eye hashes, compositor submission, usable binocular depth and clean shutdown.
