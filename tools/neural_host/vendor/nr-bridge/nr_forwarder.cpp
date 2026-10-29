// nvngx.dll_nr_bridge.dll: the Neural Rendering model (nvngx_dlssnr.dll) checks the module that calls
// its entry points and refuses any whose file path does not contain "nvngx.dll". This DLL is named to
// pass that check and does nothing but forward. Results go through a volatile so the compiler cannot
// turn the forward into a tail call (which would make the caller's return address the real caller).
// Author: LowPopLabs
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>

namespace
{
typedef int (__cdecl *PFN_InitExt)(unsigned long long, const wchar_t *, ID3D12Device *, int, const void *);
typedef int (__cdecl *PFN_Create)(ID3D12GraphicsCommandList *, int, const void *, void **);
typedef int (__cdecl *PFN_Evaluate)(ID3D12GraphicsCommandList *, const void *, const void *, void *);
typedef int (__cdecl *PFN_Release)(void *);
HMODULE g_mod = nullptr; PFN_InitExt g_init = nullptr; PFN_Create g_create = nullptr; PFN_Evaluate g_eval = nullptr; PFN_Release g_release = nullptr;
}

extern "C" {
__declspec(dllexport) int __cdecl bnr_snip_load(const wchar_t *path)
{
    if (g_mod) return g_create ? 1 : 0;
    g_mod = LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!g_mod) return -(int)GetLastError();
    g_init = (PFN_InitExt)GetProcAddress(g_mod, "NVSDK_NGX_D3D12_Init_Ext");
    g_create = (PFN_Create)GetProcAddress(g_mod, "NVSDK_NGX_D3D12_CreateFeature");
    g_eval = (PFN_Evaluate)GetProcAddress(g_mod, "NVSDK_NGX_D3D12_EvaluateFeature");
    g_release = (PFN_Release)GetProcAddress(g_mod, "NVSDK_NGX_D3D12_ReleaseFeature");
    return (g_init && g_create && g_eval) ? 1 : 0;
}
__declspec(dllexport) int __cdecl bnr_snip_init(unsigned long long appId, const wchar_t *dataPath, ID3D12Device *dev, int sdk, const void *fcInfo)
{
    if (!g_init) return 0;
    volatile int r = g_init(appId, dataPath, dev, sdk, fcInfo);
    return r;
}
__declspec(dllexport) int __cdecl bnr_snip_create(ID3D12GraphicsCommandList *cl, int feature, void *params, void **handle)
{
    if (!g_create) return 0;
    volatile int r = g_create(cl, feature, params, handle);
    return r;
}
__declspec(dllexport) int __cdecl bnr_snip_evaluate(ID3D12GraphicsCommandList *cl, void *handle, void *params)
{
    if (!g_eval) return 0;
    volatile int r = g_eval(cl, handle, params, nullptr);
    return r;
}
__declspec(dllexport) int __cdecl bnr_snip_release(void *handle)
{
    if (!g_release) return 0;
    volatile int r = g_release(handle);
    return r;
}
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
