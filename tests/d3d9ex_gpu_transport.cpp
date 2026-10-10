#include "backends/d3d9/d3d9_stereo_capture.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "backends/openvr/d3d9_shared_texture_bridge.hpp"
#include "backends/openvr/d3d11_sync.hpp"

#include <windows.h>

#include <d3d9.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
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
    cojvr::backends::d3d9::StereoCpuFrame& frame,
    const std::uint64_t generation = 1) {
    HRESULT hr = device->ColorFill(source, nullptr, D3DCOLOR_ARGB(0xFF, 0x12, 0x34, 0x56));
    if (FAILED(hr) || !capture.CaptureEyeSurface(
            device, source, cojvr::runtime::Eye::left, sequence, generation)) {
        return false;
    }
    hr = device->ColorFill(source, nullptr, D3DCOLOR_ARGB(0xFF, 0xA2, 0xB4, 0xC6));
    if (FAILED(hr) || !capture.CaptureEyeSurface(
            device, source, cojvr::runtime::Eye::right, sequence, generation) ||
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

    // A failed per-copy query is not proof of retirement. An independent GPU
    // boundary must eventually make the capture ring usable again, without a
    // wait on the producer/presenter or unsafe release during Reset generation.
    {
        cojvr::backends::d3d9::D3D9StereoCapture recovering_capture;
        cojvr::backends::openvr::D3D9SharedTextureBridge recovering_bridge;
        cojvr::backends::d3d9::StereoCpuFrame recovering_frame;
        if (!CaptureGpuFrame(recovering_capture, device9ex.Get(), source.Get(), 101, recovering_frame) ||
            !recovering_bridge.CopyFrame(device11.Get(), context11.Get(), recovering_frame,
                raw_destinations, timing)) {
            return Fail("recovery fixture did not queue its real leased GPU copy");
        }
        recovering_bridge.ForceFencePendingForTest(true);
        if (!CaptureGpuFrame(recovering_capture, device9ex.Get(), source.Get(), 104, recovering_frame) ||
            !recovering_bridge.CopyFrame(device11.Get(), context11.Get(), recovering_frame,
                raw_destinations, timing) || recovering_bridge.stats().pending_copy_fences != 2) {
            return Fail("recovery fixture did not retain multiple pending GPU copies");
        }
        recovering_bridge.ForceNextFencePollFailureForTest();
        recovering_bridge.ForceRecoveryFencePendingForTest(true);
        recovering_bridge.Poll(context11.Get());
        recovering_bridge.Poll(context11.Get());
        recovering_bridge.Poll(context11.Get());
        if (recovering_bridge.stats().recovery_queries_started != 1 ||
            recovering_bridge.stats().recoveries_completed != 0 ||
            recovering_bridge.stats().pending_copy_fences != 2) {
            return Fail("pending recovery proof released a producer lease");
        }
        hr = device9ex->ResetEx(&present, nullptr);
        if (FAILED(hr)) return Fail("recovery fixture ResetEx failed", hr);
        recovering_capture.InvalidateResources();
        if (!recovering_capture.gpu_resident_active() || recovering_capture.stats().consumer_releases != 0 ||
            recovering_capture.CaptureEyeSurface(device9ex.Get(), source.Get(),
                cojvr::runtime::Eye::left, 102, 2)) {
            return Fail("Reset generation reused an uncertain producer lease");
        }
        recovering_bridge.ForceRecoveryFencePendingForTest(false);
        recovering_bridge.ForceFencePendingForTest(false);
        for (int attempt = 0; attempt < 200; ++attempt) {
            recovering_bridge.Poll(context11.Get());
            if (recovering_bridge.stats().pending_copy_fences == 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const auto recovered = recovering_bridge.stats();
        // Drain physical work even on RED, before the artificial fixture dies.
        cojvr::backends::openvr::D3D11SyncResult drain;
        std::string drain_error;
        if (!cojvr::backends::openvr::SynchronizeD3D11(device11.Get(), context11.Get(),
                cojvr::backends::openvr::D3D11SyncStrategy::event_query, 1000, drain, drain_error)) {
            return Fail("recovery fixture could not safely retire its physical GPU work");
        }
        if (recovered.pending_copy_fences != 0 || recovered.copy_fences_completed != 2 ||
            recovered.copy_failures != 1 || recovered.recovery_first_copy != 1 ||
            recovered.recovery_last_copy != 2) {
            std::cerr << "recovery stats: pending=" << recovered.pending_copy_fences
                      << " completed=" << recovered.copy_fences_completed
                      << " copy_failures=" << recovered.copy_failures
                      << " queries=" << recovered.recovery_queries_started
                      << " recoveries=" << recovered.recoveries_completed
                      << " recovery_failures=" << recovered.recovery_failures
                      << " pending_polls=" << recovered.recovery_poll_pending
                      << " error=" << recovering_bridge.last_error() << '\n';
            return Fail("independent completion boundary did not recover the quarantined GPU lease");
        }
        if (!CaptureGpuFrame(recovering_capture, device9ex.Get(), source.Get(), 102, recovering_frame, 2) ||
            !recovering_bridge.CopyFrame(device11.Get(), context11.Get(), recovering_frame,
                raw_destinations, timing)) {
            return Fail("retired copies did not permit a fresh producer generation");
        }
        // A second fault keeps both source and consumer replacements deferred.
        recovering_bridge.ForceNextFencePollFailureForTest();
        recovering_bridge.ForceRecoveryFencePendingForTest(true);
        recovering_bridge.Poll(context11.Get());
        ComPtr<IDirect3DDevice9Ex> replacement9;
        hr = d3d9->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, present.hDeviceWindow,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
            &present, nullptr, &replacement9);
        if (FAILED(hr)) return Fail("replacement producer device creation failed", hr);
        ComPtr<IDirect3DTexture9> replacement_texture;
        hr = replacement9->CreateTexture(64, 32, 1, D3DUSAGE_RENDERTARGET,
            D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &replacement_texture, nullptr);
        if (FAILED(hr)) return Fail("replacement producer texture creation failed", hr);
        ComPtr<IDirect3DSurface9> replacement_surface;
        hr = replacement_texture->GetSurfaceLevel(0, &replacement_surface);
        if (FAILED(hr)) return Fail("replacement producer surface retrieval failed", hr);
        if (recovering_capture.CaptureEyeSurface(replacement9.Get(), replacement_surface.Get(),
                cojvr::runtime::Eye::left, 103, 3)) {
            return Fail("producer replacement reused a pending GPU lease");
        }
        ComPtr<ID3D11Device> replacement11;
        ComPtr<ID3D11DeviceContext> replacement_context;
        hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, static_cast<UINT>(std::size(levels)),
            D3D11_SDK_VERSION, &replacement11, &level, &replacement_context);
        if (FAILED(hr)) return Fail("replacement consumer creation failed", hr);
        std::array<ComPtr<ID3D11Texture2D>, 2> replacement_destinations;
        D3D11_TEXTURE2D_DESC destination_desc{};
        destinations[0]->GetDesc(&destination_desc);
        for (auto& destination : replacement_destinations) {
            hr = replacement11->CreateTexture2D(&destination_desc, nullptr, &destination);
            if (FAILED(hr)) return Fail("replacement consumer texture creation failed", hr);
        }
        const std::array<ID3D11Texture2D*, 2> replacement_raw{
            replacement_destinations[0].Get(), replacement_destinations[1].Get()};
        // Synthetic leases let these negative admissions observe ownership
        // without publishing additional producer frames.
        auto make_probe = [&recovering_frame](const std::uint64_t generation,
            const std::shared_ptr<cojvr::backends::d3d9::ProducerFrameReleaseState>& state) {
            cojvr::backends::d3d9::StereoCpuFrame probe;
            probe.transport = recovering_frame.transport;
            probe.device_id = recovering_frame.device_id;
            probe.generation = generation;
            probe.shared_eyes = recovering_frame.shared_eyes;
            probe.producer_lease = cojvr::backends::d3d9::ProducerFrameLease(state);
            return probe;
        };
        auto deferred_state = std::make_shared<cojvr::backends::d3d9::ProducerFrameReleaseState>();
        auto deferred = make_probe(2, deferred_state);
        recovering_bridge.Poll(replacement_context.Get());
        if (recovering_bridge.CopyFrame(replacement11.Get(), replacement_context.Get(), deferred,
                replacement_raw, timing) || !deferred.producer_lease.valid() ||
            recovering_bridge.stats().pending_copy_fences != 1 ||
            recovering_bridge.stats().copy_fences_completed != 2 ||
            recovering_bridge.stats().transition_deferrals != 1) {
            return Fail("consumer replacement polled or released an old-context GPU lease");
        }
        deferred.producer_lease.Release();
        auto wrong_state = std::make_shared<cojvr::backends::d3d9::ProducerFrameReleaseState>();
        auto wrong = make_probe(2, wrong_state);
        if (recovering_bridge.CopyFrame(replacement11.Get(), context11.Get(), wrong,
                replacement_raw, timing) ||
            recovering_bridge.CopyFrame(device11.Get(), context11.Get(), wrong,
                replacement_raw, timing) || !wrong.producer_lease.valid() ||
            recovering_bridge.stats().identity_rejections != 2) {
            return Fail("mismatched consumer context or destination was admitted");
        }
        wrong.producer_lease.Release();
        recovering_bridge.ForceRecoveryFencePendingForTest(false);
        for (int attempt = 0; attempt < 200; ++attempt) {
            recovering_bridge.Poll(context11.Get());
            if (recovering_bridge.stats().pending_copy_fences == 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (recovering_bridge.stats().pending_copy_fences != 0 ||
            !CaptureGpuFrame(recovering_capture, replacement9.Get(), replacement_surface.Get(),
                103, recovering_frame, 3) ||
            !recovering_bridge.CopyFrame(device11.Get(), context11.Get(), recovering_frame,
                raw_destinations, timing)) {
            return Fail("retirement did not permit producer replacement with a fresh generation");
        }
        auto stale_state = std::make_shared<cojvr::backends::d3d9::ProducerFrameReleaseState>();
        auto stale = make_probe(2, stale_state);
        if (recovering_bridge.CopyFrame(device11.Get(), context11.Get(), stale, raw_destinations, timing) ||
            !stale.producer_lease.valid() || recovering_bridge.stats().identity_rejections != 3) {
            return Fail("stale producer generation was admitted after replacement");
        }
        stale.producer_lease.Release();
        recovering_bridge.Shutdown(context11.Get());
        auto replaced_state = std::make_shared<cojvr::backends::d3d9::ProducerFrameReleaseState>();
        auto replaced = make_probe(3, replaced_state);
        if (!recovering_bridge.CopyFrame(replacement11.Get(), replacement_context.Get(), replaced,
                replacement_raw, timing)) {
            return Fail("retired consumer could not safely rebind its shared resources");
        }
        recovering_bridge.Shutdown(replacement_context.Get());
        recovering_capture.Shutdown();
        if (recovering_bridge.stats().copy_fences_completed != 5 ||
            recovering_bridge.stats().recoveries_completed != 2 ||
            recovering_bridge.stats().recovery_queries_started != 2 ||
            recovering_bridge.stats().recovery_first_copy != 3 ||
            recovering_bridge.stats().recovery_last_copy != 3 ||
            recovering_bridge.stats().copy_failures != 2 ||
            recovering_bridge.stats().abandoned_on_shutdown != 0 ||
            recovering_capture.stats().consumer_releases != 4 || recovering_capture.stats().ring_depth != 0 ||
            !replaced_state->consumer_done.load(std::memory_order_acquire)) {
            return Fail("recovered transport did not retain healthy shutdown accounting");
        }
    }

    // Inject the exact GetData failure branch against a real queued GPU copy.
    // Use independent synthetic lease states on still-live shared eye textures
    // to observe retirement without interfering with the capture ring owner.
    auto retained_state = std::make_shared<cojvr::backends::d3d9::ProducerFrameReleaseState>();
    auto rejected_state = std::make_shared<cojvr::backends::d3d9::ProducerFrameReleaseState>();
    {
        cojvr::backends::openvr::D3D9SharedTextureBridge failed_bridge;
        cojvr::backends::d3d9::StereoCpuFrame failed_frame{};
        failed_frame.transport = cojvr::backends::d3d9::StereoFrameTransport::d3d9ex_shared_texture;
        failed_frame.device_id = frame.device_id;
        failed_frame.generation = frame.generation;
        failed_frame.shared_eyes = second.shared_eyes;
        failed_frame.producer_lease = cojvr::backends::d3d9::ProducerFrameLease(retained_state);
        if (!failed_bridge.CopyFrame(
                device11.Get(), context11.Get(), failed_frame, raw_destinations, timing)) {
            return Fail("failed-query fixture did not queue its GPU copy");
        }
        failed_bridge.ForceNextFencePollFailureForTest();
        failed_bridge.ForceNextRecoveryFencePollFailureForTest();
        failed_bridge.Poll(context11.Get());
        const auto failed_stats = failed_bridge.stats();
        if (failed_stats.frames_copied != 1 || failed_stats.copy_fences_completed != 0 ||
            failed_stats.pending_copy_fences != 1 || failed_stats.copy_failures != 1 ||
            retained_state->consumer_done.load(std::memory_order_acquire)) {
            return Fail("failed GetData prematurely retired a producer GPU lease");
        }

        cojvr::backends::d3d9::StereoCpuFrame rejected_frame{};
        rejected_frame.transport = cojvr::backends::d3d9::StereoFrameTransport::d3d9ex_shared_texture;
        rejected_frame.device_id = frame.device_id;
        rejected_frame.generation = frame.generation;
        rejected_frame.shared_eyes = second.shared_eyes;
        rejected_frame.producer_lease = cojvr::backends::d3d9::ProducerFrameLease(rejected_state);
        if (failed_bridge.CopyFrame(
                device11.Get(), context11.Get(), rejected_frame, raw_destinations, timing) ||
            !rejected_frame.producer_lease.valid() ||
            failed_bridge.stats().frames_copied != 1 ||
            retained_state->consumer_done.load(std::memory_order_acquire) ||
            rejected_state->consumer_done.load(std::memory_order_acquire)) {
            return Fail("failed fence query did not quarantine subsequent GPU copies");
        }

        failed_bridge.Shutdown(context11.Get());
        const auto quarantined_stats = failed_bridge.stats();
        if (quarantined_stats.abandoned_on_shutdown != 1 ||
            quarantined_stats.recovery_queries_started != 1 ||
            quarantined_stats.recovery_failures != 1 ||
            quarantined_stats.pending_copy_fences != 1 ||
            retained_state->consumer_done.load(std::memory_order_acquire)) {
            return Fail("failed fence query released its lease during shutdown");
        }
        failed_bridge.ResetOpenedResources();
        failed_bridge.Poll(context11.Get());
        if (failed_bridge.stats().recovery_queries_started != 1 ||
            failed_bridge.stats().pending_copy_fences != 1 ||
            retained_state->consumer_done.load(std::memory_order_acquire)) {
            return Fail("failed recovery proof retried or released an uncertain lease");
        }
        // Drain the physical GPU before ending this artificial failure test;
        // the production fail-closed path instead destroys its D3D11 session.
        cojvr::backends::openvr::D3D11SyncResult drained{};
        std::string drain_error;
        if (!cojvr::backends::openvr::SynchronizeD3D11(
                device11.Get(), context11.Get(),
                cojvr::backends::openvr::D3D11SyncStrategy::event_query,
                1000, drained, drain_error)) {
            return Fail("failed-query fixture could not retire the physical GPU copy");
        }
    }
    if (retained_state->consumer_done.load(std::memory_order_acquire)) {
        return Fail("bridge destruction advertised an unproven quarantined lease as reusable");
    }
    {
        cojvr::backends::d3d9::D3D9StereoCapture abandoned_capture;
        {
            cojvr::backends::openvr::D3D9SharedTextureBridge abandoned_bridge;
            cojvr::backends::d3d9::StereoCpuFrame abandoned_frame;
            if (!CaptureGpuFrame(abandoned_capture, device9ex.Get(), source.Get(), 201, abandoned_frame) ||
                !abandoned_bridge.CopyFrame(device11.Get(), context11.Get(), abandoned_frame,
                    raw_destinations, timing)) {
                return Fail("abandonment fixture could not queue a real leased copy");
            }
            abandoned_bridge.ForceNextFencePollFailureForTest();
            abandoned_bridge.ForceNextRecoveryFencePollFailureForTest();
            abandoned_bridge.Poll(context11.Get());
            abandoned_bridge.Poll(context11.Get());
            abandoned_bridge.Poll(context11.Get());
            abandoned_bridge.Shutdown(context11.Get());
            // Test cleanup retires physical work, but deliberately supplies no
            // proof to the bridge. Its producer state must stay quarantined.
            cojvr::backends::openvr::D3D11SyncResult drained;
            std::string drain_error;
            if (!cojvr::backends::openvr::SynchronizeD3D11(device11.Get(), context11.Get(),
                    cojvr::backends::openvr::D3D11SyncStrategy::event_query, 1000, drained, drain_error)) {
                return Fail("abandonment fixture could not drain its physical GPU work");
            }
        }
        abandoned_capture.InvalidateResources();
        abandoned_capture.Shutdown();
        if (abandoned_capture.stats().consumer_releases != 0 ||
            abandoned_capture.stats().ring_depth != 1 ||
            abandoned_capture.CaptureEyeSurface(device9ex.Get(), source.Get(),
                cojvr::runtime::Eye::left, 202, 2)) {
            return Fail("destroyed consumer let the producer invalidate or reuse its uncertain slot");
        }
    }
    // A final game frame can be fenced but never published when quit stops
    // the producer. Shutdown must cancel it and report an empty ring after
    // reclaiming the consumer leases, rather than retain pre-release depth.
    if (!capture.CaptureEyeSurface(
            device9ex.Get(), source.Get(), cojvr::runtime::Eye::left, 3, 1) ||
        !capture.CaptureEyeSurface(
            device9ex.Get(), source.Get(), cojvr::runtime::Eye::right, 3, 1) ||
        !capture.EndFrame(3, TestPose(), 103)) {
        return Fail("final unpublished producer frame did not fence for shutdown coverage");
    }
    const auto before_shutdown = capture.stats();
    if (before_shutdown.ring_depth != 1 || before_shutdown.frames_fenced != 3 ||
        before_shutdown.frames_collected != 2) {
        return Fail("shutdown fixture did not retain exactly one unpublished frame");
    }
    capture.Shutdown();
    if (capture.gpu_resident_active() || capture.stats().consumer_releases != 2 ||
        capture.stats().ring_depth != 0 ||
        capture.stats().frames_invalidated != before_shutdown.frames_invalidated + 1 ||
        capture.stats().ring_depth_peak != before_shutdown.ring_depth_peak) {
        return Fail("producer shutdown did not reclaim drained GPU leases and release capture resources");
    }

    // A failed producer event query does not prove that StretchRect has retired.
    // Keep its slot quarantined and permit later healthy slots to progress.
    {
        cojvr::backends::d3d9::D3D9StereoCapture failed_capture;
        if (!failed_capture.CaptureEyeSurface(
                device9ex.Get(), source.Get(), cojvr::runtime::Eye::left, 20, 1) ||
            !failed_capture.CaptureEyeSurface(
                device9ex.Get(), source.Get(), cojvr::runtime::Eye::right, 20, 1) ||
            !failed_capture.EndFrame(20, TestPose(), 120)) {
            return Fail("producer failed-query fixture could not fence a frame");
        }
        failed_capture.ForceNextFencePollFailureForTest();
        cojvr::backends::d3d9::StereoCpuFrame quarantined{};
        if (failed_capture.TryCollectReady(quarantined) ||
            failed_capture.stats().ring_depth != 1 ||
            failed_capture.stats().frames_invalidated != 1) {
            return Fail("failed producer query did not quarantine its GPU slot");
        }
        cojvr::backends::d3d9::StereoCpuFrame following{};
        if (!CaptureGpuFrame(failed_capture, device9ex.Get(), source.Get(), 21, following) ||
            following.producer_slot == 0 ||
            failed_capture.stats().ring_depth != 2 ||
            failed_capture.stats().frames_collected != 1) {
            return Fail("quarantined producer slot blocked or contaminated a healthy successor");
        }
        following.producer_lease.Release();
        // Retire real GPU commands before teardown of the artificial failure.
        ComPtr<IDirect3DQuery9> drain;
        hr = device9ex->CreateQuery(D3DQUERYTYPE_EVENT, &drain);
        if (FAILED(hr) || !drain || FAILED(drain->Issue(D3DISSUE_END))) {
            return Fail("producer failure fixture could not create its drain query");
        }
        BOOL complete = FALSE;
        for (int attempt = 0; attempt < 200 && !complete; ++attempt) {
            const HRESULT poll = drain->GetData(&complete, sizeof(complete), D3DGETDATA_FLUSH);
            if (FAILED(poll)) return Fail("producer failure fixture drain failed", poll);
            if (!complete) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!complete) return Fail("producer failure fixture GPU drain timed out");
        failed_capture.Shutdown();
        if (failed_capture.stats().ring_depth != 0 ||
            failed_capture.stats().frames_invalidated != 1) {
            return Fail("producer failure fixture double-counted an already invalidated slot at teardown");
        }
    }

    std::cout << "D3D9Ex shared ring -> D3D11 GPU copy passed: "
              << capture.collect_description()
              << ";consumer_copy_queue_ms=" << timing.copy_queue_ms << '\n';
    return 0;
}
