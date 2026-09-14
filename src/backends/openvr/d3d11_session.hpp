#pragma once

#include <d3d11.h>

#include <memory>
#include <string_view>

namespace cojvr::runtime {
class OpenVrRuntime;
}

namespace cojvr::backends::openvr {

class D3D11SessionBridge final {
public:
    D3D11SessionBridge();
    ~D3D11SessionBridge();

    D3D11SessionBridge(const D3D11SessionBridge&) = delete;
    D3D11SessionBridge& operator=(const D3D11SessionBridge&) = delete;
    D3D11SessionBridge(D3D11SessionBridge&&) noexcept;
    D3D11SessionBridge& operator=(D3D11SessionBridge&&) noexcept;

    [[nodiscard]] bool Initialize(runtime::OpenVrRuntime& runtime) noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] ID3D11Device* device() const noexcept;
    [[nodiscard]] ID3D11DeviceContext* context() const noexcept;
    [[nodiscard]] D3D_FEATURE_LEVEL feature_level() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::backends::openvr
