#include <d3d9.h>
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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

bool SameComIdentity(IUnknown* left, IUnknown* right) {
    IUnknown* left_identity = nullptr;
    IUnknown* right_identity = nullptr;
    const HRESULT left_result = left->QueryInterface(
        IID_IUnknown, reinterpret_cast<void**>(&left_identity));
    const HRESULT right_result = right->QueryInterface(
        IID_IUnknown, reinterpret_cast<void**>(&right_identity));
    const bool same = SUCCEEDED(left_result) && SUCCEEDED(right_result) &&
        left_identity == right_identity;
    if (left_identity) left_identity->Release();
    if (right_identity) right_identity->Release();
    return same;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return Fail("expected proxy path");

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
        IDirect3D9* recovered_factory = nullptr;
        const HRESULT recovered_result = device->GetDirect3D(&recovered_factory);
        const bool factory_identity_matches = SUCCEEDED(recovered_result) &&
            recovered_factory && SameComIdentity(d3d, recovered_factory);
        if (recovered_factory) recovered_factory->Release();
        if (!factory_identity_matches) {
            return Fail("D3D9Ex device GetDirect3D changed the game's factory COM identity");
        }

        const HRESULT begin_result = device->BeginScene();
        if (FAILED(begin_result)) return Fail("BeginScene failed after hook installation");
        const HRESULT clear_result = device->Clear(
            0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(8, 16, 24), 1.0F, 0);
        if (FAILED(clear_result)) return Fail("Clear failed after hook installation");
        const HRESULT end_result = device->EndScene();
        if (FAILED(end_result)) return Fail("EndScene failed after hook installation");
        const HRESULT present_result = device->Present(nullptr, nullptr, nullptr, nullptr);
        if (FAILED(present_result)) return Fail("Present failed after hook installation");
        const HRESULT reset_result = device->Reset(&presentation);
        if (FAILED(reset_result)) return Fail("Reset failed after hook installation");

        IDirect3DDevice9Ex* device_ex = nullptr;
        const HRESULT query_ex_result = device->QueryInterface(
            IID_IDirect3DDevice9Ex, reinterpret_cast<void**>(&device_ex));
        if (FAILED(query_ex_result) || device_ex == nullptr) {
            return Fail("proxy D3D9Ex bridge did not return an Ex-capable device");
        }
        device_ex->Release();

        HANDLE shared_handle = nullptr;
        IDirect3DTexture9* shared_texture = nullptr;
        const HRESULT shared_result = device->CreateTexture(
            64, 64, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT, &shared_texture, &shared_handle);
        if (FAILED(shared_result) || shared_texture == nullptr || shared_handle == nullptr) {
            if (shared_texture) shared_texture->Release();
            return Fail("proxy D3D9Ex bridge could not create a shared render target");
        }
        shared_texture->Release();
    }
    if (device) device->Release();
    d3d->Release();
    DestroyWindow(window);
    // The D3D9 vtable hook is deliberately process-lifetime. Keep the proxy
    // module loaded until process teardown so patched entries always point to
    // valid code.

    const bool logged_identity = FileContains(log_path, "d3d9 bootstrap: host=");
    const bool logged_factory = FileContains(log_path, "d3d9 D3D9Ex bridge: native Ex factory active");
    const bool logged_device = FileContains(log_path, "d3d9 CreateDevice:");
    const bool logged_hooks = FileContains(log_path, "d3d9 device hooks: Present/Reset active");
    const bool logged_present = FileContains(log_path, "d3d9 Present: frame boundary observed");
    const bool logged_reset = FileContains(log_path, "d3d9 Reset: hr=0x0");
    if (!logged_identity) return Fail("proxy did not log host identity");
    if (!logged_factory) return Fail("proxy did not log native Ex factory activation");
    if (!logged_device) return Fail("proxy did not log device creation");
    if (SUCCEEDED(result) && !logged_hooks) return Fail("proxy did not install D3D9 device hooks");
    if (SUCCEEDED(result) && !logged_present) return Fail("proxy did not observe a D3D9 Present");
    if (SUCCEEDED(result) && !logged_reset) return Fail("proxy did not observe a successful D3D9 Reset");

    if (FAILED(result)) {
        if (result == D3DERR_NOTAVAILABLE) {
            std::cout << "d3d9 proxy smoke skipped: HAL device unavailable\n";
            return 77;
        }
        return Fail("proxy D3D9Ex CreateDevice failed unexpectedly");
    }
    std::cout << "d3d9 proxy D3D9Ex bridge smoke passed with device creation\n";
    return 0;
}
