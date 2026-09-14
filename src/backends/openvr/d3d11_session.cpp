#include "backends/openvr/d3d11_session.hpp"

#include "runtime/openvr_runtime.hpp"

#include <dxgi1_2.h>
#include <wrl/client.h>

#include <array>
#include <string>

namespace cojvr::backends::openvr {

using Microsoft::WRL::ComPtr;

struct D3D11SessionBridge::Impl {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_9_1;
    std::string last_error;
};

D3D11SessionBridge::D3D11SessionBridge() : impl_(std::make_unique<Impl>()) {}
D3D11SessionBridge::~D3D11SessionBridge() = default;
D3D11SessionBridge::D3D11SessionBridge(D3D11SessionBridge&&) noexcept = default;
D3D11SessionBridge& D3D11SessionBridge::operator=(D3D11SessionBridge&&) noexcept = default;

bool D3D11SessionBridge::Initialize(runtime::OpenVrRuntime& runtime) noexcept {
    impl_->last_error.clear();
    Shutdown();
    if (!runtime.initialized()) {
        impl_->last_error = "OpenVR runtime is not initialized";
        return false;
    }

    const std::int32_t adapter_index = runtime.system_info().dxgi_adapter_index;
    if (adapter_index < 0) {
        impl_->last_error = "OpenVR did not provide a valid DXGI adapter index";
        return false;
    }

    ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        impl_->last_error = "CreateDXGIFactory1 failed";
        return false;
    }

    ComPtr<IDXGIAdapter1> adapter;
    hr = factory->EnumAdapters1(static_cast<UINT>(adapter_index), &adapter);
    if (FAILED(hr)) {
        impl_->last_error = "OpenVR DXGI adapter index could not be enumerated";
        return false;
    }

    constexpr std::array<D3D_FEATURE_LEVEL, 3> feature_levels{
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    hr = D3D11CreateDevice(
        adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        feature_levels.data(),
        static_cast<UINT>(feature_levels.size()),
        D3D11_SDK_VERSION,
        &impl_->device,
        &impl_->feature_level,
        &impl_->context);
    if (FAILED(hr)) {
        impl_->last_error = "D3D11CreateDevice failed on the OpenVR-selected adapter";
        Shutdown();
        return false;
    }
    return true;
}

void D3D11SessionBridge::Shutdown() noexcept {
    if (!impl_) return;
    impl_->context.Reset();
    impl_->device.Reset();
    impl_->feature_level = D3D_FEATURE_LEVEL_9_1;
}

ID3D11Device* D3D11SessionBridge::device() const noexcept { return impl_->device.Get(); }
ID3D11DeviceContext* D3D11SessionBridge::context() const noexcept { return impl_->context.Get(); }
D3D_FEATURE_LEVEL D3D11SessionBridge::feature_level() const noexcept { return impl_->feature_level; }
std::string_view D3D11SessionBridge::last_error() const noexcept { return impl_->last_error; }

} // namespace cojvr::backends::openvr
