#pragma once

#include <cstdint>
#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace cojvr::backends::openvr {

enum class D3D11SyncStrategy : std::uint8_t {
    none,
    flush,
    event_query,
};

struct D3D11SyncResult {
    bool succeeded = false;
    bool timed_out = false;
    std::uint32_t polls = 0;
    long hresult = 0;
    double elapsed_ms = 0.0;
};

[[nodiscard]] const char* D3D11SyncStrategyName(D3D11SyncStrategy strategy) noexcept;

// Controlled synchronization seam for diagnostics/probes. The production
// presenter intentionally uses `none`: OpenVR submission owns the normal GPU
// handoff. `event_query` is bounded and exists for A/B evidence, not as an
// unconditional frame wait.
[[nodiscard]] bool SynchronizeD3D11(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    D3D11SyncStrategy strategy,
    std::uint32_t timeout_ms,
    D3D11SyncResult& result,
    std::string& error) noexcept;

} // namespace cojvr::backends::openvr
