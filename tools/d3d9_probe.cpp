#include "backends/d3d9/system_d3d9.hpp"

#include <d3d9.h>

#include <iostream>

int wmain() {
    const auto create_d3d9 = cojvr::backends::d3d9::SystemDirect3DCreate9();
    if (!create_d3d9 || !cojvr::backends::d3d9::IsExpectedSystemD3D9Module()) {
        std::cerr << "Native D3D9 probe did not load the expected system d3d9.dll\n";
        return 1;
    }
    IDirect3D9* d3d = create_d3d9(D3D_SDK_VERSION);
    if (!d3d) {
        std::cerr << "Direct3DCreate9 failed\n";
        return 1;
    }

    const UINT adapters = d3d->GetAdapterCount();
    std::cout << "Direct3D 9 available; adapters=" << adapters << "\n";
    d3d->Release();
    return adapters > 0 ? 0 : 77;
}
