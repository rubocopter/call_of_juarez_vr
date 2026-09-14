#pragma once

#include <d3d9.h>

#include <string>

namespace cojvr::backends::d3d9 {

// Performs one synchronous diagnostic transfer while preserving the caller's
// native classic-D3D9 device: backbuffer -> system memory -> D3D11 texture.
// This is intentionally a proof path, not a per-frame production transport.
[[nodiscard]] bool CaptureBackBufferToD3D11(
    IDirect3DDevice9* device,
    std::string& diagnostic) noexcept;

} // namespace cojvr::backends::d3d9
