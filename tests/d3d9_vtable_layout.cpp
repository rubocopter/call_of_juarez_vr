#define CINTERFACE
#include <d3d9.h>

#include <cstddef>

static_assert(sizeof(IDirect3DDevice9Vtbl) / sizeof(void*) == 119);
static_assert(offsetof(IDirect3DDevice9Vtbl, Release) / sizeof(void*) == 2);
static_assert(offsetof(IDirect3DDevice9Vtbl, Reset) / sizeof(void*) == 16);
static_assert(offsetof(IDirect3DDevice9Vtbl, Present) / sizeof(void*) == 17);
static_assert(offsetof(IDirect3DDevice9Vtbl, CreateTexture) / sizeof(void*) == 23);
static_assert(offsetof(IDirect3DDevice9Vtbl, CreateVolumeTexture) / sizeof(void*) == 24);
static_assert(offsetof(IDirect3DDevice9Vtbl, CreateCubeTexture) / sizeof(void*) == 25);
static_assert(offsetof(IDirect3DDevice9Vtbl, EndScene) / sizeof(void*) == 42);
static_assert(sizeof(IDirect3DSwapChain9Vtbl) / sizeof(void*) == 10);
static_assert(offsetof(IDirect3DSwapChain9Vtbl, Present) / sizeof(void*) == 3);

int main() {
    return 0;
}
