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

The engine's session log confirms a DX9 level load, roughly 42 seconds of
play and more than 4,000 rendered frames with ordinary shutdown. The probe
recorded successful factory/device hook installation but only three resource
creations: a 16x16 `A8R8G8B8` MANAGED texture (`levels=0`), a 128x128
`A8R8G8B8` MANAGED cube texture (`levels=1`), and a 262,144-byte
`D3DPOOL_DEFAULT` dynamic/write-only vertex buffer. Those are observations,
not a representative resource census. Hook displacement, another device or
another resource-creation path are possible explanations for the low count;
none has been demonstrated. The later startup census below supplies the missing
VB/IB evidence; no volume-texture creation has been observed.

Microsoft's [D3DPOOL contract](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dpool)
states that `D3DPOOL_MANAGED` is valid for `IDirect3DDevice9` but invalid for
`IDirect3DDevice9Ex`. [Managed resources](https://learn.microsoft.com/en-us/windows/win32/direct3d9/managing-resources)
also carry system-memory backing and lost-device persistence; changing only
the pool at creation would not by itself preserve those behaviors.

## Independent startup census with immediate hook restoration

**Live-tested (research probe, same host/build):** an independent native
classic-D3D9 probe restored its device creation/observer hooks immediately
after `BeginStateBlock`, which was observed rewriting the device vtable.
In a bounded startup window it recorded 1,552 successful creations across
the five instrumented creation APIs, 39 hook refreshes and an early summary
of 120 successful Presents. All recorded allocation HRESULTs were `S_OK`;
the original allocation parameters were preserved. This establishes much
broader startup coverage than the earlier three-record gameplay capture,
but does not complete a gameplay census or measure locking, updates,
destruction, residency or reset recovery. No volume-texture call occurred.
A traced repeat also passes with 1,538 successful creations, eleven MANAGED
calls, 39 hook refreshes and the same 120-Present summary. Its exception
tracer records zero access violations. Total video/SYSTEMMEM allocation
counts vary with the observed window; neither summary is a final frame count.

| Creation kind | DEFAULT | MANAGED | SYSTEMMEM |
| --- | ---: | ---: | ---: |
| 2D texture | 33 | 8 | 1,507 |
| Cube texture | 0 | 1 | 0 |
| Vertex buffer | 1 | 1 | 0 |
| Index buffer | 0 | 1 | 0 |

The eleven MANAGED calls comprise ten distinct allocation profiles:

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

**Historical live-tested allocation step:** the independent
Ex-backed probe rejected the first five profiles with `D3DERR_INVALIDCALL`
and retried those exact calls as DEFAULT with `S_OK`/non-null results.
The tracked VB completed 24 Lock/Unlock pairs. The following 1x1 MANAGED
texture still failed and Chrome dereferenced its null pointer at
`+0x23E3FD`. This was the point that rejected one-size-at-a-time retries and
motivated the semantic adapter described below.

The engine identity is `ChromeEngine3.dll` version `1.1.1.0st`, SHA-256
`DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8`;
the game executable SHA-256 is
`5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`.
Evidence is from an independent research build, not this mod's candidate.
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

**Live-tested production startup/reset:** a manual exact-game run retains Ex
factory/device identity, records successful Presents beyond frame 2,970,
completes a successful reset into generation 2 and continues flat capture.
All 380 logged translated texture results (332 2D, 48 cube) return S_OK.
This production trace has no individual VB/IB telemetry, so it does not extend
their allocation/Lock coverage beyond the host test. Outer hooks restore and
the proxy finalizes; the inner presenter does not finish shutdown. The first
production run rejected flat frames before upload/submission. A subsequent
manual startup run now passes with geometry-derived texture extents: logged
PS VR2 optics confirm the asymmetric clipping cause, frames upload/submit without
rejection, and the operator sees videos/menu plus a pointer in the visor.
The extent correction is live-tested and startup visibility headset-validated;
pointer usability remains open. Production shared native-stereo
publication, D3D11 copy and explicit-pose submission have since been live-tested
in gameplay. Pre-`DestroyGame` normal-quit shutdown, GPU drain and complete
finalization are now live-tested; see [shutdown boundary](COJ_SHUTDOWN_BOUNDARY.md). Production
texture contents/update/reset ownership, device loss and broader gameplay remain
unproved. The bounded shared-transport cadence target now passes near the
readback-off reference, with operator-confirmed stereo depth/head-turn stability;
sustained pacing and pending-frame reset/device loss remain separate gates.

**Live-tested research semantic adapter and reset:** the independent LTR path
retries observed usage-0 MANAGED 2D/cube textures as
`D3DPOOL_DEFAULT | D3DUSAGE_DYNAMIC` and observed MANAGED WRITEONLY VB/IB
requests as `D3DPOOL_DEFAULT` while preserving their usage and metadata. In the
bounded real-game run all nine observed MANAGED creations adapt successfully,
118 CPU-write lock traces return non-null data, 39 `BeginStateBlock` refreshes
retain the hook set, and the run reaches 118 EndScenes / 120 Presents with zero
exception markers. A subsequent engine-driven reset keeps all nine tracked
adapted resources alive. Four directly lockable textures have identical level-0
hashes before and after reset, and a generation-1 texture is still bound at stage
0 by generation-2 draws. The same LTR experiment cancels a stalled two-slot
transport generation at reset and completes 12/12 submissions in the replacement
generation with ready=done=12, 12 D3D12 consumer copies and zero sampled
mismatches. This is scoped evidence for this exact build/host; device loss,
non-lockable resources, broader gameplay, visual correctness and the production
mod candidate remain separate gates.

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
normally. The analyzer writes the raw per-call CSV and a summary under ignored
`work/evidence/d3d9_pool_probe/<run-id>/`.

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
