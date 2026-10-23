# VR reference projects and CoJ application

These are design and engineering references, reviewed against primary project
sources on 2026-10-21. They establish hypotheses and useful patterns, not Chrome
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
| [Half-Life: VR Mod release notes](https://github.com/maxmakesmods/Half-Life-VR/blob/main/docs/release-notes.md) | Physical ladders/ledges, draggable entities, binding changes and recovery across level changes. | Useful scenarios for future whip/climbing/mounted interaction and ownership recovery. Each needs CoJ-native permissions, contact and animation evidence. GoldSrc movement/event boundaries cannot be assumed in Chrome. |
| [UEVR](https://github.com/praydog/UEVR), [render/runtime boundary](https://github.com/praydog/UEVR/blob/master/src/mods/VR.hpp) | Distinguishes native, synchronized sequential and alternating stereo; treats UI as a separate integration concern. | Preserve CoJ's demonstrated full-wrapper stereo and evidence per eye/context. Use its methodology to investigate UI capture and temporal ownership; UE's stereo hooks, DX11/12 resources and profiles are not CoJ adapters. Do not substitute AFR for the current stereo path. |
| [OpenKneeboard injection architecture](https://github.com/OpenKneeboard/OpenKneeboard/blob/master/docs/internals/injectables.md) | Producer/consumer image ownership, overlays and isolated host fixtures before game integration. | Guide future native HUD sprite/flush capture with verified alpha, order, device generation and recovery. It neither locates CoJ's UI boundary nor proves scene-depth/hand correspondence. Existing text HUD remains the fallback. |
| [VR Performance Toolkit](https://github.com/fholger/vrperfkit), [configuration](https://github.com/fholger/vrperfkit/blob/master/vrperfkit.yml) | Configurable upscale/sharpen/foveation experiments and explicit API limits. | Reference only after measurements identify a native render bottleneck. Its supported graphics API is D3D11; CoJ renders in D3D9. Filtering transport output cannot remove the cost of CoJ's two native scene renders. No toolkit injection or guessed optimization is enabled. |

The earlier local notes also mentioned Doom 3 BFG VR as a wrist/body UX example.
The cited KozGit release URL could not be retrieved during this review; no source
or implementation claim from it is used to select work. Existing CoJ wrist/body
decisions stand on their own evidence.

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
