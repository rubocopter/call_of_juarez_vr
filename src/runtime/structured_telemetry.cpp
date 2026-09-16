#include "runtime/structured_telemetry.hpp"

#include "runtime/log.hpp"

#include <iomanip>
#include <sstream>

namespace cojvr::runtime {
namespace {

std::string JsonEscape(std::string_view value) {
    std::ostringstream out;
    for (const unsigned char character : value) {
        switch (character) {
        case '\"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (character < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned>(character) << std::dec;
            } else {
                out << static_cast<char>(character);
            }
        }
    }
    return out.str();
}

bool Equals(std::string_view left, std::string_view right) noexcept {
    return left == right;
}

} // namespace

StructuredRunTelemetry::StructuredRunTelemetry(
    std::filesystem::path log_path,
    std::string run_id,
    std::string build_manifest_id) noexcept
    : log_path_(std::move(log_path)),
      run_id_(std::move(run_id)),
      build_manifest_id_(std::move(build_manifest_id)) {
    LARGE_INTEGER counter{};
    LARGE_INTEGER frequency{};
    if (QueryPerformanceCounter(&counter)) start_counter_ = static_cast<std::uint64_t>(counter.QuadPart);
    if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0) {
        counter_frequency_ = static_cast<std::uint64_t>(frequency.QuadPart);
    }
}

std::uint64_t StructuredRunTelemetry::MonotonicMicroseconds() const noexcept {
    LARGE_INTEGER counter{};
    if (!QueryPerformanceCounter(&counter)) return 0;
    const std::uint64_t current = static_cast<std::uint64_t>(counter.QuadPart);
    const std::uint64_t delta = current >= start_counter_ ? current - start_counter_ : 0;
    return (delta / counter_frequency_) * 1'000'000ULL +
        ((delta % counter_frequency_) * 1'000'000ULL) / counter_frequency_;
}

bool StructuredRunTelemetry::IsDetailedSample(const std::uint64_t sequence) noexcept {
    return sequence <= 10 || sequence == 60 || sequence == 300 ||
        (sequence > 300 && sequence % 1800 == 0);
}

std::string StructuredRunTelemetry::JsonLine(const TelemetryEvent& event) const {
    const TelemetrySnapshot device = SnapshotForDevice(event.context.device_id);
    std::ostringstream out;
    out << "COJVR_EVENT {"
        << "\"schema_version\":1"
        << ",\"event\":\"" << JsonEscape(event.event) << '\"'
        << ",\"run_id\":\"" << JsonEscape(run_id_) << '\"'
        << ",\"build_manifest_id\":\"" << JsonEscape(build_manifest_id_) << '\"'
        << ",\"pid\":" << GetCurrentProcessId()
        << ",\"tid\":" << GetCurrentThreadId()
        << ",\"monotonic_us\":" << MonotonicMicroseconds()
        << ",\"factory_id\":" << event.context.factory_id
        << ",\"device_id\":" << event.context.device_id
        << ",\"swapchain_id\":" << event.context.swapchain_id
        << ",\"generation\":" << event.context.generation
        << ",\"callback_sequence\":" << event.callback_sequence
        << ",\"capture_sequence\":" << event.capture_sequence
        << ",\"content_sequence\":" << event.content_sequence
        << ",\"upload_sequence\":" << event.upload_sequence
        << ",\"submit_sequence\":" << event.submit_sequence
        << ",\"duration_us\":" << event.duration_us
        << ",\"callback_count\":" << callbacks_.load(std::memory_order_relaxed)
        << ",\"present_count\":" << present_callbacks_.load(std::memory_order_relaxed)
        << ",\"begin_scene_count\":" << begin_scene_callbacks_.load(std::memory_order_relaxed)
        << ",\"end_scene_count\":" << end_scene_callbacks_.load(std::memory_order_relaxed)
        << ",\"swapchain_present_count\":"
            << swapchain_present_callbacks_.load(std::memory_order_relaxed)
        << ",\"capture_count\":" << captures_.load(std::memory_order_relaxed)
        << ",\"content_count\":" << unique_content_.load(std::memory_order_relaxed)
        << ",\"upload_count\":" << uploads_.load(std::memory_order_relaxed)
        << ",\"submit_left_count\":" << submit_left_.load(std::memory_order_relaxed)
        << ",\"submit_right_count\":" << submit_right_.load(std::memory_order_relaxed);
    out << ",\"capture_failure_count\":" << capture_failures_.load(std::memory_order_relaxed)
        << ",\"upload_failure_count\":" << upload_failures_.load(std::memory_order_relaxed)
        << ",\"wait_poses_failure_count\":" << wait_poses_failures_.load(std::memory_order_relaxed)
        << ",\"submit_left_failure_count\":" << submit_left_failures_.load(std::memory_order_relaxed)
        << ",\"submit_right_failure_count\":" << submit_right_failures_.load(std::memory_order_relaxed)
        << ",\"last_failure_stage\":\""
        << StageName(last_failure_stage_.load(std::memory_order_relaxed)) << '\"';
    out << ",\"device_callback_count\":" << device.callbacks
        << ",\"device_present_count\":" << device.present_callbacks
        << ",\"device_begin_scene_count\":" << device.begin_scene_callbacks
        << ",\"device_end_scene_count\":" << device.end_scene_callbacks
        << ",\"device_swapchain_present_count\":" << device.swapchain_present_callbacks
        << ",\"device_capture_count\":" << device.captures
        << ",\"device_content_count\":" << device.unique_content
        << ",\"device_upload_count\":" << device.uploads
        << ",\"device_submit_left_count\":" << device.submit_left
        << ",\"device_submit_right_count\":" << device.submit_right;
    if (event.has_hresult) {
        out << ",\"hresult\":\"0x" << std::hex << std::uppercase
            << std::setw(8) << std::setfill('0')
            << static_cast<std::uint32_t>(event.hresult) << std::dec << '\"';
    } else {
        out << ",\"hresult\":null";
    }
    out << ",\"runtime_result\":\"" << JsonEscape(event.runtime_result) << '\"';
    if (!event.callback.empty()) {
        out << ",\"callback\":\"" << JsonEscape(event.callback) << '\"';
    }
    if (!event.stage.empty()) {
        out << ",\"stage\":\"" << JsonEscape(event.stage) << '\"';
    }
    if (!event.detail.empty()) {
        out << ",\"detail\":\"" << JsonEscape(event.detail) << '\"';
    }
    if (event.has_content_changed) {
        out << ",\"content_changed\":" << (event.content_changed ? "true" : "false");
    }
    out << '}';
    return out.str();
}

void StructuredRunTelemetry::Emit(const TelemetryEvent& event) noexcept {
    if (event.event.empty()) return;
    try {
        AppendLogLine(log_path_, JsonLine(event));
    } catch (...) {
    }
}

TelemetryToken StructuredRunTelemetry::CallbackEnter(
    const std::string_view callback, const TelemetryContext& context) noexcept {
    const std::uint64_t sequence = callbacks_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (Equals(callback, "Present")) present_callbacks_.fetch_add(1, std::memory_order_relaxed);
    else if (Equals(callback, "BeginScene")) begin_scene_callbacks_.fetch_add(1, std::memory_order_relaxed);
    else if (Equals(callback, "EndScene")) end_scene_callbacks_.fetch_add(1, std::memory_order_relaxed);
    else if (Equals(callback, "SwapChainPresent")) {
        swapchain_present_callbacks_.fetch_add(1, std::memory_order_relaxed);
    }
    std::uint64_t device_sequence = sequence;
    try {
        std::lock_guard lock(progress_mutex_);
        DeviceProgress& progress = device_progress_[context.device_id];
        device_sequence = ++progress.callbacks;
        if (Equals(callback, "Present")) ++progress.present_callbacks;
        else if (Equals(callback, "BeginScene")) ++progress.begin_scene_callbacks;
        else if (Equals(callback, "EndScene")) ++progress.end_scene_callbacks;
        else if (Equals(callback, "SwapChainPresent")) ++progress.swapchain_present_callbacks;
    } catch (...) {
    }
    TelemetryToken token{
        sequence, MonotonicMicroseconds(), IsDetailedSample(device_sequence)};
    if (token.sampled) {
        Emit(TelemetryEvent{
            .event = "callback_enter",
            .context = context,
            .callback_sequence = sequence,
            .callback = callback,
        });
    }
    return token;
}

void StructuredRunTelemetry::CallbackExit(
    const std::string_view callback,
    const TelemetryContext& context,
    const TelemetryToken& token,
    const HRESULT result) noexcept {
    if (!token.sampled) return;
    const std::uint64_t now = MonotonicMicroseconds();
    Emit(TelemetryEvent{
        .event = "callback_exit",
        .context = context,
        .callback_sequence = token.sequence,
        .duration_us = now >= token.start_us ? now - token.start_us : 0,
        .has_hresult = true,
        .hresult = result,
        .callback = callback,
    });
}

std::uint64_t StructuredRunTelemetry::NextStageSequence(const std::string_view stage) noexcept {
    if (Equals(stage, "capture")) return captures_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (Equals(stage, "upload")) return uploads_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (Equals(stage, "wait_poses")) return waits_.fetch_add(1, std::memory_order_relaxed) + 1;
    return 0;
}

StructuredRunTelemetry::Stage StructuredRunTelemetry::StageValue(
    const std::string_view stage) noexcept {
    if (Equals(stage, "capture")) return Stage::Capture;
    if (Equals(stage, "upload")) return Stage::Upload;
    if (Equals(stage, "wait_poses")) return Stage::WaitPoses;
    if (Equals(stage, "submit_left")) return Stage::SubmitLeft;
    if (Equals(stage, "submit_right")) return Stage::SubmitRight;
    return Stage::None;
}

const char* StructuredRunTelemetry::StageName(const Stage stage) noexcept {
    switch (stage) {
    case Stage::Capture: return "capture";
    case Stage::Upload: return "upload";
    case Stage::WaitPoses: return "wait_poses";
    case Stage::SubmitLeft: return "submit_left";
    case Stage::SubmitRight: return "submit_right";
    case Stage::None: return "none";
    }
    return "unknown";
}

TelemetryToken StructuredRunTelemetry::StageBegin(
    const std::string_view stage, const TelemetryContext& context) noexcept {
    const std::uint64_t sequence = NextStageSequence(stage);
    const Stage stage_value = StageValue(stage);
    active_stage_.store(stage_value, std::memory_order_release);
    std::uint64_t device_sequence = sequence;
    try {
        std::lock_guard lock(progress_mutex_);
        DeviceProgress& progress = device_progress_[context.device_id];
        progress.active_stage = stage_value;
        if (stage_value == Stage::Capture) device_sequence = ++progress.captures;
        else if (stage_value == Stage::Upload) device_sequence = ++progress.uploads;
    } catch (...) {
    }
    TelemetryToken token{
        sequence, MonotonicMicroseconds(), IsDetailedSample(device_sequence)};
    if (token.sampled) {
        TelemetryEvent event{
            .event = Equals(stage, "capture") ? "capture_begin" :
                (Equals(stage, "upload") ? "upload_begin" : "wait_poses_begin"),
            .context = context,
            .capture_sequence = Equals(stage, "capture") ? sequence : captures_.load(),
            .content_sequence = unique_content_.load(),
            .upload_sequence = Equals(stage, "upload") ? sequence : uploads_.load(),
            .stage = stage,
        };
        Emit(event);
    }
    return token;
}

void StructuredRunTelemetry::StageEnd(
    const std::string_view stage,
    const TelemetryContext& context,
    const TelemetryToken& token,
    const HRESULT result,
    const std::string_view runtime_result) noexcept {
    const Stage stage_value = StageValue(stage);
    active_stage_.store(Stage::None, std::memory_order_release);
    if (FAILED(result)) {
        if (stage_value == Stage::Capture) capture_failures_.fetch_add(1, std::memory_order_relaxed);
        else if (stage_value == Stage::Upload) upload_failures_.fetch_add(1, std::memory_order_relaxed);
        else if (stage_value == Stage::WaitPoses) {
            wait_poses_failures_.fetch_add(1, std::memory_order_relaxed);
        }
        last_failure_stage_.store(stage_value, std::memory_order_release);
    }
    try {
        std::lock_guard lock(progress_mutex_);
        DeviceProgress& progress = device_progress_[context.device_id];
        progress.active_stage = Stage::None;
        if (FAILED(result)) {
            progress.last_failure_stage = stage_value;
            if (stage_value == Stage::Capture) ++progress.capture_failures;
            else if (stage_value == Stage::Upload) ++progress.upload_failures;
            else if (stage_value == Stage::WaitPoses) ++progress.wait_poses_failures;
        }
    } catch (...) {
    }
    if (!token.sampled) return;
    const std::uint64_t now = MonotonicMicroseconds();
    TelemetryEvent event{
        .event = Equals(stage, "capture") ? "capture_end" :
            (Equals(stage, "upload") ? "upload_end" : "wait_poses_end"),
        .context = context,
        .capture_sequence = Equals(stage, "capture") ? token.sequence : captures_.load(),
        .content_sequence = unique_content_.load(),
        .upload_sequence = Equals(stage, "upload") ? token.sequence : uploads_.load(),
        .duration_us = now >= token.start_us ? now - token.start_us : 0,
        .has_hresult = true,
        .hresult = result,
        .runtime_result = runtime_result,
        .stage = stage,
    };
    Emit(event);
}

std::uint64_t StructuredRunTelemetry::FramePublished(
    const TelemetryContext& context,
    const std::uint64_t capture_sequence,
    const std::uint64_t content_hash) noexcept {
    bool changed = false;
    std::uint64_t content_sequence = 0;
    try {
        std::lock_guard lock(progress_mutex_);
        DeviceProgress& progress = device_progress_[context.device_id];
        changed = !progress.has_content_hash || progress.last_content_hash != content_hash;
        if (changed) {
            progress.last_content_hash = content_hash;
            progress.has_content_hash = true;
            content_sequence = ++progress.unique_content;
            unique_content_.fetch_add(1, std::memory_order_relaxed);
        } else {
            content_sequence = progress.unique_content;
        }
    } catch (...) {
        content_sequence = 0;
    }
    if (IsDetailedSample(capture_sequence)) {
        Emit(TelemetryEvent{
            .event = "frame_published",
            .context = context,
            .capture_sequence = capture_sequence,
            .content_sequence = content_sequence,
            .has_content_changed = true,
            .content_changed = changed,
        });
    }
    return content_sequence;
}

TelemetryToken StructuredRunTelemetry::SubmitBegin(
    const std::string_view eye, const TelemetryContext& context) noexcept {
    const std::uint64_t sequence = submit_sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (Equals(eye, "left")) submit_left_.fetch_add(1, std::memory_order_relaxed);
    else if (Equals(eye, "right")) submit_right_.fetch_add(1, std::memory_order_relaxed);
    active_stage_.store(Equals(eye, "left") ? Stage::SubmitLeft : Stage::SubmitRight,
        std::memory_order_release);
    std::uint64_t device_sequence = sequence;
    try {
        std::lock_guard lock(progress_mutex_);
        DeviceProgress& progress = device_progress_[context.device_id];
        progress.active_stage = Equals(eye, "left") ? Stage::SubmitLeft : Stage::SubmitRight;
        device_sequence = Equals(eye, "left") ? ++progress.submit_left : ++progress.submit_right;
    } catch (...) {
    }
    return TelemetryToken{
        sequence, MonotonicMicroseconds(), IsDetailedSample(device_sequence)};
}

void StructuredRunTelemetry::SubmitEnd(
    const std::string_view eye,
    const TelemetryContext& context,
    const TelemetryToken& token,
    const std::string_view runtime_result) noexcept {
    const Stage stage = Equals(eye, "left") ? Stage::SubmitLeft : Stage::SubmitRight;
    active_stage_.store(Stage::None, std::memory_order_release);
    if (runtime_result != "0") {
        if (stage == Stage::SubmitLeft) {
            submit_left_failures_.fetch_add(1, std::memory_order_relaxed);
        } else {
            submit_right_failures_.fetch_add(1, std::memory_order_relaxed);
        }
        last_failure_stage_.store(stage, std::memory_order_release);
    }
    try {
        std::lock_guard lock(progress_mutex_);
        DeviceProgress& progress = device_progress_[context.device_id];
        progress.active_stage = Stage::None;
        if (runtime_result != "0") {
            progress.last_failure_stage = stage;
            if (stage == Stage::SubmitLeft) ++progress.submit_left_failures;
            else ++progress.submit_right_failures;
        }
    } catch (...) {
    }
    if (!token.sampled) return;
    const std::uint64_t now = MonotonicMicroseconds();
    Emit(TelemetryEvent{
        .event = Equals(eye, "left") ? "submit_left" : "submit_right",
        .context = context,
        .capture_sequence = captures_.load(),
        .content_sequence = unique_content_.load(),
        .upload_sequence = uploads_.load(),
        .submit_sequence = token.sequence,
        .duration_us = now >= token.start_us ? now - token.start_us : 0,
        .runtime_result = runtime_result,
        .stage = Equals(eye, "left") ? "submit_left" : "submit_right",
    });
}

void StructuredRunTelemetry::RuntimeStateChanged(
    const TelemetryContext& context,
    const std::string_view state,
    const std::string_view result) noexcept {
    Emit(TelemetryEvent{
        .event = "runtime_state_changed",
        .context = context,
        .runtime_result = result,
        .detail = state,
    });
}

TelemetrySnapshot StructuredRunTelemetry::Snapshot() const noexcept {
    return TelemetrySnapshot{
        .callbacks = callbacks_.load(std::memory_order_relaxed),
        .present_callbacks = present_callbacks_.load(std::memory_order_relaxed),
        .begin_scene_callbacks = begin_scene_callbacks_.load(std::memory_order_relaxed),
        .end_scene_callbacks = end_scene_callbacks_.load(std::memory_order_relaxed),
        .swapchain_present_callbacks = swapchain_present_callbacks_.load(std::memory_order_relaxed),
        .captures = captures_.load(std::memory_order_relaxed),
        .unique_content = unique_content_.load(std::memory_order_relaxed),
        .uploads = uploads_.load(std::memory_order_relaxed),
        .submit_left = submit_left_.load(std::memory_order_relaxed),
        .submit_right = submit_right_.load(std::memory_order_relaxed),
        .capture_failures = capture_failures_.load(std::memory_order_relaxed),
        .upload_failures = upload_failures_.load(std::memory_order_relaxed),
        .wait_poses_failures = wait_poses_failures_.load(std::memory_order_relaxed),
        .submit_left_failures = submit_left_failures_.load(std::memory_order_relaxed),
        .submit_right_failures = submit_right_failures_.load(std::memory_order_relaxed),
        .active_stage = StageName(active_stage_.load(std::memory_order_acquire)),
        .last_failure_stage = StageName(last_failure_stage_.load(std::memory_order_acquire)),
    };
}

TelemetrySnapshot StructuredRunTelemetry::SnapshotForDevice(
    const std::uint64_t device_id) const noexcept {
    try {
        std::lock_guard lock(progress_mutex_);
        const auto found = device_progress_.find(device_id);
        if (found == device_progress_.end()) return {};
        const DeviceProgress& progress = found->second;
        return TelemetrySnapshot{
            .callbacks = progress.callbacks,
            .present_callbacks = progress.present_callbacks,
            .begin_scene_callbacks = progress.begin_scene_callbacks,
            .end_scene_callbacks = progress.end_scene_callbacks,
            .swapchain_present_callbacks = progress.swapchain_present_callbacks,
            .captures = progress.captures,
            .unique_content = progress.unique_content,
            .uploads = progress.uploads,
            .submit_left = progress.submit_left,
            .submit_right = progress.submit_right,
            .capture_failures = progress.capture_failures,
            .upload_failures = progress.upload_failures,
            .wait_poses_failures = progress.wait_poses_failures,
            .submit_left_failures = progress.submit_left_failures,
            .submit_right_failures = progress.submit_right_failures,
            .active_stage = StageName(progress.active_stage),
            .last_failure_stage = StageName(progress.last_failure_stage),
        };
    } catch (...) {
        return {};
    }
}

void StructuredRunTelemetry::PeriodicSummary(
    const TelemetryContext& context, const std::string_view detail) noexcept {
    const TelemetrySnapshot snapshot = context.device_id == 0
        ? Snapshot() : SnapshotForDevice(context.device_id);
    std::ostringstream summary;
    summary << "callbacks=" << snapshot.callbacks
        << ";present=" << snapshot.present_callbacks
        << ";begin_scene=" << snapshot.begin_scene_callbacks
        << ";end_scene=" << snapshot.end_scene_callbacks
        << ";swapchain_present=" << snapshot.swapchain_present_callbacks
        << ";captures=" << snapshot.captures
        << ";content=" << snapshot.unique_content
        << ";uploads=" << snapshot.uploads
        << ";submit_left=" << snapshot.submit_left
        << ";submit_right=" << snapshot.submit_right
        << ";capture_failures=" << snapshot.capture_failures
        << ";upload_failures=" << snapshot.upload_failures
        << ";wait_poses_failures=" << snapshot.wait_poses_failures
        << ";submit_left_failures=" << snapshot.submit_left_failures
        << ";submit_right_failures=" << snapshot.submit_right_failures
        << ";active_stage=" << snapshot.active_stage
        << ";last_failure_stage=" << snapshot.last_failure_stage;
    if (!detail.empty()) summary << ';' << detail;
    const std::string summary_text = summary.str();
    Emit(TelemetryEvent{
        .event = "periodic_summary",
        .context = context,
        .callback_sequence = snapshot.callbacks,
        .capture_sequence = snapshot.captures,
        .content_sequence = snapshot.unique_content,
        .upload_sequence = snapshot.uploads,
        .submit_sequence = snapshot.submit_left + snapshot.submit_right,
        .stage = snapshot.active_stage,
        .detail = summary_text,
    });
}

void StructuredRunTelemetry::RunEnd(
    const TelemetryContext& context, const std::string_view result) noexcept {
    const TelemetrySnapshot snapshot = Snapshot();
    Emit(TelemetryEvent{
        .event = "run_end",
        .context = context,
        .callback_sequence = snapshot.callbacks,
        .capture_sequence = snapshot.captures,
        .content_sequence = snapshot.unique_content,
        .upload_sequence = snapshot.uploads,
        .submit_sequence = snapshot.submit_left + snapshot.submit_right,
        .runtime_result = result,
        .stage = snapshot.active_stage,
    });
}

} // namespace cojvr::runtime
