#include "backends/d3d9/classic_readback_bridge.hpp"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

namespace cojvr::backends::d3d9 {
namespace {

using Microsoft::WRL::ComPtr;

ComPtr<IDXGIAdapter1> FindDxgiAdapterForMonitor(const HMONITOR monitor) noexcept {
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return {};

    for (UINT adapter_index = 0;; ++adapter_index) {
        ComPtr<IDXGIAdapter1> adapter;
        const HRESULT adapter_hr = factory->EnumAdapters1(adapter_index, &adapter);
        if (adapter_hr == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(adapter_hr)) return {};

        for (UINT output_index = 0;; ++output_index) {
            ComPtr<IDXGIOutput> output;
            const HRESULT output_hr = adapter->EnumOutputs(output_index, &output);
            if (output_hr == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(output_hr)) return {};

            DXGI_OUTPUT_DESC description{};
            if (SUCCEEDED(output->GetDesc(&description)) && description.Monitor == monitor) {
                return adapter;
            }
        }
    }
    return {};
}

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

} // namespace

bool CaptureBackBufferToD3D11(
    IDirect3DDevice9* device,
    std::string& diagnostic) noexcept {
    diagnostic.clear();
    if (device == nullptr) {
        diagnostic = "classic readback requires a D3D9 device";
        return false;
    }

    try {
        D3DDEVICE_CREATION_PARAMETERS creation{};
        HRESULT hr = device->GetCreationParameters(&creation);
        if (FAILED(hr)) {
            diagnostic = Failure("GetCreationParameters", hr);
            return false;
        }

        ComPtr<IDirect3D9> d3d9;
        hr = device->GetDirect3D(&d3d9);
        if (FAILED(hr) || !d3d9) {
            diagnostic = Failure("GetDirect3D", hr);
            return false;
        }

        ComPtr<IDirect3DSurface9> back_buffer;
        hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
        if (FAILED(hr) || !back_buffer) {
            diagnostic = Failure("GetBackBuffer", hr);
            return false;
        }

        D3DSURFACE_DESC back_buffer_desc{};
        hr = back_buffer->GetDesc(&back_buffer_desc);
        if (FAILED(hr)) {
            diagnostic = Failure("backbuffer GetDesc", hr);
            return false;
        }

        const DXGI_FORMAT dxgi_format = ToDxgiFormat(back_buffer_desc.Format);
        if (dxgi_format == DXGI_FORMAT_UNKNOWN) {
            std::ostringstream out;
            out << "unsupported D3D9 backbuffer format "
                << static_cast<unsigned>(back_buffer_desc.Format);
            diagnostic = out.str();
            return false;
        }

        IDirect3DSurface9* readback_source = back_buffer.Get();
        ComPtr<IDirect3DSurface9> resolved;
        if (back_buffer_desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
            hr = device->CreateRenderTarget(
                back_buffer_desc.Width,
                back_buffer_desc.Height,
                back_buffer_desc.Format,
                D3DMULTISAMPLE_NONE,
                0,
                FALSE,
                &resolved,
                nullptr);
            if (FAILED(hr) || !resolved) {
                diagnostic = Failure("non-MSAA render-target creation", hr);
                return false;
            }
            hr = device->StretchRect(
                back_buffer.Get(), nullptr, resolved.Get(), nullptr, D3DTEXF_NONE);
            if (FAILED(hr)) {
                diagnostic = Failure("MSAA resolve StretchRect", hr);
                return false;
            }
            readback_source = resolved.Get();
        }

        ComPtr<IDirect3DSurface9> system_memory;
        hr = device->CreateOffscreenPlainSurface(
            back_buffer_desc.Width,
            back_buffer_desc.Height,
            back_buffer_desc.Format,
            D3DPOOL_SYSTEMMEM,
            &system_memory,
            nullptr);
        if (FAILED(hr) || !system_memory) {
            diagnostic = Failure("system-memory surface creation", hr);
            return false;
        }

        hr = device->GetRenderTargetData(readback_source, system_memory.Get());
        if (FAILED(hr)) {
            diagnostic = Failure("GetRenderTargetData", hr);
            return false;
        }

        D3DLOCKED_RECT locked{};
        hr = system_memory->LockRect(&locked, nullptr, D3DLOCK_READONLY);
        if (FAILED(hr) || locked.pBits == nullptr || locked.Pitch <= 0) {
            diagnostic = Failure("system-memory LockRect", hr);
            return false;
        }

        const auto* d3d9_pixel = static_cast<const std::uint8_t*>(locked.pBits);
        const std::array<std::uint8_t, 4> expected_pixel{
            d3d9_pixel[0], d3d9_pixel[1], d3d9_pixel[2], d3d9_pixel[3]};
        const UINT source_pitch = static_cast<UINT>(locked.Pitch);
        const std::size_t source_size =
            static_cast<std::size_t>(source_pitch) * back_buffer_desc.Height;
        std::vector<std::uint8_t> source_pixels(source_size);
        std::memcpy(source_pixels.data(), locked.pBits, source_size);
        system_memory->UnlockRect();

        const HMONITOR monitor = d3d9->GetAdapterMonitor(creation.AdapterOrdinal);
        ComPtr<IDXGIAdapter1> adapter = FindDxgiAdapterForMonitor(monitor);
        if (!adapter) {
            diagnostic = "no DXGI adapter matched the D3D9 adapter monitor";
            return false;
        }

        constexpr std::array<D3D_FEATURE_LEVEL, 3> feature_levels{
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        ComPtr<ID3D11Device> device11;
        ComPtr<ID3D11DeviceContext> context11;
        D3D_FEATURE_LEVEL feature_level{};
        hr = D3D11CreateDevice(
            adapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            feature_levels.data(),
            static_cast<UINT>(feature_levels.size()),
            D3D11_SDK_VERSION,
            &device11,
            &feature_level,
            &context11);
        if (FAILED(hr)) {
            diagnostic = Failure("D3D11CreateDevice", hr);
            return false;
        }

        D3D11_TEXTURE2D_DESC texture_desc{};
        texture_desc.Width = back_buffer_desc.Width;
        texture_desc.Height = back_buffer_desc.Height;
        texture_desc.MipLevels = 1;
        texture_desc.ArraySize = 1;
        texture_desc.Format = dxgi_format;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Usage = D3D11_USAGE_DEFAULT;
        texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        ComPtr<ID3D11Texture2D> texture11;
        hr = device11->CreateTexture2D(&texture_desc, nullptr, &texture11);
        if (FAILED(hr) || !texture11) {
            diagnostic = Failure("D3D11 texture creation", hr);
            return false;
        }

        context11->UpdateSubresource(
            texture11.Get(), 0, nullptr, source_pixels.data(), source_pitch, 0);

        D3D11_TEXTURE2D_DESC staging_desc = texture_desc;
        staging_desc.Usage = D3D11_USAGE_STAGING;
        staging_desc.BindFlags = 0;
        staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;
        hr = device11->CreateTexture2D(&staging_desc, nullptr, &staging);
        if (FAILED(hr) || !staging) {
            diagnostic = Failure("D3D11 staging texture creation", hr);
            return false;
        }

        context11->CopyResource(staging.Get(), texture11.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr = context11->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr) || mapped.pData == nullptr) {
            diagnostic = Failure("D3D11 staging Map", hr);
            return false;
        }
        const auto* uploaded_pixel = static_cast<const std::uint8_t*>(mapped.pData);
        const bool pixel_matches =
            uploaded_pixel[0] == expected_pixel[0] &&
            uploaded_pixel[1] == expected_pixel[1] &&
            uploaded_pixel[2] == expected_pixel[2] &&
            uploaded_pixel[3] == expected_pixel[3];
        context11->Unmap(staging.Get(), 0);
        if (!pixel_matches) {
            diagnostic = "D3D9/D3D11 verification pixel mismatch";
            return false;
        }

        std::ostringstream out;
        out << "success backbuffer=" << back_buffer_desc.Width << 'x'
            << back_buffer_desc.Height
            << " format=" << static_cast<unsigned>(back_buffer_desc.Format)
            << " msaa=" << static_cast<unsigned>(back_buffer_desc.MultiSampleType)
            << " feature_level=0x" << std::hex << static_cast<unsigned>(feature_level);
        diagnostic = out.str();
        return true;
    } catch (...) {
        diagnostic = "classic readback diagnostic threw an exception";
        return false;
    }
}

} // namespace cojvr::backends::d3d9
