# ChromeEngine 3: classic D3D9 resource pool census

## Status

A classic-D3D9 gameplay session was measured. It proves that the inspected
ChromeEngine requests `D3DPOOL_MANAGED` for both a 2D texture and a cube
texture, and both creations succeed on a classic device. The capture is too
sparse to quantify overall MANAGED prevalence or inventory every resource
type. A verbatim D3D9Ex device replacement is incompatible with these two
observed requests; the amount of emulation needed remains undetermined.

The engine's session log confirms a DX9 level load, roughly 42 seconds of
play and more than 4,000 rendered frames with ordinary shutdown. The probe
recorded successful factory/device hook installation but only three resource
creations: a 16x16 `A8R8G8B8` MANAGED texture (`levels=0`), a 128x128
`A8R8G8B8` MANAGED cube texture (`levels=1`), and a 262,144-byte
`D3DPOOL_DEFAULT` dynamic/write-only vertex buffer. Those are observations,
not a representative resource census. Hook displacement, another device or
another resource-creation path are possible explanations for the low count;
none has been demonstrated. No conclusion about managed vertex/index/volume
resources follows from their absence in this capture.

Microsoft's [D3DPOOL contract](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dpool)
states that `D3DPOOL_MANAGED` is valid for `IDirect3DDevice9` but invalid for
`IDirect3DDevice9Ex`. [Managed resources](https://learn.microsoft.com/en-us/windows/win32/direct3d9/managing-resources)
also carry system-memory backing and lost-device persistence; changing only
the pool at creation would not by itself preserve those behaviors.

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
