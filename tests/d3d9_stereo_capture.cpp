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

using CreateSystemSurface = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT,
    D3DFORMAT, D3DPOOL, IDirect3DSurface9**, HANDLE*);
HRESULT STDMETHODCALLTYPE DenySurfaceAllocation(IDirect3DDevice9*, UINT, UINT,
    D3DFORMAT, D3DPOOL, IDirect3DSurface9** output, HANDLE*) {
    *output = nullptr;
    return E_OUTOFMEMORY;
}
bool ReplaceSurfaceAllocator(IDirect3DDevice9* device, void* replacement, void*& previous) {
    auto** table = *reinterpret_cast<void***>(device);
    DWORD protection = 0;
    if (!VirtualProtect(table + 36, sizeof(void*), PAGE_READWRITE, &protection)) return false;
    previous = InterlockedExchangePointer(table + 36, replacement);
    DWORD ignored = 0;
    return VirtualProtect(table + 36, sizeof(void*), protection, &ignored) != FALSE;
}

cojvr::runtime::Pose TestRenderPose() {
    cojvr::runtime::Pose pose{};
    pose.position = {0.10F, 1.65F, -0.25F};
    pose.position_valid = true;
    pose.orientation = {0.0F, 0.08715574F, 0.0F, 0.9961947F};
    pose.orientation_valid = true;
    return pose;
}

cojvr::backends::d3d9::StereoReticleOverlay TestReticle(
    const std::uint64_t frame_sequence) {
    cojvr::backends::d3d9::StereoReticleOverlay reticle{};
    reticle.active = true;
    reticle.frame_sequence = frame_sequence;
    reticle.eyes[0] = {.valid = true, .u = 0.25F, .v = 0.40F};
    reticle.eyes[1] = {.valid = true, .u = 0.75F, .v = 0.60F};
    return reticle;
}

bool CaptureAndCollect(
    cojvr::backends::d3d9::D3D9StereoCapture& capture,
    IDirect3DDevice9* device,
    IDirect3DSurface9* target,
    const std::uint64_t frame_sequence,
    const std::uint64_t generation,
    cojvr::backends::d3d9::StereoCpuFrame& frame) {
    cojvr::runtime::StereoHudTextOverlay hud{};
    hud.frame_sequence = frame_sequence;
    if (frame_sequence % 2) {
        hud.text.hint.length = 3;
        hud.text.hint.characters[0] = u'V';
        hud.text.hint.characters[1] = u'R';
        hud.text.hint.characters[2] = u'!';
        hud.eyes[0].eye_to_head.position.x = -.032F;
        hud.eyes[1].eye_to_head.position.x = .032F;
    }
    HRESULT hr = device->ColorFill(target, nullptr, D3DCOLOR_ARGB(0xFF, 0x20, 0x40, 0x80));
    if (FAILED(hr) ||
        !capture.CaptureEye(device, cojvr::runtime::Eye::left, frame_sequence, generation)) {
        return false;
    }
    hr = device->ColorFill(target, nullptr, D3DCOLOR_ARGB(0xFF, 0xD0, 0x50, 0x10));
    if (FAILED(hr) ||
        !capture.CaptureEye(device, cojvr::runtime::Eye::right, frame_sequence, generation) ||
        !capture.EndFrame(
            frame_sequence, TestRenderPose(), frame_sequence + 41,
            TestReticle(frame_sequence), hud)) {
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
        frame.hud_text.frame_sequence != 1 || frame.hud_text.text.hint.view() != u"VR!" ||
        frame.hud_text.eyes[0].eye_to_head.position.x >= 0 ||
        frame.hud_text.eyes[1].eye_to_head.position.x <= 0 ||
        frame.render_pose_sequence != 42 || !frame.render_hmd_pose.orientation_valid ||
        !frame.render_hmd_pose.position_valid ||
        std::fabs(frame.render_hmd_pose.position.y - 1.65F) > 0.0001F ||
        !frame.gameplay_reticle.active || frame.gameplay_reticle.frame_sequence != 1 ||
        !frame.gameplay_reticle.eyes[0].valid || !frame.gameplay_reticle.eyes[1].valid ||
        std::fabs(frame.gameplay_reticle.eyes[0].u - 0.25F) > 0.0001F ||
        std::fabs(frame.gameplay_reticle.eyes[1].u - 0.75F) > 0.0001F ||
        frame.eyes[0].width != 65 || frame.eyes[0].height != 37 ||
        frame.eyes[0].stride < 65 * 4 || frame.eyes[1].stride < 65 * 4 ||
        frame.transport != cojvr::backends::d3d9::StereoFrameTransport::classic_d3d9_locked_systemmem ||
        !frame.producer_lease.valid() || !frame.eyes[0].borrowed_pixels ||
        !frame.eyes[1].borrowed_pixels || !frame.eyes[0].pixels.empty() ||
        !frame.eyes[1].pixels.empty()) {
        return Fail("Deferred capture returned incorrect leased-system-memory metadata");
    }
    const std::uint64_t left_hash = cojvr::backends::d3d9::HashBgrxSurfaceIgnoringAlpha(
        frame.eyes[0].borrowed_pixels, frame.eyes[0].stride,
        frame.eyes[0].width, frame.eyes[0].height);
    const std::uint64_t right_hash = cojvr::backends::d3d9::HashBgrxSurfaceIgnoringAlpha(
        frame.eyes[1].borrowed_pixels, frame.eyes[1].stride,
        frame.eyes[1].width, frame.eyes[1].height);
    if (left_hash == 0 || right_hash == 0 || left_hash == right_hash) {
        return Fail("Deferred capture did not preserve distinct left/right pixels");
    }

    const auto invalidated_before_active_lease = capture.stats().frames_invalidated;
    capture.InvalidateResources();
    if (capture.stats().frames_invalidated != invalidated_before_active_lease ||
        capture.last_error() !=
            "capture resource invalidation deferred while consumer owns a producer slot") {
        return Fail("capture invalidated producer resources while a consumer lease was active");
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

    // A resource transition must not destroy the old ring while the consumer
    // still owns a borrowed/system-memory or shared-texture producer lease.
    if (capture.CaptureEyeSurface(
            device.Get(), resized_target.Get(), cojvr::runtime::Eye::left, 2, 1)) {
        return Fail("capture rebuilt producer resources while a consumer lease was active");
    }
    frame = {};
    if (!CaptureAndCollect(capture, device.Get(), resized_target.Get(), 2, 1, frame) ||
        frame.eyes[0].width != 71 || frame.eyes[0].height != 39 ||
        frame.generation != 1 || frame.hud_text.frame_sequence != 2 ||
        !frame.hud_text.text.hint.view().empty()) {
        std::cerr << capture.last_error() << '\n';
        return Fail("capture did not rebuild resources after a source resize");
    }
    hr = device->SetRenderTarget(0, back_buffer.Get());
    if (FAILED(hr)) return Fail("failed to restore original render target", hr);
    resized_target.Reset();

    // The owner must invalidate capture resources before a classic D3D9 Reset;
    // after Reset a new generation must rebuild all default-pool resources.
    frame = {};
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
        frame.generation != 2 || frame.hud_text.frame_sequence != 3 ||
        frame.hud_text.text.hint.view() != u"VR!") {
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
    flat_frame.hud_text.frame_sequence = 99;
    flat_frame.hud_text.text.hint.length = 1;
    flat_frame.hud_text.text.hint.characters[0] = u'X';
    hr = flat_device->ColorFill(flat_back_buffer.Get(), nullptr, D3DCOLOR_ARGB(0xFF, 0x34, 0x56, 0x78));
    if (FAILED(hr) ||
        !flat_capture.CaptureFlatFrameImmediate(
            flat_device.Get(), flat_back_buffer.Get(), 10, 1,
            TestRenderPose(), 51, flat_frame)) {
        std::cerr << flat_capture.last_error() << '\n';
        return Fail("flat capture did not produce an immediate owned CPU frame", hr);
    }
    const std::uint64_t flat_left_hash = cojvr::backends::d3d9::HashBgrxSurfaceIgnoringAlpha(
        flat_frame.eyes[0].borrowed_pixels, flat_frame.eyes[0].stride,
        flat_frame.eyes[0].width, flat_frame.eyes[0].height);
    if (flat_frame.hud_text.frame_sequence || !flat_frame.hud_text.text.hint.view().empty()) {
        return Fail("flat capture retained stale gameplay HUD metadata");
    }
    const std::uint64_t flat_right_hash = cojvr::backends::d3d9::HashBgrxSurfaceIgnoringAlpha(
        flat_frame.eyes[1].borrowed_pixels, flat_frame.eyes[1].stride,
        flat_frame.eyes[1].width, flat_frame.eyes[1].height);
    if (flat_left_hash == 0 || flat_left_hash != flat_right_hash ||
        flat_frame.capture_sequence != 10 || flat_frame.render_pose_sequence != 51) {
        return Fail("flat capture did not duplicate one backbuffer into a complete stereo frame");
    }

    if (!flat_frame.producer_lease.valid() || !flat_frame.eyes[0].pixels.empty() ||
        !flat_frame.eyes[1].pixels.empty() ||
        flat_frame.eyes[0].borrowed_pixels != flat_frame.eyes[1].borrowed_pixels) {
        return Fail("flat frame allocated redundant CPU images instead of leasing one mono surface");
    }
    // Startup must reserve the ring. Pausing after a large level loads must not
    // require another full-frame allocation in the game's 32-bit address space.
    void* allocator = nullptr;
    if (!ReplaceSurfaceAllocator(flat_device.Get(), reinterpret_cast<void*>(&DenySurfaceAllocation), allocator))
        return Fail("could not install surface allocation fault");
    std::array<cojvr::backends::d3d9::StereoCpuFrame, 3> retained{};
    retained[0] = std::move(flat_frame);
    bool warmed_ring_ok = true;
    for (int index = 1; index < 3; ++index) {
        (void)flat_device->ColorFill(flat_back_buffer.Get(), nullptr,
            D3DCOLOR_ARGB(0xFF, index * 25, 0x56, 0x78));
        warmed_ring_ok &= flat_capture.CaptureFlatFrameImmediate(flat_device.Get(),
            flat_back_buffer.Get(), 10 + index, 1, TestRenderPose(), 51 + index, retained[index]);
    }
    warmed_ring_ok &= !flat_capture.CaptureFlatFrameImmediate(flat_device.Get(),
        flat_back_buffer.Get(), 14, 1, TestRenderPose(), 55, flat_frame);
    warmed_ring_ok &= flat_left_hash == cojvr::backends::d3d9::HashBgrxSurfaceIgnoringAlpha(
        retained[0].eyes[0].borrowed_pixels, retained[0].eyes[0].stride,
        retained[0].eyes[0].width, retained[0].eyes[0].height);
    retained[0] = {};
    warmed_ring_ok &= flat_capture.CaptureFlatFrameImmediate(flat_device.Get(),
        flat_back_buffer.Get(), 15, 1, TestRenderPose(), 56, flat_frame);
    flat_back_buffer.Reset();
    warmed_ring_ok &= SUCCEEDED(flat_device->Reset(&present));
    warmed_ring_ok &= SUCCEEDED(flat_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &flat_back_buffer));
    flat_frame = {};
    warmed_ring_ok &= flat_capture.CaptureFlatFrameImmediate(flat_device.Get(),
        flat_back_buffer.Get(), 16, 2, TestRenderPose(), 57, flat_frame);
    warmed_ring_ok &= flat_frame.generation == 2 && retained[1].generation == 1;
    void* ignored_allocator = nullptr;
    const bool restored_allocator = ReplaceSurfaceAllocator(flat_device.Get(), allocator, ignored_allocator);
    if (!warmed_ring_ok || !restored_allocator)
        return Fail("flat menu capture failed under allocation denial/Reset or overwrote an owned frame");
    retained = {};

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

    frame = {};
    cojvr::runtime::StereoHudTextOverlay wrong_hud{};
    wrong_hud.frame_sequence = 999;
    if (!capture.CaptureEye(second_device.Get(),cojvr::runtime::Eye::left,50,3) ||
        !capture.CaptureEye(second_device.Get(),cojvr::runtime::Eye::right,50,3) ||
        capture.EndFrame(50,TestRenderPose(),91,{},wrong_hud) || capture.TryCollectReady(frame)) {
        return Fail("capture accepted HUD text from a different render frame");
    }
    if (!CaptureAndCollect(capture,second_device.Get(),second_back_buffer.Get(),51,3,frame) ||
        frame.hud_text.frame_sequence != 51 || frame.hud_text.text.hint.view() != u"VR!") {
        return Fail("capture did not recover from rejected HUD metadata");
    }

    std::cout << "Deferred D3D9 stereo ring -> leased SYSTEMMEM frame passed: "
              << capture.collect_description() << '\n';
    return 0;
}
