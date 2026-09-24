#include "games/call_of_juarez/d3d9ex_legacy_resource_compat.hpp"

#include "backends/d3d9/hook_registry.hpp"
#include "backends/d3d9/vtable_patch.hpp"

#include <windows.h>

#include <array>
#include <atomic>
#include <intrin.h>
#include <mutex>
#include <span>
#include <unordered_map>

namespace cojvr::games::call_of_juarez {
namespace {

constexpr std::size_t kCreateTextureIndex = 23;
constexpr std::size_t kCreateVolumeTextureIndex = 24;
constexpr std::size_t kCreateCubeTextureIndex = 25;
constexpr std::size_t kResourceReleaseIndex = 2;

using CreateTextureFn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
    IDirect3DTexture9**, HANDLE*);
using CreateVolumeTextureFn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice9*, UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
    IDirect3DVolumeTexture9**, HANDLE*);
using CreateCubeTextureFn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DDevice9*, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
    IDirect3DCubeTexture9**, HANDLE*);
using ResourceReleaseFn = ULONG(STDMETHODCALLTYPE*)(IDirect3DBaseTexture9*);

struct HookState {
    IDirect3DDevice9* device = nullptr;
    LegacyTextureCompatibilityCallbacks callbacks{};
};

struct ResourceState {
    LegacyTextureKind kind = LegacyTextureKind::texture_2d;
    LegacyTextureCompatibilityCallbacks callbacks{};
};

cojvr::backends::d3d9::HookRegistry g_registry{};
cojvr::backends::d3d9::HookRegistry g_resource_registry{};
std::mutex g_state_mutex;
std::unordered_map<void**, HookState> g_state_by_vtable;
std::mutex g_resource_mutex;
std::unordered_map<IDirect3DBaseTexture9*, ResourceState> g_resources;
std::atomic_uint64_t g_sequence{0};

void** DeviceVtable(IDirect3DDevice9* device) noexcept {
    return device ? *reinterpret_cast<void***>(device) : nullptr;
}

HookState StateFor(void** vtable, IDirect3DDevice9* device) noexcept {
    try {
        std::lock_guard lock(g_state_mutex);
        const auto found = g_state_by_vtable.find(vtable);
        if (found == g_state_by_vtable.end() || found->second.device != device) return {};
        return found->second;
    } catch (...) {
        return {};
    }
}

void Notify(
    const HookState& state,
    const LegacyTextureCreateEvent& event) noexcept {
    if (state.callbacks.event) {
        state.callbacks.event(state.callbacks.context, event);
    }
}

ULONG STDMETHODCALLTYPE HookResourceRelease(IDirect3DBaseTexture9* resource) {
    void** vtable = resource ? *reinterpret_cast<void***>(resource) : nullptr;
    const auto original = reinterpret_cast<ResourceReleaseFn>(
        g_resource_registry.OriginalTarget(vtable, kResourceReleaseIndex));
    if (!original) return 0;
    const ULONG count = original(resource);
    if (count != 0) return count;

    ResourceState state{};
    bool tracked = false;
    {
        std::lock_guard lock(g_resource_mutex);
        const auto found = g_resources.find(resource);
        if (found != g_resources.end()) {
            state = found->second;
            g_resources.erase(found);
            tracked = true;
        }
    }
    if (tracked && state.callbacks.resource_destroy) {
        state.callbacks.resource_destroy(
            state.callbacks.context, reinterpret_cast<std::uintptr_t>(resource),
            state.kind, count);
    }
    return count;
}

void TrackResource(
    IDirect3DBaseTexture9* resource, LegacyTextureKind kind,
    LegacyTextureCompatibilityCallbacks callbacks) noexcept {
    if (!resource || !callbacks.resource_destroy ||
        !cojvr::backends::d3d9::PinModuleForAddress(
            reinterpret_cast<void*>(&HookResourceRelease))) return;
    void** vtable = *reinterpret_cast<void***>(resource);
    if (!vtable) return;
    try {
        {
            std::lock_guard lock(g_resource_mutex);
            g_resources.insert_or_assign(resource, ResourceState{kind, callbacks});
        }
        const std::array requests{
            cojvr::backends::d3d9::HookSlotRequest{
                kResourceReleaseIndex, reinterpret_cast<void*>(&HookResourceRelease)}};
        const auto outcome = g_resource_registry.Install(vtable, std::span(requests));
        if (outcome.result != cojvr::backends::d3d9::HookRegistryResult::Installed &&
            outcome.result != cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled &&
            !outcome.ownership_record_retained) {
            std::lock_guard lock(g_resource_mutex);
            g_resources.erase(resource);
        }
    } catch (...) {
    }
}

void TranslateManagedPool(
    const D3DPOOL requested_pool,
    const DWORD requested_usage,
    D3DPOOL& effective_pool,
    DWORD& effective_usage,
    bool& translated) noexcept {
    effective_pool = requested_pool;
    effective_usage = requested_usage;
    translated = requested_pool == D3DPOOL_MANAGED;
    if (translated) {
        effective_pool = D3DPOOL_DEFAULT;
        effective_usage |= D3DUSAGE_DYNAMIC;
    }
}

LegacyTextureCreateEvent BaseEvent(
    const LegacyTextureKind kind,
    IDirect3DDevice9* device,
    const std::uintptr_t caller,
    const UINT width,
    const UINT height,
    const UINT depth,
    const UINT levels,
    const DWORD requested_usage,
    const DWORD effective_usage,
    const D3DFORMAT format,
    const D3DPOOL requested_pool,
    const D3DPOOL effective_pool,
    const bool translated) noexcept {
    LegacyTextureCreateEvent event{};
    event.kind = kind;
    event.phase = LegacyTextureCreatePhase::enter;
    event.sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    event.thread_id = GetCurrentThreadId();
    event.device = reinterpret_cast<std::uintptr_t>(device);
    event.caller = caller;
    event.width = width;
    event.height = height;
    event.depth = depth;
    event.levels = levels;
    event.requested_usage = requested_usage;
    event.effective_usage = effective_usage;
    event.format = format;
    event.requested_pool = requested_pool;
    event.effective_pool = effective_pool;
    event.translated = translated;
    return event;
}

HRESULT STDMETHODCALLTYPE HookCreateTexture(
    IDirect3DDevice9* device,
    UINT width,
    UINT height,
    UINT levels,
    DWORD usage,
    D3DFORMAT format,
    D3DPOOL pool,
    IDirect3DTexture9** texture,
    HANDLE* shared_handle) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    void** vtable = DeviceVtable(device);
    const auto original = reinterpret_cast<CreateTextureFn>(
        g_registry.OriginalTarget(vtable, kCreateTextureIndex));
    if (!original) return D3DERR_INVALIDCALL;

    const HookState state = StateFor(vtable, device);
    DWORD effective_usage = usage;
    D3DPOOL effective_pool = pool;
    bool translated = false;
    if (state.device) {
        TranslateManagedPool(pool, usage, effective_pool, effective_usage, translated);
    }
    LegacyTextureCreateEvent event = BaseEvent(
        LegacyTextureKind::texture_2d, device, caller, width, height, 1, levels,
        usage, effective_usage, format, pool, effective_pool, translated);
    if (state.device) Notify(state, event);
    const HRESULT result = original(
        device, width, height, levels, effective_usage, format, effective_pool,
        texture, shared_handle);
    if (state.device) {
        if (SUCCEEDED(result) && texture && *texture && translated) {
            TrackResource(static_cast<IDirect3DBaseTexture9*>(*texture),
                          LegacyTextureKind::texture_2d, state.callbacks);
        }
        event.phase = LegacyTextureCreatePhase::result;
        event.result = result;
        Notify(state, event);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateVolumeTexture(
    IDirect3DDevice9* device,
    UINT width,
    UINT height,
    UINT depth,
    UINT levels,
    DWORD usage,
    D3DFORMAT format,
    D3DPOOL pool,
    IDirect3DVolumeTexture9** texture,
    HANDLE* shared_handle) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    void** vtable = DeviceVtable(device);
    const auto original = reinterpret_cast<CreateVolumeTextureFn>(
        g_registry.OriginalTarget(vtable, kCreateVolumeTextureIndex));
    if (!original) return D3DERR_INVALIDCALL;

    const HookState state = StateFor(vtable, device);
    DWORD effective_usage = usage;
    D3DPOOL effective_pool = pool;
    bool translated = false;
    if (state.device) {
        TranslateManagedPool(pool, usage, effective_pool, effective_usage, translated);
    }
    LegacyTextureCreateEvent event = BaseEvent(
        LegacyTextureKind::volume_texture, device, caller, width, height, depth, levels,
        usage, effective_usage, format, pool, effective_pool, translated);
    if (state.device) Notify(state, event);
    const HRESULT result = original(
        device, width, height, depth, levels, effective_usage, format, effective_pool,
        texture, shared_handle);
    if (state.device) {
        if (SUCCEEDED(result) && texture && *texture && translated) {
            TrackResource(static_cast<IDirect3DBaseTexture9*>(*texture),
                          LegacyTextureKind::volume_texture, state.callbacks);
        }
        event.phase = LegacyTextureCreatePhase::result;
        event.result = result;
        Notify(state, event);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateCubeTexture(
    IDirect3DDevice9* device,
    UINT edge_length,
    UINT levels,
    DWORD usage,
    D3DFORMAT format,
    D3DPOOL pool,
    IDirect3DCubeTexture9** texture,
    HANDLE* shared_handle) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    void** vtable = DeviceVtable(device);
    const auto original = reinterpret_cast<CreateCubeTextureFn>(
        g_registry.OriginalTarget(vtable, kCreateCubeTextureIndex));
    if (!original) return D3DERR_INVALIDCALL;

    const HookState state = StateFor(vtable, device);
    DWORD effective_usage = usage;
    D3DPOOL effective_pool = pool;
    bool translated = false;
    if (state.device) {
        TranslateManagedPool(pool, usage, effective_pool, effective_usage, translated);
    }
    LegacyTextureCreateEvent event = BaseEvent(
        LegacyTextureKind::cube_texture, device, caller, edge_length, edge_length, 1, levels,
        usage, effective_usage, format, pool, effective_pool, translated);
    if (state.device) Notify(state, event);
    const HRESULT result = original(
        device, edge_length, levels, effective_usage, format, effective_pool,
        texture, shared_handle);
    if (state.device) {
        if (SUCCEEDED(result) && texture && *texture && translated) {
            TrackResource(static_cast<IDirect3DBaseTexture9*>(*texture),
                          LegacyTextureKind::cube_texture, state.callbacks);
        }
        event.phase = LegacyTextureCreatePhase::result;
        event.result = result;
        Notify(state, event);
    }
    return result;
}

bool Successful(const cojvr::backends::d3d9::HookRegistryResult result) noexcept {
    return result == cojvr::backends::d3d9::HookRegistryResult::Installed ||
        result == cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled;
}

} // namespace

bool InstallD3D9ExLegacyTextureCompatibility(
    IDirect3DDevice9* device,
    LegacyTextureCompatibilityCallbacks callbacks) noexcept {
    void** vtable = DeviceVtable(device);
    if (!vtable) return false;

    IDirect3DDevice9Ex* ex_device = nullptr;
    const HRESULT ex_result = device->QueryInterface(
        IID_IDirect3DDevice9Ex, reinterpret_cast<void**>(&ex_device));
    if (FAILED(ex_result) || !ex_device) return false;
    ex_device->Release();

    if (!cojvr::backends::d3d9::PinModuleForAddress(
            reinterpret_cast<void*>(&HookCreateTexture))) {
        return false;
    }

    try {
        {
            std::lock_guard lock(g_state_mutex);
            g_state_by_vtable.insert_or_assign(vtable, HookState{device, callbacks});
        }
        const std::array requests{
            cojvr::backends::d3d9::HookSlotRequest{
                kCreateTextureIndex, reinterpret_cast<void*>(&HookCreateTexture)},
            cojvr::backends::d3d9::HookSlotRequest{
                kCreateVolumeTextureIndex, reinterpret_cast<void*>(&HookCreateVolumeTexture)},
            cojvr::backends::d3d9::HookSlotRequest{
                kCreateCubeTextureIndex, reinterpret_cast<void*>(&HookCreateCubeTexture)},
        };
        const auto outcome = g_registry.Install(vtable, std::span(requests));
        if (Successful(outcome.result)) return true;
        if (!outcome.ownership_record_retained) {
            std::lock_guard lock(g_state_mutex);
            g_state_by_vtable.erase(vtable);
        }
        return false;
    } catch (...) {
        return false;
    }
}

bool ReacquireD3D9ExLegacyTextureCompatibility(IDirect3DDevice9* device) noexcept {
    void** vtable = DeviceVtable(device);
    if (!vtable) return false;
    return Successful(g_registry.Reacquire(vtable).result);
}

bool RestoreD3D9ExLegacyTextureCompatibility() noexcept {
    bool restored = true;
    try {
        for (void** vtable : g_registry.RegisteredVtables()) {
            if (!Successful(g_registry.Restore(vtable).result)) restored = false;
        }
        for (void** vtable : g_resource_registry.RegisteredVtables()) {
            if (!Successful(g_resource_registry.Restore(vtable).result)) restored = false;
        }
        if (restored) {
            std::lock_guard lock(g_state_mutex);
            g_state_by_vtable.clear();
            std::lock_guard resource_lock(g_resource_mutex);
            g_resources.clear();
        }
    } catch (...) {
        restored = false;
    }
    return restored;
}

} // namespace cojvr::games::call_of_juarez
