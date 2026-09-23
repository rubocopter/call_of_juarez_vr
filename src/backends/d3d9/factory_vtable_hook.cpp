#include "backends/d3d9/factory_vtable_hook.hpp"

#include "backends/d3d9/hook_registry.hpp"

#include <array>
#include <mutex>
#include <span>
#include <unordered_map>

namespace cojvr::backends::d3d9 {
namespace {

constexpr std::size_t kCreateDeviceIndex = 16;

using CreateDeviceFn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
    D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);

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

    const HRESULT result = original(
        factory, adapter, device_type, focus_window, behavior_flags,
        presentation_parameters, returned_device);
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
    if (!vtable || (!callbacks.before_create_device && !callbacks.after_create_device)) return failed;
    if (!PinModuleForAddress(reinterpret_cast<void*>(&HookCreateDevice))) {
        failed.result = HookRegistryResult::ProtectionFailure;
        return failed;
    }

    try {
        std::lock_guard lock(g_callbacks_mutex);
        g_callbacks_by_vtable.insert_or_assign(vtable, callbacks);
        const std::array requests{
            HookSlotRequest{kCreateDeviceIndex, reinterpret_cast<void*>(&HookCreateDevice)}};
        const HookRegistryOutcome outcome =
            g_registry.Install(vtable, std::span<const HookSlotRequest>(requests));
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
    return slots.size() == 1 && slots.front().index == kCreateDeviceIndex &&
        slots.front().owned;
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
                    .slot_name = slot.index == kCreateDeviceIndex ? "CreateDevice" : "unknown",
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
