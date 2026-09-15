#pragma once

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace cojvr::runtime {

struct TelemetryContext {
    std::uint64_t factory_id = 0;
    std::uint64_t device_id = 0;
    std::uint64_t swapchain_id = 0;
    std::uint64_t generation = 0;
};

struct TelemetryEvent {
    std::string_view event{};
    TelemetryContext context{};
    std::uint64_t callback_sequence = 0;
    std::uint64_t capture_sequence = 0;
    std::uint64_t content_sequence = 0;
    std::uint64_t upload_sequence = 0;
    std::uint64_t submit_sequence = 0;
    std::uint64_t duration_us = 0;
    bool has_hresult = false;
    HRESULT hresult = S_OK;
    std::string_view runtime_result{};
    std::string_view callback{};
    std::string_view stage{};
    std::string_view detail{};
    bool has_content_changed = false;
    bool content_changed = false;
};

struct TelemetrySnapshot {
    std::uint64_t callbacks = 0;
    std::uint64_t present_callbacks = 0;
    std::uint64_t begin_scene_callbacks = 0;
    std::uint64_t end_scene_callbacks = 0;
    std::uint64_t swapchain_present_callbacks = 0;
    std::uint64_t captures = 0;
    std::uint64_t unique_content = 0;
    std::uint64_t uploads = 0;
    std::uint64_t submit_left = 0;
    std::uint64_t submit_right = 0;
    std::uint64_t capture_failures = 0;
    std::uint64_t upload_failures = 0;
    std::uint64_t wait_poses_failures = 0;
    std::uint64_t submit_left_failures = 0;
    std::uint64_t submit_right_failures = 0;
    std::string active_stage{};
    std::string last_failure_stage{};
};

struct TelemetryToken {
    std::uint64_t sequence = 0;
    std::uint64_t start_us = 0;
    bool sampled = false;
};

// Emits one-line JSON records prefixed with COJVR_EVENT. Exact counters remain
// atomic even when detailed callback/stage events are sampled.
class StructuredRunTelemetry {
public:
    StructuredRunTelemetry(
        std::filesystem::path log_path,
        std::string run_id,
        std::string build_manifest_id) noexcept;

    StructuredRunTelemetry(const StructuredRunTelemetry&) = delete;
    StructuredRunTelemetry& operator=(const StructuredRunTelemetry&) = delete;

    [[nodiscard]] std::uint64_t MonotonicMicroseconds() const noexcept;
    [[nodiscard]] static bool IsDetailedSample(std::uint64_t sequence) noexcept;

    void Emit(const TelemetryEvent& event) noexcept;
    [[nodiscard]] TelemetryToken CallbackEnter(
        std::string_view callback, const TelemetryContext& context) noexcept;
    void CallbackExit(
        std::string_view callback,
        const TelemetryContext& context,
        const TelemetryToken& token,
        HRESULT result) noexcept;

    [[nodiscard]] TelemetryToken StageBegin(
        std::string_view stage, const TelemetryContext& context) noexcept;
    void StageEnd(
        std::string_view stage,
        const TelemetryContext& context,
        const TelemetryToken& token,
        HRESULT result,
        std::string_view runtime_result = {}) noexcept;

    [[nodiscard]] std::uint64_t FramePublished(
        const TelemetryContext& context,
        std::uint64_t capture_sequence,
        std::uint64_t content_hash) noexcept;
    [[nodiscard]] TelemetryToken SubmitBegin(
        std::string_view eye, const TelemetryContext& context) noexcept;
    void SubmitEnd(
        std::string_view eye,
        const TelemetryContext& context,
        const TelemetryToken& token,
        std::string_view runtime_result) noexcept;

    void RuntimeStateChanged(
        const TelemetryContext& context,
        std::string_view state,
        std::string_view result) noexcept;
    void PeriodicSummary(const TelemetryContext& context, std::string_view detail) noexcept;
    void RunEnd(const TelemetryContext& context, std::string_view result) noexcept;
    [[nodiscard]] TelemetrySnapshot Snapshot() const noexcept;
    [[nodiscard]] TelemetrySnapshot SnapshotForDevice(std::uint64_t device_id) const noexcept;

private:
    enum class Stage : std::uint32_t { None, Capture, Upload, WaitPoses, SubmitLeft, SubmitRight };

    struct DeviceProgress {
        std::uint64_t callbacks = 0;
        std::uint64_t present_callbacks = 0;
        std::uint64_t begin_scene_callbacks = 0;
        std::uint64_t end_scene_callbacks = 0;
        std::uint64_t swapchain_present_callbacks = 0;
        std::uint64_t captures = 0;
        std::uint64_t unique_content = 0;
        std::uint64_t uploads = 0;
        std::uint64_t submit_left = 0;
        std::uint64_t submit_right = 0;
        std::uint64_t capture_failures = 0;
        std::uint64_t upload_failures = 0;
        std::uint64_t wait_poses_failures = 0;
        std::uint64_t submit_left_failures = 0;
        std::uint64_t submit_right_failures = 0;
        Stage active_stage = Stage::None;
        Stage last_failure_stage = Stage::None;
        std::uint64_t last_content_hash = 0;
        bool has_content_hash = false;
    };

    [[nodiscard]] std::uint64_t NextStageSequence(std::string_view stage) noexcept;
    [[nodiscard]] static Stage StageValue(std::string_view stage) noexcept;
    [[nodiscard]] static const char* StageName(Stage stage) noexcept;
    [[nodiscard]] std::string JsonLine(const TelemetryEvent& event) const;

    std::filesystem::path log_path_{};
    std::string run_id_{};
    std::string build_manifest_id_{};
    std::uint64_t start_counter_ = 0;
    std::uint64_t counter_frequency_ = 1;

    std::atomic_uint64_t callbacks_{0};
    std::atomic_uint64_t present_callbacks_{0};
    std::atomic_uint64_t begin_scene_callbacks_{0};
    std::atomic_uint64_t end_scene_callbacks_{0};
    std::atomic_uint64_t swapchain_present_callbacks_{0};
    std::atomic_uint64_t captures_{0};
    std::atomic_uint64_t unique_content_{0};
    std::atomic_uint64_t uploads_{0};
    std::atomic_uint64_t waits_{0};
    std::atomic_uint64_t submit_sequence_{0};
    std::atomic_uint64_t submit_left_{0};
    std::atomic_uint64_t submit_right_{0};
    std::atomic_uint64_t capture_failures_{0};
    std::atomic_uint64_t upload_failures_{0};
    std::atomic_uint64_t wait_poses_failures_{0};
    std::atomic_uint64_t submit_left_failures_{0};
    std::atomic_uint64_t submit_right_failures_{0};
    std::atomic<Stage> active_stage_{Stage::None};
    std::atomic<Stage> last_failure_stage_{Stage::None};
    mutable std::mutex progress_mutex_{};
    std::unordered_map<std::uint64_t, DeviceProgress> device_progress_{};
};

} // namespace cojvr::runtime
