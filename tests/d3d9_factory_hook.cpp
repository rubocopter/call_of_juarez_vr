#include "backends/d3d9/factory_vtable_hook.hpp"
#include "backends/d3d9/diagnostic_context.hpp"
#include "backends/d3d9/system_d3d9.hpp"

#include <d3d9.h>
#include <windows.h>

#include <atomic>
#include <iostream>

namespace {

std::atomic_uint32_t g_create_callbacks{0};
std::atomic_uint32_t g_success_callbacks{0};
cojvr::backends::d3d9::D3D9DiagnosticContextRegistry g_contexts;
cojvr::backends::d3d9::DeviceDiagnosticContext g_first_context{};
cojvr::backends::d3d9::DeviceDiagnosticContext g_second_context{};

void AfterCreateDevice(
    IDirect3D9* factory,
    UINT,
    D3DDEVTYPE,
    HWND,
    DWORD,
    D3DPRESENT_PARAMETERS*,
    IDirect3DDevice9** returned_device,
    HRESULT result) noexcept {
    if (factory) g_create_callbacks.fetch_add(1, std::memory_order_relaxed);
    if (SUCCEEDED(result) && returned_device && *returned_device) {
        const std::uint32_t callback =
            g_success_callbacks.fetch_add(1, std::memory_order_relaxed) + 1;
        const auto context = g_contexts.RegisterDevice(factory, *returned_device);
        if (callback == 1) g_first_context = context;
        else if (callback == 2) g_second_context = context;
    }
}

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

bool SameComIdentity(IUnknown* left, IUnknown* right) {
    if (!left || !right) return false;
    IUnknown* left_identity = nullptr;
    IUnknown* right_identity = nullptr;
    const HRESULT left_result = left->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&left_identity));
    const HRESULT right_result = right->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&right_identity));
    const bool same = SUCCEEDED(left_result) && SUCCEEDED(right_result) &&
        left_identity == right_identity;
    if (left_identity) left_identity->Release();
    if (right_identity) right_identity->Release();
    return same;
}

HRESULT CreateTestDevice(
    IDirect3D9* factory, HWND window, IDirect3DDevice9** device) {
    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = window;
    present.BackBufferWidth = 304;
    present.BackBufferHeight = 201;
    present.BackBufferFormat = D3DFMT_UNKNOWN;
    present.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    return factory->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present, device);
}

} // namespace

int main() {
    const auto create_d3d9 = cojvr::backends::d3d9::SystemDirect3DCreate9();
    if (!create_d3d9 || !cojvr::backends::d3d9::IsExpectedSystemD3D9Module()) {
        return Fail("factory hook test did not load the expected system d3d9.dll");
    }

    IDirect3D9* factory = create_d3d9(D3D_SDK_VERSION);
    if (!factory) return Fail("Direct3DCreate9 failed");
    const auto initial_factory_context = g_contexts.RegisterCreatedFactory(factory);
    const cojvr::backends::d3d9::FactoryHookCallbacks callbacks{
        .after_create_device = AfterCreateDevice,
    };
    if (!cojvr::backends::d3d9::InstallFactoryVtableHook(factory, callbacks) ||
        !cojvr::backends::d3d9::FactoryVtableHookActive(factory)) {
        factory->Release();
        return Fail("native factory CreateDevice hook installation failed");
    }

    HWND window = CreateWindowExW(
        0, L"STATIC", L"cojvr d3d9 factory hook test",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) {
        factory->Release();
        return Fail("failed to create D3D9 test window");
    }

    IDirect3DDevice9* first_device = nullptr;
    const HRESULT first_result = CreateTestDevice(factory, window, &first_device);
    if (first_result == D3DERR_NOTAVAILABLE) {
        DestroyWindow(window);
        factory->Release();
        std::cout << "d3d9 factory hook test skipped: HAL device unavailable\n";
        return 77;
    }
    if (FAILED(first_result) || !first_device) {
        DestroyWindow(window);
        factory->Release();
        return Fail("first native CreateDevice failed unexpectedly");
    }

    IDirect3D9* recovered_factory = nullptr;
    const HRESULT recovered_result = first_device->GetDirect3D(&recovered_factory);
    if (FAILED(recovered_result) || !recovered_factory ||
        !SameComIdentity(factory, recovered_factory)) {
        if (recovered_factory) recovered_factory->Release();
        first_device->Release();
        DestroyWindow(window);
        factory->Release();
        return Fail("GetDirect3D did not round-trip native factory identity");
    }
    if (!cojvr::backends::d3d9::FactoryVtableHookActive(recovered_factory)) {
        recovered_factory->Release();
        first_device->Release();
        DestroyWindow(window);
        factory->Release();
        return Fail("factory recovered from device bypassed CreateDevice observation");
    }

    IDirect3DDevice9* second_device = nullptr;
    const HRESULT second_result = CreateTestDevice(recovered_factory, window, &second_device);
    if (FAILED(second_result) || !second_device) {
        recovered_factory->Release();
        first_device->Release();
        DestroyWindow(window);
        factory->Release();
        return Fail("CreateDevice through recovered factory failed");
    }

    const auto advanced = g_contexts.AdvanceGeneration(first_device);
    if (!advanced || advanced->generation != 2 || advanced->device_id != g_first_context.device_id ||
        advanced->swapchain_id == 0 ||
        (advanced->swapchain_identity == g_first_context.swapchain_identity &&
            advanced->swapchain_id != g_first_context.swapchain_id) ||
        (advanced->swapchain_identity != g_first_context.swapchain_identity &&
            advanced->swapchain_id == g_first_context.swapchain_id)) {
        second_device->Release();
        recovered_factory->Release();
        first_device->Release();
        DestroyWindow(window);
        factory->Release();
        return Fail("device generation/swapchain identity did not advance deterministically");
    }

    second_device->Release();
    recovered_factory->Release();
    first_device->Release();
    DestroyWindow(window);
    factory->Release();

    if (first_result != D3D_OK || second_result != D3D_OK ||
        g_create_callbacks.load(std::memory_order_relaxed) != 2 ||
        g_success_callbacks.load(std::memory_order_relaxed) != 2 ||
        initial_factory_context.factory_id == 0 ||
        g_first_context.factory_id != initial_factory_context.factory_id ||
        g_second_context.factory_id != initial_factory_context.factory_id ||
        g_first_context.device_id == 0 || g_second_context.device_id == 0 ||
        g_first_context.device_id == g_second_context.device_id ||
        g_first_context.swapchain_id == 0 || g_second_context.swapchain_id == 0 ||
        !g_first_context.factory_identity_matches ||
        !g_second_context.factory_identity_matches) {
        return Fail("native CreateDevice HRESULT/callback behavior changed through factory hook");
    }

    std::cout << "native factory identity and recovered-factory CreateDevice observation passed\n";
    return 0;
}
