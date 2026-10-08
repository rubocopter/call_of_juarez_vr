# ChromeEngine 3: classic D3D9 resource pool census

## Status

A classic-D3D9 gameplay session first proved that the inspected ChromeEngine
requests `D3DPOOL_MANAGED` resources that a verbatim D3D9Ex device cannot
create. A later bounded startup census establishes the broader observed profile:
MANAGED 2D textures, one cube, one vertex buffer and one index buffer. A separate
LTR semantic-adaptation run on the same build/host then reaches sustained
startup and a real reset with those observed resource classes adapted. The
earlier gameplay capture remains sparse and no complete gameplay, device-loss or
generic MANAGED-emulation claim follows from these results.

Classic D3D9 is the compatibility reference. Observed MANAGED creation succeeds
there but is invalid on D3D9Ex. The startup profiles below define the production
translation scope; no volume-texture creation has been observed. Counts describe
requests in a bounded window, not simultaneously resident resources.

Microsoft's [D3DPOOL contract](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dpool)
states that `D3DPOOL_MANAGED` is valid for `IDirect3DDevice9` but invalid for
`IDirect3DDevice9Ex`. [Managed resources](https://learn.microsoft.com/en-us/windows/win32/direct3d9/managing-resources)
also carry system-memory backing and lost-device persistence; changing only
the pool at creation would not by itself preserve those behaviors.

## Independent startup census with immediate hook restoration

**Live-tested research probe:** restoring creation/observer hooks immediately
after BeginStateBlock establishes MANAGED 2D/cube/VB/IB profiles with successful
classic-D3D9 allocations. BeginStateBlock rewrites the observed device vtable;
a 100 ms watchdog cannot guarantee coverage before the next native call. The
bounded census does not measure complete gameplay locking, destruction, residency
or device-loss recovery.

The bounded census contains these ten distinct MANAGED allocation profiles:

| Kind | Extent / length | Levels | Usage | Format / FVF | Calls |
| --- | --- | ---: | --- | --- | ---: |
| 2D texture | 16x16 | 0 | 0 | A8R8G8B8 | 1 |
| Cube texture | edge 128 | 1 | 0 | A8R8G8B8 | 1 |
| Vertex buffer | 262,144 bytes | n/a | 0x8 | FVF=0 | 1 |
| Index buffer | 131,072 bytes | n/a | 0x8 | INDEX16 | 1 |
| 2D texture | 64x64 | 0 | 0 | DXT1 | 1 |
| 2D texture | 1x1 | 1 | 0 | A8R8G8B8 | 1 |
| 2D texture | 2x2 | 0 | 0 | A4R4G4B4 | 1 |
| 2D texture | 256x128 | 1 | 0 | A8R8G8B8 | 1 |
| 2D texture | 1024x512 | 1 | 0 | X8R8G8B8 | 2 |
| 2D texture | 512x256 | 1 | 0 | A8R8G8B8 | 1 |

Per-size Ex retries were rejected: translating a few allocations still left
later MANAGED requests invalid and the engine dereferenced a null texture at
`+0x23E3FD`. Compatibility must cover the observed resource semantics rather
than a growing whitelist of dimensions.

Production gameplay loading also exposes this native null-dereference boundary
after translated 1024x1024 DXT1 and DXT5 texture creations return
`D3DERR_OUTOFVIDEOMEMORY` (`0x8876017C`). This establishes an allocation-failure
boundary, not the source of resource pressure, a leak or a recenter cause.
The repeated narrated campaign transition reaches the same `+0x23E3FD` null
read through `LoadModule -> LoadCurrentLevel -> LoadLevelAfterFade`, including
a repetition without input during loading. The native log also reports a
failed temporary backbuffer surface with `E_OUTOFMEMORY` before the transition's
save and the later `CreateTexture` failure. These are two observed allocation
failures; the log does not establish common ownership or their pressure source.
The audited original/system-D3D9 loading crash at `+0x22F262` is a different
fault boundary and cannot be substituted for this failure's provenance.

WER virtual-size counters establish address-space pressure close to the original
x86 2 GiB ceiling, with a recorded peak leaving approximately 25 MiB. They do not
supply a complete VirtualQuery map or prove fragmentation, resource ownership,
a leak or VRAM exhaustion. Failure-handling exceptions must not replace the
initial native allocation/null-read boundary in diagnosis.

**Host-tested capacity mitigation:** an optional native-stereo deployment sets
only `IMAGE_FILE_LARGE_ADDRESS_AWARE` at file byte `0x10E` (`0x0E -> 0x2E`).
Original SHA-256 remains `5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`;
the exact derivative is `C8B8BB82FCB3D6599C5F77B1BB9CB3444CBB3A360461DAD43AD808B49AD28DC9`.
Unknown originals fail closed; no other byte, section, code or offset changes.
On 64-bit Windows this raises x86 user address capacity from 2 to 4 GiB per
[Microsoft's address-space limits](https://learn.microsoft.com/en-us/windows/win32/memory/memory-limits-for-windows-releases).
It does not increase VRAM or demonstrate a performance improvement. Synthetic
x86 high-address allocation and exact-copy transactional failure/restore checks
pass; the previously failing narrated transition has since been operator-accepted
with the derivative. General native/JVM high-address compatibility and broader
loading stability remain separate gates. Bounded production diagnostics
sample total/free VA, largest free region and private commit without retrying,
freeing or altering resource policy.
Successful creation of preceding textures and bounded startup/Reset tests do
not accept sustained loading or device-loss recovery. Preserve the failure and
exact-build provenance; arbitrary per-size retries do not resolve this contract.

The engine identity is `ChromeEngine3.dll` version `1.1.1.0st`, SHA-256
`DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8`;
the game executable SHA-256 is
`5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`.
The startup census is from an independent research probe; production integration
coverage is distinguished below.
Raw run evidence and provenance remain under ignored `work/`.

**Implemented / host-tested integration:** the production Ex compatibility
layer now covers the exact observed MANAGED WRITEONLY VB/IB profiles. VB/IB
requests are mapped to DEFAULT while their usage, FVF/format and sizes are
preserved; the focused test verifies successful allocation and Lock/Unlock.
Before this change the same test reproduced the Ex `D3DERR_INVALIDCALL` on the
262,144-byte vertex buffer. The same test also reproduced creation-hook loss
after BeginStateBlock before the watchdog could run; production now hooks
BeginStateBlock and immediately asks the existing HookRegistry to reacquire
all owned slots on successful return. This preserves the registry's existing
ownership/conflict rules. Slots 26, 27 and 60 are compile-time checked in the
vtable-layout test. Texture compatibility remains DEFAULT|DYNAMIC.

**Production coverage:** exact-game Ex startup and a real Reset retain device
identity and continue Present/flat capture. Geometry-derived texture extents
restore startup videos/menu visibility; the native pointer is physically accepted
for exercised ordinary/modal/pause menus. Shared native-stereo publication,
D3D11 copy and explicit-pose submission are live-tested in gameplay. The bounded
cadence target, stereo depth and head-turn stability with recording off have
operator acceptance. Normal-quit inner shutdown/GPU drain/finalization are
live-tested through the [shutdown boundary](COJ_SHUTDOWN_BOUNDARY.md).

Texture contents/update/reset ownership, pending-frame reset, device loss,
sustained tail latency, recording compatibility and broader gameplay remain
separate gates. Production traces do not extend VB/IB Lock coverage beyond the
focused host tests or establish generic MANAGED emulation.

**Independent semantic-adapter evidence:** an Ex-backed research path maps
usage-0 MANAGED 2D/cube textures to DEFAULT|DYNAMIC and WRITEONLY MANAGED VB/IB
to DEFAULT, preserving metadata. Bounded native allocation/CPU-write locks and
an engine-driven Reset succeed. Directly lockable level-0 texture hashes remain
unchanged across Reset and an older texture remains bound by newer-generation
draws. Its separate transport cancels a stalled generation and completes copies
in the replacement generation. This scoped research result does not accept
production texture-content/reset semantics, non-lockable resources or device loss.

## Measurement contract

`d3d9_pool_probe.dll` forwards `Direct3DCreate9` to the system's classic D3D9
runtime. It returns the system factory and system device. It patches only the
factory's `CreateDevice` entry and five device creation entries:
`CreateTexture`, `CreateCubeTexture`, `CreateVolumeTexture`,
`CreateVertexBuffer`, and `CreateIndexBuffer`. Each hook calls the original
method once with the same arguments, preserves its HRESULT and output pointer,
then records the request. There are no COM wrappers, D3D9Ex calls, resource
substitutions, runtime changes or changes to the VR renderer.

The CSV records resource kind, pool, raw usage flags, raw format, requested
dimensions or buffer length, level count, vertex FVF, whether a shared-handle
parameter was supplied, HRESULT and device pointer. `FactoryHook` and
`DeviceHook` records establish hook coverage. Unknown format and usage values
remain available as raw numbers. Buffer creation does not accept a texture
format; `D3DFMT_UNKNOWN` is recorded for vertex buffers.

The exact inspected `CoJ.exe` and `ChromeEngine3.dll` SHA-256 hashes are
required at staging. A fresh run ID and build manifest bind the staged DLL to
the capture. The operator must launch the game, reach gameplay and exit
normally. The analyzer writes the raw per-call CSV and a summary under
local ignored evidence storage.

## Interpretation boundary

The count measures creation requests, including failures and repeated
allocations. It does not measure simultaneous live resources, GPU residency,
memory consumption, locks, updates or lost-device/reset behavior. A high
`MANAGED` count would quantify the resource-creation incompatibility with
D3D9Ex; it would not by itself prove that replacing that pool with `DEFAULT`
preserves ChromeEngine semantics. That would require separate evidence about
CPU copies, lock/update paths and reset recovery for every affected resource
type. A low count would leave other possible D3D9Ex semantic differences open.

Reference for the style of this census: [Singularity VR's D3D9 pool probe](https://github.com/letsgosportsteam/singularity-vr-mod/tree/main/spikes/d3d9_pool_probe).
