#pragma once

#include <d3d9.h>

namespace cojvr::backends::d3d9 {

struct DeviceHookCallbacks {
    void (*before_present)(IDirect3DDevice9* device) noexcept = nullptr;
    void (*after_present)(IDirect3DDevice9* device, HRESULT result) noexcept = nullptr;
    void (*after_begin_scene)(IDirect3DDevice9* device, HRESULT result) noexcept = nullptr;
    void (*after_end_scene)(IDirect3DDevice9* device, HRESULT result) noexcept = nullptr;
    void (*before_reset)(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters) noexcept = nullptr;
    void (*after_reset)(
        IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters, HRESULT result) noexcept = nullptr;
};

// Patches Reset and Present in the native D3D9 device vtable and, when requested,
// BeginScene/EndScene. The device vptr, COM identity, object layout and reference counting
// remain unchanged. The hook is process-lifetime for the observed implementation.
bool InstallDeviceVtableHook(
    IDirect3DDevice9* device, DeviceHookCallbacks callbacks) noexcept;

} // namespace cojvr::backends::d3d9
