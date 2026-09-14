#include "backends/d3d9/classic_readback_bridge.hpp"
#include "backends/d3d9/system_d3d9.hpp"

#include <windows.h>

#include <d3d9.h>
#include <wrl/client.h>

#include <iostream>
#include <string>

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
    if (create_d3d9 == nullptr) return Fail("System d3d9.dll has no Direct3DCreate9");

    ComPtr<IDirect3D9> d3d9;
    d3d9.Attach(create_d3d9(D3D_SDK_VERSION));
    if (!d3d9) return Fail("Direct3DCreate9 failed");

    const wchar_t* class_name = L"CoJVRClassicReadbackTest";
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = TestWindowProc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = class_name;
    if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return Fail("test window class registration failed");
    }

    HWND window = CreateWindowExW(
        0, class_name, L"CoJ VR classic readback test", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 240, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!window) return Fail("test window creation failed");

    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = window;
    present.BackBufferFormat = D3DFMT_UNKNOWN;
    present.BackBufferWidth = 64;
    present.BackBufferHeight = 64;

    ComPtr<IDirect3DDevice9> device9;
    HRESULT hr = d3d9->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &present,
        &device9);
    if (FAILED(hr)) return Fail("Classic D3D9 CreateDevice failed", hr);

    ComPtr<IDirect3DSurface9> back_buffer;
    hr = device9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
    if (FAILED(hr)) return Fail("GetBackBuffer failed", hr);

    hr = device9->ColorFill(
        back_buffer.Get(), nullptr, D3DCOLOR_ARGB(0xFF, 0x12, 0x34, 0x56));
    if (FAILED(hr)) return Fail("Classic D3D9 ColorFill failed", hr);

    std::string diagnostic;
    if (!cojvr::backends::d3d9::CaptureBackBufferToD3D11(device9.Get(), diagnostic)) {
        std::cerr << diagnostic << '\n';
        return 1;
    }

    std::cout << "Classic D3D9 -> CPU readback -> D3D11 upload passed: "
              << diagnostic << '\n';
    return 0;
}
