#include "runtime/structured_telemetry.hpp"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

bool Contains(const std::string& text, const std::string& value) {
    return text.find(value) != std::string::npos;
}

} // namespace

int main() {
    const std::filesystem::path log_path =
        std::filesystem::temp_directory_path() /
        (L"cojvr-telemetry-" + std::to_wstring(GetCurrentProcessId()) + L".log");
    std::error_code ignored;
    std::filesystem::remove(log_path, ignored);

    cojvr::runtime::StructuredRunTelemetry telemetry(
        log_path, "test-run", std::string(64, 'A'));
    const cojvr::runtime::TelemetryContext context{1, 2, 3, 1};
    telemetry.Emit({.event = "run_start", .context = context});
    telemetry.Emit({.event = "build/deployment_identified", .context = context});
    telemetry.Emit({.event = "device_created", .context = context});
    telemetry.Emit({.event = "generation_changed", .context = {1, 2, 4, 2}});
    telemetry.Emit({.event = "hook_installed", .context = context});
    telemetry.Emit({.event = "hook_conflict", .context = context});
    telemetry.Emit({.event = "hook_integrity_lost", .context = context});

    const auto callback = telemetry.CallbackEnter("Present", context);
    telemetry.CallbackExit("Present", context, callback, S_OK);

    const auto first_capture = telemetry.StageBegin("capture", context);
    telemetry.StageEnd("capture", context, first_capture, S_OK);
    if (telemetry.FramePublished(context, first_capture.sequence, 0x1234) != 1) {
        return Fail("first content hash did not advance content sequence");
    }
    const auto second_capture = telemetry.StageBegin("capture", context);
    telemetry.StageEnd("capture", context, second_capture, S_OK);
    if (telemetry.FramePublished(context, second_capture.sequence, 0x1234) != 1) {
        return Fail("repeated content incorrectly advanced content sequence");
    }

    const auto upload = telemetry.StageBegin("upload", context);
    telemetry.StageEnd("upload", context, upload, S_OK);
    const auto wait = telemetry.StageBegin("wait_poses", context);
    telemetry.StageEnd("wait_poses", context, wait, S_OK, "success");
    const auto left = telemetry.SubmitBegin("left", context);
    telemetry.SubmitEnd("left", context, left, "0");
    const auto right = telemetry.SubmitBegin("right", context);
    telemetry.SubmitEnd("right", context, right, "0");
    const auto failed_capture = telemetry.StageBegin("capture", context);
    telemetry.StageEnd("capture", context, failed_capture, E_FAIL, "forced_failure");
    const auto failed_right = telemetry.SubmitBegin("right", context);
    telemetry.SubmitEnd("right", context, failed_right, "7");
    telemetry.RuntimeStateChanged(context, "OpenVR", "initialized");
    telemetry.PeriodicSummary(context, "hooks_owned=true");
    telemetry.RunEnd(context, "complete");

    const auto snapshot = telemetry.Snapshot();
    const auto device_snapshot = telemetry.SnapshotForDevice(context.device_id);
    if (snapshot.callbacks != 1 || snapshot.present_callbacks != 1 ||
        snapshot.captures != 3 || snapshot.unique_content != 1 ||
        snapshot.uploads != 1 || snapshot.submit_left != 1 ||
        snapshot.submit_right != 2 || snapshot.capture_failures != 1 ||
        snapshot.submit_right_failures != 1 || snapshot.active_stage != "none" ||
        snapshot.last_failure_stage != "submit_right") {
        return Fail("telemetry exact counters do not match synthetic progression");
    }
    if (device_snapshot.callbacks != snapshot.callbacks ||
        device_snapshot.captures != snapshot.captures ||
        device_snapshot.unique_content != snapshot.unique_content ||
        device_snapshot.uploads != snapshot.uploads ||
        device_snapshot.submit_left != snapshot.submit_left ||
        device_snapshot.submit_right != snapshot.submit_right) {
        return Fail("per-device telemetry diverged from the single-device run");
    }

    std::ifstream input(log_path, std::ios::binary);
    const std::string contents{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    constexpr const char* required_events[] = {
        "run_start", "build/deployment_identified", "device_created",
        "generation_changed", "hook_installed", "hook_conflict",
        "hook_integrity_lost", "callback_enter", "callback_exit",
        "capture_begin", "capture_end", "frame_published", "upload_begin",
        "upload_end", "wait_poses_begin", "wait_poses_end", "submit_left",
        "submit_right", "runtime_state_changed", "periodic_summary", "run_end",
    };
    for (const char* event : required_events) {
        if (!Contains(contents, std::string("\"event\":\"") + event + "\"")) {
            std::filesystem::remove(log_path, ignored);
            return Fail("structured telemetry omitted a required event type");
        }
    }
    constexpr const char* required_fields[] = {
        "run_id", "build_manifest_id", "pid", "tid", "monotonic_us",
        "factory_id", "device_id", "swapchain_id", "generation",
        "callback_sequence", "capture_sequence", "content_sequence",
        "upload_sequence", "submit_sequence", "duration_us", "hresult",
        "runtime_result",
    };
    for (const char* field : required_fields) {
        if (!Contains(contents, std::string("\"") + field + "\":")) {
            std::filesystem::remove(log_path, ignored);
            return Fail("structured telemetry omitted a required field");
        }
    }

    std::filesystem::remove(log_path, ignored);
    std::cout << "structured telemetry event schema and exact counters passed\n";
    return 0;
}
