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

struct DeviceVtableHookStatus {
    bool installed = false;
    bool device_uses_hooked_vtable = false;
};

struct DeviceVtableHookContinuity {
    bool installed = false;
    bool reset_active = false;
    bool present_active = false;
    bool begin_scene_active = false;
    bool end_scene_active = false;
    void* reset_target = nullptr;
    void* present_target = nullptr;
    void* begin_scene_target = nullptr;
    void* end_scene_target = nullptr;
};

// Patches Reset and Present in the native D3D9 device vtable and, when requested,
// BeginScene/EndScene. The device vptr, COM identity, object layout and reference counting
// remain unchanged. The hook is process-lifetime for the observed implementation.
bool InstallDeviceVtableHook(
    IDirect3DDevice9* device, DeviceHookCallbacks callbacks) noexcept;

[[nodiscard]] DeviceVtableHookStatus InspectDeviceVtableHook(
    IDirect3DDevice9* device) noexcept;

[[nodiscard]] bool InstalledDeviceVtableHookActive() noexcept;

[[nodiscard]] DeviceVtableHookContinuity InspectInstalledDeviceVtableHook() noexcept;

} // namespace cojvr::backends::d3d9
