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

Native camera fields exposed by the bridge include left `+0xC4`, up `+0xD4`, forward
`+0xE4` and position `+0xF4`.

The adjacent `+0x24/+0x28` target pair occurs once in the inspected DLL, and the camera
constructor at RVA `0x001C65BD` assigns this exact primary vtable to the native camera.

## View/projection -> renderer

RVA `0x001C5BA0` is the most useful functional boundary found so far. It:

1. calls the camera virtual at slot `+0x28` with the current FOV/frustum inputs;
2. calls RVA `0x0022BB10`, which copies/builds view-related matrices and derives the
   combined matrices;
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

- slot `+0x24`: when external control is enabled, snapshot the natural basis and mark the
  current render-camera update; after the original function completes, restore the natural
  basis immediately;
- slot `+0x28`: while that same render update is active, replace its FOV argument with
  the externally configured FOV, call the engine's original FOV/frustum work, then apply
  the configured yaw/pitch basis immediately before control returns to `+0x24` and its
  `0x0022BB10` matrix construction.

The probe records the natural/applied basis, camera position, FOV values, whether the
renderer references that camera after the update, and hook restoration on normal exit.
The control file is `cojvr-camera-control.json`; `tools/set_camera_probe_control.ps1`
updates it atomically while the game is running.

## First live acceptance gate — passed 2026-09-15

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

Passing this gate proves external control of the game camera at the renderer boundary.
It does not prove stereo rendering, HMD tracking, culling correctness, 6DOF or headset
presentation.

Run `20260915T150554Z-7e0d7da45949` passed the complete gate. The user observed an
obvious change in the game view with FOV `110`, yaw `20` and pitch `-10`. Run-bound
telemetry recorded the same command, `camera_probe_fov_applied`,
`camera_probe_orientation_applied`, `renderer_camera_match=true` and `restored=true`.
Disabling the command returned the probe to natural passthrough; normal exit restored
both camera-vtable slots and emitted the bound `run_end`.

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

The inspected CoJ camera basis uses natural right = `cross(up, forward)` and local
`+forward`. The adapter maps the neutral XR basis into that local basis by reflecting the
XR Z coordinate (`-Z` physical forward -> CoJ `+forward`) and composing the resulting
right/up/forward vectors onto the natural game camera. Host tests verify identity/recenter,
approximately `+20°` physical yaw -> `+20°` camera yaw, pitch direction/scale, roll basis
preservation, return to the physical origin without accumulation, invalid-pose passthrough
and immediate disable passthrough.

The application point is unchanged from the live camera probe: snapshot the natural
`CBaseCamera` basis, call the original render-camera path, apply the relative HMD basis
inside the hooked FOV/frustum boundary immediately before matrix construction, then
restore the natural basis as soon as the original render-camera update returns. HMD
tracking therefore does not own or persistently mutate the game camera.

`d3d9_hmd_camera.dll` uses D3D9 only for load/bootstrap and system `Direct3DCreate9`
forwarding. It installs no `Present`, `BeginScene`, `EndScene` or `Reset` hooks. OpenVR is
used for this gate because its existing x86 pose path is already live-tested with PSVR2;
OpenXR remains substitutable behind `PoseSource` after its existing lifetime/semantic debt
is resolved.

The HMD candidate is **host-tested**, not live-tested. The next manual gate must prove
continuous monitor-camera tracking from a physical HMD, correct yaw/pitch direction and
scale, no integration drift, disable-to-natural passthrough, clean hook/runtime shutdown
and `renderer_camera_match=true`. Stereo rendering and headset presentation remain outside
this gate.
