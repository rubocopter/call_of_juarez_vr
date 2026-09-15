#include "backends/d3d9/classic_readback_bridge.hpp"
#include "backends/d3d9/system_d3d9.hpp"

#include <windows.h>

#include <d3d9.h>
#include <wrl/client.h>

#include <iostream>
#include <iterator>
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
    if (!cojvr::backends::d3d9::IsExpectedSystemD3D9Module()) {
        return Fail("readback test did not load the expected system d3d9.dll");
    }

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
    present.BackBufferWidth = 65;
    present.BackBufferHeight = 37;

    ComPtr<IDirect3DDevice9> device9;
    HRESULT hr = d3d9->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &present,
        &device9);
    if (FAILED(hr)) {
        if (hr == D3DERR_NOTAVAILABLE) {
            std::cout << "Classic D3D9 readback test skipped: HAL device unavailable\n";
            return 77;
        }
        return Fail("Classic D3D9 CreateDevice failed", hr);
    }

    ComPtr<IDirect3DSurface9> back_buffer;
    hr = device9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
    if (FAILED(hr)) return Fail("GetBackBuffer failed", hr);

    constexpr D3DCOLOR patterns[][5] = {
        {
            D3DCOLOR_ARGB(0xFF, 0x11, 0x22, 0x33),
            D3DCOLOR_ARGB(0xFF, 0xF0, 0x10, 0x20),
            D3DCOLOR_ARGB(0xFF, 0x30, 0xE0, 0x40),
            D3DCOLOR_ARGB(0xFF, 0x50, 0x60, 0xD0),
            D3DCOLOR_ARGB(0xFF, 0xC0, 0xB0, 0xA0),
        },
        {
            D3DCOLOR_ARGB(0xFF, 0x91, 0x82, 0x73),
            D3DCOLOR_ARGB(0xFF, 0x60, 0x50, 0x40),
            D3DCOLOR_ARGB(0xFF, 0x30, 0x20, 0x10),
            D3DCOLOR_ARGB(0xFF, 0xA5, 0xB6, 0xC7),
            D3DCOLOR_ARGB(0xFF, 0xD8, 0xE9, 0xFA),
        },
    };
    const RECT corners[] = {
        {0, 0, 9, 7},
        {56, 0, 65, 7},
        {0, 30, 9, 37},
        {56, 30, 65, 37},
    };

    std::string diagnostic;
    for (const auto& pattern : patterns) {
        hr = device9->ColorFill(back_buffer.Get(), nullptr, pattern[0]);
        if (FAILED(hr)) return Fail("Classic D3D9 background ColorFill failed", hr);
        for (std::size_t corner = 0; corner < std::size(corners); ++corner) {
            hr = device9->ColorFill(back_buffer.Get(), &corners[corner], pattern[corner + 1]);
            if (FAILED(hr)) return Fail("Classic D3D9 corner ColorFill failed", hr);
        }
        if (!cojvr::backends::d3d9::CaptureBackBufferToD3D11(device9.Get(), diagnostic)) {
            std::cerr << diagnostic << '\n';
            return 1;
        }
    }

    std::cout << "Classic D3D9 changing asymmetric frames -> CPU readback -> D3D11 upload passed: "
              << diagnostic << '\n';
    return 0;
}
