#include "backends/d3d9/openvr_stereo_readback.hpp"

#include "backends/d3d9/content_hash.hpp"
#include "backends/openvr/d3d11_compositor.hpp"
#include "backends/openvr/d3d11_session.hpp"
#include "runtime/openvr_runtime.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

namespace cojvr::backends::d3d9 {
namespace {

using Microsoft::WRL::ComPtr;
constexpr D3DFORMAT kD3dFormatNull = static_cast<D3DFORMAT>(0x4C4C554E); // 'NULL'

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

bool SameDescription(const D3DSURFACE_DESC& lhs, const D3DSURFACE_DESC& rhs) noexcept {
    return lhs.Width == rhs.Width && lhs.Height == rhs.Height &&
        lhs.Format == rhs.Format &&
        lhs.MultiSampleType == rhs.MultiSampleType &&
        lhs.MultiSampleQuality == rhs.MultiSampleQuality;
}

double MillisecondsBetween(
    const std::chrono::steady_clock::time_point begin,
    const std::chrono::steady_clock::time_point end) noexcept {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

std::size_t EyeIndex(const runtime::Eye eye) noexcept {
    return eye == runtime::Eye::left ? 0U : 1U;
}

} // namespace

struct OpenVrStereoReadback::Impl {
    openvr::D3D11SessionBridge d3d11;
    ComPtr<IDirect3DSurface9> system_memory;
    std::array<ComPtr<ID3D11Texture2D>, 2> eye_textures{};
    D3DSURFACE_DESC surface_desc{};
    IDirect3DDevice9* resource_device = nullptr;
    std::array<bool, 2> captured{};
    std::array<std::uint64_t, 2> content_hashes{};
    std::uint64_t capture_sequence = 0;
    std::uint64_t submitted_frames = 0;
    bool initialized = false;
    bool resources_initialized = false;
    std::string last_error;
    std::string description;

    void ReleaseResources() noexcept {
        system_memory.Reset();
        for (auto& texture : eye_textures) texture.Reset();
        surface_desc = {};
        resource_device = nullptr;
        captured = {};
        content_hashes = {};
        capture_sequence = 0;
        resources_initialized = false;
    }

    bool EnsureResources(IDirect3DDevice9* device, IDirect3DSurface9* source_surface) noexcept {
        D3DSURFACE_DESC desc{};
        HRESULT hr = source_surface->GetDesc(&desc);
        if (FAILED(hr)) {
            last_error = Failure("capture surface GetDesc", hr);
            ReleaseResources();
            return false;
        }
        if (resources_initialized && resource_device == device &&
            SameDescription(desc, surface_desc)) {
            return true;
        }

        ReleaseResources();
        if (desc.Format == kD3dFormatNull) {
            last_error =
                "capture source is D3DFMT_NULL auxiliary render target; "
                "the complete color render-view wrapper must finish before eye capture";
            return false;
        }
        const DXGI_FORMAT dxgi_format = ToDxgiFormat(desc.Format);
        if (dxgi_format == DXGI_FORMAT_UNKNOWN) {
            last_error = "unsupported D3D9 capture-surface format " +
                std::to_string(static_cast<unsigned>(desc.Format));
            return false;
        }

        hr = device->CreateOffscreenPlainSurface(
            desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
            &system_memory, nullptr);
        if (FAILED(hr) || !system_memory) {
            last_error = Failure("system-memory surface creation", hr);
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
        for (auto& texture : eye_textures) {
            hr = d3d11.device()->CreateTexture2D(&texture_desc, nullptr, &texture);
            if (FAILED(hr) || !texture) {
                last_error = Failure("D3D11 eye texture creation", hr);
                ReleaseResources();
                return false;
            }
        }

        surface_desc = desc;
        resource_device = device;
        resources_initialized = true;
        std::ostringstream out;
        out << "transport=classic_d3d9_cpu_readback"
            << ";eye_surface=" << desc.Width << 'x' << desc.Height
            << ";format=" << static_cast<unsigned>(desc.Format)
            << ";msaa=" << static_cast<unsigned>(desc.MultiSampleType)
            << ";feature_level=0x" << std::hex
            << static_cast<unsigned>(d3d11.feature_level());
        description = out.str();
        return true;
    }
};

OpenVrStereoReadback::OpenVrStereoReadback() : impl_(std::make_unique<Impl>()) {}
OpenVrStereoReadback::~OpenVrStereoReadback() = default;

bool OpenVrStereoReadback::Initialize(runtime::OpenVrRuntime& runtime) noexcept {
    try {
        impl_->last_error.clear();
        Shutdown();
        if (!runtime.initialized()) {
            impl_->last_error = "OpenVR runtime is not initialized";
            return false;
        }
        if (!impl_->d3d11.Initialize(runtime)) {
            impl_->last_error = std::string(impl_->d3d11.last_error());
            return false;
        }
        impl_->initialized = true;
        return true;
    } catch (...) {
        impl_->last_error = "stereo readback initialization raised an exception";
        return false;
    }
}

void OpenVrStereoReadback::Shutdown() noexcept {
    if (!impl_) return;
    impl_->ReleaseResources();
    impl_->d3d11.Shutdown();
    impl_->initialized = false;
}

bool OpenVrStereoReadback::CaptureEye(
    IDirect3DDevice9* device,
    const runtime::Eye eye,
    const std::uint64_t frame_sequence,
    std::uint64_t& content_hash) noexcept {
    content_hash = 0;
    if (!impl_->initialized || !device || frame_sequence == 0) {
        impl_->last_error = "stereo eye capture is not initialized or lacks a D3D9 device";
        return false;
    }

    try {
        const auto capture_begin = std::chrono::steady_clock::now();
        impl_->last_error.clear();
        ComPtr<IDirect3DSurface9> render_target;
        HRESULT hr = device->GetRenderTarget(0, &render_target);
        if (FAILED(hr) || !render_target) {
            impl_->last_error = Failure("GetRenderTarget(0)", FAILED(hr) ? hr : E_FAIL);
            impl_->ReleaseResources();
            return false;
        }

        ComPtr<IDirect3DSurface9> back_buffer;
        const HRESULT back_buffer_result =
            device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
        const bool source_is_backbuffer = SUCCEEDED(back_buffer_result) && back_buffer &&
            render_target.Get() == back_buffer.Get();

        D3DVIEWPORT9 viewport{};
        const HRESULT viewport_result = device->GetViewport(&viewport);
        const bool viewport_valid = SUCCEEDED(viewport_result);

        if (!impl_->EnsureResources(device, render_target.Get())) return false;

        if (impl_->capture_sequence != frame_sequence) {
            impl_->capture_sequence = frame_sequence;
            impl_->captured = {};
            impl_->content_hashes = {};
        }

        {
            std::ostringstream out;
            out << "transport=classic_d3d9_cpu_readback"
                << ";capture_source=render_target0"
                << ";source_is_backbuffer=" << (source_is_backbuffer ? "true" : "false")
                << ";eye_surface=" << impl_->surface_desc.Width << 'x'
                << impl_->surface_desc.Height
                << ";viewport=";
            if (viewport_valid) {
                out << viewport.X << ',' << viewport.Y << ',' << viewport.Width << ','
                    << viewport.Height << ',' << viewport.MinZ << ',' << viewport.MaxZ;
            } else {
                out << "unavailable";
            }
            out
                << ";format=" << static_cast<unsigned>(impl_->surface_desc.Format)
                << ";msaa=" << static_cast<unsigned>(impl_->surface_desc.MultiSampleType)
                << ";feature_level=0x" << std::hex
                << static_cast<unsigned>(impl_->d3d11.feature_level());
            impl_->description = out.str();
        }

        IDirect3DSurface9* readback_source = render_target.Get();
        ComPtr<IDirect3DSurface9> resolved;
        if (impl_->surface_desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
            hr = device->CreateRenderTarget(
                impl_->surface_desc.Width,
                impl_->surface_desc.Height,
                impl_->surface_desc.Format,
                D3DMULTISAMPLE_NONE,
                0,
                FALSE,
                &resolved,
                nullptr);
            if (FAILED(hr) || !resolved) {
                impl_->last_error = Failure("transient MSAA resolve target creation", hr);
                impl_->ReleaseResources();
                return false;
            }
            hr = device->StretchRect(
                render_target.Get(), nullptr, resolved.Get(), nullptr, D3DTEXF_NONE);
            if (FAILED(hr)) {
                impl_->last_error = Failure("MSAA resolve StretchRect", hr);
                impl_->ReleaseResources();
                return false;
            }
            readback_source = resolved.Get();
        }

        const auto gpu_readback_begin = std::chrono::steady_clock::now();
        hr = device->GetRenderTargetData(readback_source, impl_->system_memory.Get());
        const auto gpu_readback_end = std::chrono::steady_clock::now();
        if (FAILED(hr)) {
            impl_->last_error = Failure("GetRenderTargetData", hr);
            impl_->ReleaseResources();
            return false;
        }

        const auto copy_upload_begin = std::chrono::steady_clock::now();
        D3DLOCKED_RECT locked{};
        hr = impl_->system_memory->LockRect(&locked, nullptr, D3DLOCK_READONLY);
        if (FAILED(hr) || !locked.pBits || locked.Pitch <= 0) {
            impl_->last_error = Failure("system-memory LockRect", FAILED(hr) ? hr : E_FAIL);
            if (SUCCEEDED(hr)) {
                (void)impl_->system_memory->UnlockRect();
            }
            impl_->ReleaseResources();
            return false;
        }
        content_hash = HashBgrxSurfaceIgnoringAlpha(
            locked.pBits,
            static_cast<std::size_t>(locked.Pitch),
            impl_->surface_desc.Width,
            impl_->surface_desc.Height);
        if (content_hash == 0) {
            (void)impl_->system_memory->UnlockRect();
            impl_->last_error = "invalid locked surface layout for content hashing";
            impl_->ReleaseResources();
            return false;
        }
        const std::size_t index = EyeIndex(eye);
        impl_->d3d11.context()->UpdateSubresource(
            impl_->eye_textures[index].Get(), 0, nullptr, locked.pBits,
            static_cast<UINT>(locked.Pitch), 0);
        hr = impl_->system_memory->UnlockRect();
        if (FAILED(hr)) {
            impl_->last_error = Failure("system-memory UnlockRect", hr);
            impl_->ReleaseResources();
            return false;
        }
        impl_->captured[index] = true;
        impl_->content_hashes[index] = content_hash;
        const auto capture_end = std::chrono::steady_clock::now();
        std::ostringstream timing;
        timing << std::fixed << std::setprecision(3)
               << ";gpu_readback_ms="
               << MillisecondsBetween(gpu_readback_begin, gpu_readback_end)
               << ";copy_upload_ms="
               << MillisecondsBetween(copy_upload_begin, capture_end)
               << ";capture_total_ms="
               << MillisecondsBetween(capture_begin, capture_end);
        impl_->description += timing.str();
        return true;
    } catch (...) {
        impl_->last_error = "stereo eye capture raised an exception";
        impl_->ReleaseResources();
        return false;
    }
}

bool OpenVrStereoReadback::Submit(
    runtime::OpenVrRuntime& runtime,
    const std::uint64_t frame_sequence) noexcept {
    if (!impl_->initialized || frame_sequence == 0 ||
        impl_->capture_sequence != frame_sequence ||
        !impl_->captured[0] || !impl_->captured[1]) {
        impl_->last_error = "stereo submission requires two captures from the same frame";
        return false;
    }
    if (impl_->content_hashes[0] == 0 || impl_->content_hashes[1] == 0 ||
        impl_->content_hashes[0] == impl_->content_hashes[1]) {
        impl_->last_error = "stereo submission rejected identical left/right eye content";
        impl_->captured = {};
        impl_->content_hashes = {};
        return false;
    }
    try {
        const auto submit_begin = std::chrono::steady_clock::now();
        std::string error;
        if (!openvr::SubmitStereoD3D11(
                runtime,
                impl_->eye_textures[0].Get(),
                impl_->eye_textures[1].Get(),
                error)) {
            impl_->last_error = error;
            impl_->captured = {};
            impl_->content_hashes = {};
            return false;
        }
        const auto submit_end = std::chrono::steady_clock::now();
        std::ostringstream timing;
        timing << std::fixed << std::setprecision(3)
               << ";submit_ms=" << MillisecondsBetween(submit_begin, submit_end);
        impl_->description += timing.str();
        ++impl_->submitted_frames;
        impl_->captured = {};
        impl_->content_hashes = {};
        return true;
    } catch (...) {
        impl_->last_error = "stereo submission raised an exception";
        impl_->captured = {};
        impl_->content_hashes = {};
        return false;
    }
}

std::uint64_t OpenVrStereoReadback::submitted_frames() const noexcept {
    return impl_->submitted_frames;
}

std::string_view OpenVrStereoReadback::last_error() const noexcept {
    return impl_->last_error;
}

std::string_view OpenVrStereoReadback::description() const noexcept {
    return impl_->description;
}

} // namespace cojvr::backends::d3d9
