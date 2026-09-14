#include "backends/d3d9/classic_readback_bridge.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "backends/d3d9/device_vtable_hook.hpp"
#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
#include "backends/d3d9/openvr_flat_bridge.hpp"
#endif

#include "runtime/game_id.hpp"
#include "runtime/host_identity.hpp"
#include "runtime/log.hpp"


#include <atomic>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>

namespace {

HMODULE g_proxy_module = nullptr;
std::once_flag g_identity_log_once;
std::once_flag g_present_log_once;
std::once_flag g_readback_capture_once;
std::atomic_bool g_readback_requested{false};
#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
std::once_flag g_openvr_flat_success_log_once;
std::once_flag g_openvr_flat_failure_log_once;
std::once_flag g_begin_scene_log_once;
std::once_flag g_end_scene_log_once;
std::atomic_uint64_t g_present_callbacks{0};
std::atomic_uint64_t g_present_callback_returns{0};
std::atomic_uint64_t g_begin_scene_callbacks{0};
std::atomic_uint64_t g_end_scene_callbacks{0};
std::atomic_uint64_t g_end_scene_callback_returns{0};
std::atomic_bool g_device_hook_observer_started{false};
#endif

bool D3D9ExBridgeRequested() noexcept {
    wchar_t value[16]{};
    const DWORD length = GetEnvironmentVariableW(
        L"COJVR_D3D9_EX_BRIDGE", value, static_cast<DWORD>(std::size(value)));
    if (length > 0 && length < std::size(value)) {
        const std::wstring_view text(value, length);
        if (text == L"1" || text == L"true" || text == L"TRUE" || text == L"on" || text == L"ON") {
            return true;
        }
    }

    try {
        wchar_t module_path[MAX_PATH]{};
        const DWORD module_length = GetModuleFileNameW(g_proxy_module, module_path, MAX_PATH);
        if (module_length == 0 || module_length >= MAX_PATH) return false;
        const std::filesystem::path path(std::wstring_view(module_path, module_length));
        return std::filesystem::exists(path.parent_path() / L".cojvr-d3d9-ex-bridge");
    } catch (...) {
        return false;
    }
}

bool ClassicReadbackRequested() noexcept {
#if defined(COJVR_D3D9_READBACK_ALWAYS_ON)
    return true;
#else
    return false;
#endif
}

#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
cojvr::backends::d3d9::OpenVrFlatBridge& OpenVrFlatBridgeInstance() {
    // Process-lifetime diagnostic object. Avoid OpenVR/COM teardown from DllMain.
    static auto* bridge = new cojvr::backends::d3d9::OpenVrFlatBridge();
    return *bridge;
}
#endif

std::filesystem::path LogPath() noexcept {
    try {
        wchar_t buffer[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(g_proxy_module, buffer, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return L"cojvr.log";
        std::filesystem::path path(std::wstring_view(buffer, length));
        return path.parent_path() / L"cojvr.log";
    } catch (...) {
        return L"cojvr.log";
    }
}

void LogLine(std::string_view line) noexcept {
    cojvr::runtime::AppendLogLine(LogPath(), line);
}

#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
std::string ModuleNameForAddress(void* address) noexcept {
    if (!address) return "null";
    try {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(address), &module) || !module) {
            return "unknown";
        }
        wchar_t module_path[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(module, module_path, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return "unknown";
        return std::filesystem::path(std::wstring_view(module_path, length)).filename().string();
    } catch (...) {
        return "unknown";
    }
}

void AppendHookTarget(
    std::ostringstream& out, std::string_view name, bool active, void* target) noexcept {
    out << ' ' << name << '=' << (active ? "active" : "overwritten");
    if (!active) {
        out << '(' << ModuleNameForAddress(target) << "@0x" << std::hex
            << reinterpret_cast<std::uintptr_t>(target) << std::dec << ')';
    }
}

DWORD WINAPI ObserveDeviceHookContinuity(LPVOID) noexcept {
    constexpr DWORD kPollMilliseconds = 25;
    constexpr unsigned kPollCount = 400;
    for (unsigned poll = 0; poll < kPollCount; ++poll) {
        Sleep(kPollMilliseconds);
        const auto continuity = cojvr::backends::d3d9::InspectInstalledDeviceVtableHook();
        if (!continuity.installed || !continuity.reset_active || !continuity.present_active ||
            !continuity.begin_scene_active || !continuity.end_scene_active) {
            std::ostringstream out;
            out << "d3d9 device hook continuity: overwritten"
                << " present_callbacks=" << g_present_callbacks.load(std::memory_order_relaxed)
                << " begin_scene_callbacks=" << g_begin_scene_callbacks.load(std::memory_order_relaxed)
                << " end_scene_callbacks=" << g_end_scene_callbacks.load(std::memory_order_relaxed);
            AppendHookTarget(out, "reset", continuity.reset_active, continuity.reset_target);
            AppendHookTarget(out, "present", continuity.present_active, continuity.present_target);
            AppendHookTarget(
                out, "begin_scene", continuity.begin_scene_active, continuity.begin_scene_target);
            AppendHookTarget(out, "end_scene", continuity.end_scene_active, continuity.end_scene_target);
            LogLine(out.str());
            return 0;
        }
    }

    std::ostringstream out;
    out << "d3d9 device hook continuity: stable_10s"
        << " present_callbacks=" << g_present_callbacks.load(std::memory_order_relaxed)
        << " begin_scene_callbacks=" << g_begin_scene_callbacks.load(std::memory_order_relaxed)
        << " end_scene_callbacks=" << g_end_scene_callbacks.load(std::memory_order_relaxed);
    LogLine(out.str());
    return 0;
}

void StartDeviceHookObserver() noexcept {
    if (g_device_hook_observer_started.exchange(true, std::memory_order_relaxed)) return;

    HANDLE thread = CreateThread(nullptr, 0, ObserveDeviceHookContinuity, nullptr, 0, nullptr);
    if (!thread) {
        LogLine("d3d9 device hook continuity: observer_start_failed");
        return;
    }
    CloseHandle(thread);
    LogLine("d3d9 device hook continuity: observer_started");
}
#endif

void LogHostIdentityOnce() noexcept {
    try {
        std::call_once(g_identity_log_once, [] {
            const auto identity = cojvr::runtime::InspectCurrentHost();
            if (!identity) {
                LogLine("d3d9 bootstrap: host identity unavailable");
                return;
            }

            std::ostringstream out;
            out << "d3d9 bootstrap: host="
                << cojvr::runtime::GameIdName(identity->filename_game)
                << " sha256=" << identity->sha256
                << " exact_build=" << (identity->IsSupported() ? "known" : "unknown");
            LogLine(out.str());
        });
    } catch (...) {
    }
}

void BeforePresent(IDirect3DDevice9* device) noexcept {
    try {
        std::call_once(g_present_log_once, [] {
            LogLine("d3d9 Present: frame boundary observed");
        });

#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
        const std::uint64_t callbacks =
            g_present_callbacks.fetch_add(1, std::memory_order_relaxed) + 1;
        if ((callbacks >= 2 && callbacks <= 10) || callbacks == 60 || callbacks == 300 ||
            (callbacks > 300 && callbacks % 1800 == 0)) {
            LogLine("d3d9 Present: callback_count=" + std::to_string(callbacks));
        }
#endif

        if (g_readback_requested.load(std::memory_order_relaxed)) {
            std::call_once(g_readback_capture_once, [device] {
                std::string diagnostic;
                const bool success =
                    cojvr::backends::d3d9::CaptureBackBufferToD3D11(device, diagnostic);
                LogLine(std::string("d3d9 classic readback -> D3D11: ") +
                    (success ? "success " : "failed ") + diagnostic);
            });
        }


    } catch (...) {
    }
}

#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
bool IsDiagnosticMilestone(const std::uint64_t count) noexcept {
    return (count >= 2 && count <= 10) || count == 60 || count == 300 ||
        (count > 300 && count % 1800 == 0);
}

void LogOpenVrFlatPhase(const cojvr::backends::d3d9::OpenVrFlatBridgePhase phase) noexcept {
    try {
        const std::uint64_t callback = g_end_scene_callbacks.load(std::memory_order_relaxed);
        if (!IsDiagnosticMilestone(callback)) return;

        const char* name = "unknown";
        using Phase = cojvr::backends::d3d9::OpenVrFlatBridgePhase;
        switch (phase) {
        case Phase::BeforeReadback: name = "before_readback"; break;
        case Phase::AfterReadback: name = "after_readback"; break;
        case Phase::BeforeUpload: name = "before_upload"; break;
        case Phase::AfterUpload: name = "after_upload"; break;
        case Phase::BeforeWaitForHmdPose: name = "before_wait_for_hmd_pose"; break;
        case Phase::AfterWaitForHmdPose: name = "after_wait_for_hmd_pose"; break;
        case Phase::BeforeSubmitStereo: name = "before_submit_stereo"; break;
        case Phase::AfterSubmitStereo: name = "after_submit_stereo"; break;
        }
        LogLine(
            "d3d9 OpenVR flat bridge: callback=" + std::to_string(callback) +
            " phase=" + name);
    } catch (...) {
    }
}

#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
void AfterPresent(IDirect3DDevice9*, HRESULT) noexcept {
    try {
        const std::uint64_t returns =
            g_present_callback_returns.fetch_add(1, std::memory_order_relaxed) + 1;
        if ((returns >= 2 && returns <= 10) || returns == 60 || returns == 300 ||
            (returns > 300 && returns % 1800 == 0)) {
            LogLine("d3d9 Present: callback_return_count=" + std::to_string(returns));
        }
    } catch (...) {
    }
}
#endif

void AfterBeginScene(IDirect3DDevice9*, HRESULT result) noexcept {
    try {
        if (FAILED(result)) return;
        const std::uint64_t callbacks =
            g_begin_scene_callbacks.fetch_add(1, std::memory_order_relaxed) + 1;
        std::call_once(g_begin_scene_log_once, [] {
            LogLine("d3d9 BeginScene: frame boundary observed");
        });
        if (IsDiagnosticMilestone(callbacks)) {
            LogLine("d3d9 BeginScene: callback_count=" + std::to_string(callbacks));
        }
    } catch (...) {
    }
}

void AfterEndScene(IDirect3DDevice9* device, HRESULT result) noexcept {
    try {
        if (FAILED(result)) return;
        const std::uint64_t callbacks =
            g_end_scene_callbacks.fetch_add(1, std::memory_order_relaxed) + 1;
        std::call_once(g_end_scene_log_once, [] {
            LogLine("d3d9 EndScene: frame boundary observed");
        });
        if (IsDiagnosticMilestone(callbacks)) {
            LogLine("d3d9 EndScene: callback_count=" + std::to_string(callbacks));
        }

        auto& bridge = OpenVrFlatBridgeInstance();
        if (bridge.CaptureAndSubmit(device, &LogOpenVrFlatPhase)) {
            const std::uint64_t submitted = bridge.submitted_frames();
            std::call_once(g_openvr_flat_success_log_once, [&bridge] {
                LogLine(std::string("d3d9 OpenVR flat bridge: first stereo submission success ") +
                    std::string(bridge.description()));
            });
            if (IsDiagnosticMilestone(submitted)) {
                LogLine("d3d9 OpenVR flat bridge: submitted_frames=" + std::to_string(submitted));
            }
        } else {
            std::call_once(g_openvr_flat_failure_log_once, [&bridge] {
                LogLine(std::string("d3d9 OpenVR flat bridge: failure ") +
                    std::string(bridge.last_error()));
            });
        }
    } catch (...) {
    }

    try {
        const std::uint64_t returns =
            g_end_scene_callback_returns.fetch_add(1, std::memory_order_relaxed) + 1;
        if (IsDiagnosticMilestone(returns)) {
            LogLine("d3d9 EndScene: callback_return_count=" + std::to_string(returns));
        }
    } catch (...) {
    }
}
#endif

void BeforeReset(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*) noexcept {
#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
    try {
        OpenVrFlatBridgeInstance().BeforeD3D9Reset();
    } catch (...) {
    }
#endif
}

void AfterReset(
    IDirect3DDevice9*, D3DPRESENT_PARAMETERS* parameters, HRESULT result) noexcept {
    try {
        std::ostringstream out;
        out << "d3d9 Reset: hr=0x" << std::hex << static_cast<unsigned long>(result);
        if (parameters) {
            out << std::dec << " backbuffer=" << parameters->BackBufferWidth
                << 'x' << parameters->BackBufferHeight;
        }
        LogLine(out.str());
    } catch (...) {
    }
}

class Direct3D9Forwarder final : public IDirect3D9 {
public:
    explicit Direct3D9Forwarder(IDirect3D9* inner, IDirect3D9Ex* inner_ex = nullptr) noexcept
        : inner_(inner), inner_ex_(inner_ex) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDirect3D9) {
            *object = static_cast<IDirect3D9*>(this);
            AddRef();
            return S_OK;
        }
        return inner_->QueryInterface(riid, object);
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references_;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --references_;
        if (remaining == 0) {
            inner_->Release();
            delete this;
        }
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void* initialize_function) override {
        return inner_->RegisterSoftwareDevice(initialize_function);
    }

    UINT STDMETHODCALLTYPE GetAdapterCount() override {
        return inner_->GetAdapterCount();
    }

    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(
        UINT adapter, DWORD flags, D3DADAPTER_IDENTIFIER9* identifier) override {
        return inner_->GetAdapterIdentifier(adapter, flags, identifier);
    }

    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT adapter, D3DFORMAT format) override {
        return inner_->GetAdapterModeCount(adapter, format);
    }

    HRESULT STDMETHODCALLTYPE EnumAdapterModes(
        UINT adapter, D3DFORMAT format, UINT mode, D3DDISPLAYMODE* display_mode) override {
        return inner_->EnumAdapterModes(adapter, format, mode, display_mode);
    }

    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT adapter, D3DDISPLAYMODE* mode) override {
        return inner_->GetAdapterDisplayMode(adapter, mode);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceType(
        UINT adapter, D3DDEVTYPE device_type, D3DFORMAT adapter_format,
        D3DFORMAT back_buffer_format, BOOL windowed) override {
        return inner_->CheckDeviceType(
            adapter, device_type, adapter_format, back_buffer_format, windowed);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(
        UINT adapter, D3DDEVTYPE device_type, D3DFORMAT adapter_format, DWORD usage,
        D3DRESOURCETYPE resource_type, D3DFORMAT check_format) override {
        return inner_->CheckDeviceFormat(
            adapter, device_type, adapter_format, usage, resource_type, check_format);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(
        UINT adapter, D3DDEVTYPE device_type, D3DFORMAT surface_format, BOOL windowed,
        D3DMULTISAMPLE_TYPE multi_sample_type, DWORD* quality_levels) override {
        return inner_->CheckDeviceMultiSampleType(
            adapter, device_type, surface_format, windowed, multi_sample_type, quality_levels);
    }

    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(
        UINT adapter, D3DDEVTYPE device_type, D3DFORMAT adapter_format,
        D3DFORMAT render_target_format, D3DFORMAT depth_stencil_format) override {
        return inner_->CheckDepthStencilMatch(
            adapter, device_type, adapter_format, render_target_format, depth_stencil_format);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceFormatConversion(
        UINT adapter, D3DDEVTYPE device_type, D3DFORMAT source_format,
        D3DFORMAT target_format) override {
        return inner_->CheckDeviceFormatConversion(
            adapter, device_type, source_format, target_format);
    }

    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT adapter, D3DDEVTYPE device_type, D3DCAPS9* caps) override {
        return inner_->GetDeviceCaps(adapter, device_type, caps);
    }

    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT adapter) override {
        return inner_->GetAdapterMonitor(adapter);
    }

    HRESULT STDMETHODCALLTYPE CreateDevice(
        UINT adapter, D3DDEVTYPE device_type, HWND focus_window, DWORD behavior_flags,
        D3DPRESENT_PARAMETERS* presentation_parameters, IDirect3DDevice9** returned_device) override {
        const HRESULT result = CreateDeviceInner(
            adapter, device_type, focus_window, behavior_flags, presentation_parameters, returned_device);

        if (SUCCEEDED(result) && returned_device && *returned_device) {
            const cojvr::backends::d3d9::DeviceHookCallbacks callbacks{
                .before_present = BeforePresent,
#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
                .after_present = AfterPresent,
                .after_begin_scene = AfterBeginScene,
                .after_end_scene = AfterEndScene,
#else
                .after_present = nullptr,
                .after_begin_scene = nullptr,
                .after_end_scene = nullptr,
#endif
                .before_reset = BeforeReset,
                .after_reset = AfterReset,
            };
            const bool hooks_installed =
                cojvr::backends::d3d9::InstallDeviceVtableHook(*returned_device, callbacks);
            LogLine(hooks_installed
                        ? "d3d9 device hooks: Present/Reset active"
                        : "d3d9 device hooks: installation failed");
#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
            if (hooks_installed) {
                LogLine("d3d9 device hook: EndScene active");
                StartDeviceHookObserver();
            }
#endif
        }

        try {
            std::ostringstream out;
            out << "d3d9 CreateDevice: adapter=" << adapter
                << " type=" << static_cast<unsigned>(device_type)
                << " flags=0x" << std::hex << behavior_flags
                << " hr=0x" << static_cast<unsigned long>(result);
            if (presentation_parameters) {
                out << std::dec
                    << " windowed=" << (presentation_parameters->Windowed ? 1 : 0)
                    << " backbuffer=" << presentation_parameters->BackBufferWidth
                    << 'x' << presentation_parameters->BackBufferHeight;
            }
            LogLine(out.str());
        } catch (...) {
        }

        return result;
    }

private:
    HRESULT CreateDeviceInner(
        UINT adapter, D3DDEVTYPE device_type, HWND focus_window, DWORD behavior_flags,
        D3DPRESENT_PARAMETERS* presentation_parameters, IDirect3DDevice9** returned_device) noexcept {
        if (inner_ex_ == nullptr) {
            return inner_->CreateDevice(
                adapter, device_type, focus_window, behavior_flags,
                presentation_parameters, returned_device);
        }
        if (presentation_parameters == nullptr || returned_device == nullptr) {
            return D3DERR_INVALIDCALL;
        }

        *returned_device = nullptr;
        D3DDISPLAYMODEEX fullscreen_mode{};
        D3DDISPLAYMODEEX* fullscreen_mode_ptr = nullptr;
        if (!presentation_parameters->Windowed) {
            fullscreen_mode.Size = sizeof(fullscreen_mode);
            fullscreen_mode.Width = presentation_parameters->BackBufferWidth;
            fullscreen_mode.Height = presentation_parameters->BackBufferHeight;
            fullscreen_mode.RefreshRate = presentation_parameters->FullScreen_RefreshRateInHz;
            fullscreen_mode.Format = presentation_parameters->BackBufferFormat;
            fullscreen_mode.ScanLineOrdering = D3DSCANLINEORDERING_UNKNOWN;
            fullscreen_mode_ptr = &fullscreen_mode;
        }

        IDirect3DDevice9Ex* ex_device = nullptr;
        const HRESULT result = inner_ex_->CreateDeviceEx(
            adapter, device_type, focus_window, behavior_flags,
            presentation_parameters, fullscreen_mode_ptr, &ex_device);
        if (SUCCEEDED(result) && ex_device != nullptr) {
            *returned_device = static_cast<IDirect3DDevice9*>(ex_device);
        } else if (ex_device != nullptr) {
            ex_device->Release();
        }
        return result;
    }

    std::atomic<ULONG> references_{1};
    IDirect3D9* inner_ = nullptr;
    IDirect3D9Ex* inner_ex_ = nullptr;
};

} // namespace

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT sdk_version) {
    LogHostIdentityOnce();
    g_readback_requested.store(ClassicReadbackRequested(), std::memory_order_relaxed);

    if (D3D9ExBridgeRequested()) {
        const auto create_ex = cojvr::backends::d3d9::SystemDirect3DCreate9Ex();
        if (!create_ex) {
            LogLine("d3d9 D3D9Ex bridge: system entry point unavailable");
            return nullptr;
        }

        IDirect3D9Ex* real_d3d_ex = nullptr;
        const HRESULT result = create_ex(sdk_version, &real_d3d_ex);
        if (FAILED(result) || real_d3d_ex == nullptr) {
            LogLine("d3d9 D3D9Ex bridge: system creation failed");
            return nullptr;
        }

        try {
            auto* forwarder = new Direct3D9Forwarder(real_d3d_ex, real_d3d_ex);
            LogLine("d3d9 D3D9Ex bridge: forwarding wrapper active");
            return forwarder;
        } catch (...) {
            LogLine("d3d9 D3D9Ex bridge: wrapper allocation failed");
            return static_cast<IDirect3D9*>(real_d3d_ex);
        }
    }

    const auto create = cojvr::backends::d3d9::SystemDirect3DCreate9();
    if (!create) {
        LogLine("d3d9 Direct3DCreate9: system entry point unavailable");
        return nullptr;
    }

    IDirect3D9* real_d3d = create(sdk_version);
    if (!real_d3d) {
        LogLine("d3d9 Direct3DCreate9: system call returned null");
        return nullptr;
    }

    try {
        auto* forwarder = new Direct3D9Forwarder(real_d3d);
        LogLine("d3d9 Direct3DCreate9: forwarding wrapper active");
        return forwarder;
    } catch (...) {
        LogLine("d3d9 Direct3DCreate9: wrapper allocation failed; using system interface");
        return real_d3d;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_proxy_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
