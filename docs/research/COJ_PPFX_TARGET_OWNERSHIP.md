# ChromeEngine 3 postprocess target ownership

## Scope and evidence

This is an exact-build **static ownership map**, corroborated in part by the
bounded gameplay binding census. It does not qualify a native writer, successful
independent allocation, shader constants, reset recovery or headset behavior.
The [resolution design](COJ_VR_INDEPENDENT_RESOLUTION_DESIGN.md) still controls
Q0/B0/B1 admission; [Validation](../VALIDATION.md) owns acceptance.

Target: x86 `ChromeEngine3.dll`, SHA-256
`db69bc35919fe57187766771a2452aca11090474f6d63df1a85a80eded131ec8`.
All code/global addresses below are **RVAs**; object offsets are bytes.
Names describe recovered responsibilities, not a promise of exported symbols
or a callable ABI. Evidence comes from individual native procedure analysis,
immutable PE bytes/vtable/RTTI and original `Data0.pak` effect definitions.
Unanalyzed vtable entries were decoded from file bytes, with the creator bytes
also compared with REA's initialized image. Raw decompilation, disassembly,
archive members and run-specific correlations remain local under ignored `work/`.

## Object and call boundaries

The global at `0x590074` points to the **native renderer**, not the COM device.
The COM D3D9 device is at `0x590078`; `CPPManager` is at `0x5902AC`.
RTTI and file pointers identify the inspected `CD3DRenderer` vtable at
`0x312334`. Its byte offsets `+0x70/+0x74` create/release native texture handles;
`+0x58` selects a render target; `+0x4C` sets/clamps the viewport.
These are native virtual methods, distinct from similarly numbered COM slots.

| RVA | Recovered responsibility | Consequence for independent eyes |
| --- | --- | --- |
| `0x224F90` | `CPPManager::CreateBuffers`: when dirty, calls release first, updates global/local descriptors, creates or links handles, clears dirty only after success. | Destructive rebuild; a null allocation returns failure after old resources have gone. It is not a transactional per-eye factory. |
| `0x224D40` | Releases global handles and unlinked local handles through renderer `+0x74`; clears every handle including borrowed entries; marks dirty. | A private bank must preserve alias ownership and release each owned handle exactly once. |
| `0x224EC0` | Drops references to full/simple/null scene effects; a final reference removes the effect and rebuilds/releases affected buffer state. | Not a harmless dimensions setter. Effect/list/refcount lifetime belongs in the native transaction. |
| `0x225690`, `0x2254E0` | Select/load native full/simple/null effects; load reuses a named effect or allocates/parses a new one and marks buffers dirty. | Loading a second effect can affect sharing and rebuild state. A shallow manager copy does not isolate its command graph. |
| `0x24D2C0`, `0x24D090` | Add a bounded descriptor; recompute screen-relative dimensions from renderer `+0x1C8/+0x1CC`, minimum one pixel, optional default RGB/alpha bits. | Logical scene dimensions are native state, separate from the final D3D target description. |
| `0x212D70`, `0x20EF10` | Renderer buffer creation/release; texture manager allocation plus renderer-owned color/depth caches and their use counts. | Creating/releasing one native handle can mutate shared renderer caches. It is not equivalent to creating/releasing one COM texture. |
| `0x24A130`, `0x211770` | Local SetRT command resolves its saved container/index; renderer finishes the previous target, selects surfaces/depth and updates native target/viewport state. | Rebinding RT0 outside the wrapper is insufficient; native commands can select their own resources again. |
| `0x211670` | Finishes the current internal color path by copying to the native target texture. | Capture/selection order must include this resolve/copy even with a no-MSAA candidate. |
| `0x24A3E0` | Screen-quad command derives geometry from logical renderer dimensions/divisors and adjusts UVs for power-of-two storage. | Descriptors, quad dimensions, viewport and texel constants must agree. |
| `0x211440`, `0x210C20` | Renderer reset and teardown release internal resources; reset releases/recreates PP buffers around COM Reset; teardown destroys the manager/device. | Experimental banks cannot survive by retaining stale native pointers across these boundaries. |

## Descriptor and alias contract

A buffer container has a signed count at `+0x644` and at most 16 entries starting
at `+4`, stride `0x64`. The recovered descriptor offsets are relative to an entry:

| Offset | Meaning |
| --- | --- |
| `+0` | Native texture handle/pointer, not `IDirect3DTexture9*`. |
| `+4..+0x43` | Bounded name storage, explicitly terminated. |
| `+0x44/+0x48` | Logical width/height. |
| `+0x4C/+0x50` | Requested RGB/alpha bits, not D3DFORMAT. |
| `+0x54/+0x55` | Options controlling the internal color path and depth attachment, respectively; translated into native flags `0x10/0x08`. |
| `+0x58/+0x5C` | Screen-relative divisors; positive values replace the corresponding dimension. |
| `+0x60/+0x61/+0x62` | Use native screen BPP / Temp-shareable / linked-borrowed. |

The manager's loaded-effect list is at `+0x64C`; nodes contain links and an effect
pointer at `+8`. Dirty state is `+0x6B5`. Full/simple/null scene-effect references
are at `+0x6C4/+0x6C8/+0x6CC`; loaded effects have a reference count at `+0x81C`.
The render-command compiler embeds the effect/container pointer and buffer index
in commands. Switching only the manager global leaves those references unresolved.

The loaded-list size is the manager's u32 at `+0x650`; this is distinct from an
effect's i32 native reference count. Each effect filename is an inline 260-byte
array at `+0x684`, with a terminator at `+0x787`; descriptor names remain 64
bytes. The manager and effect native vtables are RVA `0x314C48/0x3180AC`.
The diagnostic's maximum of 16 loaded effects is its own conservative bound,
not an established engine list limit.

Temp entries may borrow equal-sized/equal-format/equal-option entries from
**earlier loaded effects**. The allocator avoids using one handle twice in the
same effect. Linked entries skip native release; they do not acquire an
independent ownership count through this linking path. Count handles/allocations
by identity, not by buffer names. A private family must retain the entire relevant
alias graph and its owning entries; copying descriptor bytes is insufficient.

## Renderer caches and final target

Native texture objects retain the actual 2D/cube/volume interfaces at `+4/+8/+0xC`,
with selected usage/format at `+0x10/+0x14`, native flags at `+0x134`, physical
dimensions at `+0x148/+0x14C` and reciprocal dimensions at `+0x154/+0x158`.
`0x23DDD0` creates those COM interfaces. Its errors are logged by native code;
a returned native object is not proof of a non-null successful COM allocation.
Readback validation is required before any experimental admission.

The word at native texture `+0` aliases the selected COM interface; it is not a
native vtable. Kind is u32 `+0x130` (2D/volume/cube = 0/1/2), flags are u32 at
`+0x134`, and requested W/H/D `+0x13C/+0x140/+0x144` must be distinguished from
physical creation W/H/D `+0x148/+0x14C/+0x150`. Native sampler texture pointers
also occupy two distinct ranges: 16 pixel slots at renderer `+0x1CD0` and four
vertex slots at `+0x1D50`. The intervening `+0x1D10` words are sampler state,
not four additional textures. An observer must not follow those state words as
objects or apply the renderer vtable contract to a texture's COM alias.

The renderer has additional resources outside the PP descriptor arrays:

| Renderer offsets | Responsibility recovered from creation, selection and release |
| --- | --- |
| `+0x1C10..+0x1C28` | Internal color/depth pair: selected sample type, format, COM surfaces, use count and maximum allocated dimensions. |
| `+0x1C2C..+0x1C38` | Additional render-target depth surface, use count and maximum dimensions. |
| `+0x1C3C..+0x1C48` | Depth-only rendering color placeholder texture, use count and maximum dimensions; first tries 1x1 FourCC `NULL`, then sized R5G6B5 fallback. |
| `+0x1DB4/+0x1DB8/+0x1DC0` | Current native target, face/index and NOFSAA selection state used by target finishing. |
| `+0x1DC4/+0x1DC8` | Retained native backbuffer/depth surfaces used when selecting the default target. |
| `+0x1C58..+0x1C6C` | Cached viewport including depth range. |
| `+0x15C4/+0x15C8` | Viewport clamp dimensions for the default-target branch, distinct from logical screen dimensions `+0x1C8/+0x1CC`. |

`0x211040/0x211120/0x2111B0` adjust the respective resource groups, growing to
maximum requested dimensions and changing counts. Growth can release an older
surface before allocating its replacement. The color path admits only one
internal format at a time. **FSAA 0 does not remove this internal color/depth
ownership**: a texture's native fsaa flag can still select that path with sample
type NONE. The gameplay census's unparented full HDR RT and later texture-backed
HDR targets are consistent with this distinction; this is corroboration, not a
live pointer-to-descriptor identity map.

SetRT with a null native handle means the retained native default surfaces,
not an arbitrary externally bound RT0. With an already-null current target it
can update caches without rebinding. In a non-internal color branch, requested
depth may use the native retained depth when native FSAA is disabled. These
branches prevent admission of a larger eye family that substitutes only local
PP textures. Target caches, default destination, depth, copy completion and
viewport clamp must be qualified/restored together. The exact dirty-state masks
and downstream shader-state consumers still require complete mapping.

## Effects, rounding and a partial payload budget

The original scene30 recipe contains 14 descriptors: two full HDR colors, one
quarter HDR color, a one-column HDR reduction of height H/4, a fixed 1x1 HDR
reduction, two full native-BPP colors, one half color, two quarter colors, one
eighth color and three sixteenth colors. The render order includes HDR scene
and refraction, luminance reductions, tone mapping, bloom, DOF and final default
target composition. The observed gameplay formats/extents and ordered draw
groups fit this recipe, but do not establish every shader read or active effect
identity. Other loaded effects, simple/null routes and reflections remain relevant.

The materials establish destination-history dependencies beyond buffer names:
the bloom vertical pass writes `16Prev` with source-alpha/inverse-source-alpha
blending and authored output alpha 0.75; the recipe does not clear it there.
The fixed `1x1_16` HDR adaptation uses blend-factor/inverse-blend-factor blending
with `fhdrblendweight`, also without a recipe clear before that write.
These histories must initially be treated as **separate for each eye**. Sequential
reuse of an undivided family can feed the first eye into the second. Actual
compiled state, weight updates, first-use initialization and reset/owner-change
history policy still need qualification. Do not disable either effect to admit B0.

The authored materials also identify concrete size/projection consumers:
`PROJECTION_XFORM` for screen quads, `RENDER_TARGET_PARAMS` for HDR reduction
sample offsets, `TEX_INV_SIZE_*` for blur/reduction sampling, and
`TEX_SIZEX8_*`/`TEX_SIZEY8_*` for grouped luminance loops. Native shader uploads
are conditional on renderer/material dirty state. These names identify what to
trace, not proof of the uploaded registers, physical/logical extent choice or
cache invalidation. The grouped loops also require checking non-multiple-of-eight
input extents; a bigger viewport cannot repair an incorrect loop bound.

Static parameter-table initialization and the referenced readers resolve these
dependencies further (RVA, not an enabled hook):

| Parameter | Reader RVA | Recovered source |
| --- | --- | --- |
| `RENDER_TARGET_PARAMS` | `0x2446E0` | Reciprocal and direct current-target dimensions from native texture `+0x13C/+0x140`; default target uses renderer logical `+0x1C8/+0x1CC`. |
| `TEX_INV_SIZE_*` | `0x244C90` | Reciprocal fields `+0x154/+0x158/+0x15C` of native textures retained in renderer sampler slots beginning at `+0x1CD0`; not the externally bound COM texture alone. |
| `TEX_SIZEX8_*`, `TEX_SIZEY8_*` | `0x241970`, `0x2419E0` | Physical texture width/height `+0x148/+0x14C`, shifted right three, from the same native sampler cache. |

The initialized records associate names/IDs/dirty dependencies with those
readers; `0x2465A0` conditionally dispatches material/shader updates, including
`0x246380/0x2464B0`. This maps size sources, not actual compiled registers or
successful per-eye invalidation. A COM-only texture substitution can leave
native sampler caches and all these constants referring to the original size.

For a non-power-of-two 2804x2860 recipe, quarter input extents are 701x715.
The authored eight-sample loops and recovered floor-divided bounds would visit
696 columns and 712 rows while using reciprocal full input dimensions. This is
a static inference about this shader variant, not a measured visual defect;
compiled variants, physical storage and padding must be correlated. B1's quarter
extent 640x480 avoids that remainder while the contract is qualified.

Native descriptor division uses integers; screen-quad extents use float division.
At 2804x2860 the sixteenth target is 175x178 while the quad calculation can
produce 175.25x178.75. Storage can additionally round up to powers of two under
the inspected capability branch. This is a concrete reason to start with
2560x1920 (divisible by 16), then inspect texel centers, UV bounds and projection
rather than replace W/H globally. The existing asymmetric eye optics stay fixed.

The following is **recipe payload only**, MiB, using observed 8-byte HDR and
4-byte native-BPP formats, integer division, one level and no MSAA/padding.
It is neither an allocation measurement nor a process VA budget. It counts one
instance of every descriptor; aliases with other effects must be deduplicated
from runtime identity. Internal surfaces, depth variants, final independent
destination, retained original family, transport and compositor are excluded.

| Eye extent | 14 recipe colors | Additional internal HDR+depth if separately retained | Separate final 4-byte color if required |
| --- | ---: | ---: | ---: |
| 2560x1440 | 91.79 | 42.19 | 14.06 |
| 2560x1920 | 122.39 | 56.25 | 18.75 |
| 2560x2560 | 163.19 | 75.00 | 25.00 |
| 2804x2860 | 199.68 | 91.78 | 30.59 |

The internal column assumes one 8-byte color and one 4-byte depth at eye size;
it does not authorize reuse across native depth branches. The final-color column
is separate from the recipe's two full BPP colors; whether it can safely alias
one of them depends on final read/write order. Add the design's capture/output,
reserved theater and overlap costs separately. Do not multiply these columns
by two without deciding which histories/scratch allocations need per-eye state;
do not use the columns to assume that only one bank is sufficient either.
At minimum the authored history dependencies add another sixteenth 4-byte color
and another 8-byte 1x1 color when two eyes share the remaining scratch recipe.
That is 0.07 MiB at 2560x1920 and 0.12 MiB at 2804x2860 before padding, metadata
and other histories. Cheap history payload does not solve native ownership.

## HDR-alpha depth candidate

The original pinned `Data0.pak` (SHA-256
`5eb6ec4856e53226dfabad929522706a21f02d428923a8c9c38804cc2cf11a94`)
provides a concrete depth candidate, independent of the final D3D9 depth surface.
`Data/Shaders/Common/Consts.hlsl` defines `DEPTHSCALE 0.01`.
`DefaultShader/30/DefaultShader30_Pass.mtt` selects `USE_WRITEZ` for `bWriteZ`;
`VS2PS30.hlsl` enables `TC_DEPTH`, the vertex shader writes clip-position W times
that scale, and selected pixel-shader variants write `TC_DEPTH` into color alpha.
This is consistent with the adapter's centimetre-to-metre conversion, provided
that W is positive axial view distance for the actual scoped projection.
The relevant vertex/pixel member SHA-256 identities are
`8afea48f780a86b60c9add6e4d773f6682786a18a2e09dd17ebd83bbdcabfa7f`
and `e6bc3ceaa48631d45a1c5afda6d3c8c99e9ddcaa44bcb4b609505445dfce5405`.

`Data/Postprocess/Common/Mix30.mat` samples alpha from user-map sampler 10 as
depth for near/far DOF masks; `scene30.ppfx` supplies `1B_16` at that consumer.
The same effect explicitly describes its final backbuffer scene pass as lacking
the scene Z buffer. A final presentation DSV is therefore not a qualified source.
The Mix30 member SHA-256 is
`e668a1e4771da3a61eaec0b3a761d282f503779dea3a83b6330cf3f7a659f0a4`.

This is not an all-material depth guarantee. Other branches write opacity, glow
or constant alpha; sky, transparent effects, independently moving characters,
native near-weapon/hand passes and postprocess ordering require content evidence.
Matching dimensions, descriptor endpoints or finite positive alpha cannot qualify
those semantics. Do not apply `.01` again to alpha already written with DEPTHSCALE.

The existing opt-in resource census now attempts bounded 16x16 POINT alpha copies
at a successful matching gameplay draw, only if a current COM-owned pixel texture
matches the exact-build copied `1B_16` identity and dimensions. It never calls a
decoded native pointer. A six-slot GPU-query ring keeps copied samples until
readback at a Present boundary; reset/teardown discard them. Values and
frame/eye/owner/device/generation/scope identity are diagnostic telemetry, explicitly
not temporal-source admission. Sampling may be skipped if the consumer binding
is no longer present or the format/copy operation is unavailable. The long
exercised session produced no alpha pixels: fixed D3D register 10 was incorrect.
Material user sampler 10 is a logical effect input; actual binding-state records
correlate `1B_16` consumption with D3D registers 0 and 2, including native draws.
The corrected observer checks all current pixel texture bindings 0..15 against
the exact copied identity/extent/format. It does not reinterpret the copied
pointer as a callable object or pick any texture merely by matching dimensions.
The revised observer retains begin-scope identity, marks an attempt before its
diagnostic copy to prevent reentry, and reports absent consumers at scope end.
GPU fixtures cover actual registers 0/2/10, wrong extents, unbound identities and
copied pixel lifetime. The hidden fixture submits its own event query explicitly;
the production probe still polls without flush and retains native cadence.
The subsequent native run delivered four 16x16 alpha grids in two eye pairs.
One left-eye grid has an exactly-one lower half while its right partner varies;
far-valued 65504 samples and non-normalized varying values also occur. Delivery
is observed; neither those values nor source shader text qualify depth semantics.
The two sampled frames were early/at a level transition and lacked co-located
RGB, consumer and pose witnesses. Do not infer that exactly one always means
opacity, or that every finite positive value is axial depth.

The extended optional observer records RGB and alpha from the same copied texels,
retains render target/pixel shader/sampler/pose identity, and takes at most three
distinct consumer signatures per eye. It retains complete scope-end/hook-boundary
witnesses separately from capture success. The same texture can change contents
between consumers; a source pointer alone cannot identify temporal inputs.
Beyond the original census windows, eight per-session stable gameplay windows
wait two seconds after owner/generation acquisition and are spaced ten seconds
apart. Owner loss/regression requalifies time without replenishing the budget.
The structural census's own six-pair budget is unchanged. Diagnostic work and
small copied surfaces do not allocate a full-size transport or write native FX.

`tools/analyze_coj_temporal_source.py` consumes inventoried finished evidence,
checks the canonical run/build interval, rejects duplicate/malformed/overclaimed
samples and reports pair/consumer/scope identity and constant/far-value regions.
Legacy grids retain missing witnesses explicitly. All outputs retain
`temporalAdmitted:false`; no statistical threshold grants depth qualification.
Extended delivery/content, full-size paired D3D11 leases, dynamic masks and
scoped jitter/history/presentation remain open.

The subsequent extended run delivered sixty co-located RGB/alpha grids at three
consumers per eye: thirty matched signature pairs, twenty complete scopes and
eight additional stable windows, with no skipped queues or absent readbacks.
Consumers two/three match in all sampled grids. At the first right-eye consumer,
192 of 256 RGBA texels match the preceding left terminal grid in each sampled
frame. Source contents change before the next consumer; a native scene-to-HDR
StretchRect destination write lies between the sampled phases. This is evidence
that the first matching sampler alone does not prove fresh same-eye contents;
neither a constant alpha region nor an identical texel establishes its material.

The next bounded observer counts successful source write-call intents before
each sample and at scope end. RT0 draw calls retain alpha write-mask distinction
and last-writer shader identity; target clears and typed native StretchRect
destination copies are separate from diagnostic copies. The synchronous copy
hook supplies still-live COM surface arguments and actual/null-full rectangles,
so the first source copy need not wait for a sampler or earlier RT binding.
Numeric addresses in generic resource events never authorize COM calls.
Read failures withdraw completeness. Partial rectangles stay partial; counts
do not establish fragment coverage, axial depth, all MRT paths or temporal
admission. GPU and fault fixtures test bounded copied provenance and state
preservation; tests alone do not establish delivery of those witnesses.

The writer-witness run subsequently delivered fifty-one grids in seventeen
complete eye scopes, with twenty-one matched consumer/pose pairs and no lost
queued readbacks. Three selected frames have no right diagnostic scope, leaving
nine unpaired left samples; do not reinterpret that as ordinary stereo failure.
All observed first consumers have one RT0 write intent and no source copy.
The later two consumers have one additional alpha/source write: an equal-extent
FP16 StretchRect with full source/destination rectangles. All 256 RGBA samples
change across that copy in every scope; the two later consumers match without
further observed writes through scope end. Seven complete pairs retain the
earlier left-terminal/right-first 192-texel identity pattern. This identifies
the relevant native copy boundary, not axial depth or complete material coverage.

The collector now requires a complete same-size FP16 copy in the current eye's
reset ledger before consuming a candidate phase. Source dimensions and copy
write ordinal are retained with the samples. Partial/scaled/self/non-FP16
copies, later observed RT0 writes, reset or uncertainty withdraw readiness.
Consumer ordinals are assigned after this filter; no hardcoded consumer number
selects a temporal input. CPU/GPU and analyzer regressions cover these cases.
The subsequent filter run delivered sixty grids in twenty complete eye scopes,
thirty matched consumer pairs and all sixty queued readbacks. All three retained
consumers follow a complete current-eye copy and match without later observed
writes. One pair retains split alpha: the upper half is65504 and lower half1,
while lower-half RGB differs. This establishes filter delivery, not alpha-depth
semantics or material/hand/transparent/sky coverage. New bounded producer witnesses
count upstream RT0 clear/draw calls, alpha-write-mask and blending observations,
freezing the source snapshot at native copy time. Unobserved producers stay
unknown, not zero; shared RT/callback addresses grant no extra dereference or
write authority. CPU/GPU/analyzer tests cover copying/reset/uncertainty, but
actual producer-witness delivery remains open. Surfaces stay16x16; source/depth
qualification, full-size leases, dynamic masks, jitter and NGX presentation remain
unresolved.

The subsequent producer evidence observes RGB-only geometry and occasional
blend-enabled alpha draws in complete copied scopes. It does not prove blend
corruption: state counts cannot distinguish destination preservation from source
replacement or other alpha algebra, and no material/pixel association follows.
The next bounded observer records the first blend-enabled alpha draw per existing
producer entry: effective separate/non-separate alpha factors/operation, blend
factor and up to1024DWORDs of the currently owned pixel shader. Read/size failures
stay explicit, fixed function is distinct from failed shader access, and no saved
pointer is dereferenced later. The native-copy snapshot freezes those values.
The analyzer validates the bounded record, hashes complete little-endian program
bytes and describes alpha algebra while preserving missing historical data as
unknown. Additional blend draws remain counted, not silently covered by the first
witness. Host GPU/state/analyzer regressions pass; actual delivery, shader alpha
semantics and material/hand/sky/transparent coverage remain separate gates.

## Remaining Q0 work and implementation order

The optional resource diagnostic now includes a bounded structural reader in
`games/call_of_juarez/ppfx_resource_observation.*`, an installed exact-SHA bridge
and escaped value-only telemetry. It reads native Begin/End snapshots for the
selected full-eye scopes through checked own-process memory copies, without
calling native accessors, COM interfaces or resource factories. Names, topology,
aliases, requested/physical dimensions, split sampler handles and internal/default
surface values are retained. Unknown builds, root/class/list/read failures,
dirty/unallocated resources and caps keep the optional observation incomplete.

Synthetic memory fixtures establish bounded decoding and read-failure handling;
they do not establish that a copied address remains a live object. The offline
`nativePostprocess` component checks scope/order/generation, matching native
families at both endpoints and exact-value associations with observed D3D9
bindings. It retains unnamed/unmatched usage and multiple aliases explicitly.
Current target and sampler handles can change normally; stable root/family/cache
changes reject the correlation. Neither equal endpoints nor named bindings prove
atomic lifetime, intermediate state, a complete allocation graph or resize safety.
Bounded exact-game collection now supplies complete matching snapshots for both
eyes in gameplay, VR settings and immersive movies at 2560x1440. Five loaded
effects are observed (`null`, `hsm`, `scene30`, `scene30simple`, `clear_z`);
only `scene30.ppfx` owns descriptors in this capture, and all 14 correlate with
binding use in each sampled eye scope. Native default/internal color/depth and
placeholder identities also correlate. No borrowed descriptor alias is observed
in this sampled family; that absence does not remove the static alias contract.
Unmatched bindings and the unqualified additional-depth branch remain explicit.
One settings frame additionally correlates the naturally scheduled sprite/flush
pass after the eye pair with the original-size default target and exact device
generation. Other sampled frames do not supply that pass. This establishes
bounded structural delivery/order, not pixel coverage, creation/lifetime,
interior stability, programmable constants or a private bank.

1. Extend the bounded native/COM use correlation to unresolved allocation,
   additional-depth and lifetime boundaries. The sampled five effects do not
   enumerate every campaign configuration. Reject unknown effects and
   owner/generation changes for any future writer; do not dereference stale
   native handles or treat matching endpoints as atomicity.
2. Map texture-size/viewport/projection shader constants, half-texel/rounding and
   full coverage/history, including NOFSAA copies. Establish the naturally
   scheduled desktop sprite boundary and mirror ordering inside the full wrapper.
3. Define a private bank transaction that stages all required resources without
   releasing or mutating the original family. Resolve native command references,
   renderer caches, COM/native counts, failure rollback and reset/teardown
   retirement. Neither calling CreateBuffers per eye nor shallow-cloning the
   manager satisfies this requirement. An unsafe native allocation wrapper must
   not become the prototype's failure-handling layer.
4. Prove these contracts with host faults/state checks, then use the approved B0
   same-size isolation candidate. Only accepted isolation and measured memory
   allow B1. Gameplay/settings/movie and headset A/B retain their distinct gates;
   static progress does not authorize skipping them.

No code writer, live deployment or public prerelease change follows from this
map. Preserve the existing working path while the remaining contracts are closed.
