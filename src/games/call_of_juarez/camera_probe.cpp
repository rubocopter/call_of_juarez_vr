#include "games/call_of_juarez/camera_probe.hpp"

#include "backends/d3d9/hook_registry.hpp"
#include "runtime/build_identity.hpp"
#include "runtime/game_id.hpp"
#include "runtime/host_identity.hpp"
#include "runtime/vr_math.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>

namespace cojvr::games::call_of_juarez {
namespace {

constexpr std::uintptr_t kBaseCameraVtableRva = 0x0030AE1C;
constexpr std::uintptr_t kRenderCameraUpdateRva = 0x001C5BA0;
constexpr std::uintptr_t kSetFovRva = 0x001C58E0;
constexpr std::uintptr_t kRendererGlobalRva = 0x00590074;
constexpr std::ptrdiff_t kRendererCameraPrimaryOffset = 0x1AC;
constexpr std::ptrdiff_t kRendererCameraSecondaryOffset = 0x1B0;
constexpr std::ptrdiff_t kCameraLeftOffset = 0xC4;
constexpr std::ptrdiff_t kCameraUpOffset = 0xD4;
constexpr std::ptrdiff_t kCameraForwardOffset = 0xE4;
constexpr std::ptrdiff_t kCameraPositionOffset = 0xF4;
constexpr std::size_t kRenderCameraUpdateSlot = 9;
constexpr std::size_t kSetFovSlot = 10;
constexpr float kDegreesToRadians = 0.01745329251994329577F;
constexpr float kRadiansToDegrees = 57.295779513082320876F;
constexpr ULONGLONG kControlPollMilliseconds = 250;

using RenderCameraUpdateFn = void (__thiscall *)(void* camera);
using SetFovFn = void (__thiscall *)(
    void* camera,
    float fov_radians,
    std::uint32_t projection_arg_1,
    std::uint32_t projection_arg_2,
    std::uint32_t projection_flags);

struct CommandSnapshot {
    CameraProbeCommand command{};
    std::uint64_t generation = 0;
};

cojvr::backends::d3d9::HookRegistry g_camera_hooks;
std::byte* g_engine_base = nullptr;
void** g_camera_vtable = nullptr;
RenderCameraUpdateFn g_original_render_camera_update = nullptr;
SetFovFn g_original_set_fov = nullptr;
CameraProbeEventCallback g_event_callback = nullptr;
cojvr::runtime::PoseSource* g_pose_source = nullptr;
cojvr::runtime::RelativePoseTracker g_pose_tracker;
std::filesystem::path g_control_path{};
std::mutex g_command_mutex;
CameraProbeCommand g_command{};
std::uint64_t g_command_generation = 0;
ULONGLONG g_next_control_poll = 0;
FILETIME g_last_control_write{};
std::uint64_t g_last_control_size = 0;
bool g_control_identity_valid = false;
bool g_control_missing_reported = false;
std::atomic_uint64_t g_orientation_logged_generation{0};
std::atomic_uint64_t g_fov_logged_generation{0};
std::atomic_uint64_t g_passthrough_logged_generation{0};
std::atomic_uint64_t g_recenter_command_generation{0};
std::atomic_uint64_t g_last_hmd_logged_sequence{0};
std::atomic_uint64_t g_hmd_logged_count{0};
std::atomic_bool g_initialized{false};
std::atomic_bool g_installed{false};

struct RenderUpdateOverride {
    void* camera = nullptr;
    CameraProbeVector left{};
    CameraProbeVector up{};
    CameraProbeVector forward{};
    bool orientation_active = false;
    bool orientation_applied = false;
    bool tracking_source = false;
    std::uint64_t pose_sequence = 0;
};

thread_local RenderUpdateOverride g_render_update_override{};

void EmitEvent(const char* event, const char* result, const std::string& detail) noexcept {
    try {
        if (g_event_callback) g_event_callback(event, result, detail.c_str());
    } catch (...) {
    }
}

void SetError(std::string* error, std::string_view value) noexcept {
    if (!error) return;
    try {
        *error = value;
    } catch (...) {
    }
}

std::optional<std::size_t> ValueOffset(std::string_view text, std::string_view key) noexcept {
    try {
        const std::string needle = "\"" + std::string(key) + "\"";
        const std::size_t key_offset = text.find(needle);
        if (key_offset == std::string_view::npos) return std::nullopt;
        const std::size_t colon = text.find(':', key_offset + needle.size());
        if (colon == std::string_view::npos) return std::nullopt;
        std::size_t value = colon + 1;
        while (value < text.size() &&
               (text[value] == ' ' || text[value] == '\t' || text[value] == '\r' ||
                text[value] == '\n')) {
            ++value;
        }
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

bool ReadBool(
    std::string_view text,
    std::string_view key,
    bool& value,
    bool& present) noexcept {
    present = false;
    const auto offset = ValueOffset(text, key);
    if (!offset) return true;
    present = true;
    const std::string_view tail = text.substr(*offset);
    if (tail.starts_with("true")) {
        value = true;
        return true;
    }
    if (tail.starts_with("false")) {
        value = false;
        return true;
    }
    return false;
}

bool ReadFloat(
    std::string_view text,
    std::string_view key,
    float& value,
    bool& present) noexcept {
    present = false;
    const auto offset = ValueOffset(text, key);
    if (!offset) return true;
    present = true;
    const char* begin = text.data() + *offset;
    const char* end = text.data() + text.size();
    const auto parsed = std::from_chars(begin, end, value, std::chars_format::general);
    return parsed.ec == std::errc{} && parsed.ptr != begin && std::isfinite(value);
}

float Length(const CameraProbeVector value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

CameraProbeVector Normalize(const CameraProbeVector value, const CameraProbeVector fallback) noexcept {
    const float length = Length(value);
    if (!std::isfinite(length) || length <= 1.0e-5F) return fallback;
    const float inverse = 1.0F / length;
    return {value.x * inverse, value.y * inverse, value.z * inverse};
}

float Dot(const CameraProbeVector lhs, const CameraProbeVector rhs) noexcept {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

CameraProbeVector Cross(const CameraProbeVector lhs, const CameraProbeVector rhs) noexcept {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

CameraProbeVector RotateAroundAxis(
    const CameraProbeVector value,
    CameraProbeVector axis,
    const float radians) noexcept {
    axis = Normalize(axis, {0.0F, 1.0F, 0.0F});
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float projection = Dot(axis, value) * (1.0F - cosine);
    const CameraProbeVector cross = Cross(axis, value);
    return {
        value.x * cosine + cross.x * sine + axis.x * projection,
        value.y * cosine + cross.y * sine + axis.y * projection,
        value.z * cosine + cross.z * sine + axis.z * projection,
    };
}

bool IsReadable(const void* address, const std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(address, &memory, sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0 ||
        (memory.Protect & PAGE_NOACCESS) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto region_start = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
    const auto region_end = region_start + memory.RegionSize;
    return start >= region_start && start <= region_end && size <= region_end - start;
}

bool IsWritable(const void* address, const std::size_t size) noexcept {
    if (!IsReadable(address, size)) return false;
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(address, &memory, sizeof(memory)) != sizeof(memory)) return false;
    const DWORD protection = memory.Protect & 0xffU;
    return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

bool RendererReferencesCamera(void* camera) noexcept {
    if (!camera || !g_engine_base) {
        return false;
    }
    auto** renderer_global = reinterpret_cast<void**>(g_engine_base + kRendererGlobalRva);
    if (!IsReadable(renderer_global, sizeof(void*))) return false;
    auto* renderer = static_cast<std::byte*>(*renderer_global);
    if (!renderer || !IsReadable(renderer + kRendererCameraPrimaryOffset, sizeof(void*)) ||
        !IsReadable(renderer + kRendererCameraSecondaryOffset, sizeof(void*))) {
        return false;
    }
    const auto primary = *reinterpret_cast<void**>(renderer + kRendererCameraPrimaryOffset);
    const auto secondary = *reinterpret_cast<void**>(renderer + kRendererCameraSecondaryOffset);
    return primary == camera || secondary == camera;
}

bool SameControlIdentity(const WIN32_FILE_ATTRIBUTE_DATA& attributes) noexcept {
    return g_control_identity_valid &&
        CompareFileTime(&attributes.ftLastWriteTime, &g_last_control_write) == 0 &&
        (static_cast<std::uint64_t>(attributes.nFileSizeLow) |
         (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32)) ==
        g_last_control_size;
}

void SaveControlIdentity(const WIN32_FILE_ATTRIBUTE_DATA& attributes) noexcept {
    g_last_control_write = attributes.ftLastWriteTime;
    g_last_control_size = static_cast<std::uint64_t>(attributes.nFileSizeLow) |
        (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32);
    g_control_identity_valid = true;
}

void DisableCommandLocked(const char* event, const char* result, std::string detail) noexcept {
    g_command = {};
    ++g_command_generation;
    detail += ";generation=" + std::to_string(g_command_generation);
    EmitEvent(event, result, detail);
}

void RefreshCommandLocked() noexcept {
    const ULONGLONG now = GetTickCount64();
    if (now < g_next_control_poll) return;
    g_next_control_poll = now + kControlPollMilliseconds;

    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExW(g_control_path.c_str(), GetFileExInfoStandard, &attributes)) {
        if (!g_control_missing_reported || g_command.enabled) {
            g_control_missing_reported = true;
            g_control_identity_valid = false;
            DisableCommandLocked(
                "camera_probe_control_missing", "passthrough",
                "control_path=" + g_control_path.string());
        }
        return;
    }
    g_control_missing_reported = false;
    if (SameControlIdentity(attributes)) return;
    SaveControlIdentity(attributes);

    try {
        std::ifstream input(g_control_path, std::ios::binary);
        if (!input) {
            DisableCommandLocked(
                "camera_probe_control_rejected", "read_failed",
                "control_path=" + g_control_path.string());
            return;
        }
        const std::string contents{
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        CameraProbeCommand parsed{};
        std::string error;
        if (!ParseCameraProbeCommand(contents, parsed, &error)) {
            DisableCommandLocked(
                "camera_probe_control_rejected", "invalid", "reason=" + error);
            return;
        }
        g_command = parsed;
        ++g_command_generation;
        std::ostringstream detail;
        detail << "generation=" << g_command_generation
               << ";enabled=" << (parsed.enabled ? "true" : "false")
               << ";fov=";
        if (parsed.override_fov) detail << parsed.fov_degrees;
        else detail << "natural";
        detail << ";yaw=" << parsed.yaw_degrees
               << ";pitch=" << parsed.pitch_degrees
               << ";tracking_enabled=" << (parsed.tracking_enabled ? "true" : "false")
               << ";recenter=" << (parsed.recenter ? "true" : "false");
        EmitEvent("camera_probe_control_loaded", "accepted", detail.str());
    } catch (...) {
        DisableCommandLocked(
            "camera_probe_control_rejected", "exception", "control_parse_exception");
    }
}

CommandSnapshot CurrentCommand() noexcept {
    try {
        std::lock_guard lock(g_command_mutex);
        RefreshCommandLocked();
        return {g_command, g_command_generation};
    } catch (...) {
        return {};
    }
}

bool FirstForGeneration(std::atomic_uint64_t& marker, const std::uint64_t generation) noexcept {
    if (generation == 0) return false;
    std::uint64_t observed = marker.load(std::memory_order_acquire);
    while (observed != generation) {
        if (marker.compare_exchange_weak(
                observed, generation, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
    }
    return false;
}

std::string VectorText(const CameraProbeVector value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(4)
        << '(' << value.x << ',' << value.y << ',' << value.z << ')';
    return out.str();
}

bool ShouldLogHmdSequence(const std::uint64_t sequence) noexcept {
    if (sequence == 0) return false;
    const std::uint64_t previous = g_last_hmd_logged_sequence.exchange(
        sequence, std::memory_order_acq_rel);
    if (previous == sequence) return false;
    const std::uint64_t count = g_hmd_logged_count.fetch_add(1, std::memory_order_acq_rel) + 1;
    return count <= 8 || (count % 90) == 0;
}

struct RelativeAngles {
    float yaw_degrees = 0.0F;
    float pitch_degrees = 0.0F;
};

RelativeAngles HeadAngles(const cojvr::runtime::Quaternion orientation) noexcept {
    const auto tracked_forward = cojvr::runtime::RotateVector(
        orientation, {0.0F, 0.0F, -1.0F});
    const float local_x = tracked_forward.x;
    const float local_y = tracked_forward.y;
    const float local_z = -tracked_forward.z;
    return {
        std::atan2(local_x, local_z) * kRadiansToDegrees,
        std::atan2(local_y, std::sqrt(local_x * local_x + local_z * local_z)) *
            kRadiansToDegrees,
    };
}

void __fastcall HookRenderCameraUpdate(void* camera, void*) {
    const auto original = g_original_render_camera_update;
    if (!original) return;
    const CommandSnapshot snapshot = CurrentCommand();
    if (!camera) {
        original(camera);
        return;
    }

    auto* bytes = static_cast<std::byte*>(camera);
    auto* left = reinterpret_cast<CameraProbeVector*>(bytes + kCameraLeftOffset);
    auto* up = reinterpret_cast<CameraProbeVector*>(bytes + kCameraUpOffset);
    auto* forward = reinterpret_cast<CameraProbeVector*>(bytes + kCameraForwardOffset);
    auto* position = reinterpret_cast<CameraProbeVector*>(bytes + kCameraPositionOffset);
    const bool writable_basis = IsWritable(left, sizeof(CameraProbeVector)) &&
        IsWritable(up, sizeof(CameraProbeVector)) &&
        IsWritable(forward, sizeof(CameraProbeVector)) &&
        IsReadable(position, sizeof(CameraProbeVector));

    cojvr::runtime::Pose relative_pose{};
    std::uint64_t pose_sequence = 0;
    bool tracking_active = false;
    bool recentered = false;
    if (snapshot.command.tracking_enabled && g_pose_source) {
        g_pose_tracker.SetEnabled(true);
        if (snapshot.command.recenter &&
            FirstForGeneration(g_recenter_command_generation, snapshot.generation)) {
            g_pose_tracker.RequestRecenter();
        }
        cojvr::runtime::PoseSample sample{};
        if (g_pose_source->TryGetLatestPose(sample)) {
            const std::uint64_t prior_recenter = g_pose_tracker.last_recenter_sequence();
            if (g_pose_tracker.Update(sample) && g_pose_tracker.CurrentPose(relative_pose)) {
                tracking_active = true;
                pose_sequence = sample.sequence;
                recentered = g_pose_tracker.last_recenter_sequence() != 0 &&
                    g_pose_tracker.last_recenter_sequence() != prior_recenter;
            }
        }
    } else {
        g_pose_tracker.SetEnabled(false);
    }

    const bool diagnostic_active = !snapshot.command.tracking_enabled && snapshot.command.enabled;
    const bool orientation_active = tracking_active || diagnostic_active;
    if (!orientation_active || !writable_basis) {
        original(camera);
        if (FirstForGeneration(g_passthrough_logged_generation, snapshot.generation)) {
            const char* result = "disabled";
            if (!writable_basis) result = "basis_unavailable";
            else if (snapshot.command.tracking_enabled) result = "tracking_unavailable";
            EmitEvent(
                "camera_probe_passthrough", result,
                "generation=" + std::to_string(snapshot.generation) +
                    ";camera=" + std::to_string(reinterpret_cast<std::uintptr_t>(camera)));
        }
        return;
    }

    const CameraProbeVector natural_left = *left;
    const CameraProbeVector natural_up = *up;
    const CameraProbeVector natural_forward = *forward;
    const CameraProbeVector natural_position = *position;
    const CameraProbeBasis applied = tracking_active
        ? ApplyCameraPoseOrientation(natural_forward, natural_up, relative_pose.orientation)
        : ApplyCameraProbeOrientation(
            natural_forward, natural_up,
            snapshot.command.yaw_degrees, snapshot.command.pitch_degrees);
    const CameraProbeVector applied_left = Normalize(
        Cross(applied.forward, applied.up), natural_left);

    g_render_update_override = {
        camera,
        applied_left,
        applied.up,
        applied.forward,
        true,
        false,
        tracking_active,
        pose_sequence,
    };
    original(camera);
    const bool orientation_applied = g_render_update_override.orientation_applied;
    g_render_update_override = {};
    *left = natural_left;
    *up = natural_up;
    *forward = natural_forward;

    if (!orientation_applied) return;

    if (tracking_active) {
        if (recentered) {
            EmitEvent(
                "camera_hmd_recentered", "ok",
                "generation=" + std::to_string(snapshot.generation) +
                    ";pose_sequence=" + std::to_string(pose_sequence));
        }
        if (ShouldLogHmdSequence(pose_sequence)) {
            const RelativeAngles angles = HeadAngles(relative_pose.orientation);
            try {
                std::ostringstream detail;
                detail << "generation=" << snapshot.generation
                       << ";source=hmd"
                       << ";pose_sequence=" << pose_sequence
                       << ";camera=0x" << std::hex << reinterpret_cast<std::uintptr_t>(camera)
                       << std::dec
                       << ";yaw_degrees=" << angles.yaw_degrees
                       << ";pitch_degrees=" << angles.pitch_degrees
                       << ";position=" << VectorText(natural_position)
                       << ";natural_forward=" << VectorText(natural_forward)
                       << ";applied_forward=" << VectorText(applied.forward)
                       << ";restored=true"
                       << ";renderer_camera_match="
                       << (RendererReferencesCamera(camera) ? "true" : "false");
                EmitEvent("camera_hmd_orientation_applied", "ok", detail.str());
            } catch (...) {
            }
        }
        return;
    }

    if (FirstForGeneration(g_orientation_logged_generation, snapshot.generation)) {
        try {
            std::ostringstream detail;
            detail << "generation=" << snapshot.generation
                   << ";source=diagnostic"
                   << ";camera=0x" << std::hex << reinterpret_cast<std::uintptr_t>(camera)
                   << std::dec
                   << ";yaw=" << snapshot.command.yaw_degrees
                   << ";pitch=" << snapshot.command.pitch_degrees
                   << ";position=" << VectorText(natural_position)
                   << ";natural_forward=" << VectorText(natural_forward)
                   << ";applied_forward=" << VectorText(applied.forward)
                   << ";natural_up=" << VectorText(natural_up)
                   << ";applied_up=" << VectorText(applied.up)
                   << ";restored=true"
                   << ";renderer_camera_match="
                   << (RendererReferencesCamera(camera) ? "true" : "false");
            EmitEvent("camera_probe_orientation_applied", "ok", detail.str());
        } catch (...) {
        }
    }
}

void __fastcall HookSetFov(
    void* camera,
    void*,
    const float fov_radians,
    const std::uint32_t projection_arg_1,
    const std::uint32_t projection_arg_2,
    const std::uint32_t projection_flags) {
    const auto original = g_original_set_fov;
    if (!original) return;
    const CommandSnapshot snapshot = CurrentCommand();
    const bool render_update = g_render_update_override.camera == camera;
    const bool diagnostic_fov = render_update && !snapshot.command.tracking_enabled &&
        snapshot.command.enabled && snapshot.command.override_fov;
    const float applied_radians = diagnostic_fov
        ? snapshot.command.fov_degrees * kDegreesToRadians
        : fov_radians;
    original(
        camera, applied_radians, projection_arg_1, projection_arg_2, projection_flags);

    if (render_update && g_render_update_override.orientation_active) {
        auto* bytes = static_cast<std::byte*>(camera);
        auto* left = reinterpret_cast<CameraProbeVector*>(bytes + kCameraLeftOffset);
        auto* up = reinterpret_cast<CameraProbeVector*>(bytes + kCameraUpOffset);
        auto* forward = reinterpret_cast<CameraProbeVector*>(bytes + kCameraForwardOffset);
        if (IsWritable(left, sizeof(CameraProbeVector)) &&
            IsWritable(up, sizeof(CameraProbeVector)) &&
            IsWritable(forward, sizeof(CameraProbeVector))) {
            *left = g_render_update_override.left;
            *up = g_render_update_override.up;
            *forward = g_render_update_override.forward;
            g_render_update_override.orientation_applied = true;
        }
    }

    if (diagnostic_fov &&
        FirstForGeneration(g_fov_logged_generation, snapshot.generation)) {
        std::ostringstream detail;
        detail << "generation=" << snapshot.generation
               << ";camera=0x" << std::hex << reinterpret_cast<std::uintptr_t>(camera)
               << std::dec
               << ";natural_degrees=" << fov_radians * kRadiansToDegrees
               << ";applied_degrees=" << snapshot.command.fov_degrees;
        EmitEvent("camera_probe_fov_applied", "ok", detail.str());
    }
}

} // namespace

bool ParseCameraProbeCommand(
    const std::string_view text,
    CameraProbeCommand& command,
    std::string* error) noexcept {
    CameraProbeCommand parsed{};
    bool enabled_present = false;
    if (!ReadBool(text, "enabled", parsed.enabled, enabled_present) || !enabled_present) {
        SetError(error, "enabled must be true or false");
        return false;
    }

    bool fov_present = false;
    if (!ReadFloat(text, "fovDegrees", parsed.fov_degrees, fov_present)) {
        SetError(error, "fovDegrees must be a finite number");
        return false;
    }
    parsed.override_fov = fov_present;

    bool yaw_present = false;
    if (!ReadFloat(text, "yawDegrees", parsed.yaw_degrees, yaw_present)) {
        SetError(error, "yawDegrees must be a finite number");
        return false;
    }
    bool pitch_present = false;
    if (!ReadFloat(text, "pitchDegrees", parsed.pitch_degrees, pitch_present)) {
        SetError(error, "pitchDegrees must be a finite number");
        return false;
    }
    bool tracking_present = false;
    if (!ReadBool(text, "trackingEnabled", parsed.tracking_enabled, tracking_present)) {
        SetError(error, "trackingEnabled must be true or false");
        return false;
    }
    bool recenter_present = false;
    if (!ReadBool(text, "recenter", parsed.recenter, recenter_present)) {
        SetError(error, "recenter must be true or false");
        return false;
    }
    (void)yaw_present;
    (void)pitch_present;
    (void)tracking_present;
    (void)recenter_present;

    if (parsed.override_fov &&
        (parsed.fov_degrees < 30.0F || parsed.fov_degrees > 140.0F)) {
        SetError(error, "fovDegrees must be between 30 and 140");
        return false;
    }
    if (parsed.yaw_degrees < -60.0F || parsed.yaw_degrees > 60.0F) {
        SetError(error, "yawDegrees must be between -60 and 60");
        return false;
    }
    if (parsed.pitch_degrees < -45.0F || parsed.pitch_degrees > 45.0F) {
        SetError(error, "pitchDegrees must be between -45 and 45");
        return false;
    }
    command = parsed;
    if (error) error->clear();
    return true;
}

CameraProbeBasis ApplyCameraProbeOrientation(
    CameraProbeVector forward,
    CameraProbeVector up,
    const float yaw_degrees,
    const float pitch_degrees) noexcept {
    forward = Normalize(forward, {0.0F, 0.0F, 1.0F});
    up = Normalize(up, {0.0F, 1.0F, 0.0F});

    const float yaw = yaw_degrees * kDegreesToRadians;
    forward = Normalize(RotateAroundAxis(forward, up, yaw), forward);
    CameraProbeVector right = Normalize(Cross(up, forward), {1.0F, 0.0F, 0.0F});

    const float pitch = pitch_degrees * kDegreesToRadians;
    forward = Normalize(RotateAroundAxis(forward, right, pitch), forward);
    up = Normalize(RotateAroundAxis(up, right, pitch), up);
    right = Normalize(Cross(up, forward), right);
    up = Normalize(Cross(forward, right), up);
    return {forward, up};
}

CameraProbeBasis ApplyCameraPoseOrientation(
    CameraProbeVector forward,
    CameraProbeVector up,
    const cojvr::runtime::Quaternion relative_orientation) noexcept {
    forward = Normalize(forward, {0.0F, 0.0F, 1.0F});
    up = Normalize(up, {0.0F, 1.0F, 0.0F});
    CameraProbeVector right = Normalize(Cross(up, forward), {1.0F, 0.0F, 0.0F});

    const auto tracked_forward = cojvr::runtime::RotateVector(
        relative_orientation, {0.0F, 0.0F, -1.0F});
    const auto tracked_up = cojvr::runtime::RotateVector(
        relative_orientation, {0.0F, 1.0F, 0.0F});

    // Neutral XR local coordinates use -Z forward while the inspected CoJ camera
    // basis uses +forward. Reflecting Z maps the relative head rotation into the
    // camera-local right/up/forward frame without per-axis sign patches.
    const CameraProbeVector local_forward{
        tracked_forward.x, tracked_forward.y, -tracked_forward.z};
    const CameraProbeVector local_up{tracked_up.x, tracked_up.y, -tracked_up.z};

    const auto compose = [&](const CameraProbeVector local) noexcept {
        return CameraProbeVector{
            right.x * local.x + up.x * local.y + forward.x * local.z,
            right.y * local.x + up.y * local.y + forward.y * local.z,
            right.z * local.x + up.z * local.y + forward.z * local.z,
        };
    };

    CameraProbeVector applied_forward = Normalize(compose(local_forward), forward);
    CameraProbeVector applied_up = Normalize(compose(local_up), up);
    right = Normalize(Cross(applied_up, applied_forward), right);
    applied_up = Normalize(Cross(applied_forward, right), applied_up);
    return {applied_forward, applied_up};
}

bool IsSupportedChromeEngineHash(const std::string_view sha256) noexcept {
    if (sha256.size() != kInspectedChromeEngine3Sha256.size()) return false;
    for (std::size_t index = 0; index < sha256.size(); ++index) {
        char lhs = sha256[index];
        char rhs = kInspectedChromeEngine3Sha256[index];
        if (lhs >= 'a' && lhs <= 'f') lhs = static_cast<char>(lhs - 'a' + 'A');
        if (rhs >= 'a' && rhs <= 'f') rhs = static_cast<char>(rhs - 'a' + 'A');
        if (lhs != rhs) return false;
    }
    return true;
}

CameraProbeInstallStatus InitializeCameraProbe(
    const std::filesystem::path& control_path,
    const CameraProbeEventCallback callback,
    cojvr::runtime::PoseSource* pose_source) noexcept {
    g_event_callback = callback;
    g_pose_source = pose_source;
    if (g_initialized.exchange(true, std::memory_order_acq_rel)) {
        return g_installed.load(std::memory_order_acquire)
            ? CameraProbeInstallStatus::already_installed
            : CameraProbeInstallStatus::hook_failed;
    }

    try {
        const auto host = cojvr::runtime::InspectCurrentHost();
        if (!host || !host->IsKnownExactBuild() || !host->known_build ||
            host->known_build->game != cojvr::runtime::GameId::call_of_juarez_dx9) {
            EmitEvent("camera_probe_install", "host_rejected", "exact CoJ DX9 build required");
            return CameraProbeInstallStatus::host_rejected;
        }

        HMODULE engine = GetModuleHandleW(L"ChromeEngine3.dll");
        if (!engine) {
            EmitEvent("camera_probe_install", "engine_missing", "ChromeEngine3.dll is not loaded");
            return CameraProbeInstallStatus::engine_missing;
        }
        std::array<wchar_t, 32768> engine_path_buffer{};
        const DWORD engine_path_length = GetModuleFileNameW(
            engine, engine_path_buffer.data(), static_cast<DWORD>(engine_path_buffer.size()));
        if (engine_path_length == 0 || engine_path_length >= engine_path_buffer.size()) {
            EmitEvent("camera_probe_install", "engine_rejected", "engine path unavailable");
            return CameraProbeInstallStatus::engine_rejected;
        }
        const std::filesystem::path engine_path(
            std::wstring_view(engine_path_buffer.data(), engine_path_length));
        const auto engine_hash = cojvr::runtime::Sha256File(engine_path);
        if (!engine_hash || !IsSupportedChromeEngineHash(*engine_hash)) {
            EmitEvent(
                "camera_probe_install", "engine_rejected",
                "sha256=" + (engine_hash ? *engine_hash : std::string("unavailable")));
            return CameraProbeInstallStatus::engine_rejected;
        }

        g_engine_base = reinterpret_cast<std::byte*>(engine);
        g_camera_vtable = reinterpret_cast<void**>(g_engine_base + kBaseCameraVtableRva);
        const auto expected_render_update = reinterpret_cast<void*>(g_engine_base + kRenderCameraUpdateRva);
        const auto expected_set_fov = reinterpret_cast<void*>(g_engine_base + kSetFovRva);
        if (!IsReadable(g_camera_vtable, sizeof(void*) * (kSetFovSlot + 1)) ||
            g_camera_vtable[kRenderCameraUpdateSlot] != expected_render_update ||
            g_camera_vtable[kSetFovSlot] != expected_set_fov) {
            EmitEvent(
                "camera_probe_install", "profile_mismatch",
                "CBaseCamera vtable does not match inspected RVAs");
            g_camera_vtable = nullptr;
            g_engine_base = nullptr;
            return CameraProbeInstallStatus::profile_mismatch;
        }

        g_control_path = control_path;
        g_original_render_camera_update = reinterpret_cast<RenderCameraUpdateFn>(
            g_camera_vtable[kRenderCameraUpdateSlot]);
        g_original_set_fov = reinterpret_cast<SetFovFn>(g_camera_vtable[kSetFovSlot]);
        const std::array requests{
            cojvr::backends::d3d9::HookSlotRequest{
                kRenderCameraUpdateSlot, reinterpret_cast<void*>(&HookRenderCameraUpdate)},
            cojvr::backends::d3d9::HookSlotRequest{
                kSetFovSlot, reinterpret_cast<void*>(&HookSetFov)},
        };
        const auto outcome = g_camera_hooks.Install(g_camera_vtable, requests);
        if (outcome.result != cojvr::backends::d3d9::HookRegistryResult::Installed &&
            outcome.result != cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled) {
            EmitEvent(
                "camera_probe_install", "hook_failed",
                "modified_slots=" + std::to_string(outcome.modified_slots));
            return CameraProbeInstallStatus::hook_failed;
        }
        g_installed.store(true, std::memory_order_release);
        std::ostringstream detail;
        detail << "engine_sha256=" << *engine_hash
               << ";vtable_rva=0x" << std::hex << kBaseCameraVtableRva
               << ";render_update_rva=0x" << kRenderCameraUpdateRva
               << ";set_fov_rva=0x" << kSetFovRva
               << ";control_path=" << control_path.string()
               << ";pose_source=" << (g_pose_source ? "available" : "none");
        EmitEvent("camera_probe_install", "installed", detail.str());
        return outcome.result == cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled
            ? CameraProbeInstallStatus::already_installed
            : CameraProbeInstallStatus::installed;
    } catch (...) {
        EmitEvent("camera_probe_install", "hook_failed", "unexpected exception");
        return CameraProbeInstallStatus::hook_failed;
    }
}

CameraProbeHookState InspectCameraProbe() noexcept {
    CameraProbeHookState state{};
    state.initialized = g_initialized.load(std::memory_order_acquire);
    state.installed = g_installed.load(std::memory_order_acquire);
    if (!g_camera_vtable) return state;
    const auto slots = g_camera_hooks.Inspect(g_camera_vtable);
    for (const auto& slot : slots) {
        if (slot.index == kRenderCameraUpdateSlot) state.render_update_owned = slot.owned;
        if (slot.index == kSetFovSlot) state.set_fov_owned = slot.owned;
    }
    return state;
}

void ShutdownCameraProbe() noexcept {
    try {
        if (g_camera_vtable) {
            const auto outcome = g_camera_hooks.Restore(g_camera_vtable);
            const bool restored =
                outcome.result == cojvr::backends::d3d9::HookRegistryResult::Installed ||
                outcome.result == cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled;
            EmitEvent(
                "camera_probe_restore", restored ? "restored" : "incomplete",
                "restored_slots=" + std::to_string(outcome.modified_slots));
        }
    } catch (...) {
        EmitEvent("camera_probe_restore", "exception", "restore_exception");
    }
    g_installed.store(false, std::memory_order_release);
    g_initialized.store(false, std::memory_order_release);
    g_camera_vtable = nullptr;
    g_engine_base = nullptr;
    g_original_render_camera_update = nullptr;
    g_original_set_fov = nullptr;
    g_render_update_override = {};
    g_pose_tracker.SetEnabled(false);
    g_pose_source = nullptr;
    g_recenter_command_generation.store(0, std::memory_order_release);
    g_last_hmd_logged_sequence.store(0, std::memory_order_release);
    g_hmd_logged_count.store(0, std::memory_order_release);
}

const char* CameraProbeInstallStatusName(const CameraProbeInstallStatus status) noexcept {
    switch (status) {
    case CameraProbeInstallStatus::installed: return "installed";
    case CameraProbeInstallStatus::already_installed: return "already_installed";
    case CameraProbeInstallStatus::host_rejected: return "host_rejected";
    case CameraProbeInstallStatus::engine_missing: return "engine_missing";
    case CameraProbeInstallStatus::engine_rejected: return "engine_rejected";
    case CameraProbeInstallStatus::profile_mismatch: return "profile_mismatch";
    case CameraProbeInstallStatus::hook_failed: return "hook_failed";
    }
    return "unknown";
}

} // namespace cojvr::games::call_of_juarez
