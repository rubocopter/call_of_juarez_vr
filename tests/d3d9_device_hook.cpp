#include "backends/d3d9/device_vtable_hook.hpp"

#include <d3d9.h>
#include <windows.h>

#include <atomic>
#include <iostream>

namespace {

std::atomic_uint32_t g_end_scene_callbacks{0};
std::atomic_uint32_t g_begin_scene_callbacks{0};

void AfterBeginScene(IDirect3DDevice9* device, HRESULT result) noexcept {
    if (device && SUCCEEDED(result)) {
        g_begin_scene_callbacks.fetch_add(1, std::memory_order_relaxed);
    }
}

void AfterEndScene(IDirect3DDevice9* device, HRESULT result) noexcept {
    if (device && SUCCEEDED(result)) {
        g_end_scene_callbacks.fetch_add(1, std::memory_order_relaxed);
    }
}

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    HWND window = CreateWindowExW(
        0, L"STATIC", L"cojvr d3d9 device hook test",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return Fail("failed to create D3D9 test window");

    IDirect3D9* d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d9) {
        DestroyWindow(window);
        return Fail("Direct3DCreate9 failed");
    }

    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = window;
    present.BackBufferWidth = 304;
    present.BackBufferHeight = 201;
    present.BackBufferFormat = D3DFMT_UNKNOWN;
    present.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    IDirect3DDevice9* device = nullptr;
    const HRESULT create_result = d3d9->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &present,
        &device);
    if (FAILED(create_result) || !device) {
        d3d9->Release();
        DestroyWindow(window);
        std::cout << "d3d9 device hook test skipped: HAL device unavailable\n";
        return 0;
    }

    const cojvr::backends::d3d9::DeviceHookCallbacks callbacks{
        .after_begin_scene = AfterBeginScene,
        .after_end_scene = AfterEndScene,
    };
    if (!cojvr::backends::d3d9::InstallDeviceVtableHook(device, callbacks)) {
        device->Release();
        d3d9->Release();
        DestroyWindow(window);
        return Fail("failed to install D3D9 device hook");
    }

    constexpr std::uint32_t kSceneCycles = 5;
    HRESULT end_result = S_OK;
    for (std::uint32_t cycle = 0; cycle < kSceneCycles; ++cycle) {
        const HRESULT begin_result = device->BeginScene();
        if (FAILED(begin_result)) {
            device->Release();
            d3d9->Release();
            DestroyWindow(window);
            return Fail("D3D9 BeginScene failed");
        }

        end_result = device->EndScene();
        if (FAILED(end_result)) break;
    }
    const auto callbacks_seen = g_end_scene_callbacks.load(std::memory_order_relaxed);
    const auto begin_callbacks_seen = g_begin_scene_callbacks.load(std::memory_order_relaxed);

    device->Release();
    d3d9->Release();
    DestroyWindow(window);

    if (FAILED(end_result)) return Fail("D3D9 EndScene failed");
    if (begin_callbacks_seen != kSceneCycles) {
        return Fail("D3D9 BeginScene did not traverse the installed callback for every scene cycle");
    }
    if (callbacks_seen != kSceneCycles) {
        return Fail("D3D9 EndScene did not traverse the installed callback for every scene cycle");
    }

    std::cout << "d3d9 BeginScene/EndScene device hooks passed for " << kSceneCycles << " cycles\n";
    return 0;
}
