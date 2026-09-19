#include "games/call_of_juarez/camera_probe.hpp"

#include "backends/d3d9/d3d9_stereo_capture.hpp"
#include "backends/d3d9/factory_vtable_hook.hpp"
#include "backends/d3d9/swapchain_vtable_hook.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "backends/openvr/stereo_presenter.hpp"
#include "runtime/log.hpp"

#include <windows.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
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
    std::atomic_uint64_t transport_sequence{0};
    std::atomic_uint64_t flat_capture_sequence{0};
    std::atomic_uint64_t last_native_stereo_tick_ms{0};
    std::atomic_bool flat_capture_active{false};
    std::atomic_uint64_t last_error_frame{0};
    std::atomic_uintptr_t game_window{0};
    std::mutex flat_ui_mutex;
    bool flat_ui_pointer_active = false;
    bool flat_ui_click_down = false;
    std::array<bool, 2> capture_source_logged{};
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

void CameraEvent(const char* event, const char* result, const char* detail) noexcept;

void PresenterLog(void*, const std::string_view line) noexcept { LogLine(line); }

bool InjectFlatUiMouseButton(const bool down) noexcept {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    return SendInput(1, &input, sizeof(input)) == 1U;
}

void NeutralizeFlatUiPointer(const char* reason) noexcept {
    try {
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        if (g_stereo.flat_ui_click_down) {
            const bool released = InjectFlatUiMouseButton(false);
            if (released) g_stereo.flat_ui_click_down = false;
            std::ostringstream detail;
            detail << "active=false;click_release=" << (released ? "sent" : "failed")
                   << ";reason=" << (reason ? reason : "unknown")
                   << ";route=win32_menu_mouse";
            CameraEvent("flat_ui_pointer", released ? "neutralized" : "release_failed",
                detail.str().c_str());
        }
        g_stereo.flat_ui_pointer_active = false;
    } catch (...) {
    }
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
        POINT screen{
            static_cast<LONG>(std::lround(sample.u * static_cast<float>(width - 1))),
            static_cast<LONG>(std::lround(sample.v * static_cast<float>(height - 1))),
        };
        if (!ClientToScreen(root, &screen) || !SetCursorPos(screen.x, screen.y)) {
            NeutralizeFlatUiPointer("cursor_move_failed");
            return;
        }

        std::lock_guard lock(g_stereo.flat_ui_mutex);
        if (!g_stereo.flat_ui_pointer_active) {
            std::ostringstream detail;
            detail << "active=true;hand=" << (sample.using_left_hand ? "left" : "right")
                   << ";source=" << sample.source_width << 'x' << sample.source_height
                   << ";route=win32_menu_mouse";
            CameraEvent("flat_ui_pointer", "active", detail.str().c_str());
        }
        g_stereo.flat_ui_pointer_active = true;
        if (sample.select_down != g_stereo.flat_ui_click_down) {
            const bool sent = InjectFlatUiMouseButton(sample.select_down);
            if (sent) g_stereo.flat_ui_click_down = sample.select_down;
            std::ostringstream detail;
            detail << "button=left;state=" << (sample.select_down ? "down" : "up")
                   << ";pixel=" << sample.pixel_x << ',' << sample.pixel_y
                   << ";hand=" << (sample.using_left_hand ? "left" : "right")
                   << ";route=win32_menu_mouse";
            CameraEvent("flat_ui_click", sent ? "applied" : "failed", detail.str().c_str());
        }
    } catch (...) {
        NeutralizeFlatUiPointer("exception");
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
        if (!state.presenter.Publish(std::move(frame))) {
            LogTransportFailure(
                capture_sequence, source,
                "presenter mailbox rejected completed CPU frame");
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

void BeforeSwapChainPresent(IDirect3DDevice9* device) noexcept {
    if (!device || !g_stereo.runtime_ready) return;
    try {
        constexpr std::uint64_t kStereoFreshnessMs = 250;
        const std::uint64_t now_ms = GetTickCount64();
        const std::uint64_t last_stereo_ms =
            g_stereo.last_native_stereo_tick_ms.load(std::memory_order_acquire);
        if (last_stereo_ms != 0 && now_ms >= last_stereo_ms &&
            now_ms - last_stereo_ms < kStereoFreshnessMs) {
            g_stereo.flat_capture_active.store(false, std::memory_order_release);
            return;
        }

        if (!g_stereo.flat_capture_active.exchange(true, std::memory_order_acq_rel)) {
            g_stereo.flat_capture.InvalidateResources();
            LogLine("native_stereo_flat_theater: status=entered source=swapchain_present");
        }

        cojvr::backends::d3d9::StereoCpuFrame completed{};
        if (g_stereo.flat_capture.TryCollectReady(completed)) {
            const std::uint64_t sequence = completed.capture_sequence;
            if (PublishCapturedFrame(
                    g_stereo, std::move(completed),
                    cojvr::backends::d3d9::FramePresentationMode::flat_theater,
                    "flat_publish") &&
                ShouldLogStereoTiming(sequence)) {
                LogLine(
                    "native_stereo_flat_theater: status=published " +
                    std::string(g_stereo.flat_capture.collect_description()));
            }
        }

        // The startup/menu fallback does not need a full 60/90 Hz CPU readback.
        // Capture every second Present; the presenter repeats the latest frame
        // at compositor cadence between updates.
        const std::uint64_t present_sequence =
            g_stereo.flat_capture_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
        if ((present_sequence & 1U) != 0U) return;

        cojvr::backends::openvr::OpenVrTrackingSample tracking{};
        if (!g_stereo.presenter.LatestTracking(tracking) ||
            !tracking.pose.orientation_valid || !tracking.pose.position_valid ||
            tracking.sequence == 0) {
            return;
        }

        Microsoft::WRL::ComPtr<IDirect3DSurface9> back_buffer;
        const HRESULT back_buffer_result =
            device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
        if (FAILED(back_buffer_result) || !back_buffer) return;
        const std::uint64_t generation =
            g_stereo.device_generation.load(std::memory_order_acquire);
        if (generation == 0) return;
        if (!g_stereo.flat_capture.CaptureEyeSurface(
                device, back_buffer.Get(), cojvr::runtime::Eye::left,
                present_sequence, generation) ||
            !g_stereo.flat_capture.CaptureEyeSurface(
                device, back_buffer.Get(), cojvr::runtime::Eye::right,
                present_sequence, generation) ||
            !g_stereo.flat_capture.EndFrame(
                present_sequence, tracking.pose, tracking.sequence)) {
            LogTransportFailure(
                present_sequence, "flat_capture", g_stereo.flat_capture.last_error());
            return;
        }
        if (ShouldLogStereoTiming(present_sequence)) {
            LogLine(
                "native_stereo_flat_theater: status=captured " +
                std::string(g_stereo.flat_capture.capture_description()));
        }
    } catch (...) {
        LogLine("native_stereo_flat_theater: status=exception");
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
    if (FAILED(result) || !returned_device || !*returned_device) return;
    try {
        IDirect3DDevice9* replacement = *returned_device;
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
        g_stereo.flat_capture_active.store(false, std::memory_order_release);
        g_stereo.last_native_stereo_tick_ms.store(0, std::memory_order_release);
        if (previous) previous->Release();
        const cojvr::backends::d3d9::SwapChainHookCallbacks swapchain_callbacks{
            .before_present = &BeforeSwapChainPresent,
        };
        const auto swapchain_hook =
            cojvr::backends::d3d9::InstallSwapChainVtableHookDetailed(
                replacement, swapchain_callbacks);
        std::ostringstream line;
        line << "native_stereo_device: status=observed device=0x" << std::hex
             << reinterpret_cast<std::uintptr_t>(replacement) << std::dec
             << " generation=" << generation
             << " swapchain_present_hook="
             << ((swapchain_hook.result == cojvr::backends::d3d9::HookRegistryResult::Installed ||
                     swapchain_hook.result == cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled)
                     ? "installed"
                     : "failed");
        LogLine(line.str());
    } catch (...) {
    }
}

bool BeginStereoFrame(
    void* context,
    cojvr::games::call_of_juarez::CameraStereoFrameSample& sample) noexcept {
    sample = {};
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state || !state->runtime_ready) return false;
    state->last_native_stereo_tick_ms.store(GetTickCount64(), std::memory_order_release);
    if (state->flat_capture_active.exchange(false, std::memory_order_acq_rel)) {
        LogLine("native_stereo_flat_theater: status=leaving reason=native_stereo_active");
    }

    cojvr::backends::d3d9::StereoCpuFrame completed{};
    if (state->capture.TryCollectReady(completed)) {
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

    cojvr::backends::openvr::OpenVrTrackingSample tracking{};
    if (!state->presenter.LatestTracking(tracking) ||
        !tracking.pose.orientation_valid || !tracking.pose.position_valid ||
        tracking.sequence == 0) {
        return false;
    }
    sample.hmd_pose.pose = tracking.pose;
    sample.hmd_pose.sequence = tracking.sequence;
    sample.left_controller = tracking.left_controller;
    sample.right_controller = tracking.right_controller;
    sample.left_aim = tracking.left_aim;
    sample.right_aim = tracking.right_aim;
    sample.gameplay = tracking.gameplay;
    sample.recenter_requested = tracking.recenter_requested;
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
    IDirect3DDevice9* device = RetainCurrentDevice();
    if (!device) {
        LogTransportFailure(frame_sequence, "capture", "D3D9 device unavailable");
        return false;
    }
    const std::uint64_t generation =
        state->device_generation.load(std::memory_order_acquire);
    const bool captured = state->capture.CaptureEye(
        device, eye, frame_sequence, generation);
    device->Release();
    if (captured) {
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
    const bool accepted = state->capture.EndFrame(
        frame_sequence, render_hmd_pose.pose, render_hmd_pose.sequence);
    if (!accepted) {
        LogTransportFailure(frame_sequence, "fence", state->capture.last_error());
    } else if (ShouldLogStereoTiming(frame_sequence)) {
        std::ostringstream line;
        line << "native_stereo_frame_queue: status=ok frame_sequence="
             << frame_sequence << " transport=deferred_d3d9_ring_cpu_mailbox";
        LogLine(line.str());
    }
    return accepted;
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
    cojvr::games::call_of_juarez::ShutdownCameraProbe();
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
    LogLine("native_stereo_shutdown: stage=capture_begin");
    g_stereo.capture.Shutdown();
    g_stereo.flat_capture.Shutdown();
    LogLine("native_stereo_shutdown: stage=capture_end");
    LogLine("native_stereo_shutdown: stage=presenter_begin");
    g_stereo.presenter.Stop();
    NeutralizeFlatUiPointer("presenter_stopped");
    const auto presenter_stop_stats = g_stereo.presenter.stats();
    LogLine(std::string("native_stereo_presenter_stop: shutdown_complete=") +
        (presenter_stop_stats.shutdown_complete ? "true" : "false"));
    LogLine("native_stereo_shutdown: stage=presenter_end");
    g_stereo.runtime_ready = false;
    try {
        const auto capture_stats = g_stereo.capture.stats();
        const auto presenter_stats = g_stereo.presenter.stats();
        std::ostringstream line;
        line << "native_stereo_transport_summary: frames_fenced="
             << capture_stats.frames_fenced
             << ";frames_collected=" << capture_stats.frames_collected
             << ";capture_ring_drops=" << capture_stats.frames_dropped_no_slot
             << ";mailbox_published=" << presenter_stats.mailbox.published
             << ";mailbox_replaced=" << presenter_stats.mailbox.replaced_pending
             << ";frames_uploaded=" << presenter_stats.frames_uploaded
             << ";new_submissions=" << presenter_stats.new_frame_submissions
             << ";repeat_submissions=" << presenter_stats.repeated_frame_submissions
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
    IDirect3D9* factory = create ? create(sdk_version) : nullptr;
    ObserveFactory(factory);
    return factory;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
