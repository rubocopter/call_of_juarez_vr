#include <d3d9.h>
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path dll_path(argv[1]);
    const auto log_path = dll_path.parent_path() / L"cojvr-d3d9-pool-probe.csv";
    std::filesystem::remove(log_path);
    HWND window = CreateWindowExW(0, L"STATIC", L"pool probe smoke", WS_OVERLAPPEDWINDOW,
        0, 0, 320, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return 6;
    ShowWindow(window, SW_SHOWNORMAL);
    D3DPRESENT_PARAMETERS pp{};
    pp.BackBufferWidth = 320;
    pp.BackBufferHeight = 240;
    pp.BackBufferCount = 1;
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    IDirect3D9* baseline_factory = Direct3DCreate9(D3D_SDK_VERSION);
    if (!baseline_factory) return 77;
    IDirect3DDevice9* baseline_device = nullptr;
    const HRESULT baseline_hr = baseline_factory->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &baseline_device);
    if (baseline_device) baseline_device->Release();
    baseline_factory->Release();
    HMODULE module = LoadLibraryW(dll_path.c_str());
    if (!module) return 3;
    const auto create = reinterpret_cast<IDirect3D9* (WINAPI*)(UINT)>(
        GetProcAddress(module, "Direct3DCreate9"));
    if (!create) return 4;
    IDirect3D9* factory = create(D3D_SDK_VERSION);
    if (!factory) return 5;
    if (FAILED(baseline_hr)) {
        std::ifstream input(log_path);
        std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (content.find("FactoryHook,") == std::string::npos) return 12;
        factory->Release();
        DestroyWindow(window);
        FreeLibrary(module);
        std::cerr << "Host classic D3D9 device unavailable: " << std::hex << baseline_hr << '\n';
        return 77;
    }
    IDirect3DDevice9* device = nullptr;
    HRESULT hr = factory->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    if (FAILED(hr) || !device) {
        std::cerr << "CreateDevice failed: " << std::hex << hr << '\n';
        return 7;
    }

    IDirect3DTexture9* texture = nullptr;
    IDirect3DCubeTexture9* cube = nullptr;
    IDirect3DVolumeTexture9* volume = nullptr;
    IDirect3DVertexBuffer9* vertex = nullptr;
    IDirect3DIndexBuffer9* index = nullptr;
    const HRESULT results[] = {
        device->CreateTexture(16, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr),
        device->CreateCubeTexture(8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &cube, nullptr),
        device->CreateVolumeTexture(8, 8, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &volume, nullptr),
        device->CreateVertexBuffer(128, 0, 0, D3DPOOL_MANAGED, &vertex, nullptr),
        device->CreateIndexBuffer(64, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &index, nullptr),
    };
    for (HRESULT result : results) {
        if (FAILED(result)) {
            std::cerr << "D3D9 resource creation failed: " << std::hex << result << '\n';
            return 8;
        }
    }
    index->Release(); vertex->Release(); volume->Release(); cube->Release(); texture->Release();
    device->Release(); factory->Release(); DestroyWindow(window);

    std::ifstream input(log_path);
    if (!input) return 9;
    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    for (const char* kind : {"CreateTexture", "CreateCubeTexture", "CreateVolumeTexture",
             "CreateVertexBuffer", "CreateIndexBuffer"}) {
        if (content.find(std::string(kind) + ",") == std::string::npos) return 10;
    }
    if (content.find(",1,0x00000000,") == std::string::npos) return 11;
    FreeLibrary(module);
    return 0;
}
