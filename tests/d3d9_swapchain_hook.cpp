#include "backends/d3d9/swapchain_vtable_hook.hpp"
#include "backends/d3d9/system_d3d9.hpp"

#include <d3d9.h>
#include <windows.h>

#include <atomic>
#include <iostream>

namespace {

std::atomic_uint32_t g_present_callbacks{0};

void BeforeSwapChainPresent(IDirect3DDevice9* device) noexcept {
    if (device) {
        g_present_callbacks.fetch_add(1, std::memory_order_relaxed);
    }
}

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    HWND window = CreateWindowExW(
        0, L"STATIC", L"cojvr d3d9 swapchain hook test",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return Fail("failed to create D3D9 test window");

    const auto create_d3d9 = cojvr::backends::d3d9::SystemDirect3DCreate9();
    if (!create_d3d9 || !cojvr::backends::d3d9::IsExpectedSystemD3D9Module()) {
        DestroyWindow(window);
        return Fail("native swapchain test did not load the expected system d3d9.dll");
    }
    IDirect3D9* d3d9 = create_d3d9(D3D_SDK_VERSION);
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
        if (create_result == D3DERR_NOTAVAILABLE) {
            std::cout << "d3d9 swapchain hook test skipped: HAL device unavailable\n";
            return 77;
        }
        return Fail("D3D9 device creation failed unexpectedly");
    }

    IDirect3DSwapChain9* swap_chain = nullptr;
    if (FAILED(device->GetSwapChain(0, &swap_chain)) || !swap_chain) {
        device->Release();
        d3d9->Release();
        DestroyWindow(window);
        return Fail("failed to obtain implicit D3D9 swapchain");
    }

    if (!cojvr::backends::d3d9::InstallSwapChainVtableHook(
            device, BeforeSwapChainPresent)) {
        swap_chain->Release();
        device->Release();
        d3d9->Release();
        DestroyWindow(window);
        return Fail("failed to install D3D9 swapchain Present hook");
    }

    const HRESULT present_result = swap_chain->Present(nullptr, nullptr, nullptr, nullptr, 0);
    const auto callbacks = g_present_callbacks.load(std::memory_order_relaxed);
    const bool restored = cojvr::backends::d3d9::RestoreAllSwapChainVtableHooks();
    const HRESULT restored_present_result =
        swap_chain->Present(nullptr, nullptr, nullptr, nullptr, 0);
    const auto callbacks_after_restore =
        g_present_callbacks.load(std::memory_order_relaxed);

    swap_chain->Release();
    device->Release();
    d3d9->Release();
    DestroyWindow(window);

    if (callbacks != 1) {
        return Fail("D3D9 swapchain Present did not traverse the installed callback exactly once");
    }
    if (FAILED(present_result) && present_result != D3DERR_DEVICELOST) {
        return Fail("D3D9 swapchain Present returned an unexpected failure");
    }
    if (!restored || callbacks_after_restore != callbacks) {
        return Fail("D3D9 swapchain Present hook did not restore cleanly");
    }
    if (FAILED(restored_present_result) && restored_present_result != D3DERR_DEVICELOST) {
        return Fail("restored D3D9 swapchain Present returned an unexpected failure");
    }

    std::cout << "d3d9 swapchain Present hook install/restore passed\n";
    return 0;
}
