#pragma once

#include "backends/d3d9/hook_diagnostics.hpp"
#include "backends/d3d9/hook_registry.hpp"
#include "backends/d3d9/com_trace.hpp"
#include "backends/d3d9/render_resource_probe.hpp"

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
    ComTraceCallback com_trace = nullptr;
    void (*method_trace)(
        IDirect3DDevice9* device, const char* method, bool entering, HRESULT result) noexcept = nullptr;
    // Borrowed factory. The owner must retain it until this device hook is restored.
    IDirect3D9* get_direct3d_factory = nullptr;
    IDirect3DDevice9* get_direct3d_device = nullptr;
    void (*resource_probe_event)(IDirect3DDevice9*, const RenderResourceEvent&) noexcept = nullptr;
    void (*resource_probe_call)(IDirect3DDevice9*, bool entering) noexcept = nullptr;
    // Synchronous borrowed arguments at the real native copy boundary, after
    // forwarding and before the caller can retire them. Never retain them.
    void (*resource_probe_copy)(IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,
        IDirect3DSurface9*,const RECT*,HRESULT) noexcept = nullptr;
    // Separate bounded HUD diagnostic ingress. Register only with the existing
    // render-resource opt-in AND a qualified natural sprite/flush coordinator.
    // Entry observes actual pre-draw bindings; exit supplies the native result.
    // No callback/collector lock may survive into original forwarding.
    void (*hud_batch_draw)(IDirect3DDevice9*, const RenderResourceEvent&, bool entering) noexcept = nullptr;
    void (*hud_batch_call)(IDirect3DDevice9*, bool entering) noexcept = nullptr;
    // Source retirement on the caller thread, before BOTH Reset entry points.
    // False defers forwarding while an owned rollback remains incomplete.
    bool (*allow_reset)(IDirect3DDevice9*) noexcept = nullptr;
    void (*after_reset_ex)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*,HRESULT) noexcept = nullptr;
};

struct DeviceVtableHookStatus {
    bool installed = false;
    bool device_uses_hooked_vtable = false;
};
struct ResourceProbeHookContinuity {
    std::uint64_t epoch=0;
    bool intact=false;
};
[[nodiscard]] ResourceProbeHookContinuity InspectResourceProbeHookContinuity(IDirect3DDevice9*) noexcept;
// Fixed-slot, allocation-free, nonblocking check for the HUD draw ingress.
[[nodiscard]] ResourceProbeHookContinuity TryInspectHudBatchHookContinuity(IDirect3DDevice9*) noexcept;

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
