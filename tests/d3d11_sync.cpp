#include "backends/openvr/d3d11_sync.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <iostream>
#include <string>

namespace {

int Fail(const char* message, const HRESULT hr = S_OK) {
    std::cerr << message;
    if (FAILED(hr)) std::cerr << " hr=0x" << std::hex << static_cast<unsigned long>(hr);
    std::cerr << '\n';
    return 1;
}

} // namespace

int main() {
    using Microsoft::WRL::ComPtr;
    using namespace cojvr::backends::openvr;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature_level{};
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &device, &feature_level, &context);
    if (FAILED(hr) || !device || !context) return Fail("WARP D3D11CreateDevice failed", hr);

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.Width = 32;
    texture_desc.Height = 32;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> texture;
    hr = device->CreateTexture2D(&texture_desc, nullptr, &texture);
    if (FAILED(hr) || !texture) return Fail("test texture creation failed", hr);

    std::array<std::uint32_t, 32 * 32> pixels{};
    constexpr std::array<D3D11SyncStrategy, 3> strategies{
        D3D11SyncStrategy::none,
        D3D11SyncStrategy::flush,
        D3D11SyncStrategy::event_query,
    };
    for (std::size_t index = 0; index < strategies.size(); ++index) {
        pixels.fill(static_cast<std::uint32_t>(0xFF102030U + index));
        context->UpdateSubresource(
            texture.Get(), 0, nullptr, pixels.data(), 32U * sizeof(std::uint32_t), 0);

        D3D11SyncResult result{};
        std::string error;
        if (!SynchronizeD3D11(
                device.Get(), context.Get(), strategies[index], 1000, result, error) ||
            !result.succeeded || result.timed_out) {
            std::cerr << D3D11SyncStrategyName(strategies[index]) << ": " << error << '\n';
            return 1;
        }
        if (strategies[index] == D3D11SyncStrategy::event_query && result.polls == 0) {
            return Fail("event-query strategy did not record polling evidence");
        }
        std::cout << D3D11SyncStrategyName(strategies[index])
                  << " elapsed_ms=" << result.elapsed_ms
                  << " polls=" << result.polls << '\n';
    }

    D3D11SyncResult invalid_result{};
    std::string invalid_error;
    if (SynchronizeD3D11(
            nullptr, context.Get(), D3D11SyncStrategy::flush, 1000,
            invalid_result, invalid_error) || invalid_error.empty()) {
        return Fail("invalid synchronization inputs did not fail closed");
    }

    std::cout << "Controlled D3D11 synchronization strategies passed\n";
    return 0;
}
