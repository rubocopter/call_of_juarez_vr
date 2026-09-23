#include "backends/d3d9/system_d3d9.hpp"

#include <windows.h>

#include <d3d9.h>
#include <wrl/client.h>

#include <iostream>

using Microsoft::WRL::ComPtr;

namespace {

int Fail(const char* message, const HRESULT hr = S_OK) {
    std::cerr << message;
    if (FAILED(hr)) {
        std::cerr << " (HRESULT 0x" << std::hex << static_cast<unsigned long>(hr) << ')';
    }
    std::cerr << '\n';
    return 1;
}

bool IsUnavailable(const HRESULT hr) {
    return hr == D3DERR_NOTAVAILABLE || hr == D3DERR_DEVICELOST;
}

HRESULT ExerciseTexture2D(IDirect3DDevice9* device, D3DPOOL pool, DWORD usage) {
    ComPtr<IDirect3DTexture9> texture;
    HRESULT hr = device->CreateTexture(
        32, 32, 1, usage, D3DFMT_A8R8G8B8, pool, &texture, nullptr);
    if (FAILED(hr) || !texture) return FAILED(hr) ? hr : E_FAIL;

    D3DLOCKED_RECT locked{};
    // ChromeEngine believes these are MANAGED resources and therefore does
    // not know that a compatibility path may have made them dynamic.
    // Exercise the lock flags the engine actually uses rather than relying on
    // D3DLOCK_DISCARD to make the substituted resource easier to lock.
    hr = texture->LockRect(0, &locked, nullptr, 0);
    if (FAILED(hr)) return hr;
    static_cast<unsigned char*>(locked.pBits)[0] = 0x5A;
    hr = texture->UnlockRect(0);
    if (FAILED(hr)) return hr;
    hr = device->SetTexture(0, texture.Get());
    if (FAILED(hr)) return hr;
    return device->SetTexture(0, nullptr);
}

HRESULT ExerciseCubeTexture(IDirect3DDevice9* device, D3DPOOL pool, DWORD usage) {
    ComPtr<IDirect3DCubeTexture9> texture;
    HRESULT hr = device->CreateCubeTexture(
        16, 1, usage, D3DFMT_A8R8G8B8, pool, &texture, nullptr);
    if (FAILED(hr) || !texture) return FAILED(hr) ? hr : E_FAIL;

    D3DLOCKED_RECT locked{};
    hr = texture->LockRect(
        D3DCUBEMAP_FACE_POSITIVE_X,
        0,
        &locked,
        nullptr,
        0);
    if (FAILED(hr)) return hr;
    static_cast<unsigned char*>(locked.pBits)[0] = 0x6B;
    hr = texture->UnlockRect(D3DCUBEMAP_FACE_POSITIVE_X, 0);
    if (FAILED(hr)) return hr;
    hr = device->SetTexture(0, texture.Get());
    if (FAILED(hr)) return hr;
    return device->SetTexture(0, nullptr);
}

HRESULT ExerciseVolumeTexture(IDirect3DDevice9* device, D3DPOOL pool, DWORD usage) {
    ComPtr<IDirect3DVolumeTexture9> texture;
    HRESULT hr = device->CreateVolumeTexture(
        8, 8, 4, 1, usage, D3DFMT_A8R8G8B8, pool, &texture, nullptr);
    if (FAILED(hr) || !texture) return FAILED(hr) ? hr : E_FAIL;

    D3DLOCKED_BOX locked{};
    hr = texture->LockBox(0, &locked, nullptr, 0);
    if (FAILED(hr)) return hr;
    static_cast<unsigned char*>(locked.pBits)[0] = 0x7C;
    hr = texture->UnlockBox(0);
    if (FAILED(hr)) return hr;
    hr = device->SetTexture(0, texture.Get());
    if (FAILED(hr)) return hr;
    return device->SetTexture(0, nullptr);
}

} // namespace

int main() {
    const auto create_classic = cojvr::backends::d3d9::SystemDirect3DCreate9();
    const auto create_ex = cojvr::backends::d3d9::SystemDirect3DCreate9Ex();
    if (!create_classic || !create_ex) {
        return Fail("System d3d9.dll is missing a required D3D9 entry point");
    }

    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = GetDesktopWindow();
    present.BackBufferFormat = D3DFMT_UNKNOWN;
    present.BackBufferWidth = 64;
    present.BackBufferHeight = 64;

    ComPtr<IDirect3D9> classic_factory;
    classic_factory.Attach(create_classic(D3D_SDK_VERSION));
    if (!classic_factory) return Fail("Direct3DCreate9 failed");

    ComPtr<IDirect3DDevice9> classic_device;
    HRESULT hr = classic_factory->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        present.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &present,
        &classic_device);
    if (FAILED(hr) || !classic_device) {
        if (IsUnavailable(hr)) {
            std::cout << "D3D9Ex legacy-resource contract skipped: classic HAL device unavailable\n";
            return 77;
        }
        return Fail("Classic D3D9 CreateDevice failed", hr);
    }

    ComPtr<IDirect3D9Ex> ex_factory;
    hr = create_ex(D3D_SDK_VERSION, &ex_factory);
    if (FAILED(hr) || !ex_factory) return Fail("Direct3DCreate9Ex failed", hr);

    D3DPRESENT_PARAMETERS ex_present = present;
    ComPtr<IDirect3DDevice9Ex> ex_device;
    hr = ex_factory->CreateDeviceEx(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        ex_present.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &ex_present,
        nullptr,
        &ex_device);
    if (FAILED(hr) || !ex_device) {
        if (IsUnavailable(hr)) {
            std::cout << "D3D9Ex legacy-resource contract skipped: Ex HAL device unavailable\n";
            return 77;
        }
        return Fail("D3D9Ex CreateDeviceEx failed", hr);
    }

    const HRESULT classic_2d = ExerciseTexture2D(classic_device.Get(), D3DPOOL_MANAGED, 0);
    const HRESULT classic_cube = ExerciseCubeTexture(classic_device.Get(), D3DPOOL_MANAGED, 0);
    const HRESULT classic_volume = ExerciseVolumeTexture(classic_device.Get(), D3DPOOL_MANAGED, 0);
    if (FAILED(classic_2d) || FAILED(classic_cube) || FAILED(classic_volume)) {
        return Fail("Classic D3D9 did not satisfy the legacy MANAGED texture contract",
                    FAILED(classic_2d) ? classic_2d : (FAILED(classic_cube) ? classic_cube : classic_volume));
    }

    const HRESULT ex_managed_2d = ExerciseTexture2D(ex_device.Get(), D3DPOOL_MANAGED, 0);
    const HRESULT ex_managed_cube = ExerciseCubeTexture(ex_device.Get(), D3DPOOL_MANAGED, 0);
    const HRESULT ex_managed_volume = ExerciseVolumeTexture(ex_device.Get(), D3DPOOL_MANAGED, 0);
    if (ex_managed_2d != D3DERR_INVALIDCALL ||
        ex_managed_cube != D3DERR_INVALIDCALL ||
        ex_managed_volume != D3DERR_INVALIDCALL) {
        return Fail("D3D9Ex unexpectedly accepted a legacy D3DPOOL_MANAGED texture contract");
    }

    constexpr DWORD dynamic_usage = D3DUSAGE_DYNAMIC;
    const HRESULT ex_dynamic_2d = ExerciseTexture2D(ex_device.Get(), D3DPOOL_DEFAULT, dynamic_usage);
    const HRESULT ex_dynamic_cube = ExerciseCubeTexture(ex_device.Get(), D3DPOOL_DEFAULT, dynamic_usage);
    const HRESULT ex_dynamic_volume = ExerciseVolumeTexture(ex_device.Get(), D3DPOOL_DEFAULT, dynamic_usage);
    if (FAILED(ex_dynamic_2d) || FAILED(ex_dynamic_cube) || FAILED(ex_dynamic_volume)) {
        return Fail("D3D9Ex DEFAULT|DYNAMIC compatibility texture contract failed",
                    FAILED(ex_dynamic_2d) ? ex_dynamic_2d :
                    (FAILED(ex_dynamic_cube) ? ex_dynamic_cube : ex_dynamic_volume));
    }

    // A classic MANAGED resource is intentionally retained by the application
    // across Reset. Verify that the Ex-compatible replacement does not force
    // ChromeEngine to learn DEFAULT-pool lost-device bookkeeping just to
    // survive an early startup display transition.
    ComPtr<IDirect3DTexture9> reset_texture;
    hr = ex_device->CreateTexture(
        32, 32, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, &reset_texture, nullptr);
    if (FAILED(hr) || !reset_texture) {
        return Fail("D3D9Ex reset-survival texture creation failed", hr);
    }
    D3DPRESENT_PARAMETERS reset_present = ex_present;
    hr = ex_device->Reset(&reset_present);
    if (FAILED(hr)) {
        return Fail("D3D9Ex Reset rejected a retained DEFAULT|DYNAMIC compatibility texture", hr);
    }
    D3DLOCKED_RECT after_reset{};
    hr = reset_texture->LockRect(0, &after_reset, nullptr, 0);
    if (FAILED(hr)) {
        return Fail("D3D9Ex compatibility texture was not lockable after Reset", hr);
    }
    static_cast<unsigned char*>(after_reset.pBits)[0] = 0x8D;
    hr = reset_texture->UnlockRect(0);
    if (FAILED(hr)) return Fail("D3D9Ex compatibility texture UnlockRect failed after Reset", hr);
    hr = ex_device->SetTexture(0, reset_texture.Get());
    if (FAILED(hr)) return Fail("D3D9Ex compatibility texture could not be rebound after Reset", hr);
    (void)ex_device->SetTexture(0, nullptr);

    std::cout
        << "Classic MANAGED texture contract passed; D3D9Ex rejected MANAGED with D3DERR_INVALIDCALL; "
        << "D3D9Ex DEFAULT|DYNAMIC preserved create/lock/use for 2D, cube and volume textures "
        << "and survived Reset while retained\n";
    return 0;
}
