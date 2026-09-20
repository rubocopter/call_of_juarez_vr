#include "backends/d3d9/content_hash.hpp"
#include "backends/d3d9/d3d9_stereo_capture.hpp"
#include "backends/d3d9/system_d3d9.hpp"

#include <windows.h>

#include <d3d9.h>
#include <wrl/client.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <thread>

using Microsoft::WRL::ComPtr;

namespace {

LRESULT CALLBACK TestWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcW(window, message, wparam, lparam);
}

int Fail(const char* message, const HRESULT hr = S_OK) {
    std::cerr << message;
    if (FAILED(hr)) {
        std::cerr << " (HRESULT 0x" << std::hex << static_cast<unsigned long>(hr) << ')';
    }
    std::cerr << '\n';
    return 1;
}

cojvr::runtime::Pose TestRenderPose() {
    cojvr::runtime::Pose pose{};
    pose.position = {0.10F, 1.65F, -0.25F};
    pose.position_valid = true;
    pose.orientation = {0.0F, 0.08715574F, 0.0F, 0.9961947F};
    pose.orientation_valid = true;
    return pose;
}

bool CaptureAndCollect(
    cojvr::backends::d3d9::D3D9StereoCapture& capture,
    IDirect3DDevice9* device,
    IDirect3DSurface9* target,
    const std::uint64_t frame_sequence,
    const std::uint64_t generation,
    cojvr::backends::d3d9::StereoCpuFrame& frame) {
    HRESULT hr = device->ColorFill(target, nullptr, D3DCOLOR_ARGB(0xFF, 0x20, 0x40, 0x80));
    if (FAILED(hr) ||
        !capture.CaptureEye(device, cojvr::runtime::Eye::left, frame_sequence, generation)) {
        return false;
    }
    hr = device->ColorFill(target, nullptr, D3DCOLOR_ARGB(0xFF, 0xD0, 0x50, 0x10));
    if (FAILED(hr) ||
        !capture.CaptureEye(device, cojvr::runtime::Eye::right, frame_sequence, generation) ||
        !capture.EndFrame(frame_sequence, TestRenderPose(), frame_sequence + 41)) {
        return false;
    }
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (SUCCEEDED(device->BeginScene())) {
            (void)device->EndScene();
        }
        (void)device->Present(nullptr, nullptr, nullptr, nullptr);
        if (capture.TryCollectReady(frame)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

} // namespace

int main() {
    const auto create_d3d9 = cojvr::backends::d3d9::SystemDirect3DCreate9();
    if (!create_d3d9) return Fail("System d3d9.dll has no Direct3DCreate9");

    ComPtr<IDirect3D9> d3d9;
    d3d9.Attach(create_d3d9(D3D_SDK_VERSION));
    if (!d3d9) return Fail("Direct3DCreate9 failed");

    const wchar_t* class_name = L"CoJVRDeferredStereoCaptureTest";
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = TestWindowProc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = class_name;
    if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return Fail("test window class registration failed");
    }
    HWND window = CreateWindowExW(
        0, class_name, L"CoJ VR deferred stereo capture test", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 240, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!window) return Fail("test window creation failed");

    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = window;
    present.BackBufferFormat = D3DFMT_UNKNOWN;
    present.BackBufferWidth = 65;
    present.BackBufferHeight = 37;

    ComPtr<IDirect3DDevice9> device;
    HRESULT hr = d3d9->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &present,
        &device);
    if (FAILED(hr)) {
        if (hr == D3DERR_NOTAVAILABLE) {
            std::cout << "Deferred D3D9 stereo capture test skipped: HAL device unavailable\n";
            return 77;
        }
        return Fail("Classic D3D9 CreateDevice failed", hr);
    }

    ComPtr<IDirect3DSurface9> back_buffer;
    hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
    if (FAILED(hr)) return Fail("GetBackBuffer failed", hr);

    cojvr::backends::d3d9::D3D9StereoCapture capture;
    cojvr::backends::d3d9::StereoCpuFrame frame{};
    if (!CaptureAndCollect(capture, device.Get(), back_buffer.Get(), 1, 1, frame)) {
        std::cerr << "Deferred capture never became ready: " << capture.last_error() << '\n';
        return 1;
    }
    if (frame.capture_sequence != 1 || frame.generation != 1 ||
        frame.render_pose_sequence != 42 || !frame.render_hmd_pose.orientation_valid ||
        !frame.render_hmd_pose.position_valid ||
        std::fabs(frame.render_hmd_pose.position.y - 1.65F) > 0.0001F ||
        frame.eyes[0].width != 65 || frame.eyes[0].height != 37 ||
        frame.eyes[0].stride != 65 * 4 || frame.eyes[1].stride != 65 * 4) {
        return Fail("Deferred capture returned incorrect owned-frame metadata");
    }
    const std::uint64_t left_hash = cojvr::backends::d3d9::HashBgrxSurfaceIgnoringAlpha(
        frame.eyes[0].pixels.data(), frame.eyes[0].stride,
        frame.eyes[0].width, frame.eyes[0].height);
    const std::uint64_t right_hash = cojvr::backends::d3d9::HashBgrxSurfaceIgnoringAlpha(
        frame.eyes[1].pixels.data(), frame.eyes[1].stride,
        frame.eyes[1].width, frame.eyes[1].height);
    if (left_hash == 0 || right_hash == 0 || left_hash == right_hash) {
        return Fail("Deferred capture did not preserve distinct left/right pixels");
    }

    // A source-size change on the same device/generation must rebuild capture
    // resources rather than reusing surfaces with stale dimensions.
    ComPtr<IDirect3DSurface9> resized_target;
    hr = device->CreateRenderTarget(
        71, 39, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
        &resized_target, nullptr);
    if (FAILED(hr) || !resized_target) return Fail("resized render target creation failed", hr);
    hr = device->SetRenderTarget(0, resized_target.Get());
    if (FAILED(hr)) return Fail("SetRenderTarget for resize coverage failed", hr);
    frame = {};
    if (!CaptureAndCollect(capture, device.Get(), resized_target.Get(), 2, 1, frame) ||
        frame.eyes[0].width != 71 || frame.eyes[0].height != 39 ||
        frame.generation != 1) {
        std::cerr << capture.last_error() << '\n';
        return Fail("capture did not rebuild resources after a source resize");
    }
    hr = device->SetRenderTarget(0, back_buffer.Get());
    if (FAILED(hr)) return Fail("failed to restore original render target", hr);
    resized_target.Reset();

    // The owner must invalidate capture resources before a classic D3D9 Reset;
    // after Reset a new generation must rebuild all default-pool resources.
    capture.InvalidateResources();
    back_buffer.Reset();
    present.BackBufferWidth = 83;
    present.BackBufferHeight = 41;
    hr = device->Reset(&present);
    if (FAILED(hr)) return Fail("D3D9 Reset failed after capture invalidation", hr);
    hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
    if (FAILED(hr) || !back_buffer) return Fail("post-Reset GetBackBuffer failed", hr);
    frame = {};
    if (!CaptureAndCollect(capture, device.Get(), back_buffer.Get(), 3, 2, frame) ||
        frame.eyes[0].width != 83 || frame.eyes[0].height != 41 ||
        frame.generation != 2) {
        std::cerr << capture.last_error() << '\n';
        return Fail("capture did not recover on the post-Reset generation");
    }

    // An entirely new device with the same dimensions/format must not reuse the
    // old device's D3D9 resources merely because the surface description matches.
    ComPtr<IDirect3DDevice9> second_device;
    hr = d3d9->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &present,
        &second_device);
    if (FAILED(hr) || !second_device) return Fail("second classic D3D9 device creation failed", hr);
    ComPtr<IDirect3DSurface9> second_back_buffer;
    hr = second_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &second_back_buffer);
    if (FAILED(hr) || !second_back_buffer) return Fail("second-device GetBackBuffer failed", hr);
    frame = {};
    if (!CaptureAndCollect(capture, second_device.Get(), second_back_buffer.Get(), 4, 3, frame) ||
        frame.device_id != reinterpret_cast<std::uintptr_t>(second_device.Get()) ||
        frame.generation != 3 || frame.eyes[0].width != 83 || frame.eyes[0].height != 41) {
        std::cerr << capture.last_error() << '\n';
        return Fail("capture reused stale resources across an identical new device");
    }

    // Flat-theater capture runs across menu/loading transitions where the game
    // may Reset its classic D3D9 device. It must therefore leave no persistent
    // default-pool resources that can make Reset fail when the owner has no
    // explicit pre-Reset callback.
    ComPtr<IDirect3DDevice9> flat_device;
    hr = d3d9->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &present,
        &flat_device);
    if (FAILED(hr) || !flat_device) return Fail("flat-capture D3D9 device creation failed", hr);
    ComPtr<IDirect3DSurface9> flat_back_buffer;
    hr = flat_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &flat_back_buffer);
    if (FAILED(hr) || !flat_back_buffer) return Fail("flat-capture GetBackBuffer failed", hr);

    cojvr::backends::d3d9::D3D9StereoCapture flat_capture;
    cojvr::backends::d3d9::StereoCpuFrame flat_frame{};
    hr = flat_device->ColorFill(flat_back_buffer.Get(), nullptr, D3DCOLOR_ARGB(0xFF, 0x34, 0x56, 0x78));
    if (FAILED(hr) ||
        !flat_capture.CaptureFlatFrameImmediate(
            flat_device.Get(), flat_back_buffer.Get(), 10, 1,
            TestRenderPose(), 51, flat_frame)) {
        std::cerr << flat_capture.last_error() << '\n';
        return Fail("flat capture did not produce an immediate owned CPU frame", hr);
    }
    const std::uint64_t flat_left_hash = cojvr::backends::d3d9::HashBgrxSurfaceIgnoringAlpha(
        flat_frame.eyes[0].pixels.data(), flat_frame.eyes[0].stride,
        flat_frame.eyes[0].width, flat_frame.eyes[0].height);
    const std::uint64_t flat_right_hash = cojvr::backends::d3d9::HashBgrxSurfaceIgnoringAlpha(
        flat_frame.eyes[1].pixels.data(), flat_frame.eyes[1].stride,
        flat_frame.eyes[1].width, flat_frame.eyes[1].height);
    if (flat_left_hash == 0 || flat_left_hash != flat_right_hash ||
        flat_frame.capture_sequence != 10 || flat_frame.render_pose_sequence != 51) {
        return Fail("flat capture did not duplicate one backbuffer into a complete stereo frame");
    }

    flat_back_buffer.Reset();
    D3DPRESENT_PARAMETERS flat_present = present;
    flat_present.BackBufferWidth = 89;
    flat_present.BackBufferHeight = 43;
    hr = flat_device->Reset(&flat_present);
    if (FAILED(hr)) {
        return Fail("flat capture retained resources that blocked classic D3D9 Reset", hr);
    }
    hr = flat_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &flat_back_buffer);
    if (FAILED(hr) || !flat_back_buffer) return Fail("flat-capture post-Reset GetBackBuffer failed", hr);
    flat_frame = {};
    if (!flat_capture.CaptureFlatFrameImmediate(
            flat_device.Get(), flat_back_buffer.Get(), 11, 2,
            TestRenderPose(), 52, flat_frame) ||
        flat_frame.eyes[0].width != 89 || flat_frame.eyes[0].height != 43 ||
        flat_frame.generation != 2) {
        std::cerr << flat_capture.last_error() << '\n';
        return Fail("flat capture did not recover across Reset without explicit invalidation");
    }

    const auto stats = capture.stats();
    if (stats.frames_fenced != 4 || stats.frames_collected != 4 ||
        stats.frames_dropped_no_slot != 0) {
        return Fail("Deferred capture accounting is inconsistent");
    }

    std::cout << "Deferred D3D9 stereo ring -> owned CPU frame passed: "
              << capture.collect_description() << '\n';
    return 0;
}
