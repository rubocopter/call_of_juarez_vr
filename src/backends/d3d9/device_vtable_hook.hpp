#pragma once

#include "backends/d3d9/hook_diagnostics.hpp"
#include "backends/d3d9/hook_registry.hpp"

#include <d3d9.h>

namespace cojvr::backends::d3d9 {

struct DeviceHookCallbacks {
    void (*before_present)(IDirect3DDevice9* device) noexcept = nullptr;
    void (*after_present)(IDirect3DDevice9* device, HRESULT result) noexcept = nullptr;
    void (*before_begin_scene)(IDirect3DDevice9* device) noexcept = nullptr;
    void (*after_begin_scene)(IDirect3DDevice9* device, HRESULT result) noexcept = nullptr;
    void (*before_end_scene)(IDirect3DDevice9* device) noexcept = nullptr;
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
    void** vtable = nullptr;
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

// Patches Present in the native D3D9 device vtable plus Reset and BeginScene/EndScene
// only when their callbacks are requested. The device vptr, COM identity, object layout
// and reference counting remain unchanged.
bool InstallDeviceVtableHook(
    IDirect3DDevice9* device, DeviceHookCallbacks callbacks) noexcept;

[[nodiscard]] HookRegistryOutcome InstallDeviceVtableHookDetailed(
    IDirect3DDevice9* device, DeviceHookCallbacks callbacks) noexcept;

// Reclaims recorded slots only when every lost slot has returned to its exact
// native target. A foreign replacement is reported and never overwritten.
[[nodiscard]] HookRegistryOutcome ReacquireDeviceVtableHookDetailed(
    IDirect3DDevice9* device) noexcept;

[[nodiscard]] bool RestoreAllDeviceVtableHooks() noexcept;

[[nodiscard]] DeviceVtableHookStatus InspectDeviceVtableHook(
    IDirect3DDevice9* device) noexcept;

[[nodiscard]] bool InstalledDeviceVtableHookActive() noexcept;

[[nodiscard]] DeviceVtableHookContinuity InspectInstalledDeviceVtableHook() noexcept;

[[nodiscard]] HookDiagnostics InspectAllDeviceVtableHooks() noexcept;

} // namespace cojvr::backends::d3d9
