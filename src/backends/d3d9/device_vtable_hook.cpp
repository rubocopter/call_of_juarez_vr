#include "backends/d3d9/device_vtable_hook.hpp"

#include "backends/d3d9/hook_registry.hpp"

#include <array>
#include <algorithm>
#include <atomic>
#include <intrin.h>
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
std::atomic_uint64_t g_integrity_epoch{0};
std::mutex g_continuity_mutex;
std::mutex g_callbacks_mutex;
std::unordered_map<void**, DeviceHookCallbacks> g_callbacks_by_vtable;
std::unordered_map<void**, bool> g_probe_extended_by_vtable;

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
    const DWORD error = GetLastError();
    if (callbacks.method_trace) callbacks.method_trace(device, method, entering, result);
    SetLastError(error);
}

void ProbeEvent(const DeviceHookCallbacks& callbacks, IDirect3DDevice9* device,
    const RenderResourceEvent& event) noexcept {
    if (!callbacks.resource_probe_event) return;
    const DWORD error = GetLastError();
    callbacks.resource_probe_event(device, event);
    SetLastError(error);
}

class ProbeCall final {
public:
    ProbeCall(const DeviceHookCallbacks& callbacks,IDirect3DDevice9* device) noexcept
        : callback_(callbacks.resource_probe_call),hud_callback_(callbacks.hud_batch_call),device_(device) { Notify(true); }
    ~ProbeCall(){Notify(false);}
private:
    void Notify(bool entering) noexcept {
        if(!callback_&&!hud_callback_)return;
        const DWORD error=GetLastError();
        if(callback_)callback_(device_,entering);
        SetLastError(error);
        if(hud_callback_)hud_callback_(device_,entering);
        SetLastError(error);
    }
    void (*callback_)(IDirect3DDevice9*,bool) noexcept;
    void (*hud_callback_)(IDirect3DDevice9*,bool) noexcept;
    IDirect3DDevice9* device_;
};

void HudDrawEvent(const DeviceHookCallbacks& callbacks, IDirect3DDevice9* device,
    const RenderResourceEvent& event, bool entering) noexcept {
    if (!callbacks.hud_batch_draw) return;
    const DWORD error = GetLastError();
    callbacks.hud_batch_draw(device, event, entering);
    SetLastError(error);
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
    ProbeCall observation_call(callbacks,device);
    TraceMethod(callbacks, device, "Clear", true, S_OK);
    const HRESULT result = original(device, count, rects, flags, color, z, stencil);
    TraceMethod(callbacks, device, "Clear", false, result);
    ProbeEvent(callbacks, device, {RenderResourceOperation::clear, flags, result,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0});
    return result;
}

HRESULT STDMETHODCALLTYPE HookSetRenderTarget(
    IDirect3DDevice9* device, DWORD index, IDirect3DSurface9* target) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<SetRenderTargetFn>(
        g_registry.OriginalTarget(vtable, kSetRenderTargetIndex));
    if (!original) return D3DERR_INVALIDCALL;
    ProbeCall observation_call(callbacks,device);
    TraceMethod(callbacks, device, "SetRenderTarget", true, S_OK);
    const HRESULT result = original(device, index, target);
    TraceMethod(callbacks, device, "SetRenderTarget", false, result);
    ProbeEvent(callbacks, device, {RenderResourceOperation::set_target, index, result,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, reinterpret_cast<std::uintptr_t>(target)});
    return result;
}

HRESULT STDMETHODCALLTYPE HookSetDepthStencilSurface(
    IDirect3DDevice9* device, IDirect3DSurface9* surface) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<SetDepthStencilSurfaceFn>(
        g_registry.OriginalTarget(vtable, kSetDepthStencilSurfaceIndex));
    if (!original) return D3DERR_INVALIDCALL;
    ProbeCall observation_call(callbacks,device);
    TraceMethod(callbacks, device, "SetDepthStencilSurface", true, S_OK);
    const HRESULT result = original(device, surface);
    TraceMethod(callbacks, device, "SetDepthStencilSurface", false, result);
    ProbeEvent(callbacks, device, {RenderResourceOperation::set_depth, 0, result,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, reinterpret_cast<std::uintptr_t>(surface)});
    return result;
}

HRESULT STDMETHODCALLTYPE HookSetTexture(
    IDirect3DDevice9* device, DWORD stage, IDirect3DBaseTexture9* texture) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<SetTextureFn>(
        g_registry.OriginalTarget(vtable, kSetTextureIndex));
    if (!original) return D3DERR_INVALIDCALL;
    ProbeCall observation_call(callbacks,device);
    TraceMethod(callbacks, device, "SetTexture", true, S_OK);
    const HRESULT result = original(device, stage, texture);
    TraceMethod(callbacks, device, "SetTexture", false, result);
    ProbeEvent(callbacks, device, {RenderResourceOperation::set_texture, stage, result,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), reinterpret_cast<std::uintptr_t>(texture), 0});
    return result;
}

HRESULT STDMETHODCALLTYPE HookSetViewport(IDirect3DDevice9* device, const D3DVIEWPORT9* viewport) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const D3DVIEWPORT9* viewport);
    const auto original = reinterpret_cast<Fn>(g_registry.OriginalTarget(vtable, 47));
    if (!original) return D3DERR_INVALIDCALL;
    ProbeCall observation_call(callbacks,device);
    const HRESULT result = original(device, viewport);
    ProbeEvent(callbacks, device, {RenderResourceOperation::set_viewport, 0, result,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0});
    return result;
}

HRESULT STDMETHODCALLTYPE HookSetScissorRect(IDirect3DDevice9* device, const RECT* rect) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT* rect);
    const auto original = reinterpret_cast<Fn>(g_registry.OriginalTarget(vtable, 75));
    if (!original) return D3DERR_INVALIDCALL;
    ProbeCall observation_call(callbacks,device);
    const HRESULT result = original(device, rect);
    ProbeEvent(callbacks, device, {RenderResourceOperation::set_scissor, 0, result,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0});
    return result;
}

HRESULT STDMETHODCALLTYPE HookDrawPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, UINT start, UINT count) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE type, UINT start, UINT count);
    const auto original = reinterpret_cast<Fn>(g_registry.OriginalTarget(vtable, 81));
    if (!original) return D3DERR_INVALIDCALL;
    ProbeCall observation_call(callbacks,device);
    RenderResourceEvent draw{RenderResourceOperation::draw, static_cast<DWORD>(type), S_OK,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0};
    HudDrawEvent(callbacks, device, draw, true);
    const HRESULT result = original(device, type, start, count);
    draw.result = result;
    HudDrawEvent(callbacks, device, draw, false);
    ProbeEvent(callbacks, device, {RenderResourceOperation::draw, static_cast<DWORD>(type), result,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0});
    return result;
}

HRESULT STDMETHODCALLTYPE HookDrawIndexedPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT base, UINT min, UINT vertices, UINT start, UINT count) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE type, INT base, UINT min, UINT vertices, UINT start, UINT count);
    const auto original = reinterpret_cast<Fn>(g_registry.OriginalTarget(vtable, 82));
    if (!original) return D3DERR_INVALIDCALL;
    ProbeCall observation_call(callbacks,device);
    RenderResourceEvent draw{RenderResourceOperation::draw_indexed, static_cast<DWORD>(type), S_OK,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0};
    HudDrawEvent(callbacks, device, draw, true);
    const HRESULT result = original(device, type, base, min, vertices, start, count);
    draw.result = result;
    HudDrawEvent(callbacks, device, draw, false);
    ProbeEvent(callbacks, device, {RenderResourceOperation::draw_indexed, static_cast<DWORD>(type), result,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0});
    return result;
}

HRESULT STDMETHODCALLTYPE HookDrawPrimitiveUP(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, UINT count, const void* data, UINT stride) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE type, UINT count, const void* data, UINT stride);
    const auto original = reinterpret_cast<Fn>(g_registry.OriginalTarget(vtable, 83));
    if (!original) return D3DERR_INVALIDCALL;
    ProbeCall observation_call(callbacks,device);
    RenderResourceEvent draw{RenderResourceOperation::draw_up, static_cast<DWORD>(type), S_OK,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0};
    HudDrawEvent(callbacks, device, draw, true);
    const HRESULT result = original(device, type, count, data, stride);
    draw.result = result;
    HudDrawEvent(callbacks, device, draw, false);
    ProbeEvent(callbacks, device, {RenderResourceOperation::draw_up, static_cast<DWORD>(type), result,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0});
    return result;
}

HRESULT STDMETHODCALLTYPE HookDrawIndexedPrimitiveUP(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, UINT min, UINT vertices, UINT count, const void* indices, D3DFORMAT format, const void* data, UINT stride) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE type, UINT min, UINT vertices, UINT count, const void* indices, D3DFORMAT format, const void* data, UINT stride);
    const auto original = reinterpret_cast<Fn>(g_registry.OriginalTarget(vtable, 84));
    if (!original) return D3DERR_INVALIDCALL;
    ProbeCall observation_call(callbacks,device);
    RenderResourceEvent draw{RenderResourceOperation::draw_indexed_up, static_cast<DWORD>(type), S_OK,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0};
    HudDrawEvent(callbacks, device, draw, true);
    const HRESULT result = original(device, type, min, vertices, count, indices, format, data, stride);
    draw.result = result;
    HudDrawEvent(callbacks, device, draw, false);
    ProbeEvent(callbacks, device, {RenderResourceOperation::draw_indexed_up, static_cast<DWORD>(type), result,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0});
    return result;
}

HRESULT STDMETHODCALLTYPE HookStretchRect(IDirect3DDevice9* device, IDirect3DSurface9* source, const RECT* source_rect, IDirect3DSurface9* dest, const RECT* dest_rect, D3DTEXTUREFILTERTYPE filter) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DSurface9* source, const RECT* source_rect, IDirect3DSurface9* dest, const RECT* dest_rect, D3DTEXTUREFILTERTYPE filter);
    const auto original = reinterpret_cast<Fn>(g_registry.OriginalTarget(vtable, 34));
    if (!original) return D3DERR_INVALIDCALL;
    ProbeCall observation_call(callbacks,device);
    const HRESULT result = original(device, source, source_rect, dest, dest_rect, filter);
    if(callbacks.resource_probe_copy){
        const DWORD native_error=GetLastError();
        callbacks.resource_probe_copy(device,source,source_rect,dest,dest_rect,result);
        SetLastError(native_error);
    }
    ProbeEvent(callbacks, device, {RenderResourceOperation::stretch_rect, static_cast<DWORD>(filter), result,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), reinterpret_cast<std::uintptr_t>(source), reinterpret_cast<std::uintptr_t>(dest)});
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
    if (callbacks.allow_reset && !callbacks.allow_reset(device)) return D3DERR_INVALIDCALL;
    ProbeCall observation_call(callbacks,device);
    ProbeEvent(callbacks, device, {RenderResourceOperation::reset, 0, S_FALSE,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0});
    TraceMethod(callbacks, device, "Reset", true, S_OK);
    if (callbacks.before_reset) callbacks.before_reset(device, parameters);
    const HRESULT result = original(device, parameters);
    if (callbacks.after_reset) callbacks.after_reset(device, parameters, result);
    TraceMethod(callbacks, device, "Reset", false, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookResetEx(IDirect3DDevice9Ex* device,
    D3DPRESENT_PARAMETERS* parameters, D3DDISPLAYMODEEX* mode) {
    void** vtable = DeviceVtable(device);
    const auto callbacks = CallbacksFor(vtable);
    using Fn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9Ex*, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*);
    const auto original = reinterpret_cast<Fn>(g_registry.OriginalTarget(vtable, 132));
    if (!original) return D3DERR_INVALIDCALL;
    if (callbacks.allow_reset && !callbacks.allow_reset(device)) return D3DERR_INVALIDCALL;
    ProbeCall observation_call(callbacks,device);
    // Invalidation only. Do not redirect ResetEx through classic Reset callbacks
    // or modify any presentation parameter in this diagnostic hook.
    ProbeEvent(callbacks, device, {RenderResourceOperation::reset, 1, S_FALSE,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), 0, 0});
    const HRESULT result=original(device,parameters,mode);
    if(callbacks.after_reset_ex)callbacks.after_reset_ex(device,parameters,result);
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

    bool probe_ex = false;
    const bool resource_observer = callbacks.resource_probe_event || callbacks.resource_probe_copy ||
        callbacks.hud_batch_draw || callbacks.hud_batch_call;
    if (resource_observer || callbacks.allow_reset || callbacks.after_reset_ex) {
        // Query before taking the callback mutex: a repeated installation may
        // already have QueryInterface hooked through the same callback map.
        IDirect3DDevice9Ex* extended = nullptr;
        const DWORD error = GetLastError();
        const HRESULT query = device->QueryInterface(IID_PPV_ARGS(&extended));
        probe_ex = SUCCEEDED(query) && extended && DeviceVtable(extended) == vtable;
        if (extended) extended->Release();
        SetLastError(error);
    }

    try {
        std::lock_guard continuity_lock(g_continuity_mutex);
        std::lock_guard lock(g_callbacks_mutex);
        g_callbacks_by_vtable.insert_or_assign(vtable, callbacks);
        g_probe_extended_by_vtable.insert_or_assign(vtable, probe_ex);

        std::array<HookSlotRequest, 24> requests{};
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
        }
        if (callbacks.method_trace || resource_observer) {
            requests[request_count++] = {kSetRenderTargetIndex, reinterpret_cast<void*>(&HookSetRenderTarget)};
            requests[request_count++] = {kSetDepthStencilSurfaceIndex, reinterpret_cast<void*>(&HookSetDepthStencilSurface)};
            requests[request_count++] = {kClearIndex, reinterpret_cast<void*>(&HookClear)};

            requests[request_count++] = {kSetTextureIndex, reinterpret_cast<void*>(&HookSetTexture)};
        }
        if (callbacks.method_trace) {
            requests[request_count++] = {kCreateStateBlockIndex, reinterpret_cast<void*>(&HookCreateStateBlock)};
        }
        if (resource_observer) {
            requests[request_count++] = {47, reinterpret_cast<void*>(&HookSetViewport)};
            requests[request_count++] = {75, reinterpret_cast<void*>(&HookSetScissorRect)};
            requests[request_count++] = {81, reinterpret_cast<void*>(&HookDrawPrimitive)};
            requests[request_count++] = {82, reinterpret_cast<void*>(&HookDrawIndexedPrimitive)};
            requests[request_count++] = {83, reinterpret_cast<void*>(&HookDrawPrimitiveUP)};
            requests[request_count++] = {84, reinterpret_cast<void*>(&HookDrawIndexedPrimitiveUP)};
            requests[request_count++] = {34, reinterpret_cast<void*>(&HookStretchRect)};
        }
        if (probe_ex) requests[request_count++] = {132, reinterpret_cast<void*>(&HookResetEx)};
        if (callbacks.before_reset || callbacks.after_reset || callbacks.method_trace || resource_observer || callbacks.allow_reset) {
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
        if(outcome.modified_slots)g_integrity_epoch.fetch_add(1,std::memory_order_acq_rel);
        const bool success = outcome.result == HookRegistryResult::Installed ||
            outcome.result == HookRegistryResult::AlreadyInstalled;
        if (!success && !outcome.ownership_record_retained) {
            g_callbacks_by_vtable.erase(vtable);
            g_probe_extended_by_vtable.erase(vtable);
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
    if(!vtable)return failed;
    std::lock_guard continuity_lock(g_continuity_mutex);
    const auto outcome=g_registry.Reacquire(vtable);
    if(outcome.modified_slots||outcome.result!=HookRegistryResult::AlreadyInstalled)
        g_integrity_epoch.fetch_add(1,std::memory_order_acq_rel);
    return outcome;
}

ResourceProbeHookContinuity InspectResourceProbeHookContinuity(IDirect3DDevice9* device) noexcept {
    const DWORD error=GetLastError();
    std::lock_guard continuity_lock(g_continuity_mutex);
    ResourceProbeHookContinuity result{};result.epoch=g_integrity_epoch.load(std::memory_order_acquire);
    try {
        const auto slots=g_registry.Inspect(DeviceVtable(device));
        constexpr std::size_t required[]{16,17,34,37,39,43,47,65,75,81,82,83,84};
        result.intact=std::all_of(std::begin(required),std::end(required),[&](std::size_t index){
            const auto* slot=FindSlot(slots,index);return slot&&slot->owned;
        })&&std::all_of(slots.begin(),slots.end(),[](const auto& slot){return slot.owned;});
        result.intact=result.intact&&result.epoch==g_integrity_epoch.load(std::memory_order_acquire);
    }catch(...){result.intact=false;}
    SetLastError(error);return result;
}

ResourceProbeHookContinuity TryInspectHudBatchHookContinuity(IDirect3DDevice9* device) noexcept {
    const DWORD error = GetLastError();
    ResourceProbeHookContinuity result{};
    std::unique_lock integrity(g_continuity_mutex, std::try_to_lock);
    std::unique_lock callbacks(g_callbacks_mutex, std::try_to_lock);
    if (integrity.owns_lock() && callbacks.owns_lock() && device) {
        auto** vtable = DeviceVtable(device);
        const auto found = g_callbacks_by_vtable.find(vtable);
        const auto extended = g_probe_extended_by_vtable.find(vtable);
        result.epoch = g_integrity_epoch.load(std::memory_order_acquire);
        const std::array<HookSlotRequest, 13> required{{
            {16, reinterpret_cast<void*>(&HookReset)}, {17, reinterpret_cast<void*>(&HookPresent)},
            {34, reinterpret_cast<void*>(&HookStretchRect)}, {37, reinterpret_cast<void*>(&HookSetRenderTarget)},
            {39, reinterpret_cast<void*>(&HookSetDepthStencilSurface)}, {43, reinterpret_cast<void*>(&HookClear)},
            {47, reinterpret_cast<void*>(&HookSetViewport)}, {65, reinterpret_cast<void*>(&HookSetTexture)},
            {75, reinterpret_cast<void*>(&HookSetScissorRect)}, {81, reinterpret_cast<void*>(&HookDrawPrimitive)},
            {82, reinterpret_cast<void*>(&HookDrawIndexedPrimitive)}, {83, reinterpret_cast<void*>(&HookDrawPrimitiveUP)},
            {84, reinterpret_cast<void*>(&HookDrawIndexedPrimitiveUP)}}};
        result.intact = found != g_callbacks_by_vtable.end() && extended != g_probe_extended_by_vtable.end() &&
            found->second.hud_batch_draw && found->second.hud_batch_call &&
            std::all_of(required.begin(), required.end(), [vtable](const auto& slot) {
                return vtable[slot.index] == slot.replacement;
            });
        if (result.intact && extended->second)
            result.intact = vtable[132] == reinterpret_cast<void*>(&HookResetEx);
        result.intact = result.intact && result.epoch == g_integrity_epoch.load(std::memory_order_acquire);
    }
    SetLastError(error); return result;
}

bool RestoreAllDeviceVtableHooks() noexcept {
    bool restored_all = true;
    try {
        std::lock_guard continuity_lock(g_continuity_mutex);
        std::lock_guard lock(g_callbacks_mutex);
        for (void** vtable : g_registry.RegisteredVtables()) {
            g_integrity_epoch.fetch_add(1,std::memory_order_acq_rel);
            const HookRegistryOutcome outcome = g_registry.Restore(vtable);
            if (outcome.result != HookRegistryResult::Installed &&
                outcome.result != HookRegistryResult::AlreadyInstalled) {
                restored_all = false;
            }
            g_callbacks_by_vtable.erase(vtable);
            g_probe_extended_by_vtable.erase(vtable);
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
                else if (slot.index == 47) name = "SetViewport";
                else if (slot.index == 75) name = "SetScissorRect";
                else if (slot.index == 81) name = "DrawPrimitive";
                else if (slot.index == 82) name = "DrawIndexedPrimitive";
                else if (slot.index == 83) name = "DrawPrimitiveUP";
                else if (slot.index == 84) name = "DrawIndexedPrimitiveUP";
                else if (slot.index == 34) name = "StretchRect";
                else if (slot.index == 132) name = "ResetEx";
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
