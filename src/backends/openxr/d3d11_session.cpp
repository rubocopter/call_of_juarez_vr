#include "backends/openxr/d3d11_session.hpp"

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

#include <dxgi1_2.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace cojvr::backends::openxr {
namespace {

using Microsoft::WRL::ComPtr;

bool EqualLuid(const LUID& lhs, const LUID& rhs) noexcept {
    return lhs.LowPart == rhs.LowPart && lhs.HighPart == rhs.HighPart;
}

std::string HResultMessage(const char* operation, HRESULT result) {
    return std::string(operation) + " failed with HRESULT " +
           std::to_string(static_cast<std::int32_t>(result));
}

} // namespace

struct D3D11SessionBridge::Impl {
    runtime::OpenXrRuntime* runtime = nullptr;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_9_1;
    XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    std::string last_error;
};

D3D11SessionBridge::D3D11SessionBridge() : impl_(std::make_unique<Impl>()) {}
D3D11SessionBridge::~D3D11SessionBridge() { Shutdown(); }
D3D11SessionBridge::D3D11SessionBridge(D3D11SessionBridge&&) noexcept = default;
D3D11SessionBridge& D3D11SessionBridge::operator=(D3D11SessionBridge&&) noexcept = default;

bool D3D11SessionBridge::Initialize(runtime::OpenXrRuntime& xr_runtime) noexcept {
    if (!impl_) return false;
    if (impl_->device) {
        impl_->last_error = "D3D11 OpenXR session bridge is already initialized";
        return false;
    }
    if (xr_runtime.status() != runtime::OpenXrStatus::system_ready) {
        impl_->last_error = "OpenXR runtime must be system-ready before creating the D3D11 binding";
        return false;
    }

    const auto get_requirements = reinterpret_cast<PFN_xrGetD3D11GraphicsRequirementsKHR>(
        xr_runtime.GetProcAddress("xrGetD3D11GraphicsRequirementsKHR"));
    if (!get_requirements) {
        impl_->last_error = "xrGetD3D11GraphicsRequirementsKHR is unavailable";
        return false;
    }

    XrGraphicsRequirementsD3D11KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    const XrResult requirements_result = get_requirements(
        reinterpret_cast<XrInstance>(xr_runtime.native_instance()),
        static_cast<XrSystemId>(xr_runtime.system_id()), &requirements);
    if (XR_FAILED(requirements_result)) {
        impl_->last_error = "xrGetD3D11GraphicsRequirementsKHR failed with XrResult " +
                            std::to_string(requirements_result);
        return false;
    }

    ComPtr<IDXGIFactory1> factory;
    HRESULT result = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(result)) {
        impl_->last_error = HResultMessage("CreateDXGIFactory1", result);
        return false;
    }

    for (UINT index = 0;; ++index) {
        ComPtr<IDXGIAdapter1> candidate;
        result = factory->EnumAdapters1(index, &candidate);
        if (result == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(result)) {
            impl_->last_error = HResultMessage("IDXGIFactory1::EnumAdapters1", result);
            return false;
        }

        DXGI_ADAPTER_DESC1 description{};
        result = candidate->GetDesc1(&description);
        if (FAILED(result)) continue;
        if (EqualLuid(description.AdapterLuid, requirements.adapterLuid)) {
            impl_->adapter = std::move(candidate);
            break;
        }
    }

    if (!impl_->adapter) {
        impl_->last_error = "OpenXR-required D3D11 adapter was not found through DXGI";
        return false;
    }

    constexpr std::array<D3D_FEATURE_LEVEL, 6> feature_levels{{
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    }};

    result = D3D11CreateDevice(
        impl_->adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, feature_levels.data(),
        static_cast<UINT>(feature_levels.size()), D3D11_SDK_VERSION,
        &impl_->device, &impl_->feature_level, &impl_->context);
    if (FAILED(result)) {
        impl_->last_error = HResultMessage("D3D11CreateDevice", result);
        return false;
    }
    if (impl_->feature_level < requirements.minFeatureLevel) {
        impl_->last_error = "created D3D11 device does not satisfy OpenXR minimum feature level";
        Shutdown();
        return false;
    }

    impl_->binding = XrGraphicsBindingD3D11KHR{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    impl_->binding.device = impl_->device.Get();
    if (!xr_runtime.CreateSession(runtime::OpenXrGraphicsBinding{&impl_->binding})) {
        impl_->last_error = std::string(xr_runtime.last_error());
        Shutdown();
        return false;
    }

    impl_->runtime = &xr_runtime;
    impl_->last_error.clear();
    return true;
}

void D3D11SessionBridge::Shutdown() noexcept {
    if (!impl_) return;
    if (impl_->runtime) {
        impl_->runtime->DestroySession();
        impl_->runtime = nullptr;
    }
    impl_->binding.device = nullptr;
    impl_->context.Reset();
    impl_->device.Reset();
    impl_->adapter.Reset();
    impl_->feature_level = D3D_FEATURE_LEVEL_9_1;
}

ID3D11Device* D3D11SessionBridge::device() const noexcept {
    return impl_ ? impl_->device.Get() : nullptr;
}

ID3D11DeviceContext* D3D11SessionBridge::context() const noexcept {
    return impl_ ? impl_->context.Get() : nullptr;
}

D3D_FEATURE_LEVEL D3D11SessionBridge::feature_level() const noexcept {
    return impl_ ? impl_->feature_level : D3D_FEATURE_LEVEL_9_1;
}

std::string_view D3D11SessionBridge::last_error() const noexcept {
    return impl_ ? std::string_view(impl_->last_error)
                 : std::string_view("D3D11 OpenXR session bridge unavailable");
}

} // namespace cojvr::backends::openxr
