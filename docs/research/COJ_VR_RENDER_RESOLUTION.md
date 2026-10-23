# Call of Juarez stereo render resolution

## Established boundary

The active D3D9/OpenVR path renders two native views, but does not allocate the
game's scene targets from the headset recommendation. There is no fixed 1024x768
limit in the mod. Scene detail is bounded by the engine's actual color targets
and raster viewport; the capture and native-stereo presenter retain that extent.
This is source-established behavior, not independent-resolution implementation
or new headset acceptance. The exact engine identity and memory findings remain
in [the resource census](COJ_D3D9_RESOURCE_CENSUS.md).

The [approved independent-resolution design](COJ_VR_INDEPENDENT_RESOLUTION_DESIGN.md)
defines fixed-startup experiments, optical/resource ownership, memory accounting,
rollback and acceptance, with autonomous progression through its prerequisites.
Its first gate still requires the complete native target/effect map;
the source trace below does not qualify an independent-size writer.

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

The table describes the ordinary active path. A separate default-off private-bank
prototype is implemented/host-tested; its first exact-game B0 trial was rejected
and B1 remains blocked. Its existence does not qualify independent scene sizing
or change the source-sized production contract above. The
[design](COJ_VR_INDEPENDENT_RESOLUTION_DESIGN.md) owns that experimental boundary.

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
- The retained exact-build decompilation of `0x1D8A00` also calls `0x224EC0`,
  `0x225690` and `0x224F90` in the postprocess rebuild branch. Creation failure
  can disable postprocessing and a later failure reaches the native fatal path.
  These call sites guide resource-owner research; their full allocation graph,
  signatures and safe offscreen scope are not qualified. They are not an approved
  per-eye construction/destruction API.

These static observations identify the ordinary game's settings transaction,
not the complete renderer allocation graph or live success of a selected mode.
They do not authorize calling those setters during eye rendering. The renderer's
indirect reinitialization entry and its target/effect lifetime still need focused
qualification before any independent-size writer is added.

Focused follow-up now resolves the responsibilities of the native PP buffer
allocator, alias-aware release, effect references and renderer-owned internal
color/depth/default-target caches. The exact RVAs, descriptor layout, rounding
and partial recipe budget live in [the PP target ownership map](COJ_PPFX_TARGET_OWNERSHIP.md).
Creation releases the old family first and is not a safe per-eye transaction;
FSAA 0 still permits the native internal color/copy path. This is static progress,
not live allocation qualification. Native/COM identity correlation, programmable
constants/history, desktop sprite ordering and retirement still block Q0.

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
- `render_resource_probe`: startup-only bounded full-wrapper binding/draw census,
  with live gameplay evidence. Collection completed without read failure or
  truncation; this does not qualify allocation/lifetime or every presentation
  route. The operator reports preserved exercised functionality at the original
  size with occasional stutters. Instrumented pacing cannot qualify performance.

## Native target-family finding from the gameplay census

The eye wrapper uses multiple color formats and intermediate extents rather than
one interchangeable RT0. In particular, a native pass binds a FourCC `NULL`
color surface with a viewport larger than that color descriptor and a larger
bound depth surface. The observed native draw succeeds. An RT0-only containment
rule initially rejected this combination. NVIDIA documents `NULL` as a color
placeholder for passes that do not need color storage, including shadow depth
rendering; see its [GPU Programming Guide, section 6.4.1](https://developer.download.nvidia.com/GPU_Programming_Guide/GPU_Programming_Guide_G80.pdf#page=57).
That API description explains the placeholder, not this game's precise effect.

The analyzer now classifies the observed one-pixel `NULL` / D16 combination as
`null_color_depth`: no additional bound MRT, DEFAULT surface resources, native
RT/depth usage, no multisampling and depth test/write enabled. It checks viewport
containment against depth and reports both color and raster extents. Other
layouts remain unqualified; ordinary color targets still use their own bounds.
`drawPasses` separates these classes and counts the actual recorded draw repeats
within each eye scope, without assigning effect names from size alone. Existing
capture data now completes this bounded analysis. This does not establish shadow
contents, allocation lifetime, a one-pixel scene or a resizing-safe family.

Classify scene, depth-oriented, reduction/filtering and other effect resources
by their actual pass/format/alias relationships before selecting any family for
replacement. The captured ordinary color passes include the selected scene
extent and multiple smaller intermediates. Neither a color descriptor alone nor
uniform scaling of every bound texture defines the eye-resolution contract.
Use the retained complete trace for further offline classification; no repeat of
the same capture is required for this resolved containment assumption.
Settings/movie coverage, programmable constants, creation,
reuse/destruction and compositor retirement remain separate unresolved gates.

The presenter and bridge's size checks establish copy equality by source
inspection. Sampled native capture now reports the actual destination surface
descriptor (`capture_surface`); sampled presenter uploads read both D3D11 output
descriptors (`left_texture`, `right_texture`) alongside the source extent.
Retained local logs are corroboration of their recorded executions only; they
do not qualify another machine or a new physical gate.

## Explicit native-resolution trial

**Current state:** the ordinary 2804x2860 route is headset-validated in the
exercised PS VR2 setup and is the operator's chosen local Manual default. Keep
that acceptance; no identical repeat trial or new Ray-fit test is pending.
Other profiles and independent rendering retain their own unqualified gates.

`tools/vr_test.ps1 prepare -BodyIkAtStart -LargeAddressAware -ManualReload
-NativeResolution 2804x2860` requests that extent through the game's existing
`Video.scr` allocation path, using windowed presentation and FSAA 0. This is an
explicit exception to preserving the selected video size: menus and theater also
use the changed game source extent. It never changes the OS desktop display mode,
introduces an upscaler, enables the rejected independent bank, or changes HMD
optics. The setting is a request until observed in the actual game. Window/client
clamping or engine fallback must reject the resolution gate, not be called success.

The ordinary native stereo passes still run the full wrapper with asymmetric HMD
projections. Capture copies the full RT0 to equal-sized eye surfaces with
`D3DTEXF_NONE`; the shared D3D11 bridge rejects unequal descriptions before
`CopyResource`. Both output descriptors are measured, not inferred from window,
mirror or SteamVR recommendation. The qualifying trial used the bounded read-only
resource probe to record native draws, color/depth extents, viewport and scissor.
It is off in the accepted ordinary candidate; retain it for dimension qualification.
Native lower-resolution shadow/filter passes are retained. A full-size depth-tested
and depth-writing draw is binding evidence, not proof of shader content or visible
extra detail; new dimensions or independent-rendering candidates still require
their own headset comparison. The accepted local size needs no repeat.

`analyze_coj_native_resolution.py` requires a complete run/build interval,
complete gameplay eye scopes with full-size color/depth/viewport and uncut scissor,
matching per-eye capture source/destination sizes, shared transport, and a sampled
pair uploaded to distinct full-size D3D11 eyes and submitted successfully with its
matching explicit pose. Geometry and submitted-pair samples are reported separately;
they need not be the same frame. Missing, smaller, incomplete or failed data rejects.
`finish` embeds the per-eye measurements and rejection reasons under
`renderResolution` in the inventoried native-stereo summary. This resolution gate
is separate from the general continuity checks and operator headset acceptance.

The requested extent is 8,019,440 pixels per eye (2.1754 times 2560x1440); one
uncompressed four-byte surface is about 30.6 MiB. The six capture-ring textures
alone hold about 183.6 MiB of pixel payload, before engine scene/depth/effects,
D3D11 outputs, theater/CPU buffers, alignment, driver duplication or fragmentation.
LAA does not guarantee allocation success, stable pacing or proportional timings.
Exclude the bounded probe frames when judging sustained performance. Compare the
same save/view, SteamVR scale/refresh and recording state against the baseline.

The video journal retains the exact original bytes, resolution and fullscreen
setting before mutation. Failed preparation and `finish` restore them through
the existing recovery path. Default preparation still preserves the selected size.
The published .1 archive is preserved; .2 packages this ordinary native path
and scoped acceptance. The independent-resolution
prototype remains rejected and default-off under its separate design gates.

The completed ordinary-path trial observes native per-eye HDR color (format 113,
A16B16G16R16F), depth (77, D24X8) and viewport at 2804x2860. Final D3D9 color and
capture surfaces are also 2804x2860 (format 21, A8R8G8B8); measured D3D11 eye
outputs retain that extent, with successful matched explicit-pose submissions.
This establishes native raster/capture/output dimensions for the exercised run;
lower-resolution shadow/filter passes and shader content remain separate facts.
The operator reports a correct image and no noticeable poor performance, rather
than a quantified GPU-time or memory limit. The canonical general verifier's
unobserved blocking-UI trigger release remains a separate rejected requirement.

The main-menu correction is presentation-only. Read-only `FindUI` resolution and
`IsInstanceOf(MenuMain)` require the existing global current UI, followed by
successful `IsActuallyVisible()` and valid `GetThisID()` observation. Active-game
menus, loading, movies, modals, hidden objects and missing/failed observations
do not admit it. These use the same existing UI inheritance/read-only getters as
the established native pointer route; no UI is created or input dispatched.

The previous per-axis padding formula is superseded: it changed output sizes and
could force the captured image into the old reference aspect. Compute instead the
old 2560x1440 angular bounding box using the current optical recommendation, then
fit the current captured image uniformly in plane space, retaining its aspect.
Account for FOV tangent spans and the existing output canvas dimensions when
converting the visible rectangle to output pixels. Keep the current asymmetric
eye centers and finite plane distance. The output allocation key remains actual
source extent/format and existing presentation mode; reference/owner changes
never resize or recreate render targets. Clear old content on geometry changes.

Full-image CPU bilinear sampling on the submission thread is rejected. A full-size
Win32 reproduction exceeded compositor cadence, matching the operator's severe
main-menu pacing/flicker and immediate submenu recovery. Its cost preceded the old
upload timer and was therefore missing from that metric; retained source/target
identities do not support continuous render-target churn as the cause. Small-pixel
correctness tests alone cannot qualify presentation performance.

The replacement uploads the unchanged original mono capture once to a private
D3D11 source texture with SHADER_RESOURCE binding only, then linearly samples a
quad into each existing output target/visible rectangle. Source-coordinate beam
and reticle composition run in that pixel shader. This is 2D menu presentation,
never world-render upscaling. It adds one four-byte-per-pixel source texture, about
30.6 MiB of payload at this trial size, plus shaders/states; driver copies and VA
remain separate. Source allocation depends only on actual extent/device, not
angular geometry. Output views are scoped per call; no output target is created
or retained by the menu helper. Source resources are retired before presenter
shutdown. Context/device, both output layouts and views are validated before writes.

Input intersections use the same visible rectangle and map back to original
capture pixels; hover/click dispatch and physical-mouse priority stay native.
Reference/native-identity changes cancel pending claims and retain release barriers.
A failed menu layout/prepare/draw latches the adjustment off for the session, clears
its geometry/owner metadata and retains ordinary full-size UI; there is no repeated
allocation retry. Generation/capture/recovery ownership is unchanged. Sampled
`native_stereo_menu_gpu` records include CPU queue time and source allocations;
CPU queue time is not GPU duration. WARP tests cover pixels/black borders, both
placements, source-coordinate pointer, source preservation, geometry reuse,
foreign context rejection and bounds. Actual-size hardware enqueue checks are
host evidence only. The operator accepts the exercised menu pacing correction;
the separate intermittent flash cause remains open. A deformation already inside
the captured layout is separate; fitting cannot repair it. The independent bank
stays off, and unknown routes retain their original presentation.

The operator confirms that the GPU menu replacement removes the severe main-menu
slowdown. Earlier full-scene white flashes were visible on the monitor and during
native gameplay; subsequent exercised sessions are symptom-free and sampled data
contains no detected white jump. Their intermittent cause remains unresolved.
Successful copies/submissions do not identify
the pixel-content fault. Do not disable tone mapping, bloom or change image values
as a presumed fix without evidence. A separate startup-only `FrameLumaProbe`
diagnostic observes a fixed 16x16 point grid at native left/right RT0 capture and
desktop pre-Present, retaining source/pose/generation/time identity. It uses three
private tiny GPU/SYSTEMMEM pairs per stream, event polls without flush/spin and
readback only after completion. Sparse records retain the preceding sample around
large RGB/white-fraction changes. Native source pixels, render state, resolution
and effects are untouched; resources release before attempted Reset/ResetEx,
device replacement and shutdown. Unsupported sampling disables only that stream.
Raw RGB averages are not photometric luminance or proof of a defect: authored
white transitions remain possible. Point coverage, missed samples and readback
overhead remain explicit; no observation proves absence of flashes or performance
acceptance. The binding census remains off by default in this content diagnostic.

The [startup profile implementation](COJ_VR_RELEASE_RESOLUTION_PROFILES.md) provides
saved launcher Auto/75%/100%/125%/Manual choices through those native seams, with
host-tested bounds, layered recovery and isolated exact-game install coverage.
It does not qualify other dimensions, automatic defaults or this local size as
universal. The .2 consolidation retains these experimental limits and the
operator's scoped Manual 2804x2860 acceptance; the published .1 archive is unchanged.

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
