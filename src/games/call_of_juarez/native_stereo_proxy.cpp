#include "games/call_of_juarez/camera_probe.hpp"
#include "games/call_of_juarez/d3d9ex_legacy_resource_compat.hpp"
#include "games/call_of_juarez/java_player_bridge.hpp"

#include "backends/d3d9/d3d9_stereo_capture.hpp"
#include "backends/d3d9/d3d9ex_forwarder.hpp"
#include "backends/d3d9/device_vtable_hook.hpp"
#include "backends/d3d9/factory_vtable_hook.hpp"
#include "backends/d3d9/swapchain_vtable_hook.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "backends/openvr/stereo_presenter.hpp"
#include "runtime/log.hpp"

#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

struct NativeStereoState {
    cojvr::backends::d3d9::D3D9StereoCapture capture;
    cojvr::backends::d3d9::D3D9StereoCapture flat_capture;
    cojvr::backends::openvr::OpenVrStereoPresenter presenter;
    std::array<cojvr::runtime::EyeView, 2> eyes{};
    std::mutex device_mutex;
    IDirect3D9* factory = nullptr;
    IDirect3DDevice9* device = nullptr;
    std::atomic_uint64_t device_generation{0};
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
    std::uint32_t flat_ui_pointer_x = 0;
    std::uint32_t flat_ui_pointer_y = 0;
    bool flat_ui_pointer_game_route_logged = false;
    bool flat_ui_pointer_game_error_logged = false;
    bool flat_loading_resume_pending = false;
    bool flat_ui_select_release_required = false;
    cojvr::games::call_of_juarez::CoJUiSelectRetryState flat_ui_select_retry{};
    std::array<bool, 2> capture_source_logged{};
    std::atomic_bool capture_readback_enabled{true};
    bool runtime_ready = false;
};

NativeStereoState g_stereo;

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
                   << ";route=win32_cursor_position";
            CameraEvent("flat_ui_pointer", "neutralized", detail.str().c_str());
        }
        g_stereo.flat_ui_pointer_active = false;
        g_stereo.flat_ui_pointer_x = 0;
        g_stereo.flat_ui_pointer_y = 0;
        g_stereo.flat_ui_pointer_game_route_logged = false;
        g_stereo.flat_ui_pointer_game_error_logged = false;
    } catch (...) {
    }
}

bool SendAbsoluteMouseMove(const POINT screen) noexcept {
    const LONG left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const LONG top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const LONG width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const LONG height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (width <= 1 || height <= 1) return false;

    const auto normalize = [](const LONG value, const LONG origin, const LONG extent) noexcept {
        const double relative = static_cast<double>(value - origin);
        const double scaled = relative * 65535.0 / static_cast<double>(extent - 1);
        return static_cast<LONG>(std::lround(std::max(0.0, std::min(65535.0, scaled))));
    };

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = normalize(screen.x, left, width);
    input.mi.dy = normalize(screen.y, top, height);
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    return SendInput(1, &input, sizeof(input)) == 1;
}

void FlatUiPointer(
    void*,
    const cojvr::backends::openvr::FlatUiPointerSample& sample) noexcept {
    try {
        const HWND window = reinterpret_cast<HWND>(
            g_stereo.game_window.load(std::memory_order_acquire));
        HWND root = window ? GetAncestor(window, GA_ROOT) : nullptr;
        if (!root) root = window;
        const bool foreground = root && GetForegroundWindow() == root;
        if (!sample.active || !foreground ||
            sample.source_width == 0 || sample.source_height == 0) {
            NeutralizeFlatUiPointer(
                !sample.active ? "ray_inactive" :
                (!foreground ? "game_not_foreground" : "invalid_source"));
            return;
        }

        RECT client{};
        if (!GetClientRect(root, &client)) {
            NeutralizeFlatUiPointer("client_rect_failed");
            return;
        }
        const LONG width = client.right - client.left;
        const LONG height = client.bottom - client.top;
        if (width <= 1 || height <= 1) {
            NeutralizeFlatUiPointer("invalid_client_extent");
            return;
        }
        const POINT client_pointer{
            static_cast<LONG>(std::lround(sample.u * static_cast<float>(width - 1))),
            static_cast<LONG>(std::lround(sample.v * static_cast<float>(height - 1))),
        };
        POINT screen = client_pointer;
        if (!ClientToScreen(root, &screen) || !SetCursorPos(screen.x, screen.y) ||
            !SendAbsoluteMouseMove(screen)) {
            NeutralizeFlatUiPointer("cursor_move_failed");
            return;
        }
        // Chrome Engine's UI hit-testing also consumes the real mouse-input
        // stream. A posted WM_MOUSEMOVE plus cursor relocation still required a
        // physical mouse nudge in the 20260921 headset clip, so publish an
        // absolute SendInput move before synchronizing the window message.
        if (!PostMessageW(
                root, WM_MOUSEMOVE, 0,
                MAKELPARAM(client_pointer.x, client_pointer.y))) {
            NeutralizeFlatUiPointer("mouse_move_post_failed");
            return;
        }

        std::lock_guard lock(g_stereo.flat_ui_mutex);
        if (!g_stereo.flat_ui_pointer_active) {
            std::ostringstream detail;
            detail << "active=true;hand=" << (sample.using_left_hand ? "left" : "right")
                   << ";source=" << sample.source_width << 'x' << sample.source_height
                   << ";route=win32_cursor_position+SendInput_absolute+WM_MOUSEMOVE";
            CameraEvent("flat_ui_pointer", "active", detail.str().c_str());
        }
        g_stereo.flat_ui_pointer_active = true;
        g_stereo.flat_ui_pointer_x = sample.pixel_x;
        g_stereo.flat_ui_pointer_y = sample.pixel_y;
    } catch (...) {
        NeutralizeFlatUiPointer("exception");
    }
}

bool FlatUiPointerActive() noexcept {
    try {
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        return g_stereo.flat_ui_pointer_active;
    } catch (...) {
        return false;
    }
}

bool ReadFlatUiPointer(std::uint32_t& pixel_x, std::uint32_t& pixel_y) noexcept {
    try {
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        if (!g_stereo.flat_ui_pointer_active) return false;
        pixel_x = g_stereo.flat_ui_pointer_x;
        pixel_y = g_stereo.flat_ui_pointer_y;
        return true;
    } catch (...) {
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

void BeforePresentBoundary(IDirect3DDevice9* device, const char* source) noexcept {
    if (!device || !g_stereo.runtime_ready) return;
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

        const bool pointer_active = FlatUiPointerActive();
        // Once the projected Sense ray has entered the real Windows mouse
        // stream, let that stream be the sole owner of CoJ's menu cursor.  The
        // 20260921 physical run showed that simultaneously forcing UICursorGame
        // through JNI made hover/select oscillate even though SendInput itself
        // finally reached the shipped menu input path.

        if (tracking.ui_back_pressed) {
            cojvr::games::call_of_juarez::CoJUiDispatchRoute back_route =
                cojvr::games::call_of_juarez::CoJUiDispatchRoute::none;
            std::string back_error;
            const bool backed = cojvr::games::call_of_juarez::DispatchCameraUiBackPress(
                &back_error, &back_route);
            std::ostringstream detail;
            detail << "source=right_circle;route="
                   << cojvr::games::call_of_juarez::CoJUiBackDispatchRouteName(back_route);
            if (!back_error.empty()) detail << ";detail=" << back_error;
            CameraEvent("flat_ui_back", backed ? "applied" : "failed", detail.str().c_str());
        }

        if (g_stereo.flat_ui_select_release_required && timer_valid && !timer_frozen &&
            !tracking.ui_select_left && !tracking.ui_select_right && !tracking.ui_accept) {
            g_stereo.flat_ui_select_release_required = false;
            g_stereo.flat_loading_resume_pending = false;
            CameraEvent(
                "blocking_ui_resume",
                "observed",
                "game_timer_valid=true;game_timer_frozen=false;trigger_released=true;route=LawmanModule.TimerStart");
        }

        const bool ui_select_pressed =
            tracking.ui_select_left_pressed || tracking.ui_select_right_pressed ||
            tracking.ui_accept_pressed;
        const bool ui_select_held =
            tracking.ui_select_left || tracking.ui_select_right || tracking.ui_accept;
        g_stereo.flat_ui_select_retry.Observe(ui_select_pressed, ui_select_held);
        const bool select_allowed = pointer_active || (timer_valid && timer_frozen);
        if (g_stereo.flat_ui_select_retry.ShouldDispatch(
                pointer_active, timer_valid && timer_frozen)) {
            bool current_ui_is_loading = false;
            bool paused_hint_dismissed = false;
            cojvr::games::call_of_juarez::CoJUiDispatchRoute ui_route =
                cojvr::games::call_of_juarez::CoJUiDispatchRoute::none;
            std::string ui_error;
            const bool dispatched = select_allowed &&
                cojvr::games::call_of_juarez::DispatchCameraUiSelectPress(
                    false, &current_ui_is_loading, &ui_error,
                    &paused_hint_dismissed, &ui_route);
            g_stereo.flat_ui_select_retry.Complete(dispatched);
            if (ui_select_pressed || dispatched) {
                std::ostringstream detail;
                detail << "source=";
                if (tracking.ui_accept_pressed ||
                    (!tracking.ui_select_left_pressed && !tracking.ui_select_right_pressed &&
                     tracking.ui_accept)) {
                    detail << "right_cross";
                } else {
                    detail
                       << ((tracking.ui_select_left_pressed ||
                            (!tracking.ui_select_right_pressed && tracking.ui_select_left))
                               ? "left_l2"
                               : "right_r2");
                }
                detail
                       << ";pointer_active=" << (pointer_active ? "true" : "false")
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
                                << (tracking.ui_accept_pressed ? "right_cross" :
                                    (tracking.ui_select_left_pressed ? "left_l2" : "right_r2"))
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
    D3DPRESENT_PARAMETERS*,
    const HRESULT result) noexcept {
    try {
        std::uint64_t generation =
            g_stereo.device_generation.load(std::memory_order_acquire);
        if (SUCCEEDED(result)) {
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
                 << static_cast<unsigned>(presentation_parameters->SwapEffect);
        }
        LogLine(line.str());
    } catch (...) {
    }
}

void AfterCreateDevice(
    IDirect3D9*,
    UINT,
    D3DDEVTYPE,
    HWND focus_window,
    DWORD,
    D3DPRESENT_PARAMETERS*,
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
    if (FAILED(result) || !observed) return;
    try {
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
                });
        const cojvr::backends::d3d9::DeviceHookCallbacks device_callbacks{
            .before_present = &BeforeDevicePresent,
            .after_present = &AfterDevicePresent,
            .before_reset = &BeforeDeviceReset,
            .after_reset = &AfterDeviceReset,
        };
        const auto device_hook =
            cojvr::backends::d3d9::InstallDeviceVtableHookDetailed(
                replacement, device_callbacks);
        const cojvr::backends::d3d9::SwapChainHookCallbacks swapchain_callbacks{
            .before_present = &BeforeSwapChainPresent,
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
    if (!state || !state->runtime_ready) return false;
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
    sample.recenter_requested = tracking.recenter_requested;
    sample.ui_select_left = tracking.ui_select_left;
    sample.ui_select_right = tracking.ui_select_right;
    sample.ui_select_left_pressed = tracking.ui_select_left_pressed;
    sample.ui_select_right_pressed = tracking.ui_select_right_pressed;
    sample.ui_accept = tracking.ui_accept;
    sample.ui_back = tracking.ui_back;
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
    const cojvr::runtime::PoseSample& render_hmd_pose) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return false;
    if (!state->capture_readback_enabled.load(std::memory_order_acquire)) return true;
    const bool accepted = state->capture.EndFrame(
        frame_sequence, render_hmd_pose.pose, render_hmd_pose.sequence);
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
    // These COM references are deliberately process-lifetime. The live game has shown
    // that releasing the retained D3D9 device during CRT teardown can terminate finalization
    // after hook restoration but before OpenVR shutdown/run_end. Windows reclaims the
    // remaining process references immediately after atexit completes.
    {
        std::lock_guard lock(g_stereo.device_mutex);
        g_stereo.device = nullptr;
    }
    g_stereo.factory = nullptr;
    LogLine("native_stereo_d3d9_refs: status=process_lifetime_released_by_os");
    LogLine("run_end: run_id=" + g_run_id);
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

            cojvr::games::call_of_juarez::CameraStereoRuntimeCallbacks stereo_callbacks{};
            if (StartStereoRuntime()) {
                stereo_callbacks.context = &g_stereo;
                stereo_callbacks.begin_frame = &BeginStereoFrame;
                stereo_callbacks.capture_eye = &CaptureStereoEye;
                stereo_callbacks.submit_frame = &SubmitStereoFrame;
                stereo_callbacks.set_capture_readback_enabled = &SetCaptureReadbackEnabled;
                stereo_callbacks.diagnostic_counters = &QueryDiagnosticCounters;
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
            std::atexit(Finalize);
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
            .before_create_device = &BeforeCreateDevice,
            .after_create_device = &AfterCreateDevice,
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
        try {
            auto* forwarder = new cojvr::backends::d3d9::Direct3D9ExForwarder(
                ex_factory, classic_factory);
            {
                std::ostringstream line;
                line << "native_stereo_startup_event: event=forwarder_created"
                     << ";thread_id=" << GetCurrentThreadId()
                     << ";factory=0x" << std::hex
                     << reinterpret_cast<std::uintptr_t>(forwarder)
                     << ";inner_ex=0x" << reinterpret_cast<std::uintptr_t>(ex_factory)
                     << ";classic_fallback=0x"
                     << reinterpret_cast<std::uintptr_t>(classic_factory) << std::dec;
                LogLine(line.str());
            }
            ex_factory->Release();
            if (classic_factory) classic_factory->Release();
            LogLine(
                "native_stereo_d3d9_factory: status=d3d9ex_primary "
                "fallback=classic_create_device transport=d3d9ex_shared_texture_ring");
            ObserveFactory(forwarder);
            return forwarder;
        } catch (...) {
            ex_factory->Release();
            LogLine(
                "native_stereo_d3d9_factory: status=classic_fallback "
                "reason=d3d9ex_forwarder_allocation_failed");
        }
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
