#include "games/call_of_juarez/camera_probe.hpp"
#include "games/call_of_juarez/d3d9ex_legacy_resource_compat.hpp"
#include "games/call_of_juarez/game_shutdown_hook.hpp"
#include "games/call_of_juarez/java_player_bridge.hpp"

#include "backends/d3d9/d3d9_stereo_capture.hpp"
#include "backends/d3d9/device_vtable_hook.hpp"
#include "backends/d3d9/factory_vtable_hook.hpp"
#include "backends/d3d9/swapchain_vtable_hook.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "backends/openvr/stereo_presenter.hpp"
#include "runtime/log.hpp"
#include "runtime/hmd_frame_pacing.hpp"
#include "runtime/ui_pointer_ownership.hpp"
#include "backends/d3d9/vr_present_policy.hpp"

#include <windows.h>
#include <psapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

std::once_flag g_start_once;
std::filesystem::path g_game_directory{};
std::string g_run_id{"unbound"};
std::atomic_bool g_finalization_started{false};
std::atomic_bool g_finalization_completed{false};
wchar_t g_process_exit_log_path[32768]{};

struct NativeStereoState {
    cojvr::backends::d3d9::D3D9StereoCapture capture;
    cojvr::backends::d3d9::D3D9StereoCapture flat_capture;
    cojvr::backends::openvr::OpenVrStereoPresenter presenter;
    cojvr::runtime::HmdFramePacer frame_pacer;
    cojvr::runtime::VrFrameCadence frame_cadence;
    std::atomic_bool desktop_vsync_disabled{false};
    cojvr::backends::d3d9::VrPresentPolicy present_policy;
    bool late_rate_reported = false;
    float last_pacing_hz = 0.0F;
    std::uint64_t pacing_sequence = 0;
    std::array<cojvr::runtime::EyeView, 2> eyes{};
    std::mutex device_mutex;
    IDirect3D9* factory = nullptr;
    IDirect3DDevice9* device = nullptr;
    std::atomic_uint64_t device_generation{0};
    std::atomic<DWORD> native_render_thread{0};
    std::atomic_uint64_t present_telemetry_sequence{0};
    std::atomic_uint64_t capture_resources_generation_logged{0};
    std::atomic_uint64_t transport_sequence{0};
    std::atomic_uint64_t flat_capture_sequence{0};
    std::atomic_uint64_t last_native_stereo_tick_ms{0};
    std::atomic_bool flat_capture_active{false};
    std::atomic_uint64_t last_error_frame{0};
    std::atomic_uintptr_t game_window{0};
    std::mutex present_hook_worker_mutex;
    std::jthread present_hook_worker;
    std::atomic_bool present_hook_conflict_logged{false};
    std::mutex flat_ui_mutex;
    bool flat_ui_pointer_active = false;
    cojvr::runtime::UiPointerSelection flat_ui_pointer_selection;
    std::uint32_t flat_ui_pointer_x = 0;
    std::uint32_t flat_ui_pointer_y = 0;
    bool flat_ui_pointer_game_route_logged = false;
    bool flat_ui_pointer_game_error_logged = false;
    std::uint64_t flat_ui_pointer_delivery_sequence = 0;
    cojvr::runtime::UiPointerMousePriority flat_ui_mouse_priority;
    std::atomic_uint64_t flat_ui_mouse_override_until_ms{0};
    cojvr::runtime::UiPointerSelectionSnapshot flat_ui_applied_selection{};
    std::uint32_t flat_ui_applied_x = 0, flat_ui_applied_y = 0;
    bool flat_ui_target_applied = false;
    std::int32_t flat_ui_menu_index = 0;
    bool flat_ui_menu_known = false;
    bool flat_loading_resume_pending = false;
    bool flat_ui_select_release_required = false;
    cojvr::games::call_of_juarez::CoJUiSelectRetryState flat_ui_select_retry{};
    std::array<bool, 2> capture_source_logged{};
    std::atomic_bool capture_readback_enabled{true};
    bool runtime_ready = false;
};

// Finalize explicitly drains owners before DestroyGame. If abnormal exit bypasses
// that boundary, Windows owns the remaining allocations; DLL static destruction
// must not join killed threads or release GPU resources under the loader lock.
NativeStereoState& g_stereo = *new NativeStereoState;

std::filesystem::path GameDirectory() noexcept {
    try {
        if (!g_game_directory.empty()) return g_game_directory;
        std::vector<wchar_t> buffer(32768);
        const DWORD length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size()) return {};
        g_game_directory = std::filesystem::path(
            std::wstring_view(buffer.data(), length)).parent_path();
        return g_game_directory;
    } catch (...) {
        return {};
    }
}

std::filesystem::path LogPath() noexcept {
    const auto directory = GameDirectory();
    return directory.empty() ? std::filesystem::path{} : directory / L"cojvr.log";
}

void LogLine(const std::string_view line) noexcept {
    const auto path = LogPath();
    if (!path.empty()) cojvr::runtime::AppendLogLine(path, line);
}

std::uintptr_t ModuleRva(const std::uintptr_t address) noexcept {
    if (address == 0) return 0;
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address), &module) ||
        !module) {
        return 0;
    }
    return address - reinterpret_cast<std::uintptr_t>(module);
}

const char* LegacyTextureKindName(
    const cojvr::games::call_of_juarez::LegacyTextureKind kind) noexcept {
    using Kind = cojvr::games::call_of_juarez::LegacyTextureKind;
    switch (kind) {
    case Kind::texture_2d: return "texture2d";
    case Kind::volume_texture: return "volume_texture";
    case Kind::cube_texture: return "cube_texture";
    }
    return "unknown";
}

// Sample only at bounded allocation boundaries. This reports address capacity
// separately from physical memory; it never frees, retries or changes a resource.
void LogAddressSpace(const std::uint64_t sequence, const bool allocation_failed) noexcept {
    try {
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        const std::uint64_t limit = reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress) + 1ULL;
        std::uint64_t free_bytes = 0, largest_free = 0, used_bytes = 0;
        std::uint64_t cursor = reinterpret_cast<std::uintptr_t>(info.lpMinimumApplicationAddress);
        bool complete = true;
        while (cursor < limit) {
            MEMORY_BASIC_INFORMATION region{};
            if (!VirtualQuery(reinterpret_cast<void*>(static_cast<std::uintptr_t>(cursor)), &region, sizeof(region)) ||
                region.RegionSize == 0) { complete = false; break; }
            const auto begin = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(region.BaseAddress));
            const auto end = std::min(limit, begin + region.RegionSize);
            if (end <= cursor) { complete = false; break; }
            const auto bytes = end - cursor;
            if (region.State == MEM_FREE) { free_bytes += bytes; largest_free = std::max(largest_free, bytes); }
            else used_bytes += bytes;
            cursor = end;
        }
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        const bool counters_valid = K32GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)) != FALSE;
        std::ostringstream line;
        line << "native_stereo_address_space: sequence=" << sequence
             << ";thread_id=" << GetCurrentThreadId() << ";allocation_failed=" << allocation_failed
             << ";map_complete=" << complete << ";user_address_limit=" << limit
             << ";used_va_bytes=" << used_bytes << ";free_va_bytes=" << free_bytes
             << ";largest_free_region_bytes=" << largest_free << ";counters_valid=" << counters_valid
             << ";private_commit_bytes=" << counters.PrivateUsage
             << ";peak_private_commit_bytes=" << counters.PeakPagefileUsage
             << ";working_set_bytes=" << counters.WorkingSetSize;
        LogLine(line.str());
    } catch (...) {}
}

void LegacyTextureEvent(
    void*,
    const cojvr::games::call_of_juarez::LegacyTextureCreateEvent& event) noexcept {
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=legacy_texture_create"
             << ";phase="
             << (event.phase ==
                         cojvr::games::call_of_juarez::LegacyTextureCreatePhase::enter
                     ? "enter"
                     : "result")
             << ";sequence=" << event.sequence
             << ";thread_id=" << event.thread_id
             << ";device=0x" << std::hex << event.device
             << ";caller=0x" << event.caller
             << ";caller_rva=0x" << ModuleRva(event.caller)
             << std::dec
             << ";kind=" << LegacyTextureKindName(event.kind)
             << ";width=" << event.width
             << ";height=" << event.height
             << ";depth=" << event.depth
             << ";levels=" << event.levels
             << ";requested_usage=0x" << std::hex << event.requested_usage
             << ";effective_usage=0x" << event.effective_usage
             << std::dec
             << ";format=" << static_cast<unsigned>(event.format)
             << ";requested_pool=" << static_cast<unsigned>(event.requested_pool)
             << ";effective_pool=" << static_cast<unsigned>(event.effective_pool)
             << ";translated=" << (event.translated ? "true" : "false");
        if (event.phase ==
            cojvr::games::call_of_juarez::LegacyTextureCreatePhase::result) {
            line << ";hr=0x" << std::hex << static_cast<unsigned long>(event.result)
                 << std::dec;
        }
        LogLine(line.str());
        if (event.phase == cojvr::games::call_of_juarez::LegacyTextureCreatePhase::result &&
            (FAILED(event.result) || event.sequence == 1 || event.sequence % 64 == 0)) {
            LogAddressSpace(event.sequence, FAILED(event.result));
        }
    } catch (...) {
    }
}

void ResourceDestroyEvent(
    void*, std::uintptr_t resource,
    cojvr::games::call_of_juarez::LegacyTextureKind kind,
    ULONG refcount) noexcept {
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=resource_destroy"
             << ";thread_id=" << GetCurrentThreadId()
             << ";object=0x" << std::hex << resource << std::dec
             << ";interface=IDirect3DBaseTexture9"
             << ";kind=" << LegacyTextureKindName(kind)
             << ";refcount=" << refcount
             << ";hr=0x0";
        LogLine(line.str());
    } catch (...) {
    }
}

const char* KnownInterface(const IID* iid) noexcept {
    if (!iid) return "none";
    if (*iid == IID_IUnknown) return "IUnknown";
    if (*iid == IID_IDirect3D9) return "IDirect3D9";
    if (*iid == IID_IDirect3D9Ex) return "IDirect3D9Ex";
    if (*iid == IID_IDirect3DDevice9) return "IDirect3DDevice9";
    if (*iid == IID_IDirect3DDevice9Ex) return "IDirect3DDevice9Ex";
    if (*iid == IID_IDirect3DSwapChain9) return "IDirect3DSwapChain9";
    if (*iid == IID_IDirect3DSwapChain9Ex) return "IDirect3DSwapChain9Ex";
    return "other";
}

void LogComTrace(
    const char* kind, void* object, const char* event, const IID* iid,
    void* returned, HRESULT result, ULONG refcount) noexcept {
    if (g_stereo.present_telemetry_sequence.load(std::memory_order_relaxed) > 8 &&
        !(std::string_view(event) == "release" && refcount == 0)) return;
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=" << event
             << ";kind=" << kind
             << ";thread_id=" << GetCurrentThreadId()
             << ";object=0x" << std::hex << reinterpret_cast<std::uintptr_t>(object)
             << ";returned=0x" << reinterpret_cast<std::uintptr_t>(returned)
             << ";hr=0x" << static_cast<unsigned long>(result)
             << std::dec << ";interface=" << KnownInterface(iid);
        if (iid && std::string_view(KnownInterface(iid)) == "other") {
            line << ";iid=" << std::hex << std::setfill('0')
                 << std::setw(8) << iid->Data1 << '-'
                 << std::setw(4) << iid->Data2 << '-'
                 << std::setw(4) << iid->Data3 << '-'
                 << std::setw(2) << static_cast<unsigned>(iid->Data4[0])
                 << std::setw(2) << static_cast<unsigned>(iid->Data4[1]) << '-';
            for (int i = 2; i < 8; ++i) {
                line << std::setw(2) << static_cast<unsigned>(iid->Data4[i]);
            }
            line << std::dec;
        }
        if (std::string_view(event) == "add_ref" ||
            std::string_view(event) == "release") {
            line << ";refcount=" << refcount;
        }
        LogLine(line.str());
        if (refcount == 0 && std::string_view(event) == "release") {
            LogLine(std::string("native_stereo_startup_event: event=") + kind +
                    "_destroy;thread_id=" + std::to_string(GetCurrentThreadId()));
        }
    } catch (...) {
    }
}

void FactoryComTrace(void* object, const char* event, const IID* iid,
                     void* returned, HRESULT result, ULONG refcount) noexcept {
    LogComTrace("factory", object, event, iid, returned, result, refcount);
}

void DeviceComTrace(void* object, const char* event, const IID* iid,
                    void* returned, HRESULT result, ULONG refcount) noexcept {
    LogComTrace("device", object, event, iid, returned, result, refcount);
}

void SwapChainComTrace(void* object, const char* event, const IID* iid,
                       void* returned, HRESULT result, ULONG refcount) noexcept {
    LogComTrace("swapchain", object, event, iid, returned, result, refcount);
}

void DeviceMethodTrace(
    IDirect3DDevice9* device, const char* method, bool entering, HRESULT result) noexcept {
    if (g_stereo.present_telemetry_sequence.load(std::memory_order_relaxed) > 8) return;
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=device_method_"
             << (entering ? "enter" : "exit")
             << ";thread_id=" << GetCurrentThreadId()
             << ";object=0x" << std::hex << reinterpret_cast<std::uintptr_t>(device)
             << ";hr=0x" << static_cast<unsigned long>(result)
             << std::dec << ";method=" << method;
        LogLine(line.str());
    } catch (...) {
    }
}

void SwapChainMethodTrace(
    IDirect3DSwapChain9* chain, const char* method, bool entering, HRESULT result) noexcept {
    if (g_stereo.present_telemetry_sequence.load(std::memory_order_relaxed) > 8) return;
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=swapchain_method_"
             << (entering ? "enter" : "exit")
             << ";thread_id=" << GetCurrentThreadId()
             << ";object=0x" << std::hex << reinterpret_cast<std::uintptr_t>(chain)
             << ";hr=0x" << static_cast<unsigned long>(result)
             << std::dec << ";method=" << method;
        LogLine(line.str());
    } catch (...) {
    }
}

void CameraEvent(const char* event, const char* result, const char* detail) noexcept;

void PresenterLog(void*, const std::string_view line) noexcept { LogLine(line); }

void NeutralizeFlatUiPointer(const char* reason) noexcept {
    try {
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        if (g_stereo.flat_ui_pointer_active) {
            std::ostringstream detail;
            detail << "active=false;reason=" << (reason ? reason : "unknown")
                   << ";route=game_cursor_mailbox";
            CameraEvent("flat_ui_pointer", "neutralized", detail.str().c_str());
        }
        g_stereo.flat_ui_pointer_active = false;
        g_stereo.flat_ui_pointer_selection.Observe(cojvr::runtime::UiPointerHand::none, 0, false, false);
        g_stereo.flat_ui_target_applied = false;
        g_stereo.flat_ui_pointer_x = 0;
        g_stereo.flat_ui_pointer_y = 0;
        g_stereo.flat_ui_pointer_game_route_logged = false;
        g_stereo.flat_ui_pointer_game_error_logged = false;
    } catch (...) {
    }
}

bool FlatUiPointer(
    void*,
    const cojvr::backends::openvr::FlatUiPointerSample& sample) noexcept {
    try {
        const HWND window = reinterpret_cast<HWND>(
            g_stereo.game_window.load(std::memory_order_acquire));
        HWND root = window ? GetAncestor(window, GA_ROOT) : nullptr;
        if (!root) root = window;
        const bool foreground = root && GetForegroundWindow() == root;
        if (!foreground) {
            NeutralizeFlatUiPointer("game_not_foreground");
            return false;
        }
        if (!sample.active) {
            NeutralizeFlatUiPointer("ray_inactive");
            return true;
        }
        if (GetTickCount64() < g_stereo.flat_ui_mouse_override_until_ms.load(std::memory_order_acquire)) {
            NeutralizeFlatUiPointer("physical_mouse_priority");
            return false;
        }
        if (sample.source_width == 0 || sample.source_height == 0 ||
            sample.pixel_x >= sample.source_width || sample.pixel_y >= sample.source_height ||
            sample.claim == 0) {
            NeutralizeFlatUiPointer("invalid_source");
            return false;
        }

        // Queue source-image coordinates only. Desktop absolute movement is
        // reinterpreted by Chrome's mouse stream and feeds back into its cursor.
        // The game thread applies the target through UICursor.SetPos instead.
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        if (!g_stereo.flat_ui_pointer_active) {
            std::ostringstream detail;
            detail << "active=true;hand=" << (sample.using_left_hand ? "left" : "right")
                   << ";source=" << sample.source_width << 'x' << sample.source_height
                   << ";route=game_cursor_mailbox;desktop_injection=false";
            CameraEvent("flat_ui_pointer", "active", detail.str().c_str());
        }
        g_stereo.flat_ui_pointer_active = true;
        const auto previous = g_stereo.flat_ui_pointer_selection.snapshot();
        g_stereo.flat_ui_pointer_selection.Observe(sample.using_left_hand
            ? cojvr::runtime::UiPointerHand::left : cojvr::runtime::UiPointerHand::right,
            sample.claim, sample.select_down, sample.select_pressed);
        const auto current = g_stereo.flat_ui_pointer_selection.snapshot();
        // Retain the click's target until the engine has processed its hover.
        // A later pose cannot move that queued click to another menu option.
        if (!current.pending || !previous.pending || current.hand != previous.hand ||
            current.claim != previous.claim || current.click != previous.click) {
            g_stereo.flat_ui_pointer_x = sample.pixel_x;
            g_stereo.flat_ui_pointer_y = sample.pixel_y;
        }
        return true;
    } catch (...) {
        NeutralizeFlatUiPointer("exception");
        return false;
    }
}

cojvr::runtime::UiPointerSelectionSnapshot FlatUiSelection() noexcept {
    try {
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        return g_stereo.flat_ui_pointer_selection.snapshot();
    } catch (...) {
        return {};
    }
}

void CompleteFlatUiSelection(const cojvr::runtime::UiPointerSelectionSnapshot& selection,
                             bool dispatched) noexcept {
    try {
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        g_stereo.flat_ui_pointer_selection.Complete(selection, dispatched);
    } catch (...) {}
}

cojvr::runtime::UiPointerSelectionSnapshot ApplyFlatUiPointerMotion() noexcept {
    try {
        const HWND window = reinterpret_cast<HWND>(g_stereo.game_window.load(std::memory_order_acquire));
        HWND root = window ? GetAncestor(window, GA_ROOT) : nullptr;
        if (!root) root = window;
        if (!root || GetForegroundWindow() != root) {
            NeutralizeFlatUiPointer("game_not_foreground");
            return {};
        }
        cojvr::games::call_of_juarez::CameraProbeVector previous{};
        std::int32_t menu_index = 0;
        std::string observation_error;
        const bool readable = cojvr::games::call_of_juarez::ObserveCameraUiPointerPosition(
            previous, &observation_error, &menu_index);
        if (readable) {
            const bool changed = g_stereo.flat_ui_menu_known && g_stereo.flat_ui_menu_index != menu_index;
            g_stereo.flat_ui_menu_index = menu_index;
            g_stereo.flat_ui_menu_known = true;
            if (changed) {
                NeutralizeFlatUiPointer("menu_index_changed");
                return {};
            }
        }
        const bool mouse_button = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 ||
                                  (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        const bool vr_allowed = g_stereo.flat_ui_mouse_priority.Observe(
            previous.x, previous.y, readable, mouse_button, GetTickCount64());
        g_stereo.flat_ui_mouse_override_until_ms.store(
            g_stereo.flat_ui_mouse_priority.override_until_ms(), std::memory_order_release);
        if (!vr_allowed) {
            if (readable) {
                NeutralizeFlatUiPointer("physical_mouse_priority");
            } else {
                std::lock_guard lock(g_stereo.flat_ui_mutex);
                g_stereo.flat_ui_pointer_selection.Cancel(g_stereo.flat_ui_pointer_selection.snapshot());
                g_stereo.flat_ui_target_applied = false;
                if (g_stereo.flat_ui_pointer_active && !g_stereo.flat_ui_pointer_game_error_logged) {
                    const std::string detail =
                        "route=MainMenuModule.FindUI.GetMousePos;owner=game_cursor_only;"
                        "desktop_injection=false;detail=" + observation_error;
                    CameraEvent("flat_ui_pointer_game_route", "failed", detail.c_str());
                    g_stereo.flat_ui_pointer_game_error_logged = true;
                }
            }
            return {};
        }
        cojvr::runtime::UiPointerSelectionSnapshot selection{};
        std::uint32_t pixel_x = 0, pixel_y = 0;
        {
            std::lock_guard lock(g_stereo.flat_ui_mutex);
            if (!g_stereo.flat_ui_pointer_active) return {};
            selection = g_stereo.flat_ui_pointer_selection.snapshot();
            pixel_x = g_stereo.flat_ui_pointer_x;
            pixel_y = g_stereo.flat_ui_pointer_y;
        }
        std::string error;
        bool input_consumed = false;
        bool applied = cojvr::games::call_of_juarez::DispatchCameraUiPointerMotion(
            static_cast<float>(pixel_x), static_cast<float>(pixel_y), &error, &input_consumed);
        cojvr::games::call_of_juarez::CameraProbeVector observed{};
        std::int32_t observed_index = 0;
        if (applied) {
            applied = cojvr::games::call_of_juarez::ObserveCameraUiPointerPosition(observed, &error, &observed_index) &&
                observed_index == menu_index &&
                std::abs(observed.x - static_cast<float>(pixel_x)) <= 1.0F &&
                std::abs(observed.y - static_cast<float>(pixel_y)) <= 1.0F;
            if (!applied && error.empty()) error = "logical cursor did not match the projected target";
            if (applied) g_stereo.flat_ui_mouse_priority.Applied(observed.x, observed.y);
        }
        {
            std::lock_guard lock(g_stereo.flat_ui_mutex);
            const auto current = g_stereo.flat_ui_pointer_selection.snapshot();
            if (!g_stereo.flat_ui_pointer_active || current.hand != selection.hand ||
                current.claim != selection.claim || current.click != selection.click ||
                current.held != selection.held) return {};
            // A failed mouse delivery must not keep its trigger target latched
            // indefinitely. Cancel the click while preserving automatic aim.
            if (!applied) g_stereo.flat_ui_pointer_selection.Cancel(selection);
            const auto previous_delivery = g_stereo.flat_ui_applied_selection;
            const bool hover_ready = g_stereo.flat_ui_target_applied &&
                previous_delivery.hand == selection.hand && previous_delivery.claim == selection.claim &&
                previous_delivery.click == selection.click &&
                g_stereo.flat_ui_applied_x == pixel_x && g_stereo.flat_ui_applied_y == pixel_y &&
                std::abs(previous.x - static_cast<float>(pixel_x)) <= 1.0F &&
                std::abs(previous.y - static_cast<float>(pixel_y)) <= 1.0F;
            g_stereo.flat_ui_target_applied = applied;
            if (applied) {
                g_stereo.flat_ui_applied_selection = selection;
                g_stereo.flat_ui_applied_x = pixel_x;
                g_stereo.flat_ui_applied_y = pixel_y;
            }
            if (selection.pending && !hover_ready) selection.pending = false;
            const auto sequence = ++g_stereo.flat_ui_pointer_delivery_sequence;
            if (!g_stereo.flat_ui_pointer_game_route_logged ||
                (applied && g_stereo.flat_ui_pointer_game_error_logged) ||
                (!applied && !g_stereo.flat_ui_pointer_game_error_logged) ||
                (applied && sequence % 90 == 0)) {
                std::ostringstream detail;
                detail << "route=MainMenuModule.GetGlobalCursor.UICursor.SetPos+OnMouseMove"
                       << ";owner=game_cursor_only;desktop_injection=false;pixel="
                       << pixel_x << ',' << pixel_y << ";claim=" << selection.claim
                       << ";input_consumer=" << (input_consumed ? "native_sprite_tree" : "startup_sprite_only")
                       << ";menu_index=" << menu_index
                       << ";previous=" << previous.x << ',' << previous.y
                       << ";observed=" << observed.x << ',' << observed.y;
                if (!error.empty()) detail << ";detail=" << error;
                CameraEvent("flat_ui_pointer_game_route", applied ? "applied" : "failed", detail.str().c_str());
            }
            g_stereo.flat_ui_pointer_game_route_logged = true;
            g_stereo.flat_ui_pointer_game_error_logged = !applied;
        }
        return applied ? selection : cojvr::runtime::UiPointerSelectionSnapshot{};
    } catch (...) {
        return {};
    }
}

bool DispatchFlatUiPointerSelection(
    const cojvr::runtime::UiPointerSelectionSnapshot& selection,
    bool* loading, std::string* error, bool* hint_dismissed,
    cojvr::games::call_of_juarez::CoJUiDispatchRoute* route) noexcept {
    try {
        const auto reject = [error](const char* reason) {
            if (error) *error = reason;
            return false;
        };
        const auto reject_with_detail = [error](
            const char* reason, const std::string& detail) {
            if (error) {
                *error = reason;
                if (!detail.empty()) {
                    error->append(": ");
                    error->append(detail);
                }
            }
            return false;
        };
        // Serialize validation and dispatch with presenter cancellation/claim
        // changes. An accepted tap can be released while waiting for hover.
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        const auto current = g_stereo.flat_ui_pointer_selection.snapshot();
        const HWND window = reinterpret_cast<HWND>(g_stereo.game_window.load(std::memory_order_acquire));
        HWND root = window ? GetAncestor(window, GA_ROOT) : nullptr;
        if (!root) root = window;
        if (!selection.pending) return reject("selection_not_pending");
        if (!current.pending) return reject("current_not_pending");
        if (!g_stereo.flat_ui_pointer_active) return reject("pointer_inactive");
        if (!g_stereo.flat_ui_target_applied) return reject("target_not_applied");
        if (current.hand != selection.hand || current.claim != selection.claim ||
            current.click != selection.click) {
            return reject("identity_mismatch");
        }
        if (!root) return reject("game_window_unavailable");
        if (GetForegroundWindow() != root) return reject("game_not_foreground");
        cojvr::games::call_of_juarez::CameraProbeVector position{};
        std::int32_t menu_index = 0;
        std::string readback_error;
        const bool readback_ok = cojvr::games::call_of_juarez::ObserveCameraUiPointerPosition(
            position, &readback_error, &menu_index);
        const char* rejection_reason = nullptr;
        if (!readback_ok) {
            rejection_reason = "readback_failed";
        } else if (!g_stereo.flat_ui_menu_known) {
            rejection_reason = "menu_unknown";
        } else if (menu_index != g_stereo.flat_ui_menu_index) {
            rejection_reason = "menu_index_mismatch";
        } else if (std::abs(position.x - static_cast<float>(g_stereo.flat_ui_applied_x)) > 1.0F ||
                   std::abs(position.y - static_cast<float>(g_stereo.flat_ui_applied_y)) > 1.0F) {
            rejection_reason = "coordinate_mismatch";
        }
        if (rejection_reason) {
            g_stereo.flat_ui_pointer_selection.Observe(cojvr::runtime::UiPointerHand::none, 0, false, false);
            g_stereo.flat_ui_target_applied = false;
            return reject_with_detail(rejection_reason, readback_error);
        }
        std::string dispatch_error;
        const bool dispatched = cojvr::games::call_of_juarez::DispatchCameraUiPointerSelectPress(
            false, loading, &dispatch_error, hint_dismissed, route);
        if (!dispatched) return reject_with_detail("dispatch_failed", dispatch_error);
        if (error) error->clear();
        return true;
    } catch (...) {
        try {
            if (error) *error = "exception";
        } catch (...) {}
        return false;
    }
}

bool ShouldLogStereoTiming(const std::uint64_t frame_sequence) noexcept {
    return frame_sequence > 0 && (frame_sequence <= 8 || (frame_sequence % 90) == 0);
}

std::string ReadJsonStringField(const std::string_view text, const std::string_view field) {
    const std::string needle = "\"" + std::string(field) + "\"";
    const std::size_t field_offset = text.find(needle);
    if (field_offset == std::string_view::npos) return {};
    const std::size_t colon = text.find(':', field_offset + needle.size());
    if (colon == std::string_view::npos) return {};
    const std::size_t quote = text.find('"', colon + 1);
    if (quote == std::string_view::npos) return {};
    const std::size_t end = text.find('"', quote + 1);
    if (end == std::string_view::npos) return {};
    return std::string(text.substr(quote + 1, end - quote - 1));
}

std::string ReadRunManifestField(const std::string_view field) noexcept {
    try {
        const auto path = GameDirectory() / L".cojvr-run.json";
        std::ifstream input(path, std::ios::binary);
        if (!input) return {};
        const std::string text{
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        return ReadJsonStringField(text, field);
    } catch (...) {
        return {};
    }
}

void CameraEvent(const char* event, const char* result, const char* detail) noexcept {
    try {
        std::ostringstream line;
        line << "camera_probe_event: event=" << (event ? event : "unknown")
             << " result=" << (result ? result : "unknown");
        if (detail && *detail) line << " detail=" << detail;
        LogLine(line.str());
    } catch (...) {
    }
}

void LogTransportFailure(
    const std::uint64_t frame_sequence,
    const char* stage,
    const std::string_view error) noexcept {
    if (frame_sequence == 0 ||
        g_stereo.last_error_frame.exchange(frame_sequence, std::memory_order_acq_rel) ==
            frame_sequence) {
        return;
    }
    try {
        std::ostringstream line;
        line << "native_stereo_transport: status=failed stage=" << stage
             << " frame_sequence=" << frame_sequence << " error=" << error;
        LogLine(line.str());
    } catch (...) {
    }
}

IDirect3DDevice9* RetainCurrentDevice() noexcept {
    try {
        std::lock_guard lock(g_stereo.device_mutex);
        if (!g_stereo.device) return nullptr;
        g_stereo.device->AddRef();
        return g_stereo.device;
    } catch (...) {
        return nullptr;
    }
}

bool PublishCapturedFrame(
    NativeStereoState& state,
    cojvr::backends::d3d9::StereoCpuFrame frame,
    const cojvr::backends::d3d9::FramePresentationMode presentation_mode,
    const char* source) noexcept {
    try {
        frame.presentation_mode = presentation_mode;
        frame.transport_sequence =
            state.transport_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
        const std::uint64_t capture_sequence = frame.capture_sequence;
        const bool shared_frame =
            frame.transport ==
            cojvr::backends::d3d9::StereoFrameTransport::d3d9ex_shared_texture;
        const std::uintptr_t device_id = frame.device_id;
        const std::uint64_t generation = frame.generation;
        const std::uint32_t producer_slot = frame.producer_slot;
        if (!state.presenter.Publish(std::move(frame))) {
            LogTransportFailure(
                capture_sequence, source,
                "presenter mailbox rejected completed CPU frame");
            return false;
        }
        if (shared_frame) {
            std::ostringstream line;
            line << "native_stereo_startup_event: event=shared_frame_published"
                 << ";thread_id=" << GetCurrentThreadId()
                 << ";device=0x" << std::hex << device_id << std::dec
                 << ";generation=" << generation
                 << ";frame_sequence=" << capture_sequence
                 << ";slot=" << producer_slot;
            LogLine(line.str());
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool PaceRenderFrame(NativeStereoState* state) noexcept {
    if (!state || g_finalization_started.load(std::memory_order_acquire)) return false;
    try {
    const float reported_hz = state->presenter.display_frequency_hz();
    const float refresh_hz = state->frame_cadence.Target(
        reported_hz, state->desktop_vsync_disabled.load(std::memory_order_acquire));
    if (refresh_hz != state->last_pacing_hz) {
        state->last_pacing_hz = refresh_hz;
        std::ostringstream line;
        line << "native_stereo_render_pacing: source=hmd_property;target_hz="
             << refresh_hz << ";mode=" << (refresh_hz > 0.0F ? "rate_cap" : "unavailable_fail_open");
        LogLine(line.str());
    }
    if (reported_hz > 0.0F && refresh_hz == 0.0F && !state->late_rate_reported) {
        state->late_rate_reported = true;
        LogLine("native_stereo_render_pacing: status=pending reason=desktop_vsync_reset_required");
    }
    const auto pacing_begin = std::chrono::steady_clock::now();
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        pacing_begin.time_since_epoch()).count();
    const auto deadline = std::chrono::steady_clock::time_point(
        std::chrono::nanoseconds(state->frame_pacer.Deadline(now_ns, refresh_hz)));
    // Intentional CPU render pacing, separate from nonblocking GPU handoff.
    // Refresh changes rebase the cap. Retain the last cadence while the mirror
    // is IMMEDIATE; missing initial timing keeps the game's desktop interval.
    while (std::chrono::steady_clock::now() < deadline) {
        if (g_finalization_started.load(std::memory_order_acquire)) {
            state->frame_pacer.Reset();
            return false;
        }
        std::this_thread::sleep_until(std::min(
            deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds(2)));
    }
    if (ShouldLogStereoTiming(++state->pacing_sequence)) {
        const double wait_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - pacing_begin).count();
        std::ostringstream line;
        line << "native_stereo_render_pacing_timing: sequence=" << state->pacing_sequence
             << ";target_hz=" << refresh_hz << std::fixed << std::setprecision(3)
             << ";pacing_wait_ms=" << wait_ms << ";wait_kind=cpu_render_schedule";
        LogLine(line.str());
    }
    state->frame_cadence.RenderPaced();
    return true;
    } catch (...) {
        state->frame_pacer.Reset();
        return false;
    }
}

void BeforePresentBoundary(IDirect3DDevice9* device, const char* source) noexcept {
    if (!device || !g_stereo.runtime_ready) return;
    if (g_stereo.frame_cadence.PresentNeedsPacing()) {
        if (!PaceRenderFrame(&g_stereo)) return;
        (void)g_stereo.frame_cadence.PresentNeedsPacing();
    }
    try {
        bool timer_frozen = false;
        std::string timer_error;
        const bool timer_valid =
            cojvr::games::call_of_juarez::ObserveCameraGameTimerFrozen(
                timer_frozen, &timer_error);
        constexpr std::uint64_t kStereoFreshnessMs = 250;
        const std::uint64_t now_ms = GetTickCount64();
        const std::uint64_t last_stereo_ms =
            g_stereo.last_native_stereo_tick_ms.load(std::memory_order_acquire);
        if (!(timer_valid && timer_frozen) && !g_stereo.flat_ui_select_release_required &&
            last_stereo_ms != 0 && now_ms >= last_stereo_ms &&
            now_ms - last_stereo_ms < kStereoFreshnessMs) {
            g_stereo.flat_capture_active.store(false, std::memory_order_release);
            return;
        }

        if (!g_stereo.flat_capture_active.exchange(true, std::memory_order_acq_rel)) {
            std::ostringstream entered;
            entered << "native_stereo_flat_theater: status=entered source="
                    << (source ? source : "unknown")
                    << ";reason="
                    << (timer_valid && timer_frozen ? "game_timer_frozen" : "native_stereo_stale");
            LogLine(entered.str());
        }

        cojvr::backends::openvr::OpenVrTrackingSample tracking{};
        if (!g_stereo.presenter.LatestTracking(tracking) ||
            !tracking.pose.orientation_valid || !tracking.pose.position_valid ||
            tracking.sequence == 0) {
            return;
        }

        const HWND ui_window = reinterpret_cast<HWND>(g_stereo.game_window.load(std::memory_order_acquire));
        HWND ui_root = ui_window ? GetAncestor(ui_window, GA_ROOT) : nullptr;
        if (!ui_root) ui_root = ui_window;
        const bool menu_dispatch_allowed = tracking.ui_actions_allowed && ui_root && GetForegroundWindow() == ui_root;
        if (!menu_dispatch_allowed) {
            g_stereo.flat_ui_select_retry = {};
        }
        // Input focus gates menu mutation, not the frame publication that can
        // establish compositor focus during startup.
        if (menu_dispatch_allowed) {
            // Navigation cancels the prior menu's trigger before moving or
            // selecting anything in the destination menu.
            if ((tracking.ui_back_pressed || tracking.pause_pressed) || tracking.ui_accept_pressed)
                NeutralizeFlatUiPointer("ui_navigation");
            if ((tracking.ui_back_pressed || tracking.pause_pressed)) g_stereo.flat_ui_select_retry = {};
            if ((tracking.ui_back_pressed || tracking.pause_pressed)) {
                cojvr::games::call_of_juarez::CoJUiDispatchRoute back_route =
                    cojvr::games::call_of_juarez::CoJUiDispatchRoute::none;
                std::string back_error;
                const bool backed = cojvr::games::call_of_juarez::DispatchCameraUiBackPress(
                    &back_error, &back_route);
                std::ostringstream detail;
                detail << "source=" << (tracking.pause_pressed ? "right_options" : "right_circle") << ";route="
                       << cojvr::games::call_of_juarez::CoJUiBackDispatchRouteName(back_route);
                if (!back_error.empty()) detail << ";detail=" << back_error;
                CameraEvent("flat_ui_back", backed ? "applied" : "failed", detail.str().c_str());
            }
            const auto pointer_selection = ApplyFlatUiPointerMotion();
            const auto pointer_hand = pointer_selection.hand;
            const bool pointer_active = pointer_hand != cojvr::runtime::UiPointerHand::none;
            // Selection may follow only a successfully applied logical cursor
            // target from the same hand/claim/click. No desktop motion is injected.

            if (g_stereo.flat_ui_select_release_required && timer_valid && !timer_frozen &&
                !tracking.ui_select_left && !tracking.ui_select_right && !tracking.ui_accept) {
                g_stereo.flat_ui_select_release_required = false;
                g_stereo.flat_loading_resume_pending = false;
                CameraEvent(
                    "blocking_ui_resume",
                    "observed",
                    "game_timer_valid=true;game_timer_frozen=false;trigger_released=true;route=LawmanModule.TimerStart");
            }

            // Ray edges arrive only after the accepted motion, tagged with its hand
            // and claim. Raw action edges cannot become valid under a later claim.
            const bool pointer_select = pointer_selection.pending;
            g_stereo.flat_ui_select_retry.Observe(!(tracking.ui_back_pressed || tracking.pause_pressed) && tracking.ui_accept_pressed,
                !(tracking.ui_back_pressed || tracking.pause_pressed) && tracking.ui_accept);
            const bool cross_select = g_stereo.flat_ui_select_retry.ShouldDispatch(true, timer_valid && timer_frozen);
            const bool ui_select_pressed = pointer_select || tracking.ui_accept_pressed;
            const bool select_allowed = pointer_active || tracking.ui_accept || (timer_valid && timer_frozen);
            if (pointer_select || cross_select) {
                bool current_ui_is_loading = false;
                bool paused_hint_dismissed = false;
                cojvr::games::call_of_juarez::CoJUiDispatchRoute ui_route =
                    cojvr::games::call_of_juarez::CoJUiDispatchRoute::none;
                std::string ui_error;
                const bool dispatched = select_allowed && (cross_select ?
                    cojvr::games::call_of_juarez::DispatchCameraUiSelectPress(
                        false, &current_ui_is_loading, &ui_error,
                        &paused_hint_dismissed, &ui_route) :
                    DispatchFlatUiPointerSelection(pointer_selection, &current_ui_is_loading,
                        &ui_error, &paused_hint_dismissed, &ui_route));
                g_stereo.flat_ui_select_retry.Complete(dispatched);
                if (pointer_select && !cross_select) CompleteFlatUiSelection(pointer_selection, dispatched);
                if (ui_select_pressed || dispatched) {
                    std::ostringstream detail;
                    detail << "source=";
                    if (cross_select) {
                        detail << "right_cross";
                    } else {
                        detail
                           << (pointer_hand == cojvr::runtime::UiPointerHand::left ? "left_l2" : "right_r2");
                    }
                    detail
                           << ";pointer_active=" << (pointer_active ? "true" : "false")
                           << ";pointer_claim=" << pointer_selection.claim
                           << ";retry=" << (!ui_select_pressed ? "true" : "false")
                           << ";current_ui_is_loading="
                           << (current_ui_is_loading ? "true" : "false")
                           << ";paused_hint_dismissed="
                           << (paused_hint_dismissed ? "true" : "false")
                           << ";route=" <<
                               cojvr::games::call_of_juarez::CoJUiDispatchRouteName(ui_route);
                    if (!ui_error.empty()) detail << ";detail=" << ui_error;
                    CameraEvent(
                        "flat_ui_select",
                        dispatched ? "applied" : "failed",
                        detail.str().c_str());
                }
                if (dispatched && timer_valid && timer_frozen) {
                    g_stereo.flat_loading_resume_pending = true;
                    g_stereo.flat_ui_select_release_required = true;
                    std::ostringstream blocking_detail;
                    blocking_detail << "source="
                                    << (cross_select ? "right_cross" :
                                        (pointer_hand == cojvr::runtime::UiPointerHand::left ? "left_l2" : "right_r2"))
                                    << ";game_timer_valid=true;game_timer_frozen=true"
                                    << ";current_ui_is_loading="
                                    << (current_ui_is_loading ? "true" : "false")
                                    << ";gameplay_suppressed=true"
                                    << ";body_mutation_suppressed=true"
                                    << ";fire_suppressed_until_release=true"
                                    << ";route=" <<
                                        cojvr::games::call_of_juarez::CoJUiDispatchRouteName(ui_route);
                    CameraEvent("blocking_ui_select", "applied", blocking_detail.str().c_str());
                }
            }

        }
        // The startup/menu fallback does not need a full 60/90 Hz CPU readback.
        // Capture every second Present; the presenter repeats the latest frame
        // at compositor cadence between updates.
        const std::uint64_t present_sequence =
            g_stereo.flat_capture_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
        if ((present_sequence & 1U) != 0U) return;

        Microsoft::WRL::ComPtr<IDirect3DSurface9> back_buffer;
        const HRESULT back_buffer_result =
            device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
        if (FAILED(back_buffer_result) || !back_buffer) return;
        const std::uint64_t generation =
            g_stereo.device_generation.load(std::memory_order_acquire);
        if (generation == 0) return;
        cojvr::backends::d3d9::StereoCpuFrame completed{};
        if (!g_stereo.flat_capture.CaptureFlatFrameImmediate(
                device, back_buffer.Get(), present_sequence, generation,
                tracking.pose, tracking.sequence, completed)) {
            LogTransportFailure(
                present_sequence, "flat_capture", g_stereo.flat_capture.last_error());
            return;
        }
        if (ShouldLogStereoTiming(present_sequence)) {
            LogLine(
                "native_stereo_flat_theater: status=captured " +
                std::string(g_stereo.flat_capture.capture_description()));
        }
        if (PublishCapturedFrame(
                g_stereo, std::move(completed),
                cojvr::backends::d3d9::FramePresentationMode::flat_theater,
                "flat_publish") &&
            ShouldLogStereoTiming(present_sequence)) {
            LogLine(
                "native_stereo_flat_theater: status=published " +
                std::string(g_stereo.flat_capture.collect_description()));
        }
    } catch (...) {
        LogLine("native_stereo_flat_theater: status=exception");
    }
}

thread_local std::uint32_t g_device_present_depth = 0;
thread_local std::uint64_t g_device_present_sequence = 0;

void BeforeDevicePresent(IDirect3DDevice9* device) noexcept {
    ++g_device_present_depth;
    if (g_device_present_depth == 1) {
        g_device_present_sequence =
            g_stereo.present_telemetry_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (g_device_present_sequence <= 8 || (g_device_present_sequence % 90) == 0) {
            try {
                std::ostringstream line;
                line << "native_stereo_startup_event: event=present_enter"
                     << ";thread_id=" << GetCurrentThreadId()
                     << ";device=0x" << std::hex
                     << reinterpret_cast<std::uintptr_t>(device) << std::dec
                     << ";generation="
                     << g_stereo.device_generation.load(std::memory_order_acquire)
                     << ";frame_sequence=" << g_device_present_sequence;
                LogLine(line.str());
            } catch (...) {
            }
        }
        BeforePresentBoundary(device, "device_present");
    }
}

void AfterDevicePresent(IDirect3DDevice9* device, const HRESULT result) noexcept {
    if (g_device_present_depth == 1 &&
        (g_device_present_sequence <= 8 || (g_device_present_sequence % 90) == 0)) {
        try {
            std::ostringstream line;
            line << "native_stereo_startup_event: event=present_exit"
                 << ";thread_id=" << GetCurrentThreadId()
                 << ";device=0x" << std::hex
                 << reinterpret_cast<std::uintptr_t>(device) << std::dec
                 << ";generation="
                 << g_stereo.device_generation.load(std::memory_order_acquire)
                 << ";frame_sequence=" << g_device_present_sequence
                 << ";hr=0x" << std::hex << static_cast<unsigned long>(result) << std::dec;
            LogLine(line.str());
        } catch (...) {
        }
    }
    if (g_device_present_depth > 0) --g_device_present_depth;
}

void BeforeDeviceReset(
    IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* parameters) noexcept {
    g_stereo.frame_pacer.Reset();
    const float refresh_hz = g_stereo.runtime_ready ? g_stereo.presenter.display_frequency_hz() : 0.0F;
    if (parameters) (void)g_stereo.present_policy.Begin(*parameters, refresh_hz, true);
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=reset_enter"
             << ";thread_id=" << GetCurrentThreadId()
             << ";device=0x" << std::hex << reinterpret_cast<std::uintptr_t>(device)
             << std::dec
             << ";generation="
             << g_stereo.device_generation.load(std::memory_order_acquire);
        if (parameters) {
            line << ";backbuffer=" << parameters->BackBufferWidth << 'x'
                 << parameters->BackBufferHeight
                 << ";format=" << static_cast<unsigned>(parameters->BackBufferFormat)
                 << ";windowed=" << (parameters->Windowed ? "true" : "false")
                 << ";swap_effect=" << static_cast<unsigned>(parameters->SwapEffect);
        }
        LogLine(line.str());
    } catch (...) {
    }
}

void AfterDeviceReset(
    IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* parameters,
    const HRESULT result) noexcept {
    if (parameters) g_stereo.present_policy.Complete(SUCCEEDED(result), *parameters, g_stereo.frame_cadence);
    try {
        std::uint64_t generation =
            g_stereo.device_generation.load(std::memory_order_acquire);
        if (SUCCEEDED(result)) {
            g_stereo.desktop_vsync_disabled.store(parameters &&
                parameters->PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE,
                std::memory_order_release);
            g_stereo.late_rate_reported = false;
            generation = g_stereo.device_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
            g_stereo.flat_capture_active.store(false, std::memory_order_release);
            g_stereo.last_native_stereo_tick_ms.store(0, std::memory_order_release);
            g_stereo.capture_source_logged = {};
        }
        std::ostringstream line;
        line << "native_stereo_startup_event: event=reset_exit"
             << ";thread_id=" << GetCurrentThreadId()
             << ";device=0x" << std::hex << reinterpret_cast<std::uintptr_t>(device)
             << std::dec
             << ";generation=" << generation
             << ";hr=0x" << std::hex << static_cast<unsigned long>(result) << std::dec;
        LogLine(line.str());
        if (SUCCEEDED(result)) {
            std::ostringstream changed;
            changed << "native_stereo_startup_event: event=resource_generation_changed"
                    << ";thread_id=" << GetCurrentThreadId()
                    << ";device=0x" << std::hex
                    << reinterpret_cast<std::uintptr_t>(device) << std::dec
                    << ";generation=" << generation
                    << ";reason=reset_success";
            LogLine(changed.str());
        }
    } catch (...) {
    }
}

void BeforeSwapChainPresent(IDirect3DDevice9* device) noexcept {
    if (g_device_present_depth == 0) {
        BeforePresentBoundary(device, "swapchain_present");
    }
}

void MaintainDevicePresentHook(const std::stop_token stop_token) noexcept {
    using namespace std::chrono_literals;
    while (!stop_token.stop_requested()) {
        IDirect3DDevice9* device = RetainCurrentDevice();
        if (device) {
            const auto outcome =
                cojvr::backends::d3d9::ReacquireDeviceVtableHookDetailed(device);
            (void)cojvr::games::call_of_juarez::
                ReacquireD3D9ExLegacyTextureCompatibility(device);
            device->Release();
            if (outcome.result ==
                    cojvr::backends::d3d9::HookRegistryResult::Installed &&
                outcome.modified_slots > 0) {
                g_stereo.present_hook_conflict_logged.store(
                    false, std::memory_order_release);
                LogLine(
                    "native_stereo_device_present_hook: status=reacquired slots=" +
                    std::to_string(outcome.modified_slots));
            } else if (outcome.result ==
                           cojvr::backends::d3d9::HookRegistryResult::Conflict &&
                !g_stereo.present_hook_conflict_logged.exchange(
                    true, std::memory_order_acq_rel)) {
                LogLine(
                    "native_stereo_device_present_hook: status=foreign_owner_preserved");
            }
        }
        std::this_thread::sleep_for(100ms);
    }
}

void StartPresentHookWorker() noexcept {
    try {
        std::lock_guard lock(g_stereo.present_hook_worker_mutex);
        if (!g_stereo.present_hook_worker.joinable()) {
            g_stereo.present_hook_worker = std::jthread(MaintainDevicePresentHook);
        }
    } catch (...) {
        LogLine("native_stereo_device_present_hook: status=watchdog_start_failed");
    }
}

void StopPresentHookWorker() noexcept {
    try {
        std::lock_guard lock(g_stereo.present_hook_worker_mutex);
        if (g_stereo.present_hook_worker.joinable()) {
            g_stereo.present_hook_worker.request_stop();
            g_stereo.present_hook_worker.join();
        }
    } catch (...) {
        LogLine("native_stereo_device_present_hook: status=watchdog_stop_failed");
    }
}

void BeforeCreateDevice(
    IDirect3D9* factory,
    const UINT adapter,
    const D3DDEVTYPE device_type,
    HWND focus_window,
    const DWORD behavior_flags,
    D3DPRESENT_PARAMETERS* presentation_parameters) noexcept {
    const float refresh_hz = g_stereo.runtime_ready ? g_stereo.presenter.display_frequency_hz() : 0.0F;
    bool vr_paced = false;
    const DWORD requested_interval = presentation_parameters
        ? presentation_parameters->PresentationInterval : 0;
    if (presentation_parameters) {
        vr_paced = g_stereo.present_policy.Begin(*presentation_parameters, refresh_hz, false);
    }
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=create_device_enter"
             << ";thread_id=" << GetCurrentThreadId()
             << ";factory=0x" << std::hex << reinterpret_cast<std::uintptr_t>(factory)
             << std::dec
             << ";adapter=" << adapter
             << ";device_type=" << static_cast<unsigned>(device_type)
             << ";focus_window=0x" << std::hex
             << reinterpret_cast<std::uintptr_t>(focus_window)
             << ";behavior_flags=0x" << behavior_flags << std::dec;
        if (presentation_parameters) {
            line << ";backbuffer=" << presentation_parameters->BackBufferWidth << 'x'
                 << presentation_parameters->BackBufferHeight
                 << ";format="
                 << static_cast<unsigned>(presentation_parameters->BackBufferFormat)
                 << ";windowed="
                 << (presentation_parameters->Windowed ? "true" : "false")
                 << ";swap_effect="
                 << static_cast<unsigned>(presentation_parameters->SwapEffect)
                 << ";requested_present_interval=" << requested_interval
                 << ";effective_present_interval=" << presentation_parameters->PresentationInterval
                 << ";vr_paced=" << (vr_paced ? "true" : "false");
        }
        LogLine(line.str());
    } catch (...) {
    }
}

void AfterCreateDevice(
    IDirect3D9* factory,
    UINT,
    D3DDEVTYPE,
    HWND focus_window,
    DWORD,
    D3DPRESENT_PARAMETERS* parameters,
    IDirect3DDevice9** returned_device,
    const HRESULT result) noexcept {
    IDirect3DDevice9* observed = returned_device ? *returned_device : nullptr;
    Microsoft::WRL::ComPtr<IDirect3DDevice9Ex> observed_ex;
    const bool returned_ex = observed &&
        SUCCEEDED(observed->QueryInterface(IID_PPV_ARGS(&observed_ex))) && observed_ex;
    try {
        std::ostringstream result_line;
        result_line << "native_stereo_startup_event: event=create_device_ex_result"
                    << ";thread_id=" << GetCurrentThreadId()
                    << ";device=0x" << std::hex
                    << reinterpret_cast<std::uintptr_t>(observed)
                    << ";hr=0x" << static_cast<unsigned long>(result) << std::dec
                    << ";device_api="
                    << (returned_ex ? "d3d9ex" : (observed ? "classic_d3d9" : "none"));
        LogLine(result_line.str());
    } catch (...) {
    }
    if (parameters) g_stereo.present_policy.Complete(SUCCEEDED(result) && observed, *parameters, g_stereo.frame_cadence);
    if (FAILED(result) || !observed) return;
    g_stereo.desktop_vsync_disabled.store(parameters &&
        parameters->PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE,
        std::memory_order_release);
    try {
        const auto log_factory_identity = [factory, observed](const char* event) {
            Microsoft::WRL::ComPtr<IDirect3D9> recovered_factory;
            Microsoft::WRL::ComPtr<IUnknown> expected_identity;
            Microsoft::WRL::ComPtr<IUnknown> recovered_identity;
            const HRESULT recovered_result = observed->GetDirect3D(&recovered_factory);
            const bool matches = factory &&
                SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&expected_identity))) &&
                SUCCEEDED(recovered_result) && recovered_factory &&
                SUCCEEDED(recovered_factory->QueryInterface(IID_PPV_ARGS(&recovered_identity))) &&
                expected_identity.Get() == recovered_identity.Get();
            std::ostringstream line;
            line << "native_stereo_startup_event: event=" << event
                 << ";thread_id=" << GetCurrentThreadId()
                 << ";factory=0x" << std::hex
                 << reinterpret_cast<std::uintptr_t>(factory)
                 << ";recovered=0x"
                 << reinterpret_cast<std::uintptr_t>(recovered_factory.Get())
                 << ";device=0x" << reinterpret_cast<std::uintptr_t>(observed)
                 << ";hr=0x" << static_cast<unsigned long>(recovered_result)
                 << std::dec << ";matches=" << (matches ? "true" : "false");
            LogLine(line.str());
        };
        log_factory_identity("factory_native_identity");
        IDirect3DDevice9* replacement = observed;
        HWND root = focus_window ? GetAncestor(focus_window, GA_ROOT) : nullptr;
        if (!root) root = focus_window;
        g_stereo.game_window.store(
            reinterpret_cast<std::uintptr_t>(root), std::memory_order_release);
        replacement->AddRef();
        IDirect3DDevice9* previous = nullptr;
        {
            std::lock_guard lock(g_stereo.device_mutex);
            previous = g_stereo.device;
            g_stereo.device = replacement;
        }
        const std::uint64_t generation =
            g_stereo.device_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
        const bool is_d3d9ex = returned_ex;
        g_stereo.flat_capture_active.store(false, std::memory_order_release);
        g_stereo.last_native_stereo_tick_ms.store(0, std::memory_order_release);
        if (previous) previous->Release();
        const bool legacy_compat = is_d3d9ex &&
            cojvr::games::call_of_juarez::InstallD3D9ExLegacyTextureCompatibility(
                replacement,
                cojvr::games::call_of_juarez::LegacyTextureCompatibilityCallbacks{
                    .event = &LegacyTextureEvent,
                    .resource_destroy = &ResourceDestroyEvent,
                });
        const cojvr::backends::d3d9::DeviceHookCallbacks device_callbacks{
            .before_present = &BeforeDevicePresent,
            .after_present = &AfterDevicePresent,
            .before_reset = &BeforeDeviceReset,
            .after_reset = &AfterDeviceReset,
            .com_trace = &DeviceComTrace,
            .method_trace = &DeviceMethodTrace,
            .get_direct3d_factory = factory,
            .get_direct3d_device = replacement,
        };
        const auto device_hook =
            cojvr::backends::d3d9::InstallDeviceVtableHookDetailed(
                replacement, device_callbacks);
        log_factory_identity("factory_identity");
        const cojvr::backends::d3d9::SwapChainHookCallbacks swapchain_callbacks{
            .before_present = &BeforeSwapChainPresent,
            .com_trace = &SwapChainComTrace,
            .method_trace = &SwapChainMethodTrace,
        };
        const auto swapchain_hook =
            cojvr::backends::d3d9::InstallSwapChainVtableHookDetailed(
                replacement, swapchain_callbacks);
        {
            std::ostringstream hook_line;
            hook_line << "native_stereo_startup_event: event=device_hook_installed"
                      << ";thread_id=" << GetCurrentThreadId()
                      << ";device=0x" << std::hex
                      << reinterpret_cast<std::uintptr_t>(replacement) << std::dec
                      << ";generation=" << generation
                      << ";result=" << static_cast<unsigned>(device_hook.result)
                      << ";modified_slots=" << device_hook.modified_slots;
            LogLine(hook_line.str());
        }
        {
            std::ostringstream hook_line;
            hook_line << "native_stereo_startup_event: event=swapchain_hook_installed"
                      << ";thread_id=" << GetCurrentThreadId()
                      << ";device=0x" << std::hex
                      << reinterpret_cast<std::uintptr_t>(replacement) << std::dec
                      << ";generation=" << generation
                      << ";result=" << static_cast<unsigned>(swapchain_hook.result)
                      << ";modified_slots=" << swapchain_hook.modified_slots;
            LogLine(hook_line.str());
        }
        {
            std::ostringstream observed_line;
            observed_line << "native_stereo_startup_event: event=device_observed"
                          << ";thread_id=" << GetCurrentThreadId()
                          << ";device=0x" << std::hex
                          << reinterpret_cast<std::uintptr_t>(replacement) << std::dec
                          << ";generation=" << generation
                          << ";device_api="
                          << (is_d3d9ex ? "d3d9ex" : "classic_d3d9_fallback")
                          << ";legacy_texture_compat="
                          << (legacy_compat ? "installed" : "unavailable");
            LogLine(observed_line.str());
        }
        std::ostringstream line;
        line << "native_stereo_device: status=observed device=0x" << std::hex
             << reinterpret_cast<std::uintptr_t>(replacement) << std::dec
             << " generation=" << generation
             << " device_api=" << (is_d3d9ex ? "d3d9ex" : "classic_d3d9_fallback")
             << " device_present_hook="
             << ((device_hook.result == cojvr::backends::d3d9::HookRegistryResult::Installed ||
                     device_hook.result == cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled)
                     ? "installed"
                     : "failed")
             << " swapchain_present_hook="
             << ((swapchain_hook.result == cojvr::backends::d3d9::HookRegistryResult::Installed ||
                     swapchain_hook.result == cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled)
                     ? "installed"
                     : "failed");
        LogLine(line.str());
        StartPresentHookWorker();
    } catch (...) {
    }
}

bool BeginStereoFrame(
    void* context,
    cojvr::games::call_of_juarez::CameraStereoFrameSample& sample) noexcept {
    sample = {};
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state || !state->runtime_ready ||
        g_finalization_started.load(std::memory_order_acquire) || !state->presenter.running()) return false;
    if (!PaceRenderFrame(state)) return false;
    bool timer_frozen = false;
    std::string timer_error;
    if (cojvr::games::call_of_juarez::ObserveCameraGameTimerFrozen(
            timer_frozen, &timer_error) && timer_frozen) {
        state->last_native_stereo_tick_ms.store(0, std::memory_order_release);
        return false;
    }
    cojvr::backends::openvr::OpenVrTrackingSample tracking{};
    if (!state->presenter.LatestTracking(tracking) ||
        !tracking.pose.orientation_valid || !tracking.pose.position_valid ||
        tracking.sequence == 0) {
        return false;
    }
    if (state->flat_ui_select_release_required) {
        if (tracking.ui_select_left || tracking.ui_select_right || tracking.ui_accept) {
            state->last_native_stereo_tick_ms.store(0, std::memory_order_release);
            return false;
        }
        state->flat_ui_select_release_required = false;
        state->flat_loading_resume_pending = false;
        CameraEvent(
            "blocking_ui_resume",
            "observed",
            "game_timer_valid=true;game_timer_frozen=false;trigger_released=true;route=BeginStereoFrame");
    }

    state->last_native_stereo_tick_ms.store(GetTickCount64(), std::memory_order_release);
    if (state->flat_capture_active.exchange(false, std::memory_order_acq_rel)) {
        LogLine("native_stereo_flat_theater: status=leaving reason=native_stereo_active");
    }

    cojvr::backends::d3d9::StereoCpuFrame completed{};
    const bool capture_enabled =
        state->capture_readback_enabled.load(std::memory_order_acquire);
    if (capture_enabled && state->capture.TryCollectReady(completed)) {
        const std::uint64_t sequence = completed.capture_sequence;
        if (PublishCapturedFrame(
                *state, std::move(completed),
                cojvr::backends::d3d9::FramePresentationMode::native_stereo,
                "publish")) {
            if (ShouldLogStereoTiming(sequence)) {
                LogLine(
                    "native_stereo_producer_timing: status=published " +
                    std::string(state->capture.collect_description()));
            }
        }
    }
    sample.hmd_pose.pose = tracking.pose;
    sample.hmd_pose.sequence = tracking.sequence;
    sample.left_controller = tracking.left_controller;
    sample.right_controller = tracking.right_controller;
    sample.left_aim = tracking.left_aim;
    sample.right_aim = tracking.right_aim;
    sample.gameplay = tracking.gameplay;
    sample.left_fingers = tracking.left_fingers;
    sample.right_fingers = tracking.right_fingers;
    sample.recenter_requested = tracking.recenter_requested;
    sample.ui_select_left = tracking.ui_select_left;
    sample.ui_select_right = tracking.ui_select_right;
    sample.ui_select_left_pressed = tracking.ui_select_left_pressed;
    sample.ui_select_right_pressed = tracking.ui_select_right_pressed;
    sample.ui_accept = tracking.ui_accept;
    sample.ui_back = tracking.ui_back;
    sample.pause_pressed = tracking.pause_pressed;
    sample.ui_accept_pressed = tracking.ui_accept_pressed;
    sample.ui_back_pressed = tracking.ui_back_pressed;
    if (tracking.recenter_requested) {
        LogLine("openvr_input_event: action=recenter result=pressed source=global_action owner=presenter_thread");
    }
    sample.eyes = state->eyes;
    return true;
}

bool CaptureStereoEye(
    void* context,
    const cojvr::runtime::Eye eye,
    const std::uint64_t frame_sequence,
    std::uint64_t* content_hash) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state || !content_hash) return false;
    state->native_render_thread.store(GetCurrentThreadId(), std::memory_order_release);
    *content_hash = 0;
    if (!state->capture_readback_enabled.load(std::memory_order_acquire)) return true;
    IDirect3DDevice9* device = RetainCurrentDevice();
    if (!device) {
        LogTransportFailure(frame_sequence, "capture", "D3D9 device unavailable");
        return false;
    }
    const std::uint64_t generation =
        state->device_generation.load(std::memory_order_acquire);
    const std::uintptr_t device_id = reinterpret_cast<std::uintptr_t>(device);
    const bool captured = state->capture.CaptureEye(
        device, eye, frame_sequence, generation);
    device->Release();
    if (captured) {
        std::uint64_t previously_logged =
            state->capture_resources_generation_logged.load(std::memory_order_acquire);
        if (generation != 0 && previously_logged != generation &&
            state->capture_resources_generation_logged.compare_exchange_strong(
                previously_logged, generation,
                std::memory_order_acq_rel)) {
            try {
                std::ostringstream line;
                line << "native_stereo_startup_event: event=capture_resources_created"
                     << ";thread_id=" << GetCurrentThreadId()
                     << ";device=0x" << std::hex << device_id << std::dec
                     << ";generation=" << generation
                     << ";frame_sequence=" << frame_sequence
                     << ";slot=all"
                     << ";detail=" << state->capture.capture_description();
                LogLine(line.str());
            } catch (...) {
            }
        }
        const std::size_t eye_index = eye == cojvr::runtime::Eye::left ? 0U : 1U;
        if (!state->capture_source_logged[eye_index]) {
            state->capture_source_logged[eye_index] = true;
            LogLine(
                std::string("native_stereo_capture: status=source eye=") +
                (eye == cojvr::runtime::Eye::left ? "left " : "right ") +
                std::string(state->capture.capture_description()));
        }
        if (ShouldLogStereoTiming(frame_sequence)) {
            std::ostringstream line;
            line << "native_stereo_capture_timing: status=ok frame_sequence="
                 << frame_sequence << " eye="
                 << (eye == cojvr::runtime::Eye::left ? "left " : "right ")
                 << state->capture.capture_description();
            LogLine(line.str());
        }
    }
    if (!captured) {
        LogTransportFailure(frame_sequence, "capture", state->capture.last_error());
    }
    return captured;
}

bool SubmitStereoFrame(
    void* context,
    const std::uint64_t frame_sequence,
    const cojvr::runtime::PoseSample& render_hmd_pose,
    const cojvr::games::call_of_juarez::CameraStereoReticleOverlay& gameplay_reticle,
    const cojvr::runtime::StereoHudTextOverlay& hud_text) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return false;
    if (!state->capture_readback_enabled.load(std::memory_order_acquire)) return true;
    cojvr::backends::d3d9::StereoReticleOverlay capture_reticle{};
    capture_reticle.active = gameplay_reticle.active;
    capture_reticle.no_shoot = gameplay_reticle.no_shoot;
    capture_reticle.frame_sequence = gameplay_reticle.frame_sequence;
    for (std::size_t eye = 0; eye < capture_reticle.eyes.size(); ++eye) {
        capture_reticle.eyes[eye] = {
            .valid = gameplay_reticle.eyes[eye].valid,
            .u = gameplay_reticle.eyes[eye].u,
            .v = gameplay_reticle.eyes[eye].v,
        };
    }
    const bool accepted = state->capture.EndFrame(
        frame_sequence, render_hmd_pose.pose, render_hmd_pose.sequence, capture_reticle, hud_text);
    if (!accepted) {
        LogTransportFailure(frame_sequence, "fence", state->capture.last_error());
    } else if (ShouldLogStereoTiming(frame_sequence)) {
        std::ostringstream line;
        line << "native_stereo_frame_queue: status=ok frame_sequence="
             << frame_sequence << " transport="
             << (state->capture.gpu_resident_active()
                     ? "d3d9ex_shared_texture_ring"
                     : "classic_d3d9_locked_systemmem_fallback");
        LogLine(line.str());
    }
    return accepted;
}

void SetCaptureReadbackEnabled(void* context, const bool enabled) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return;
    const bool previous = state->capture_readback_enabled.exchange(
        enabled, std::memory_order_acq_rel);
    if (previous == enabled) return;
    LogLine(std::string("native_stereo_diagnostic_capture: status=") +
        (enabled ? "enabled" : "disabled") +
        ";readback=" + (enabled ? "active" : "bypassed") +
        ";presenter=repeat_last_frame");
}

bool QueryDiagnosticCounters(
    void* context,
    cojvr::games::call_of_juarez::CameraStereoDiagnosticCounters& counters) noexcept {
    counters = {};
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return false;
    try {
        const auto capture = state->capture.stats();
        const auto presenter = state->presenter.stats();
        counters.frames_fenced = capture.frames_fenced;
        counters.frames_collected = capture.frames_collected;
        counters.frames_uploaded = presenter.frames_uploaded;
        counters.new_submissions = presenter.new_frame_submissions;
        counters.repeat_submissions = presenter.repeated_frame_submissions;
        return true;
    } catch (...) {
        return false;
    }
}

bool StartStereoRuntime() noexcept {
    std::string manifest_path;
    try {
        const auto manifest = GameDirectory() / L"cojvr_openvr_input" / L"actions.json";
        const std::u8string manifest_utf8 = manifest.u8string();
        manifest_path.assign(
            reinterpret_cast<const char*>(manifest_utf8.data()), manifest_utf8.size());
    } catch (...) {
        LogLine("openvr_input: status=unavailable error=manifest_path_exception");
    }
    if (!g_stereo.presenter.Start(
            manifest_path, &PresenterLog, nullptr, &FlatUiPointer, nullptr)) {
        LogLine(
            "native_stereo_runtime: status=unavailable owner=presenter_thread error=" +
            g_stereo.presenter.last_error());
        return false;
    }
    if (!g_stereo.presenter.EyeViews(g_stereo.eyes)) {
        LogLine("native_stereo_runtime: status=eye_config_failed owner=presenter_thread");
        g_stereo.presenter.Stop();
        return false;
    }
    g_stereo.runtime_ready = true;
    try {
        std::ostringstream line;
        line << "native_stereo_runtime: status=started backend=openvr owner=presenter_thread"
             << " recommended_eye=" << g_stereo.eyes[0].width << 'x'
             << g_stereo.eyes[0].height
             << " left_eye_x=" << g_stereo.eyes[0].eye_to_head.position.x
             << " right_eye_x=" << g_stereo.eyes[1].eye_to_head.position.x
             << " pose_semantics=eye_to_head";
        LogLine(line.str());
    } catch (...) {
    }
    return true;
}

void Finalize() noexcept {
    if (g_finalization_started.exchange(true, std::memory_order_acq_rel)) return;
    LogLine("native_stereo_pre_exit: stage=begin boundary=CoJ.exe!DestroyGame");
    g_stereo.runtime_ready = false;
    StopPresentHookWorker();
    cojvr::games::call_of_juarez::ShutdownCameraProbe();
    const bool legacy_texture_restored =
        cojvr::games::call_of_juarez::RestoreD3D9ExLegacyTextureCompatibility();
    LogLine(std::string("native_stereo_legacy_texture_hook: status=") +
        (legacy_texture_restored ? "restored" : "incomplete"));
    const bool device_restored =
        cojvr::backends::d3d9::RestoreAllDeviceVtableHooks();
    LogLine(std::string("native_stereo_device_hook: status=") +
        (device_restored ? "restored" : "incomplete"));
    const bool swapchain_restored =
        cojvr::backends::d3d9::RestoreAllSwapChainVtableHooks();
    LogLine(std::string("native_stereo_swapchain_hook: status=") +
        (swapchain_restored ? "restored" : "incomplete"));
    if (g_stereo.factory) {
        const bool restored =
            cojvr::backends::d3d9::RestoreFactoryVtableHook(g_stereo.factory);
        LogLine(std::string("native_stereo_factory_hook: status=") +
            (restored ? "restored" : "incomplete"));
    }
    LogLine("native_stereo_shutdown: stage=presenter_begin");
    g_stereo.presenter.Stop();
    NeutralizeFlatUiPointer("presenter_stopped");
    g_stereo.flat_ui_select_retry = {};
    const auto presenter_stop_stats = g_stereo.presenter.stats();
    LogLine(std::string("native_stereo_presenter_stop: shutdown_complete=") +
        (presenter_stop_stats.shutdown_complete ? "true" : "false"));
    LogLine("native_stereo_shutdown: stage=presenter_end");
    LogLine("native_stereo_shutdown: stage=capture_begin");
    g_stereo.capture.Shutdown();
    g_stereo.flat_capture.Shutdown();
    LogLine("native_stereo_shutdown: stage=capture_end");
    try {
        const auto capture_stats = g_stereo.capture.stats();
        const auto presenter_stats = g_stereo.presenter.stats();
        std::ostringstream line;
        line << "native_stereo_transport_summary: frames_fenced="
             << capture_stats.frames_fenced
             << ";frames_collected=" << capture_stats.frames_collected
             << ";capture_ring_drops=" << capture_stats.frames_dropped_no_slot
             << ";capture_query_not_ready=" << capture_stats.query_not_ready
             << ";capture_query_flushes=" << capture_stats.query_flushes
             << ";shared_frames_published=" << capture_stats.shared_frames_published
             << ";cpu_fallback_frames=" << capture_stats.cpu_fallback_frames
             << ";fallback_activations=" << capture_stats.fallback_activations
             << ";consumer_releases=" << capture_stats.consumer_releases
             << ";capture_ring_depth=" << capture_stats.ring_depth
             << ";capture_ring_depth_peak=" << capture_stats.ring_depth_peak
             << ";mailbox_published=" << presenter_stats.mailbox.published
             << ";mailbox_replaced=" << presenter_stats.mailbox.replaced_pending
             << ";frames_uploaded=" << presenter_stats.frames_uploaded
             << ";new_submissions=" << presenter_stats.new_frame_submissions
             << ";repeat_submissions=" << presenter_stats.repeated_frame_submissions
             << ";shared_frames_copied=" << presenter_stats.shared_frames_copied
             << ";shared_resources_opened=" << presenter_stats.shared_resources_opened
             << ";shared_copy_fences_completed="
             << presenter_stats.shared_copy_fences_completed
             << ";shared_pending_copy_fences="
             << presenter_stats.shared_pending_copy_fences
             << ";shared_pending_copy_fences_peak="
             << presenter_stats.shared_pending_copy_fences_peak
             << ";shared_open_failures=" << presenter_stats.shared_open_failures
             << ";shared_copy_failures=" << presenter_stats.shared_copy_failures
             << ";submit_failures=" << presenter_stats.submit_failures;
        LogLine(line.str());
    } catch (...) {
    }
    LogLine("native_stereo_runtime: status=stopped");
    // These extra factory/device references remain deliberately process-lifetime.
    // Capture resources and consumer leases were released above while their owners
    // were alive. Windows reclaims only the retained roots at process exit.
    {
        std::lock_guard lock(g_stereo.device_mutex);
        g_stereo.device = nullptr;
    }
    g_stereo.factory = nullptr;
    LogLine("native_stereo_d3d9_refs: status=process_lifetime_released_by_os");
    const bool exit_hook_restored = cojvr::games::call_of_juarez::RestoreGameShutdownHook();
    LogLine(std::string("native_stereo_pre_exit_hook: status=") +
        (exit_hook_restored ? "restored" : "incomplete"));
    LogLine("native_stereo_pre_exit: stage=end boundary=CoJ.exe!DestroyGame");
    LogLine("run_end: run_id=" + g_run_id);
    g_finalization_completed.store(true, std::memory_order_release);
}

void BeforeGameDestroy(void*) noexcept { Finalize(); }

void ReportIncompletePreExitShutdown() noexcept {
    if (g_finalization_completed.load(std::memory_order_acquire)) return;
    // Do not use the shared logger mutex: a terminated presenter may own it.
    // Report failure without joining threads, COM calls or promoting run_end.
    constexpr char message[] =
        "native_stereo_shutdown: status=incomplete reason=pre_exit_boundary_not_completed\r\n";
    HANDLE log = CreateFileW(g_process_exit_log_path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) return;
    DWORD written{};
    (void)WriteFile(log, message, sizeof(message) - 1, &written, nullptr);
    CloseHandle(log);
}

void ObserveHudBoundary(void* context,
    const cojvr::games::call_of_juarez::CoJHudBoundaryEvent& event,
    const std::uint64_t frame_sequence) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return;
    try {
        std::ostringstream line;
        line << "native_hud_boundary: stage="
             << cojvr::games::call_of_juarez::CoJHudBoundaryStageName(event.stage)
             << ";pass_sequence=" << event.pass_sequence
             << ";frame_sequence=" << frame_sequence
             << ";present_sequence=" << state->present_telemetry_sequence.load()
             << ";thread_id=" << GetCurrentThreadId()
             << ";owner=0x" << std::hex << reinterpret_cast<std::uintptr_t>(event.owner)
             << std::dec << ";option=" << event.option;
        // Query only on the observed native eye-render thread. Never wait for
        // the device owner, redirect a target, or replay native sprite traversal.
        std::unique_lock lock(state->device_mutex, std::try_to_lock);
        if (!lock.owns_lock() || !state->device || GetCurrentThreadId() !=
            state->native_render_thread.load(std::memory_order_acquire)) {
            line << ";target_observation=unavailable";
        } else {
            Microsoft::WRL::ComPtr<IDirect3DSurface9> target;
            D3DSURFACE_DESC desc{};
            D3DVIEWPORT9 viewport{};
            const HRESULT rt_result = state->device->GetRenderTarget(0, &target);
            const HRESULT desc_result = target ? target->GetDesc(&desc) : E_FAIL;
            const HRESULT viewport_result = state->device->GetViewport(&viewport);
            line << ";device=0x" << std::hex << reinterpret_cast<std::uintptr_t>(state->device)
                 << ";rt=0x" << reinterpret_cast<std::uintptr_t>(target.Get())
                 << ";rt_hr=0x" << static_cast<unsigned long>(rt_result)
                 << ";desc_hr=0x" << static_cast<unsigned long>(desc_result)
                 << ";viewport_hr=0x" << static_cast<unsigned long>(viewport_result)
                 << std::dec << ";generation=" << state->device_generation.load()
                 << ";size=" << desc.Width << ',' << desc.Height << ";format=" << desc.Format
                 << ";viewport=" << viewport.X << ',' << viewport.Y << ','
                 << viewport.Width << ',' << viewport.Height;
            const std::array<std::pair<D3DRENDERSTATETYPE, const char*>, 6> states{{
                {D3DRS_ALPHABLENDENABLE, "alpha_blend"}, {D3DRS_SRCBLEND, "src_blend"},
                {D3DRS_DESTBLEND, "dst_blend"}, {D3DRS_COLORWRITEENABLE, "color_write"},
                {D3DRS_SEPARATEALPHABLENDENABLE, "separate_alpha"}, {D3DRS_ZENABLE, "depth"}}};
            for (const auto& [key, name] : states) {
                DWORD value = 0;
                if (SUCCEEDED(state->device->GetRenderState(key, &value)))
                    line << ';' << name << '=' << value;
                else line << ';' << name << "=unavailable";
            }
        }
        if (lock.owns_lock()) lock.unlock();
        LogLine(line.str());
    } catch (...) { LogLine("native_hud_boundary: status=observation_exception"); }
}

void EnsureStarted() noexcept {
    try {
        std::call_once(g_start_once, [] {
            const std::string run_id = ReadRunManifestField("runId");
            const std::string build_manifest_id = ReadRunManifestField("buildManifestId");
            if (!run_id.empty()) g_run_id = run_id;
            std::ostringstream start;
            start << "run_start: run_id=" << g_run_id
                  << " build_manifest_id="
                  << (build_manifest_id.empty() ? "unbound" : build_manifest_id)
                  << " pid=" << GetCurrentProcessId();
            LogLine(start.str());

            lstrcpynW(g_process_exit_log_path, LogPath().c_str(), 32768);
            std::atexit(ReportIncompletePreExitShutdown);
            const auto exit_hook = cojvr::games::call_of_juarez::InstallCurrentGameShutdownHook(
                &BeforeGameDestroy, nullptr);
            LogLine(std::string("native_stereo_pre_exit_hook: status=") +
                cojvr::games::call_of_juarez::GameShutdownHookStatusName(exit_hook));
            const bool exit_boundary_ready = exit_hook ==
                cojvr::games::call_of_juarez::GameShutdownHookStatus::installed;

            cojvr::games::call_of_juarez::CameraStereoRuntimeCallbacks stereo_callbacks{};
            if (exit_boundary_ready && StartStereoRuntime()) {
                stereo_callbacks.context = &g_stereo;
                stereo_callbacks.begin_frame = &BeginStereoFrame;
                stereo_callbacks.capture_eye = &CaptureStereoEye;
                stereo_callbacks.submit_frame = &SubmitStereoFrame;
                stereo_callbacks.set_capture_readback_enabled = &SetCaptureReadbackEnabled;
                stereo_callbacks.diagnostic_counters = &QueryDiagnosticCounters;
                stereo_callbacks.observe_hud_boundary = &ObserveHudBoundary;
            }

            const auto control_path = GameDirectory() / L"cojvr-camera-control.json";
            const auto status = cojvr::games::call_of_juarez::InitializeCameraProbe(
                control_path, &CameraEvent, nullptr, stereo_callbacks);
            LogLine(
                std::string("camera_probe_bootstrap: status=") +
                cojvr::games::call_of_juarez::CameraProbeInstallStatusName(status) +
                " system_d3d9=" +
                (cojvr::backends::d3d9::IsExpectedSystemD3D9Module()
                    ? "expected" : "unexpected"));
        });
    } catch (...) {
        LogLine("native_stereo_bootstrap: status=exception");
    }
}

void ObserveFactory(IDirect3D9* factory) noexcept {
    if (!factory) return;
    try {
        if (!g_stereo.factory) {
            factory->AddRef();
            g_stereo.factory = factory;
        }
        const cojvr::backends::d3d9::FactoryHookCallbacks callbacks{
            .prefer_ex_device = [&] {
                IDirect3D9Ex* ex = nullptr;
                const bool available = SUCCEEDED(factory->QueryInterface(
                    IID_IDirect3D9Ex, reinterpret_cast<void**>(&ex))) && ex;
                if (ex) ex->Release();
                return available;
            }(),
            .before_create_device = &BeforeCreateDevice,
            .after_create_device = &AfterCreateDevice,
            .com_trace = &FactoryComTrace,
        };
        const bool installed =
            cojvr::backends::d3d9::InstallFactoryVtableHook(factory, callbacks);
        LogLine(std::string("native_stereo_factory_hook: status=") +
            (installed ? "installed" : "failed"));
    } catch (...) {
        LogLine("native_stereo_factory_hook: status=exception");
    }
}

} // namespace

extern "C" IDirect3D9* WINAPI Direct3DCreate9(const UINT sdk_version) {
    EnsureStarted();
    const auto create = cojvr::backends::d3d9::SystemDirect3DCreate9();
    IDirect3D9* classic_factory = create ? create(sdk_version) : nullptr;

    const auto create_ex = cojvr::backends::d3d9::SystemDirect3DCreate9Ex();
    IDirect3D9Ex* ex_factory = nullptr;
    const HRESULT ex_result = create_ex
        ? create_ex(sdk_version, &ex_factory)
        : E_NOINTERFACE;
    {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=factory_ex_created"
             << ";thread_id=" << GetCurrentThreadId()
             << ";factory=0x" << std::hex
             << reinterpret_cast<std::uintptr_t>(ex_factory)
             << ";hr=0x" << static_cast<unsigned long>(ex_result) << std::dec;
        LogLine(line.str());
    }
    if (SUCCEEDED(ex_result) && ex_factory) {
        if (classic_factory) classic_factory->Release();
        LogLine(
            "native_stereo_d3d9_factory: status=d3d9ex_primary "
            "fallback=classic_create_device transport=d3d9ex_shared_texture_ring");
        ObserveFactory(ex_factory);
        return ex_factory;
    } else {
        LogLine(
            "native_stereo_d3d9_factory: status=classic_fallback "
            "reason=Direct3DCreate9Ex_unavailable_or_failed");
    }

    ObserveFactory(classic_factory);
    return classic_factory;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
