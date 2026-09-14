#include "backends/d3d9/system_d3d9.hpp"

#include <array>
#include <mutex>
#include <string>

namespace cojvr::backends::d3d9 {
namespace {

HMODULE g_system_d3d9 = nullptr;
Direct3DCreate9Fn g_direct3d_create9 = nullptr;
Direct3DCreate9ExFn g_direct3d_create9_ex = nullptr;
std::once_flag g_load_once;

void LoadSystemD3D9() noexcept {
    try {
        std::array<wchar_t, MAX_PATH> directory{};
        const UINT length = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
        if (length == 0 || length >= directory.size()) return;

        std::wstring path(directory.data(), length);
        path += L"\\d3d9.dll";

        g_system_d3d9 = LoadLibraryW(path.c_str());
        if (!g_system_d3d9) return;

        g_direct3d_create9 = reinterpret_cast<Direct3DCreate9Fn>(
            GetProcAddress(g_system_d3d9, "Direct3DCreate9"));
        g_direct3d_create9_ex = reinterpret_cast<Direct3DCreate9ExFn>(
            GetProcAddress(g_system_d3d9, "Direct3DCreate9Ex"));
    } catch (...) {
        g_system_d3d9 = nullptr;
        g_direct3d_create9 = nullptr;
        g_direct3d_create9_ex = nullptr;
    }
}

} // namespace

HMODULE SystemD3D9Module() noexcept {
    try {
        std::call_once(g_load_once, LoadSystemD3D9);
    } catch (...) {
        return nullptr;
    }
    return g_system_d3d9;
}

Direct3DCreate9Fn SystemDirect3DCreate9() noexcept {
    (void)SystemD3D9Module();
    return g_direct3d_create9;
}

Direct3DCreate9ExFn SystemDirect3DCreate9Ex() noexcept {
    (void)SystemD3D9Module();
    return g_direct3d_create9_ex;
}

} // namespace cojvr::backends::d3d9
