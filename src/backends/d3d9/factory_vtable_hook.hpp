#pragma once

#include "backends/d3d9/hook_diagnostics.hpp"
#include "backends/d3d9/hook_registry.hpp"
#include "backends/d3d9/com_trace.hpp"

#include <d3d9.h>

namespace cojvr::backends::d3d9 {

struct FactoryHookCallbacks {
    // On an Ex factory, satisfy legacy CreateDevice with CreateDeviceEx while
    // preserving the native factory identity returned by device->GetDirect3D.
    bool prefer_ex_device = false;
    void (*before_create_device)(
        IDirect3D9* factory,
        UINT adapter,
        D3DDEVTYPE device_type,
        HWND focus_window,
        DWORD behavior_flags,
        D3DPRESENT_PARAMETERS* presentation_parameters) noexcept = nullptr;
    void (*after_create_device)(
        IDirect3D9* factory,
        UINT adapter,
        D3DDEVTYPE device_type,
        HWND focus_window,
        DWORD behavior_flags,
        D3DPRESENT_PARAMETERS* presentation_parameters,
        IDirect3DDevice9** returned_device,
        HRESULT result) noexcept = nullptr;
    ComTraceCallback com_trace = nullptr;
};

[[nodiscard]] bool InstallFactoryVtableHook(
    IDirect3D9* factory, FactoryHookCallbacks callbacks) noexcept;

[[nodiscard]] HookRegistryOutcome InstallFactoryVtableHookDetailed(
    IDirect3D9* factory, FactoryHookCallbacks callbacks) noexcept;

[[nodiscard]] bool FactoryVtableHookActive(IDirect3D9* factory) noexcept;

[[nodiscard]] bool RestoreFactoryVtableHook(IDirect3D9* factory) noexcept;

[[nodiscard]] HookDiagnostics InspectAllFactoryVtableHooks() noexcept;

} // namespace cojvr::backends::d3d9
