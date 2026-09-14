#include <d3d9.h>
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

namespace {

using Direct3DCreate9Fn = IDirect3D9* (WINAPI*)(UINT);

LRESULT CALLBACK SmokeWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcW(window, message, wparam, lparam);
}

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

bool FileContains(const std::filesystem::path& path, const char* needle) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    const std::string contents(
        (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return contents.find(needle) != std::string::npos;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2 && argc != 3) {
        return Fail("expected proxy path [--expect-readback|--expect-openvr-flat]");
    }
    const std::wstring_view expectation = argc == 3 ? std::wstring_view(argv[2]) : L"";
    const bool expect_readback = expectation == L"--expect-readback";
    const bool expect_openvr_flat = expectation == L"--expect-openvr-flat";
    if (argc == 3 && !expect_readback && !expect_openvr_flat) {
        return Fail("unknown proxy smoke expectation");
    }

    const std::filesystem::path proxy_path = argv[1];
    const std::filesystem::path log_path = proxy_path.parent_path() / L"cojvr.log";
    std::error_code remove_error;
    std::filesystem::remove(log_path, remove_error);

    HMODULE proxy = LoadLibraryW(proxy_path.c_str());
    if (!proxy) return Fail("failed to load proxy");

    const auto create = reinterpret_cast<Direct3DCreate9Fn>(
        GetProcAddress(proxy, "Direct3DCreate9"));
    if (!create) {
        FreeLibrary(proxy);
        return Fail("Direct3DCreate9 export missing");
    }

    IDirect3D9* d3d = create(D3D_SDK_VERSION);
    if (!d3d) {
        FreeLibrary(proxy);
        return Fail("proxy Direct3DCreate9 failed");
    }

    const wchar_t* class_name = L"CoJVRD3D9ProxySmoke";
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = SmokeWindowProc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = class_name;
    if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        d3d->Release();
        FreeLibrary(proxy);
        return Fail("window class registration failed");
    }

    HWND window = CreateWindowExW(
        0, class_name, L"CoJ VR D3D9 proxy smoke", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 240, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!window) {
        d3d->Release();
        FreeLibrary(proxy);
        return Fail("window creation failed");
    }

    D3DPRESENT_PARAMETERS presentation{};
    presentation.Windowed = TRUE;
    presentation.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentation.hDeviceWindow = window;

    IDirect3DDevice9* device = nullptr;
    const HRESULT result = d3d->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &presentation, &device);

    if (device && SUCCEEDED(result)) {
        const HRESULT clear_result = device->Clear(
            0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(8, 16, 24), 1.0F, 0);
        if (FAILED(clear_result)) return Fail("Clear failed after hook installation");
        const HRESULT present_result = device->Present(nullptr, nullptr, nullptr, nullptr);
        if (FAILED(present_result)) return Fail("Present failed after hook installation");
        const HRESULT reset_result = device->Reset(&presentation);
        if (FAILED(reset_result)) return Fail("Reset failed after hook installation");
    }
    if (device) device->Release();
    d3d->Release();
    DestroyWindow(window);
    // The D3D9 vtable hook is deliberately process-lifetime. Keep the proxy
    // module loaded until process teardown so patched entries always point to
    // valid code.

    const bool logged_identity = FileContains(log_path, "d3d9 bootstrap: host=");
    const bool logged_forwarder = FileContains(log_path, "d3d9 Direct3DCreate9: forwarding wrapper active");
    const bool logged_device = FileContains(log_path, "d3d9 CreateDevice:");
    const bool logged_hooks = FileContains(log_path, "d3d9 device hooks: Present/Reset active");
    const bool logged_present = FileContains(log_path, "d3d9 Present: frame boundary observed");
    const bool logged_reset = FileContains(log_path, "d3d9 Reset: hr=0x0");
    const bool logged_readback = FileContains(
        log_path, "d3d9 classic readback -> D3D11: success");
    const bool logged_openvr_flat = FileContains(
        log_path, "d3d9 OpenVR flat bridge: first stereo submission success");

    if (!logged_identity) return Fail("proxy did not log host identity");
    if (!logged_forwarder) return Fail("proxy did not log forwarding activation");
    if (!logged_device) return Fail("proxy did not log device creation");
    if (SUCCEEDED(result) && !logged_hooks) return Fail("proxy did not install D3D9 device hooks");
    if (SUCCEEDED(result) && !logged_present) return Fail("proxy did not observe a D3D9 Present");
    if (SUCCEEDED(result) && !logged_reset) return Fail("proxy did not observe a successful D3D9 Reset");
    if (SUCCEEDED(result) && expect_readback && !logged_readback) {
        return Fail("proxy readback diagnostic did not complete D3D9 -> CPU -> D3D11 upload");
    }
    if (SUCCEEDED(result) && expect_openvr_flat && !logged_openvr_flat) {
        return Fail("proxy OpenVR flat bridge did not submit the D3D9 frame to SteamVR");
    }

    if (FAILED(result)) {
        std::filesystem::remove(log_path, remove_error);
        std::cout << "d3d9 proxy smoke passed without an available HAL device\n";
        return 0;
    }
    std::filesystem::remove(log_path, remove_error);
    std::cout << "d3d9 proxy smoke passed with device creation\n";
    return 0;
}
