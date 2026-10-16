# Call of Juarez stereo render resolution

## Established boundary

The active D3D9/OpenVR path renders two native views, but does not allocate the
game's scene targets from the headset recommendation. There is no fixed 1024x768
limit in the mod. Scene detail is bounded by the engine's actual color targets
and raster viewport; the capture and native-stereo presenter retain that extent.
This is source-established behavior, not independent-resolution implementation
or new headset acceptance. The exact engine identity and memory findings remain
in [the resource census](COJ_D3D9_RESOURCE_CENSUS.md).

| Stage | Dimension decision and owner | Source boundary |
| --- | --- | --- |
| Game configuration | Native video settings select `Resolution(w,h)` in Documents `call of juarez/out/Settings/Video.scr`. The deployment helper preserves it and journals/restores the original bytes; it changes FSAA to 0. | `tools/coj_video_profile.ps1`, `release/cojvr.ps1` |
| Device presentation | ChromeEngine supplies `D3DPRESENT_PARAMETERS`. `CreateExDevice` passes these to `CreateDeviceEx`, with matching fullscreen dimensions. VR present policy changes cadence/interval, not width/height. Requested backbuffer values alone are not actual RT0 measurements. | `backends/d3d9/factory_vtable_hook.cpp`, `vr_present_policy.hpp` |
| Native scene | ChromeEngine owns color/depth allocation, intermediate targets, binding and viewport. The mod does not substitute a higher-resolution native scene family. RT0 at eye completion can be the backbuffer or another surface; its identity must be observed. | `games/call_of_juarez/camera_probe.cpp`, `backends/d3d9/depth_observation.hpp` |
| Eye rendering | `HookRenderViewImpl` and the passive movie/settings route execute full wrapper `0x30FB0` per eye, applying runtime eye transforms and asymmetric projection, then capturing before restoring camera state. The second eye transaction temporarily clears/restores the native rendered guard. No target-size or viewport override accompanies it. | `camera_probe.cpp`: `ReplayFullRenderViewForStereoEye`, `HookSetFov`, `RenderCameraOnlyStereo` |
| Capture | `CaptureEye` obtains bound RT0. `EnsureResources` calls its `GetDesc` and allocates a three-slot, two-eye ring of that width/height/format. `StretchRect` copies whole surfaces with `D3DTEXF_NONE`; the capture is not a larger scene render. MSAA is resolved into non-MSAA capture surfaces. | `backends/d3d9/d3d9_stereo_capture.cpp` |
| Shared transport | D3D9Ex creates shared DEFAULT render-target textures. D3D11 opens those allocations, checks equal copy descriptions and uses `CopyResource` into presenter-owned eyes. Opening a shared handle is not another pixel allocation. Classic fallback retains the same source size through SYSTEMMEM/readback. | `backends/openvr/d3d9_shared_texture_bridge.cpp`, `d3d9_stereo_capture.cpp` |
| Presenter/compositor | Native stereo `EnsureTextures` uses `FrameWidth/FrameHeight`, not recommended eye size. `SubmitEyeD3D11WithPose` submits those D3D11 textures with the captured HMD pose and full texture bounds. There is no native-stereo resolution multiplier. | `backends/openvr/stereo_presenter.cpp`, `d3d11_compositor.cpp` |
| Runtime recommendation | `OpenVrRuntime::Initialize` reads `GetRecommendedRenderTargetSize`; `ReadEyeConfiguration` copies the cached values into `EyeView`. Optics and eye transforms affect camera projection, independently of the engine's allocation extent. The recommendation is read at initialization, not refreshed for every frame. | `runtime/openvr_runtime.cpp` |

Source paths above are relative to `src/` except the named deployment scripts.
Valve describes the [recommended offscreen extent](https://github.com/ValveSoftware/openvr/wiki/IVRSystem::GetRecommendedRenderTargetSize)
as accounting for projection/distortion, rather than simply panel pixel count.

Flat theater has a different sizing rule: `ComputeFlatTheaterTextureExtent`
fits the captured desktop image onto the finite-depth screen using each eye's
optics. Its output may exceed the source size to accommodate that geometry.
This does not increase the source scene detail or constitute VR supersampling.

## Increasing the native game resolution

A higher selected mode can improve the captured scene if ChromeEngine actually
creates and rasterizes larger scene targets/viewports. The existing installer
does not force a low resolution. It also does not prove the native mode change
succeeded. Native video-menu application can recreate resources/reset the device;
arbitrary in-session VR mode changes remain unqualified.

Static inspection of the census-identified `ChromeEngine3.dll` establishes the
native configuration side more precisely (addresses below are RVAs):

- `0x1D7170` parses `Resolution` into current/proposed width and height pairs.
- JNI registration at `0x1D90A0` binds `GameObject.SetResolution(II)V` to
  `0x1D81B0`; this setter writes proposed width/height at settings `+8/+0xC`.
  Selection alone does not allocate a new target. `SetResolutionMode(II)V`
  at `0x1D81D0` instead resolves a mode through `0x1D7B30`.
- `ApplyVideoSettings` at `0x1D9080` calls settings application `0x1D8A00`.
  Changed dimensions/BPP/fullscreen/FSAA/refresh trigger a renderer virtual call
  at slot `+8` with the proposed mode. Failure restores old settings and retries
  that renderer entry; failure of both reaches the native fatal-exit path.
  Success commits proposed dimensions; `0x6830` publishes the committed size to
  the native dimension cache and conditionally rebuilds its dependent state.
  This is a renderer reinitialization request, not an eye-only extent API.

These static observations identify the ordinary game's settings transaction,
not the complete renderer allocation graph or live success of a selected mode.
They do not authorize calling those setters during eye rendering. The renderer's
indirect reinitialization entry and its target/effect lifetime still need focused
qualification before any independent-size writer is added.

The reversible workaround is to close the game, uninstall/recover through the
launcher, select a higher mode in the unmodded D3D9 game, quit and reinstall.
DSR/custom modes are conditional candidates: enabling a driver option alone
does not prove the game selected the mode or rendered a larger source. Compare
the actual RT0/viewport before and after, not just the desktop or SteamVR slider.
No DSR/headset combination is accepted by this investigation.

The native function that rebuilds the complete scene target family and its
resolution-dependent effects is not yet qualified as a safe independent-size
writer. Passing larger presentation parameters would change desktop/native
presentation too; it is an intermediate workaround, not decoupled eye rendering.

## Existing diagnostics and their limits

Reuse the bounded diagnostics before adding another observer:

- `native_stereo_eye_optics`: cached runtime recommendation, FOV and eye offset.
- Device creation/reset events: requested presentation parameters, device and
  generation. Zero requested windowed dimensions need actual surface readback.
- `native_stereo_capture` / sampled `native_stereo_capture_timing`: actual
  `eye_surface`, viewport, format, MSAA and `source_is_backbuffer` for each eye.
  Resource-generation events retain the same capture description.
- `manual_reload_depth`: existing default-off, bounded full-eye target/depth
  descriptors, identities, viewport and matrices. It is tied to eligible manual
  pickup observations, not a general campaign census. Matching metadata does
  not establish depth content, ownership throughout the pass or hand coverage.

The presenter and bridge's size checks establish copy equality by source
inspection. Native-stereo submission does not independently log every output
texture descriptor. A future diagnostic can report the verified actual output
descriptor once per resource generation if needed, without per-frame spam.
Retained local logs are corroboration of their recorded executions only; they
do not qualify another machine or a new physical gate.

## Independent resolution: intervention and prerequisites

The intervention belongs **inside native scene resource selection, before each
full render-view pass**, with the game adapter identifying the exact scene family
and the D3D9 backend owning replacement resources/state. Changing only
`OpenVrStereoPresenter::Impl::EnsureTextures`, capture ring size, camera FOV or a final blit
cannot produce higher-resolution geometry. Binding a bigger RT immediately
outside `original(owner, view)` is also insufficient proof: the wrapper/core can
rebind its own targets or use cached dimensions internally.

Qualify the color/depth/intermediate allocation and bind graph through both full
eye passes first. The current creation-pool census covers observed resource
semantics, not complete target lifetimes, postprocessing dimensions or residency.
Do not resize every texture, shadow map or native sprite target globally.

Required contracts before enabling an independent-size candidate:

- Color, depth/MSAA and resolution-dependent intermediates must agree across
  clears, scene rendering and postprocessing. Preserve the asymmetric runtime
  frusta and near/far conventions; do not derive headset projection from desktop
  aspect ratio. Check shader texel offsets, screen quads and cached screen sizes.
- Explicitly save/restore all affected render targets, depth, viewport, scissor,
  transforms and changed states per eye, including failure paths. D3D9
  [SetRenderTarget resets the viewport](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setrendertarget),
  so restoration order matters. Depth must satisfy the
  [format/MSAA/extent constraints](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setdepthstencilsurface).
- Keep naturally scheduled native sprites/HUD out of replay. Text/reticle
  composition already targets presenter eye dimensions, but fixed-pixel marks,
  subtitle readability, native sprites and pointer source coordinates need
  explicit size/aspect checks. Complete native graphical HUD and immersive movie
  subtitle transport remain separate gates.
- Preserve shared format/layout checks, producer fences, consumer leases and
  device/generation identity. A size change must retire outstanding work before
  rebuilding resources; it cannot overwrite a leased image. Reset/ResetEx,
  failed reset, replacement and classic lost-device semantics require separate
  tests. Existing host recovery tests do not establish new target ownership.
- Keep desktop/native Present and theater resources independent. Mode changes
  must discard old captures and require a fresh complete pair, retaining the
  existing freshness/ownership fences and native fallback. A partially resized
  stereo family must never escape to the compositor.
- Reject unsupported device caps, invalid/overflowing extents and allocation
  failure without partial native mutation. Start with restart-only size changes;
  live resizing is a later contract, not a requirement of the first prototype.

Memory cost grows with pixel count. With four-byte color, the existing six
producer textures plus two presenter textures alone cost approximately `32*w*h`
bytes (about 275 MiB at 3000x3000 per eye), excluding native color/depth/effects,
driver/compositor overhead and old resources awaiting retirement. The separate
three-slot mono theater reservation costs approximately `12*desktop_w*desktop_h`
bytes of SYSTEMMEM. Classic stereo fallback adds six SYSTEMMEM eye surfaces.
These are layout estimates, not measured residency or process VA usage. Doubling
both dimensions quadruples these costs and raster work; total frame time also
contains CPU/native and compositor costs.

The original process is x86. The existing reversible exact-hash LAA derivative
raises user address capacity on 64-bit Windows; it does not add VRAM, remove
fragmentation, or guarantee JVM/native high-address safety. Existing native
allocation failures in the census make VA/commit/largest-free-region and frame
age measurements prerequisites to choosing a default scale. Do not compensate
for pressure with dimension-specific allocation retries.

## Incremental development and verifiable exits

1. **Qualify engine ownership without changing rendering.** Identify exact
   creation/bind/resize owners and all full-eye intermediates; use existing
   observations and add bounded missing identity data only when necessary. Exit:
   a coherent same-frame, same-device/generation allocation/bind map for both
   eyes, plus a clear desktop/theater boundary. This remains research, not a
   headset-resolution feature.
2. **Prove a scoped fixed-size prototype.** Retain full wrapper and transport;
   initially use one conservative restart-only extent. Host GPU tests must prove
   color/depth/viewport setup and exact restoration, independent eye patterns,
   mismatched layouts, partial allocation failure and outstanding lease rejection.
   Exact-game evidence must then prove new geometry detail in both eyes, correct
   postprocessing and unchanged desktop/theater ownership. Larger destination
   textures or interpolated screenshots alone fail this criterion.
3. **Add policy only after that proof.** A runtime-neutral requested scale can
   derive pixel dimensions from the recommendation with finite bounds, checked
   arithmetic, caps and a measured memory budget. Define scale as a dimension
   multiplier (`s*w`, `s*h`, pixel count proportional to `s*s`), distinct from any
   SteamVR percentage. Reuse the shared preferences path; expose only the
   implemented restart/apply behavior, never an inert slider.
4. **Consolidate physical acceptance.** Batch sharpness, coherent eyes, HUD/movie
   transitions, recovery and sustained performance into a relevant candidate
   under `prepare`/`finish`. Do not reopen already accepted camera/hand offsets or
   claim support from host tests. Live resizing follows only after retirement
   and Reset/device-loss contracts are independently demonstrated.

Post-capture enlargement is a different operation. It may change filtering or
presentation size, but cannot recover geometry/texture samples absent from the
original eye render and must never be presented as real render supersampling.
