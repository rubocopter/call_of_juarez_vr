#pragma once

#include "backends/d3d9/hook_diagnostics.hpp"
#include "backends/d3d9/hook_registry.hpp"

#include <d3d9.h>

namespace cojvr::backends::d3d9 {

using SwapChainPresentCallback = void (*)(IDirect3DDevice9* device) noexcept;

struct SwapChainHookCallbacks {
    void (*before_present)(IDirect3DDevice9* device) noexcept = nullptr;
    void (*after_present)(IDirect3DDevice9* device, HRESULT result) noexcept = nullptr;
};

// Patches IDirect3DSwapChain9::Present on the implicit swap chain while leaving
// the native D3D9 device and swap-chain objects unchanged.
[[nodiscard]] bool InstallSwapChainVtableHook(
    IDirect3DDevice9* device, SwapChainPresentCallback before_present) noexcept;

[[nodiscard]] HookRegistryOutcome InstallSwapChainVtableHookDetailed(
    IDirect3DDevice9* device, SwapChainHookCallbacks callbacks) noexcept;

[[nodiscard]] HookDiagnostics InspectAllSwapChainVtableHooks() noexcept;

} // namespace cojvr::backends::d3d9
