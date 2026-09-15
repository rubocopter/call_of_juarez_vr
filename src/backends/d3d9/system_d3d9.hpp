#pragma once

#include <d3d9.h>
#include <windows.h>

#include <filesystem>

namespace cojvr::backends::d3d9 {

using Direct3DCreate9Fn = IDirect3D9* (WINAPI*)(UINT);
using Direct3DCreate9ExFn = HRESULT (WINAPI*)(UINT, IDirect3D9Ex**);

HMODULE SystemD3D9Module() noexcept;
std::filesystem::path SystemD3D9ModulePath() noexcept;
bool IsExpectedSystemD3D9Module() noexcept;
Direct3DCreate9Fn SystemDirect3DCreate9() noexcept;
Direct3DCreate9ExFn SystemDirect3DCreate9Ex() noexcept;

} // namespace cojvr::backends::d3d9
