#include "backends/d3d9/swapchain_vtable_hook.hpp"

#include <windows.h>

#include <mutex>

namespace cojvr::backends::d3d9 {
namespace {

constexpr std::size_t kPresentIndex = 3;

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);

std::mutex g_install_mutex;
void** g_hooked_vtable = nullptr;
PresentFn g_original_present = nullptr;
SwapChainPresentCallback g_before_present = nullptr;

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

HRESULT STDMETHODCALLTYPE HookPresent(
    IDirect3DSwapChain9* swap_chain,
    const RECT* source_rect,
    const RECT* destination_rect,
    HWND destination_window_override,
    const RGNDATA* dirty_region,
    DWORD flags) {
    const PresentFn original = g_original_present;
    const SwapChainPresentCallback callback = g_before_present;
    if (!original) return D3DERR_INVALIDCALL;

    IDirect3DDevice9* device = nullptr;
    if (callback && swap_chain && SUCCEEDED(swap_chain->GetDevice(&device)) && device) {
        callback(device);
    }

    const HRESULT result = original(
        swap_chain, source_rect, destination_rect, destination_window_override, dirty_region, flags);

    if (device) device->Release();
    return result;
}

} // namespace

bool InstallSwapChainVtableHook(
    IDirect3DDevice9* device, SwapChainPresentCallback before_present) noexcept {
    if (!device || !before_present) return false;

    try {
        IDirect3DSwapChain9* swap_chain = nullptr;
        if (FAILED(device->GetSwapChain(0, &swap_chain)) || !swap_chain) return false;

        std::lock_guard lock(g_install_mutex);
        auto** vtable = *reinterpret_cast<void***>(swap_chain);
        if (!vtable) {
            swap_chain->Release();
            return false;
        }

        if (g_hooked_vtable) {
            const bool matches = g_hooked_vtable == vtable;
            if (matches) g_before_present = before_present;
            swap_chain->Release();
            return matches;
        }

        const auto original_present = reinterpret_cast<PresentFn>(vtable[kPresentIndex]);
        if (!original_present) {
            swap_chain->Release();
            return false;
        }

        g_original_present = original_present;
        g_before_present = before_present;

        void* replaced_present = nullptr;
        const bool replaced = ReplaceVtableEntry(
            &vtable[kPresentIndex], reinterpret_cast<void*>(&HookPresent), &replaced_present);
        if (!replaced || replaced_present != reinterpret_cast<void*>(original_present)) {
            g_original_present = nullptr;
            g_before_present = nullptr;
            swap_chain->Release();
            return false;
        }

        g_hooked_vtable = vtable;
        swap_chain->Release();
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace cojvr::backends::d3d9
