#include "backends/d3d9/system_d3d9.hpp"
#include "games/call_of_juarez/d3d9ex_legacy_resource_compat.hpp"

#include <windows.h>

#include <d3d9.h>
#include <wrl/client.h>

#include <atomic>
#include <iostream>

using Microsoft::WRL::ComPtr;

namespace {

std::atomic_uint32_t g_enter_events{0};
std::atomic_uint32_t g_result_events{0};
std::atomic_uint32_t g_translated_results{0};
std::atomic_uint32_t g_destroy_events{0};

void ObserveDestroy(void*, std::uintptr_t resource,
                    cojvr::games::call_of_juarez::LegacyTextureKind,
                    ULONG refcount) noexcept {
    if (resource != 0 && refcount == 0) {
        g_destroy_events.fetch_add(1, std::memory_order_relaxed);
    }
}

void ObserveEvent(
    void*,
    const cojvr::games::call_of_juarez::LegacyTextureCreateEvent& event) noexcept {
    using Phase = cojvr::games::call_of_juarez::LegacyTextureCreatePhase;
    if (event.phase == Phase::enter) {
        g_enter_events.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_result_events.fetch_add(1, std::memory_order_relaxed);
        if (event.translated && SUCCEEDED(event.result) &&
            event.requested_pool == D3DPOOL_MANAGED &&
            event.effective_pool == D3DPOOL_DEFAULT &&
            (event.effective_usage & D3DUSAGE_DYNAMIC) != 0) {
            g_translated_results.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

int Fail(const char* message, HRESULT hr = S_OK) {
    std::cerr << message;
    if (FAILED(hr)) {
        std::cerr << " (HRESULT 0x" << std::hex << static_cast<unsigned long>(hr) << ')';
    }
    std::cerr << '\n';
    return 1;
}

} // namespace

int main() {
    const auto create_ex = cojvr::backends::d3d9::SystemDirect3DCreate9Ex();
    if (!create_ex) return Fail("System d3d9.dll has no Direct3DCreate9Ex");

    ComPtr<IDirect3D9Ex> d3d9;
    HRESULT hr = create_ex(D3D_SDK_VERSION, &d3d9);
    if (FAILED(hr) || !d3d9) return Fail("Direct3DCreate9Ex failed", hr);

    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = GetDesktopWindow();
    present.BackBufferFormat = D3DFMT_UNKNOWN;
    present.BackBufferWidth = 64;
    present.BackBufferHeight = 64;

    ComPtr<IDirect3DDevice9Ex> device;
    hr = d3d9->CreateDeviceEx(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        present.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &present,
        nullptr,
        &device);
    if (FAILED(hr) || !device) {
        if (hr == D3DERR_NOTAVAILABLE || hr == D3DERR_DEVICELOST) {
            std::cout << "D3D9Ex legacy-resource compatibility test skipped: HAL device unavailable\n";
            return 77;
        }
        return Fail("D3D9Ex CreateDeviceEx failed", hr);
    }

    const cojvr::games::call_of_juarez::LegacyTextureCompatibilityCallbacks callbacks{
        .event = &ObserveEvent,
        .resource_destroy = &ObserveDestroy,
    };
    if (!cojvr::games::call_of_juarez::InstallD3D9ExLegacyTextureCompatibility(
            device.Get(), callbacks)) {
        return Fail("failed to install CoJ D3D9Ex legacy texture compatibility");
    }

    ComPtr<IDirect3DTexture9> texture;
    hr = device->CreateTexture(
        32, 32, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr);
    if (FAILED(hr) || !texture) return Fail("MANAGED 2D texture compatibility failed", hr);
    D3DLOCKED_RECT locked{};
    hr = texture->LockRect(0, &locked, nullptr, 0);
    if (FAILED(hr)) return Fail("translated 2D texture was not lockable", hr);
    static_cast<unsigned char*>(locked.pBits)[0] = 0x11;
    if (FAILED(texture->UnlockRect(0))) return Fail("translated 2D texture unlock failed");

    ComPtr<IDirect3DVolumeTexture9> volume;
    hr = device->CreateVolumeTexture(
        8, 8, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &volume, nullptr);
    if (FAILED(hr) || !volume) return Fail("MANAGED volume texture compatibility failed", hr);

    ComPtr<IDirect3DCubeTexture9> cube;
    hr = device->CreateCubeTexture(
        16, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &cube, nullptr);
    if (FAILED(hr) || !cube) return Fail("MANAGED cube texture compatibility failed", hr);

    D3DPRESENT_PARAMETERS reset_present = present;
    hr = device->Reset(&reset_present);
    if (FAILED(hr)) return Fail("Reset failed with translated legacy textures retained", hr);
    if (FAILED(device->SetTexture(0, texture.Get()))) {
        return Fail("translated legacy texture could not be rebound after Reset");
    }
    (void)device->SetTexture(0, nullptr);
    texture.Reset();
    volume.Reset();
    cube.Reset();

    if (g_enter_events.load(std::memory_order_relaxed) != 3 ||
        g_result_events.load(std::memory_order_relaxed) != 3 ||
        g_translated_results.load(std::memory_order_relaxed) != 3 ||
        g_destroy_events.load(std::memory_order_relaxed) != 3) {
        return Fail("compatibility telemetry did not describe all translated texture calls");
    }
    if (!cojvr::games::call_of_juarez::ReacquireD3D9ExLegacyTextureCompatibility(device.Get())) {
        return Fail("legacy texture compatibility hook did not remain reacquirable");
    }
    if (!cojvr::games::call_of_juarez::RestoreD3D9ExLegacyTextureCompatibility()) {
        return Fail("legacy texture compatibility hook did not restore cleanly");
    }

    std::cout << "CoJ D3D9Ex legacy MANAGED texture compatibility passed for 2D/volume/cube + Reset\n";
    return 0;
}
