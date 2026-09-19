#include "backends/d3d9/device_vtable_hook.hpp"
#include "backends/d3d9/system_d3d9.hpp"

#include <d3d9.h>
#include <windows.h>

#include <atomic>
#include <iostream>

namespace {

std::atomic_uint32_t g_end_scene_callbacks{0};
std::atomic_uint32_t g_begin_scene_callbacks{0};
std::atomic_uint32_t g_present_callbacks{0};
std::atomic_uint32_t g_present_callback_returns{0};

void BeforePresent(IDirect3DDevice9* device) noexcept {
    if (device) {
        g_present_callbacks.fetch_add(1, std::memory_order_relaxed);
    }
}

void AfterPresent(IDirect3DDevice9* device, HRESULT result) noexcept {
    if (device && SUCCEEDED(result)) {
        g_present_callback_returns.fetch_add(1, std::memory_order_relaxed);
    }
}

void AfterBeginScene(IDirect3DDevice9* device, HRESULT result) noexcept {
    if (device && SUCCEEDED(result)) {
        g_begin_scene_callbacks.fetch_add(1, std::memory_order_relaxed);
    }
}

void AfterEndScene(IDirect3DDevice9* device, HRESULT result) noexcept {
    if (device && SUCCEEDED(result)) {
        g_end_scene_callbacks.fetch_add(1, std::memory_order_relaxed);
    }
}

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

bool RestorePresentSlotExternally() {
    const auto diagnostics = cojvr::backends::d3d9::InspectAllDeviceVtableHooks();
    for (const auto& slot : diagnostics) {
        if (slot.slot_name != "Present" || !slot.vtable || !slot.original || !slot.replacement) {
            continue;
        }
        DWORD old_protection = 0;
        if (!VirtualProtect(
                &slot.vtable[slot.index], sizeof(void*), PAGE_READWRITE,
                &old_protection)) {
            return false;
        }
        void* observed = InterlockedCompareExchangePointer(
            reinterpret_cast<PVOID volatile*>(&slot.vtable[slot.index]),
            slot.original, slot.replacement);
        DWORD ignored = 0;
        const bool protection_restored = VirtualProtect(
            &slot.vtable[slot.index], sizeof(void*), old_protection, &ignored) != FALSE;
        return observed == slot.replacement && protection_restored;
    }
    return false;
}

} // namespace

int main() {
    HWND window = CreateWindowExW(
        0, L"STATIC", L"cojvr d3d9 device hook test",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return Fail("failed to create D3D9 test window");

    const auto create_d3d9 = cojvr::backends::d3d9::SystemDirect3DCreate9();
    if (!create_d3d9 || !cojvr::backends::d3d9::IsExpectedSystemD3D9Module()) {
        DestroyWindow(window);
        return Fail("native hook test did not load the expected system d3d9.dll");
    }
    IDirect3D9* d3d9 = create_d3d9(D3D_SDK_VERSION);
    if (!d3d9) {
        DestroyWindow(window);
        return Fail("Direct3DCreate9 failed");
    }

    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = window;
    present.BackBufferWidth = 304;
    present.BackBufferHeight = 201;
    present.BackBufferFormat = D3DFMT_UNKNOWN;
    present.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    IDirect3DDevice9* device = nullptr;
    const HRESULT create_result = d3d9->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &present,
        &device);
    if (FAILED(create_result) || !device) {
        d3d9->Release();
        DestroyWindow(window);
        if (create_result == D3DERR_NOTAVAILABLE) {
            std::cout << "d3d9 device hook test skipped: HAL device unavailable\n";
            return 77;
        }
        return Fail("D3D9 device creation failed unexpectedly");
    }

    const cojvr::backends::d3d9::DeviceHookCallbacks callbacks{
        .before_present = BeforePresent,
        .after_present = AfterPresent,
        .after_begin_scene = AfterBeginScene,
        .after_end_scene = AfterEndScene,
    };
    if (!cojvr::backends::d3d9::InstallDeviceVtableHook(device, callbacks)) {
        device->Release();
        d3d9->Release();
        DestroyWindow(window);
        return Fail("failed to install D3D9 device hook");
    }
    for (const auto& slot : cojvr::backends::d3d9::InspectAllDeviceVtableHooks()) {
        if (slot.slot_name == "Reset") {
            device->Release();
            d3d9->Release();
            DestroyWindow(window);
            return Fail("present-only callback registration patched the unrelated Reset slot");
        }
    }

    const auto installed_status = cojvr::backends::d3d9::InspectDeviceVtableHook(device);
    if (!installed_status.installed || !installed_status.device_uses_hooked_vtable) {
        device->Release();
        d3d9->Release();
        DestroyWindow(window);
        return Fail("D3D9 device hook status did not report the hooked vtable");
    }

    constexpr std::uint32_t kSceneCycles = 5;
    HRESULT end_result = S_OK;
    HRESULT present_result = S_OK;
    for (std::uint32_t cycle = 0; cycle < kSceneCycles; ++cycle) {
        const HRESULT begin_result = device->BeginScene();
        if (FAILED(begin_result)) {
            device->Release();
            d3d9->Release();
            DestroyWindow(window);
            return Fail("D3D9 BeginScene failed");
        }

        end_result = device->EndScene();
        if (FAILED(end_result)) break;

        present_result = device->Present(nullptr, nullptr, nullptr, nullptr);
        if (FAILED(present_result)) break;
    }
    const auto callbacks_seen = g_end_scene_callbacks.load(std::memory_order_relaxed);
    const auto begin_callbacks_seen = g_begin_scene_callbacks.load(std::memory_order_relaxed);
    const auto present_callbacks_seen = g_present_callbacks.load(std::memory_order_relaxed);
    const auto present_callback_returns_seen =
        g_present_callback_returns.load(std::memory_order_relaxed);
    const auto final_status = cojvr::backends::d3d9::InspectDeviceVtableHook(device);
    const auto continuity = cojvr::backends::d3d9::InspectInstalledDeviceVtableHook();

    if (!RestorePresentSlotExternally()) {
        device->Release();
        d3d9->Release();
        DestroyWindow(window);
        return Fail("test could not reproduce external restoration of device Present");
    }
    const auto callbacks_before_unhooked_present =
        g_present_callbacks.load(std::memory_order_relaxed);
    present_result = device->Present(nullptr, nullptr, nullptr, nullptr);
    if (FAILED(present_result) ||
        g_present_callbacks.load(std::memory_order_relaxed) !=
            callbacks_before_unhooked_present) {
        device->Release();
        d3d9->Release();
        DestroyWindow(window);
        return Fail("externally restored device Present still traversed our callback");
    }

    const auto reacquired =
        cojvr::backends::d3d9::ReacquireDeviceVtableHookDetailed(device);
    present_result = device->Present(nullptr, nullptr, nullptr, nullptr);
    const auto callbacks_after_reacquire =
        g_present_callbacks.load(std::memory_order_relaxed);
    const bool restored = cojvr::backends::d3d9::RestoreAllDeviceVtableHooks();

    device->Release();
    d3d9->Release();
    DestroyWindow(window);

    if (FAILED(end_result)) return Fail("D3D9 EndScene failed");
    if (FAILED(present_result)) return Fail("D3D9 Present failed");
    if (present_callbacks_seen != kSceneCycles) {
        return Fail("D3D9 Present did not traverse the installed entry callback for every scene cycle");
    }
    if (present_callback_returns_seen != kSceneCycles) {
        return Fail("D3D9 Present did not traverse the installed return callback for every scene cycle");
    }
    if (!final_status.device_uses_hooked_vtable) {
        return Fail("D3D9 device changed vtable during the hook test");
    }
    if (!continuity.installed || !continuity.reset_active || !continuity.present_active ||
        !continuity.begin_scene_active || !continuity.end_scene_active) {
        return Fail("D3D9 device hook entries did not remain installed during the hook test");
    }
    if (reacquired.result != cojvr::backends::d3d9::HookRegistryResult::Installed ||
        reacquired.modified_slots != 1 ||
        callbacks_after_reacquire != callbacks_before_unhooked_present + 1) {
        return Fail("D3D9 device Present hook was not reacquired after exact-original restoration");
    }
    if (!restored) {
        return Fail("reacquired D3D9 device hook did not restore cleanly");
    }
    if (begin_callbacks_seen != kSceneCycles) {
        return Fail("D3D9 BeginScene did not traverse the installed callback for every scene cycle");
    }
    if (callbacks_seen != kSceneCycles) {
        return Fail("D3D9 EndScene did not traverse the installed callback for every scene cycle");
    }

    std::cout << "d3d9 Present/BeginScene/EndScene device hooks passed for " << kSceneCycles << " cycles\n";
    return 0;
}
