#include "backends/d3d9/system_d3d9.hpp"

#include <windows.h>

#include <d3d9.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cstdint>
#include <iostream>
#include <iterator>

using Microsoft::WRL::ComPtr;

namespace {

int Fail(const char* message, const HRESULT hr = S_OK) {
    std::cerr << message;
    if (FAILED(hr)) {
        std::cerr << " (HRESULT 0x" << std::hex << static_cast<unsigned long>(hr) << ')';
    }
    std::cerr << '\n';
    return 1;
}

bool SameLuid(const LUID& left, const LUID& right) {
    return left.HighPart == right.HighPart && left.LowPart == right.LowPart;
}

} // namespace

int main() {
    const auto create_d3d9_ex = cojvr::backends::d3d9::SystemDirect3DCreate9Ex();
    if (create_d3d9_ex == nullptr) return Fail("System d3d9.dll has no Direct3DCreate9Ex");

    ComPtr<IDirect3D9Ex> d3d9;
    HRESULT hr = create_d3d9_ex(D3D_SDK_VERSION, &d3d9);
    if (FAILED(hr)) return Fail("Direct3DCreate9Ex failed", hr);

    LUID d3d9_luid{};
    hr = d3d9->GetAdapterLUID(D3DADAPTER_DEFAULT, &d3d9_luid);
    if (FAILED(hr)) return Fail("GetAdapterLUID failed", hr);

    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = GetDesktopWindow();
    present.BackBufferFormat = D3DFMT_UNKNOWN;
    present.BackBufferWidth = 64;
    present.BackBufferHeight = 64;

    ComPtr<IDirect3DDevice9Ex> device9;
    hr = d3d9->CreateDeviceEx(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        present.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &present,
        nullptr,
        &device9);
    if (FAILED(hr)) return Fail("CreateDeviceEx failed", hr);

    IDirect3DDevice9* classic_device = device9.Get();
    hr = classic_device->Clear(
        0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0xFF, 0x08, 0x10, 0x18), 1.0F, 0);
    if (FAILED(hr)) return Fail("D3D9Ex device failed through IDirect3DDevice9::Clear", hr);
    hr = classic_device->Present(nullptr, nullptr, nullptr, nullptr);
    if (FAILED(hr)) return Fail("D3D9Ex device failed through IDirect3DDevice9::Present", hr);
    hr = classic_device->Reset(&present);
    if (FAILED(hr)) return Fail("D3D9Ex device failed through IDirect3DDevice9::Reset", hr);

    HANDLE shared_handle = nullptr;
    ComPtr<IDirect3DTexture9> texture9;
    hr = classic_device->CreateTexture(
        64, 64, 1,
        D3DUSAGE_RENDERTARGET,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        &texture9,
        &shared_handle);
    if (FAILED(hr) || shared_handle == nullptr) {
        return Fail("D3D9Ex shared render-target creation failed", hr);
    }

    ComPtr<IDirect3DSurface9> surface9;
    hr = texture9->GetSurfaceLevel(0, &surface9);
    if (FAILED(hr)) return Fail("GetSurfaceLevel failed", hr);
    hr = classic_device->ColorFill(surface9.Get(), nullptr, D3DCOLOR_ARGB(0xFF, 0x12, 0x34, 0x56));
    if (FAILED(hr)) return Fail("ColorFill failed", hr);

    ComPtr<IDirect3DQuery9> event_query;
    hr = classic_device->CreateQuery(D3DQUERYTYPE_EVENT, &event_query);
    if (FAILED(hr)) return Fail("D3D9 event query creation failed", hr);
    hr = event_query->Issue(D3DISSUE_END);
    if (FAILED(hr)) return Fail("D3D9 event query issue failed", hr);
    while ((hr = event_query->GetData(nullptr, 0, D3DGETDATA_FLUSH)) == S_FALSE) {
        Sleep(0);
    }
    if (FAILED(hr)) return Fail("D3D9 event query synchronization failed", hr);

    ComPtr<IDXGIFactory1> factory;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return Fail("CreateDXGIFactory1 failed", hr);

    ComPtr<IDXGIAdapter1> matching_adapter;
    for (UINT index = 0;; ++index) {
        ComPtr<IDXGIAdapter1> candidate;
        hr = factory->EnumAdapters1(index, &candidate);
        if (hr == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(hr)) return Fail("EnumAdapters1 failed", hr);

        DXGI_ADAPTER_DESC1 description{};
        hr = candidate->GetDesc1(&description);
        if (FAILED(hr)) return Fail("GetDesc1 failed", hr);
        if (SameLuid(description.AdapterLuid, d3d9_luid)) {
            matching_adapter = candidate;
            break;
        }
    }
    if (!matching_adapter) return Fail("No DXGI adapter matched the D3D9Ex adapter LUID");

    constexpr D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    ComPtr<ID3D11Device> device11;
    ComPtr<ID3D11DeviceContext> context11;
    D3D_FEATURE_LEVEL feature_level{};
    hr = D3D11CreateDevice(
        matching_adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        feature_levels,
        static_cast<UINT>(std::size(feature_levels)),
        D3D11_SDK_VERSION,
        &device11,
        &feature_level,
        &context11);
    if (FAILED(hr)) return Fail("D3D11CreateDevice on matching adapter failed", hr);

    ComPtr<ID3D11Texture2D> texture11;
    hr = device11->OpenSharedResource(shared_handle, IID_PPV_ARGS(&texture11));
    if (FAILED(hr)) return Fail("D3D11 OpenSharedResource failed", hr);

    D3D11_TEXTURE2D_DESC description{};
    texture11->GetDesc(&description);
    if (description.Width != 64 || description.Height != 64) {
        return Fail("Shared texture dimensions changed across D3D9Ex/D3D11");
    }

    D3D11_TEXTURE2D_DESC staging_desc = description;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    hr = device11->CreateTexture2D(&staging_desc, nullptr, &staging);
    if (FAILED(hr)) return Fail("D3D11 staging texture creation failed", hr);

    context11->CopyResource(staging.Get(), texture11.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = context11->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return Fail("D3D11 staging texture map failed", hr);

    const auto* pixel = static_cast<const std::uint8_t*>(mapped.pData);
    const bool expected_bgra = pixel[0] == 0x56 && pixel[1] == 0x34 &&
        pixel[2] == 0x12 && pixel[3] == 0xFF;
    context11->Unmap(staging.Get(), 0);
    if (!expected_bgra) {
        return Fail("Shared texture pixel data did not cross the API boundary intact");
    }

    std::cout << "D3D9Ex -> D3D11 shared texture interop passed; DXGI format="
              << static_cast<unsigned>(description.Format)
              << ", feature_level=0x" << std::hex << static_cast<unsigned>(feature_level)
              << '\n';

    return 0;
}
