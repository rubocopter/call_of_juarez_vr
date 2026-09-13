# Architecture

## Product boundary

Call of Juarez VR is intended to be one user-facing project with shared VR policy
and multiple integration backends. A common installer/bootstrap may eventually
select the correct backend from the detected game and exact executable build.

## Layers

1. **Runtime** — renderer/game-independent poses, eye/view data, tracking space,
   configuration, logical input, haptics and future OpenXR lifecycle.
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

The first renderer-neutral data types are `Pose`, `EyeFov` and `EyeView`. OpenXR
will translate runtime poses/views into these types rather than leaking OpenXR
headers through every game/backend boundary.

## Build identity

Filename matching is useful for diagnostics but insufficient for binary
integration. `build_catalog` records exact SHA-256 identities for the inspected
Steam builds. Game-specific modifications must fail closed on unknown hashes.

## Renderer direction

D3D9 is the first integration target because all three inspected games expose a
D3D9 path. The first game additionally has a D3D10 renderer with visible graphics
improvements, so D3D10 is planned once the common VR contracts are proven.

The first renderer gate is deliberately small:

`normal game -> forwarding bootstrap -> unchanged rendering -> diagnostics`

Only after that path is live-tested should OpenXR/session creation, eye targets,
stereo rendering or camera overrides be enabled.

## OpenXR direction

OpenXR is the intended VR API for this project. Runtime ownership should include
instance/system/session lifecycle, spaces, predicted display timing, poses and
swapchains. Renderer backends should own the graphics binding and texture
submission details.

## Upscaling and antialiasing

DLSS/DLAA is tracked separately from basic VR enablement. Temporal reconstruction
needs suitable color, depth, motion-vector and camera/jitter inputs for each eye.
The original D3D9/D3D10 renderers do not by themselves provide a direct modern
DLSS integration surface. See `docs/DLSS_DLAA.md`.
