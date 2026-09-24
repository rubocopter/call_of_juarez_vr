#include "backends/d3d9/factory_vtable_hook.hpp"

#include "backends/d3d9/hook_registry.hpp"

#include <algorithm>
#include <array>
#include <mutex>
#include <span>
#include <unordered_map>

namespace cojvr::backends::d3d9 {
namespace {

constexpr std::size_t kCreateDeviceIndex = 16;
constexpr std::size_t kQueryInterfaceIndex = 0;
constexpr std::size_t kAddRefIndex = 1;
constexpr std::size_t kReleaseIndex = 2;

using CreateDeviceFn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
    D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
using QueryInterfaceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*, REFIID, void**);
using RefFn = ULONG(STDMETHODCALLTYPE*)(IDirect3D9*);

HookRegistry g_registry{};
std::mutex g_callbacks_mutex;
std::unordered_map<void**, FactoryHookCallbacks> g_callbacks_by_vtable;

void** FactoryVtable(IDirect3D9* factory) noexcept {
    return factory ? *reinterpret_cast<void***>(factory) : nullptr;
}

FactoryHookCallbacks CallbacksFor(void** vtable) noexcept {
    try {
        std::lock_guard lock(g_callbacks_mutex);
        const auto callbacks = g_callbacks_by_vtable.find(vtable);
        return callbacks == g_callbacks_by_vtable.end()
            ? FactoryHookCallbacks{}
            : callbacks->second;
    } catch (...) {
        return {};
    }
}

HRESULT STDMETHODCALLTYPE HookQueryInterface(IDirect3D9* factory, REFIID iid, void** object) {
    void** vtable = FactoryVtable(factory);
    const auto original = reinterpret_cast<QueryInterfaceFn>(
        g_registry.OriginalTarget(vtable, kQueryInterfaceIndex));
    if (!original) return E_NOINTERFACE;
    const auto callbacks = CallbacksFor(vtable);
    if (callbacks.com_trace) callbacks.com_trace(factory, "query_interface_enter", &iid, nullptr, S_OK, 0);
    const HRESULT result = original(factory, iid, object);
    if (callbacks.com_trace) callbacks.com_trace(
        factory, "query_interface_result", &iid,
        SUCCEEDED(result) && object ? *object : nullptr, result, 0);
    return result;
}

ULONG STDMETHODCALLTYPE HookAddRef(IDirect3D9* factory) {
    void** vtable = FactoryVtable(factory);
    const auto original = reinterpret_cast<RefFn>(
        g_registry.OriginalTarget(vtable, kAddRefIndex));
    if (!original) return 0;
    const ULONG count = original(factory);
    const auto callbacks = CallbacksFor(vtable);
    if (callbacks.com_trace) callbacks.com_trace(factory, "add_ref", nullptr, nullptr, S_OK, count);
    return count;
}

ULONG STDMETHODCALLTYPE HookRelease(IDirect3D9* factory) {
    void** vtable = FactoryVtable(factory);
    const auto callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<RefFn>(
        g_registry.OriginalTarget(vtable, kReleaseIndex));
    if (!original) return 0;
    const ULONG count = original(factory);
    if (callbacks.com_trace) callbacks.com_trace(factory, "release", nullptr, nullptr, S_OK, count);
    return count;
}

HRESULT CreateExDevice(
    IDirect3D9* factory, UINT adapter, D3DDEVTYPE device_type,
    HWND focus_window, DWORD behavior_flags,
    D3DPRESENT_PARAMETERS* presentation_parameters,
    IDirect3DDevice9** returned_device) noexcept {
    if (!presentation_parameters || !returned_device) return D3DERR_INVALIDCALL;
    *returned_device = nullptr;
    IDirect3D9Ex* ex_factory = nullptr;
    const HRESULT query_result = factory->QueryInterface(
        IID_IDirect3D9Ex, reinterpret_cast<void**>(&ex_factory));
    if (FAILED(query_result) || !ex_factory) return FAILED(query_result) ? query_result : E_NOINTERFACE;

    D3DDISPLAYMODEEX fullscreen_mode{};
    D3DDISPLAYMODEEX* fullscreen_mode_ptr = nullptr;
    if (!presentation_parameters->Windowed) {
        fullscreen_mode.Size = sizeof(fullscreen_mode);
        fullscreen_mode.Width = presentation_parameters->BackBufferWidth;
        fullscreen_mode.Height = presentation_parameters->BackBufferHeight;
        fullscreen_mode.RefreshRate = presentation_parameters->FullScreen_RefreshRateInHz;
        fullscreen_mode.Format = presentation_parameters->BackBufferFormat;
        fullscreen_mode.ScanLineOrdering = D3DSCANLINEORDERING_UNKNOWN;
        fullscreen_mode_ptr = &fullscreen_mode;
    }

    IDirect3DDevice9Ex* ex_device = nullptr;
    const HRESULT result = ex_factory->CreateDeviceEx(
        adapter, device_type, focus_window, behavior_flags,
        presentation_parameters, fullscreen_mode_ptr, &ex_device);
    ex_factory->Release();
    if (SUCCEEDED(result) && ex_device) {
        *returned_device = static_cast<IDirect3DDevice9*>(ex_device);
        return result;
    }
    if (ex_device) ex_device->Release();
    return FAILED(result) ? result : E_FAIL;
}

HRESULT STDMETHODCALLTYPE HookCreateDevice(
    IDirect3D9* factory,
    UINT adapter,
    D3DDEVTYPE device_type,
    HWND focus_window,
    DWORD behavior_flags,
    D3DPRESENT_PARAMETERS* presentation_parameters,
    IDirect3DDevice9** returned_device) {
    void** vtable = FactoryVtable(factory);
    const FactoryHookCallbacks callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<CreateDeviceFn>(
        g_registry.OriginalTarget(vtable, kCreateDeviceIndex));
    if (!original) return D3DERR_INVALIDCALL;

    if (callbacks.before_create_device) {
        callbacks.before_create_device(
            factory, adapter, device_type, focus_window, behavior_flags,
            presentation_parameters);
    }

    HRESULT result = callbacks.prefer_ex_device
        ? CreateExDevice(factory, adapter, device_type, focus_window,
            behavior_flags, presentation_parameters, returned_device)
        : original(factory, adapter, device_type, focus_window,
            behavior_flags, presentation_parameters, returned_device);
    if (callbacks.prefer_ex_device && FAILED(result)) {
        result = original(factory, adapter, device_type, focus_window,
            behavior_flags, presentation_parameters, returned_device);
    }
    if (callbacks.after_create_device) {
        callbacks.after_create_device(
            factory, adapter, device_type, focus_window, behavior_flags,
            presentation_parameters, returned_device, result);
    }
    return result;
}

} // namespace

bool InstallFactoryVtableHook(
    IDirect3D9* factory, FactoryHookCallbacks callbacks) noexcept {
    const HookRegistryOutcome outcome = InstallFactoryVtableHookDetailed(factory, callbacks);
    return outcome.result == HookRegistryResult::Installed ||
        outcome.result == HookRegistryResult::AlreadyInstalled;
}

HookRegistryOutcome InstallFactoryVtableHookDetailed(
    IDirect3D9* factory, FactoryHookCallbacks callbacks) noexcept {
    HookRegistryOutcome failed{};
    void** vtable = FactoryVtable(factory);
    if (!vtable || (!callbacks.before_create_device && !callbacks.after_create_device &&
                    !callbacks.prefer_ex_device)) return failed;
    if (!PinModuleForAddress(reinterpret_cast<void*>(&HookCreateDevice))) {
        failed.result = HookRegistryResult::ProtectionFailure;
        return failed;
    }

    try {
        std::lock_guard lock(g_callbacks_mutex);
        g_callbacks_by_vtable.insert_or_assign(vtable, callbacks);
        std::array<HookSlotRequest, 4> requests{};
        std::size_t request_count = 0;
        if (callbacks.com_trace) {
            requests[request_count++] = {kQueryInterfaceIndex, reinterpret_cast<void*>(&HookQueryInterface)};
            requests[request_count++] = {kAddRefIndex, reinterpret_cast<void*>(&HookAddRef)};
            requests[request_count++] = {kReleaseIndex, reinterpret_cast<void*>(&HookRelease)};
        }
        requests[request_count++] = {kCreateDeviceIndex, reinterpret_cast<void*>(&HookCreateDevice)};
        const HookRegistryOutcome outcome =
            g_registry.Install(vtable, std::span<const HookSlotRequest>(requests.data(), request_count));
        const bool success = outcome.result == HookRegistryResult::Installed ||
            outcome.result == HookRegistryResult::AlreadyInstalled;
        if (!success && !outcome.ownership_record_retained) {
            g_callbacks_by_vtable.erase(vtable);
        }
        return outcome;
    } catch (...) {
        failed.result = HookRegistryResult::RollbackIncomplete;
        failed.ownership_record_retained = true;
        return failed;
    }
}

bool FactoryVtableHookActive(IDirect3D9* factory) noexcept {
    void** vtable = FactoryVtable(factory);
    if (!vtable) return false;
    const auto slots = g_registry.Inspect(vtable);
    return !slots.empty() && std::all_of(slots.begin(), slots.end(),
        [](const HookSlotStatus& slot) { return slot.owned; }) &&
        std::any_of(slots.begin(), slots.end(), [](const HookSlotStatus& slot) {
            return slot.index == kCreateDeviceIndex;
        });
}

bool RestoreFactoryVtableHook(IDirect3D9* factory) noexcept {
    void** vtable = FactoryVtable(factory);
    if (!vtable) return false;
    const HookRegistryOutcome outcome = g_registry.Restore(vtable);
    const bool restored = outcome.result == HookRegistryResult::Installed ||
        outcome.result == HookRegistryResult::AlreadyInstalled;
    if (restored) {
        try {
            std::lock_guard lock(g_callbacks_mutex);
            g_callbacks_by_vtable.erase(vtable);
        } catch (...) {
        }
    }
    return restored;
}

HookDiagnostics InspectAllFactoryVtableHooks() noexcept {
    HookDiagnostics diagnostics;
    try {
        for (void** vtable : g_registry.RegisteredVtables()) {
            for (const HookSlotStatus& slot : g_registry.Inspect(vtable)) {
                diagnostics.push_back(HookSlotDiagnostic{
                    .interface_name = "IDirect3D9",
                    .slot_name = slot.index == kCreateDeviceIndex ? "CreateDevice" :
                        slot.index == kQueryInterfaceIndex ? "QueryInterface" :
                        slot.index == kAddRefIndex ? "AddRef" :
                        slot.index == kReleaseIndex ? "Release" : "unknown",
                    .vtable = vtable,
                    .index = slot.index,
                    .original = slot.original,
                    .replacement = slot.replacement,
                    .current = slot.current,
                    .owned = slot.owned,
                });
            }
        }
    } catch (...) {
    }
    return diagnostics;
}

} // namespace cojvr::backends::d3d9
