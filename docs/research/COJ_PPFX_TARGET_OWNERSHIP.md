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

## Remaining Q0 work and implementation order

1. Correlate native effect/container/index/handle, actual COM interfaces and the
   internal/default/depth surfaces with existing scoped draw evidence. Reject
   unknown loaded effects and owner/generation changes; do not dereference stale
   native handles. Static layouts are candidates for a bounded read-only adapter,
   not live-validated memory contracts.
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
