#include "backends/d3d9/d3d9_stereo_capture.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "backends/openvr/d3d9_shared_texture_bridge.hpp"

#include <windows.h>

#include <d3d9.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

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

cojvr::runtime::Pose TestPose() {
    cojvr::runtime::Pose pose{};
    pose.position = {0.25F, 1.70F, -0.5F};
    pose.position_valid = true;
    pose.orientation = {0.0F, 0.0F, 0.0F, 1.0F};
    pose.orientation_valid = true;
    return pose;
}

bool CaptureGpuFrame(
    cojvr::backends::d3d9::D3D9StereoCapture& capture,
    IDirect3DDevice9* device,
    IDirect3DSurface9* source,
    const std::uint64_t sequence,
    cojvr::backends::d3d9::StereoCpuFrame& frame) {
    HRESULT hr = device->ColorFill(source, nullptr, D3DCOLOR_ARGB(0xFF, 0x12, 0x34, 0x56));
    if (FAILED(hr) || !capture.CaptureEyeSurface(
            device, source, cojvr::runtime::Eye::left, sequence, 1)) {
        return false;
    }
    hr = device->ColorFill(source, nullptr, D3DCOLOR_ARGB(0xFF, 0xA2, 0xB4, 0xC6));
    if (FAILED(hr) || !capture.CaptureEyeSurface(
            device, source, cojvr::runtime::Eye::right, sequence, 1) ||
        !capture.EndFrame(sequence, TestPose(), sequence + 100)) {
        return false;
    }
    for (int attempt = 0; attempt < 200; ++attempt) {
        (void)device->Present(nullptr, nullptr, nullptr, nullptr);
        if (capture.TryCollectReady(frame)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

} // namespace

int main() {
    const auto create_ex = cojvr::backends::d3d9::SystemDirect3DCreate9Ex();
    if (!create_ex) return Fail("System d3d9.dll has no Direct3DCreate9Ex");

    ComPtr<IDirect3D9Ex> d3d9;
    HRESULT hr = create_ex(D3D_SDK_VERSION, &d3d9);
    if (FAILED(hr) || !d3d9) return Fail("Direct3DCreate9Ex failed", hr);

    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = GetDesktopWindow();
    present.BackBufferFormat = D3DFMT_UNKNOWN;
    present.BackBufferWidth = 64;
    present.BackBufferHeight = 32;

    ComPtr<IDirect3DDevice9Ex> device9ex;
    hr = d3d9->CreateDeviceEx(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, present.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &present, nullptr, &device9ex);
    if (FAILED(hr) || !device9ex) {
        if (hr == D3DERR_NOTAVAILABLE || hr == D3DERR_DEVICELOST) {
            std::cout << "D3D9Ex GPU transport test skipped: HAL device unavailable\n";
            return 77;
        }
        return Fail("D3D9Ex CreateDeviceEx failed", hr);
    }

    ComPtr<IDirect3DTexture9> source_texture;
    hr = device9ex->CreateTexture(
        64, 32, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, &source_texture, nullptr);
    if (FAILED(hr) || !source_texture) return Fail("D3D9Ex source texture creation failed", hr);
    ComPtr<IDirect3DSurface9> source;
    hr = source_texture->GetSurfaceLevel(0, &source);
    if (FAILED(hr) || !source) return Fail("D3D9Ex source surface retrieval failed", hr);

    cojvr::backends::d3d9::D3D9StereoCapture capture;
    cojvr::backends::d3d9::StereoCpuFrame frame{};
    if (!CaptureGpuFrame(capture, device9ex.Get(), source.Get(), 1, frame)) {
        std::cerr << capture.last_error() << '\n';
        return Fail("D3D9Ex capture did not produce a shared frame");
    }
    if (!capture.gpu_resident_active() ||
        frame.transport !=
            cojvr::backends::d3d9::StereoFrameTransport::d3d9ex_shared_texture ||
        frame.shared_eyes[0].shared_handle == 0 ||
        frame.shared_eyes[1].shared_handle == 0 ||
        frame.shared_eyes[0].shared_handle == frame.shared_eyes[1].shared_handle ||
        !frame.eyes[0].pixels.empty() || !frame.eyes[1].pixels.empty()) {
        return Fail("D3D9Ex capture crossed or aliased the expected GPU-resident boundary");
    }

    LUID d3d9_luid{};
    hr = d3d9->GetAdapterLUID(D3DADAPTER_DEFAULT, &d3d9_luid);
    if (FAILED(hr)) return Fail("D3D9Ex GetAdapterLUID failed", hr);
    ComPtr<IDXGIFactory1> dxgi_factory;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
    if (FAILED(hr)) return Fail("CreateDXGIFactory1 failed", hr);
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT index = 0;; ++index) {
        ComPtr<IDXGIAdapter1> candidate;
        hr = dxgi_factory->EnumAdapters1(index, &candidate);
        if (hr == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(hr)) return Fail("EnumAdapters1 failed", hr);
        DXGI_ADAPTER_DESC1 desc{};
        candidate->GetDesc1(&desc);
        if (SameLuid(desc.AdapterLuid, d3d9_luid)) {
            adapter = std::move(candidate);
            break;
        }
    }
    if (!adapter) return Fail("No DXGI adapter matched the D3D9Ex producer");

    constexpr D3D_FEATURE_LEVEL levels[]{
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    ComPtr<ID3D11Device> device11;
    ComPtr<ID3D11DeviceContext> context11;
    D3D_FEATURE_LEVEL level{};
    hr = D3D11CreateDevice(
        adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, static_cast<UINT>(std::size(levels)),
        D3D11_SDK_VERSION, &device11, &level, &context11);
    if (FAILED(hr)) return Fail("D3D11CreateDevice failed", hr);

    std::array<ComPtr<ID3D11Texture2D>, 2> destinations{};
    for (auto& destination : destinations) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = 64;
        desc.Height = 32;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        hr = device11->CreateTexture2D(&desc, nullptr, &destination);
        if (FAILED(hr)) return Fail("D3D11 destination texture creation failed", hr);
    }

    cojvr::backends::openvr::D3D9SharedTextureBridge bridge;
    cojvr::backends::openvr::D3D9SharedTextureCopyTiming timing{};
    const std::array<ID3D11Texture2D*, 2> raw_destinations{
        destinations[0].Get(), destinations[1].Get()};
    if (!bridge.CopyFrame(
            device11.Get(), context11.Get(), frame, raw_destinations, timing)) {
        std::cerr << bridge.last_error() << '\n';
        return Fail("D3D9Ex shared frame did not copy into D3D11");
    }
    if (timing.consumer_wait_ms != 0.0 || timing.pending_copy_fences == 0) {
        return Fail("GPU consumer unexpectedly waited or failed to retain the producer lease");
    }
    for (int attempt = 0; attempt < 200; ++attempt) {
        bridge.Poll(context11.Get());
        if (bridge.stats().copy_fences_completed == 1) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (bridge.stats().copy_fences_completed != 1 ||
        bridge.stats().pending_copy_fences != 0) {
        return Fail("D3D11 copy fence did not release the shared producer slot");
    }

    constexpr std::array<std::array<std::uint8_t, 4>, 2> expected{{
        {{0x56, 0x34, 0x12, 0xFF}},
        {{0xC6, 0xB4, 0xA2, 0xFF}},
    }};
    for (std::size_t eye = 0; eye < destinations.size(); ++eye) {
        D3D11_TEXTURE2D_DESC staging_desc{};
        destinations[eye]->GetDesc(&staging_desc);
        staging_desc.Usage = D3D11_USAGE_STAGING;
        staging_desc.BindFlags = 0;
        staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;
        hr = device11->CreateTexture2D(&staging_desc, nullptr, &staging);
        if (FAILED(hr)) return Fail("D3D11 staging texture creation failed", hr);
        context11->CopyResource(staging.Get(), destinations[eye].Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr = context11->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) return Fail("D3D11 staging texture map failed", hr);
        const auto* pixel = static_cast<const std::uint8_t*>(mapped.pData);
        const bool matches = pixel[0] == expected[eye][0] &&
            pixel[1] == expected[eye][1] && pixel[2] == expected[eye][2] &&
            pixel[3] == expected[eye][3];
        context11->Unmap(staging.Get(), 0);
        if (!matches) return Fail("GPU-resident transport changed an eye's pixel data");
    }

    // Acquiring the next producer slot also reclaims the completed consumer
    // lease without waiting or flushing the D3D9 device.
    cojvr::backends::d3d9::StereoCpuFrame second{};
    if (!CaptureGpuFrame(capture, device9ex.Get(), source.Get(), 2, second)) {
        return Fail("producer ring did not continue after the consumer fence");
    }
    const auto capture_stats = capture.stats();
    if (capture_stats.consumer_releases == 0 ||
        capture_stats.shared_frames_published != 2 ||
        capture_stats.cpu_fallback_frames != 0 ||
        capture_stats.fallback_activations != 0) {
        return Fail("GPU-resident producer accounting is inconsistent");
    }

    if (!bridge.CopyFrame(
            device11.Get(), context11.Get(), second, raw_destinations, timing)) {
        std::cerr << bridge.last_error() << '\n';
        return Fail("second shared frame did not queue for shutdown-drain coverage");
    }
    bridge.Shutdown(context11.Get());
    const auto shutdown_stats = bridge.stats();
    if (shutdown_stats.pending_copy_fences != 0 ||
        shutdown_stats.abandoned_on_shutdown != 0 ||
        shutdown_stats.copy_fences_completed != shutdown_stats.frames_copied) {
        return Fail("shared-texture shutdown did not retire every queued GPU copy before releasing producer leases");
    }

    std::cout << "D3D9Ex shared ring -> D3D11 GPU copy passed: "
              << capture.collect_description()
              << ";consumer_copy_queue_ms=" << timing.copy_queue_ms << '\n';
    return 0;
}
