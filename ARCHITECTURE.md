# Architecture

## Product boundary

Call of Juarez VR is intended to be one user-facing project with shared VR policy and multiple integration backends. A common installer/bootstrap may eventually select the correct backend from the detected game and exact executable build.

Call of Juarez (2006) is the reference implementation. Reuse is evidence-driven: a Chrome Engine behavior is not promoted to a shared contract until at least one additional game demonstrates the same boundary.

## Layers

1. **Runtime** — renderer/game-independent poses, eye/view data, tracking space, configuration, logical input, haptics and VR runtime lifecycle.
2. **Renderer backend** — D3D9 or D3D10 device/frame boundaries, image transport, eye targets and presentation/submission.
3. **Game backend** — camera, player, weapon, UI, physics and exact-build knowledge.

This mirrors the useful separation proven in Penumbra VR while keeping Chrome Engine-specific behavior local to this project.

## Integration matrix

| Game | Engine evidence | Renderer path | Initial integration |
| --- | --- | --- | --- |
| Call of Juarez | Chrome Engine 3 | D3D9 + D3D10 | D3D9 first; D3D10 retained as a first-class backend |
| Bound in Blood | Chrome Engine 4 | D3D9 | shared D3D9 layer + game adapter once contracts are proven |
| Gunslinger | later Chrome Engine branch | D3D9 | shared D3D9 layer + game adapter once contracts are proven |

## Runtime boundary

`src/runtime` must not contain Chrome Engine addresses, native object layouts, camera offsets or weapon assumptions. Those belong to game backends and must be supported by build-specific evidence.

The renderer-neutral types include `Pose`, `EyeFov` and `EyeView`. VR API adapters translate OpenVR/OpenXR data into these types rather than leaking API-specific headers through every game/backend boundary.

## Build identity

Filename matching is diagnostic only. `build_catalog` records exact SHA-256 identities for inspected builds. Game-specific modifications must fail closed on unknown hashes while renderer/runtime diagnostics may continue only where they are safe and build-independent.

## D3D9 renderer path

D3D9 is the first integration target because all three inspected games expose a D3D9 path. Call of Juarez additionally has a D3D10 renderer with visible graphics improvements, so D3D10 remains a planned first-class backend rather than a disposable fallback.

The initial renderer gate was deliberately small:

`normal game -> forwarding bootstrap -> unchanged rendering -> diagnostics`

The forwarding proxy and native-vtable observation path are live-tested on the exact Call of Juarez D3D9 build.

### Transport evidence

Host interoperability testing established the current transport boundary:

- classic `Direct3DCreate9` cannot create the shared render target required for direct D3D9 -> D3D11 sharing on the development host;
- D3D9Ex -> D3D11 shared-resource transport works synthetically with pixel verification;
- substituting the live game device with D3D9Ex passed host tests but failed the exact-build live test and is therefore rejected as the current game path;
- classic D3D9 `GetRenderTargetData` -> system-memory readback -> D3D11 upload is slower but preserves the original device semantics and is live-tested in Call of Juarez.

The sustained diagnostic uses that conservative path:

`D3D9 backbuffer -> CPU readback -> D3D11 texture -> OpenVR -> SteamVR`

The implementation is packaged as `d3d9_openvr_flat.dll`. It submits the same captured flat game image to both eyes. It is a renderer/runtime transport proof only; it does not yet implement stereo cameras or HMD-driven view transforms.

### Current interception problem

The transport and compositor calls are not the active blocker. Exact-build live evidence has shown that the flat bridge successfully completes D3D9 readback, D3D11 upload, HMD pose wait and OpenVR submission for the first three frames.

After exactly three `Present`, `BeginScene` and `EndScene` callbacks, the D3D9 device-vtable entries installed by the project are overwritten while the game continues rendering normally on the monitor. This means the VR path loses interception rather than stalling inside `Present`, readback, upload or compositor submission.

The active diagnostic observes `Reset`, `Present`, `BeginScene` and `EndScene` individually and resolves each replacement function address to the loaded module that owns it. The next architecture decision depends on that evidence:

- if a known overlay/hook module replaces the entries, either remove that interference for validation or chain after it;
- if the targets resolve back to D3D9/engine-owned code, investigate the engine/runtime transition that restores or replaces the native vtable;
- if vtable ownership remains inherently unstable, prefer an owned `IDirect3DDevice9` forwarding wrapper over repeated re-hooking of a shared/native vtable.

Do not move on to camera, stereo or motion-controller work until sustained frame interception is demonstrated.

## OpenVR direction

OpenVR -> SteamVR is the initial VR runtime path. It reuses game-neutral semantics proven elsewhere: runtime lifecycle, standing tracking space, recommended eye size, per-eye projection, eye-to-head transforms, compositor poses and later logical controller actions/haptics.

The pinned dependency is Valve OpenVR SDK 2.15.6. `OpenVrRuntime` translates OpenVR eye and HMD data into renderer-neutral runtime types. The D3D11 backend creates a device on the runtime-selected adapter and submits ordinary `ID3D11Texture2D` eye images through `IVRCompositor::Submit`.

The isolated OpenVR path is live-tested against manually started SteamVR with PSVR2: runtime initialization, eye configuration, valid HMD pose acquisition and synthetic D3D11 stereo submission have all succeeded. Headset-visible confirmation of the isolated synthetic test remains distinct from those technical submissions.

The in-game flat bridge has also completed real OpenVR submissions from captured game frames; sustained presentation is blocked by the D3D9 hook replacement described above.

## OpenXR direction

The OpenXR backend is retained as an experimental/future path. It already contains instance/system/session lifecycle, frame timing, D3D11 graphics binding, adapter-LUID selection, stereo swapchains and projection layers. A SteamVR run reached valid runtime/session/swapchain creation and one projection-frame submission without headset-visible/focused evidence.

OpenXR is not the critical path for the first Call of Juarez headset proof. Its code remains useful as a future backend and as prior evidence for D3D11 adapter/resource handling.

## Game integration direction

Renderer transport and sustained HMD presentation come before game-camera work. Once the sustained frame gate is complete, the intended progression is:

1. rotational HMD tracking / 3DOF camera proof;
2. stereo per-eye projection;
3. positional tracking and room-scale reconciliation;
4. HUD, cinematics and post-process handling;
5. decoupled weapon/controller interaction where game boundaries permit.

Exact camera/player/weapon knowledge belongs to the Call of Juarez game backend and must not be generalized to Bound in Blood or Gunslinger without independent evidence.

## Primary validation hardware

The primary headset target is PlayStation VR2 on PC through OpenVR -> SteamVR. The primary motion-controller target is the paired PS VR2 Sense controllers. Shared runtime input remains expressed as logical actions so controller-specific bindings do not leak into game or renderer policy.
