#include "backends/openvr/stereo_presenter.hpp"
#include "backends/openvr/reticle_pixels.hpp"

#include "backends/d3d9/content_hash.hpp"
#include "backends/openvr/d3d11_compositor.hpp"
#include "backends/openvr/hud_text_overlay.hpp"
#include "backends/openvr/d3d11_session.hpp"
#include "backends/openvr/d3d9_shared_texture_bridge.hpp"
#include "backends/openvr/presentation_cadence.hpp"
#include "runtime/hmd_frame_pacing.hpp"
#include "runtime/ui_pointer_ownership.hpp"
#include "runtime/openvr_runtime.hpp"
#include "runtime/gameplay_utility.hpp"
#include "runtime/reload_haptics.hpp"
#include "runtime/vr_math.hpp"

#include <d3d11.h>
#include <d3d9.h>
#include <wrl/client.h>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
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

std::uint32_t FrameWidth(const d3d9::StereoCpuFrame& frame) noexcept {
    return frame.transport == d3d9::StereoFrameTransport::d3d9ex_shared_texture
        ? frame.shared_eyes[0].width
        : frame.eyes[0].width;
}

std::uint32_t FrameHeight(const d3d9::StereoCpuFrame& frame) noexcept {
    return frame.transport == d3d9::StereoFrameTransport::d3d9ex_shared_texture
        ? frame.shared_eyes[0].height
        : frame.eyes[0].height;
}

DXGI_FORMAT FrameDxgiFormat(const d3d9::StereoCpuFrame& frame) noexcept {
    if (frame.transport != d3d9::StereoFrameTransport::d3d9ex_shared_texture) {
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    }
    switch (static_cast<D3DFORMAT>(frame.shared_eyes[0].d3d_format)) {
    case D3DFMT_A8R8G8B8: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case D3DFMT_X8R8G8B8: return DXGI_FORMAT_B8G8R8X8_UNORM;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

const std::uint8_t* CpuEyeData(const d3d9::CpuEyeFrame& eye) noexcept {
    return eye.borrowed_pixels ? eye.borrowed_pixels : eye.pixels.data();
}

} // namespace

struct OpenVrStereoPresenter::Impl {
    d3d9::FrameMailbox mailbox;
    runtime::GameplayUiFeedbackContextMailbox feedback_context;
    std::atomic<std::uint64_t> reload_feedback_owner{0};
    std::thread worker;
    std::atomic_bool stop_requested{false};
    std::atomic_bool running{false};
    std::atomic_bool input_ready{false};
    std::atomic<float> display_frequency_hz{0.0F};

    mutable std::mutex init_mutex;
    std::condition_variable init_cv;
    std::atomic_bool init_done{false};
    bool init_ok = false;
    std::array<runtime::EyeView, 2> eyes{};
    HudTextCompositor hud_text_compositor;

    mutable std::mutex tracking_mutex;
    runtime::Pose latest_pose{};
    runtime::Pose latest_left_controller{};
    runtime::Pose latest_right_controller{};
    runtime::Pose latest_left_aim{};
    runtime::Pose latest_right_aim{};
    runtime::FingerTrackingState latest_left_fingers{};
    runtime::FingerTrackingState latest_right_fingers{};
    bool latest_left_handgrip_active = false;
    bool latest_right_handgrip_active = false;
    bool latest_left_aim_active = false;
    bool latest_right_aim_active = false;
    runtime::GameplayInputState latest_gameplay{};
    std::uint64_t gameplay_context_generation = 0;
    bool gameplay_context_active = false;
    std::uint64_t pose_sequence = 0;
    bool recenter_pending = false;
    runtime::AuxiliaryObjectivesPending objectives_pending{};
    runtime::AuxiliaryButtonGesture create_gesture{};
    bool latest_ui_select_left = false;
    bool latest_ui_select_right = false;
    bool ui_select_left_pressed_pending = false;
    bool ui_select_right_pressed_pending = false;
    bool latest_ui_accept = false;
    bool latest_ui_back = false;
    bool latest_ui_actions_allowed = false;
    bool ui_accept_pressed_pending = false;
    bool ui_back_pressed_pending = false;
    bool pause_pressed_pending = false;

    mutable std::mutex error_mutex;
    std::string error;

    std::atomic_uint64_t pose_updates{0};
    std::atomic_uint64_t frames_uploaded{0};
    std::atomic_uint64_t new_frame_submissions{0};
    std::atomic_uint64_t repeated_frame_submissions{0};
    std::atomic_uint64_t rejected_frames{0};
    std::atomic_uint64_t submit_failures{0};
    std::atomic_uint64_t shared_frames_copied{0};
    std::atomic_uint64_t shared_resources_opened{0};
    std::atomic_uint64_t shared_copy_fences_completed{0};
    std::atomic_uint64_t shared_open_failures{0};
    std::atomic_uint64_t shared_copy_failures{0};
    std::atomic_uint64_t shared_pending_copy_fences{0};
    std::atomic_uint64_t shared_pending_copy_fences_peak{0};
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

    bool PublishFlatUiPointer(const FlatUiPointerSample& sample) const noexcept {
        if (!flat_ui_callback) return true;
        try {
            return flat_ui_callback(flat_ui_context, sample);
        } catch (...) {
            return false;
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
        std::uint32_t& texture_height,
        DXGI_FORMAT& texture_format) noexcept {
        const auto& left = frame.eyes[0];
        const auto& right = frame.eyes[1];
        const bool shared =
            frame.transport == d3d9::StereoFrameTransport::d3d9ex_shared_texture;
        if (shared) {
            const auto& shared_left = frame.shared_eyes[0];
            const auto& shared_right = frame.shared_eyes[1];
            if (shared_left.shared_handle == 0 || shared_right.shared_handle == 0 ||
                shared_left.width == 0 || shared_left.height == 0 ||
                shared_left.width != shared_right.width ||
                shared_left.height != shared_right.height ||
                shared_left.d3d_format != shared_right.d3d_format) {
                SetError("presenter rejected inconsistent D3D9Ex shared stereo frame layout");
                return false;
            }
        } else {
            if (left.width == 0 || left.height == 0 ||
                left.width != right.width || left.height != right.height ||
                left.stride < left.width * 4U || right.stride < right.width * 4U ||
                left.format != d3d9::CpuPixelFormat::bgrx8_unorm ||
                right.format != d3d9::CpuPixelFormat::bgrx8_unorm) {
                SetError("presenter rejected inconsistent stereo CPU frame layout");
                return false;
            }
        }
        const bool flat_theater =
            frame.presentation_mode == d3d9::FramePresentationMode::flat_theater;
        if (shared && flat_theater) {
            SetError("flat theater currently requires the CPU composition fallback");
            return false;
        }
        const std::uint32_t source_width = FrameWidth(frame);
        const std::uint32_t source_height = FrameHeight(frame);
        std::uint32_t desired_width = source_width;
        std::uint32_t desired_height = source_height;
        if (flat_theater &&
            !runtime::ComputeFlatTheaterTextureExtent(
                eyes, source_width, source_height, desired_width, desired_height)) {
            SetError("presenter rejected invalid flat-theater eye geometry");
            return false;
        }
        if (desired_width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
            desired_height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
            SetError("presenter eye texture exceeds the D3D11 extent limit");
            return false;
        }
        const DXGI_FORMAT desired_format = FrameDxgiFormat(frame);
        if (desired_format == DXGI_FORMAT_UNKNOWN) {
            SetError("presenter rejected unsupported D3D9Ex shared texture format");
            return false;
        }
        if (textures[0] && textures[1] &&
            texture_width == desired_width && texture_height == desired_height &&
            texture_format == desired_format) {
            return true;
        }
        for (auto& texture : textures) texture.Reset();
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = desired_width;
        desc.Height = desired_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = desired_format;
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
        texture_format = desired_format;
        return true;
    }

    bool UploadFrame(
        D3D11SessionBridge& d3d11,
        D3D9SharedTextureBridge& shared_bridge,
        d3d9::StereoCpuFrame& frame,
        const FlatUiPointerSample& flat_ui_pointer,
        std::array<ComPtr<ID3D11Texture2D>, 2>& textures,
        std::uint32_t& texture_width,
        std::uint32_t& texture_height,
        DXGI_FORMAT& texture_format,
        std::uint64_t& left_hash,
        std::uint64_t& right_hash,
        double& hash_ms,
        double& upload_ms,
        D3D9SharedTextureCopyTiming& shared_timing) noexcept {
        const auto draw_hud_text = [&]() noexcept {
            if (frame.presentation_mode != d3d9::FramePresentationMode::native_stereo ||
                frame.hud_text.frame_sequence != frame.capture_sequence) return;
            const std::array<ID3D11Texture2D*, 2> targets{textures[0].Get(), textures[1].Get()};
            if (!hud_text_compositor.Draw(d3d11.device(), d3d11.context(),
                    frame.hud_text, frame.capture_sequence, targets)) {
                if (frame.capture_sequence <= 8 || frame.capture_sequence % 127 == 0)
                    SetError("native HUD text composition unavailable");
            }
        };
        const auto draw_gameplay_reticle = [&]() noexcept {
            if (!frame.gameplay_reticle.active ||
                frame.gameplay_reticle.frame_sequence != frame.capture_sequence ||
                frame.presentation_mode != d3d9::FramePresentationMode::native_stereo ||
                texture_width == 0 || texture_height == 0 ||
                (texture_format != DXGI_FORMAT_B8G8R8A8_UNORM &&
                 texture_format != DXGI_FORMAT_B8G8R8X8_UNORM)) {
                return;
            }
            constexpr std::uint32_t kOutlineRadius = 8U;
            constexpr std::uint32_t kInnerRadius = 6U;
            constexpr std::uint32_t kBlack = 0xFF000000U;
            constexpr std::uint32_t kWhite = 0xFFFFFFFFU;
            std::array<std::uint32_t, kOutlineRadius * 2U + 1U> pixels{};
            const auto draw_horizontal = [&](const std::size_t eye,
                                             const std::uint32_t center_x,
                                             const std::uint32_t center_y,
                                             const std::uint32_t radius,
                                             const std::uint32_t color) noexcept {
                const std::uint32_t left = center_x > radius ? center_x - radius : 0U;
                const std::uint32_t right = std::min(texture_width, center_x + radius + 1U);
                const std::uint32_t count = right - left;
                std::fill_n(pixels.begin(), count, color);
                D3D11_BOX box{left, center_y, 0U, right, center_y + 1U, 1U};
                d3d11.context()->UpdateSubresource(
                    textures[eye].Get(), 0, &box, pixels.data(), count * 4U, 0);
            };
            const auto draw_vertical = [&](const std::size_t eye,
                                           const std::uint32_t center_x,
                                           const std::uint32_t center_y,
                                           const std::uint32_t radius,
                                           const std::uint32_t color) noexcept {
                const std::uint32_t top = center_y > radius ? center_y - radius : 0U;
                const std::uint32_t bottom = std::min(texture_height, center_y + radius + 1U);
                const std::uint32_t count = bottom - top;
                std::fill_n(pixels.begin(), count, color);
                D3D11_BOX box{center_x, top, 0U, center_x + 1U, bottom, 1U};
                d3d11.context()->UpdateSubresource(
                    textures[eye].Get(), 0, &box, pixels.data(), 4U, 0);
            };
            for (std::size_t eye = 0; eye < 2; ++eye) {
                const auto& point = frame.gameplay_reticle.eyes[eye];
                if (!point.valid || !std::isfinite(point.u) || !std::isfinite(point.v) ||
                    point.u < 0.0F || point.u > 1.0F || point.v < 0.0F || point.v > 1.0F) {
                    continue;
                }
                const std::uint32_t center_x = std::min(
                    texture_width - 1U,
                    static_cast<std::uint32_t>(
                        point.u * static_cast<float>(texture_width - 1U) + 0.5F));
                const std::uint32_t center_y = std::min(
                    texture_height - 1U,
                    static_cast<std::uint32_t>(
                        point.v * static_cast<float>(texture_height - 1U) + 0.5F));
                if (frame.gameplay_reticle.no_shoot) {
                    // Upload only contiguous opaque spans of the warning;
                    // transparent gaps keep the captured world untouched.
                    for(int y=-8;y<=8;++y){
                        const int py=static_cast<int>(center_y)+y;
                        if(py<0||py>=static_cast<int>(texture_height))continue;
                        for(int x=-8;x<=8;){
                            const int px=static_cast<int>(center_x)+x;
                            if(px<0||px>=static_cast<int>(texture_width)||!NoShootReticlePixel(x,y)){++x;continue;}
                            const int first=px;std::uint32_t count=0;
                            while(x<=8&&static_cast<int>(center_x)+x<static_cast<int>(texture_width)){
                                const auto color=NoShootReticlePixel(x,y);if(!color)break;
                                pixels[count++]=color;++x;
                            }
                            D3D11_BOX box{static_cast<UINT>(first),static_cast<UINT>(py),0U,
                                static_cast<UINT>(first)+count,static_cast<UINT>(py)+1U,1U};
                            d3d11.context()->UpdateSubresource(textures[eye].Get(),0,&box,pixels.data(),count*4U,0);
                        }
                    }
                    continue;
                }
                for (int offset = -1; offset <= 1; ++offset) {
                    const int horizontal_y = static_cast<int>(center_y) + offset;
                    const int vertical_x = static_cast<int>(center_x) + offset;
                    if (horizontal_y >= 0 && horizontal_y < static_cast<int>(texture_height)) {
                        draw_horizontal(
                            eye, center_x, static_cast<std::uint32_t>(horizontal_y),
                            kOutlineRadius, kBlack);
                    }
                    if (vertical_x >= 0 && vertical_x < static_cast<int>(texture_width)) {
                        draw_vertical(
                            eye, static_cast<std::uint32_t>(vertical_x), center_y,
                            kOutlineRadius, kBlack);
                    }
                }
                draw_horizontal(eye, center_x, center_y, kInnerRadius, kWhite);
                draw_vertical(eye, center_x, center_y, kInnerRadius, kWhite);
            }
        };
        if (!EnsureTextures(
                d3d11, frame, textures, texture_width, texture_height, texture_format)) {
            return false;
        }
        if (frame.transport == d3d9::StereoFrameTransport::d3d9ex_shared_texture) {
            const std::array<ID3D11Texture2D*, 2> destination{
                textures[0].Get(), textures[1].Get()};
            if (!shared_bridge.CopyFrame(
                    d3d11.device(), d3d11.context(), frame,
                    destination, shared_timing)) {
                SetError(std::string(shared_bridge.last_error()));
                return false;
            }
            upload_ms = shared_timing.open_ms + shared_timing.copy_queue_ms;
            draw_gameplay_reticle();
            draw_hud_text();
            return true;
        }
        const auto& left = frame.eyes[0];
        const auto& right = frame.eyes[1];
        const bool flat_theater =
            frame.presentation_mode == d3d9::FramePresentationMode::flat_theater;
        const std::size_t left_required = static_cast<std::size_t>(left.stride) * left.height;
        const std::size_t right_required = static_cast<std::size_t>(right.stride) * right.height;
        const bool borrowed = frame.transport ==
            d3d9::StereoFrameTransport::classic_d3d9_locked_systemmem;
        if ((!borrowed &&
             (left.pixels.size() < left_required || right.pixels.size() < right_required)) ||
            (borrowed && (!left.borrowed_pixels || !right.borrowed_pixels))) {
            SetError("presenter rejected truncated stereo CPU frame pixels");
            return false;
        }
        // Full-frame hashes are evidence telemetry, not a presentation primitive.
        // Preserve them for the sampled frames that are logged while keeping them
        // off the per-frame hot path. Sampled stereo hashes also prove that the
        // retained evidence frames contain distinct left/right eye content.
        if (ShouldLogSequence(frame.capture_sequence)) {
            const auto hash_begin = std::chrono::steady_clock::now();
            left_hash = d3d9::HashBgrxSurfaceIgnoringAlpha(
                CpuEyeData(left), left.stride, left.width, left.height);
            right_hash = d3d9::HashBgrxSurfaceIgnoringAlpha(
                CpuEyeData(right), right.stride, right.width, right.height);
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
                    textures[eye].Get(), 0, &box, CpuEyeData(source), source.stride, 0);

                if (flat_ui_pointer.active &&
                    flat_ui_pointer.source_width == source.width &&
                    flat_ui_pointer.source_height == source.height) {
                    constexpr std::uint32_t kRadius = 9U;
                    const std::uint32_t center_x = std::min(
                        flat_ui_pointer.pixel_x, source.width - 1U);
                    const std::uint32_t center_y = std::min(
                        flat_ui_pointer.pixel_y, source.height - 1U);
                    runtime::FlatTheaterPointerBeam beam{};
                    const std::uint32_t beam_origin_x = flat_ui_pointer.beam_origin_valid
                        ? std::min(flat_ui_pointer.beam_origin_x, source.width - 1U)
                        : center_x;
                    const std::uint32_t beam_origin_y = flat_ui_pointer.beam_origin_valid
                        ? std::min(flat_ui_pointer.beam_origin_y, source.height - 1U)
                        : center_y;
                    if (!runtime::ComputeFlatTheaterPointerBeam(
                            beam_origin_x, beam_origin_y, center_x, center_y,
                            source.width, source.height, beam)) {
                        SetError("presenter could not build flat-theater pointer beam");
                        return false;
                    }
                    constexpr std::uint32_t kBeamPadding = 2U;
                    const std::uint32_t content_left = std::min(center_x, beam.start_x);
                    const std::uint32_t content_top = std::min(center_y, beam.start_y);
                    const std::uint32_t content_right = std::max(center_x, beam.start_x);
                    const std::uint32_t content_bottom = std::max(center_y, beam.start_y);
                    const std::uint32_t patch_left = content_left > kRadius
                        ? content_left - kRadius
                        : 0U;
                    const std::uint32_t patch_top = content_top > kRadius
                        ? content_top - kRadius
                        : 0U;
                    const std::uint32_t patch_right = std::min(
                        source.width,
                        std::max(center_x + kRadius + 1U,
                                 content_right + kBeamPadding + 1U));
                    const std::uint32_t patch_bottom = std::min(
                        source.height,
                        std::max(center_y + kRadius + 1U,
                                 content_bottom + kBeamPadding + 1U));
                    const std::uint32_t patch_width = patch_right - patch_left;
                    const std::uint32_t patch_height = patch_bottom - patch_top;
                    std::vector<std::uint8_t> patch(
                        static_cast<std::size_t>(patch_width) * patch_height * 4U);
                    for (std::uint32_t row = 0; row < patch_height; ++row) {
                        const auto* source_row = CpuEyeData(source) +
                            static_cast<std::size_t>(patch_top + row) * source.stride +
                            static_cast<std::size_t>(patch_left) * 4U;
                        std::memcpy(
                            patch.data() + static_cast<std::size_t>(row) * patch_width * 4U,
                            source_row,
                            static_cast<std::size_t>(patch_width) * 4U);
                    }
                    const auto set_pixel = [&](const std::uint32_t x,
                                               const std::uint32_t y,
                                               const std::uint8_t blue,
                                               const std::uint8_t green,
                                               const std::uint8_t red) noexcept {
                        if (x >= patch_width || y >= patch_height) return;
                        auto* pixel = patch.data() +
                            (static_cast<std::size_t>(y) * patch_width + x) * 4U;
                        pixel[0] = blue;
                        pixel[1] = green;
                        pixel[2] = red;
                        pixel[3] = 0xFFU;
                    };
                    const std::uint32_t local_x = center_x - patch_left;
                    const std::uint32_t local_y = center_y - patch_top;

                    // Projected Sense origin -> menu hit on the same theater plane.
                    std::int32_t x0 = static_cast<std::int32_t>(beam.start_x - patch_left);
                    std::int32_t y0 = static_cast<std::int32_t>(beam.start_y - patch_top);
                    const std::int32_t x1 = static_cast<std::int32_t>(local_x);
                    const std::int32_t y1 = static_cast<std::int32_t>(local_y);
                    const std::int32_t dx = std::abs(x1 - x0);
                    const std::int32_t sx = x0 < x1 ? 1 : -1;
                    const std::int32_t dy = -std::abs(y1 - y0);
                    const std::int32_t sy = y0 < y1 ? 1 : -1;
                    std::int32_t line_error = dx + dy;
                    for (;;) {
                        for (std::int32_t oy = -1; oy <= 1; ++oy) {
                            for (std::int32_t ox = -1; ox <= 1; ++ox) {
                                const std::int32_t px = x0 + ox;
                                const std::int32_t py = y0 + oy;
                                if (px >= 0 && py >= 0) {
                                    set_pixel(
                                        static_cast<std::uint32_t>(px),
                                        static_cast<std::uint32_t>(py),
                                        0xFFU, 0xF0U, 0x20U);
                                }
                            }
                        }
                        if (x0 == x1 && y0 == y1) break;
                        const std::int32_t doubled = 2 * line_error;
                        if (doubled >= dy) {
                            line_error += dy;
                            x0 += sx;
                        }
                        if (doubled <= dx) {
                            line_error += dx;
                            y0 += sy;
                        }
                    }
                    for (std::uint32_t offset = 2U; offset <= kRadius; ++offset) {
                        if (local_x >= offset) set_pixel(local_x - offset, local_y, 0, 0, 0);
                        if (local_x + offset < patch_width) set_pixel(local_x + offset, local_y, 0, 0, 0);
                        if (local_y >= offset) set_pixel(local_x, local_y - offset, 0, 0, 0);
                        if (local_y + offset < patch_height) set_pixel(local_x, local_y + offset, 0, 0, 0);
                    }
                    for (std::uint32_t offset = 3U; offset + 1U <= kRadius; ++offset) {
                        if (local_x >= offset) set_pixel(local_x - offset, local_y, 0xFFU, 0xF0U, 0x20U);
                        if (local_x + offset < patch_width) set_pixel(local_x + offset, local_y, 0xFFU, 0xF0U, 0x20U);
                        if (local_y >= offset) set_pixel(local_x, local_y - offset, 0xFFU, 0xF0U, 0x20U);
                        if (local_y + offset < patch_height) set_pixel(local_x, local_y + offset, 0xFFU, 0xF0U, 0x20U);
                    }
                    set_pixel(local_x, local_y, 0xFFU, 0xFFU, 0xFFU);
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
                    textures[eye].Get(), 0, nullptr, CpuEyeData(source), source.stride, 0);
            }
        }
        draw_gameplay_reticle();
        draw_hud_text();
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
        D3D9SharedTextureBridge shared_bridge;
        std::array<ComPtr<ID3D11Texture2D>, 2> textures{};
        std::uint32_t texture_width = 0;
        std::uint32_t texture_height = 0;
        DXGI_FORMAT texture_format = DXGI_FORMAT_UNKNOWN;
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
        runtime::UiPointerOwnership pointer_ownership;
        runtime::UiPointerGameplayGate pointer_gameplay_gate;
        runtime::EquipmentWheelHaptics wheel_haptics;
        runtime::ReloadInsertionHaptics reload_haptics;
        std::uint64_t input_sample_sequence = 0;
        std::uint64_t last_input_log_sequence = 0;
        std::uint32_t last_raw_pressed = 0;
        std::uint32_t last_raw_available = 0;
        std::uint32_t last_input_flags = 0;
        bool flat_ui_fire_release_required = false;
        std::uint64_t flat_ui_pointer_sequence = 0;
        bool flat_ui_rejection_logged = false;
        d3d9::FramePresentationMode active_presentation_mode =
            d3d9::FramePresentationMode::native_stereo;
        bool have_active_presentation_mode = false;
        PresentationCadence cadence;
        runtime::OpenVrRuntimeState previous_runtime_state{};
        bool have_previous_runtime_state = false;
        runtime::OpenVrPresentationState previous_presentation_state{};
        bool have_previous_presentation_state = false;
        std::uint64_t presentation_poll_sequence = 0;
        auto next_display_frequency_read = std::chrono::steady_clock::time_point{};
        bool display_timing_logged = false;
        struct LeaseTelemetry {
            std::uintptr_t device_id = 0;
            std::uint64_t generation = 0;
            std::uint64_t frame_sequence = 0;
            std::uint32_t slot = 0;
        };
        std::deque<LeaseTelemetry> pending_lease_telemetry;
        std::uint64_t logged_copy_fences_completed = 0;

        const auto log_retired_leases = [&](const std::uint64_t completed) noexcept {
            try {
                while (logged_copy_fences_completed < completed) {
                    ++logged_copy_fences_completed;
                    if (pending_lease_telemetry.empty()) continue;
                    const LeaseTelemetry retired = pending_lease_telemetry.front();
                    pending_lease_telemetry.pop_front();
                    std::ostringstream line;
                    line << "native_stereo_startup_event: event=lease_retired"
                         << ";thread_id=" << GetCurrentThreadId()
                         << ";device=0x" << std::hex << retired.device_id << std::dec
                         << ";generation=" << retired.generation
                         << ";frame_sequence=" << retired.frame_sequence
                         << ";slot=" << retired.slot;
                    Log(line.str());
                }
            } catch (...) {
            }
        };

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
            for (const auto& eye : eyes) {
                std::ostringstream optics;
                optics << "native_stereo_eye_optics: eye="
                       << (eye.eye == runtime::Eye::left ? "left" : "right")
                       << ";recommended=" << eye.width << 'x' << eye.height
                       << ";fov=" << eye.fov.angle_left << ',' << eye.fov.angle_right
                       << ',' << eye.fov.angle_up << ',' << eye.fov.angle_down
                       << ";eye_to_head_position=" << eye.eye_to_head.position.x
                       << ',' << eye.eye_to_head.position.y << ',' << eye.eye_to_head.position.z
                       << ";eye_to_head_orientation=" << eye.eye_to_head.orientation.x
                       << ',' << eye.eye_to_head.orientation.y << ',' << eye.eye_to_head.orientation.z
                       << ',' << eye.eye_to_head.orientation.w;
                Log(optics.str());
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
            const float initial_refresh = runtime.ReadDisplayFrequency();
            display_frequency_hz.store(
                runtime::HmdFramePacer::ValidRefresh(initial_refresh) ? initial_refresh : 0.0F,
                std::memory_order_release);
            SignalInitialization(true);
            // Compile/cache the tiny text pipeline before campaign frames.
            Log(std::string("native_hud_text_pipeline: status=") +
                (hud_text_compositor.Prepare(d3d11.device()) ? "ready" : "unavailable"));
            Log("native_stereo_presenter: status=started owner_thread=openvr+d3d11 mode=flat_theater_to_native_stereo");
            Log("openvr_gpu_handoff: native_stereo=D3D9Ex_shared_texture+CopyResource;flat_fallback=UpdateSubresource;producer_sync=nonblocking_D3D9_event_query;consumer_sync=nonblocking_D3D11_event_query;submit=Submit_TextureWithPose;handoff=PostPresentHandoff");
            if (runtime.ReadPresentationState(previous_presentation_state)) {
                have_previous_presentation_state = true;
                LogPresentationState(previous_presentation_state, "initialized");
            }
            previous_runtime_state = runtime.state();
            have_previous_runtime_state = true;
            LogRuntimeState(previous_runtime_state, "initialized");

            while (!stop_requested.load(std::memory_order_acquire)) {
                shared_bridge.Poll(d3d11.context());
                {
                    const auto shared_stats = shared_bridge.stats();
                    log_retired_leases(shared_stats.copy_fences_completed);
                    shared_frames_copied.store(
                        shared_stats.frames_copied, std::memory_order_release);
                    shared_resources_opened.store(
                        shared_stats.resources_opened, std::memory_order_release);
                    shared_copy_fences_completed.store(
                        shared_stats.copy_fences_completed, std::memory_order_release);
                    shared_open_failures.store(
                        shared_stats.open_failures, std::memory_order_release);
                    shared_copy_failures.store(
                        shared_stats.copy_failures, std::memory_order_release);
                    shared_pending_copy_fences.store(
                        shared_stats.pending_copy_fences, std::memory_order_release);
                    shared_pending_copy_fences_peak.store(
                        shared_stats.pending_copy_fences_peak, std::memory_order_release);
                }
                const auto pose_begin = std::chrono::steady_clock::now();
                // The configured refresh comes from the HMD, never a monitor
                // mode or a fixed 90 Hz assumption. Only this owner calls OpenVR.
                if (pose_begin >= next_display_frequency_read) {
                    next_display_frequency_read = pose_begin + std::chrono::seconds(1);
                    const float reported_hz = runtime.ReadDisplayFrequency();
                    const float refresh_hz = runtime::HmdFramePacer::ValidRefresh(reported_hz)
                        ? reported_hz : 0.0F;
                    if (display_frequency_hz.exchange(refresh_hz, std::memory_order_acq_rel) != refresh_hz ||
                        !display_timing_logged) {
                        display_timing_logged = true;
                        std::ostringstream line;
                        line << "openvr_display_timing: source=hmd_property;refresh_hz="
                             << refresh_hz << ";compositor_pacing=WaitGetPoses";
                        Log(line.str());
                    }
                }
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
                bool wheel_haptic_context = false;
                bool wheel_haptic_deflected = false;
                bool reload_haptic_context = false;
                if (pose_ok) {
                    bool recenter = false;
                    bool objectives = false;
                    bool auxiliary_available = false;
                    runtime::OpenVrGlobalActions actions{};
                    runtime::GameplayInputState gameplay{};
                    runtime::GameplayInputState polled_gameplay{};
                    runtime::OpenVrHandPoses hand_poses{};
                    bool input_polled = false;
                    if (input_ready.load(std::memory_order_acquire)) {
                        if (runtime.PollActions(actions, polled_gameplay, hand_poses)) {
                            input_polled = true;
                            const bool native_gameplay_active =
                                have_active_presentation_mode &&
                                active_presentation_mode ==
                                    d3d9::FramePresentationMode::native_stereo;
                            auxiliary_available = actions.recenter_active && runtime_state.focused &&
                                !dashboard_visible && tracked_poses.hmd.orientation_valid &&
                                tracked_poses.hmd.position_valid;
                            const auto auxiliary=create_gesture.Update(
                                auxiliary_available,
                                actions.recenter_held, GetTickCount64(),
                                native_gameplay_active && !polled_gameplay.weapon_radial);
                            recenter = auxiliary.recenter;
                            objectives = auxiliary.objectives;
                            if (recenter && tracked_poses.hmd.orientation_valid &&
                                tracked_poses.hmd.position_valid) {
                                flat_theater_anchor_pose = tracked_poses.hmd;
                                flat_theater_anchor_valid = true;
                                Log("native_stereo_flat_theater: status=recentered source=global_action");
                            }
                            const auto shoulder_gameplay = pointer_gameplay_gate.Filter(
                                have_active_presentation_mode && !native_gameplay_active, polled_gameplay);
                            if (!dashboard_visible && runtime_state.focused &&
                                native_gameplay_active) {
                                gameplay = shoulder_gameplay;
                                if (flat_ui_fire_release_required) {
                                    gameplay.fire_left = false;
                                    gameplay.fire_right = false;
                                    gameplay.digital_available &= ~3U;
                                    if ((polled_gameplay.digital_available & 3U) == 3U &&
                                        !polled_gameplay.fire_left && !polled_gameplay.fire_right) {
                                        flat_ui_fire_release_required = false;
                                    }
                                }
                            }
                            // Retain pre-policy availability/buttons and the
                            // shoulder result separately. Analog jitter does
                            // not trigger logging; the heartbeat still records
                            // both sticks at least once per 90 successful polls.
                            ++input_sample_sequence;
                            std::uint32_t raw_pressed = 0;
                            for (std::size_t i = 0; i < runtime::kGameplayDigitalMembers.size(); ++i)
                                if (polled_gameplay.*runtime::kGameplayDigitalMembers[i]) raw_pressed |= 1U << i;
                            for (std::size_t i = 0; i < polled_gameplay.equipment_select.size(); ++i)
                                if (polled_gameplay.equipment_select[i])
                                    raw_pressed |= 1U << (runtime::kGameplayDigitalMembers.size() + i);
                            int raw_turn_sector = -1;
                            if (polled_gameplay.turn_available && std::isfinite(polled_gameplay.turn.x) &&
                                std::isfinite(polled_gameplay.turn.y)) {
                                const float magnitude = std::hypot(polled_gameplay.turn.x, polled_gameplay.turn.y);
                                raw_turn_sector = 9; // between center and selection thresholds
                                if (magnitude <= .25F) raw_turn_sector = 0;
                                else if (magnitude >= .65F) {
                                    float angle = std::atan2(polled_gameplay.turn.x, polled_gameplay.turn.y) * 57.295779513F;
                                    if (angle < 0) angle += 360.0F;
                                    raw_turn_sector = 1 + static_cast<int>(std::floor((angle + 22.5F) / 45.0F)) % 8;
                                }
                            }
                            const std::uint32_t input_flags =
                                (polled_gameplay.weapon_radial ? 1U : 0U) |
                                (polled_gameplay.radial_available ? 1U << 1 : 0U) |
                                (polled_gameplay.move_available ? 1U << 2 : 0U) |
                                (polled_gameplay.turn_available ? 1U << 3 : 0U) |
                                (actions.ui_pointer_left_active ? 1U << 4 : 0U) |
                                (actions.ui_pointer_left ? 1U << 5 : 0U) |
                                (actions.ui_pointer_right_active ? 1U << 6 : 0U) |
                                (actions.ui_pointer_right ? 1U << 7 : 0U) |
                                (pointer_gameplay_gate.left_blocked() ? 1U << 8 : 0U) |
                                (pointer_gameplay_gate.right_blocked() ? 1U << 9 : 0U) |
                                (native_gameplay_active ? 1U << 10 : 0U) |
                                (runtime_state.focused ? 1U << 11 : 0U) |
                                (dashboard_visible ? 1U << 12 : 0U) |
                                (flat_ui_fire_release_required ? 1U << 13 : 0U) |
                                (static_cast<std::uint32_t>(raw_turn_sector + 1) << 14);
                            const bool input_changed = last_input_log_sequence == 0 ||
                                raw_pressed != last_raw_pressed ||
                                polled_gameplay.digital_available != last_raw_available ||
                                input_flags != last_input_flags;
                            if (input_changed || input_sample_sequence - last_input_log_sequence >= 90) {
                                std::ostringstream detail;
                                detail << "openvr_raw_input: status=observed;sample_sequence=" << input_sample_sequence
                                       << ";reason=" << (input_changed ? "change" : "heartbeat")
                                       << ";raw_available_mask=0x" << std::hex << polled_gameplay.digital_available
                                       << ";raw_pressed_mask=0x" << raw_pressed
                                       << ";forwarded_available_mask=0x" << gameplay.digital_available << std::dec
                                       << ";weapon_radial=" << polled_gameplay.weapon_radial
                                       << ";radial_available=" << polled_gameplay.radial_available
                                       << ";move_available=" << polled_gameplay.move_available
                                       << ";turn_available=" << polled_gameplay.turn_available
                                       << ";move=" << polled_gameplay.move.x << ',' << polled_gameplay.move.y
                                       << ";turn=" << polled_gameplay.turn.x << ',' << polled_gameplay.turn.y
                                       << ";turn_sector=" << raw_turn_sector
                                       << ";raw_reload=" << polled_gameplay.reload
                                       << ";raw_jump=" << polled_gameplay.jump
                                       << ";raw_kick=" << polled_gameplay.kick
                                       << ";raw_run=" << polled_gameplay.run
                                       << ";raw_crouch=" << polled_gameplay.crouch
                                       << ";raw_fire_left=" << polled_gameplay.fire_left
                                       << ";raw_fire_right=" << polled_gameplay.fire_right
                                       << ";global_left_available=" << actions.ui_pointer_left_active
                                       << ";global_left_held=" << actions.ui_pointer_left
                                       << ";global_right_available=" << actions.ui_pointer_right_active
                                       << ";global_right_held=" << actions.ui_pointer_right
                                       << ";left_shoulder_blocked=" << pointer_gameplay_gate.left_blocked()
                                       << ";right_shoulder_blocked=" << pointer_gameplay_gate.right_blocked()
                                       << ";raw_interact=" << polled_gameplay.interact
                                       << ";raw_weapon_next=" << polled_gameplay.weapon_next
                                       << ";forwarded_interact=" << gameplay.interact
                                       << ";forwarded_weapon_next=" << gameplay.weapon_next
                                       << ";native_gameplay=" << native_gameplay_active
                                       << ";focused=" << runtime_state.focused
                                       << ";dashboard=" << dashboard_visible
                                       << ";forwarded_active=" << gameplay.active;
                                Log(detail.str());
                                last_input_log_sequence = input_sample_sequence;
                                last_raw_pressed = raw_pressed;
                                last_raw_available = polled_gameplay.digital_available;
                                last_input_flags = input_flags;
                            }
                        }
                    }
                    // Keep pose/render fallback independent from gameplay ownership.
                    if (!tracked_poses.hmd.orientation_valid || !tracked_poses.hmd.position_valid)
                        gameplay = {};
                    if (gameplay.active != gameplay_context_active) {
                        ++gameplay_context_generation;
                        gameplay_context_active = gameplay.active;
                    }
                    gameplay.input_context_generation = gameplay_context_generation;
                    gameplay.objectives_event_available = auxiliary_available;
                    if (!input_polled)
                        (void)create_gesture.Update(false,false,GetTickCount64());
                    const bool left_handgrip_valid = input_polled &&
                        hand_poses.left_grip_active &&
                        hand_poses.left_grip.position_valid &&
                        hand_poses.left_grip.orientation_valid;
                    const bool right_handgrip_valid = input_polled &&
                        hand_poses.right_grip_active &&
                        hand_poses.right_grip.position_valid &&
                        hand_poses.right_grip.orientation_valid;
                    reload_haptic_context=left_handgrip_valid&&right_handgrip_valid&&
                        runtime::ReloadHapticInputAvailable(polled_gameplay,gameplay.active,
                            input_polled,runtime_state.focused,dashboard_visible,
                            tracked_poses.hmd,hand_poses.left_grip,hand_poses.right_grip)&&
                        !actions.pause&&!actions.ui_back&&!actions.ui_accept&&!recenter&&!objectives;
                    if(reload_haptic_context){
                        runtime::OpenVrPresentationState current{};
                        reload_haptic_context=runtime.ReadPresentationState(current)&&
                            current.can_render_scene&&current.scene_focus_process_id==current.process_id&&
                            current.input_available&&!current.dashboard_visible&&!current.should_pause;
                    }
                    // Current raw held Triangle and finite right-stick input
                    // gate captured wheel metadata; a delayed wheel cannot buzz
                    // after release. The captured highlight remains authoritative.
                    wheel_haptic_context = right_handgrip_valid &&
                        runtime::WheelHapticInputAvailable(polled_gameplay,
                            gameplay.active, input_polled, runtime_state.focused,
                            dashboard_visible, tracked_poses.hmd, hand_poses.right_grip) &&
                        !actions.pause && !actions.ui_back && !actions.ui_accept && !recenter && !objectives;
                    if (wheel_haptic_context) {
                        runtime::OpenVrPresentationState current{};
                        wheel_haptic_context = runtime.ReadPresentationState(current) &&
                            current.can_render_scene && current.scene_focus_process_id == current.process_id &&
                            current.input_available && !current.dashboard_visible && !current.should_pause;
                        // The mapper clears selection at <= .55. Suppress output
                        // immediately if raw input has already returned to center;
                        // do not derive a sector or selection from that stick.
                        wheel_haptic_deflected = std::hypot(polled_gameplay.turn.x, polled_gameplay.turn.y) > .55F;
                    }
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
                    const auto pointer_hand = pointer_ownership.Update(
                        pointer_allowed && !recenter, input_polled,
                        actions.ui_select_left_pressed, actions.ui_select_right_pressed,
                        left_aim_valid || left_handgrip_valid,
                        right_aim_valid || right_handgrip_valid,
                        hand_poses.left_aim_active || hand_poses.left_grip_active,
                        hand_poses.right_aim_active || hand_poses.right_grip_active);
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
                            pointer.claim = pointer_ownership.claim();
                            pointer.u = projected.u;
                            pointer.v = projected.v;
                            pointer.pixel_x = projected.pixel_x;
                            pointer.pixel_y = projected.pixel_y;
                            pointer.beam_origin_valid = projected.beam_origin_valid;
                            pointer.beam_origin_x = projected.beam_origin_x;
                            pointer.beam_origin_y = projected.beam_origin_y;
                            pointer.source_width = active_source_width;
                            pointer.source_height = active_source_height;
                            pointer.ray_distance_m = projected.ray_distance_m;
                        };
                        // A valid tip that misses the screen must not silently
                        // switch to a different ray or the other controller.
                        if (pointer_hand == runtime::UiPointerHand::right)
                            try_pointer(right_aim_valid ? hand_poses.right_aim : hand_poses.right_grip,
                                right_aim_valid || right_handgrip_valid, false);
                        else if (pointer_hand == runtime::UiPointerHand::left)
                            try_pointer(left_aim_valid ? hand_poses.left_aim : hand_poses.left_grip,
                                left_aim_valid || left_handgrip_valid, true);
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
                            runtime::UiPointerSelect(pointer_hand, actions.ui_select_left, actions.ui_select_right);
                        pointer.select_pressed = pointer.active &&
                            runtime::UiPointerSelect(pointer_hand,
                                actions.ui_select_left_pressed, actions.ui_select_right_pressed);
                        if (pointer.select_down) flat_ui_fire_release_required = true;
                    }
                    flat_ui_pointer = pointer;
                    if (!PublishFlatUiPointer(flat_ui_pointer)) {
                        pointer_ownership.Suspend();
                        if (flat_ui_pointer.active && !flat_ui_rejection_logged)
                            Log("flat_ui_pointer: status=released;reason=game_input_rejected_or_mouse_priority");
                        flat_ui_rejection_logged = true;
                        flat_ui_pointer = {};
                    } else if (flat_ui_pointer.active) flat_ui_rejection_logged = false;
                    ++flat_ui_pointer_sequence;
                    if (flat_ui_pointer.active &&
                        ShouldLogSequence(flat_ui_pointer_sequence)) {
                        std::ostringstream line;
                        line << "flat_ui_pointer: status=hit"
                             << ";hand=" << (flat_ui_pointer.using_left_hand ? "left" : "right")
                             << ";activation=automatic;pose=tip_with_grip_fallback"
                             << ";u=" << std::fixed << std::setprecision(4) << flat_ui_pointer.u
                             << ";v=" << flat_ui_pointer.v
                             << ";pixel=" << flat_ui_pointer.pixel_x << ',' << flat_ui_pointer.pixel_y
                             << ";beam_origin=";
                        if (flat_ui_pointer.beam_origin_valid) {
                            line << flat_ui_pointer.beam_origin_x << ','
                                 << flat_ui_pointer.beam_origin_y;
                        } else {
                            line << "invalid";
                        }
                        line
                             << ";source=" << flat_ui_pointer.source_width << 'x'
                             << flat_ui_pointer.source_height
                             << ";select=" << (flat_ui_pointer.select_down ? "true" : "false")
                             << ";smoothing=0.40"
                             << ";visual=cyan_beam_reticle"
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
                        objectives_pending.Observe(auxiliary_available,
                            gameplay.active && !gameplay.weapon_radial, objectives);
                        // Finger samples are gameplay presentation values. A
                        // failed poll or focus/menu/tracking loss must discard
                        // them, even if the skeletal source remains active.
                        latest_left_fingers = gameplay.active && left_handgrip_valid
                            ? hand_poses.left_fingers : runtime::FingerTrackingState{};
                        latest_right_fingers = gameplay.active && right_handgrip_valid
                            ? hand_poses.right_fingers : runtime::FingerTrackingState{};
                        latest_ui_actions_allowed = input_polled && runtime_state.focused && !dashboard_visible;
                        latest_ui_select_left = actions.ui_select_left;
                        latest_ui_select_right = actions.ui_select_right;
                        latest_ui_accept = latest_ui_actions_allowed && actions.ui_accept;
                        latest_ui_back = latest_ui_actions_allowed && actions.ui_back;
                        if (!latest_ui_actions_allowed) {
                            ui_select_left_pressed_pending = false;
                            ui_select_right_pressed_pending = false;
                            ui_accept_pressed_pending = false;
                            ui_back_pressed_pending = false;
                            pause_pressed_pending = false;
                        }
                        if (latest_ui_actions_allowed && actions.ui_select_left_pressed) {
                            ui_select_left_pressed_pending = true;
                        }
                        if (latest_ui_actions_allowed && actions.ui_select_right_pressed) {
                            ui_select_right_pressed_pending = true;
                        }
                        if (latest_ui_actions_allowed && actions.ui_accept_pressed) {
                            ui_accept_pressed_pending = true;
                        }
                        if (latest_ui_actions_allowed && actions.ui_back_pressed) {
                            ui_back_pressed_pending = true;
                        }
                        if (latest_ui_actions_allowed && actions.pause_pressed) {
                            pause_pressed_pending = true;
                        }
                        ++pose_sequence;
                        if (recenter) recenter_pending = true;
                    }
                    ++pose_updates;
                } else {
                    (void)create_gesture.Update(false,false,GetTickCount64());
                    pointer_ownership.Suspend();
                    flat_ui_pointer = {};
                    PublishFlatUiPointer(flat_ui_pointer);
                    std::lock_guard lock(tracking_mutex);
                    latest_gameplay = {};
                    objectives_pending.Observe(false,false,false);
                    latest_left_fingers = {};
                    latest_right_fingers = {};
                    if (gameplay_context_active) ++gameplay_context_generation;
                    gameplay_context_active = false;
                    latest_pose = {};
                    latest_ui_actions_allowed = false;
                    latest_ui_accept = latest_ui_back = false;
                    ui_select_left_pressed_pending = ui_select_right_pressed_pending = false;
                    ui_accept_pressed_pending = ui_back_pressed_pending = false;
                    pause_pressed_pending = false;
                }

                wheel_haptics.UpdateNativeContext(feedback_context.Snapshot(),
                    std::chrono::steady_clock::now());
                wheel_haptics.UpdateContext(wheel_haptic_context, gameplay_context_generation,
                    std::chrono::steady_clock::now());
                reload_haptics.UpdateContext(reload_haptic_context,gameplay_context_generation,
                    feedback_context.Snapshot(),std::chrono::steady_clock::now());
                reload_haptics.UpdateOwner(reload_feedback_owner.load(std::memory_order_acquire));
                d3d9::StereoCpuFrame frame{};
                const bool have_new_frame = mailbox.WaitConsumeLatest(
                    frame, cadence.scene_submission_allowed(dashboard_visible) ? 0U : 2U);
                std::uint64_t left_hash = 0;
                std::uint64_t right_hash = 0;
                double hash_ms = 0.0;
                double upload_ms = 0.0;
                D3D9SharedTextureCopyTiming shared_timing{};
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
                            shared_bridge.ResetOpenedResources();
                            texture_width = 0;
                            texture_height = 0;
                            texture_format = DXGI_FORMAT_UNKNOWN;
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
                        const bool shared_frame = frame.transport ==
                            d3d9::StereoFrameTransport::d3d9ex_shared_texture;
                        const auto shared_stats_before = shared_bridge.stats();
                        const bool uploaded = UploadFrame(
                                d3d11, shared_bridge, frame, flat_ui_pointer,
                                textures, texture_width, texture_height, texture_format,
                                left_hash, right_hash, hash_ms, upload_ms, shared_timing);
                        const auto shared_stats_after = shared_bridge.stats();
                        log_retired_leases(shared_stats_after.copy_fences_completed);
                        if (uploaded) {
                            // Feedback observes only fresh accepted captures;
                            // compositor repeats, rejected uploads and stale
                            // metadata never produce events. Resource replacement
                            // and context reacquisition establish silent baselines.
                            if (frame.presentation_mode == d3d9::FramePresentationMode::native_stereo &&
                                frame.hud_text.frame_sequence == frame.capture_sequence &&
                                frame.render_pose_sequence != 0 &&
                                frame.render_hmd_pose.orientation_valid && frame.render_hmd_pose.position_valid) {
                                // Upload may span a native loss/reacquire edge.
                                wheel_haptics.UpdateNativeContext(feedback_context.Snapshot(),
                                    std::chrono::steady_clock::now());
                                const auto pulse = wheel_haptics.Observe(frame.hud_text.ui.wheel,
                                    frame.capture_sequence, frame.generation, frame.device_id,
                                    frame.capture_time, std::chrono::steady_clock::now(),
                                    frame.hud_text.feedback_context_token);
                                if (pulse && wheel_haptic_deflected) {
                                    const bool delivered = feedback_context.Matches(frame.hud_text.feedback_context_token) &&
                                        runtime.SubmitUiHapticPulse(*pulse);
                                    // No delivery failure is retained for retry;
                                    // context failures also require a fresh baseline.
                                    if (!delivered) wheel_haptics.UpdateContext(false,
                                        gameplay_context_generation, std::chrono::steady_clock::now());
                                    runtime::OptionalUiHapticDiagnostic([&] {
                                    std::ostringstream feedback;
                                    feedback << "openvr_ui_haptic: event=wheel_highlight;hand=right"
                                             << ";status=" << (delivered ? "submitted" : "dropped")
                                             << ";frame_sequence=" << frame.capture_sequence
                                             << ";device_id=" << frame.device_id
                                             << ";generation=" << frame.generation
                                             << ";context_generation=" << gameplay_context_generation
                                             << ";sector=" << frame.hud_text.ui.wheel.selected
                                             << ";duration_seconds=" << pulse->duration_seconds
                                             << ";frequency_hz=" << pulse->frequency_hz
                                             << ";amplitude=" << pulse->amplitude;
                                    Log(feedback.str());
                                    });
                                }
                                const auto reload_now=std::chrono::steady_clock::now();
                                reload_haptics.UpdateOwner(reload_feedback_owner.load(std::memory_order_acquire));
                                reload_haptics.UpdateContext(reload_haptic_context,gameplay_context_generation,
                                    feedback_context.Snapshot(),reload_now);
                                const auto reload_pulse=reload_haptics.Observe(frame.hud_text.reload_feedback,
                                    frame.capture_sequence,frame.generation,frame.device_id,
                                    frame.capture_time,reload_now,frame.hud_text.feedback_context_token);
                                if(reload_pulse){
                                    const bool delivered=feedback_context.Matches(frame.hud_text.feedback_context_token)&&
                                        reload_feedback_owner.load(std::memory_order_acquire)==frame.hud_text.reload_feedback.owner_token&&
                                        runtime.SubmitUiHapticPulse(*reload_pulse);
                                    if(!delivered)reload_haptics.UpdateContext(false,gameplay_context_generation,
                                        feedback_context.Snapshot(),std::chrono::steady_clock::now());
                                    if(frame.hud_text.reload_feedback.diagnostics)runtime::OptionalUiHapticDiagnostic([&]{
                                        std::ostringstream feedback;
                                        feedback<<"openvr_reload_haptic: event=insert_accepted;hand="
                                            <<(reload_pulse->hand==runtime::UiHapticHand::left?"left":"right")
                                            <<";status="<<(delivered?"submitted":"dropped")
                                            <<";frame_sequence="<<frame.capture_sequence
                                            <<";device_id="<<frame.device_id<<";generation="<<frame.generation
                                            <<";context_generation="<<gameplay_context_generation
                                            <<";feedback_owner="<<frame.hud_text.reload_feedback.owner_token
                                            <<";duration_seconds="<<reload_pulse->duration_seconds
                                            <<";frequency_hz="<<reload_pulse->frequency_hz
                                            <<";amplitude="<<reload_pulse->amplitude;
                                        Log(feedback.str());
                                    });
                                }
                            } else {
                                wheel_haptics.UpdateContext(false,gameplay_context_generation,
                                    std::chrono::steady_clock::now());
                                reload_haptics.UpdateContext(false,gameplay_context_generation,
                                    feedback_context.Snapshot(),std::chrono::steady_clock::now());
                            }
                            if (shared_frame) {
                                const LeaseTelemetry lease{
                                    .device_id = frame.device_id,
                                    .generation = frame.generation,
                                    .frame_sequence = frame.capture_sequence,
                                    .slot = frame.producer_slot,
                                };
                                pending_lease_telemetry.push_back(lease);
                                std::ostringstream acquired;
                                acquired << "native_stereo_startup_event: event=lease_acquired"
                                         << ";thread_id=" << GetCurrentThreadId()
                                         << ";device=0x" << std::hex << frame.device_id << std::dec
                                         << ";generation=" << frame.generation
                                         << ";frame_sequence=" << frame.capture_sequence
                                         << ";slot=" << frame.producer_slot;
                                Log(acquired.str());

                                if (shared_stats_after.resources_opened >
                                    shared_stats_before.resources_opened) {
                                    std::ostringstream opened;
                                    opened << "native_stereo_startup_event: event=shared_resource_opened"
                                           << ";thread_id=" << GetCurrentThreadId()
                                           << ";device=0x" << std::hex << frame.device_id << std::dec
                                           << ";generation=" << frame.generation
                                           << ";frame_sequence=" << frame.capture_sequence
                                           << ";slot=" << frame.producer_slot
                                           << ";count="
                                           << (shared_stats_after.resources_opened -
                                               shared_stats_before.resources_opened)
                                           << ";hr=0x0";
                                    Log(opened.str());
                                }
                                std::ostringstream queued;
                                queued << "native_stereo_startup_event: event=copy_resource_queued"
                                       << ";thread_id=" << GetCurrentThreadId()
                                       << ";device=0x" << std::hex << frame.device_id << std::dec
                                       << ";generation=" << frame.generation
                                       << ";frame_sequence=" << frame.capture_sequence
                                       << ";slot=" << frame.producer_slot
                                       << ";pending_fences="
                                       << shared_timing.pending_copy_fences;
                                Log(queued.str());
                            }
                            const bool first_presentable_frame = !cadence.presentable();
                            const bool mode_changed = !have_active_presentation_mode ||
                                active_presentation_mode != frame.presentation_mode;
                            active_capture_sequence = frame.capture_sequence;
                            active_transport_sequence = incoming_transport_sequence;
                            active_capture_time = frame.capture_time;
                            active_render_pose_sequence = frame.render_pose_sequence;
                            active_render_hmd_pose = frame.render_hmd_pose;
                            active_source_width = FrameWidth(frame);
                            active_source_height = FrameHeight(frame);
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
                                            eyes[0], FrameWidth(frame), FrameHeight(frame),
                                            texture_width, texture_height, left_placement) &&
                                        runtime::ComputeFlatTheaterEyePlacement(
                                            eyes[1], FrameWidth(frame), FrameHeight(frame),
                                            texture_width, texture_height, right_placement)) {
                                        std::ostringstream geometry;
                                        geometry << "native_stereo_flat_theater_geometry: status=active"
                                                 << ";plane_distance_m=1.500"
                                                 << ";texture=" << texture_width << 'x' << texture_height
                                                 << ";source=" << FrameWidth(frame) << 'x'
                                                 << FrameHeight(frame)
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
                                const bool frame_uses_shared_transport = frame.transport ==
                                    d3d9::StereoFrameTransport::d3d9ex_shared_texture;
                                std::ostringstream line;
                                line << "native_stereo_presenter_frame: status=new frame_sequence="
                                     << frame.capture_sequence
                                     << ";render_pose_sequence=" << frame.render_pose_sequence
                                     << ";generation=" << frame.generation
                                     << ";presentation_mode="
                                      << (frame.presentation_mode == d3d9::FramePresentationMode::flat_theater
                                              ? "flat_theater"
                                              : "native_stereo")
                                     << ";transport=";
                                if (frame_uses_shared_transport) {
                                    line << "d3d9ex_shared_texture_ring";
                                } else if (frame.transport ==
                                           d3d9::StereoFrameTransport::classic_d3d9_locked_systemmem) {
                                    line << "classic_d3d9_locked_systemmem_fallback";
                                } else {
                                    line << "cpu_bgrx";
                                }
                                line
                                     << ";left_hash=" << left_hash
                                     << ";right_hash=" << right_hash
                                     << ";distinct_eye_content="
                                     << (frame.presentation_mode == d3d9::FramePresentationMode::flat_theater
                                             ? "not_required"
                                             : (frame_uses_shared_transport ? "not_cpu_sampled" : "true"))
                                     << ";eye_resources_distinct="
                                     << (frame_uses_shared_transport ? "true" : "not_applicable")
                                     << ";distinct_check="
                                     << (frame.presentation_mode == d3d9::FramePresentationMode::flat_theater
                                              ? "not_required_flat_theater"
                                              : (frame_uses_shared_transport
                                                     ? "separate_shared_eye_resources"
                                                     : "sampled_hash"))
                                     << ";hash_mode="
                                     << (frame_uses_shared_transport ? "disabled_gpu_resident" : "sampled_telemetry")
                                     << std::fixed << std::setprecision(3)
                                     << ";hash_ms=" << hash_ms
                                     << ";upload_ms=" << upload_ms
                                     << ";shared_open_ms=" << shared_timing.open_ms
                                     << ";consumer_gpu_copy_queue_ms="
                                     << shared_timing.copy_queue_ms
                                     << ";consumer_wait_ms=" << shared_timing.consumer_wait_ms
                                     << ";consumer_pending_fences="
                                     << shared_timing.pending_copy_fences
                                     << ";frame_age_ms="
                                     << PoseAgeMilliseconds(frame.capture_time);
                                Log(line.str());
                            }
                        } else {
                            wheel_haptics.UpdateContext(false, gameplay_context_generation,
                                std::chrono::steady_clock::now());
                            ++rejected_frames;
                            Log("native_stereo_presenter_frame: status=rejected frame_sequence=" +
                                std::to_string(frame.capture_sequence) + ";error=" + ErrorCopy());
                        }
                    }
                    if (frame.transport == d3d9::StereoFrameTransport::cpu_bgrx) {
                        mailbox.Recycle(std::move(frame));
                    }
                }

                if (!pose_ok) {
                    std::lock_guard lock(tracking_mutex);
                    latest_pose = {};
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
        shared_bridge.Shutdown(d3d11.context());
        {
            const auto shared_stats = shared_bridge.stats();
            shared_frames_copied.store(shared_stats.frames_copied, std::memory_order_release);
            shared_resources_opened.store(
                shared_stats.resources_opened, std::memory_order_release);
            shared_copy_fences_completed.store(
                shared_stats.copy_fences_completed, std::memory_order_release);
            shared_open_failures.store(
                shared_stats.open_failures, std::memory_order_release);
            shared_copy_failures.store(
                shared_stats.copy_failures, std::memory_order_release);
            shared_pending_copy_fences.store(
                shared_stats.pending_copy_fences, std::memory_order_release);
            shared_pending_copy_fences_peak.store(
                shared_stats.pending_copy_fences_peak, std::memory_order_release);
            std::ostringstream summary;
            summary << "native_stereo_gpu_transport_summary: frames_copied="
                    << shared_stats.frames_copied
                    << ";resources_opened=" << shared_stats.resources_opened
                    << ";copy_fences_completed=" << shared_stats.copy_fences_completed
                    << ";copy_fence_poll_pending="
                    << shared_stats.copy_fence_poll_pending
                    << ";pending_fences_peak="
                    << shared_stats.pending_copy_fences_peak
                    << ";open_failures=" << shared_stats.open_failures
                    << ";copy_failures=" << shared_stats.copy_failures
                    << ";abandoned_on_shutdown="
                    << shared_stats.abandoned_on_shutdown
                    << std::fixed << std::setprecision(3)
                    << ";last_copy_completion_ms="
                    << shared_stats.last_copy_completion_ms
                    << ";max_copy_completion_ms="
                    << shared_stats.max_copy_completion_ms;
            Log(summary.str());
        }
        for (auto& texture : textures) texture.Reset();
        Log("native_stereo_presenter_shutdown: stage=d3d11_begin");
        hud_text_compositor.Reset();
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

void OpenVrStereoPresenter::PublishGameplayFeedbackContext(bool available) noexcept {
    impl_->feedback_context.Publish(available);
}

std::uint64_t OpenVrStereoPresenter::GameplayFeedbackContextToken() const noexcept {
    return impl_->feedback_context.Snapshot();
}

void OpenVrStereoPresenter::PublishReloadFeedbackOwner(std::uint64_t token) noexcept {
    impl_->reload_feedback_owner.store(token, std::memory_order_release);
}

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
            impl_->latest_left_fingers = {};
            impl_->latest_right_fingers = {};
            impl_->gameplay_context_generation = 0;
            impl_->gameplay_context_active = false;
            impl_->latest_ui_select_left = false;
            impl_->latest_ui_select_right = false;
            impl_->latest_ui_accept = false;
            impl_->latest_ui_back = false;
            impl_->latest_ui_actions_allowed = false;
            impl_->pose_sequence = 0;
            impl_->recenter_pending = false;
            impl_->objectives_pending = {};
            impl_->create_gesture = {};
            impl_->ui_select_left_pressed_pending = false;
            impl_->ui_select_right_pressed_pending = false;
            impl_->ui_accept_pressed_pending = false;
            impl_->ui_back_pressed_pending = false;
            impl_->pause_pressed_pending = false;
        }
        impl_->pose_updates.store(0, std::memory_order_release);
        impl_->frames_uploaded.store(0, std::memory_order_release);
        impl_->new_frame_submissions.store(0, std::memory_order_release);
        impl_->repeated_frame_submissions.store(0, std::memory_order_release);
        impl_->rejected_frames.store(0, std::memory_order_release);
        impl_->submit_failures.store(0, std::memory_order_release);
        impl_->shared_frames_copied.store(0, std::memory_order_release);
        impl_->shared_resources_opened.store(0, std::memory_order_release);
        impl_->shared_copy_fences_completed.store(0, std::memory_order_release);
        impl_->shared_open_failures.store(0, std::memory_order_release);
        impl_->shared_copy_failures.store(0, std::memory_order_release);
        impl_->shared_pending_copy_fences.store(0, std::memory_order_release);
        impl_->shared_pending_copy_fences_peak.store(0, std::memory_order_release);
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
    // Start also calls Stop: invalidate the epoch even if already unavailable.
    // Never reset to zero and accidentally reuse a previous capture token.
    impl_->feedback_context.Reset();
    impl_->reload_feedback_owner.store(0, std::memory_order_release);
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

bool OpenVrStereoPresenter::TryAcquireReusableFrame(d3d9::StereoCpuFrame& frame) noexcept {
    if (!impl_->running.load(std::memory_order_acquire)) return false;
    return impl_->mailbox.TryAcquireRecycled(frame);
}

void OpenVrStereoPresenter::RecycleFrame(d3d9::StereoCpuFrame frame) noexcept {
    if (!impl_->running.load(std::memory_order_acquire)) return;
    impl_->mailbox.Recycle(std::move(frame));
}

bool OpenVrStereoPresenter::LatestTracking(OpenVrTrackingSample& sample) noexcept {
    if (!running()) { sample = {}; return false; }
    sample = {};
    try {
        std::lock_guard lock(impl_->tracking_mutex);
        if (!impl_->running.load(std::memory_order_acquire) ||
            impl_->stop_requested.load(std::memory_order_acquire)) return false;
        if (impl_->pose_sequence == 0 || !impl_->latest_pose.orientation_valid) return false;
        sample.pose = impl_->latest_pose;
        sample.left_controller = impl_->latest_left_controller;
        sample.right_controller = impl_->latest_right_controller;
        sample.left_aim = impl_->latest_left_aim;
        sample.right_aim = impl_->latest_right_aim;
        sample.gameplay = impl_->latest_gameplay;
        sample.left_fingers = impl_->latest_left_fingers;
        sample.right_fingers = impl_->latest_right_fingers;
        sample.sequence = impl_->pose_sequence;
        sample.recenter_requested = impl_->recenter_pending;
        sample.objectives_requested = impl_->objectives_pending.Consume();
        sample.ui_select_left = impl_->latest_ui_select_left;
        sample.ui_select_right = impl_->latest_ui_select_right;
        sample.ui_select_left_pressed = impl_->ui_select_left_pressed_pending;
        sample.ui_select_right_pressed = impl_->ui_select_right_pressed_pending;
        sample.ui_accept = impl_->latest_ui_accept;
        sample.ui_back = impl_->latest_ui_back;
        sample.ui_accept_pressed = impl_->ui_accept_pressed_pending;
        sample.ui_back_pressed = impl_->ui_back_pressed_pending;
        sample.pause_pressed = impl_->pause_pressed_pending;
        sample.ui_actions_allowed = impl_->latest_ui_actions_allowed;
        impl_->recenter_pending = false;
        impl_->ui_select_left_pressed_pending = false;
        impl_->ui_select_right_pressed_pending = false;
        impl_->ui_accept_pressed_pending = false;
        impl_->ui_back_pressed_pending = false;
        impl_->pause_pressed_pending = false;
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

float OpenVrStereoPresenter::display_frequency_hz() const noexcept {
    return impl_ && impl_->running.load(std::memory_order_acquire)
        ? impl_->display_frequency_hz.load(std::memory_order_acquire) : 0.0F;
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
    result.shared_frames_copied =
        impl_->shared_frames_copied.load(std::memory_order_acquire);
    result.shared_resources_opened =
        impl_->shared_resources_opened.load(std::memory_order_acquire);
    result.shared_copy_fences_completed =
        impl_->shared_copy_fences_completed.load(std::memory_order_acquire);
    result.shared_open_failures =
        impl_->shared_open_failures.load(std::memory_order_acquire);
    result.shared_copy_failures =
        impl_->shared_copy_failures.load(std::memory_order_acquire);
    result.shared_pending_copy_fences =
        impl_->shared_pending_copy_fences.load(std::memory_order_acquire);
    result.shared_pending_copy_fences_peak =
        impl_->shared_pending_copy_fences_peak.load(std::memory_order_acquire);
    result.shutdown_complete = impl_->shutdown_complete.load(std::memory_order_acquire);
    result.mailbox = impl_->mailbox.stats();
    return result;
}

std::string OpenVrStereoPresenter::last_error() const noexcept { return impl_->ErrorCopy(); }

} // namespace cojvr::backends::openvr
