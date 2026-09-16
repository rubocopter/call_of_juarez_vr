#include "backends/d3d9/system_d3d9.hpp"

#include <array>
#include <mutex>
#include <string>
#include <vector>

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

std::filesystem::path SystemD3D9ModulePath() noexcept {
    try {
        HMODULE module = SystemD3D9Module();
        if (!module) return {};
        std::vector<wchar_t> path(32768);
        const DWORD length = GetModuleFileNameW(
            module, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size()) return {};
        return std::filesystem::path(std::wstring_view(path.data(), length));
    } catch (...) {
        return {};
    }
}

bool IsExpectedSystemD3D9Module() noexcept {
    try {
        std::vector<wchar_t> system_directory(32768);
        const UINT length = GetSystemDirectoryW(
            system_directory.data(), static_cast<UINT>(system_directory.size()));
        if (length == 0 || length >= system_directory.size()) return false;
        const std::filesystem::path expected =
            std::filesystem::path(std::wstring_view(system_directory.data(), length)) / L"d3d9.dll";
        const std::filesystem::path actual = SystemD3D9ModulePath();
        if (actual.empty()) return false;
        return _wcsicmp(actual.c_str(), expected.c_str()) == 0;
    } catch (...) {
        return false;
    }
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
