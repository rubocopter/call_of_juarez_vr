# Human-focused Neural rendering boundary

## Scope and evidence

The operator prioritizes human appearance, especially nearby faces, over changes
to scenery. The full-frame Neural candidate delivered real inference and commits
but is rejected for image cadence and lack of appreciable improvement. This
document records exact-game targeting evidence and the narrower experiment;
[Validation](../VALIDATION.md#standalone-neural-laboratory) owns acceptance.

The inspected original code archive has SHA-256
`f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12`.
The engine and Data0 identities remain the existing exact Steam contracts. Native
method registration establishes JNI availability, not a callable C++ ABI or
qualified output semantics. No unknown build acquires game-specific authority.

## Human identity and head ownership

`NewAIHuman` and the `HumanBeing` player family are separate descendants of
`PawnArmed`. An NPC collector cannot reuse the player's class/instance assumptions.
The shipped `ArmedPlayerBeing.ComputeDestinyBulletTimeLookDistance()` supplies an
independent NPC precedent: resolve `NewAIHuman.GetMyClassID()`, enumerate through
`GameObject.GetObjectsFromClass(int)`, check active/renderable state and compare
`GetLastVisibleFrame()` with the module render counter. It then resolves
`Bip01 Head` and obtains its position. Bullet-time conflict-side/ODE policy is
specific to that gameplay feature and is not a definition of all humans.

`NewAIHuman.OnCreate()` also resolves `head` and `neck`; its head-tracking update
rejects invalid elements. Element names and lookup case are not interchangeable
without evidence. `GetHeadTrackPoint(Vector)` identifies what the NPC is looking
at, not the NPC's own head. Missing-head origin fallback is unsuitable for a face
crop. Runtime class IDs must be resolved rather than hardcoded.

Original sampled male, female, child and named-character meshes contain separate
skinned head geometry in addition to the head transform bone. Node names, indices,
LODs, morphs and attached hats differ by model. A head bone's extents do not prove
coverage of the animated facial geometry. Equipment/player affine assumptions
must not be extended to these meshes merely because their containers match.

## Available boundaries and unresolved semantics

| JNI boundary | Established use / remaining qualification |
| --- | --- |
| `GameObject.GetObjectsFromClass(int)` | Shipped NPC enumeration; module/lifetime and film/corpse coverage remain to be established. |
| `GetObjectsFromClassModule(String, Module)` | Module-scoped native registration exists; not an existing C++ NPC snapshot collector. |
| `MeshObject.GetElementID(String)` / `GetElementPos(int, Vector)` | Shipped NPC head queries; validate return/element/finite coordinates. |
| `GetElementExtentsInWorld(Vector, Vector, int)` | Candidate head-geometry bounds; animated/LOD coverage and failure outputs unqualified. |
| `GetMeshExtentsInWorld(Vector, Vector)` | Whole-character candidate bounds, not a face mask. |
| `GetScreenExtents(Camera, Vector, Vector, int)` | Candidate element rectangle; pixel space, clipping and correlation with each actual eye target unqualified. |
| `GetObjectsInFrustum(Camera, float)` | Candidate set only; neither per-eye visible-pixel ownership nor occlusion. |
| `GetLastVisibleFrame()` / `GetRenderCounter()` | Shipped object-level hint; behavior across stereo passes, shadows and mirrors unqualified. |

Use the existing game-thread JNI exception/reference discipline and value-only
snapshots. Preserve unsigned actor identities with module/lifetime generation;
a numeric object ID alone is not lifetime proof. The helper and renderer must
not retain Java/native NPC references. Existing `JavaPlayerBridge` bone readers
are bound to its player instance and cannot inspect an arbitrary NPC unchanged.

The camera adapter owns centimetre conversion, per-eye source basis/frustum and
top-left projection conventions. Its point/ray projection is not a clipped-box
projector. Bounds crossing the near plane or screen edge require explicit
clipping; simply accepting a subset of projected corners is insufficient.

No qualified NPC ID buffer, face raster mask or full-resolution per-eye depth
consumer is established. A centre line-of-sight trace, a rectangle or a visible
object counter does not prove which pixels belong to a partially obscured face.
The separate [PPFX depth research](COJ_PPFX_TARGET_OWNERSHIP.md) retains its material
coverage and transport gates. Skin-color heuristics and model `UseAutoMask` do
not supply game-owned human identity or occlusion proof.

## Renderer and model experiment

Crop native-density human regions **before** inference. Processing the entire
eye and masking the result afterwards cannot remove that inference cost.
Associate each eye rectangle and visible-pixel mask with actor/lifetime,
capture/pose/device/generation, settings and crop identity. Crops must retain
their exact source pixels; padding and atlas seams must not bleed between eyes.
Compose only owned RGB pixels, preserving alpha and all pixels outside the mask.

Native delivery must continue while optional work is pending. A result for an
already superseded source pair is discarded; neither holding native images nor
attaching an old residual to a new image is an accepted shortcut. A low or zero
same-source completion rate is a feasibility failure to measure, not authority
to relax those fences.

The standalone image laboratory makes model scale explicit and avoids the hidden
second reduction of detail when testing native-density face crops. Controlled
original-asset head previews are not native shader output, representative scene
lighting/animation or stereo evidence. Duplicating one preview into two inputs
does not establish binocular quality. RGB difference and elapsed time do not
establish a useful change in facial identity, skin, eyes or hair.

Cost comparisons must name source, crop, padding, atlas and actual model extents.
A higher-detail crop may cost more than the rejected very-low-resolution whole
scene model. Savings against a comparable-detail full-eye pass, private allocation
bytes, copy bandwidth and total native frame cadence are separate measurements.
Include selection/mask cost and rejected/cancelled work. The laboratory's blocking
readback timings are not an admitted game-thread pipeline or a headset FPS result.

The controlled head comparisons currently show tone/detail changes, without a
demonstrated worthwhile facial-quality gain. Explicit scale and style comparisons
are diagnostic inputs to that decision, not selected presets. Changing two knobs
at once cannot isolate their individual effects; intensity-matched style controls
are recorded separately from the inherited defaults.

A manually selected crop from an operator-positioned flat game capture extends
these comparisons to native scene lighting and a rendered nearby face. The real
model changes skin detail and contrast in that still image; this is a reviewable
appearance comparison, not an accepted enhancement. Preserve the JPEG source and
its baked crosshair as input limitations. Manual crop selection is not automatic
human detection, an occlusion mask or native eye-buffer ownership. Monocular
duplication and static poses cannot qualify moving NPC history or stereo.

The operator additionally prefers a single Neural pass, the style named
*Cinematic* in third-party controls (numeric style 2), preset hint 1 and a
moderately higher intensity. The local model reproducibly distinguishes
`style 2 / preset 1 / intensity 80` from `style 2 / preset 0 / intensity 80`
on this same static capture, but the difference is mainly tonal and does not
establish a useful facial enhancement. Offline model changes do not resolve the
previously rejected live frame cadence or temporal behavior.

## Temporal guidance

Human-focused crops introduce actor/skinning motion and changing crop origins in
addition to head rotation. A model history keyed only to HMD rotation and constant
depth does not account for these changes. Preserve crop projection/scale and
history identity, and establish appropriate motion/depth ownership before claiming
temporal stability. Reframing and invalid actor/crop history must invalidate the
old guidance. Skin automatic masking is not motion guidance.

The primary [nvpro renderer documentation](https://github.com/nvpro-samples/vk_gltf_renderer/blob/master/docs/denoising.md)
describes separate NR controls and depth/motion inputs for temporal stability.
That renderer's implementation is a comparison source; it grants no Chrome
resource, coordinate or material-coverage assumption.
