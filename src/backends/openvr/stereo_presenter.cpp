#include "backends/openvr/stereo_presenter.hpp"

#include "backends/d3d9/content_hash.hpp"
#include "backends/openvr/d3d11_compositor.hpp"
#include "backends/openvr/d3d11_session.hpp"
#include "backends/openvr/presentation_cadence.hpp"
#include "runtime/openvr_runtime.hpp"
#include "runtime/vr_math.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace cojvr::backends::openvr {
namespace {

using Microsoft::WRL::ComPtr;

double MillisecondsBetween(
    const std::chrono::steady_clock::time_point begin,
    const std::chrono::steady_clock::time_point end) noexcept {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

bool ShouldLogSequence(const std::uint64_t sequence) noexcept {
    return sequence > 0 && (sequence <= 8 || (sequence % 90) == 0);
}

const char* LifecycleName(const runtime::OpenVrLifecycleState state) noexcept {
    switch (state) {
    case runtime::OpenVrLifecycleState::idle: return "idle";
    case runtime::OpenVrLifecycleState::initializing: return "initializing";
    case runtime::OpenVrLifecycleState::ready: return "ready";
    case runtime::OpenVrLifecycleState::shutdown_requested: return "shutdown_requested";
    case runtime::OpenVrLifecycleState::shutdown_complete: return "shutdown_complete";
    case runtime::OpenVrLifecycleState::failed: return "failed";
    }
    return "unknown";
}

bool SameRuntimeState(
    const runtime::OpenVrRuntimeState& left,
    const runtime::OpenVrRuntimeState& right) noexcept {
    return left.lifecycle == right.lifecycle &&
        left.initialized == right.initialized &&
        left.connected == right.connected &&
        left.focused == right.focused &&
        left.tracking_valid == right.tracking_valid &&
        left.presenting == right.presenting &&
        left.shutdown_requested == right.shutdown_requested;
}

double PoseAgeMilliseconds(const std::chrono::steady_clock::time_point capture_time) noexcept {
    if (capture_time == std::chrono::steady_clock::time_point{}) return 0.0;
    return MillisecondsBetween(capture_time, std::chrono::steady_clock::now());
}

std::uint32_t FlatTextureExtent(
    const std::uint32_t source_extent,
    const std::uint32_t recommended_extent) noexcept {
    return std::max(
        recommended_extent,
        static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(source_extent) * 135U) / 100U));
}

} // namespace

struct OpenVrStereoPresenter::Impl {
    d3d9::FrameMailbox mailbox;
    std::thread worker;
    std::atomic_bool stop_requested{false};
    std::atomic_bool running{false};
    std::atomic_bool input_ready{false};

    mutable std::mutex init_mutex;
    std::condition_variable init_cv;
    std::atomic_bool init_done{false};
    bool init_ok = false;
    std::array<runtime::EyeView, 2> eyes{};

    mutable std::mutex tracking_mutex;
    runtime::Pose latest_pose{};
    runtime::Pose latest_left_controller{};
    runtime::Pose latest_right_controller{};
    runtime::Pose latest_left_aim{};
    runtime::Pose latest_right_aim{};
    bool latest_left_handgrip_active = false;
    bool latest_right_handgrip_active = false;
    bool latest_left_aim_active = false;
    bool latest_right_aim_active = false;
    runtime::GameplayInputState latest_gameplay{};
    std::uint64_t pose_sequence = 0;
    bool recenter_pending = false;
    bool latest_ui_select_left = false;
    bool latest_ui_select_right = false;
    bool ui_select_left_pressed_pending = false;
    bool ui_select_right_pressed_pending = false;

    mutable std::mutex error_mutex;
    std::string error;

    std::atomic_uint64_t pose_updates{0};
    std::atomic_uint64_t frames_uploaded{0};
    std::atomic_uint64_t new_frame_submissions{0};
    std::atomic_uint64_t repeated_frame_submissions{0};
    std::atomic_uint64_t rejected_frames{0};
    std::atomic_uint64_t submit_failures{0};
    std::atomic_bool shutdown_complete{false};

    std::string action_manifest_path;
    PresenterLogCallback log_callback = nullptr;
    void* log_context = nullptr;
    FlatUiPointerCallback flat_ui_callback = nullptr;
    void* flat_ui_context = nullptr;

    void Log(const std::string_view line) const noexcept {
        if (log_callback) log_callback(log_context, line);
    }

    void SetError(std::string value) noexcept {
        try {
            std::lock_guard lock(error_mutex);
            error = std::move(value);
        } catch (...) {
        }
    }

    void PublishFlatUiPointer(const FlatUiPointerSample& sample) const noexcept {
        if (!flat_ui_callback) return;
        try {
            flat_ui_callback(flat_ui_context, sample);
        } catch (...) {
        }
    }

    void LogPresentationState(
        const runtime::OpenVrPresentationState& state,
        const std::string_view phase) const noexcept {
        std::ostringstream line;
        line << "openvr_scene_state: phase=" << phase
             << ";process_id=" << state.process_id
             << ";scene_focus_process_id=" << state.scene_focus_process_id
             << ";can_render_scene=" << (state.can_render_scene ? "true" : "false")
             << ";input_available=" << (state.input_available ? "true" : "false")
             << ";dashboard_visible=" << (state.dashboard_visible ? "true" : "false")
             << ";should_pause=" << (state.should_pause ? "true" : "false")
             << ";should_reduce_rendering_work="
             << (state.should_reduce_rendering_work ? "true" : "false");
        Log(line.str());
    }

    void LogPresentationState(
        runtime::OpenVrRuntime& runtime,
        const std::string_view phase) const noexcept {
        runtime::OpenVrPresentationState state{};
        if (!runtime.ReadPresentationState(state)) return;
        LogPresentationState(state, phase);
    }

    void LogRuntimeState(
        const runtime::OpenVrRuntimeState& state,
        const std::string_view phase) const noexcept {
        try {
            std::ostringstream line;
            line << "openvr_runtime_state: phase=" << phase
                 << ";lifecycle=" << LifecycleName(state.lifecycle)
                 << ";initialized=" << (state.initialized ? "true" : "false")
                 << ";connected=" << (state.connected ? "true" : "false")
                 << ";focused=" << (state.focused ? "true" : "false")
                 << ";tracking_valid=" << (state.tracking_valid ? "true" : "false")
                 << ";presenting=" << (state.presenting ? "true" : "false")
                 << ";shutdown_requested="
                 << (state.shutdown_requested ? "true" : "false");
            Log(line.str());
        } catch (...) {
        }
    }

    void SignalInitialization(const bool ok) noexcept {
        try {
            std::lock_guard lock(init_mutex);
            init_ok = ok;
            init_done.store(true, std::memory_order_release);
            init_cv.notify_all();
        } catch (...) {
        }
    }

    bool EnsureTextures(
        D3D11SessionBridge& d3d11,
        const d3d9::StereoCpuFrame& frame,
        std::array<ComPtr<ID3D11Texture2D>, 2>& textures,
        std::uint32_t& texture_width,
        std::uint32_t& texture_height) noexcept {
        const auto& left = frame.eyes[0];
        const auto& right = frame.eyes[1];
        if (left.width == 0 || left.height == 0 ||
            left.width != right.width || left.height != right.height ||
            left.stride < left.width * 4U || right.stride < right.width * 4U ||
            left.format != d3d9::CpuPixelFormat::bgrx8_unorm ||
            right.format != d3d9::CpuPixelFormat::bgrx8_unorm) {
            SetError("presenter rejected inconsistent stereo CPU frame layout");
            return false;
        }
        const bool flat_theater =
            frame.presentation_mode == d3d9::FramePresentationMode::flat_theater;
        const std::uint32_t desired_width = flat_theater
            ? FlatTextureExtent(left.width, eyes[0].width)
            : left.width;
        const std::uint32_t desired_height = flat_theater
            ? FlatTextureExtent(left.height, eyes[0].height)
            : left.height;
        if (textures[0] && textures[1] &&
            texture_width == desired_width && texture_height == desired_height) {
            return true;
        }
        for (auto& texture : textures) texture.Reset();
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = desired_width;
        desc.Height = desired_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        for (auto& texture : textures) {
            const HRESULT hr = d3d11.device()->CreateTexture2D(&desc, nullptr, &texture);
            if (FAILED(hr) || !texture) {
                SetError("presenter D3D11 eye texture creation failed");
                for (auto& cleanup : textures) cleanup.Reset();
                return false;
            }
            if (flat_theater) {
                ComPtr<ID3D11RenderTargetView> view;
                if (SUCCEEDED(d3d11.device()->CreateRenderTargetView(texture.Get(), nullptr, &view)) &&
                    view) {
                    constexpr float black[4] = {0.0F, 0.0F, 0.0F, 1.0F};
                    d3d11.context()->ClearRenderTargetView(view.Get(), black);
                }
            }
        }
        texture_width = desired_width;
        texture_height = desired_height;
        return true;
    }

    bool UploadFrame(
        D3D11SessionBridge& d3d11,
        const d3d9::StereoCpuFrame& frame,
        const FlatUiPointerSample& flat_ui_pointer,
        std::array<ComPtr<ID3D11Texture2D>, 2>& textures,
        std::uint32_t& texture_width,
        std::uint32_t& texture_height,
        std::uint64_t& left_hash,
        std::uint64_t& right_hash,
        double& hash_ms,
        double& upload_ms) noexcept {
        if (!EnsureTextures(d3d11, frame, textures, texture_width, texture_height)) return false;
        const auto& left = frame.eyes[0];
        const auto& right = frame.eyes[1];
        const bool flat_theater =
            frame.presentation_mode == d3d9::FramePresentationMode::flat_theater;
        const std::size_t left_required = static_cast<std::size_t>(left.stride) * left.height;
        const std::size_t right_required = static_cast<std::size_t>(right.stride) * right.height;
        if (left.pixels.size() < left_required || right.pixels.size() < right_required) {
            SetError("presenter rejected truncated stereo CPU frame pixels");
            return false;
        }
        const bool identical_eye_content = d3d9::BgrxSurfacesEqualIgnoringAlpha(
                left.pixels.data(), left.stride,
                right.pixels.data(), right.stride,
                left.width, left.height);
        if (!flat_theater && identical_eye_content) {
            SetError("presenter rejected identical left/right eye RGB content");
            return false;
        }

        // Full-frame hashes are evidence telemetry, not a presentation primitive.
        // Preserve them for the sampled frames that are logged while keeping them
        // off the per-frame hot path. RGB equality is still checked on every frame.
        if (ShouldLogSequence(frame.capture_sequence)) {
            const auto hash_begin = std::chrono::steady_clock::now();
            left_hash = d3d9::HashBgrxSurfaceIgnoringAlpha(
                left.pixels.data(), left.stride, left.width, left.height);
            right_hash = d3d9::HashBgrxSurfaceIgnoringAlpha(
                right.pixels.data(), right.stride, right.width, right.height);
            const auto hash_end = std::chrono::steady_clock::now();
            hash_ms = MillisecondsBetween(hash_begin, hash_end);
            if (left_hash == 0 || right_hash == 0 ||
                (!flat_theater && left_hash == right_hash)) {
                SetError("presenter diagnostic hashes did not preserve distinct eye content");
                return false;
            }
        }

        const auto upload_begin = std::chrono::steady_clock::now();
        for (std::size_t eye = 0; eye < 2; ++eye) {
            const auto& source = frame.eyes[eye];
            if (flat_theater) {
                runtime::FlatTheaterEyePlacement placement{};
                if (!runtime::ComputeFlatTheaterEyePlacement(
                        eyes[eye], source.width, source.height,
                        texture_width, texture_height, placement)) {
                    SetError("presenter could not project the flat-theater plane into the runtime eye");
                    return false;
                }
                const std::uint32_t left_offset = placement.left;
                const std::uint32_t top_offset = placement.top;
                D3D11_BOX box{};
                box.left = left_offset;
                box.top = top_offset;
                box.front = 0;
                box.right = left_offset + source.width;
                box.bottom = top_offset + source.height;
                box.back = 1;
                d3d11.context()->UpdateSubresource(
                    textures[eye].Get(), 0, &box, source.pixels.data(), source.stride, 0);

                if (flat_ui_pointer.active &&
                    flat_ui_pointer.source_width == source.width &&
                    flat_ui_pointer.source_height == source.height) {
                    constexpr std::uint32_t kRadius = 9U;
                    const std::uint32_t center_x = std::min(
                        flat_ui_pointer.pixel_x, source.width - 1U);
                    const std::uint32_t center_y = std::min(
                        flat_ui_pointer.pixel_y, source.height - 1U);
                    const std::uint32_t patch_left = center_x > kRadius
                        ? center_x - kRadius
                        : 0U;
                    const std::uint32_t patch_top = center_y > kRadius
                        ? center_y - kRadius
                        : 0U;
                    const std::uint32_t patch_right = std::min(
                        source.width, center_x + kRadius + 1U);
                    const std::uint32_t patch_bottom = std::min(
                        source.height, center_y + kRadius + 1U);
                    const std::uint32_t patch_width = patch_right - patch_left;
                    const std::uint32_t patch_height = patch_bottom - patch_top;
                    std::vector<std::uint8_t> patch(
                        static_cast<std::size_t>(patch_width) * patch_height * 4U);
                    for (std::uint32_t row = 0; row < patch_height; ++row) {
                        const auto* source_row = source.pixels.data() +
                            static_cast<std::size_t>(patch_top + row) * source.stride +
                            static_cast<std::size_t>(patch_left) * 4U;
                        std::memcpy(
                            patch.data() + static_cast<std::size_t>(row) * patch_width * 4U,
                            source_row,
                            static_cast<std::size_t>(patch_width) * 4U);
                    }
                    const auto set_pixel = [&](const std::uint32_t x,
                                               const std::uint32_t y,
                                               const std::uint8_t value) noexcept {
                        if (x >= patch_width || y >= patch_height) return;
                        auto* pixel = patch.data() +
                            (static_cast<std::size_t>(y) * patch_width + x) * 4U;
                        pixel[0] = value;
                        pixel[1] = value;
                        pixel[2] = value;
                        pixel[3] = 0xFFU;
                    };
                    const std::uint32_t local_x = center_x - patch_left;
                    const std::uint32_t local_y = center_y - patch_top;
                    for (std::uint32_t offset = 2U; offset <= kRadius; ++offset) {
                        if (local_x >= offset) set_pixel(local_x - offset, local_y, 0x00U);
                        if (local_x + offset < patch_width) set_pixel(local_x + offset, local_y, 0x00U);
                        if (local_y >= offset) set_pixel(local_x, local_y - offset, 0x00U);
                        if (local_y + offset < patch_height) set_pixel(local_x, local_y + offset, 0x00U);
                    }
                    for (std::uint32_t offset = 3U; offset + 1U <= kRadius; ++offset) {
                        if (local_x >= offset) set_pixel(local_x - offset, local_y, 0xFFU);
                        if (local_x + offset < patch_width) set_pixel(local_x + offset, local_y, 0xFFU);
                        if (local_y >= offset) set_pixel(local_x, local_y - offset, 0xFFU);
                        if (local_y + offset < patch_height) set_pixel(local_x, local_y + offset, 0xFFU);
                    }
                    set_pixel(local_x, local_y, 0xFFU);
                    D3D11_BOX cursor_box{};
                    cursor_box.left = left_offset + patch_left;
                    cursor_box.top = top_offset + patch_top;
                    cursor_box.front = 0;
                    cursor_box.right = left_offset + patch_right;
                    cursor_box.bottom = top_offset + patch_bottom;
                    cursor_box.back = 1;
                    d3d11.context()->UpdateSubresource(
                        textures[eye].Get(), 0, &cursor_box, patch.data(), patch_width * 4U, 0);
                }
            } else {
                d3d11.context()->UpdateSubresource(
                    textures[eye].Get(), 0, nullptr, source.pixels.data(), source.stride, 0);
            }
        }
        const auto upload_end = std::chrono::steady_clock::now();
        upload_ms = MillisecondsBetween(upload_begin, upload_end);
        return true;
    }

    bool SubmitEyes(
        runtime::OpenVrRuntime& runtime,
        const std::array<ComPtr<ID3D11Texture2D>, 2>& textures,
        const runtime::Pose& render_hmd_pose,
        int& left_result,
        int& right_result,
        double& submit_ms,
        bool& scene_focus_pending) noexcept {
        scene_focus_pending = false;
        const auto submit_begin = std::chrono::steady_clock::now();
        std::string submit_error;
        if (!SubmitEyeD3D11WithPose(
                runtime, textures[0].Get(), EyeSubmission::Left,
                render_hmd_pose,
                left_result, submit_error)) {
            SetError(submit_error);
            if (IsOpenVrSceneFocusPending(left_result)) {
                scene_focus_pending = true;
                return false;
            }
            ++submit_failures;
            return false;
        }
        if (!SubmitEyeD3D11WithPose(
                runtime, textures[1].Get(), EyeSubmission::Right,
                render_hmd_pose,
                right_result, submit_error)) {
            SetError(submit_error);
            if (IsOpenVrSceneFocusPending(right_result)) {
                scene_focus_pending = true;
                return false;
            }
            ++submit_failures;
            return false;
        }
        const auto submit_end = std::chrono::steady_clock::now();
        submit_ms = MillisecondsBetween(submit_begin, submit_end);
        return true;
    }

    void Worker() noexcept {
        runtime::OpenVrRuntime runtime;
        D3D11SessionBridge d3d11;
        std::array<ComPtr<ID3D11Texture2D>, 2> textures{};
        std::uint32_t texture_width = 0;
        std::uint32_t texture_height = 0;
        std::uint64_t active_device_id = 0;
        std::uint64_t active_generation = 0;
        std::uint64_t active_capture_sequence = 0;
        std::uint64_t active_transport_sequence = 0;
        std::uint64_t active_render_pose_sequence = 0;
        std::chrono::steady_clock::time_point active_capture_time{};
        runtime::Pose active_render_hmd_pose{};
        std::uint32_t active_source_width = 0;
        std::uint32_t active_source_height = 0;
        runtime::Pose flat_theater_anchor_pose{};
        bool flat_theater_anchor_valid = false;
        FlatUiPointerSample flat_ui_pointer{};
        bool flat_ui_fire_release_required = false;
        std::uint64_t flat_ui_pointer_sequence = 0;
        d3d9::FramePresentationMode active_presentation_mode =
            d3d9::FramePresentationMode::native_stereo;
        bool have_active_presentation_mode = false;
        PresentationCadence cadence;
        runtime::OpenVrRuntimeState previous_runtime_state{};
        bool have_previous_runtime_state = false;
        runtime::OpenVrPresentationState previous_presentation_state{};
        bool have_previous_presentation_state = false;
        std::uint64_t presentation_poll_sequence = 0;

        try {
            if (!runtime.Initialize("Call of Juarez VR native stereo presenter")) {
                SetError(std::string(runtime.last_error()));
                SignalInitialization(false);
                return;
            }
            if (!runtime.ReadEyeConfiguration(eyes)) {
                SetError(std::string(runtime.last_error()));
                Log("native_stereo_presenter_shutdown: stage=runtime_begin init_failed=true");
                runtime.Shutdown();
                Log("native_stereo_presenter_shutdown: stage=runtime_end init_failed=true");
                SignalInitialization(false);
                return;
            }
            if (!action_manifest_path.empty()) {
                input_ready.store(
                    runtime.InitializeGlobalActions(action_manifest_path),
                    std::memory_order_release);
                if (input_ready.load(std::memory_order_acquire)) {
                    Log("openvr_input: status=started action_sets=/actions/global,/actions/gameplay recenter=/actions/global/in/recenter hand_pose=/user/hand/{left,right}/pose/handgrip aim_pose=/user/hand/{left,right}/pose/tip gameplay=semantic_sense_profile owner=presenter_thread");
                } else {
                    Log("openvr_input: status=unavailable owner=presenter_thread error=" +
                        std::string(runtime.last_error()));
                }
            }
            if (!d3d11.Initialize(runtime)) {
                SetError(std::string(d3d11.last_error()));
                Log("native_stereo_presenter_shutdown: stage=runtime_begin d3d11_init_failed=true");
                runtime.Shutdown();
                Log("native_stereo_presenter_shutdown: stage=runtime_end d3d11_init_failed=true");
                SignalInitialization(false);
                return;
            }

            running.store(true, std::memory_order_release);
            SignalInitialization(true);
            Log("native_stereo_presenter: status=started owner_thread=openvr+d3d11 mode=flat_theater_to_native_stereo");
            Log("openvr_gpu_handoff: upload=UpdateSubresource;gpu_sync=none;submit=Submit_TextureWithPose;handoff=PostPresentHandoff");
            if (runtime.ReadPresentationState(previous_presentation_state)) {
                have_previous_presentation_state = true;
                LogPresentationState(previous_presentation_state, "initialized");
            }
            previous_runtime_state = runtime.state();
            have_previous_runtime_state = true;
            LogRuntimeState(previous_runtime_state, "initialized");

            while (!stop_requested.load(std::memory_order_acquire)) {
                const auto pose_begin = std::chrono::steady_clock::now();
                runtime::OpenVrTrackedPoses tracked_poses{};
                // Enter compositor pacing as soon as either flat-theater or native
                // stereo content is presentable. This gives CoJ scene ownership
                // during intro/menu rendering instead of leaving SteamVR's
                // dashboard as the only visible layer until gameplay begins.
                const bool dashboard_visible_before_poll =
                    have_previous_presentation_state &&
                    previous_presentation_state.dashboard_visible;
                const bool compositor_pacing =
                    cadence.scene_submission_allowed(dashboard_visible_before_poll);
                const bool pose_ok =
                    (compositor_pacing
                         ? runtime.WaitForTrackedPoses(tracked_poses)
                         : runtime.ReadTrackedPoses(tracked_poses)) &&
                    tracked_poses.hmd.orientation_valid;
                const auto pose_end = std::chrono::steady_clock::now();
                const runtime::OpenVrRuntimeState runtime_state = runtime.state();
                if (!have_previous_runtime_state ||
                    !SameRuntimeState(runtime_state, previous_runtime_state)) {
                    const bool focus_changed = have_previous_runtime_state &&
                        runtime_state.focused != previous_runtime_state.focused;
                    LogRuntimeState(runtime_state, "transition");
                    if (focus_changed) {
                        LogPresentationState(
                            runtime,
                            runtime_state.focused ? "focus_gained" : "focus_lost");
                    }
                    previous_runtime_state = runtime_state;
                    have_previous_runtime_state = true;
                }
                ++presentation_poll_sequence;
                if (!have_previous_presentation_state || (presentation_poll_sequence % 8U) == 0U) {
                    runtime::OpenVrPresentationState presentation_state{};
                    if (runtime.ReadPresentationState(presentation_state)) {
                        if (have_previous_presentation_state &&
                            presentation_state.dashboard_visible !=
                                previous_presentation_state.dashboard_visible) {
                            LogPresentationState(
                                presentation_state,
                                presentation_state.dashboard_visible
                                    ? "dashboard_opened"
                                    : "dashboard_closed");
                            Log(std::string(
                                    "native_stereo_presenter_transition: status=") +
                                (presentation_state.dashboard_visible
                                     ? "dashboard_visible action=neutralize_gameplay_keep_scene_submission"
                                     : "dashboard_hidden action=resume_gameplay_keep_scene_submission"));
                        }
                        previous_presentation_state = presentation_state;
                        have_previous_presentation_state = true;
                    }
                }
                const bool dashboard_visible =
                    have_previous_presentation_state &&
                    previous_presentation_state.dashboard_visible;
                if (runtime_state.shutdown_requested) {
                    Log("native_stereo_presenter_transition: status=runtime_shutdown_requested action=stop_presenter");
                    break;
                }
                if (stop_requested.load(std::memory_order_acquire)) break;
                if (pose_ok) {
                    bool recenter = false;
                    runtime::OpenVrGlobalActions actions{};
                    runtime::GameplayInputState gameplay{};
                    runtime::GameplayInputState polled_gameplay{};
                    runtime::OpenVrHandPoses hand_poses{};
                    bool input_polled = false;
                    if (input_ready.load(std::memory_order_acquire)) {
                        if (runtime.PollActions(actions, polled_gameplay, hand_poses)) {
                            input_polled = true;
                            recenter = actions.recenter_requested;
                            if (recenter && tracked_poses.hmd.orientation_valid &&
                                tracked_poses.hmd.position_valid) {
                                flat_theater_anchor_pose = tracked_poses.hmd;
                                flat_theater_anchor_valid = true;
                                Log("native_stereo_flat_theater: status=recentered source=global_action");
                            }
                            const bool native_gameplay_active =
                                have_active_presentation_mode &&
                                active_presentation_mode ==
                                    d3d9::FramePresentationMode::native_stereo;
                            if (!dashboard_visible && runtime_state.focused &&
                                native_gameplay_active) {
                                gameplay = polled_gameplay;
                                if (flat_ui_fire_release_required) {
                                    gameplay.fire_left = false;
                                    gameplay.fire_right = false;
                                    if (!polled_gameplay.fire_left && !polled_gameplay.fire_right) {
                                        flat_ui_fire_release_required = false;
                                    }
                                }
                            }
                        }
                    }
                    const bool left_handgrip_valid = input_polled &&
                        hand_poses.left_grip_active &&
                        hand_poses.left_grip.position_valid &&
                        hand_poses.left_grip.orientation_valid;
                    const bool right_handgrip_valid = input_polled &&
                        hand_poses.right_grip_active &&
                        hand_poses.right_grip.position_valid &&
                        hand_poses.right_grip.orientation_valid;
                    const bool left_aim_valid = input_polled &&
                        hand_poses.left_aim_active &&
                        hand_poses.left_aim.position_valid &&
                        hand_poses.left_aim.orientation_valid;
                    const bool right_aim_valid = input_polled &&
                        hand_poses.right_aim_active &&
                        hand_poses.right_aim.position_valid &&
                        hand_poses.right_aim.orientation_valid;
                    FlatUiPointerSample pointer{};
                    const bool flat_mode_active =
                        have_active_presentation_mode &&
                        active_presentation_mode ==
                            d3d9::FramePresentationMode::flat_theater;
                    const bool pointer_allowed =
                        flat_mode_active && flat_theater_anchor_valid && input_polled &&
                        runtime_state.focused && !dashboard_visible &&
                        active_source_width > 0 && active_source_height > 0 &&
                        texture_width >= active_source_width &&
                        texture_height >= active_source_height;
                    if (pointer_allowed) {
                        const auto try_pointer = [&](const runtime::Pose& aim_pose,
                                                     const bool valid,
                                                     const bool using_left_hand) noexcept {
                            if (!valid || pointer.active) return;
                            runtime::FlatTheaterPointerProjection projected{};
                            if (!runtime::ProjectFlatTheaterPointer(
                                    flat_theater_anchor_pose, aim_pose,
                                    eyes[0].fov, eyes[1].fov,
                                    active_source_width, active_source_height,
                                    texture_width, texture_height, projected)) {
                                return;
                            }
                            pointer.active = true;
                            pointer.using_left_hand = using_left_hand;
                            pointer.u = projected.u;
                            pointer.v = projected.v;
                            pointer.pixel_x = projected.pixel_x;
                            pointer.pixel_y = projected.pixel_y;
                            pointer.source_width = active_source_width;
                            pointer.source_height = active_source_height;
                            pointer.ray_distance_m = projected.ray_distance_m;
                        };
                        try_pointer(hand_poses.right_aim, right_aim_valid, false);
                        try_pointer(hand_poses.right_grip, right_handgrip_valid, false);
                        try_pointer(hand_poses.left_aim, left_aim_valid, true);
                        try_pointer(hand_poses.left_grip, left_handgrip_valid, true);
                        if (pointer.active && flat_ui_pointer.active && !recenter &&
                            pointer.using_left_hand == flat_ui_pointer.using_left_hand &&
                            pointer.source_width == flat_ui_pointer.source_width &&
                            pointer.source_height == flat_ui_pointer.source_height) {
                            constexpr float kPointerSmoothing = 0.40F;
                            pointer.u = flat_ui_pointer.u +
                                (pointer.u - flat_ui_pointer.u) * kPointerSmoothing;
                            pointer.v = flat_ui_pointer.v +
                                (pointer.v - flat_ui_pointer.v) * kPointerSmoothing;
                            pointer.pixel_x = std::min(
                                pointer.source_width - 1U,
                                static_cast<std::uint32_t>(
                                    pointer.u * static_cast<float>(pointer.source_width)));
                            pointer.pixel_y = std::min(
                                pointer.source_height - 1U,
                                static_cast<std::uint32_t>(
                                    pointer.v * static_cast<float>(pointer.source_height)));
                        }
                        pointer.select_down = pointer.active &&
                            (actions.ui_select_left || actions.ui_select_right);
                        pointer.select_pressed = pointer.active &&
                            (actions.ui_select_left_pressed || actions.ui_select_right_pressed);
                        if (pointer.select_down) flat_ui_fire_release_required = true;
                    }
                    flat_ui_pointer = pointer;
                    PublishFlatUiPointer(flat_ui_pointer);
                    ++flat_ui_pointer_sequence;
                    if (flat_ui_pointer.active &&
                        ShouldLogSequence(flat_ui_pointer_sequence)) {
                        std::ostringstream line;
                        line << "flat_ui_pointer: status=hit"
                             << ";hand=" << (flat_ui_pointer.using_left_hand ? "left" : "right")
                             << ";pose=tip_with_grip_fallback"
                             << ";u=" << std::fixed << std::setprecision(4) << flat_ui_pointer.u
                             << ";v=" << flat_ui_pointer.v
                             << ";pixel=" << flat_ui_pointer.pixel_x << ',' << flat_ui_pointer.pixel_y
                             << ";source=" << flat_ui_pointer.source_width << 'x'
                             << flat_ui_pointer.source_height
                             << ";select=" << (flat_ui_pointer.select_down ? "true" : "false")
                             << ";smoothing=0.40"
                             << ";route=flat_theater_menu_pointer";
                        Log(line.str());
                    }
                    {
                        std::lock_guard lock(tracking_mutex);
                        latest_pose = tracked_poses.hmd;
                        latest_left_controller = left_handgrip_valid
                            ? hand_poses.left_grip
                            : runtime::Pose{};
                        latest_right_controller = right_handgrip_valid
                            ? hand_poses.right_grip
                            : runtime::Pose{};
                        latest_left_aim = left_aim_valid
                            ? hand_poses.left_aim
                            : runtime::Pose{};
                        latest_right_aim = right_aim_valid
                            ? hand_poses.right_aim
                            : runtime::Pose{};
                        if (latest_left_handgrip_active != left_handgrip_valid ||
                            latest_right_handgrip_active != right_handgrip_valid ||
                            pose_sequence == 0) {
                            std::ostringstream line;
                            line << "openvr_controller_pose: source=handgrip"
                                 << ";left_active="
                                 << (left_handgrip_valid ? "true" : "false")
                                 << ";right_active="
                                 << (right_handgrip_valid ? "true" : "false")
                                 << ";raw_role_fallback=false";
                            Log(line.str());
                        }
                        latest_left_handgrip_active = left_handgrip_valid;
                        latest_right_handgrip_active = right_handgrip_valid;
                        if (latest_left_aim_active != left_aim_valid ||
                            latest_right_aim_active != right_aim_valid ||
                            pose_sequence == 0) {
                            std::ostringstream line;
                            line << "openvr_controller_pose: source=tip"
                                 << ";left_active="
                                 << (left_aim_valid ? "true" : "false")
                                 << ";right_active="
                                 << (right_aim_valid ? "true" : "false")
                                 << ";purpose=weapon_aim";
                            Log(line.str());
                        }
                        latest_left_aim_active = left_aim_valid;
                        latest_right_aim_active = right_aim_valid;
                        latest_gameplay = gameplay;
                        latest_ui_select_left = actions.ui_select_left;
                        latest_ui_select_right = actions.ui_select_right;
                        if (actions.ui_select_left_pressed) {
                            ui_select_left_pressed_pending = true;
                        }
                        if (actions.ui_select_right_pressed) {
                            ui_select_right_pressed_pending = true;
                        }
                        ++pose_sequence;
                        if (recenter) recenter_pending = true;
                    }
                    ++pose_updates;
                } else {
                    flat_ui_pointer = {};
                    PublishFlatUiPointer(flat_ui_pointer);
                }

                d3d9::StereoCpuFrame frame{};
                const bool have_new_frame = mailbox.WaitConsumeLatest(
                    frame, cadence.scene_submission_allowed(dashboard_visible) ? 0U : 2U);
                std::uint64_t left_hash = 0;
                std::uint64_t right_hash = 0;
                double hash_ms = 0.0;
                double upload_ms = 0.0;
                if (have_new_frame) {
                    const std::uint64_t incoming_transport_sequence =
                        frame.transport_sequence != 0
                            ? frame.transport_sequence
                            : frame.capture_sequence;
                    const bool stale = active_transport_sequence != 0 &&
                        incoming_transport_sequence <= active_transport_sequence;
                    const bool stale_generation = active_generation != 0 &&
                        (frame.generation < active_generation ||
                         (frame.generation == active_generation &&
                          frame.device_id != active_device_id));
                    if (stale || stale_generation) {
                        ++rejected_frames;
                        Log("native_stereo_presenter_frame: status=rejected reason=stale frame_sequence=" +
                            std::to_string(frame.capture_sequence));
                    } else {
                        if (frame.generation != active_generation ||
                            frame.device_id != active_device_id) {
                            for (auto& texture : textures) texture.Reset();
                            texture_width = 0;
                            texture_height = 0;
                            active_generation = frame.generation;
                            active_device_id = frame.device_id;
                            active_transport_sequence = 0;
                            active_render_pose_sequence = 0;
                            active_render_hmd_pose = {};
                            active_source_width = 0;
                            active_source_height = 0;
                            cadence.Invalidate();
                        }
                        if (frame.presentation_mode ==
                                d3d9::FramePresentationMode::flat_theater &&
                            (!flat_theater_anchor_valid ||
                             !have_active_presentation_mode ||
                             active_presentation_mode != frame.presentation_mode)) {
                            flat_theater_anchor_pose = frame.render_hmd_pose;
                            flat_theater_anchor_valid = true;
                        }
                        if (UploadFrame(
                                d3d11, frame, flat_ui_pointer,
                                textures, texture_width, texture_height,
                                left_hash, right_hash, hash_ms, upload_ms)) {
                            const bool first_presentable_frame = !cadence.presentable();
                            const bool mode_changed = !have_active_presentation_mode ||
                                active_presentation_mode != frame.presentation_mode;
                            active_capture_sequence = frame.capture_sequence;
                            active_transport_sequence = incoming_transport_sequence;
                            active_capture_time = frame.capture_time;
                            active_render_pose_sequence = frame.render_pose_sequence;
                            active_render_hmd_pose = frame.render_hmd_pose;
                            active_source_width = frame.eyes[0].width;
                            active_source_height = frame.eyes[0].height;
                            active_presentation_mode = frame.presentation_mode;
                            have_active_presentation_mode = true;
                            cadence.FrameUploaded();
                            ++frames_uploaded;
                            if (mode_changed) {
                                Log(std::string("native_stereo_presenter_transition: status=content_mode mode=") +
                                    (frame.presentation_mode == d3d9::FramePresentationMode::flat_theater
                                         ? "flat_theater"
                                         : "native_stereo"));
                                if (frame.presentation_mode ==
                                    d3d9::FramePresentationMode::flat_theater) {
                                    runtime::FlatTheaterEyePlacement left_placement{};
                                    runtime::FlatTheaterEyePlacement right_placement{};
                                    if (runtime::ComputeFlatTheaterEyePlacement(
                                            eyes[0], frame.eyes[0].width, frame.eyes[0].height,
                                            texture_width, texture_height, left_placement) &&
                                        runtime::ComputeFlatTheaterEyePlacement(
                                            eyes[1], frame.eyes[1].width, frame.eyes[1].height,
                                            texture_width, texture_height, right_placement)) {
                                        std::ostringstream geometry;
                                        geometry << "native_stereo_flat_theater_geometry: status=active"
                                                 << ";plane_distance_m=1.500"
                                                 << ";texture=" << texture_width << 'x' << texture_height
                                                 << ";source=" << frame.eyes[0].width << 'x'
                                                 << frame.eyes[0].height
                                                 << ";left_offset=" << left_placement.left << ','
                                                 << left_placement.top
                                                 << ";right_offset=" << right_placement.left << ','
                                                 << right_placement.top
                                                 << ";binocular_projection=eye_to_head_fov";
                                        Log(geometry.str());
                                    }
                                }
                            }
                            if (first_presentable_frame) {
                                Log("native_stereo_presenter_transition: status=scene_ready action=claim_compositor_focus_with_presentable_content");
                                LogPresentationState(runtime, "frame_ready");
                            }
                            if (ShouldLogSequence(frame.capture_sequence)) {
                                std::ostringstream line;
                                line << "native_stereo_presenter_frame: status=new frame_sequence="
                                     << frame.capture_sequence
                                     << ";render_pose_sequence=" << frame.render_pose_sequence
                                     << ";generation=" << frame.generation
                                     << ";presentation_mode="
                                     << (frame.presentation_mode == d3d9::FramePresentationMode::flat_theater
                                             ? "flat_theater"
                                             : "native_stereo")
                                     << ";left_hash=" << left_hash
                                     << ";right_hash=" << right_hash
                                     << ";distinct_eye_content="
                                     << (frame.presentation_mode == d3d9::FramePresentationMode::flat_theater
                                             ? "false"
                                             : "true")
                                     << ";distinct_check="
                                     << (frame.presentation_mode == d3d9::FramePresentationMode::flat_theater
                                             ? "not_required_flat_theater"
                                             : "rgb_compare_every_frame")
                                     << ";hash_mode=sampled_telemetry"
                                     << std::fixed << std::setprecision(3)
                                     << ";hash_ms=" << hash_ms
                                     << ";upload_ms=" << upload_ms;
                                Log(line.str());
                            }
                        } else {
                            ++rejected_frames;
                            Log("native_stereo_presenter_frame: status=rejected frame_sequence=" +
                                std::to_string(frame.capture_sequence) + ";error=" + ErrorCopy());
                        }
                    }
                }

                if (!cadence.presentable()) continue;
                // Do not permanently gate scene submission on the dashboard flag.
                // SteamVR can report the overlay state while transitioning between
                // compositor layers, and dropping all submits here can leave the
                // application stuck on the dashboard when gameplay starts. The
                // runtime owns overlay composition; keep feeding valid scene frames.
                if (!pose_ok || !runtime_state.connected || !runtime_state.tracking_valid) {
                    // Preserve the last valid textures/cadence, but do not submit
                    // them while tracking is invalid or the HMD is disconnected.
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }
                int left_result = 0;
                int right_result = 0;
                double submit_ms = 0.0;
                bool scene_focus_pending = false;
                const bool flat_theater_active =
                    active_presentation_mode == d3d9::FramePresentationMode::flat_theater;
                const runtime::Pose& submit_pose =
                    flat_theater_active && flat_theater_anchor_valid
                        ? flat_theater_anchor_pose
                        : active_render_hmd_pose;
                if (active_render_pose_sequence == 0 ||
                    !submit_pose.orientation_valid || !submit_pose.position_valid) {
                    ++rejected_frames;
                    Log("native_stereo_presenter_submit: status=failed frame_sequence=" +
                        std::to_string(active_capture_sequence) +
                        ";error=missing exact render pose");
                    cadence.Invalidate();
                    continue;
                }
                const PresentationContent pending_content = cadence.pending_content();
                if (!SubmitEyes(
                        runtime, textures, submit_pose,
                        left_result, right_result, submit_ms, scene_focus_pending)) {
                    if (scene_focus_pending) {
                        Log("native_stereo_presenter_submit: status=deferred reason=scene_focus_pending frame_sequence=" +
                            std::to_string(active_capture_sequence) + ";runtime_result=" +
                            std::to_string(left_result != 0 ? left_result : right_result));
                    } else {
                        Log("native_stereo_presenter_submit: status=failed frame_sequence=" +
                            std::to_string(active_capture_sequence) + ";error=" + ErrorCopy());
                    }
                    continue;
                }
                cadence.SubmissionSucceeded();
                runtime.PostPresentHandoff();
                const std::uint64_t completed_submissions =
                    new_frame_submissions.load(std::memory_order_acquire) +
                    repeated_frame_submissions.load(std::memory_order_acquire);
                if (completed_submissions == 0) {
                    LogPresentationState(runtime, "first_submit");
                }
                const bool uploaded_new_frame =
                    pending_content == PresentationContent::new_frame;
                if (uploaded_new_frame) {
                    ++new_frame_submissions;
                } else {
                    ++repeated_frame_submissions;
                }
                const std::uint64_t submit_sequence =
                    new_frame_submissions.load(std::memory_order_acquire) +
                    repeated_frame_submissions.load(std::memory_order_acquire);
                if (ShouldLogSequence(submit_sequence)) {
                    std::ostringstream line;
                    line << "native_stereo_presenter_timing: status=ok submit_sequence="
                         << submit_sequence
                         << ";capture_sequence=" << active_capture_sequence
                         << ";render_pose_sequence=" << active_render_pose_sequence
                         << ";pose_mode="
                         << (flat_theater_active ? "flat_theater_anchor" : "explicit_render_pose")
                         << ";presentation_mode="
                         << (flat_theater_active ? "flat_theater" : "native_stereo")
                         << ";content=" << (uploaded_new_frame ? "new" : "repeated")
                         << ";left_result=" << left_result
                         << ";right_result=" << right_result
                         << std::fixed << std::setprecision(3)
                         << ";wait_pose_ms=" << MillisecondsBetween(pose_begin, pose_end)
                         << ";submit_ms=" << submit_ms;
                    Log(line.str());

                    std::ostringstream pose_line;
                    pose_line << "native_stereo_pose_telemetry: status=ok"
                              << ";frame_sequence=" << active_capture_sequence
                              << ";render_pose_sequence=" << active_render_pose_sequence
                              << ";presentation_mode="
                              << (flat_theater_active ? "flat_theater" : "native_stereo")
                              << std::fixed << std::setprecision(3)
                              << ";pose_age_ms=" << PoseAgeMilliseconds(active_capture_time)
                              << ";position_x_m=" << active_render_hmd_pose.position.x
                              << ";position_y_m=" << active_render_hmd_pose.position.y
                              << ";position_z_m=" << active_render_hmd_pose.position.z
                              << ";capture_time_valid="
                              << (active_capture_time != std::chrono::steady_clock::time_point{}
                                  ? "true" : "false");
                    Log(pose_line.str());
                }
            }
        } catch (...) {
            SetError("OpenVR presenter worker raised an exception");
            if (!init_done.load(std::memory_order_acquire)) SignalInitialization(false);
        }

        PublishFlatUiPointer({});
        running.store(false, std::memory_order_release);
        for (auto& texture : textures) texture.Reset();
        Log("native_stereo_presenter_shutdown: stage=d3d11_begin");
        d3d11.Shutdown();
        Log("native_stereo_presenter_shutdown: stage=d3d11_end");
        Log("native_stereo_presenter_shutdown: stage=runtime_begin");
        runtime.Shutdown();
        shutdown_complete.store(
            runtime.state().lifecycle == runtime::OpenVrLifecycleState::shutdown_complete,
            std::memory_order_release);
        LogRuntimeState(runtime.state(), "shutdown_complete");
        Log("native_stereo_presenter_shutdown: stage=runtime_end");
        input_ready.store(false, std::memory_order_release);
        Log("native_stereo_presenter: status=stopped");
    }

    std::string ErrorCopy() const noexcept {
        try {
            std::lock_guard lock(error_mutex);
            return error;
        } catch (...) {
            return "error unavailable";
        }
    }
};

OpenVrStereoPresenter::OpenVrStereoPresenter() : impl_(std::make_unique<Impl>()) {}
OpenVrStereoPresenter::~OpenVrStereoPresenter() { Stop(); }

bool OpenVrStereoPresenter::Start(
    std::string action_manifest_path,
    const PresenterLogCallback log_callback,
    void* const log_context,
    const FlatUiPointerCallback flat_ui_callback,
    void* const flat_ui_context) noexcept {
    try {
        Stop();
        impl_->mailbox.Reset();
        impl_->stop_requested.store(false, std::memory_order_release);
        impl_->running.store(false, std::memory_order_release);
        impl_->input_ready.store(false, std::memory_order_release);
        impl_->action_manifest_path = std::move(action_manifest_path);
        impl_->log_callback = log_callback;
        impl_->log_context = log_context;
        impl_->flat_ui_callback = flat_ui_callback;
        impl_->flat_ui_context = flat_ui_context;
        {
            std::lock_guard lock(impl_->init_mutex);
            impl_->init_done.store(false, std::memory_order_release);
            impl_->init_ok = false;
            impl_->eyes = {};
        }
        {
            std::lock_guard lock(impl_->tracking_mutex);
            impl_->latest_pose = {};
            impl_->latest_left_controller = {};
            impl_->latest_right_controller = {};
            impl_->latest_left_aim = {};
            impl_->latest_right_aim = {};
            impl_->latest_left_handgrip_active = false;
            impl_->latest_right_handgrip_active = false;
            impl_->latest_left_aim_active = false;
            impl_->latest_right_aim_active = false;
            impl_->latest_gameplay = {};
            impl_->latest_ui_select_left = false;
            impl_->latest_ui_select_right = false;
            impl_->pose_sequence = 0;
            impl_->recenter_pending = false;
            impl_->ui_select_left_pressed_pending = false;
            impl_->ui_select_right_pressed_pending = false;
        }
        impl_->pose_updates.store(0, std::memory_order_release);
        impl_->frames_uploaded.store(0, std::memory_order_release);
        impl_->new_frame_submissions.store(0, std::memory_order_release);
        impl_->repeated_frame_submissions.store(0, std::memory_order_release);
        impl_->rejected_frames.store(0, std::memory_order_release);
        impl_->submit_failures.store(0, std::memory_order_release);
        impl_->shutdown_complete.store(false, std::memory_order_release);
        impl_->worker = std::thread([state = impl_.get()] { state->Worker(); });

        std::unique_lock lock(impl_->init_mutex);
        const bool signaled = impl_->init_cv.wait_for(
            lock,
            std::chrono::seconds(10),
            [this] { return impl_->init_done.load(std::memory_order_acquire); });
        const bool ok = signaled && impl_->init_ok;
        lock.unlock();
        if (!ok) {
            if (!signaled) impl_->SetError("OpenVR presenter initialization timed out");
            impl_->stop_requested.store(true, std::memory_order_release);
            impl_->mailbox.Stop();
            if (impl_->worker.joinable()) impl_->worker.join();
            return false;
        }
        return true;
    } catch (...) {
        impl_->SetError("OpenVR presenter start raised an exception");
        Stop();
        return false;
    }
}

void OpenVrStereoPresenter::Stop() noexcept {
    if (!impl_) return;
    impl_->stop_requested.store(true, std::memory_order_release);
    impl_->mailbox.Stop();
    try {
        if (impl_->worker.joinable()) impl_->worker.join();
    } catch (...) {
        impl_->SetError("OpenVR presenter worker join failed");
    }
    impl_->running.store(false, std::memory_order_release);
}

bool OpenVrStereoPresenter::Publish(d3d9::StereoCpuFrame frame) noexcept {
    if (!impl_->running.load(std::memory_order_acquire)) return false;
    return impl_->mailbox.Publish(std::move(frame));
}

bool OpenVrStereoPresenter::LatestTracking(OpenVrTrackingSample& sample) noexcept {
    sample = {};
    try {
        std::lock_guard lock(impl_->tracking_mutex);
        if (impl_->pose_sequence == 0 || !impl_->latest_pose.orientation_valid) return false;
        sample.pose = impl_->latest_pose;
        sample.left_controller = impl_->latest_left_controller;
        sample.right_controller = impl_->latest_right_controller;
        sample.left_aim = impl_->latest_left_aim;
        sample.right_aim = impl_->latest_right_aim;
        sample.gameplay = impl_->latest_gameplay;
        sample.sequence = impl_->pose_sequence;
        sample.recenter_requested = impl_->recenter_pending;
        sample.ui_select_left = impl_->latest_ui_select_left;
        sample.ui_select_right = impl_->latest_ui_select_right;
        sample.ui_select_left_pressed = impl_->ui_select_left_pressed_pending;
        sample.ui_select_right_pressed = impl_->ui_select_right_pressed_pending;
        impl_->recenter_pending = false;
        impl_->ui_select_left_pressed_pending = false;
        impl_->ui_select_right_pressed_pending = false;
        return true;
    } catch (...) {
        return false;
    }
}

bool OpenVrStereoPresenter::EyeViews(std::array<runtime::EyeView, 2>& eyes) const noexcept {
    eyes = {};
    try {
        std::lock_guard lock(impl_->init_mutex);
        if (!impl_->init_ok) return false;
        eyes = impl_->eyes;
        return true;
    } catch (...) {
        return false;
    }
}

bool OpenVrStereoPresenter::running() const noexcept {
    return impl_->running.load(std::memory_order_acquire);
}

bool OpenVrStereoPresenter::input_ready() const noexcept {
    return impl_->input_ready.load(std::memory_order_acquire);
}

OpenVrPresenterStats OpenVrStereoPresenter::stats() const noexcept {
    OpenVrPresenterStats result{};
    result.pose_updates = impl_->pose_updates.load(std::memory_order_acquire);
    result.frames_uploaded = impl_->frames_uploaded.load(std::memory_order_acquire);
    result.new_frame_submissions = impl_->new_frame_submissions.load(std::memory_order_acquire);
    result.repeated_frame_submissions =
        impl_->repeated_frame_submissions.load(std::memory_order_acquire);
    result.rejected_frames = impl_->rejected_frames.load(std::memory_order_acquire);
    result.submit_failures = impl_->submit_failures.load(std::memory_order_acquire);
    result.shutdown_complete = impl_->shutdown_complete.load(std::memory_order_acquire);
    result.mailbox = impl_->mailbox.stats();
    return result;
}

std::string OpenVrStereoPresenter::last_error() const noexcept { return impl_->ErrorCopy(); }

} // namespace cojvr::backends::openvr
