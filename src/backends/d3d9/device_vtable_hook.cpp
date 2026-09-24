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
constexpr std::size_t kQueryInterfaceIndex = 0;
constexpr std::size_t kAddRefIndex = 1;
constexpr std::size_t kReleaseIndex = 2;
constexpr std::size_t kTestCooperativeLevelIndex = 3;
constexpr std::size_t kGetDirect3DIndex = 6;
constexpr std::size_t kSetRenderTargetIndex = 37;
constexpr std::size_t kSetDepthStencilSurfaceIndex = 39;
constexpr std::size_t kClearIndex = 43;
constexpr std::size_t kCreateStateBlockIndex = 59;
constexpr std::size_t kSetTextureIndex = 65;

using ResetFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using BeginSceneFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using EndSceneFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using QueryInterfaceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, REFIID, void**);
using RefFn = ULONG(STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using GetDirect3DFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3D9**);
using ClearFn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, float, DWORD);
using SetRenderTargetFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);
using SetDepthStencilSurfaceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DSurface9*);
using SetTextureFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
using CreateStateBlockFn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice9*, D3DSTATEBLOCKTYPE, IDirect3DStateBlock9**);

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

void TraceMethod(
    const DeviceHookCallbacks& callbacks, IDirect3DDevice9* device,
    const char* method, bool entering, HRESULT result) noexcept {
    if (callbacks.method_trace) callbacks.method_trace(device, method, entering, result);
}

HRESULT STDMETHODCALLTYPE HookQueryInterface(IDirect3DDevice9* device, REFIID iid, void** object) {
    void** vtable = DeviceVtable(device);
    const auto original = reinterpret_cast<QueryInterfaceFn>(
        g_registry.OriginalTarget(vtable, kQueryInterfaceIndex));
    if (!original) return E_NOINTERFACE;
    const auto callbacks = CallbacksFor(vtable);
    if (callbacks.com_trace) callbacks.com_trace(device, "query_interface_enter", &iid, nullptr, S_OK, 0);
    const HRESULT result = original(device, iid, object);
    if (callbacks.com_trace) callbacks.com_trace(
        device, "query_interface_result", &iid,
        SUCCEEDED(result) && object ? *object : nullptr, result, 0);
    return result;
}

ULONG STDMETHODCALLTYPE HookAddRef(IDirect3DDevice9* device) {
    void** vtable = DeviceVtable(device);
    const auto original = reinterpret_cast<RefFn>(g_registry.OriginalTarget(vtable, kAddRefIndex));
    if (!original) return 0;
    const ULONG count = original(device);
    const auto callbacks = CallbacksFor(vtable);
    if (callbacks.com_trace) callbacks.com_trace(device, "add_ref", nullptr, nullptr, S_OK, count);
    return count;
}

ULONG STDMETHODCALLTYPE HookRelease(IDirect3DDevice9* device) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<RefFn>(g_registry.OriginalTarget(vtable, kReleaseIndex));
    if (!original) return 0;
    const ULONG count = original(device);
    if (callbacks.com_trace) callbacks.com_trace(device, "release", nullptr, nullptr, S_OK, count);
    return count;
}

HRESULT STDMETHODCALLTYPE HookTestCooperativeLevel(IDirect3DDevice9* device) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<BeginSceneFn>(
        g_registry.OriginalTarget(vtable, kTestCooperativeLevelIndex));
    if (!original) return D3DERR_INVALIDCALL;
    TraceMethod(callbacks, device, "TestCooperativeLevel", true, S_OK);
    const HRESULT result = original(device);
    TraceMethod(callbacks, device, "TestCooperativeLevel", false, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookGetDirect3D(IDirect3DDevice9* device, IDirect3D9** factory) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<GetDirect3DFn>(
        g_registry.OriginalTarget(vtable, kGetDirect3DIndex));
    if (!original) return D3DERR_INVALIDCALL;
    TraceMethod(callbacks, device, "GetDirect3D", true, S_OK);
    HRESULT result = D3DERR_INVALIDCALL;
    if (callbacks.get_direct3d_factory &&
        callbacks.get_direct3d_device == device) {
        if (factory) {
            callbacks.get_direct3d_factory->AddRef();
            *factory = callbacks.get_direct3d_factory;
            result = D3D_OK;
        }
    } else {
        result = original(device, factory);
    }
    TraceMethod(callbacks, device, "GetDirect3D", false, result);
    if (callbacks.com_trace) callbacks.com_trace(
        device, "get_direct3d_result", &IID_IDirect3D9,
        SUCCEEDED(result) && factory ? *factory : nullptr, result, 0);
    return result;
}

HRESULT STDMETHODCALLTYPE HookClear(
    IDirect3DDevice9* device, DWORD count, const D3DRECT* rects, DWORD flags,
    D3DCOLOR color, float z, DWORD stencil) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<ClearFn>(g_registry.OriginalTarget(vtable, kClearIndex));
    if (!original) return D3DERR_INVALIDCALL;
    TraceMethod(callbacks, device, "Clear", true, S_OK);
    const HRESULT result = original(device, count, rects, flags, color, z, stencil);
    TraceMethod(callbacks, device, "Clear", false, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookSetRenderTarget(
    IDirect3DDevice9* device, DWORD index, IDirect3DSurface9* target) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<SetRenderTargetFn>(
        g_registry.OriginalTarget(vtable, kSetRenderTargetIndex));
    if (!original) return D3DERR_INVALIDCALL;
    TraceMethod(callbacks, device, "SetRenderTarget", true, S_OK);
    const HRESULT result = original(device, index, target);
    TraceMethod(callbacks, device, "SetRenderTarget", false, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookSetDepthStencilSurface(
    IDirect3DDevice9* device, IDirect3DSurface9* surface) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<SetDepthStencilSurfaceFn>(
        g_registry.OriginalTarget(vtable, kSetDepthStencilSurfaceIndex));
    if (!original) return D3DERR_INVALIDCALL;
    TraceMethod(callbacks, device, "SetDepthStencilSurface", true, S_OK);
    const HRESULT result = original(device, surface);
    TraceMethod(callbacks, device, "SetDepthStencilSurface", false, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookSetTexture(
    IDirect3DDevice9* device, DWORD stage, IDirect3DBaseTexture9* texture) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<SetTextureFn>(
        g_registry.OriginalTarget(vtable, kSetTextureIndex));
    if (!original) return D3DERR_INVALIDCALL;
    TraceMethod(callbacks, device, "SetTexture", true, S_OK);
    const HRESULT result = original(device, stage, texture);
    TraceMethod(callbacks, device, "SetTexture", false, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateStateBlock(
    IDirect3DDevice9* device, D3DSTATEBLOCKTYPE type, IDirect3DStateBlock9** block) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<CreateStateBlockFn>(
        g_registry.OriginalTarget(vtable, kCreateStateBlockIndex));
    if (!original) return D3DERR_INVALIDCALL;
    TraceMethod(callbacks, device, "CreateStateBlock", true, S_OK);
    const HRESULT result = original(device, type, block);
    TraceMethod(callbacks, device, "CreateStateBlock", false, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookReset(
    IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters) {
    void** vtable = DeviceVtable(device);
    const DeviceHookCallbacks callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<ResetFn>(
        g_registry.OriginalTarget(vtable, kResetIndex));
    if (!original) return D3DERR_INVALIDCALL;
    TraceMethod(callbacks, device, "Reset", true, S_OK);
    if (callbacks.before_reset) callbacks.before_reset(device, parameters);
    const HRESULT result = original(device, parameters);
    if (callbacks.after_reset) callbacks.after_reset(device, parameters, result);
    TraceMethod(callbacks, device, "Reset", false, result);
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
    TraceMethod(callbacks, device, "Present", true, S_OK);
    if (callbacks.before_present) callbacks.before_present(device);
    const HRESULT result = original(
        device, source_rect, destination_rect, destination_window_override, dirty_region);
    if (callbacks.after_present) callbacks.after_present(device, result);
    TraceMethod(callbacks, device, "Present", false, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookBeginScene(IDirect3DDevice9* device) {
    void** vtable = DeviceVtable(device);
    const DeviceHookCallbacks callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<BeginSceneFn>(
        g_registry.OriginalTarget(vtable, kBeginSceneIndex));
    if (!original) return D3DERR_INVALIDCALL;
    TraceMethod(callbacks, device, "BeginScene", true, S_OK);
    if (callbacks.before_begin_scene) callbacks.before_begin_scene(device);
    const HRESULT result = original(device);
    if (callbacks.after_begin_scene) callbacks.after_begin_scene(device, result);
    TraceMethod(callbacks, device, "BeginScene", false, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookEndScene(IDirect3DDevice9* device) {
    void** vtable = DeviceVtable(device);
    const DeviceHookCallbacks callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<EndSceneFn>(
        g_registry.OriginalTarget(vtable, kEndSceneIndex));
    if (!original) return D3DERR_INVALIDCALL;
    TraceMethod(callbacks, device, "EndScene", true, S_OK);
    if (callbacks.before_end_scene) callbacks.before_end_scene(device);
    const HRESULT result = original(device);
    if (callbacks.after_end_scene) callbacks.after_end_scene(device, result);
    TraceMethod(callbacks, device, "EndScene", false, result);
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

        std::array<HookSlotRequest, 15> requests{};
        std::size_t request_count = 0;
        if (callbacks.com_trace) {
            requests[request_count++] = {kQueryInterfaceIndex, reinterpret_cast<void*>(&HookQueryInterface)};
            requests[request_count++] = {kAddRefIndex, reinterpret_cast<void*>(&HookAddRef)};
            requests[request_count++] = {kReleaseIndex, reinterpret_cast<void*>(&HookRelease)};
        }
        if (callbacks.com_trace || callbacks.get_direct3d_factory) {
            requests[request_count++] = {kGetDirect3DIndex, reinterpret_cast<void*>(&HookGetDirect3D)};
        }
        if (callbacks.method_trace) {
            requests[request_count++] = {kTestCooperativeLevelIndex, reinterpret_cast<void*>(&HookTestCooperativeLevel)};
            requests[request_count++] = {kSetRenderTargetIndex, reinterpret_cast<void*>(&HookSetRenderTarget)};
            requests[request_count++] = {kSetDepthStencilSurfaceIndex, reinterpret_cast<void*>(&HookSetDepthStencilSurface)};
            requests[request_count++] = {kClearIndex, reinterpret_cast<void*>(&HookClear)};
            requests[request_count++] = {kCreateStateBlockIndex, reinterpret_cast<void*>(&HookCreateStateBlock)};
            requests[request_count++] = {kSetTextureIndex, reinterpret_cast<void*>(&HookSetTexture)};
        }
        if (callbacks.before_reset || callbacks.after_reset || callbacks.method_trace) {
            requests[request_count++] = HookSlotRequest{
                kResetIndex, reinterpret_cast<void*>(&HookReset)};
        }
        requests[request_count++] = HookSlotRequest{
            kPresentIndex, reinterpret_cast<void*>(&HookPresent)};
        if (callbacks.before_begin_scene || callbacks.after_begin_scene || callbacks.method_trace) {
            requests[request_count++] = HookSlotRequest{
                kBeginSceneIndex, reinterpret_cast<void*>(&HookBeginScene)};
        }
        if (callbacks.before_end_scene || callbacks.after_end_scene || callbacks.method_trace) {
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
                else if (slot.index == kQueryInterfaceIndex) name = "QueryInterface";
                else if (slot.index == kAddRefIndex) name = "AddRef";
                else if (slot.index == kReleaseIndex) name = "Release";
                else if (slot.index == kTestCooperativeLevelIndex) name = "TestCooperativeLevel";
                else if (slot.index == kGetDirect3DIndex) name = "GetDirect3D";
                else if (slot.index == kSetRenderTargetIndex) name = "SetRenderTarget";
                else if (slot.index == kSetDepthStencilSurfaceIndex) name = "SetDepthStencilSurface";
                else if (slot.index == kClearIndex) name = "Clear";
                else if (slot.index == kCreateStateBlockIndex) name = "CreateStateBlock";
                else if (slot.index == kSetTextureIndex) name = "SetTexture";
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
