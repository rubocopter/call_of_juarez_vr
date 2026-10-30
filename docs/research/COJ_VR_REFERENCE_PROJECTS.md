# VR reference projects and CoJ application

These are design and engineering references, reviewed against primary project
sources on 2026-10-21, with the FarCry2VR rendering review added on 2026-10-24.
They establish hypotheses and useful patterns, not Chrome
Engine contracts or CoJ headset acceptance. No implementation from these
projects was copied into this candidate. Check the exact file licence and
attribution before any future code reuse.

## Sources and transferable conclusions

| Project and primary evidence | What it contributes | CoJ application and boundary |
| --- | --- | --- |
| [Penumbra VR Framework](https://github.com/rubocopter/penumbra_vr_framework), [architecture](https://github.com/rubocopter/penumbra_vr_framework/blob/main/docs/ARCHITECTURE.md), `src/runtime/vr_tracking_space.hpp`, `vr_haptics.hpp` (also inspected in the local checkout) | Metre-space tracking/recenter, logical input/haptic policy, distinct evidence stages, shared policy separated from exact native adapters. | Continue these existing ownership boundaries. CoJ alone converts centimetres, owns weapon/native identities and scoped mutations. HPL addresses, layouts and callbacks are inapplicable; no renderer/backend migration follows. |
| [CyberpunkVR Port two-hand grip](https://github.com/dariulone/cyberpunk-vr-port/blob/main/src/Anim/TwoHandGrip.cpp) and [revolver module](https://github.com/dariulone/cyberpunk-vr-port/blob/main/mods/cet/CyberpunkVRPort_Reload/reload/revolver.lua) | Captured native poses, separate grip/reload profiles and a gradual transition into two-hand control. | Use native displayed finger/support frames as measurement sources. CoJ already captures a natural support socket and maps one rigid weapon. A future optional blend must preserve same-owner muzzle/ray alignment and release barriers. REDengine reload/cylinder control is not evidence for Chrome; do not import inertia, tuning constants or mechanical writers. |
| [Godot XR Tools hand grab points](https://github.com/GodotVR/godot-xr-tools/blob/master/addons/godot-xr-tools/objects/grab_points/grab_point_hand.gd), [pickable](https://github.com/GodotVR/godot-xr-tools/blob/master/addons/godot-xr-tools/objects/pickable.gd) | Explicit primary/secondary hand roles, per-point poses and constrained acquisition. | Keep acquisition, retention and release as separate policies; bind future reload sockets to measured weapon-local frames and phase. Current CoJ crossed-support rejection is a geometry safeguard; visual fit and a real loading port remain unqualified. Reuse semantics rather than Godot dependencies or collision behaviour. |
| [NomaiVR input prompts](https://github.com/Raicuparta/nomai-vr/blob/master/NomaiVR/UI/InputPrompts.cs), [gesture prompts](https://github.com/Raicuparta/nomai-vr/blob/master/NomaiVR/UI/GesturePrompts.cs) | Action/device-aware button images and cache invalidation on binding changes. | CoJ now has host-tested OpenVR-origin text labels and brief reload coaching, with generation invalidation and semantic fallback. Device-aware graphical glyphs remain future work; metadata/text tests do not qualify other controllers or physical readability. Avoid a permanent aiming ring. |
| [Half-Life 2 VR: Unleashed](https://github.com/vittorioromeo/HL2VRU) | Documented virtual stock and grip-holster interactions, including priority for hand interactions. | Compare support-grip comfort and input arbitration. CoJ already consumes grip claims before Action/Kick/holster dispatch. Virtual shoulder stabilization remains optional future work after rigid grip acceptance. The consulted public repository exposes documentation, not the fork's implementation; this is a UX reference. |
| [Half-Life 2: VR Mod reloading manual](https://halflife2vr.com/manual/#reloading), [FAQ](https://halflife2vr.com/faq/) | Each firearm has weapon-specific manual steps and optional quick reload. Shotgun pump, pistol slide and magazine exchange differ; two-hand use also depends on the weapon. | Reuse the separation between ammunition transfer, required follow-up action and support grip as interaction policy. For CoJ, Schofield clip, Winchester feed, Remington barrels and Lemant indices must each preserve their original native accounting and animations. The reference documents UX, not transferable engine code or physical acceptance. |
| [Half-Life: VR Mod release notes](https://github.com/maxmakesmods/Half-Life-VR/blob/main/docs/release-notes.md) | Physical ladders/ledges, draggable entities, binding changes and recovery across level changes. | Useful scenarios for future whip/climbing/mounted interaction and ownership recovery. Each needs CoJ-native permissions, contact and animation evidence. GoldSrc movement/event boundaries cannot be assumed in Chrome. |
| [UEVR](https://github.com/praydog/UEVR), [render/runtime boundary](https://github.com/praydog/UEVR/blob/master/src/mods/VR.hpp) | Distinguishes native, synchronized sequential and alternating stereo; treats UI as a separate integration concern. | Preserve CoJ's demonstrated full-wrapper stereo and evidence per eye/context. Use its methodology to investigate UI capture and temporal ownership; UE's stereo hooks, DX11/12 resources and profiles are not CoJ adapters. Do not substitute AFR for the current stereo path. |
| [OpenKneeboard injection architecture](https://github.com/OpenKneeboard/OpenKneeboard/blob/master/docs/internals/injectables.md) | Producer/consumer image ownership, overlays and isolated host fixtures before game integration. | Guide future native HUD sprite/flush capture with verified alpha, order, device generation and recovery. It neither locates CoJ's UI boundary nor proves scene-depth/hand correspondence. Existing text HUD remains the fallback. |
| [VR Performance Toolkit](https://github.com/fholger/vrperfkit), [configuration](https://github.com/fholger/vrperfkit/blob/master/vrperfkit.yml) | Configurable upscale/sharpen/foveation experiments and explicit API limits. | Reference only after measurements identify a native render bottleneck. Its supported graphics API is D3D11; CoJ renders in D3D9. Filtering transport output cannot remove the cost of CoJ's two native scene renders. No toolkit injection or guessed optimization is enabled. |
| [FarCry2VR](https://github.com/Braxusx/FarCry2VR), [DLSS guides](https://github.com/Braxusx/FarCry2VR/blob/c6bc74c370a0e32edf3c7227f2519fd8f199ab74/mod/src/dlss_fc2.cpp), [x64 helper](https://github.com/Braxusx/FarCry2VR/blob/c6bc74c370a0e32edf3c7227f2519fd8f199ab74/mod/src/dlss_host/dlss_host.cpp) | An x86 game can use an x64 DLSS helper through shared GPU resources; separate eye histories, camera-depth motion reconstruction, projection jitter and near-pass masks. Also separates render/output size and shares qualified sun-shadow work between eyes. | Research reference for a future temporal reconstruction backend. CoJ's D3D11 color transport is only one prerequisite: sampled per-eye depth, motion coverage, history/reset identity and GPU retirement remain unqualified. Dunia's D3D10 target/near-pass/shadow hooks do not establish Chrome contracts or justify changing the active D3D9/OpenVR path. |

The earlier local notes also mentioned Doom 3 BFG VR as a wrist/body UX example.
The cited KozGit release URL could not be retrieved during this review; no source
or implementation claim from it is used to select work. Existing CoJ wrist/body
decisions stand on their own evidence.

## FarCry2VR rendering findings and limits

Reviewed revision: `c6bc74c370a0e32edf3c7227f2519fd8f199ab74`.
This is source inspection, not a local build, performance comparison or headset
test of FarCry2VR. Its [README](https://github.com/Braxusx/FarCry2VR/blob/c6bc74c370a0e32edf3c7227f2519fd8f199ab74/README.md)
requires D3D10/OpenXR and advertises DLSS/DLAA, not DLSS frame generation.

- **Separate the algorithm host from the x86 game.** The
  [client](https://github.com/Braxusx/FarCry2VR/blob/c6bc74c370a0e32edf3c7227f2519fd8f199ab74/mod/src/dlss_client.cpp)
  selects the helper GPU by adapter LUID and duplicates D3D11 texture/fence
  handles into an x64 D3D12/NGX process. One feature per eye keeps temporal
  histories separate. This is a transferable architecture; it does not port
  the game renderer to D3D12 or remove the game's x86 allocation limits.
- **Generate meaningful temporal inputs.** The
  [guide pass](https://github.com/Braxusx/FarCry2VR/blob/c6bc74c370a0e32edf3c7227f2519fd8f199ab74/mod/src/dlss_fc2.cpp#L24)
  reconstructs camera motion from depth and current/previous eye matrices;
  it does not reconstruct independent object or skeletal motion. A near-pass
  mask biases arms/weapon pixels toward current color. For CoJ, moving hands,
  NPCs, foliage, particles, transparency and newly exposed surfaces require
  their own coverage/quality assessment. Camera-only vectors are a bounded
  prototype, not proof of artifact-free temporal reconstruction.
- **Keep projection and history coherent.**
  [Eye rendering](https://github.com/Braxusx/FarCry2VR/blob/c6bc74c370a0e32edf3c7227f2519fd8f199ab74/mod/src/main.cpp#L2667)
  uses subpixel jitter, unjittered guide matrices and history invalidation for
  discontinuities. CoJ must bind inputs to the same eye/frame/pose/generation
  and qualify recenter, snap turns, cinematics, tracking loss and resource
  recreation before enabling such a pass. Existing depth diagnostics establish
  descriptors only, not sampled scene/hand content or D3D11 transport.
- **Distinguish DLAA from upscaling.**
  [Target selection](https://github.com/Braxusx/FarCry2VR/blob/c6bc74c370a0e32edf3c7227f2519fd8f199ab74/mod/src/main.cpp#L1690)
  lowers actual eye rendering dimensions for DLSS modes while retaining larger
  output dimensions; DLAA keeps dimensions equal. DLAA is a possible initial
  quality experiment after depth/motion qualification, without first requiring
  independent eye resizing. Performance-oriented upscaling still requires
  lower native raster work and measured net benefit including helper overhead.
  Neither approach qualifies CoJ's rejected independent-resolution bank.
- **Investigate redundant work before raising quality costs.** The
  [sun-shadow path](https://github.com/Braxusx/FarCry2VR/blob/c6bc74c370a0e32edf3c7227f2519fd8f199ab74/mod/src/main.cpp#L1190)
  reuses a common atlas for the right eye while retaining per-eye work for other
  lights. CoJ would need independent evidence of equal light/cascade/caster
  inputs and retained atlas lifetime. Never skip the demonstrated full second-eye
  wrapper on this basis. Isolating native HUD before temporal processing is
  another useful pattern; CoJ's native graphical HUD ownership remains open.
- **Audit executable behavior, synchronization and attribution.** In this
  revision, FSR references remain in comments/protocol structures, but the host
  selects DLSS; those references do not establish working FSR support. Likewise,
  the IPC header describes a bounded CPU fence-completion wait, while the client
  frame path receives an acknowledgement and queues a GPU wait without that
  completion check. Do not inherit a completion guarantee from the header.
  Preserve CoJ's proven retirement rules and test helper exit/failure separately.
  The project declares GPLv3 and credits helper ancestry from Dishonored/BioShock
  projects; verify exact source notices and NVIDIA runtime terms before reuse.

The original standalone CoJ x86/x64 NGX D3D11 backend now exercises the helper
pattern on controlled host inputs, without copying FarCry2VR implementation or
replacing the primary D3D9/OpenVR path. Native options save reconstruction requests
with explicit pending status. Exact-game depth/coverage, transport, scoped jitter
and atomic presentation remain open; [architecture](../ARCHITECTURE.md#physical-candidate-image-quality)
and [validation](../VALIDATION.md) own those boundaries and acceptance.

### Independent DLSS cross-check (2026-10-25)

NVIDIA's [DLSS Super Resolution programming guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS.md)
requires render-resolution input colour, depth, motion vectors and a separate
output, with coherent per-frame resources. Its
[integration checklist](https://developer.nvidia.com/blog/how-to-integrate-nvidia-dlss-4-into-your-game-with-nvidia-streamline/)
calls out object/material motion, compatible jitter, reset on camera discontinuity,
exposure and real input/output resolution selection. These are requirements on the
source game integration even when NGX evaluation works on synthetic GPU textures.
CoJ's `1B_16` alpha consumer is a **candidate** source, not a qualified depth
buffer; positive/far alpha values, write-call counters and shader identity alone
do not establish an axial depth field for hands, sky, transparency or NPCs.

The [ETS2/ATS DLAA/DLSS injection project](https://github.com/JRDevo/ETS2-ATS-DLAA-DLSS-Injection-Mod)
describes separate OpenXR passes per eye, dynamic-object motion, DLAA at native
render resolution and actual reduced raster for DLSS. This comparison is based
on its public project documentation, **not** an independent source/runtime audit
or verification of its advertised quality. Its native D3D11 interception cannot
be transferred directly to CoJ's D3D9 game. The separately developed
[OptiScaler VR fork](https://github.com/tig3rmast3r/OptiScaler_DLSSNR_VR)
also describes isolated per-eye model resources/history, with wider compatibility
explicitly experimental. Both reinforce the stereo-history boundary already
enforced by CoJ's host fixtures, without resolving CoJ's input-ownership gap.

**Super Resolution decision gate:** stop adding new NGX host modes or sampling-only diagnostics
until one exact CoJ eye/frame shows a full-size, source-qualified depth and colour
pair with measured material coverage, and the other eye is correlated to the same
pose/generation. If the HDR alpha field fails coverage, investigate a different
native depth stage before expanding the helper. Only after that admission should
the project attempt scoped jitter/motion, an atomic DLAA A/B and finally a safe
reduced-native-raster DLSS path with measured end-to-end latency and GPU cost.

### Penumbra DirectNR comparison

The operator reports that the initial working Neural experiment was exercised in
Deus Ex: Human Revolution Director's Cut, with a later Penumbra implementation
that functions but does not apply the effect entirely correctly. Penumbra is a
source/lifecycle reuse reference here, not an accepted image-quality baseline.
That report does not establish a cause for Penumbra's imperfect application or
transfer visual acceptance to CoJ.

Source inspection of Penumbra VR Framework revision
`7b014c96ed928440abf4452664f35220d06c1fb6` establishes a separate experimental
route from the Super Resolution backend above. Its
[`NeuralPass`](https://github.com/rubocopter/penumbra_vr_framework/blob/7b014c96ed928440abf4452664f35220d06c1fb6/tools/neural_post/src/NeuralPass.cpp)
adapts the included MIT LowPopLabs DirectNR implementation: D3D11 final-eye
color is processed through a private D3D12 NGX Neural Rendering backend using
`nvngx_dlssnr.dll`, estimated rotational motion and constant depth. This is a
generative appearance edit, not Super Resolution, DLAA or frame generation.
It neither demonstrates real scene depth nor reduces native raster work.

The reusable boundaries are visible in the
[`bridge`](https://github.com/rubocopter/penumbra_vr_framework/blob/7b014c96ed928440abf4452664f35220d06c1fb6/src/graphics/neural_post_bridge.cpp),
protocol, residual resolve and explicit GPU roundtrip tests:

- One request binds both eyes, tracking, settings revision and history epoch.
  Startup/model warmup is asynchronous. Deadline misses retain the original
  pair; input remains immutable until completion, late output is discarded,
  and a new request resets history. Worker failure/stall retires the session.
- The transport proxy is not the final eye. Resolve adds the processed-minus-
  input proxy difference to the original full-resolution eye, preserving source
  detail/alpha and the encoded UNORM color-space contract. Identity, fine-detail,
  sRGB, timeout and child-fault cases are explicit test patterns to adapt.
- Pose/FOV is supplied per view, but DirectNR packs views into a shared atlas
  and creates model features by pass/tier. This is not evidence of the independent
  per-eye Super Resolution histories used by CoJ. Atlas boundaries and binocular
  consistency need their own qualification.

CoJ already transports native D3D9 eye colors into D3D11. A renderer-owned
DirectNR candidate can therefore be investigated without admitting Chrome HDR
alpha as depth or enabling the rejected independent-resolution bank. Candidate
placement is after a fresh complete native color pair and before CoJ comfort,
scope and overlay composition. Captured native HUD content may already be baked
into color; that coverage must be observed rather than assumed removable.
The current presenter/spatial processor is an insertion boundary, not an existing
Neural consumer. Preserve capture frame/pose/device/generation identity and publish
either a complete current processed pair or the complete original pair.

Penumbra's OpenGL readback/upload and CPU shared-memory transport are not a
drop-in match for CoJ. CoJ's existing x86/x64 D3D11 shared-resource/fence work
offers a transport starting point, but its `dlss_protocol` and Super Resolution
client do not speak DirectNR. Choose one bounded transport experiment, measure
copy/wait/inference cost, and retain immutable pending-resource leases through
completion before expanding settings or packaging. Preserve Penumbra/DirectNR
attribution and licenses; local NVIDIA model import remains a separate dependency.

**Decision:** treat DirectNR as an optional, default-off appearance experiment,
separate from the Super Resolution depth gate. First adapt a synthetic stereo
roundtrip with identity/residual/color/timeout/recovery tests, then compare real
CoJ native/Neural images and total frame cost in one explicit candidate. Penumbra's
documented synthetic and limited live-menu evidence does not qualify CoJ gameplay
or headset quality. Greater visual benefit in CoJ is a hypothesis; no performance
or visual improvement is established. The depth/motion/jitter/reduced-raster gates
above still apply to genuine DLSS Super Resolution.

CoJ now has a standalone implementation in `tools/neural_host` and
`backends/openvr/neural_session.*`, exercised by explicit D3D11 laboratory probes.
It uses a bounded CPU-pixel transport experiment, asynchronous disposable model
warmup, response identity checks and late-output discard. The residual fixture
retains the full original eye, checks UNORM/sRGB and alpha, and observes a real
local-model RGB edit. These controlled inputs prove an inference/transport path,
not a correct application to gameplay. A 64px diagnostic stage caused device
removal in a real-model attempt; the 256px transport reference completes. Other
sizes, sustained behavior, model-atlas binocular consistency and real scene/HUD
coverage remain unqualified. An explicit developer presenter candidate now
consumes the protocol using current captured HMD/eye optics, asynchronous proxy readback,
private pair residual composition and a separate native-menu Neural request with
actual state and committed-pair count. The first game's same-frame readback
remained pending and processed no pairs; the correction retains one private pair
and its metadata across polls instead of restarting the query. Publication is
cancelled on context, identity or settings changes and capture age expiry. It bypasses
theater/scope/private-bank paths, preserves originals while warming/recovering,
and records dependency identities through reversible staging. Host native-size
fixtures establish integration; real game application/quality and sustained cost
remain separate physical gates. See
[validation](../VALIDATION.md#standalone-neural-laboratory) for its evidence state.

## Decisions for this project

1. **Measure visible contact before moving the cartridge again.** Compare both
   displayed skin pads against the actual displayed case mesh and axis. Existing
   pad-gap/envelope and triangle-proximity diagnostics are host-tested; replay
   overlap does not resolve the operator's floating-cartridge report. These
   measurements can distinguish longitudinal misalignment from spacing and
   occlusion. They do not establish skin contact or native depth coverage.
2. **Keep the rifle rigid and acquire deliberately.** The exact native idle
   hand socket supplies anatomy; acquisition uses measured wood geometry on the
   live weapon element, following the corrected Remington grasp. Preserve one map
   for hands/weapon/muzzle,
   crossed-support rejection, cancellation and fresh regrip. Smooth engagement
   and virtual stock are later comfort options, contingent on physical evidence;
   do not hide ownership errors with damping or arbitrary offsets.
3. **Develop reload families independently.** Winchester is the first passive
   long-gun candidate. Read raw native states/animation/reserve across conventional
   Square before building a manual session. Two-barrel and mixed-ammo weapons
   require indexed transfer, not the pistol whitelist. Exact classes and ammo
   facts live in [reload ownership](COJ_RELOAD_OWNERSHIP.md).
4. **Keep control hints action-aware.** Preserve implemented origin-aware text,
   localization, bounded coaching and binding-generation invalidation. Graphical
   glyphs still need qualified device/component identity. Keep prompts, haptics
   and native action admission distinct; host tests do not accept visor fit.
5. **Qualify HUD/depth ownership before composition.** Capture the right native
   content once, establish per-eye timing and restore/reset boundaries, then
   investigate composition. A matching depth size or eye metadata is insufficient.
6. **Optimize from attribution.** Use existing copy/fence, frame-age, cadence,
   drops and memory evidence. Diagnose recording separately from normal play.
   Resource/reload probes perturb timing and cannot qualify performance. Keep
   independent resolution default-off until its native ownership gates close.

The [roadmap](../ROADMAP.md#development-backlog) owns implementation priorities;
[validation](../VALIDATION.md#pending-headset-checks) owns physical acceptance.
Neither a source comparison nor a test pass changes those states. Temporary
research searches, downloaded references and individual candidate/run notes stay
under ignored `work/`.
