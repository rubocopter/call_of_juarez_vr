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
    hr = device->ColorFill(back_buffer.Get(), nullptr, D3DCOLOR_ARGB(0xFF, 0x20, 0x40, 0x80));
    if (FAILED(hr)) return Fail("left-eye ColorFill failed", hr);
    if (!capture.CaptureEye(device.Get(), cojvr::runtime::Eye::left, 1, 1)) {
        std::cerr << capture.last_error() << '\n';
        return 1;
    }

    hr = device->ColorFill(back_buffer.Get(), nullptr, D3DCOLOR_ARGB(0xFF, 0xD0, 0x50, 0x10));
    if (FAILED(hr)) return Fail("right-eye ColorFill failed", hr);
    cojvr::runtime::Pose render_pose{};
    render_pose.position = {0.10F, 1.65F, -0.25F};
    render_pose.position_valid = true;
    render_pose.orientation = {0.0F, 0.08715574F, 0.0F, 0.9961947F};
    render_pose.orientation_valid = true;
    if (!capture.CaptureEye(device.Get(), cojvr::runtime::Eye::right, 1, 1) ||
        !capture.EndFrame(1, render_pose, 42)) {
        std::cerr << capture.last_error() << '\n';
        return 1;
    }

    cojvr::backends::d3d9::StereoCpuFrame frame{};
    bool collected = false;
    for (int attempt = 0; attempt < 100 && !collected; ++attempt) {
        if (SUCCEEDED(device->BeginScene())) {
            (void)device->EndScene();
        }
        (void)device->Present(nullptr, nullptr, nullptr, nullptr);
        collected = capture.TryCollectReady(frame);
        if (!collected) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!collected) {
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
    const auto stats = capture.stats();
    if (stats.frames_fenced != 1 || stats.frames_collected != 1 ||
        stats.frames_dropped_no_slot != 0) {
        return Fail("Deferred capture accounting is inconsistent");
    }

    std::cout << "Deferred D3D9 stereo ring -> owned CPU frame passed: "
              << capture.collect_description() << '\n';
    return 0;
}
