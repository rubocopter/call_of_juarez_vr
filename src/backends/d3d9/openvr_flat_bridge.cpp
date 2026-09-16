#include "backends/d3d9/openvr_flat_bridge.hpp"

#include "backends/d3d9/content_hash.hpp"
#include "backends/openvr/d3d11_compositor.hpp"
#include "backends/openvr/d3d11_session.hpp"
#include "runtime/openvr_runtime.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <sstream>
#include <string>

namespace cojvr::backends::d3d9 {
namespace {

using Microsoft::WRL::ComPtr;

DXGI_FORMAT ToDxgiFormat(const D3DFORMAT format) noexcept {
    switch (format) {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

std::string Failure(const char* stage, const HRESULT result) {
    std::ostringstream out;
    out << stage << " failed with HRESULT 0x" << std::hex
        << static_cast<unsigned long>(result);
    return out.str();
}

bool SameSurfaceDescription(const D3DSURFACE_DESC& left, const D3DSURFACE_DESC& right) noexcept {
    return left.Width == right.Width && left.Height == right.Height &&
        left.Format == right.Format && left.MultiSampleType == right.MultiSampleType &&
        left.MultiSampleQuality == right.MultiSampleQuality;
}

void Report(
    const OpenVrFlatBridgePhaseCallback callback,
    const OpenVrFlatBridgePhase phase,
    const HRESULT hresult = S_OK,
    const int runtime_result = 0,
    const std::uint64_t content_hash = 0) noexcept {
    if (callback) callback(OpenVrFlatBridgePhaseEvent{phase, hresult, runtime_result, content_hash});
}

} // namespace

struct OpenVrFlatBridge::Impl {
    runtime::OpenVrRuntime runtime;
    openvr::D3D11SessionBridge d3d11;
    ComPtr<IDirect3DSurface9> resolved;
    ComPtr<IDirect3DSurface9> system_memory;
    ComPtr<ID3D11Texture2D> texture11;
    D3DSURFACE_DESC surface_desc{};
    bool runtime_initialized = false;
    bool resources_initialized = false;
    std::uint64_t submitted_frames = 0;
    std::string last_error;
    std::string description;

    void ReleaseD3D9Resources() noexcept {
        resolved.Reset();
        system_memory.Reset();
        resources_initialized = false;
        surface_desc = {};
    }

    bool EnsureRuntime() noexcept {
        if (runtime_initialized) return true;
        if (!runtime.Initialize("Call of Juarez VR flat game bridge")) {
            last_error = std::string(runtime.last_error());
            return false;
        }
        if (!d3d11.Initialize(runtime)) {
            last_error = std::string(d3d11.last_error());
            runtime.Shutdown();
            return false;
        }
        runtime_initialized = true;
        return true;
    }

    bool EnsureResources(IDirect3DDevice9* device, IDirect3DSurface9* back_buffer) noexcept {
        D3DSURFACE_DESC desc{};
        HRESULT hr = back_buffer->GetDesc(&desc);
        if (FAILED(hr)) {
            last_error = Failure("backbuffer GetDesc", hr);
            return false;
        }

        if (resources_initialized && SameSurfaceDescription(desc, surface_desc)) return true;

        ReleaseD3D9Resources();
        texture11.Reset();

        const DXGI_FORMAT dxgi_format = ToDxgiFormat(desc.Format);
        if (dxgi_format == DXGI_FORMAT_UNKNOWN) {
            last_error = "unsupported D3D9 backbuffer format " +
                std::to_string(static_cast<unsigned>(desc.Format));
            return false;
        }

        if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
            hr = device->CreateRenderTarget(
                desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0,
                FALSE, &resolved, nullptr);
            if (FAILED(hr) || !resolved) {
                last_error = Failure("non-MSAA render-target creation", hr);
                return false;
            }
        }

        hr = device->CreateOffscreenPlainSurface(
            desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
            &system_memory, nullptr);
        if (FAILED(hr) || !system_memory) {
            last_error = Failure("system-memory surface creation", hr);
            ReleaseD3D9Resources();
            return false;
        }

        D3D11_TEXTURE2D_DESC texture_desc{};
        texture_desc.Width = desc.Width;
        texture_desc.Height = desc.Height;
        texture_desc.MipLevels = 1;
        texture_desc.ArraySize = 1;
        texture_desc.Format = dxgi_format;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Usage = D3D11_USAGE_DEFAULT;
        texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        hr = d3d11.device()->CreateTexture2D(&texture_desc, nullptr, &texture11);
        if (FAILED(hr) || !texture11) {
            last_error = Failure("D3D11 texture creation", hr);
            ReleaseD3D9Resources();
            return false;
        }

        surface_desc = desc;
        resources_initialized = true;

        std::ostringstream out;
        out << "backbuffer=" << desc.Width << 'x' << desc.Height
            << " format=" << static_cast<unsigned>(desc.Format)
            << " msaa=" << static_cast<unsigned>(desc.MultiSampleType)
            << " feature_level=0x" << std::hex
            << static_cast<unsigned>(d3d11.feature_level());
        description = out.str();
        return true;
    }
};

OpenVrFlatBridge::OpenVrFlatBridge() : impl_(std::make_unique<Impl>()) {}
OpenVrFlatBridge::~OpenVrFlatBridge() = default;

bool OpenVrFlatBridge::CaptureAndSubmit(
    IDirect3DDevice9* device,
    const OpenVrFlatBridgePhaseCallback phase_callback) noexcept {
    if (!device) {
        impl_->last_error = "flat bridge requires a D3D9 device";
        return false;
    }

    try {
        impl_->last_error.clear();
        if (!impl_->EnsureRuntime()) {
            const int runtime_result = impl_->runtime.last_result_code() == 0
                ? -1 : impl_->runtime.last_result_code();
            Report(phase_callback, OpenVrFlatBridgePhase::RuntimeInitializationFailed,
                E_FAIL, runtime_result);
            return false;
        }
        Report(phase_callback, OpenVrFlatBridgePhase::RuntimeInitialized);

        ComPtr<IDirect3DSurface9> back_buffer;
        HRESULT hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
        if (FAILED(hr) || !back_buffer) {
            impl_->last_error = Failure("GetBackBuffer", hr);
            return false;
        }
        if (!impl_->EnsureResources(device, back_buffer.Get())) return false;

        IDirect3DSurface9* readback_source = back_buffer.Get();
        if (impl_->surface_desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
            hr = device->StretchRect(
                back_buffer.Get(), nullptr, impl_->resolved.Get(), nullptr, D3DTEXF_NONE);
            if (FAILED(hr)) {
                impl_->last_error = Failure("MSAA resolve StretchRect", hr);
                return false;
            }
            readback_source = impl_->resolved.Get();
        }

        Report(phase_callback, OpenVrFlatBridgePhase::BeforeReadback);
        hr = device->GetRenderTargetData(readback_source, impl_->system_memory.Get());
        Report(phase_callback, OpenVrFlatBridgePhase::AfterReadback, hr);
        if (FAILED(hr)) {
            impl_->last_error = Failure("GetRenderTargetData", hr);
            return false;
        }
        Report(phase_callback, OpenVrFlatBridgePhase::BeforeUpload);
        D3DLOCKED_RECT locked{};
        hr = impl_->system_memory->LockRect(&locked, nullptr, D3DLOCK_READONLY);
        if (FAILED(hr) || locked.pBits == nullptr || locked.Pitch <= 0) {
            if (SUCCEEDED(hr)) hr = E_FAIL;
            Report(phase_callback, OpenVrFlatBridgePhase::AfterUpload, hr);
            impl_->last_error = Failure("system-memory LockRect", hr);
            return false;
        }

        const std::uint64_t content_hash = HashBgrxSurfaceIgnoringAlpha(
            locked.pBits,
            static_cast<std::size_t>(locked.Pitch),
            impl_->surface_desc.Width,
            impl_->surface_desc.Height);
        if (content_hash == 0) {
            (void)impl_->system_memory->UnlockRect();
            Report(phase_callback, OpenVrFlatBridgePhase::AfterUpload, E_FAIL);
            impl_->last_error = "invalid locked surface layout for content hashing";
            return false;
        }
        Report(phase_callback, OpenVrFlatBridgePhase::FramePublished, S_OK, 0, content_hash);

        impl_->d3d11.context()->UpdateSubresource(
            impl_->texture11.Get(), 0, nullptr, locked.pBits,
            static_cast<UINT>(locked.Pitch), 0);
        hr = impl_->system_memory->UnlockRect();
        Report(phase_callback, OpenVrFlatBridgePhase::AfterUpload, hr);
        if (FAILED(hr)) {
            impl_->last_error = Failure("system-memory UnlockRect", hr);
            return false;
        }

        runtime::Pose ignored_pose{};
        Report(phase_callback, OpenVrFlatBridgePhase::BeforeWaitForHmdPose);
        if (!impl_->runtime.WaitForHmdPose(ignored_pose)) {
            Report(phase_callback, OpenVrFlatBridgePhase::AfterWaitForHmdPose,
                E_FAIL, impl_->runtime.last_result_code());
            impl_->last_error = std::string(impl_->runtime.last_error());
            return false;
        }
        Report(phase_callback, OpenVrFlatBridgePhase::AfterWaitForHmdPose);

        std::string submit_error;
        int submit_result = 0;
        Report(phase_callback, OpenVrFlatBridgePhase::BeforeSubmitLeft);
        const bool left_submitted = openvr::SubmitEyeD3D11(
            impl_->runtime, impl_->texture11.Get(), openvr::EyeSubmission::Left,
            submit_result, submit_error);
        Report(phase_callback, OpenVrFlatBridgePhase::AfterSubmitLeft,
            left_submitted ? S_OK : E_FAIL, submit_result);
        if (!left_submitted) {
            impl_->last_error = submit_error;
            return false;
        }

        Report(phase_callback, OpenVrFlatBridgePhase::BeforeSubmitRight);
        const bool right_submitted = openvr::SubmitEyeD3D11(
            impl_->runtime, impl_->texture11.Get(), openvr::EyeSubmission::Right,
            submit_result, submit_error);
        Report(phase_callback, OpenVrFlatBridgePhase::AfterSubmitRight,
            right_submitted ? S_OK : E_FAIL, submit_result);
        if (!right_submitted) {
            impl_->last_error = submit_error;
            return false;
        }

        ++impl_->submitted_frames;
        return true;
    } catch (...) {
        impl_->last_error = "flat bridge threw an exception";
        return false;
    }
}

void OpenVrFlatBridge::BeforeD3D9Reset() noexcept {
    impl_->ReleaseD3D9Resources();
    impl_->texture11.Reset();
}

std::uint64_t OpenVrFlatBridge::submitted_frames() const noexcept {
    return impl_->submitted_frames;
}

std::string_view OpenVrFlatBridge::last_error() const noexcept {
    return impl_->last_error;
}

std::string_view OpenVrFlatBridge::description() const noexcept {
    return impl_->description;
}

} // namespace cojvr::backends::d3d9
