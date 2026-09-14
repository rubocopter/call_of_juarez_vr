#include "backends/d3d9/device_vtable_hook.hpp"

#include <windows.h>

#include <mutex>

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

std::mutex g_install_mutex;
void** g_hooked_vtable = nullptr;
ResetFn g_original_reset = nullptr;
PresentFn g_original_present = nullptr;
BeginSceneFn g_original_begin_scene = nullptr;
EndSceneFn g_original_end_scene = nullptr;
DeviceHookCallbacks g_callbacks{};

bool ReplaceVtableEntry(void** entry, void* replacement, void** previous) noexcept {
    if (!entry || !replacement) return false;

    DWORD old_protection = 0;
    if (!VirtualProtect(entry, sizeof(*entry), PAGE_READWRITE, &old_protection)) return false;

    void* prior = InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(entry), replacement);

    DWORD ignored = 0;
    const BOOL restored = VirtualProtect(entry, sizeof(*entry), old_protection, &ignored);
    if (previous) *previous = prior;
    return restored != FALSE;
}

HRESULT STDMETHODCALLTYPE HookReset(
    IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters) {
    const ResetFn original = g_original_reset;
    const DeviceHookCallbacks callbacks = g_callbacks;
    if (!original) return D3DERR_INVALIDCALL;
    if (callbacks.before_reset) callbacks.before_reset(device, parameters);
    const HRESULT result = original(device, parameters);
    if (callbacks.after_reset) callbacks.after_reset(device, parameters, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookPresent(
    IDirect3DDevice9* device, const RECT* source_rect, const RECT* destination_rect,
    HWND destination_window_override, const RGNDATA* dirty_region) {
    const PresentFn original = g_original_present;
    const DeviceHookCallbacks callbacks = g_callbacks;
    if (!original) return D3DERR_INVALIDCALL;
    if (callbacks.before_present) callbacks.before_present(device);
    const HRESULT result = original(
        device, source_rect, destination_rect, destination_window_override, dirty_region);
    if (callbacks.after_present) callbacks.after_present(device, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookBeginScene(IDirect3DDevice9* device) {
    const BeginSceneFn original = g_original_begin_scene;
    const DeviceHookCallbacks callbacks = g_callbacks;
    if (!original) return D3DERR_INVALIDCALL;
    const HRESULT result = original(device);
    if (callbacks.after_begin_scene) callbacks.after_begin_scene(device, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookEndScene(IDirect3DDevice9* device) {
    const EndSceneFn original = g_original_end_scene;
    const DeviceHookCallbacks callbacks = g_callbacks;
    if (!original) return D3DERR_INVALIDCALL;
    const HRESULT result = original(device);
    if (callbacks.after_end_scene) callbacks.after_end_scene(device, result);
    return result;
}

} // namespace

bool InstallDeviceVtableHook(
    IDirect3DDevice9* device, DeviceHookCallbacks callbacks) noexcept {
    if (!device) return false;

    try {
        std::lock_guard lock(g_install_mutex);
        auto** vtable = *reinterpret_cast<void***>(device);
        if (!vtable) return false;

        if (g_hooked_vtable) {
            return g_hooked_vtable == vtable;
        }

        const auto original_reset = reinterpret_cast<ResetFn>(vtable[kResetIndex]);
        const auto original_present = reinterpret_cast<PresentFn>(vtable[kPresentIndex]);
        const bool hook_begin_scene = callbacks.after_begin_scene != nullptr;
        const auto original_begin_scene = hook_begin_scene
            ? reinterpret_cast<BeginSceneFn>(vtable[kBeginSceneIndex])
            : nullptr;
        const bool hook_end_scene = callbacks.after_end_scene != nullptr;
        const auto original_end_scene = hook_end_scene
            ? reinterpret_cast<EndSceneFn>(vtable[kEndSceneIndex])
            : nullptr;
        if (!original_reset || !original_present ||
            (hook_begin_scene && !original_begin_scene) ||
            (hook_end_scene && !original_end_scene)) return false;

        g_hooked_vtable = vtable;
        g_original_reset = original_reset;
        g_original_present = original_present;
        g_original_begin_scene = original_begin_scene;
        g_original_end_scene = original_end_scene;
        g_callbacks = callbacks;

        void* replaced_reset = nullptr;
        if (!ReplaceVtableEntry(
                &vtable[kResetIndex], reinterpret_cast<void*>(&HookReset), &replaced_reset) ||
            replaced_reset != reinterpret_cast<void*>(original_reset)) {
            g_hooked_vtable = nullptr;
            g_original_reset = nullptr;
            g_original_present = nullptr;
            g_original_begin_scene = nullptr;
            g_original_end_scene = nullptr;
            g_callbacks = {};
            return false;
        }

        void* replaced_present = nullptr;
        if (!ReplaceVtableEntry(
                &vtable[kPresentIndex], reinterpret_cast<void*>(&HookPresent), &replaced_present) ||
            replaced_present != reinterpret_cast<void*>(original_present)) {
            void* ignored = nullptr;
            (void)ReplaceVtableEntry(
                &vtable[kResetIndex], reinterpret_cast<void*>(original_reset), &ignored);
            g_hooked_vtable = nullptr;
            g_original_reset = nullptr;
            g_original_present = nullptr;
            g_original_begin_scene = nullptr;
            g_original_end_scene = nullptr;
            g_callbacks = {};
            return false;
        }

        if (hook_begin_scene) {
            void* replaced_begin_scene = nullptr;
            if (!ReplaceVtableEntry(
                    &vtable[kBeginSceneIndex], reinterpret_cast<void*>(&HookBeginScene),
                    &replaced_begin_scene) ||
                replaced_begin_scene != reinterpret_cast<void*>(original_begin_scene)) {
                void* ignored = nullptr;
                (void)ReplaceVtableEntry(
                    &vtable[kPresentIndex], reinterpret_cast<void*>(original_present), &ignored);
                (void)ReplaceVtableEntry(
                    &vtable[kResetIndex], reinterpret_cast<void*>(original_reset), &ignored);
                g_hooked_vtable = nullptr;
                g_original_reset = nullptr;
                g_original_present = nullptr;
                g_original_begin_scene = nullptr;
                g_original_end_scene = nullptr;
                g_callbacks = {};
                return false;
            }
        }

        if (hook_end_scene) {
            void* replaced_end_scene = nullptr;
            if (!ReplaceVtableEntry(
                    &vtable[kEndSceneIndex], reinterpret_cast<void*>(&HookEndScene), &replaced_end_scene) ||
                replaced_end_scene != reinterpret_cast<void*>(original_end_scene)) {
                void* ignored = nullptr;
                if (hook_begin_scene) {
                    (void)ReplaceVtableEntry(
                        &vtable[kBeginSceneIndex], reinterpret_cast<void*>(original_begin_scene),
                        &ignored);
                }
                (void)ReplaceVtableEntry(
                    &vtable[kPresentIndex], reinterpret_cast<void*>(original_present), &ignored);
                (void)ReplaceVtableEntry(
                    &vtable[kResetIndex], reinterpret_cast<void*>(original_reset), &ignored);
                g_hooked_vtable = nullptr;
                g_original_reset = nullptr;
                g_original_present = nullptr;
                g_original_begin_scene = nullptr;
                g_original_end_scene = nullptr;
                g_callbacks = {};
                return false;
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

DeviceVtableHookStatus InspectDeviceVtableHook(IDirect3DDevice9* device) noexcept {
    DeviceVtableHookStatus status{};
    if (!device) return status;

    try {
        std::lock_guard lock(g_install_mutex);
        status.installed = g_hooked_vtable != nullptr;
        if (!status.installed) return status;

        auto** vtable = *reinterpret_cast<void***>(device);
        status.device_uses_hooked_vtable = vtable == g_hooked_vtable;
    } catch (...) {
    }
    return status;
}

bool InstalledDeviceVtableHookActive() noexcept {
    const DeviceVtableHookContinuity continuity = InspectInstalledDeviceVtableHook();
    return continuity.installed && continuity.reset_active && continuity.present_active &&
        continuity.begin_scene_active && continuity.end_scene_active;
}

DeviceVtableHookContinuity InspectInstalledDeviceVtableHook() noexcept {
    DeviceVtableHookContinuity continuity{};
    try {
        std::lock_guard lock(g_install_mutex);
        if (!g_hooked_vtable) return continuity;

        continuity.installed = true;
        continuity.reset_target = g_hooked_vtable[kResetIndex];
        continuity.present_target = g_hooked_vtable[kPresentIndex];
        continuity.begin_scene_target = g_hooked_vtable[kBeginSceneIndex];
        continuity.end_scene_target = g_hooked_vtable[kEndSceneIndex];
        continuity.reset_active = continuity.reset_target == reinterpret_cast<void*>(&HookReset);
        continuity.present_active = continuity.present_target == reinterpret_cast<void*>(&HookPresent);
        continuity.begin_scene_active = g_original_begin_scene == nullptr ||
            continuity.begin_scene_target == reinterpret_cast<void*>(&HookBeginScene);
        continuity.end_scene_active = g_original_end_scene == nullptr ||
            continuity.end_scene_target == reinterpret_cast<void*>(&HookEndScene);
    } catch (...) {
    }
    return continuity;
}

} // namespace cojvr::backends::d3d9
