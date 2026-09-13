#include <d3d9.h>

#include <iostream>

int wmain() {
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) {
        std::cerr << "Direct3DCreate9 failed\n";
        return 1;
    }

    const UINT adapters = d3d->GetAdapterCount();
    std::cout << "Direct3D 9 available; adapters=" << adapters << "\n";
    d3d->Release();
    return adapters > 0 ? 0 : 2;
}
