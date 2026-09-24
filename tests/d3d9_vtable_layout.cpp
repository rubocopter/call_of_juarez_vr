#define CINTERFACE
#include <d3d9.h>

#include <cstddef>

static_assert(sizeof(IDirect3DDevice9Vtbl) / sizeof(void*) == 119);
static_assert(sizeof(IDirect3D9Vtbl) / sizeof(void*) == 17);
static_assert(sizeof(IDirect3D9ExVtbl) / sizeof(void*) == 22);
static_assert(sizeof(IDirect3DDevice9ExVtbl) / sizeof(void*) == 134);
static_assert(offsetof(IDirect3DDevice9Vtbl, Release) / sizeof(void*) == 2);
static_assert(offsetof(IDirect3DDevice9Vtbl, TestCooperativeLevel) / sizeof(void*) == 3);
static_assert(offsetof(IDirect3DDevice9Vtbl, GetDirect3D) / sizeof(void*) == 6);
static_assert(offsetof(IDirect3DDevice9Vtbl, Reset) / sizeof(void*) == 16);
static_assert(offsetof(IDirect3DDevice9Vtbl, Present) / sizeof(void*) == 17);
static_assert(offsetof(IDirect3DDevice9Vtbl, CreateTexture) / sizeof(void*) == 23);
static_assert(offsetof(IDirect3DDevice9Vtbl, CreateVolumeTexture) / sizeof(void*) == 24);
static_assert(offsetof(IDirect3DDevice9Vtbl, CreateCubeTexture) / sizeof(void*) == 25);
static_assert(offsetof(IDirect3DDevice9Vtbl, EndScene) / sizeof(void*) == 42);
static_assert(offsetof(IDirect3DDevice9Vtbl, Clear) / sizeof(void*) == 43);
static_assert(offsetof(IDirect3DDevice9Vtbl, SetRenderTarget) / sizeof(void*) == 37);
static_assert(offsetof(IDirect3DDevice9Vtbl, SetDepthStencilSurface) / sizeof(void*) == 39);
static_assert(offsetof(IDirect3DDevice9Vtbl, CreateStateBlock) / sizeof(void*) == 59);
static_assert(offsetof(IDirect3DDevice9Vtbl, SetTexture) / sizeof(void*) == 65);
static_assert(sizeof(IDirect3DSwapChain9Vtbl) / sizeof(void*) == 10);
static_assert(sizeof(IDirect3DSwapChain9ExVtbl) / sizeof(void*) == 13);
static_assert(offsetof(IDirect3DSwapChain9Vtbl, Present) / sizeof(void*) == 3);
static_assert(offsetof(IDirect3DDevice9ExVtbl, PresentEx) / sizeof(void*) == 121);

int main() {
    return 0;
}
