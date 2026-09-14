# Architecture

## Product boundary

Call of Juarez VR is intended to be one user-facing project with shared VR policy
and multiple integration backends. A common installer/bootstrap may eventually
select the correct backend from the detected game and exact executable build.

## Layers

1. **Runtime** — renderer/game-independent poses, eye/view data, tracking space,
   configuration, logical input, haptics and VR runtime lifecycle.
2. **Renderer backend** — D3D9 or D3D10 device/frame boundaries, eye targets and
   presentation/submission.
3. **Game backend** — camera, player, weapon, UI, physics and exact-build knowledge.

This mirrors the successful separation used in Penumbra VR while keeping Chrome
Engine details specific to this project.

## Integration matrix

| Game | Engine evidence | Renderer path | Initial integration |
| --- | --- | --- | --- |
| Call of Juarez | Chrome Engine 3 | D3D9 + D3D10 | D3D9 first, D3D10 retained as first-class backend |
| Bound in Blood | Chrome Engine 4 | D3D9 | shared D3D9 layer + game adapter |
| Gunslinger | later Chrome Engine (`ChromeEngineCoJ4`) | D3D9 | shared D3D9 layer + game adapter |

## Runtime boundary

`src/runtime` must not contain Chrome Engine addresses, native object layouts,
camera offsets or weapon assumptions. Those belong to game backends and must be
supported by build-specific evidence.

The first renderer-neutral data types are `Pose`, `EyeFov` and `EyeView`. VR API
adapters translate runtime poses/views into these types rather than leaking OpenVR/OpenXR headers
through every game/backend boundary.

## Build identity

Filename matching is useful for diagnostics but insufficient for binary
integration. `build_catalog` records exact SHA-256 identities for the inspected
Steam builds. Game-specific modifications must fail closed on unknown hashes.

## Renderer direction

D3D9 is the first integration target because all three inspected games expose a
D3D9 path. The first game additionally has a D3D10 renderer with visible graphics
improvements, so D3D10 is planned once the common VR contracts are proven.

The first renderer gate was deliberately small:

`normal game -> forwarding bootstrap -> unchanged rendering -> diagnostics`

The forwarding path and native-vtable `Present` / `Reset` observation are now
live-tested on the exact Call of Juarez D3D9 build. They preserve the native device
object/vptr and COM identity and provide the first proven frame boundary.

Host interoperability testing established an important boundary. A device created
through classic `Direct3DCreate9` returns `D3DERR_INVALIDCALL` when asked for the
shared render target needed by the D3D11 compositor path. The same operation works
through D3D9Ex, including pixel-verified D3D9Ex -> D3D11 sharing on the same adapter.

The proxy also has an opt-in D3D9Ex substitution used only as a diagnostic. It
passes synthetic `Clear`, `Present`, `Reset`, Ex-interface and shared-resource
coverage, but the first exact-build live-test crashed after an observed `Present`.
It is therefore not the primary transport path.

The fallback keeps the game's original classic D3D9 device unchanged: copy the
render target to system memory with `GetRenderTargetData`, then upload the pixels
to a D3D11 texture. This path is slower because it introduces a GPU/CPU
synchronization and copy, but pixel-verified host testing passes and it preserves
the live-tested device semantics. It is suitable for the next narrow proof before
optimizing transport.

The live proof is packaged as a distinct `d3d9_readback.dll` diagnostic proxy. Its
readback path is compile-time enabled in that artifact and absent from the normal
`d3d9.dll` runtime behavior, avoiding hidden environment-variable or marker state.

## OpenVR direction

OpenVR -> SteamVR is the initial VR runtime path. It reuses the proven neutral
semantics from Penumbra VR: runtime lifecycle, standing tracking space,
recommended eye size, per-eye projection, eye-to-head transforms, compositor
poses and later logical controller actions/haptics.

The pinned dependency is Valve OpenVR SDK 2.15.6. `OpenVrRuntime` translates
OpenVR eye and HMD data into the renderer-neutral runtime types. The OpenVR D3D11
backend creates a device on the runtime-selected adapter and submits ordinary
`ID3D11Texture2D` eye images through `IVRCompositor::Submit`.

The isolated `cojvr_openvr_probe` builds in Debug and Release but must only be run
after the user starts SteamVR manually. Its normal mode checks runtime, eye and
D3D11-adapter configuration; `--pose` adds physical tracking and `--submit` adds a
single synthetic stereo submission.

OpenVR scene submission still expects D3D11 textures. The next renderer gate is
proving the classic-D3D9 readback -> D3D11 upload path in the real game without
changing flat rendering or stability. Direct D3D9Ex sharing remains useful as
research evidence but not as a transparent replacement for this game build.

The sustained in-game diagnostic extends that proven boundary without changing
the game's native D3D9 device. Exact-build live evidence showed device `Present`
only once and no implicit-swapchain `Present` callbacks, so neither is a sustained
frame boundary for this path. The current diagnostic captures after successful
`EndScene`, reads the flat backbuffer into system memory, uploads it to a persistent
D3D11 texture on the OpenVR-selected adapter, advances compositor poses and submits
the same texture to both eyes. The `EndScene` hook is host-tested and still needs
the exact-build live/headset gate. It does not provide stereo cameras or
HMD-controlled view. The diagnostic is built as `d3d9_openvr_flat.dll`.

## OpenXR direction

The existing OpenXR backend is retained as an experimental/future path. It already
contains instance/system/session lifecycle, frame timing, LOCAL-space stereo views,
D3D11 graphics binding, adapter-LUID selection, stereo swapchains and projection
layers. A real SteamVR run reached valid runtime/session/swapchain creation and one
projection-frame submission, without headset-visible/focused evidence.

OpenXR is not the critical path for the first Call of Juarez headset proof. Its
code remains useful as a future backend and as prior evidence for D3D11 adapter and
resource handling.
## Primary validation hardware

The primary headset target is PlayStation VR2 on PC through OpenVR -> SteamVR. The primary
motion-controller target is the paired PS VR2 Sense controllers. Shared runtime
input remains expressed as logical actions so PSVR2-specific bindings do not leak
into game or renderer policy.
