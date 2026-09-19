#include "backends/d3d9/device_vtable_hook.hpp"

#include "backends/d3d9/hook_registry.hpp"

#include <array>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>

namespace cojvr::backends::d3d9 {
namespace {

constexpr std::size_t kResetIndex = 16;
constexpr std::size_t kPresentIndex = 17;
constexpr std::size_t kBeginSceneIndex = 41;
constexpr std::size_t kEndSceneIndex = 42;

using ResetFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using BeginSceneFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using EndSceneFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*);

HookRegistry g_registry{};
std::mutex g_callbacks_mutex;
std::unordered_map<void**, DeviceHookCallbacks> g_callbacks_by_vtable;

void** DeviceVtable(IDirect3DDevice9* device) noexcept {
    return device ? *reinterpret_cast<void***>(device) : nullptr;
}

DeviceHookCallbacks CallbacksFor(void** vtable) noexcept {
    try {
        std::lock_guard lock(g_callbacks_mutex);
        const auto callbacks = g_callbacks_by_vtable.find(vtable);
        return callbacks == g_callbacks_by_vtable.end()
            ? DeviceHookCallbacks{}
            : callbacks->second;
    } catch (...) {
        return {};
    }
}

HRESULT STDMETHODCALLTYPE HookReset(
    IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters) {
    void** vtable = DeviceVtable(device);
    const DeviceHookCallbacks callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<ResetFn>(
        g_registry.OriginalTarget(vtable, kResetIndex));
    if (!original) return D3DERR_INVALIDCALL;
    if (callbacks.before_reset) callbacks.before_reset(device, parameters);
    const HRESULT result = original(device, parameters);
    if (callbacks.after_reset) callbacks.after_reset(device, parameters, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookPresent(
    IDirect3DDevice9* device, const RECT* source_rect, const RECT* destination_rect,
    HWND destination_window_override, const RGNDATA* dirty_region) {
    void** vtable = DeviceVtable(device);
    const DeviceHookCallbacks callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<PresentFn>(
        g_registry.OriginalTarget(vtable, kPresentIndex));
    if (!original) return D3DERR_INVALIDCALL;
    if (callbacks.before_present) callbacks.before_present(device);
    const HRESULT result = original(
        device, source_rect, destination_rect, destination_window_override, dirty_region);
    if (callbacks.after_present) callbacks.after_present(device, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookBeginScene(IDirect3DDevice9* device) {
    void** vtable = DeviceVtable(device);
    const DeviceHookCallbacks callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<BeginSceneFn>(
        g_registry.OriginalTarget(vtable, kBeginSceneIndex));
    if (!original) return D3DERR_INVALIDCALL;
    if (callbacks.before_begin_scene) callbacks.before_begin_scene(device);
    const HRESULT result = original(device);
    if (callbacks.after_begin_scene) callbacks.after_begin_scene(device, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookEndScene(IDirect3DDevice9* device) {
    void** vtable = DeviceVtable(device);
    const DeviceHookCallbacks callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<EndSceneFn>(
        g_registry.OriginalTarget(vtable, kEndSceneIndex));
    if (!original) return D3DERR_INVALIDCALL;
    if (callbacks.before_end_scene) callbacks.before_end_scene(device);
    const HRESULT result = original(device);
    if (callbacks.after_end_scene) callbacks.after_end_scene(device, result);
    return result;
}

const HookSlotStatus* FindSlot(
    const std::vector<HookSlotStatus>& slots, std::size_t index) noexcept {
    for (const HookSlotStatus& slot : slots) {
        if (slot.index == index) return &slot;
    }
    return nullptr;
}

DeviceVtableHookContinuity ContinuityFor(void** vtable) noexcept {
    DeviceVtableHookContinuity continuity{};
    continuity.vtable = vtable;
    const std::vector<HookSlotStatus> slots = g_registry.Inspect(vtable);
    if (slots.empty()) return continuity;

    continuity.installed = true;
    const HookSlotStatus* reset = FindSlot(slots, kResetIndex);
    const HookSlotStatus* present = FindSlot(slots, kPresentIndex);
    const HookSlotStatus* begin_scene = FindSlot(slots, kBeginSceneIndex);
    const HookSlotStatus* end_scene = FindSlot(slots, kEndSceneIndex);
    continuity.reset_active = reset == nullptr || reset->owned;
    if (reset) {
        continuity.reset_target = reset->current;
    }
    if (present) {
        continuity.present_target = present->current;
        continuity.present_active = present->owned;
    }
    continuity.begin_scene_active = begin_scene == nullptr || begin_scene->owned;
    continuity.end_scene_active = end_scene == nullptr || end_scene->owned;
    if (begin_scene) continuity.begin_scene_target = begin_scene->current;
    if (end_scene) continuity.end_scene_target = end_scene->current;
    return continuity;
}

} // namespace

bool InstallDeviceVtableHook(
    IDirect3DDevice9* device, DeviceHookCallbacks callbacks) noexcept {
    const HookRegistryOutcome outcome = InstallDeviceVtableHookDetailed(device, callbacks);
    return outcome.result == HookRegistryResult::Installed ||
        outcome.result == HookRegistryResult::AlreadyInstalled;
}

HookRegistryOutcome InstallDeviceVtableHookDetailed(
    IDirect3DDevice9* device, DeviceHookCallbacks callbacks) noexcept {
    HookRegistryOutcome failed{};
    void** vtable = DeviceVtable(device);
    if (!vtable) return failed;
    if (!PinModuleForAddress(reinterpret_cast<void*>(&HookPresent))) {
        failed.result = HookRegistryResult::ProtectionFailure;
        return failed;
    }

    try {
        std::lock_guard lock(g_callbacks_mutex);
        g_callbacks_by_vtable.insert_or_assign(vtable, callbacks);

        std::array<HookSlotRequest, 4> requests{};
        std::size_t request_count = 0;
        if (callbacks.before_reset || callbacks.after_reset) {
            requests[request_count++] = HookSlotRequest{
                kResetIndex, reinterpret_cast<void*>(&HookReset)};
        }
        requests[request_count++] = HookSlotRequest{
            kPresentIndex, reinterpret_cast<void*>(&HookPresent)};
        if (callbacks.before_begin_scene || callbacks.after_begin_scene) {
            requests[request_count++] = HookSlotRequest{
                kBeginSceneIndex, reinterpret_cast<void*>(&HookBeginScene)};
        }
        if (callbacks.before_end_scene || callbacks.after_end_scene) {
            requests[request_count++] = HookSlotRequest{
                kEndSceneIndex, reinterpret_cast<void*>(&HookEndScene)};
        }

        const HookRegistryOutcome outcome = g_registry.Install(
            vtable, std::span<const HookSlotRequest>(requests.data(), request_count));
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

HookRegistryOutcome ReacquireDeviceVtableHookDetailed(
    IDirect3DDevice9* device) noexcept {
    HookRegistryOutcome failed{};
    void** vtable = DeviceVtable(device);
    return vtable ? g_registry.Reacquire(vtable) : failed;
}

bool RestoreAllDeviceVtableHooks() noexcept {
    bool restored_all = true;
    try {
        std::lock_guard lock(g_callbacks_mutex);
        for (void** vtable : g_registry.RegisteredVtables()) {
            const HookRegistryOutcome outcome = g_registry.Restore(vtable);
            if (outcome.result != HookRegistryResult::Installed &&
                outcome.result != HookRegistryResult::AlreadyInstalled) {
                restored_all = false;
            }
            g_callbacks_by_vtable.erase(vtable);
        }
    } catch (...) {
        restored_all = false;
    }
    return restored_all;
}

DeviceVtableHookStatus InspectDeviceVtableHook(IDirect3DDevice9* device) noexcept {
    DeviceVtableHookStatus status{};
    void** vtable = DeviceVtable(device);
    if (!vtable) return status;
    const std::vector<HookSlotStatus> slots = g_registry.Inspect(vtable);
    status.installed = !slots.empty();
    status.device_uses_hooked_vtable = status.installed;
    return status;
}

bool InstalledDeviceVtableHookActive() noexcept {
    const std::vector<void**> vtables = g_registry.RegisteredVtables();
    if (vtables.empty()) return false;
    for (void** vtable : vtables) {
        const DeviceVtableHookContinuity continuity = ContinuityFor(vtable);
        if (!continuity.installed || !continuity.reset_active || !continuity.present_active ||
            !continuity.begin_scene_active || !continuity.end_scene_active) {
            return false;
        }
    }
    return true;
}

DeviceVtableHookContinuity InspectInstalledDeviceVtableHook() noexcept {
    const std::vector<void**> vtables = g_registry.RegisteredVtables();
    return vtables.empty() ? DeviceVtableHookContinuity{} : ContinuityFor(vtables.front());
}

HookDiagnostics InspectAllDeviceVtableHooks() noexcept {
    HookDiagnostics diagnostics;
    try {
        const std::vector<void**> vtables = g_registry.RegisteredVtables();
        for (void** vtable : vtables) {
            for (const HookSlotStatus& slot : g_registry.Inspect(vtable)) {
                std::string_view name = "unknown";
                if (slot.index == kResetIndex) name = "Reset";
                else if (slot.index == kPresentIndex) name = "Present";
                else if (slot.index == kBeginSceneIndex) name = "BeginScene";
                else if (slot.index == kEndSceneIndex) name = "EndScene";
                diagnostics.push_back(HookSlotDiagnostic{
                    .interface_name = "IDirect3DDevice9",
                    .slot_name = name,
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
