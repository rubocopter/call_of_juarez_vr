#include "backends/d3d9/hook_registry.hpp"
#include <d3d9.h>
#include <windows.h>
#include <array>
#include <cstdio>
#include <mutex>

namespace {
using cojvr::backends::d3d9::HookRegistry;
using cojvr::backends::d3d9::HookRegistryResult;
using cojvr::backends::d3d9::HookSlotRequest;

using CreateDeviceFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
    D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
using CreateTextureFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD,
    D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
using CreateVolumeTextureFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, UINT,
    UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9**, HANDLE*);
using CreateCubeTextureFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, UINT, DWORD,
    D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9**, HANDLE*);
using CreateVertexBufferFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, DWORD, DWORD,
    D3DPOOL, IDirect3DVertexBuffer9**, HANDLE*);
using CreateIndexBufferFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, DWORD, D3DFORMAT,
    D3DPOOL, IDirect3DIndexBuffer9**, HANDLE*);

HookRegistry g_factory_hooks;
HookRegistry g_device_hooks;
std::mutex g_log_mutex;
HMODULE g_self = nullptr;
HANDLE g_log = INVALID_HANDLE_VALUE;

IDirect3D9* CreateSystemClassicFactory(const UINT version) noexcept {
    using CreateFn = IDirect3D9* (WINAPI*)(UINT);
    static std::once_flag load_once;
    static CreateFn create = nullptr;
    try {
        std::call_once(load_once, [] {
            wchar_t directory[32768]{};
            const UINT length = GetSystemDirectoryW(directory, 32768);
            if (!length || length >= 32768) return;
            wchar_t path[32768]{};
            if (wcscpy_s(path, directory) != 0 ||
                wcscat_s(path, L"\\d3d9.dll") != 0) return;
            const HMODULE module = LoadLibraryW(path);
            if (!module) return;
            create = reinterpret_cast<CreateFn>(GetProcAddress(module, "Direct3DCreate9"));
        });
    } catch (...) { return nullptr; }
    return create ? create(version) : nullptr;
}

void WriteRow(const char* kind, const void* device, const D3DPOOL pool, const DWORD usage,
    const D3DFORMAT format, const UINT width, const UINT height, const UINT depth,
    const UINT levels, const UINT length, const DWORD fvf, const bool shared, const HRESULT hr) noexcept {
    try {
        std::lock_guard lock(g_log_mutex);
        if (g_log == INVALID_HANDLE_VALUE) {
            wchar_t path[32768]{};
            const DWORD size = GetModuleFileNameW(g_self, path, 32768);
            if (!size || size >= 32768) return;
            wchar_t* slash = wcsrchr(path, L'\\');
            if (!slash) return;
            wcscpy_s(slash + 1, 32768 - (slash + 1 - path), L"cojvr-d3d9-pool-probe.csv");
            g_log = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL, nullptr);
            if (g_log == INVALID_HANDLE_VALUE) return;
            constexpr char header[] = "kind,device,pool,usage,format,width,height,depth,levels,length,fvf,shared_handle_parameter,hr\r\n";
            DWORD written = 0;
            WriteFile(g_log, header, sizeof(header) - 1, &written, nullptr);
        }
        char row[256]{};
        const int size = std::snprintf(row, sizeof(row), "%s,%p,%u,0x%08lX,0x%08X,%u,%u,%u,%u,%u,0x%08lX,%u,0x%08lX\r\n",
            kind, device, static_cast<unsigned>(pool), static_cast<unsigned long>(usage),
            static_cast<unsigned>(format), width, height, depth, levels, length,
            static_cast<unsigned long>(fvf), shared ? 1u : 0u,
            static_cast<unsigned long>(hr));
        if (size > 0 && static_cast<size_t>(size) < sizeof(row)) {
            DWORD written = 0;
            WriteFile(g_log, row, static_cast<DWORD>(size), &written, nullptr);
        }
    } catch (...) {
        // Observation must never change the Direct3D call's result.
    }
}

template <typename Fn>
Fn Original(IDirect3DDevice9* device, const size_t index) noexcept {
    return reinterpret_cast<Fn>(g_device_hooks.OriginalTarget(*reinterpret_cast<void***>(device), index));
}

HRESULT STDMETHODCALLTYPE CreateTexture(IDirect3DDevice9* device, UINT width, UINT height,
    UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** result,
    HANDLE* shared) {
    const HRESULT hr = Original<CreateTextureFn>(device, 23)(device, width, height, levels,
        usage, format, pool, result, shared);
    WriteRow("CreateTexture", device, pool, usage, format, width, height, 1, levels, 0, 0,
        shared != nullptr, hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE CreateVolumeTexture(IDirect3DDevice9* device, UINT width, UINT height,
    UINT depth, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool,
    IDirect3DVolumeTexture9** result, HANDLE* shared) {
    const HRESULT hr = Original<CreateVolumeTextureFn>(device, 24)(device, width, height, depth,
        levels, usage, format, pool, result, shared);
    WriteRow("CreateVolumeTexture", device, pool, usage, format, width, height, depth,
        levels, 0, 0, shared != nullptr, hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE CreateCubeTexture(IDirect3DDevice9* device, UINT edge, UINT levels,
    DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DCubeTexture9** result,
    HANDLE* shared) {
    const HRESULT hr = Original<CreateCubeTextureFn>(device, 25)(device, edge, levels, usage,
        format, pool, result, shared);
    WriteRow("CreateCubeTexture", device, pool, usage, format, edge, edge, 6, levels,
        0, 0, shared != nullptr, hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE CreateVertexBuffer(IDirect3DDevice9* device, UINT length,
    DWORD usage, DWORD fvf, D3DPOOL pool, IDirect3DVertexBuffer9** result, HANDLE* shared) {
    const HRESULT hr = Original<CreateVertexBufferFn>(device, 26)(device, length, usage, fvf,
        pool, result, shared);
    WriteRow("CreateVertexBuffer", device, pool, usage, D3DFMT_UNKNOWN, 0, 0, 0, 0,
        length, fvf, shared != nullptr, hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE CreateIndexBuffer(IDirect3DDevice9* device, UINT length,
    DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DIndexBuffer9** result,
    HANDLE* shared) {
    const HRESULT hr = Original<CreateIndexBufferFn>(device, 27)(device, length, usage, format,
        pool, result, shared);
    WriteRow("CreateIndexBuffer", device, pool, usage, format, 0, 0, 0, 0, length, 0,
        shared != nullptr, hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE CreateDevice(IDirect3D9* factory, UINT adapter, D3DDEVTYPE type,
    HWND window, DWORD flags, D3DPRESENT_PARAMETERS* parameters, IDirect3DDevice9** result) {
    const auto original = reinterpret_cast<CreateDeviceFn>(
        g_factory_hooks.OriginalTarget(*reinterpret_cast<void***>(factory), 16));
    const HRESULT hr = original(factory, adapter, type, window, flags, parameters, result);
    if (SUCCEEDED(hr) && result && *result) {
        void** vtable = *reinterpret_cast<void***>(*result);
        const std::array<HookSlotRequest, 5> requests{{
            {23, reinterpret_cast<void*>(&CreateTexture)},
            {24, reinterpret_cast<void*>(&CreateVolumeTexture)},
            {25, reinterpret_cast<void*>(&CreateCubeTexture)},
            {26, reinterpret_cast<void*>(&CreateVertexBuffer)},
            {27, reinterpret_cast<void*>(&CreateIndexBuffer)},
        }};
        const auto hook = g_device_hooks.Install(vtable, requests);
        WriteRow("DeviceHook", *result, D3DPOOL_DEFAULT, 0, D3DFMT_UNKNOWN,
            0, 0, 0, 0, 0, 0, false, static_cast<HRESULT>(hook.result));
    }
    return hr;
}
} // namespace

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT version) {
    IDirect3D9* factory = CreateSystemClassicFactory(version);
    if (factory) {
        if (!cojvr::backends::d3d9::PinModuleForAddress(
                reinterpret_cast<void*>(&CreateDevice))) {
            WriteRow("FactoryHook", factory, D3DPOOL_DEFAULT, 0, D3DFMT_UNKNOWN,
                0, 0, 0, 0, 0, 0, false, 2);
            return factory;
        }
        const HookSlotRequest request{16, reinterpret_cast<void*>(&CreateDevice)};
        const auto hook = g_factory_hooks.Install(
            *reinterpret_cast<void***>(factory), std::span(&request, 1));
        WriteRow("FactoryHook", factory, D3DPOOL_DEFAULT, 0, D3DFMT_UNKNOWN,
            0, 0, 0, 0, 0, 0, false, static_cast<HRESULT>(hook.result));
    }
    return factory;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = module;
        DisableThreadLibraryCalls(module);
    } else if (reason == DLL_PROCESS_DETACH && g_log != INVALID_HANDLE_VALUE) {
        CloseHandle(g_log);
    }
    return TRUE;
}
