#include "backends/d3d9/frame_luma_probe.hpp"
#include "backends/d3d9/system_d3d9.hpp"

#include <wrl/client.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using namespace cojvr::backends::d3d9;
using Microsoft::WRL::ComPtr;

namespace {
void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void Hr(HRESULT hr, const char* message) {
    if (FAILED(hr)) { std::cerr << "HRESULT 0x" << std::hex << hr << std::dec << ' '; }
    Check(SUCCEEDED(hr), message);
}
bool Near(double a, double b) { return std::abs(a - b) < 0.01; }

ComPtr<IDirect3DSurface9> Target(IDirect3DDevice9* device, UINT width = 64,
                               D3DFORMAT format = D3DFMT_A8R8G8B8) {
    ComPtr<IDirect3DSurface9> result;
    Hr(device->CreateRenderTarget(width, 64, format, D3DMULTISAMPLE_NONE, 0,
                                  FALSE, &result, nullptr), "create source RT");
    return result;
}
std::vector<unsigned char> Pixels(IDirect3DDevice9* device, IDirect3DSurface9* source) {
    D3DSURFACE_DESC desc{}; Hr(source->GetDesc(&desc), "source description");
    ComPtr<IDirect3DSurface9> cpu;
    Hr(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
        D3DPOOL_SYSTEMMEM, &cpu, nullptr), "create test readback");
    Hr(device->GetRenderTargetData(source, cpu.Get()), "read source in fixture");
    D3DLOCKED_RECT lock{}; Hr(cpu->LockRect(&lock, nullptr, D3DLOCK_READONLY), "lock fixture");
    std::vector<unsigned char> pixels(desc.Width * desc.Height * 4);
    for (UINT y = 0; y < desc.Height; ++y)
        std::memcpy(pixels.data() + y * desc.Width * 4,
            static_cast<const unsigned char*>(lock.pBits) + y * lock.Pitch, desc.Width * 4);
    Hr(cpu->UnlockRect(), "unlock fixture");
    return pixels;
}
void State(IDirect3DDevice9* device, IDirect3DSurface9* expected, const D3DVIEWPORT9& vp) {
    ComPtr<IDirect3DSurface9> actual; D3DVIEWPORT9 actual_vp{};
    Hr(device->GetRenderTarget(0, &actual), "read bound RT");
    Hr(device->GetViewport(&actual_vp), "read viewport");
    Check(actual.Get() == expected && std::memcmp(&vp, &actual_vp, sizeof(vp)) == 0,
          "probe changed native RT/viewport");
}
FrameLumaSample Await(D3D9FrameLumaProbe& probe, IDirect3DDevice9* device) {
    FrameLumaSample result{};
    const ULONGLONG deadline = GetTickCount64() + 4000;
    do {
        if (probe.Poll(result)) return result;
        Check(!probe.unavailable, "probe readback unavailable");
        Hr(device->Present(nullptr, nullptr, nullptr, nullptr), "next natural frame Present");
        Sleep(1);
    } while (GetTickCount64() < deadline);
    Check(false, "natural Present did not complete queued sample");
    return result;
}
void Identity(const FrameLumaSample& sample, IDirect3DDevice9* device,
              IDirect3DSurface9* source, UINT width, D3DFORMAT format,
              std::uint64_t sequence, std::uint64_t generation) {
    Check(sample.sequence == sequence && sample.pose_sequence == sequence + 100 &&
          sample.generation == generation, "original sequence/generation/pose lost");
    Check(sample.device == reinterpret_cast<std::uintptr_t>(device) &&
          sample.source == reinterpret_cast<std::uintptr_t>(source) &&
          sample.source_width == width && sample.source_height == 64 &&
          sample.source_format == format, "original source metadata lost");
    Check(sample.timestamp_ns > 0 && std::isfinite(sample.queue_ms) && sample.queue_ms >= 0 &&
          std::isfinite(sample.readback_ms) && sample.readback_ms >= 0, "timing unavailable");
}
} // namespace

int main(int argc, char** argv) {
    const bool use_ex = argc > 1 && std::strcmp(argv[1], "--ex") == 0;
    const HWND window = CreateWindowExW(0, L"STATIC", L"frame luma GPU test",
        WS_OVERLAPPEDWINDOW, 0, 0, 128, 96, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(window != nullptr, "create test window");
    ShowWindow(window, SW_SHOWNOACTIVATE);
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = window;
    pp.BackBufferWidth = 128; pp.BackBufferHeight = 96; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    ComPtr<IDirect3D9> factory; ComPtr<IDirect3D9Ex> factory_ex;
    ComPtr<IDirect3DDevice9> device; ComPtr<IDirect3DDevice9Ex> device_ex;
    HRESULT hr = D3DERR_NOTAVAILABLE;
    if (use_ex) {
        const auto create = SystemDirect3DCreate9Ex();
        if (create && SUCCEEDED(create(D3D_SDK_VERSION, &factory_ex))) {
            hr = factory_ex->CreateDeviceEx(0, D3DDEVTYPE_HAL, window,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, nullptr, &device_ex);
            if (SUCCEEDED(hr)) device = device_ex;
        }
    } else {
        const auto create = SystemDirect3DCreate9();
        if (create) factory.Attach(create(D3D_SDK_VERSION));
        if (factory) hr = factory->CreateDevice(0, D3DDEVTYPE_HAL, window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    }
    if (FAILED(hr)) {
        std::cout << "SKIP: HAL " << (use_ex ? "Ex" : "classic") << " creation 0x"
                  << std::hex << hr << '\n';
        DestroyWindow(window); return 77;
    }

    D3D9FrameLumaProbe probe;
    FrameLumaSample untouched{}; untouched.sequence = 999;
    Check(!probe.Poll(untouched) && untouched.sequence == 999, "empty Poll wrote output");
    auto source = Target(device.Get());
    ComPtr<IDirect3DSurface9> native;
    Hr(device->GetRenderTarget(0, &native), "get native RT");
    Hr(device->SetRenderTarget(0, source.Get()), "bind eye RT");
    const D3DVIEWPORT9 viewport{3, 4, 51, 49, .1F, .9F};
    Hr(device->SetViewport(&viewport), "set sentinel viewport");

    // Isolate container/runtime availability before making any scene assumption.
    ComPtr<IDirect3DBaseTexture9> source_container;
    const HRESULT container_hr = source->GetContainer(__uuidof(IDirect3DBaseTexture9),
        reinterpret_cast<void**>(source_container.GetAddressOf()));
    Hr(device->ColorFill(source.Get(), nullptr, 0x00123456), "fill in-scene sentinel");
    const auto scene_before = Pixels(device.Get(), source.Get());
    const bool outside = probe.Queue(device.Get(), source.Get(), 90, 6, 190);
    std::cout << "standalone RT GetContainer=0x" << std::hex << container_hr
              << " outside Queue=" << outside << " hr=0x" << probe.last_result << std::dec << '\n';
    Check(outside, "outside-scene standalone RT Queue unavailable");
    probe.Reset();
    Hr(device->BeginScene(), "begin in-scene availability check");
    const bool inside = probe.Queue(device.Get(), source.Get(), 91, 6, 191);
    const HRESULT inside_hr = probe.last_result;
    State(device.Get(), source.Get(), viewport);
    Hr(device->EndScene(), "end in-scene availability check");
    std::cout << "inside Queue=" << inside << " hr=0x" << std::hex << inside_hr << std::dec << '\n';
    Check(Pixels(device.Get(), source.Get()) == scene_before, "in-scene Queue changed source pixels");
    if (!inside) {
        Check(probe.unavailable && FAILED(inside_hr), "in-scene failure did not latch unavailability");
        const auto failures = probe.failures;
        Check(!probe.Queue(device.Get(), source.Get(), 92, 6, 192) &&
              probe.failures == failures && probe.last_result == inside_hr,
              "unavailable operation retried every frame");
    }
    probe.Reset();
    const auto initial_queued = probe.queued;
    const auto initial_dropped = probe.dropped;

    // Omitting point sampling, retaining only the most recent source, or reading
    // the source during Poll breaks this dark/white/dark sequence.
    const D3DCOLOR colors[]{0x00000000, 0x00ffffff, 0xff000000};
    for (std::uint64_t i = 0; i < 3; ++i) {
        Hr(device->ColorFill(source.Get(), nullptr, colors[i]), "fill test pattern");
        const bool accepted = probe.Queue(device.Get(), source.Get(), i + 1, 7, i + 101);
        State(device.Get(), source.Get(), viewport);
        Check(accepted, "Queue must accept a valid outside-scene source");
    }
    Check(!probe.Queue(device.Get(), source.Get(), 4, 7, 104) && probe.queued == initial_queued + 3 &&
          probe.dropped == initial_dropped + 1 && !probe.unavailable, "three-slot saturation must drop");
    const auto before = Pixels(device.Get(), source.Get());
    Hr(device->Present(nullptr, nullptr, nullptr, nullptr), "natural frame Present");
    std::uint64_t previous_time = 0;
    for (std::uint64_t i = 0; i < 3; ++i) {
        const auto sample = Await(probe, device.Get());
        Identity(sample, device.Get(), source.Get(), 64, D3DFMT_A8R8G8B8, i + 1, 7);
        Check(sample.timestamp_ns >= previous_time, "timestamps regressed");
        previous_time = sample.timestamp_ns;
        Check(Near(sample.mean_rgb, i == 1 ? 255 : 0) &&
              Near(sample.white_fraction, i == 1 ? 1 : 0), "dark/white/dark or alpha wrong");
        State(device.Get(), source.Get(), viewport);
    }
    Check(probe.completed == 3 && Pixels(device.Get(), source.Get()) == before,
          "probe changed source content or lost completions");

    // Four equal quadrants: white, red, green, blue. Alpha is zero throughout.
    const RECT quadrants[]{{0,0,32,32}, {32,0,64,32}, {0,32,32,64}, {32,32,64,64}};
    const D3DCOLOR mixed[]{0x00ffffff, 0x00ff0000, 0x0000ff00, 0x000000ff};
    for (unsigned i = 0; i < 4; ++i)
        Hr(device->ColorFill(source.Get(), &quadrants[i], mixed[i]), "fill mixed colors");
    const auto mixed_before = Pixels(device.Get(), source.Get());
    Check(probe.Queue(device.Get(), source.Get(), 10, 7, 110), "queue mixed colors");
    Hr(device->Present(nullptr, nullptr, nullptr, nullptr), "present mixed colors");
    auto sample = Await(probe, device.Get());
    Check(Near(sample.mean_r, 127.5) && Near(sample.mean_g, 127.5) &&
          Near(sample.mean_b, 127.5) && Near(sample.mean_rgb, 127.5) &&
          Near(sample.min_rgb, 85) && Near(sample.max_rgb, 255) &&
          Near(sample.white_fraction, .25), "mixed RGB statistics wrong");
    Check(Pixels(device.Get(), source.Get()) == mixed_before, "mixed source changed");
    const D3DCOLOR threshold[]{0x00f0f0f0, 0xffefefef, 0xffff0000, 0xff000000};
    for (unsigned i = 0; i < 4; ++i)
        Hr(device->ColorFill(source.Get(), &quadrants[i], threshold[i]), "fill threshold colors");
    Check(probe.Queue(device.Get(), source.Get(), 11, 7, 111), "queue white threshold");
    Hr(device->Present(nullptr, nullptr, nullptr, nullptr), "present threshold");
    sample = Await(probe, device.Get());
    Check(Near(sample.white_fraction, .25) && Near(sample.mean_rgb, 141) &&
          Near(sample.min_rgb, 0) && Near(sample.max_rgb, 240), "white threshold/alpha wrong");

    // Generation/source changes retire pending samples without publishing them.
    Check(probe.Queue(device.Get(), source.Get(), 20, 7, 120), "queue old generation");
    Check(probe.Queue(device.Get(), source.Get(), 21, 8, 121), "queue new generation");
    Hr(device->Present(nullptr, nullptr, nullptr, nullptr), "present generation change");
    sample = Await(probe, device.Get()); Identity(sample, device.Get(), source.Get(), 64, D3DFMT_A8R8G8B8, 21, 8);
    Check(!probe.Poll(untouched), "stale generation result survived");
    auto resized = Target(device.Get(), 96, D3DFMT_X8R8G8B8);
    Hr(device->ColorFill(resized.Get(), nullptr, 0xffffffff), "fill resized XRGB");
    Check(probe.Queue(device.Get(), source.Get(), 22, 8, 122), "queue old source");
    Check(probe.Queue(device.Get(), resized.Get(), 23, 8, 123), "queue replaced source");
    Hr(device->Present(nullptr, nullptr, nullptr, nullptr), "present source replacement");
    sample = Await(probe, device.Get()); Identity(sample, device.Get(), resized.Get(), 96, D3DFMT_X8R8G8B8, 23, 8);
    Check(Near(sample.mean_rgb, 255) && !probe.Poll(untouched), "XRGB/replacement wrong");

    auto invalid = Target(device.Get(), 64, D3DFMT_R5G6B5);
    Check(!probe.Queue(device.Get(), invalid.Get(), 30, 8, 130) && FAILED(probe.last_result),
          "unsupported format accepted");
    probe.Reset();
    Check(!probe.Queue(nullptr, source.Get(), 31, 8, 131), "null device accepted");
    probe.Reset();
    Check(!probe.Queue(device.Get(), nullptr, 32, 8, 132), "null source accepted");
    probe.Reset();
    ComPtr<IDirect3DDevice9> foreign;
    if (use_ex) {
        ComPtr<IDirect3DDevice9Ex> foreign_ex;
        Hr(factory_ex->CreateDeviceEx(0, D3DDEVTYPE_HAL, window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, nullptr, &foreign_ex), "foreign Ex device");
        foreign = foreign_ex;
    } else Hr(factory->CreateDevice(0, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &foreign), "foreign classic device");
    Check(!probe.Queue(foreign.Get(), source.Get(), 33, 8, 133) && FAILED(probe.last_result),
          "foreign source device accepted");
    probe.Reset();
    ComPtr<IDirect3DCubeTexture9> cube;
    Hr(device->CreateCubeTexture(64, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, &cube, nullptr), "create cube rejection fixture");
    ComPtr<IDirect3DSurface9> face;
    Hr(cube->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X, 0, &face), "get cube face");
    Check(!probe.Queue(device.Get(), face.Get(), 34, 8, 134), "cube face accepted as 2D");
    probe.Reset();
    ComPtr<IDirect3DSurface9> msaa;
    if (SUCCEEDED(device->CreateRenderTarget(64, 64, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_2_SAMPLES, 0, FALSE, &msaa, nullptr))) {
        Check(!probe.Queue(device.Get(), msaa.Get(), 35, 8, 135), "MSAA source accepted");
        probe.Reset();
    }

    Check(probe.Queue(device.Get(), source.Get(), 40, 9, 140), "queue before reset");
    const auto dropped = probe.dropped;
    probe.Reset(); probe.Reset();
    Check(probe.dropped == dropped + 1 && !probe.Poll(untouched) && !probe.unavailable,
          "Reset not idempotent or stale result survived");
    Hr(device->SetRenderTarget(0, native.Get()), "restore native RT before Reset");
    native.Reset(); source.Reset(); resized.Reset(); invalid.Reset(); face.Reset(); cube.Reset(); msaa.Reset();
    hr = use_ex ? device_ex->ResetEx(&pp, nullptr) : device->Reset(&pp);
    Hr(hr, "native Reset after releasing probe resources");
    source = Target(device.Get());
    Hr(device->ColorFill(source.Get(), nullptr, 0xffffffff), "fill after native Reset");
    Check(probe.Queue(device.Get(), source.Get(), 41, 10, 141), "reuse after native Reset");
    Hr(device->Present(nullptr, nullptr, nullptr, nullptr), "present after native Reset");
    sample = Await(probe, device.Get()); Identity(sample, device.Get(), source.Get(), 64, D3DFMT_A8R8G8B8, 41, 10);
    Check(Near(sample.mean_rgb, 255), "post-Reset sample wrong");
    probe.Reset();
    std::cout << "PASS: " << (use_ex ? "Ex" : "classic") << " real HAL; queued=" << probe.queued
              << " completed=" << probe.completed << " dropped=" << probe.dropped << '\n';
    source.Reset(); device.Reset(); device_ex.Reset(); foreign.Reset();
    DestroyWindow(window);
}
