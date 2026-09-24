#include "backends/d3d9/device_vtable_hook.hpp"
#include "backends/d3d9/factory_vtable_hook.hpp"
#include "backends/d3d9/swapchain_vtable_hook.hpp"
#include "backends/d3d9/system_d3d9.hpp"

#include <d3d9.h>
#include <windows.h>

#include <cstring>
#include <iostream>

namespace {

unsigned g_factory_queries = 0;
unsigned g_device_queries = 0;
unsigned g_device_refs = 0;
unsigned g_get_factory = 0;
unsigned g_cooperative = 0;
unsigned g_clear = 0;
unsigned g_set_texture = 0;
unsigned g_swapchain_refs = 0;

void FactoryCom(void*, const char* event, const IID*, void*, HRESULT, ULONG) noexcept {
    if (std::strcmp(event, "query_interface_result") == 0) ++g_factory_queries;
}

void DeviceCom(void*, const char* event, const IID*, void*, HRESULT, ULONG) noexcept {
    if (std::strcmp(event, "query_interface_result") == 0) ++g_device_queries;
    if (std::strcmp(event, "add_ref") == 0 || std::strcmp(event, "release") == 0) ++g_device_refs;
    if (std::strcmp(event, "get_direct3d_result") == 0) ++g_get_factory;
}

void SwapchainCom(void*, const char* event, const IID*, void*, HRESULT, ULONG) noexcept {
    if (std::strcmp(event, "add_ref") == 0 || std::strcmp(event, "release") == 0) ++g_swapchain_refs;
}

void DeviceMethod(IDirect3DDevice9*, const char* method, bool entering, HRESULT) noexcept {
    if (entering) return;
    if (std::strcmp(method, "TestCooperativeLevel") == 0) ++g_cooperative;
    if (std::strcmp(method, "Clear") == 0) ++g_clear;
    if (std::strcmp(method, "SetTexture") == 0) ++g_set_texture;
}

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

bool SameIdentity(IUnknown* left, IUnknown* right) {
    IUnknown* left_unknown = nullptr;
    IUnknown* right_unknown = nullptr;
    const bool same = SUCCEEDED(left->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&left_unknown))) &&
        SUCCEEDED(right->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&right_unknown))) &&
        left_unknown == right_unknown;
    if (left_unknown) left_unknown->Release();
    if (right_unknown) right_unknown->Release();
    return same;
}

} // namespace

int main() {
    const auto create_ex = cojvr::backends::d3d9::SystemDirect3DCreate9Ex();
    if (!create_ex) return Fail("system Direct3DCreate9Ex is unavailable");

    IDirect3D9Ex* factory = nullptr;
    HRESULT hr = create_ex(D3D_SDK_VERSION, &factory);
    if (FAILED(hr) || !factory) return Fail("system D3D9Ex factory creation failed");
    const cojvr::backends::d3d9::FactoryHookCallbacks factory_callbacks{
        .prefer_ex_device = true,
        .com_trace = FactoryCom,
    };
    if (!cojvr::backends::d3d9::InstallFactoryVtableHook(factory, factory_callbacks)) {
        factory->Release();
        return Fail("factory tracing hook installation failed");
    }

    HWND window = CreateWindowExW(0, L"STATIC", L"CoJ D3D9Ex startup contract",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return Fail("test window creation failed");
    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = window;
    present.BackBufferWidth = 320;
    present.BackBufferHeight = 240;
    present.BackBufferFormat = D3DFMT_UNKNOWN;
    present.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    IDirect3DDevice9* device = nullptr;
    hr = factory->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present, &device);
    if ((hr == D3DERR_NOTAVAILABLE || hr == D3DERR_DEVICELOST) && !device) {
        (void)cojvr::backends::d3d9::RestoreFactoryVtableHook(factory);
        factory->Release();
        DestroyWindow(window);
        std::cout << "D3D9Ex startup contract skipped: HAL device unavailable\n";
        return 77;
    }
    if (FAILED(hr) || !device) return Fail("hooked Ex factory could not create a device");

    IDirect3D9Ex* alternate_factory = nullptr;
    hr = create_ex(D3D_SDK_VERSION, &alternate_factory);
    if (FAILED(hr) || !alternate_factory || SameIdentity(factory, alternate_factory)) {
        return Fail("independent Ex factory for GetDirect3D identity test unavailable");
    }

    const cojvr::backends::d3d9::DeviceHookCallbacks device_callbacks{
        .com_trace = DeviceCom,
        .method_trace = DeviceMethod,
        .get_direct3d_factory = alternate_factory,
        .get_direct3d_device = device,
    };
    if (!cojvr::backends::d3d9::InstallDeviceVtableHook(device, device_callbacks)) {
        return Fail("device tracing hook installation failed");
    }
    const cojvr::backends::d3d9::SwapChainHookCallbacks swapchain_callbacks{
        .com_trace = SwapchainCom,
    };
    if (cojvr::backends::d3d9::InstallSwapChainVtableHookDetailed(device, swapchain_callbacks).result !=
        cojvr::backends::d3d9::HookRegistryResult::Installed) {
        return Fail("swapchain tracing hook installation failed");
    }

    IDirect3DDevice9Ex* queried_ex = nullptr;
    hr = device->QueryInterface(IID_IDirect3DDevice9Ex, reinterpret_cast<void**>(&queried_ex));
    if (FAILED(hr) || !queried_ex) return Fail("device QueryInterface lost Ex identity");
    queried_ex->Release();

    IDirect3D9* recovered = nullptr;
    hr = device->GetDirect3D(&recovered);
    if (FAILED(hr) || !recovered || !SameIdentity(alternate_factory, recovered) ||
        SameIdentity(factory, recovered)) {
        return Fail("device GetDirect3D did not return the configured factory identity");
    }
    recovered->Release();

    device->AddRef();
    device->Release();
    if (FAILED(device->TestCooperativeLevel())) return Fail("TestCooperativeLevel failed");
    if (FAILED(device->BeginScene())) return Fail("BeginScene failed");
    if (FAILED(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0))) {
        return Fail("Clear failed");
    }
    if (FAILED(device->SetTexture(0, nullptr))) return Fail("SetTexture failed");
    if (FAILED(device->EndScene())) return Fail("EndScene failed");
    if (FAILED(device->Present(nullptr, nullptr, nullptr, nullptr))) return Fail("Present failed");

    IDirect3DSwapChain9* chain = nullptr;
    if (FAILED(device->GetSwapChain(0, &chain)) || !chain) return Fail("implicit swapchain unavailable");
    chain->AddRef();
    chain->Release();
    chain->Release();

    const bool saw_traces = g_factory_queries > 0 && g_device_queries > 0 &&
        g_device_refs >= 2 && g_get_factory > 0 && g_cooperative > 0 &&
        g_clear > 0 && g_set_texture > 0 && g_swapchain_refs >= 2;
    const bool restored = cojvr::backends::d3d9::RestoreAllSwapChainVtableHooks() &&
        cojvr::backends::d3d9::RestoreAllDeviceVtableHooks() &&
        cojvr::backends::d3d9::RestoreFactoryVtableHook(factory);
    device->Release();
    alternate_factory->Release();
    factory->Release();
    DestroyWindow(window);
    if (!saw_traces) return Fail("startup COM/device/swapchain trace missed an exercised operation");
    if (!restored) return Fail("startup COM/device/swapchain trace did not restore cleanly");
    std::cout << "D3D9Ex startup COM identity and trace hooks passed\n";
    return 0;
}
