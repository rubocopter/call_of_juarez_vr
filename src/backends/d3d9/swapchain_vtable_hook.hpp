#pragma once

#include <d3d9.h>

namespace cojvr::backends::d3d9 {

using SwapChainPresentCallback = void (*)(IDirect3DDevice9* device) noexcept;

// Patches IDirect3DSwapChain9::Present on the implicit swap chain while leaving
// the native D3D9 device and swap-chain objects unchanged.
[[nodiscard]] bool InstallSwapChainVtableHook(
    IDirect3DDevice9* device, SwapChainPresentCallback before_present) noexcept;

} // namespace cojvr::backends::d3d9
