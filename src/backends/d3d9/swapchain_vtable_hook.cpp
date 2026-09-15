#include "backends/d3d9/swapchain_vtable_hook.hpp"

#include "backends/d3d9/hook_registry.hpp"

#include <array>
#include <mutex>
#include <span>
#include <unordered_map>

namespace cojvr::backends::d3d9 {
namespace {

constexpr std::size_t kPresentIndex = 3;

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);

HookRegistry g_registry{};
std::mutex g_callbacks_mutex;
std::unordered_map<void**, SwapChainHookCallbacks> g_callbacks_by_vtable;

void** SwapChainVtable(IDirect3DSwapChain9* swap_chain) noexcept {
    return swap_chain ? *reinterpret_cast<void***>(swap_chain) : nullptr;
}

SwapChainHookCallbacks CallbacksFor(void** vtable) noexcept {
    try {
        std::lock_guard lock(g_callbacks_mutex);
        const auto callback = g_callbacks_by_vtable.find(vtable);
        return callback == g_callbacks_by_vtable.end() ? SwapChainHookCallbacks{} : callback->second;
    } catch (...) {
        return {};
    }
}

HRESULT STDMETHODCALLTYPE HookPresent(
    IDirect3DSwapChain9* swap_chain,
    const RECT* source_rect,
    const RECT* destination_rect,
    HWND destination_window_override,
    const RGNDATA* dirty_region,
    DWORD flags) {
    void** vtable = SwapChainVtable(swap_chain);
    const SwapChainHookCallbacks callbacks = CallbacksFor(vtable);
    const auto original = reinterpret_cast<PresentFn>(
        g_registry.OriginalTarget(vtable, kPresentIndex));
    if (!original) return D3DERR_INVALIDCALL;

    IDirect3DDevice9* device = nullptr;
    if ((callbacks.before_present || callbacks.after_present) && swap_chain &&
        SUCCEEDED(swap_chain->GetDevice(&device)) && device) {
        if (callbacks.before_present) callbacks.before_present(device);
    }

    const HRESULT result = original(
        swap_chain, source_rect, destination_rect, destination_window_override,
        dirty_region, flags);
    if (device) {
        if (callbacks.after_present) callbacks.after_present(device, result);
        device->Release();
    }
    return result;
}

} // namespace

bool InstallSwapChainVtableHook(
    IDirect3DDevice9* device, SwapChainPresentCallback before_present) noexcept {
    const HookRegistryOutcome outcome = InstallSwapChainVtableHookDetailed(
        device, SwapChainHookCallbacks{.before_present = before_present});
    return outcome.result == HookRegistryResult::Installed ||
        outcome.result == HookRegistryResult::AlreadyInstalled;
}

HookRegistryOutcome InstallSwapChainVtableHookDetailed(
    IDirect3DDevice9* device, SwapChainHookCallbacks callbacks) noexcept {
    HookRegistryOutcome failed{};
    if (!device || (!callbacks.before_present && !callbacks.after_present)) return failed;
    if (!PinModuleForAddress(reinterpret_cast<void*>(&HookPresent))) {
        failed.result = HookRegistryResult::ProtectionFailure;
        return failed;
    }

    try {
        IDirect3DSwapChain9* swap_chain = nullptr;
        if (FAILED(device->GetSwapChain(0, &swap_chain)) || !swap_chain) return failed;

        void** vtable = SwapChainVtable(swap_chain);
        if (!vtable) {
            swap_chain->Release();
            return failed;
        }

        HookRegistryOutcome outcome{};
        {
            std::lock_guard lock(g_callbacks_mutex);
            g_callbacks_by_vtable.insert_or_assign(vtable, callbacks);
            const std::array requests{
                HookSlotRequest{kPresentIndex, reinterpret_cast<void*>(&HookPresent)}};
            outcome = g_registry.Install(vtable, std::span<const HookSlotRequest>(requests));
            if (outcome.result != HookRegistryResult::Installed &&
                outcome.result != HookRegistryResult::AlreadyInstalled &&
                !outcome.ownership_record_retained) {
                g_callbacks_by_vtable.erase(vtable);
            }
        }

        swap_chain->Release();
        return outcome;
    } catch (...) {
        failed.result = HookRegistryResult::RollbackIncomplete;
        failed.ownership_record_retained = true;
        return failed;
    }
}

HookDiagnostics InspectAllSwapChainVtableHooks() noexcept {
    HookDiagnostics diagnostics;
    try {
        for (void** vtable : g_registry.RegisteredVtables()) {
            for (const HookSlotStatus& slot : g_registry.Inspect(vtable)) {
                diagnostics.push_back(HookSlotDiagnostic{
                    .interface_name = "IDirect3DSwapChain9",
                    .slot_name = slot.index == kPresentIndex ? "Present" : "unknown",
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
