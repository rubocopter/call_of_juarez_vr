#include "games/call_of_juarez/camera_probe.hpp"
#include "games/call_of_juarez/body_adapter.hpp"
#include "games/call_of_juarez/java_player_bridge.hpp"
#include "runtime/body_tracking.hpp"

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
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace cojvr::games::call_of_juarez {
namespace {

constexpr std::uintptr_t kBaseCameraVtableRva = 0x0030AE1C;
constexpr std::uintptr_t kRenderCameraUpdateRva = 0x001C5BA0;
constexpr std::uintptr_t kSetFovRva = 0x001C58E0;
constexpr std::uintptr_t kInvertMatrixRva = 0x001F3F80;
constexpr std::uintptr_t kProjectionBuilderRva = 0x0022BC50;
constexpr std::uintptr_t kRenderViewCoreRva = 0x00030E00;
constexpr std::uintptr_t kRenderViewEntryRva = 0x002E1C88;
constexpr std::uintptr_t kRenderViewRva = 0x00030FB0;
constexpr std::uintptr_t kRendererGlobalRva = 0x00590074;
constexpr std::ptrdiff_t kRendererCameraPrimaryOffset = 0x1AC;
constexpr std::ptrdiff_t kRendererCameraSecondaryOffset = 0x1B0;
constexpr std::ptrdiff_t kRenderOwnerActiveViewOffset = 0x3B8;
constexpr std::ptrdiff_t kViewCameraOffset = 0x38;
constexpr std::ptrdiff_t kViewRenderedThisFrameOffset = 0xD7;
constexpr std::ptrdiff_t kCameraViewOwnerOffset = 0x4A0;
constexpr std::ptrdiff_t kCameraSourceViewMatrixOffset = 0x04;
constexpr std::ptrdiff_t kCameraSourceRightOffset = 0x44;
constexpr std::ptrdiff_t kCameraSourceUpOffset = 0x54;
constexpr std::ptrdiff_t kCameraSourceForwardOffset = 0x64;
constexpr std::ptrdiff_t kCameraSourcePositionOffset = 0x74;
constexpr std::ptrdiff_t kCameraDerivedViewSourceOffset = 0x84;
constexpr std::ptrdiff_t kCameraDerivedRightOffset = 0xC4;
constexpr std::ptrdiff_t kCameraDerivedUpOffset = 0xD4;
constexpr std::ptrdiff_t kCameraDerivedForwardOffset = 0xE4;
constexpr std::ptrdiff_t kCameraViewMatrixOffset = 0x104;
constexpr std::ptrdiff_t kCameraWorldCullingMatrixOffset = 0x144;
constexpr std::ptrdiff_t kCameraProjectionMatrixOffset = 0x184;
constexpr std::ptrdiff_t kCameraViewProjectionMatrixOffset = 0x204;
constexpr std::ptrdiff_t kCameraFrustumLeftOffset = 0x244;
constexpr std::ptrdiff_t kCameraFrustumRightOffset = 0x248;
constexpr std::ptrdiff_t kCameraFrustumBottomOffset = 0x24C;
constexpr std::ptrdiff_t kCameraFrustumTopOffset = 0x250;
constexpr std::ptrdiff_t kCameraNearOffset = 0x254;
constexpr std::ptrdiff_t kCameraFarOffset = 0x258;
constexpr std::size_t kCameraMatrixBytes = sizeof(float) * 16;
constexpr std::size_t kRenderCameraUpdateSlot = 9;
constexpr std::size_t kSetFovSlot = 10;
constexpr float kDegreesToRadians = 0.01745329251994329577F;
constexpr float kRadiansToDegrees = 57.295779513082320876F;
constexpr ULONGLONG kControlPollMilliseconds = 250;

using RenderCameraUpdateFn = void (__thiscall *)(void* camera);
using InvertMatrixFn = void (__thiscall *)(void* destination, const void* source);
using ProjectionBuilderFn = void (__thiscall *)(void* camera);
using RenderViewFn = void (__thiscall *)(void* owner, void* view);
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
cojvr::backends::d3d9::HookRegistry g_render_view_hooks;
std::byte* g_engine_base = nullptr;
void** g_camera_vtable = nullptr;
void** g_render_view_entry = nullptr;
RenderCameraUpdateFn g_original_render_camera_update = nullptr;
InvertMatrixFn g_invert_matrix = nullptr;
ProjectionBuilderFn g_projection_builder = nullptr;
RenderViewFn g_original_render_view = nullptr;
SetFovFn g_original_set_fov = nullptr;
CameraProbeEventCallback g_event_callback = nullptr;
cojvr::runtime::PoseSource* g_pose_source = nullptr;
cojvr::runtime::BodyTracker g_body_tracker;
JavaPlayerBridge g_java_player_bridge;
cojvr::runtime::Vec3 g_body_applied_world_offset{};
cojvr::runtime::Vec3 g_body_applied_tracking_offset{};
std::uint64_t g_body_player_generation = 0;
bool g_body_player_space_applied = false;
struct ArmElementCache {
    std::uint64_t being_generation = 0;
    int left_upper_arm = -1;
    int left_forearm = -1;
    int left_foretwist = -1;
    int left_hand = -1;
    int right_upper_arm = -1;
    int right_forearm = -1;
    int right_foretwist = -1;
    int right_hand = -1;
    bool valid = false;
};
ArmElementCache g_arm_element_cache{};
struct HandOrientationCalibrationState {
    std::uint64_t being_generation = 0;
    std::uint64_t recenter_sequence = 0;
    HandOrientationReference reference{};
};
HandOrientationCalibrationState g_left_hand_orientation{};
HandOrientationCalibrationState g_right_hand_orientation{};
struct AppliedArmElementRotation {
    ArmBoneRotationPlan rotations{};
    BoneRotationDelta forearm_twist{};
    BoneRotationDelta hand_rotation{};
    HandOrientationTarget hand_target{};
    ArmGeometrySample natural_geometry{};
    ArmGeometrySample post_write_geometry{};
    bool post_write_geometry_valid = false;
    bool active = false;
};
AppliedArmElementRotation g_left_arm_rotation{};
AppliedArmElementRotation g_right_arm_rotation{};
bool g_arm_rotation_faulted = false;
cojvr::runtime::GameplayInputState g_last_gameplay_telemetry{};
bool g_gameplay_telemetry_initialized = false;
CameraStereoRuntimeCallbacks g_stereo_callbacks{};
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
std::atomic_bool g_meaningful_roll_logged{false};
std::atomic_uint64_t g_stereo_frame_sequence{0};
std::atomic_uint64_t g_stereo_logged_count{0};
std::atomic_bool g_initialized{false};
std::atomic_bool g_installed{false};

struct RenderUpdateOverride {
    void* camera = nullptr;
    bool orientation_applied = false;
};

thread_local RenderUpdateOverride g_render_update_override{};

bool GameplayInputChanged(
    const cojvr::runtime::GameplayInputState& left,
    const cojvr::runtime::GameplayInputState& right) noexcept {
    constexpr float kAxisChange = 0.005F;
    return left.active != right.active ||
        std::fabs(left.move.x - right.move.x) > kAxisChange ||
        std::fabs(left.move.y - right.move.y) > kAxisChange ||
        std::fabs(left.turn.x - right.turn.x) > kAxisChange ||
        std::fabs(left.turn.y - right.turn.y) > kAxisChange ||
        left.fire_left != right.fire_left ||
        left.fire_right != right.fire_right ||
        left.jump != right.jump ||
        left.reload != right.reload ||
        left.run != right.run ||
        left.crouch != right.crouch ||
        left.interact != right.interact ||
        left.weapon_next != right.weapon_next ||
        left.weapon_previous != right.weapon_previous ||
        left.kick != right.kick;
}

struct StereoEyeOverride {
    bool active = false;
    bool view_hook_active = false;
    void* camera = nullptr;
    cojvr::runtime::Pose relative_head_pose{};
    const cojvr::runtime::EyeView* eye = nullptr;
    std::uint64_t frame_sequence = 0;
    std::uint64_t pose_sequence = 0;
    bool camera_applied = false;
    bool projection_applied = false;
    bool renderer_camera_match = false;
    CameraProbeVector applied_position{};
    CameraProbeFrustum applied_frustum{};
};

thread_local StereoEyeOverride g_stereo_eye_override{};

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

struct CameraRenderStateSnapshot {
    std::array<std::byte, kCameraMatrixBytes> source_view{};
    std::array<std::byte, kCameraMatrixBytes> source_world{};
    std::array<std::array<std::byte, kCameraMatrixBytes>, 6> matrices{};
    std::array<float, 6> frustum{};
};

constexpr std::array<std::ptrdiff_t, 6> kCameraDerivedMatrixOffsets{
    kCameraDerivedViewSourceOffset,
    kCameraDerivedRightOffset,
    kCameraViewMatrixOffset,
    kCameraWorldCullingMatrixOffset,
    kCameraProjectionMatrixOffset,
    kCameraViewProjectionMatrixOffset,
};

bool CaptureCameraRenderState(
    void* camera,
    CameraRenderStateSnapshot& snapshot) noexcept {
    if (!camera) return false;
    auto* bytes = static_cast<std::byte*>(camera);
    auto* source_view = bytes + kCameraSourceViewMatrixOffset;
    auto* source_world = bytes + kCameraSourceRightOffset;
    if (!IsReadable(source_view, kCameraMatrixBytes) ||
        !IsWritable(source_view, kCameraMatrixBytes) ||
        !IsReadable(source_world, kCameraMatrixBytes) ||
        !IsWritable(source_world, kCameraMatrixBytes)) {
        return false;
    }
    std::memcpy(snapshot.source_view.data(), source_view, kCameraMatrixBytes);
    std::memcpy(snapshot.source_world.data(), source_world, kCameraMatrixBytes);
    for (std::size_t index = 0; index < kCameraDerivedMatrixOffsets.size(); ++index) {
        auto* matrix = bytes + kCameraDerivedMatrixOffsets[index];
        if (!IsReadable(matrix, kCameraMatrixBytes) ||
            !IsWritable(matrix, kCameraMatrixBytes)) {
            return false;
        }
        std::memcpy(snapshot.matrices[index].data(), matrix, kCameraMatrixBytes);
    }
    auto* frustum = reinterpret_cast<float*>(bytes + kCameraFrustumLeftOffset);
    if (!IsReadable(frustum, sizeof(float) * snapshot.frustum.size()) ||
        !IsWritable(frustum, sizeof(float) * snapshot.frustum.size())) {
        return false;
    }
    std::copy_n(frustum, snapshot.frustum.size(), snapshot.frustum.begin());
    return true;
}

bool RestoreCameraRenderState(
    void* camera,
    const CameraRenderStateSnapshot& snapshot) noexcept {
    if (!camera) return false;
    auto* bytes = static_cast<std::byte*>(camera);
    auto* source_view = bytes + kCameraSourceViewMatrixOffset;
    auto* source_world = bytes + kCameraSourceRightOffset;
    auto* frustum = reinterpret_cast<float*>(bytes + kCameraFrustumLeftOffset);
    if (!IsWritable(source_view, kCameraMatrixBytes) ||
        !IsWritable(source_world, kCameraMatrixBytes) ||
        !IsWritable(frustum, sizeof(float) * snapshot.frustum.size())) {
        return false;
    }
    for (const auto offset : kCameraDerivedMatrixOffsets) {
        if (!IsWritable(bytes + offset, kCameraMatrixBytes)) return false;
    }
    std::memcpy(source_view, snapshot.source_view.data(), kCameraMatrixBytes);
    std::memcpy(source_world, snapshot.source_world.data(), kCameraMatrixBytes);
    for (std::size_t index = 0; index < kCameraDerivedMatrixOffsets.size(); ++index) {
        std::memcpy(
            bytes + kCameraDerivedMatrixOffsets[index],
            snapshot.matrices[index].data(),
            kCameraMatrixBytes);
    }
    std::copy(snapshot.frustum.begin(), snapshot.frustum.end(), frustum);
    return true;
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
               << ";body_ik_enabled=" << (parsed.body_ik_enabled ? "true" : "false")
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

std::string RuntimeVectorText(const cojvr::runtime::Vec3 value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6)
        << '(' << value.x << ',' << value.y << ',' << value.z << ')';
    return out.str();
}

float VectorDistanceSquared(
    const CameraProbeVector lhs,
    const CameraProbeVector rhs) noexcept {
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    const float dz = lhs.z - rhs.z;
    return dx * dx + dy * dy + dz * dz;
}

float RuntimeVectorDistanceSquared(
    const cojvr::runtime::Vec3 lhs,
    const cojvr::runtime::Vec3 rhs) noexcept {
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    const float dz = lhs.z - rhs.z;
    return dx * dx + dy * dy + dz * dz;
}

bool SourceMatrixHasNativeHomogeneousLayout(const void* source_world_matrix) noexcept {
    if (!source_world_matrix) return false;
    const auto* matrix = static_cast<const float*>(source_world_matrix);
    constexpr float kTolerance = 0.0001F;
    for (std::size_t index = 0; index < 16; ++index) {
        if (!std::isfinite(matrix[index])) return false;
    }
    return std::fabs(matrix[3]) <= kTolerance &&
        std::fabs(matrix[7]) <= kTolerance &&
        std::fabs(matrix[11]) <= kTolerance &&
        std::fabs(matrix[15] - 1.0F) <= kTolerance;
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
    float roll_degrees = 0.0F;
};

RelativeAngles HeadAngles(const cojvr::runtime::Quaternion orientation) noexcept {
    const auto tracked_forward = cojvr::runtime::RotateVector(
        orientation, {0.0F, 0.0F, -1.0F});
    const auto tracked_up = cojvr::runtime::RotateVector(
        orientation, {0.0F, 1.0F, 0.0F});
    const float local_x = tracked_forward.x;
    const float local_y = tracked_forward.y;
    const float local_z = -tracked_forward.z;
    const auto cross = [](const cojvr::runtime::Vec3 left,
                          const cojvr::runtime::Vec3 right) noexcept {
        return cojvr::runtime::Vec3{
            left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x,
        };
    };
    const auto dot = [](const cojvr::runtime::Vec3 left,
                        const cojvr::runtime::Vec3 right) noexcept {
        return left.x * right.x + left.y * right.y + left.z * right.z;
    };
    const auto normalize = [&](const cojvr::runtime::Vec3 value,
                               const cojvr::runtime::Vec3 fallback) noexcept {
        const float length = std::sqrt(dot(value, value));
        if (!std::isfinite(length) || length <= 1.0e-5F) return fallback;
        return cojvr::runtime::Vec3{
            value.x / length, value.y / length, value.z / length};
    };
    const cojvr::runtime::Vec3 world_up{0.0F, 1.0F, 0.0F};
    const cojvr::runtime::Vec3 tracked_right = cojvr::runtime::RotateVector(
        orientation, {1.0F, 0.0F, 0.0F});
    const cojvr::runtime::Vec3 no_roll_right = normalize(
        cross(tracked_forward, world_up), tracked_right);
    const cojvr::runtime::Vec3 no_roll_up = normalize(
        cross(no_roll_right, tracked_forward), tracked_up);
    return {
        std::atan2(local_x, local_z) * kRadiansToDegrees,
        std::atan2(local_y, std::sqrt(local_x * local_x + local_z * local_z)) *
            kRadiansToDegrees,
        std::atan2(
            dot(cross(no_roll_up, tracked_up), tracked_forward),
            dot(no_roll_up, tracked_up)) * kRadiansToDegrees,
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
    auto* source_right = reinterpret_cast<CameraProbeVector*>(bytes + kCameraSourceRightOffset);
    auto* source_up = reinterpret_cast<CameraProbeVector*>(bytes + kCameraSourceUpOffset);
    auto* source_forward = reinterpret_cast<CameraProbeVector*>(bytes + kCameraSourceForwardOffset);
    auto* source_position = reinterpret_cast<CameraProbeVector*>(bytes + kCameraSourcePositionOffset);
    auto* source_view_matrix = bytes + kCameraSourceViewMatrixOffset;
    auto* source_world_matrix = bytes + kCameraSourceRightOffset;
    auto* derived_right = reinterpret_cast<CameraProbeVector*>(bytes + kCameraDerivedRightOffset);
    auto* derived_up = reinterpret_cast<CameraProbeVector*>(bytes + kCameraDerivedUpOffset);
    auto* derived_forward = reinterpret_cast<CameraProbeVector*>(bytes + kCameraDerivedForwardOffset);
    auto* view_matrix = bytes + kCameraViewMatrixOffset;
    auto* projection_matrix = bytes + kCameraProjectionMatrixOffset;
    auto* view_projection_matrix = bytes + kCameraViewProjectionMatrixOffset;
    auto* frustum_left = reinterpret_cast<float*>(bytes + kCameraFrustumLeftOffset);
    auto* frustum_right = reinterpret_cast<float*>(bytes + kCameraFrustumRightOffset);
    auto* frustum_bottom = reinterpret_cast<float*>(bytes + kCameraFrustumBottomOffset);
    auto* frustum_top = reinterpret_cast<float*>(bytes + kCameraFrustumTopOffset);
    auto* near_plane = reinterpret_cast<float*>(bytes + kCameraNearOffset);
    auto* far_plane = reinterpret_cast<float*>(bytes + kCameraFarOffset);
    const bool stereo_active = g_stereo_eye_override.active &&
        g_stereo_eye_override.camera == camera && g_stereo_eye_override.eye != nullptr;
    const bool writable_basis = IsWritable(source_right, sizeof(CameraProbeVector)) &&
        IsWritable(source_up, sizeof(CameraProbeVector)) &&
        IsWritable(source_forward, sizeof(CameraProbeVector)) &&
        (stereo_active
            ? IsWritable(source_position, sizeof(CameraProbeVector))
            : IsReadable(source_position, sizeof(CameraProbeVector))) &&
        IsReadable(source_world_matrix, kCameraMatrixBytes) &&
        IsWritable(source_view_matrix, kCameraMatrixBytes) &&
        g_invert_matrix != nullptr;
    const bool readable_render_basis = IsReadable(derived_right, sizeof(CameraProbeVector)) &&
        IsReadable(derived_up, sizeof(CameraProbeVector)) &&
        IsReadable(derived_forward, sizeof(CameraProbeVector));
    const bool readable_view_matrices = IsReadable(view_matrix, kCameraMatrixBytes) &&
        IsReadable(projection_matrix, kCameraMatrixBytes) &&
        IsReadable(view_projection_matrix, kCameraMatrixBytes);
    const bool readable_frustum = IsReadable(frustum_left, sizeof(float)) &&
        IsReadable(frustum_right, sizeof(float)) &&
        IsReadable(frustum_bottom, sizeof(float)) &&
        IsReadable(frustum_top, sizeof(float)) &&
        IsReadable(near_plane, sizeof(float)) && IsReadable(far_plane, sizeof(float));
    const bool writable_frustum = readable_frustum && IsWritable(frustum_left, sizeof(float)) &&
        IsWritable(frustum_right, sizeof(float)) &&
        IsWritable(frustum_bottom, sizeof(float)) &&
        IsWritable(frustum_top, sizeof(float)) &&
        IsWritable(near_plane, sizeof(float)) && IsWritable(far_plane, sizeof(float));

    cojvr::runtime::Pose relative_pose{};
    std::uint64_t pose_sequence = 0;
    bool tracking_active = false;
    bool recentered = false;
    if (stereo_active) {
        relative_pose = g_stereo_eye_override.relative_head_pose;
        pose_sequence = g_stereo_eye_override.pose_sequence;
        tracking_active = relative_pose.orientation_valid;
    } else if (snapshot.command.tracking_enabled && g_pose_source) {
        g_pose_tracker.SetEnabled(true);
        if (snapshot.command.recenter &&
            FirstForGeneration(g_recenter_command_generation, snapshot.generation)) {
            g_pose_tracker.RequestRecenter();
        }
        cojvr::runtime::PoseSample sample{};
        if (g_pose_source->TryGetLatestPose(sample)) {
            const std::uint64_t prior_recenter = g_pose_tracker.last_recenter_sequence();
            if (g_pose_tracker.Update(sample) && g_pose_tracker.CurrentPose(relative_pose)) {
                g_body_tracker.SetHeadPose(relative_pose);
                g_body_tracker.UpdatePlayerSpace(relative_pose);
                tracking_active = true;
                pose_sequence = sample.sequence;
                recentered = g_pose_tracker.last_recenter_sequence() != 0 &&
                    g_pose_tracker.last_recenter_sequence() != prior_recenter;
            }
        }
    } else {
        g_pose_tracker.SetEnabled(false);
    }

    const bool diagnostic_active = !stereo_active &&
        !snapshot.command.tracking_enabled && snapshot.command.enabled;
    const bool orientation_active = tracking_active || diagnostic_active;
    if (!orientation_active || !writable_basis || !readable_frustum ||
        (stereo_active && !writable_frustum)) {
        original(camera);
        if (FirstForGeneration(g_passthrough_logged_generation, snapshot.generation)) {
            const char* result = "disabled";
            if (!writable_basis) result = "basis_unavailable";
            else if (!readable_frustum) result = "frustum_unreadable";
            else if (stereo_active && !writable_frustum) result = "frustum_unavailable";
            else if (snapshot.command.tracking_enabled) result = "tracking_unavailable";
            EmitEvent(
                "camera_probe_passthrough", result,
                "generation=" + std::to_string(snapshot.generation) +
                    ";camera=" + std::to_string(reinterpret_cast<std::uintptr_t>(camera)));
        }
        return;
    }

    const CameraProbeVector natural_right = *source_right;
    const CameraProbeVector natural_up = *source_up;
    const CameraProbeVector natural_forward = *source_forward;
    const CameraProbeVector natural_position = *source_position;
    const CameraProbeBasis natural_basis{natural_forward, natural_up, natural_right};
    const bool natural_basis_right_handed =
        std::isfinite(CameraProbeBasisDeterminant(natural_basis)) &&
        CameraProbeBasisDeterminant(natural_basis) > 0.0F;
    const bool source_world_homogeneous_layout =
        SourceMatrixHasNativeHomogeneousLayout(source_world_matrix);
    const bool source_view_homogeneous_layout =
        SourceMatrixHasNativeHomogeneousLayout(source_view_matrix);
    if (!natural_basis_right_handed || !source_world_homogeneous_layout ||
        !source_view_homogeneous_layout) {
        original(camera);
        if (FirstForGeneration(g_passthrough_logged_generation, snapshot.generation)) {
            std::ostringstream detail;
            detail << "generation=" << snapshot.generation
                   << ";camera=" << reinterpret_cast<std::uintptr_t>(camera)
                   << ";determinant=" << CameraProbeBasisDeterminant(natural_basis)
                   << ";source_world_homogeneous_layout="
                   << (source_world_homogeneous_layout ? "true" : "false")
                   << ";source_view_homogeneous_layout="
                   << (source_view_homogeneous_layout ? "true" : "false");
            EmitEvent("camera_probe_passthrough", "source_matrix_invalid", detail.str());
        }
        return;
    }
    const CameraProbeVector natural_render_right =
        readable_render_basis ? *derived_right : CameraProbeVector{};
    const CameraProbeVector natural_render_up =
        readable_render_basis ? *derived_up : CameraProbeVector{};
    const CameraProbeVector natural_render_forward =
        readable_render_basis ? *derived_forward : CameraProbeVector{};
    std::array<std::byte, kCameraMatrixBytes> natural_source_view_matrix{};
    std::array<std::byte, kCameraMatrixBytes> natural_view_matrix{};
    std::array<std::byte, kCameraMatrixBytes> natural_projection_matrix{};
    std::array<std::byte, kCameraMatrixBytes> natural_view_projection_matrix{};
    std::memcpy(
        natural_source_view_matrix.data(), source_view_matrix, natural_source_view_matrix.size());
    if (readable_view_matrices) {
        std::memcpy(natural_view_matrix.data(), view_matrix, natural_view_matrix.size());
        std::memcpy(
            natural_projection_matrix.data(), projection_matrix, natural_projection_matrix.size());
        std::memcpy(
            natural_view_projection_matrix.data(), view_projection_matrix,
            natural_view_projection_matrix.size());
    }
    const CameraProbeBasis applied = tracking_active
        ? ApplyCameraPoseOrientation(natural_forward, natural_up, relative_pose.orientation)
        : ApplyCameraProbeOrientation(
            natural_forward, natural_up,
            snapshot.command.yaw_degrees, snapshot.command.pitch_degrees);
    if (!IsCameraProbeBasisRigidRightHanded(applied)) {
        original(camera);
        if (FirstForGeneration(g_passthrough_logged_generation, snapshot.generation)) {
            std::ostringstream detail;
            detail << "generation=" << snapshot.generation
                   << ";camera=" << reinterpret_cast<std::uintptr_t>(camera)
                   << ";determinant=" << CameraProbeBasisDeterminant(applied);
            EmitEvent("camera_probe_passthrough", "applied_basis_invalid", detail.str());
        }
        return;
    }
    const CameraProbeVector applied_right = Normalize(applied.right, natural_right);
    const CameraProbeVector tracked_head_position = stereo_active
        ? ApplyCameraTrackingOffset(natural_position, natural_basis, relative_pose.position)
        : natural_position;
    const CameraProbeVector applied_position = stereo_active
        ? ApplyCameraEyeOffset(
            tracked_head_position, applied,
            g_stereo_eye_override.eye->eye_to_head.position)
        : natural_position;
    if (stereo_active &&
        (!relative_pose.position_valid ||
         !g_stereo_eye_override.eye->eye_to_head.position_valid ||
         !std::isfinite(tracked_head_position.x) ||
         !std::isfinite(tracked_head_position.y) ||
         !std::isfinite(tracked_head_position.z) ||
         !std::isfinite(applied_position.x) || !std::isfinite(applied_position.y) ||
         !std::isfinite(applied_position.z))) {
        original(camera);
        return;
    }

    // Native camera setters write the world/camera transform at +0x44 and then
    // call 0x001F3F80 to maintain its inverse at +0x04. 0x0022BB10 copies the
    // two synchronized transforms into the culling/world path (+0xC4/+0x144)
    // and the actual view path (+0x84/+0x104) before building view-projection
    // at +0x204. Keep both source matrices synchronized for the transient HMD
    // offset, then restore both after the render update returns.
    *source_right = applied_right;
    *source_up = applied.up;
    *source_forward = applied.forward;
    if (stereo_active) *source_position = applied_position;
    g_invert_matrix(source_view_matrix, source_world_matrix);
    const bool injected_view_homogeneous_layout =
        SourceMatrixHasNativeHomogeneousLayout(source_view_matrix);
    if (!injected_view_homogeneous_layout) {
        *source_right = natural_right;
        *source_up = natural_up;
        *source_forward = natural_forward;
        if (stereo_active) *source_position = natural_position;
        std::memcpy(
            source_view_matrix, natural_source_view_matrix.data(), natural_source_view_matrix.size());
        original(camera);
        if (FirstForGeneration(g_passthrough_logged_generation, snapshot.generation)) {
            std::ostringstream detail;
            detail << "generation=" << snapshot.generation
                   << ";camera=" << reinterpret_cast<std::uintptr_t>(camera)
                   << ";applied_determinant=" << CameraProbeBasisDeterminant(applied);
            EmitEvent("camera_probe_passthrough", "inverse_matrix_invalid", detail.str());
        }
        return;
    }

    g_render_update_override = {camera, false};
    g_render_update_override.orientation_applied = true;
    original(camera);
    const bool orientation_applied = g_render_update_override.orientation_applied;
    const CameraProbeVector rendered_right =
        readable_render_basis ? *derived_right : CameraProbeVector{};
    const CameraProbeVector rendered_up =
        readable_render_basis ? *derived_up : CameraProbeVector{};
    const CameraProbeVector rendered_forward =
        readable_render_basis ? *derived_forward : CameraProbeVector{};
    const bool render_basis_changed = readable_render_basis &&
        (VectorDistanceSquared(natural_render_right, rendered_right) > 0.000001F ||
         VectorDistanceSquared(natural_render_up, rendered_up) > 0.000001F ||
         VectorDistanceSquared(natural_render_forward, rendered_forward) > 0.000001F);
    const bool view_matrix_changed = readable_view_matrices &&
        std::memcmp(natural_view_matrix.data(), view_matrix, natural_view_matrix.size()) != 0;
    const bool projection_matrix_changed = readable_view_matrices &&
        std::memcmp(
            natural_projection_matrix.data(), projection_matrix,
            natural_projection_matrix.size()) != 0;
    const bool view_projection_changed = readable_view_matrices &&
        std::memcmp(
            natural_view_projection_matrix.data(), view_projection_matrix,
            natural_view_projection_matrix.size()) != 0;
    const bool renderer_camera_match = RendererReferencesCamera(camera);
    g_render_update_override = {};
    if (!stereo_active) {
        *source_right = natural_right;
        *source_up = natural_up;
        *source_forward = natural_forward;
        std::memcpy(
            source_view_matrix, natural_source_view_matrix.data(), natural_source_view_matrix.size());
    } else {
        // Keep the complete eye state alive until the render-view pass has finished.
        // ChromeEngine performs scene/visibility work after this camera update, so
        // restoring source/frustum fields here would let later culling observe the
        // natural mouse camera while the renderer still uses the HMD-derived matrices.
        g_stereo_eye_override.camera_applied = orientation_applied;
        g_stereo_eye_override.renderer_camera_match = renderer_camera_match;
        g_stereo_eye_override.applied_position = applied_position;
    }

    if (!orientation_applied) return;

    if (tracking_active) {
        if (recentered) {
            EmitEvent(
                "camera_hmd_recentered", "ok",
                "generation=" + std::to_string(snapshot.generation) +
                    ";pose_sequence=" + std::to_string(pose_sequence));
        }
        const RelativeAngles angles = HeadAngles(relative_pose.orientation);
        const bool first_meaningful_roll = std::fabs(angles.roll_degrees) >= 3.0F &&
            !g_meaningful_roll_logged.exchange(true, std::memory_order_acq_rel);
        if (ShouldLogHmdSequence(pose_sequence) || first_meaningful_roll) {
            try {
                std::ostringstream detail;
                detail << "generation=" << snapshot.generation
                       << ";source=hmd"
                       << ";pose_sequence=" << pose_sequence
                       << ";camera=0x" << std::hex << reinterpret_cast<std::uintptr_t>(camera)
                       << std::dec
                       << ";yaw_degrees=" << angles.yaw_degrees
                       << ";pitch_degrees=" << angles.pitch_degrees
                       << ";roll_degrees=" << angles.roll_degrees
                       << ";roll_mode=native_camera_basis"
                       << ";position=" << VectorText(natural_position)
                       << ";relative_head_position=" << RuntimeVectorText(relative_pose.position)
                       << ";head_position_valid="
                       << (relative_pose.position_valid ? "true" : "false")
                       << ";tracked_head_position=" << VectorText(tracked_head_position)
                       << (stereo_active ? ";applied_position=" : ";applied_position=natural")
                       << (stereo_active ? VectorText(applied_position) : std::string{})
                       << (stereo_active ? ";game_units_per_meter=100" : "")
                       << ";natural_right=" << VectorText(natural_right)
                       << ";applied_right=" << VectorText(applied_right)
                       << ";natural_forward=" << VectorText(natural_forward)
                       << ";applied_forward=" << VectorText(applied.forward)
                       << ";natural_determinant=" << CameraProbeBasisDeterminant(natural_basis)
                       << ";applied_determinant=" << CameraProbeBasisDeterminant(applied)
                       << ";native_homogeneous_layout=true"
                       << ";source_world_homogeneous_layout=true"
                       << ";source_view_homogeneous_layout=true"
                       << ";injected_view_homogeneous_layout=true"
                       << ";render_basis_observed=" << (readable_render_basis ? "true" : "false")
                       << ";render_basis_changed=" << (render_basis_changed ? "true" : "false")
                       << ";view_matrix_observed=" << (readable_view_matrices ? "true" : "false")
                       << ";view_matrix_changed=" << (view_matrix_changed ? "true" : "false")
                       << ";projection_matrix_changed="
                       << (projection_matrix_changed ? "true" : "false")
                       << ";view_projection_changed="
                       << (view_projection_changed ? "true" : "false")
                       << ";rendered_forward=" << VectorText(rendered_forward)
                       << (stereo_active ? ";restore_deferred=true" : ";restored=true")
                       << ";stereo=" << (stereo_active ? "true" : "false")
                       << ";renderer_camera_match="
                       << (renderer_camera_match ? "true" : "false");
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
                   << ";natural_determinant=" << CameraProbeBasisDeterminant(natural_basis)
                   << ";applied_determinant=" << CameraProbeBasisDeterminant(applied)
                   << ";native_homogeneous_layout=true"
                   << ";source_world_homogeneous_layout=true"
                   << ";source_view_homogeneous_layout=true"
                   << ";injected_view_homogeneous_layout=true"
                   << ";render_basis_observed=" << (readable_render_basis ? "true" : "false")
                   << ";render_basis_changed=" << (render_basis_changed ? "true" : "false")
                   << ";view_matrix_observed=" << (readable_view_matrices ? "true" : "false")
                   << ";view_matrix_changed=" << (view_matrix_changed ? "true" : "false")
                   << ";view_projection_changed="
                   << (view_projection_changed ? "true" : "false")
                   << ";rendered_forward=" << VectorText(rendered_forward)
                   << ";rendered_up=" << VectorText(rendered_up)
                   << ";restored=true"
                   << ";renderer_camera_match="
                   << (RendererReferencesCamera(camera) ? "true" : "false");
            EmitEvent("camera_probe_orientation_applied", "ok", detail.str());
        } catch (...) {
        }
    }
}

bool StereoCallbacksReady() noexcept {
    return g_stereo_callbacks.context != nullptr &&
        g_stereo_callbacks.begin_frame != nullptr &&
        g_stereo_callbacks.capture_eye != nullptr &&
        g_stereo_callbacks.submit_frame != nullptr;
}

void* CameraForView(void* view) noexcept {
    if (!view || !g_camera_vtable) return nullptr;
    auto* view_bytes = static_cast<std::byte*>(view);
    if (!IsReadable(view_bytes + kViewCameraOffset, sizeof(void*))) return nullptr;
    void* camera = *reinterpret_cast<void**>(view_bytes + kViewCameraOffset);
    if (!camera || !IsReadable(camera, sizeof(void*))) return nullptr;
    auto** camera_vtable = *reinterpret_cast<void***>(camera);
    if (camera_vtable != g_camera_vtable) return nullptr;
    auto* camera_bytes = static_cast<std::byte*>(camera);
    if (!IsReadable(camera_bytes + kCameraViewOwnerOffset, sizeof(void*))) return nullptr;
    if (*reinterpret_cast<void**>(camera_bytes + kCameraViewOwnerOffset) != view) return nullptr;
    return camera;
}

bool RenderOwnerStillUsesView(void* owner, void* view) noexcept {
    if (!owner || !view) return false;
    auto* owner_bytes = static_cast<std::byte*>(owner);
    if (!IsReadable(owner_bytes + kRenderOwnerActiveViewOffset, sizeof(void*))) return false;
    return *reinterpret_cast<void**>(owner_bytes + kRenderOwnerActiveViewOffset) == view;
}

bool ReplayFullRenderViewForStereoEye(
    const RenderViewFn original,
    void* owner,
    void* view,
    bool& rendered_flag_restored) noexcept {
    rendered_flag_restored = false;
    if (!original || !owner || !view) return false;

    auto* rendered_this_frame = reinterpret_cast<std::uint8_t*>(
        static_cast<std::byte*>(view) + kViewRenderedThisFrameOffset);
    if (!IsReadable(rendered_this_frame, sizeof(*rendered_this_frame)) ||
        !IsWritable(rendered_this_frame, sizeof(*rendered_this_frame))) {
        return false;
    }

    // ChromeEngine3::RenderView (0x30FB0) guards each view with view+0xD7,
    // sets it before entering core 0x30E00 and performs additional work after
    // the core returns. The first eye therefore leaves the guard set. Replay
    // the complete wrapper for the second eye by clearing only this exact-build
    // guard for the duration of the call, then restore its natural post-left
    // value so the engine still observes the view as rendered for this frame.
    const std::uint8_t natural_post_left_flag = *rendered_this_frame;
    *rendered_this_frame = 0;
    original(owner, view);
    const bool wrapper_consumed_guard = *rendered_this_frame != 0;
    *rendered_this_frame = natural_post_left_flag;
    rendered_flag_restored = *rendered_this_frame == natural_post_left_flag;
    return wrapper_consumed_guard && rendered_flag_restored;
}

bool ShouldLogStereoFrame(const std::uint64_t frame_sequence) noexcept {
    if (frame_sequence == 0) return false;
    const std::uint64_t count = g_stereo_logged_count.fetch_add(1, std::memory_order_acq_rel) + 1;
    return count <= 8 || (count % 90) == 0;
}

bool ShouldObserveBodyFrame(const std::uint64_t frame_sequence) noexcept {
    return frame_sequence != 0 && (frame_sequence <= 8 || (frame_sequence % 90) == 0);
}

void ObservePlayerSkeleton(
    const std::uint64_t frame_sequence,
    const JavaPlayerPosition position) noexcept {
    if (!ShouldObserveBodyFrame(frame_sequence)) return;

    try {
        std::string error;
        const SkeletonBinding binding = ExactGameSkeletonBinding();
        const std::array<std::pair<const char*, int>, 16> required_bones{{
            {"pelvis", binding.pelvis},
            {"spine", binding.spine},
            {"chest", binding.chest},
            {"head", binding.head},
            {"left_upper_arm", binding.left_upper_arm},
            {"left_forearm", binding.left_forearm},
            {"left_hand", binding.left_hand},
            {"right_upper_arm", binding.right_upper_arm},
            {"right_forearm", binding.right_forearm},
            {"right_hand", binding.right_hand},
            {"left_thigh", binding.left_thigh},
            {"left_shin", binding.left_shin},
            {"left_foot", binding.left_foot},
            {"right_thigh", binding.right_thigh},
            {"right_shin", binding.right_shin},
            {"right_foot", binding.right_foot},
        }};

        std::ostringstream elements;
        bool complete = true;
        for (std::size_t index = 0; index < required_bones.size(); ++index) {
            int element = -1;
            const auto [name, bone] = required_bones[index];
            if (!g_java_player_bridge.TryGetMeshElement(
                    static_cast<std::int8_t>(bone), element, &error)) {
                complete = false;
                elements << (index == 0 ? "" : ",") << name << ":missing";
                continue;
            }
            elements << (index == 0 ? "" : ",") << name << ':' << element;
        }

        std::ostringstream detail;
        detail << "frame_sequence=" << frame_sequence
               << ";player_position=(" << position.x << ',' << position.y << ',' << position.z << ')'
               << ";player_source="
               << (g_java_player_bridge.campaign_module_fallback_used()
                       ? "lawman_module_single_main_player"
                       : (g_java_player_bridge.single_player_fallback_used()
                              ? "single_session_player"
                              : "session_local_player"))
               << ";required_bones=" << elements.str()
               << ";binding_source=code.pak/EBones.class"
               << ";access=read_only";
        EmitEvent("body_player_discovery", complete ? "ok" : "partial", detail.str());
    } catch (...) {
        EmitEvent(
            "body_player_discovery", "exception",
            "frame_sequence=" + std::to_string(frame_sequence));
    }
}

cojvr::runtime::Vec3 RuntimeVector(const JavaPlayerPosition value) noexcept {
    return {value.x, value.y, value.z};
}

JavaPlayerPosition JavaVector(const cojvr::runtime::Vec3 value) noexcept {
    return {value.x, value.y, value.z};
}

bool ReadNaturalCameraFrame(
    const CameraRenderStateSnapshot& natural_render_state,
    CameraProbeBasis& basis,
    CameraProbeVector& position) noexcept {
    static_assert(kCameraSourceUpOffset - kCameraSourceRightOffset == sizeof(float) * 4);
    static_assert(kCameraSourceForwardOffset - kCameraSourceRightOffset == sizeof(float) * 8);
    static_assert(kCameraSourcePositionOffset - kCameraSourceRightOffset == sizeof(float) * 12);
    std::memcpy(&basis.right, natural_render_state.source_world.data(), sizeof(basis.right));
    std::memcpy(
        &basis.up,
        natural_render_state.source_world.data() +
            (kCameraSourceUpOffset - kCameraSourceRightOffset),
        sizeof(basis.up));
    std::memcpy(
        &basis.forward,
        natural_render_state.source_world.data() +
            (kCameraSourceForwardOffset - kCameraSourceRightOffset),
        sizeof(basis.forward));
    std::memcpy(
        &position,
        natural_render_state.source_world.data() +
            (kCameraSourcePositionOffset - kCameraSourceRightOffset),
        sizeof(position));
    return IsCameraProbeBasisRigidRightHanded(basis) &&
        std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

void ObservePostLoadLiveness(
    const CameraRenderStateSnapshot& natural_render_state,
    const std::uint64_t frame_sequence) noexcept {
    static bool initialized = false;
    static DWORD last_input_tick = 0;
    static CameraProbeVector last_camera_position{};
    static bool last_camera_position_valid = false;

    LASTINPUTINFO last_input{sizeof(last_input)};
    const bool last_input_valid = GetLastInputInfo(&last_input) != FALSE;
    const bool input_changed = last_input_valid &&
        (!initialized || last_input.dwTime != last_input_tick);
    const bool periodic = frame_sequence <= 8 || (frame_sequence % 90) == 0;
    if (!input_changed && !periodic) return;

    CameraProbeBasis basis{};
    CameraProbeVector camera_position{};
    const bool camera_valid =
        ReadNaturalCameraFrame(natural_render_state, basis, camera_position);
    const bool camera_changed = camera_valid && last_camera_position_valid &&
        VectorDistanceSquared(camera_position, last_camera_position) > 0.000001F;

    const DWORD process_id = GetCurrentProcessId();
    const DWORD thread_id = GetCurrentThreadId();
    const HWND foreground = GetForegroundWindow();
    const HWND active = GetActiveWindow();
    const HWND focus = GetFocus();
    const auto window_process = [](const HWND window) noexcept {
        DWORD pid = 0;
        if (window) (void)GetWindowThreadProcessId(window, &pid);
        return pid;
    };

    GUITHREADINFO gui{sizeof(gui)};
    const bool gui_valid = GetGUIThreadInfo(thread_id, &gui) != FALSE;
    BYTE keyboard[256]{};
    const bool keyboard_valid = GetKeyboardState(keyboard) != FALSE;
    unsigned int keyboard_down_count = 0;
    if (keyboard_valid) {
        for (unsigned int key = 0; key < 256; ++key) {
            if ((keyboard[key] & 0x80U) != 0U) ++keyboard_down_count;
        }
    }
    POINT cursor{};
    const bool cursor_valid = GetCursorPos(&cursor) != FALSE;
    const DWORD now = GetTickCount();
    const DWORD input_age_ms = last_input_valid ? now - last_input.dwTime : 0;
    bool game_timer_frozen = false;
    std::string game_timer_error;
    const bool game_timer_valid =
        g_java_player_bridge.TryGetActiveGameTimerFrozen(game_timer_frozen, &game_timer_error);

    try {
        std::ostringstream detail;
        detail << "frame_sequence=" << frame_sequence
               << ";reason=" << (input_changed ? "input_activity" : "sample")
               << ";render_thread_id=" << thread_id
               << ";foreground_pid=" << window_process(foreground)
               << ";foreground_is_game="
               << (window_process(foreground) == process_id ? "true" : "false")
               << ";active_pid=" << window_process(active)
               << ";active_is_game="
               << (window_process(active) == process_id ? "true" : "false")
               << ";focus_pid=" << window_process(focus)
               << ";focus_is_game="
               << (window_process(focus) == process_id ? "true" : "false")
               << ";gui_valid=" << (gui_valid ? "true" : "false")
               << ";gui_flags=" << (gui_valid ? gui.flags : 0)
               << ";input_pending=" << (GetInputState() ? "true" : "false")
               << ";last_input_valid=" << (last_input_valid ? "true" : "false")
               << ";last_input_tick=" << (last_input_valid ? last_input.dwTime : 0)
               << ";last_input_age_ms=" << input_age_ms
               << ";keyboard_state_valid=" << (keyboard_valid ? "true" : "false")
               << ";keyboard_down_count=" << keyboard_down_count
               << ";mouse_left_down="
               << (keyboard_valid && (keyboard[VK_LBUTTON] & 0x80U) != 0U ? "true" : "false")
               << ";mouse_right_down="
               << (keyboard_valid && (keyboard[VK_RBUTTON] & 0x80U) != 0U ? "true" : "false")
               << ";cursor_valid=" << (cursor_valid ? "true" : "false")
               << ";cursor=(" << (cursor_valid ? cursor.x : 0) << ','
               << (cursor_valid ? cursor.y : 0) << ')'
               << ";natural_camera_valid=" << (camera_valid ? "true" : "false")
               << ";natural_camera_changed=" << (camera_changed ? "true" : "false")
               << ";game_timer_valid=" << (game_timer_valid ? "true" : "false")
               << ";game_timer_frozen="
               << (game_timer_valid && game_timer_frozen ? "true" : "false");
        if (!game_timer_valid && !game_timer_error.empty()) {
            detail << ";game_timer_error=" << game_timer_error;
        }
        if (camera_valid) {
            detail << ";natural_camera_position=(" << camera_position.x << ','
                   << camera_position.y << ',' << camera_position.z << ')';
        }
        EmitEvent("post_load_liveness", "observed", detail.str());
    } catch (...) {
    }

    initialized = true;
    if (last_input_valid) last_input_tick = last_input.dwTime;
    if (camera_valid) {
        last_camera_position = camera_position;
        last_camera_position_valid = true;
    }
}

bool ResolveArmElements(const std::uint64_t generation, std::string* error) noexcept {
    if (g_arm_element_cache.being_generation == generation && g_arm_element_cache.valid) {
        return true;
    }
    g_arm_element_cache = {};
    g_arm_element_cache.being_generation = generation;
    const SkeletonBinding binding = ExactGameSkeletonBinding();
    const bool complete =
        g_java_player_bridge.TryGetMeshElement(
            static_cast<std::int8_t>(binding.left_upper_arm),
            g_arm_element_cache.left_upper_arm, error) &&
        g_java_player_bridge.TryGetMeshElement(
            static_cast<std::int8_t>(binding.left_forearm),
            g_arm_element_cache.left_forearm, error) &&
        g_java_player_bridge.TryGetMeshElement(
            static_cast<std::int8_t>(binding.left_foretwist),
            g_arm_element_cache.left_foretwist, error) &&
        g_java_player_bridge.TryGetMeshElement(
            static_cast<std::int8_t>(binding.left_hand),
            g_arm_element_cache.left_hand, error) &&
        g_java_player_bridge.TryGetMeshElement(
            static_cast<std::int8_t>(binding.right_upper_arm),
            g_arm_element_cache.right_upper_arm, error) &&
        g_java_player_bridge.TryGetMeshElement(
            static_cast<std::int8_t>(binding.right_forearm),
            g_arm_element_cache.right_forearm, error) &&
        g_java_player_bridge.TryGetMeshElement(
            static_cast<std::int8_t>(binding.right_foretwist),
            g_arm_element_cache.right_foretwist, error) &&
        g_java_player_bridge.TryGetMeshElement(
            static_cast<std::int8_t>(binding.right_hand),
            g_arm_element_cache.right_hand, error);
    g_arm_element_cache.valid = complete;
    return complete;
}

bool ReadArmGeometry(
    const bool left,
    ArmGeometrySample& geometry,
    std::string* error) noexcept {
    geometry = {};
    const SkeletonBinding binding = ExactGameSkeletonBinding();
    const int upper = left ? binding.left_upper_arm : binding.right_upper_arm;
    const int forearm = left ? binding.left_forearm : binding.right_forearm;
    const int hand = left ? binding.left_hand : binding.right_hand;
    JavaPlayerPosition shoulder{};
    JavaPlayerPosition elbow{};
    JavaPlayerPosition wrist{};
    JavaPlayerPosition upper_element_position{};
    JavaPlayerPosition upper_element_up{};
    JavaPlayerPosition upper_element_forward{};
    JavaPlayerPosition forearm_element_position{};
    JavaPlayerPosition forearm_element_up{};
    JavaPlayerPosition forearm_element_forward{};
    JavaPlayerPosition foretwist_element_position{};
    JavaPlayerPosition foretwist_element_up{};
    JavaPlayerPosition foretwist_element_forward{};
    JavaPlayerPosition hand_element_position{};
    JavaPlayerPosition hand_element_up{};
    JavaPlayerPosition hand_element_forward{};
    const int upper_element = left
        ? g_arm_element_cache.left_upper_arm
        : g_arm_element_cache.right_upper_arm;
    const int forearm_element = left
        ? g_arm_element_cache.left_forearm
        : g_arm_element_cache.right_forearm;
    const int foretwist_element = left
        ? g_arm_element_cache.left_foretwist
        : g_arm_element_cache.right_foretwist;
    const int hand_element = left
        ? g_arm_element_cache.left_hand
        : g_arm_element_cache.right_hand;
    if (!g_java_player_bridge.TryGetBoneJointPosition(
            static_cast<std::int8_t>(upper), shoulder, error) ||
        !g_java_player_bridge.TryGetBoneJointPosition(
            static_cast<std::int8_t>(forearm), elbow, error) ||
        !g_java_player_bridge.TryGetBoneJointPosition(
            static_cast<std::int8_t>(hand), wrist, error) ||
        !g_java_player_bridge.TryGetElementWorldBasis(
            upper_element,
            upper_element_position,
            upper_element_up,
            upper_element_forward,
            error) ||
        !g_java_player_bridge.TryGetElementWorldBasis(
            forearm_element,
            forearm_element_position,
            forearm_element_up,
            forearm_element_forward,
            error) ||
        !g_java_player_bridge.TryGetElementWorldBasis(
            foretwist_element,
            foretwist_element_position,
            foretwist_element_up,
            foretwist_element_forward,
            error) ||
        !g_java_player_bridge.TryGetElementWorldBasis(
            hand_element,
            hand_element_position,
            hand_element_up,
            hand_element_forward,
            error)) {
        return false;
    }
    geometry.shoulder = RuntimeVector(shoulder);
    geometry.elbow = RuntimeVector(elbow);
    geometry.wrist = RuntimeVector(wrist);
    geometry.upper_element_position = RuntimeVector(upper_element_position);
    geometry.upper_element_up = RuntimeVector(upper_element_up);
    geometry.upper_element_forward = RuntimeVector(upper_element_forward);
    geometry.forearm_element_position = RuntimeVector(forearm_element_position);
    geometry.forearm_element_up = RuntimeVector(forearm_element_up);
    geometry.forearm_element_forward = RuntimeVector(forearm_element_forward);
    geometry.foretwist_element_position = RuntimeVector(foretwist_element_position);
    geometry.foretwist_element_up = RuntimeVector(foretwist_element_up);
    geometry.foretwist_element_forward = RuntimeVector(foretwist_element_forward);
    geometry.hand_element_position = RuntimeVector(hand_element_position);
    geometry.hand_element_up = RuntimeVector(hand_element_up);
    geometry.hand_element_forward = RuntimeVector(hand_element_forward);
    return true;
}

bool ArmGeometryChanged(
    const ArmGeometrySample& lhs,
    const ArmGeometrySample& rhs) noexcept {
    constexpr float kDifferenceSquared = 0.000001F;
    return
        RuntimeVectorDistanceSquared(lhs.elbow, rhs.elbow) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(lhs.wrist, rhs.wrist) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(
            lhs.upper_element_position, rhs.upper_element_position) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(
            lhs.upper_element_up, rhs.upper_element_up) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(
            lhs.upper_element_forward, rhs.upper_element_forward) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(
            lhs.forearm_element_position, rhs.forearm_element_position) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(
            lhs.forearm_element_up, rhs.forearm_element_up) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(
            lhs.forearm_element_forward, rhs.forearm_element_forward) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(
            lhs.foretwist_element_position, rhs.foretwist_element_position) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(
            lhs.foretwist_element_up, rhs.foretwist_element_up) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(
            lhs.foretwist_element_forward, rhs.foretwist_element_forward) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(
            lhs.hand_element_position, rhs.hand_element_position) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(
            lhs.hand_element_up, rhs.hand_element_up) > kDifferenceSquared ||
        RuntimeVectorDistanceSquared(
            lhs.hand_element_forward, rhs.hand_element_forward) > kDifferenceSquared;
}

void ObserveAppliedArmRenderState(
    const bool left,
    const AppliedArmElementRotation& applied,
    const std::uint64_t frame_sequence,
    const char* phase) noexcept {
    if (!applied.active || !ShouldObserveBodyFrame(frame_sequence)) return;

    ArmGeometrySample rendered{};
    std::string error;
    if (!ReadArmGeometry(left, rendered, &error)) {
        EmitEvent(
            "body_arm_render_probe", "unavailable",
            "frame_sequence=" + std::to_string(frame_sequence) +
                ";side=" + (left ? std::string("left") : std::string("right")) +
                ";phase=" + phase + ";detail=" + error);
        return;
    }

    const bool changed_from_natural =
        ArmGeometryChanged(applied.natural_geometry, rendered);
    const bool matches_post_write = applied.post_write_geometry_valid &&
        !ArmGeometryChanged(applied.post_write_geometry, rendered);
    std::ostringstream detail;
    detail << "frame_sequence=" << frame_sequence
           << ";side=" << (left ? "left" : "right")
           << ";phase=" << phase
           << ";changed_from_natural=" << (changed_from_natural ? "true" : "false")
           << ";post_write_geometry_valid="
           << (applied.post_write_geometry_valid ? "true" : "false")
           << ";matches_post_write=" << (matches_post_write ? "true" : "false")
           << ";elbow=" << RuntimeVectorText(rendered.elbow)
           << ";wrist=" << RuntimeVectorText(rendered.wrist)
           << ";upper_element_up=" << RuntimeVectorText(rendered.upper_element_up)
           << ";upper_element_forward=" << RuntimeVectorText(rendered.upper_element_forward)
           << ";forearm_element_up=" << RuntimeVectorText(rendered.forearm_element_up)
           << ";forearm_element_forward="
           << RuntimeVectorText(rendered.forearm_element_forward)
           << ";foretwist_element_up=" << RuntimeVectorText(rendered.foretwist_element_up)
           << ";foretwist_element_forward="
           << RuntimeVectorText(rendered.foretwist_element_forward)
           << ";hand_element_up=" << RuntimeVectorText(rendered.hand_element_up)
           << ";hand_element_forward="
           << RuntimeVectorText(rendered.hand_element_forward)
           << ";writer=RotateElementWithChildren";
    EmitEvent(
        "body_arm_render_probe",
        changed_from_natural ? "changed" : "natural",
        detail.str());
}

bool ReadLegGeometry(
    const bool left,
    LegGeometrySample& geometry,
    std::string* error) noexcept {
    geometry = {};
    const SkeletonBinding binding = ExactGameSkeletonBinding();
    const int thigh = left ? binding.left_thigh : binding.right_thigh;
    const int shin = left ? binding.left_shin : binding.right_shin;
    const int foot = left ? binding.left_foot : binding.right_foot;
    JavaPlayerPosition hip{};
    JavaPlayerPosition knee{};
    JavaPlayerPosition ankle{};
    JavaPlayerPosition thigh_up{};
    JavaPlayerPosition thigh_forward{};
    JavaPlayerPosition shin_up{};
    JavaPlayerPosition shin_forward{};
    JavaPlayerPosition foot_up{};
    JavaPlayerPosition foot_forward{};
    if (!g_java_player_bridge.TryGetBoneJointPosition(
            static_cast<std::int8_t>(thigh), hip, error) ||
        !g_java_player_bridge.TryGetBoneJointPosition(
            static_cast<std::int8_t>(shin), knee, error) ||
        !g_java_player_bridge.TryGetBoneJointPosition(
            static_cast<std::int8_t>(foot), ankle, error) ||
        !g_java_player_bridge.TryGetBoneDirection(
            static_cast<std::int8_t>(thigh), thigh_up, error) ||
        !g_java_player_bridge.TryGetBonePerpendicular(
            static_cast<std::int8_t>(thigh), thigh_forward, error) ||
        !g_java_player_bridge.TryGetBoneDirection(
            static_cast<std::int8_t>(shin), shin_up, error) ||
        !g_java_player_bridge.TryGetBonePerpendicular(
            static_cast<std::int8_t>(shin), shin_forward, error) ||
        !g_java_player_bridge.TryGetBoneDirection(
            static_cast<std::int8_t>(foot), foot_up, error) ||
        !g_java_player_bridge.TryGetBonePerpendicular(
            static_cast<std::int8_t>(foot), foot_forward, error)) {
        return false;
    }
    geometry.hip = RuntimeVector(hip);
    geometry.knee = RuntimeVector(knee);
    geometry.ankle = RuntimeVector(ankle);
    geometry.thigh_up = RuntimeVector(thigh_up);
    geometry.thigh_forward = RuntimeVector(thigh_forward);
    geometry.shin_up = RuntimeVector(shin_up);
    geometry.shin_forward = RuntimeVector(shin_forward);
    geometry.foot_up = RuntimeVector(foot_up);
    geometry.foot_forward = RuntimeVector(foot_forward);
    return true;
}

bool ApplyArmPlan(
    const bool left,
    const ArmIkPlan& plan,
    const ArmBoneRotationPlan& rotations,
    const HandOrientationTarget& hand_target,
    const ArmGeometrySample& natural_geometry,
    ArmBoneRotationPlan& applied_rotations,
    BoneRotationDelta& applied_forearm_twist,
    BoneRotationDelta& applied_hand_rotation,
    BoneRotationDelta& diagnostic_hand_residual,
    float& skinning_axis_error,
    bool& rollback_attempted,
    bool& rollback_ok,
    std::string* error) noexcept {
    rollback_attempted = false;
    rollback_ok = true;
    applied_rotations = {};
    applied_forearm_twist = {};
    applied_hand_rotation = {};
    diagnostic_hand_residual = {};
    skinning_axis_error = std::numeric_limits<float>::infinity();
    if (!plan.valid || !rotations.valid || !hand_target.valid) {
        if (error) *error = "arm element/orientation rotation plan is invalid";
        return false;
    }

    const int upper = left
        ? g_arm_element_cache.left_upper_arm
        : g_arm_element_cache.right_upper_arm;
    const int forearm = left
        ? g_arm_element_cache.left_forearm
        : g_arm_element_cache.right_forearm;
    const int foretwist = left
        ? g_arm_element_cache.left_foretwist
        : g_arm_element_cache.right_foretwist;
    const int hand = left
        ? g_arm_element_cache.left_hand
        : g_arm_element_cache.right_hand;
    applied_rotations.upper_arm = ConvertWorldRotationToElementLocal(
        rotations.upper_arm,
        natural_geometry.upper_element_up,
        natural_geometry.upper_element_forward);
    if (!applied_rotations.upper_arm.valid) {
        if (error) *error = "upper-arm world-to-element-local rotation conversion failed";
        return false;
    }
    bool upper_applied = false;
    bool forearm_applied = false;
    bool twist_applied = false;
    bool hand_applied = false;

    const auto rotate = [&](const int element,
                            const BoneRotationDelta& rotation,
                            const float sign,
                            std::string* rotate_error) noexcept {
        if (rotation.no_op) return true;
        return g_java_player_bridge.TryRotateElementWithChildren(
            element,
            JavaVector(rotation.axis),
            rotation.angle_degrees * sign,
            rotate_error);
    };
    const auto rollback_chain = [&]() noexcept {
        if (!upper_applied && !forearm_applied && !twist_applied && !hand_applied) return;
        rollback_attempted = true;
        std::string hand_error;
        std::string twist_error;
        std::string forearm_error;
        std::string upper_error;
        const bool hand_ok = !hand_applied || rotate(
            hand, applied_hand_rotation, -1.0F, &hand_error);
        const bool twist_ok = !twist_applied || rotate(
            foretwist, applied_forearm_twist, -1.0F, &twist_error);
        const bool forearm_ok = !forearm_applied || rotate(
            forearm, applied_rotations.forearm, -1.0F, &forearm_error);
        const bool upper_ok = !upper_applied || rotate(
            upper, applied_rotations.upper_arm, -1.0F, &upper_error);
        rollback_ok = hand_ok && twist_ok && forearm_ok && upper_ok;
        if (!rollback_ok && error) {
            if (!error->empty()) *error += "; ";
            *error += "arm RotateElementWithChildren rollback failed";
            if (!hand_error.empty()) *error += "; hand_rollback=" + hand_error;
            if (!twist_error.empty()) *error += "; twist_rollback=" + twist_error;
            if (!forearm_error.empty()) *error += "; forearm_rollback=" + forearm_error;
            if (!upper_error.empty()) *error += "; upper_rollback=" + upper_error;
        }
    };

    std::string write_error;
    if (!rotate(upper, applied_rotations.upper_arm, 1.0F, &write_error)) {
        if (error) *error = write_error;
        return false;
    }
    upper_applied = !applied_rotations.upper_arm.no_op;

    JavaPlayerPosition forearm_position{};
    JavaPlayerPosition forearm_up{};
    JavaPlayerPosition forearm_forward{};
    if (!g_java_player_bridge.TryGetElementWorldBasis(
            forearm,
            forearm_position,
            forearm_up,
            forearm_forward,
            &write_error)) {
        if (error) {
            *error = "post-parent forearm basis read failed";
            if (!write_error.empty()) *error += ": " + write_error;
        }
        rollback_chain();
        return false;
    }
    applied_rotations.forearm = ConvertWorldRotationToElementLocal(
        rotations.forearm,
        RuntimeVector(forearm_up),
        RuntimeVector(forearm_forward));
    applied_rotations.valid = applied_rotations.upper_arm.valid &&
        applied_rotations.forearm.valid;
    if (!applied_rotations.forearm.valid) {
        if (error) *error = "forearm world-to-element-local rotation conversion failed";
        rollback_chain();
        return false;
    }
    if (!rotate(forearm, applied_rotations.forearm, 1.0F, &write_error)) {
        if (error) *error = write_error;
        rollback_chain();
        return false;
    }
    forearm_applied = !applied_rotations.forearm.no_op;

    JavaPlayerPosition foretwist_position{};
    JavaPlayerPosition foretwist_up{};
    JavaPlayerPosition foretwist_forward{};
    JavaPlayerPosition hand_position{};
    JavaPlayerPosition hand_up{};
    JavaPlayerPosition hand_forward{};
    if (!g_java_player_bridge.TryGetElementWorldBasis(
            foretwist,
            foretwist_position,
            foretwist_up,
            foretwist_forward,
            &write_error) ||
        !g_java_player_bridge.TryGetElementWorldBasis(
            hand,
            hand_position,
            hand_up,
            hand_forward,
            &write_error)) {
        if (error) {
            *error = "post-IK foretwist/hand basis read failed";
            if (!write_error.empty()) *error += ": " + write_error;
        }
        rollback_chain();
        return false;
    }

    const HandOrientationRotationPlan orientation_plan =
        BuildHandOrientationRotationPlan(
            {
                plan.wrist_target.x - plan.elbow_target.x,
                plan.wrist_target.y - plan.elbow_target.y,
                plan.wrist_target.z - plan.elbow_target.z,
            },
            RuntimeVector(hand_up),
            RuntimeVector(hand_forward),
            hand_target);
    if (!orientation_plan.valid) {
        if (error) *error = "controller-driven hand orientation plan is invalid";
        rollback_chain();
        return false;
    }

    // FORETWIST is a sibling of forearm in the observed native hierarchy.
    // Reconstruct its full swung frame, then apply the SAME axial roll to that
    // skinning frame and the independently parented hand. Ordinal order does
    // not imply hierarchy: the old path bent forearm but left FORETWIST behind.
    const ArmSkinningPlan skinning = BuildArmSkinningPlan(
        natural_geometry, rotations, orientation_plan.forearm_twist,
        RuntimeVector(hand_up), RuntimeVector(hand_forward));
    if (!skinning.valid) {
        if (error) *error = "arm sibling skinning plan is invalid";
        rollback_chain();
        return false;
    }
    const BoneRotationDelta skinning_rotation = BuildHandResidualRotationDelta(
        RuntimeVector(foretwist_up), RuntimeVector(foretwist_forward), skinning.foretwist);
    applied_forearm_twist = ConvertWorldRotationToElementLocal(
        skinning_rotation,
        RuntimeVector(foretwist_up),
        RuntimeVector(foretwist_forward));
    if (!applied_forearm_twist.valid) {
        if (error) *error = "forearm twist world-to-element-local conversion failed";
        rollback_chain();
        return false;
    }
    if (!rotate(foretwist, applied_forearm_twist, 1.0F, &write_error)) {
        if (error) *error = write_error;
        rollback_chain();
        return false;
    }
    twist_applied = !applied_forearm_twist.no_op;

    if (!g_java_player_bridge.TryGetElementWorldBasis(
            hand,
            hand_position,
            hand_up,
            hand_forward,
            &write_error)) {
        if (error) {
            *error = "post-forearm-twist hand basis read failed";
            if (!write_error.empty()) *error += ": " + write_error;
        }
        rollback_chain();
        return false;
    }
    const BoneRotationDelta observed_hand_residual = BuildHandResidualRotationDelta(
        RuntimeVector(hand_up), RuntimeVector(hand_forward), hand_target);
    if (!observed_hand_residual.valid) {
        if (error) *error = "post-FORETWIST observed hand residual is invalid";
        rollback_chain();
        return false;
    }
    diagnostic_hand_residual = ConvertWorldRotationToElementLocal(
        observed_hand_residual,
        RuntimeVector(hand_up),
        RuntimeVector(hand_forward));
    if (!diagnostic_hand_residual.valid) {
        if (error) *error = "hand world-to-element-local rotation conversion failed";
        rollback_chain();
        return false;
    }

    // Only the shared axial roll belongs on the hand. The arbitrary full
    // controller residual remains diagnostic; it must not bend the wrist.
    applied_hand_rotation = ConvertWorldRotationToElementLocal(
        BuildHandResidualRotationDelta(
            RuntimeVector(hand_up), RuntimeVector(hand_forward), skinning.hand),
        RuntimeVector(hand_up), RuntimeVector(hand_forward));
    if (!applied_hand_rotation.valid ||
        !rotate(hand, applied_hand_rotation, 1.0F, &write_error)) {
        if (error) *error = "shared hand roll write failed: " + write_error;
        rollback_chain();
        return false;
    }
    hand_applied = !applied_hand_rotation.no_op;
    ArmGeometrySample verified{};
    const bool skinning_read = ReadArmGeometry(left, verified, &write_error);
    if (skinning_read) {
        const float errors[] = {
            RuntimeVectorDistanceSquared(verified.foretwist_element_up, skinning.foretwist.up),
            RuntimeVectorDistanceSquared(verified.foretwist_element_forward, skinning.foretwist.forward),
            RuntimeVectorDistanceSquared(verified.hand_element_up, skinning.hand.up),
            RuntimeVectorDistanceSquared(verified.hand_element_forward, skinning.hand.forward),
        };
        skinning_axis_error = 0.0F;
        for (const float squared_error : errors) {
            if (!std::isfinite(squared_error)) {
                skinning_axis_error = std::numeric_limits<float>::infinity();
                break;
            }
            skinning_axis_error = std::max(skinning_axis_error, std::sqrt(squared_error));
        }
    }
    if (!skinning_read || !(skinning_axis_error <= 0.02F)) {
        if (error) *error = "sibling skinning frames did not reach shared swing/roll: " + write_error;
        rollback_chain();
        return false;
    }
    return true;
}

bool RestoreNaturalArmElementFrames(
    const bool left,
    const ArmGeometrySample& natural,
    std::string* error) noexcept {
    const int upper = left
        ? g_arm_element_cache.left_upper_arm
        : g_arm_element_cache.right_upper_arm;
    const int forearm = left
        ? g_arm_element_cache.left_forearm
        : g_arm_element_cache.right_forearm;
    const int foretwist = left
        ? g_arm_element_cache.left_foretwist
        : g_arm_element_cache.right_foretwist;
    const int hand = left
        ? g_arm_element_cache.left_hand
        : g_arm_element_cache.right_hand;
    std::string upper_error;
    if (!g_java_player_bridge.TrySetElementWorldBasis(
            upper,
            JavaVector(natural.upper_element_up),
            JavaVector(natural.upper_element_forward),
            JavaVector(natural.upper_element_position),
            &upper_error)) {
        if (error) *error = "exact upper-arm frame restore failed: " + upper_error;
        return false;
    }
    std::string forearm_error;
    if (!g_java_player_bridge.TrySetElementWorldBasis(
            forearm,
            JavaVector(natural.forearm_element_up),
            JavaVector(natural.forearm_element_forward),
            JavaVector(natural.forearm_element_position),
            &forearm_error)) {
        if (error) *error = "exact forearm frame restore failed: " + forearm_error;
        return false;
    }
    std::string foretwist_error;
    if (!g_java_player_bridge.TrySetElementWorldBasis(
            foretwist,
            JavaVector(natural.foretwist_element_up),
            JavaVector(natural.foretwist_element_forward),
            JavaVector(natural.foretwist_element_position),
            &foretwist_error)) {
        if (error) *error = "exact foretwist frame restore failed: " + foretwist_error;
        return false;
    }
    std::string hand_error;
    if (!g_java_player_bridge.TrySetElementWorldBasis(
            hand,
            JavaVector(natural.hand_element_up),
            JavaVector(natural.hand_element_forward),
            JavaVector(natural.hand_element_position),
            &hand_error)) {
        if (error) *error = "exact hand frame restore failed: " + hand_error;
        return false;
    }
    return true;
}

bool RestoreAppliedArmRotation(
    const bool left,
    AppliedArmElementRotation& applied,
    const std::uint64_t frame_sequence,
    const char* transaction) noexcept {
    if (!applied.active) return true;
    const int upper = left
        ? g_arm_element_cache.left_upper_arm
        : g_arm_element_cache.right_upper_arm;
    const int forearm = left
        ? g_arm_element_cache.left_forearm
        : g_arm_element_cache.right_forearm;
    const int foretwist = left
        ? g_arm_element_cache.left_foretwist
        : g_arm_element_cache.right_foretwist;
    const int hand = left
        ? g_arm_element_cache.left_hand
        : g_arm_element_cache.right_hand;
    const auto inverse = [&](const int element,
                             const BoneRotationDelta& rotation,
                             std::string* restore_error) noexcept {
        if (rotation.no_op) return true;
        return g_java_player_bridge.TryRotateElementWithChildren(
            element,
            JavaVector(rotation.axis),
            -rotation.angle_degrees,
            restore_error);
    };

    std::string hand_error;
    std::string twist_error;
    std::string forearm_error;
    std::string upper_error;
    const bool hand_ok = inverse(hand, applied.hand_rotation, &hand_error);
    const bool twist_ok = inverse(foretwist, applied.forearm_twist, &twist_error);
    const bool forearm_ok = inverse(forearm, applied.rotations.forearm, &forearm_error);
    const bool upper_ok = inverse(upper, applied.rotations.upper_arm, &upper_error);
    ArmGeometrySample restored_geometry{};
    std::string geometry_error;
    bool geometry_read = hand_ok && twist_ok && forearm_ok && upper_ok &&
        ReadArmGeometry(left, restored_geometry, &geometry_error);
    ArmGeometryRestoreCheck restore_check{};
    if (geometry_read) {
        restore_check = CheckArmGeometryRestored(
            applied.natural_geometry, restored_geometry);
    }
    const bool inverse_geometry_restored = geometry_read && restore_check.matches;
    bool exact_restore_attempted = false;
    bool exact_restore_ok = false;
    bool geometry_restored = inverse_geometry_restored;
    std::string exact_restore_error;
    if (!geometry_restored) {
        exact_restore_attempted = true;
        exact_restore_ok = RestoreNaturalArmElementFrames(
            left, applied.natural_geometry, &exact_restore_error);
        if (exact_restore_ok) {
            geometry_error.clear();
            geometry_read = ReadArmGeometry(left, restored_geometry, &geometry_error);
            if (geometry_read) {
                restore_check = CheckArmGeometryRestored(
                    applied.natural_geometry, restored_geometry);
            }
            geometry_restored = geometry_read && restore_check.matches;
            exact_restore_ok = geometry_restored;
        }
    }
    const bool restored = geometry_restored;
    applied.active = false;
    if (!restored) g_arm_rotation_faulted = true;

    try {
        std::ostringstream detail;
        detail << "frame_sequence=" << frame_sequence
               << ";side=" << (left ? "left" : "right")
               << ";hand_restored=" << (hand_ok ? "true" : "false")
               << ";forearm_twist_restored=" << (twist_ok ? "true" : "false")
               << ";forearm_restored=" << (forearm_ok ? "true" : "false")
               << ";upper_restored=" << (upper_ok ? "true" : "false")
               << ";geometry_read=" << (geometry_read ? "true" : "false")
               << ";geometry_restored=" << (geometry_restored ? "true" : "false")
               << ";writer=RotateElementWithChildren"
               << ";twist_owner=foretwist_element"
               << ";transaction=" << transaction
               << ";inverse_geometry_restored="
               << (inverse_geometry_restored ? "true" : "false")
               << ";exact_restore_attempted="
               << (exact_restore_attempted ? "true" : "false")
               << ";exact_restore_ok=" << (exact_restore_ok ? "true" : "false")
               << ";restore_joint_error=" << restore_check.max_joint_position_error
               << ";restore_element_position_error="
               << restore_check.max_element_position_error
               << ";restore_axis_error=" << restore_check.max_axis_error;
        if (!hand_error.empty()) detail << ";hand_detail=" << hand_error;
        if (!twist_error.empty()) detail << ";forearm_twist_detail=" << twist_error;
        if (!forearm_error.empty()) detail << ";forearm_detail=" << forearm_error;
        if (!upper_error.empty()) detail << ";upper_detail=" << upper_error;
        if (!geometry_error.empty()) detail << ";geometry_detail=" << geometry_error;
        if (!exact_restore_error.empty()) {
            detail << ";exact_restore_detail=" << exact_restore_error;
        }
        EmitEvent("body_arm_restore", restored ? "ok" : "failed", detail.str());
    } catch (...) {
    }
    return restored;
}

void RestoreAppliedArmRotations(
    const std::uint64_t frame_sequence,
    const char* transaction = "post_stereo_capture") noexcept {
    const bool left_ok =
        RestoreAppliedArmRotation(true, g_left_arm_rotation, frame_sequence, transaction);
    const bool right_ok =
        RestoreAppliedArmRotation(false, g_right_arm_rotation, frame_sequence, transaction);
    if (!left_ok || !right_ok) g_arm_rotation_faulted = true;
}

bool RecoverArmTrackingAfterRecenter(
    const std::uint64_t frame_sequence,
    const std::uint64_t recenter_sequence) noexcept {
    const bool previous_fault = g_arm_rotation_faulted;
    const bool transaction_active =
        g_left_arm_rotation.active || g_right_arm_rotation.active;

    // Recenter always establishes a new controller-to-hand calibration. The
    // writer latch is a separate safety concern and is only cleared below.
    g_left_hand_orientation = {};
    g_right_hand_orientation = {};

    bool natural_geometry_verified = !previous_fault;
    std::string verification_error;
    if (previous_fault && !transaction_active) {
        const std::uint64_t generation = g_java_player_bridge.being_generation();
        std::string element_error;
        ArmGeometrySample left_geometry{};
        ArmGeometrySample right_geometry{};
        const bool elements_ready = ResolveArmElements(generation, &element_error);
        const bool left_read = elements_ready &&
            ReadArmGeometry(true, left_geometry, &verification_error);
        if (left_read) verification_error.clear();
        const bool right_read = left_read &&
            ReadArmGeometry(false, right_geometry, &verification_error);
        if (right_read) {
            // Solving each freshly-read chain to its own current wrist is a
            // non-mutating health check for finite/non-degenerate natural
            // skeleton geometry before a latched writer is allowed to resume.
            const ArmIkPlan left_natural =
                BuildArmIkPlan(left_geometry, left_geometry.wrist);
            const ArmIkPlan right_natural =
                BuildArmIkPlan(right_geometry, right_geometry.wrist);
            natural_geometry_verified = left_natural.valid && right_natural.valid;
            if (!natural_geometry_verified) {
                verification_error = "fresh natural arm geometry is degenerate";
            }
        } else if (!elements_ready) {
            verification_error = element_error;
        }
    }

    const bool recovered = CanRecoverArmWriterAfterRecenter(
        previous_fault, transaction_active, natural_geometry_verified);
    if (recovered) {
        g_arm_rotation_faulted = false;
        g_left_arm_rotation = {};
        g_right_arm_rotation = {};
    }

    try {
        std::ostringstream detail;
        detail << "frame_sequence=" << frame_sequence
               << ";recenter_sequence=" << recenter_sequence
               << ";previous_fault=" << (previous_fault ? "true" : "false")
               << ";transaction_active=" << (transaction_active ? "true" : "false")
               << ";natural_verified="
               << (natural_geometry_verified ? "true" : "false")
               << ";calibration_invalidated=true";
        if (!verification_error.empty()) detail << ";detail=" << verification_error;
        EmitEvent(
            "body_arm_recovery",
            recovered ? "ok" : "failed",
            detail.str());
    } catch (...) {
    }
    return recovered;
}

void UpdatePlayerArmTracking(
    const CameraRenderStateSnapshot& natural_render_state,
    const bool body_ik_enabled,
    const bool allow_write,
    const std::uint64_t frame_sequence) noexcept {
    const bool observe = ShouldObserveBodyFrame(frame_sequence);
    if (!observe && !body_ik_enabled) return;

    const auto& tracked_body = g_body_tracker.pose();
    CameraProbeBasis basis{};
    CameraProbeVector camera_position{};
    if (!ReadNaturalCameraFrame(natural_render_state, basis, camera_position)) {
        if (observe) {
            EmitEvent(
                "body_arm_tracking", "unavailable",
                "frame_sequence=" + std::to_string(frame_sequence) +
                    ";stage=camera_basis");
        }
        return;
    }
    (void)camera_position;

    const std::uint64_t generation = g_java_player_bridge.being_generation();
    std::string element_error;
    const bool elements_ready = ResolveArmElements(generation, &element_error);
    const SkeletonBinding binding = ExactGameSkeletonBinding();
    JavaPlayerPosition head_joint{};
    std::string head_error;
    const bool head_ready = g_java_player_bridge.TryGetBoneJointPosition(
        static_cast<std::int8_t>(binding.head), head_joint, &head_error);
    const auto update_arm = [&](const bool left, const cojvr::runtime::Pose& controller) noexcept {
        const char* side = left ? "left" : "right";
        if (!controller.position_valid) {
            if (observe) {
                EmitEvent(
                    "body_arm_tracking", "unavailable",
                    "frame_sequence=" + std::to_string(frame_sequence) +
                        ";side=" + side + ";stage=controller_pose");
            }
            return;
        }
        if (!elements_ready) {
            if (observe || body_ik_enabled) {
                EmitEvent(
                    "body_arm_tracking", "unavailable",
                    "frame_sequence=" + std::to_string(frame_sequence) +
                        ";side=" + side + ";stage=elements;detail=" + element_error);
            }
            return;
        }
        if (!head_ready || !tracked_body.head.position_valid) {
            if (observe || body_ik_enabled) {
                EmitEvent(
                    "body_arm_tracking", "unavailable",
                    "frame_sequence=" + std::to_string(frame_sequence) +
                        ";side=" + side + ";stage=head_anchor;detail=" +
                        (head_ready ? std::string("tracked HMD position is unavailable") : head_error));
            }
            return;
        }
        try {
            std::string error;
            ArmGeometrySample geometry{};
            if (!ReadArmGeometry(left, geometry, &error)) {
                if (observe || body_ik_enabled) {
                    EmitEvent(
                        "body_arm_tracking", "unavailable",
                        "frame_sequence=" + std::to_string(frame_sequence) +
                            ";side=" + side + ";stage=skeleton;detail=" + error);
                }
                return;
            }
            if (!controller.orientation_valid) {
                if (observe || body_ik_enabled) {
                    EmitEvent(
                        "body_arm_tracking", "unavailable",
                        "frame_sequence=" + std::to_string(frame_sequence) +
                            ";side=" + side + ";stage=controller_orientation");
                }
                return;
            }
            const cojvr::runtime::Vec3 camera_right{
                basis.right.x, basis.right.y, basis.right.z};
            const cojvr::runtime::Vec3 camera_up{
                basis.up.x, basis.up.y, basis.up.z};
            const cojvr::runtime::Vec3 camera_forward{
                basis.forward.x, basis.forward.y, basis.forward.z};
            auto& orientation_calibration = left
                ? g_left_hand_orientation
                : g_right_hand_orientation;
            const std::uint64_t recenter_sequence =
                g_pose_tracker.last_recenter_sequence();
            if (!orientation_calibration.reference.valid ||
                orientation_calibration.being_generation != generation ||
                orientation_calibration.recenter_sequence != recenter_sequence) {
                orientation_calibration.reference = BuildHandOrientationReference(
                    controller.orientation,
                    camera_right,
                    camera_up,
                    camera_forward,
                    geometry.hand_element_up,
                    geometry.hand_element_forward);
                orientation_calibration.being_generation = generation;
                orientation_calibration.recenter_sequence = recenter_sequence;
            }
            const HandOrientationTarget hand_target =
                BuildTrackedHandOrientationTarget(
                    orientation_calibration.reference,
                    controller.orientation,
                    camera_right,
                    camera_up,
                    camera_forward);
            if (!orientation_calibration.reference.valid || !hand_target.valid) {
                if (observe || body_ik_enabled) {
                    EmitEvent(
                        "body_arm_tracking", "invalid",
                        "frame_sequence=" + std::to_string(frame_sequence) +
                            ";side=" + side + ";stage=controller_orientation_mapping");
                }
                return;
            }
            bool target_valid = false;
            const cojvr::runtime::Vec3 target = BuildTrackedHandTarget(
                RuntimeVector(head_joint),
                camera_right,
                camera_up,
                camera_forward,
                tracked_body.head.position,
                controller.position,
                kGameUnitsPerMeter,
                target_valid);
            if (!target_valid) {
                if (observe || body_ik_enabled) {
                    EmitEvent(
                        "body_arm_tracking", "invalid",
                        "frame_sequence=" + std::to_string(frame_sequence) +
                            ";side=" + side + ";stage=controller_target_mapping");
                }
                return;
            }
            const ArmIkPlan plan = BuildArmIkPlan(geometry, target);
            const ArmBoneRotationPlan rotation_plan =
                BuildArmBoneRotationPlan(geometry, plan);
            if (observe) {
                std::ostringstream natural_probe;
                natural_probe << "frame_sequence=" << frame_sequence
                              << ";side=" << side
                              << ";phase=before_write"
                              << ";elbow=" << RuntimeVectorText(geometry.elbow)
                              << ";wrist=" << RuntimeVectorText(geometry.wrist)
                              << ";upper_element_position="
                              << RuntimeVectorText(geometry.upper_element_position)
                              << ";upper_element_up="
                              << RuntimeVectorText(geometry.upper_element_up)
                              << ";upper_element_forward="
                              << RuntimeVectorText(geometry.upper_element_forward)
                              << ";forearm_element_position="
                              << RuntimeVectorText(geometry.forearm_element_position)
                              << ";forearm_element_up="
                              << RuntimeVectorText(geometry.forearm_element_up)
                              << ";forearm_element_forward="
                              << RuntimeVectorText(geometry.forearm_element_forward)
                              << ";foretwist_element_position="
                              << RuntimeVectorText(geometry.foretwist_element_position)
                              << ";foretwist_element_up="
                              << RuntimeVectorText(geometry.foretwist_element_up)
                              << ";foretwist_element_forward="
                              << RuntimeVectorText(geometry.foretwist_element_forward)
                              << ";hand_element_position="
                              << RuntimeVectorText(geometry.hand_element_position)
                              << ";hand_element_up="
                              << RuntimeVectorText(geometry.hand_element_up)
                              << ";hand_element_forward="
                              << RuntimeVectorText(geometry.hand_element_forward)
                              << ";hand_target_up=" << RuntimeVectorText(hand_target.up)
                              << ";hand_target_forward="
                              << RuntimeVectorText(hand_target.forward)
                              << ";writer=RotateElementWithChildren";
                EmitEvent("body_arm_write_probe", "natural", natural_probe.str());
            }
            bool write_ok = false;
            bool rollback_attempted = false;
            bool rollback_ok = true;
            ArmBoneRotationPlan applied_rotations{};
            BoneRotationDelta applied_forearm_twist{};
            BoneRotationDelta applied_hand_rotation{};
            BoneRotationDelta diagnostic_hand_residual{};
            float skinning_axis_error = std::numeric_limits<float>::infinity();
            float elbow_target_error = std::numeric_limits<float>::infinity();
            float wrist_target_error = std::numeric_limits<float>::infinity();
            float hand_up_error = std::numeric_limits<float>::infinity();
            float hand_forward_error = std::numeric_limits<float>::infinity();
            bool targets_reached = false;
            bool hand_orientation_reached = false;
            std::string write_error;
            const bool write_allowed =
                allow_write && !g_arm_rotation_faulted && rotation_plan.valid &&
                hand_target.valid;
            if (body_ik_enabled && write_allowed && elements_ready && plan.valid) {
                write_ok = ApplyArmPlan(
                    left,
                    plan,
                    rotation_plan,
                    hand_target,
                    geometry,
                    applied_rotations,
                    applied_forearm_twist,
                    applied_hand_rotation,
                    diagnostic_hand_residual,
                    skinning_axis_error,
                    rollback_attempted,
                    rollback_ok,
                    &write_error);
                if (write_ok) {
                    auto& applied = left ? g_left_arm_rotation : g_right_arm_rotation;
                    applied.rotations = applied_rotations;
                    applied.forearm_twist = applied_forearm_twist;
                    applied.hand_rotation = applied_hand_rotation;
                    applied.hand_target = hand_target;
                    applied.natural_geometry = geometry;
                    applied.post_write_geometry = {};
                    applied.active = true;
                    std::string post_write_error;
                    applied.post_write_geometry_valid = ReadArmGeometry(
                        left,
                        applied.post_write_geometry,
                        &post_write_error);
                    const bool expects_change =
                        !applied_rotations.upper_arm.no_op ||
                        !applied_rotations.forearm.no_op ||
                        !applied_forearm_twist.no_op ||
                        !applied_hand_rotation.no_op;
                    const bool changed = applied.post_write_geometry_valid &&
                        ArmGeometryChanged(
                            applied.natural_geometry,
                            applied.post_write_geometry);
                    if (applied.post_write_geometry_valid) {
                        elbow_target_error = std::sqrt(RuntimeVectorDistanceSquared(
                            applied.post_write_geometry.elbow, plan.elbow_target));
                        wrist_target_error = std::sqrt(RuntimeVectorDistanceSquared(
                            applied.post_write_geometry.wrist, plan.wrist_target));
                        constexpr float kArmTargetTolerance = 0.5F;
                        targets_reached = elbow_target_error <= kArmTargetTolerance &&
                            wrist_target_error <= kArmTargetTolerance;
                        hand_up_error = std::sqrt(RuntimeVectorDistanceSquared(
                            applied.post_write_geometry.hand_element_up,
                            hand_target.up));
                        hand_forward_error = std::sqrt(RuntimeVectorDistanceSquared(
                            applied.post_write_geometry.hand_element_forward,
                            hand_target.forward));
                        constexpr float kHandOrientationTolerance = 0.02F;
                        hand_orientation_reached =
                            hand_up_error <= kHandOrientationTolerance &&
                            hand_forward_error <= kHandOrientationTolerance;
                    }
                    if (observe) {
                        if (applied.post_write_geometry_valid) {
                            std::ostringstream probe;
                            probe << "frame_sequence=" << frame_sequence
                                  << ";side=" << side
                                  << ";phase=after_write"
                                  << ";expects_change="
                                  << (expects_change ? "true" : "false")
                                  << ";changed_from_natural="
                                  << (changed ? "true" : "false")
                                  << ";targets_reached="
                                  << (targets_reached ? "true" : "false")
                                  << ";elbow_target_error=" << elbow_target_error
                                  << ";wrist_target_error=" << wrist_target_error
                                  << ";hand_orientation_reached="
                                  << (hand_orientation_reached ? "true" : "false")
                                  << ";hand_up_error=" << hand_up_error
                                  << ";hand_forward_error=" << hand_forward_error
                                  << ";elbow="
                                  << RuntimeVectorText(applied.post_write_geometry.elbow)
                                  << ";wrist="
                                  << RuntimeVectorText(applied.post_write_geometry.wrist)
                                  << ";upper_element_up="
                                  << RuntimeVectorText(
                                         applied.post_write_geometry.upper_element_up)
                                  << ";upper_element_forward="
                                  << RuntimeVectorText(
                                         applied.post_write_geometry.upper_element_forward)
                                  << ";forearm_element_up="
                                  << RuntimeVectorText(
                                         applied.post_write_geometry.forearm_element_up)
                                  << ";forearm_element_forward="
                                  << RuntimeVectorText(
                                         applied.post_write_geometry.forearm_element_forward)
                                  << ";foretwist_element_up="
                                  << RuntimeVectorText(
                                         applied.post_write_geometry.foretwist_element_up)
                                  << ";foretwist_element_forward="
                                  << RuntimeVectorText(
                                         applied.post_write_geometry.foretwist_element_forward)
                                  << ";hand_element_up="
                                  << RuntimeVectorText(
                                         applied.post_write_geometry.hand_element_up)
                                  << ";hand_element_forward="
                                  << RuntimeVectorText(
                                         applied.post_write_geometry.hand_element_forward)
                                  << ";writer=RotateElementWithChildren";
                            EmitEvent(
                                "body_arm_write_probe",
                                changed ? "changed" : "natural",
                                probe.str());
                        } else {
                            EmitEvent(
                                "body_arm_write_probe", "unavailable",
                                "frame_sequence=" + std::to_string(frame_sequence) +
                                    ";side=" + side + ";phase=after_write;writer=RotateElementWithChildren;detail=" +
                                    post_write_error);
                        }
                    }
                    if (!applied.post_write_geometry_valid ||
                        (expects_change && !changed) || !targets_reached) {
                        if (!applied.post_write_geometry_valid) {
                            write_error =
                                "post-write arm geometry could not be read: " + post_write_error;
                        } else if (expects_change && !changed) {
                            write_error =
                                "RotateElementWithChildren returned without changing arm geometry";
                        } else if (!targets_reached) {
                            write_error =
                                "render-element rotation did not reach the solved elbow/wrist targets";
                        }
                        g_arm_rotation_faulted = true;
                        rollback_attempted = true;
                        rollback_ok = RestoreAppliedArmRotation(
                            left,
                            applied,
                            frame_sequence,
                            "write_validation_failure");
                        write_ok = false;
                    }
                }
            } else if (body_ik_enabled && g_arm_rotation_faulted) {
                write_error = "prior element writer/restore validation failed; arm writes are fail-closed";
            }

            if (observe || body_ik_enabled) {
                std::ostringstream detail;
                detail << "frame_sequence=" << frame_sequence
                       << ";being_generation=" << generation
                       << ";side=" << side
                       << ";head_anchor=" << RuntimeVectorText(RuntimeVector(head_joint))
                       << ";tracked_head=" << RuntimeVectorText(tracked_body.head.position)
                       << ";tracked_hand=" << RuntimeVectorText(controller.position)
                       << ";controller_target=" << RuntimeVectorText(target)
                       << ";shoulder=" << RuntimeVectorText(geometry.shoulder)
                       << ";elbow=" << RuntimeVectorText(geometry.elbow)
                       << ";wrist=" << RuntimeVectorText(geometry.wrist)
                       << ";upper_element_position="
                       << RuntimeVectorText(geometry.upper_element_position)
                       << ";upper_element_up=" << RuntimeVectorText(geometry.upper_element_up)
                       << ";upper_element_forward="
                       << RuntimeVectorText(geometry.upper_element_forward)
                       << ";forearm_element_position="
                       << RuntimeVectorText(geometry.forearm_element_position)
                       << ";forearm_element_up="
                       << RuntimeVectorText(geometry.forearm_element_up)
                       << ";forearm_element_forward="
                       << RuntimeVectorText(geometry.forearm_element_forward)
                       << ";foretwist_element_position="
                       << RuntimeVectorText(geometry.foretwist_element_position)
                       << ";foretwist_element_up="
                       << RuntimeVectorText(geometry.foretwist_element_up)
                       << ";foretwist_element_forward="
                       << RuntimeVectorText(geometry.foretwist_element_forward)
                       << ";hand_element_position="
                       << RuntimeVectorText(geometry.hand_element_position)
                       << ";hand_element_up=" << RuntimeVectorText(geometry.hand_element_up)
                       << ";hand_element_forward="
                       << RuntimeVectorText(geometry.hand_element_forward)
                       << ";hand_target_up=" << RuntimeVectorText(hand_target.up)
                       << ";hand_target_forward=" << RuntimeVectorText(hand_target.forward)
                       << ";elbow_target=" << RuntimeVectorText(plan.elbow_target)
                       << ";upper_target_position="
                       << RuntimeVectorText(plan.upper_arm.position)
                       << ";upper_target_up=" << RuntimeVectorText(plan.upper_arm.up)
                       << ";upper_target_forward=" << RuntimeVectorText(plan.upper_arm.forward)
                       << ";forearm_target_position="
                       << RuntimeVectorText(plan.forearm.position)
                       << ";forearm_target_up=" << RuntimeVectorText(plan.forearm.up)
                       << ";forearm_target_forward="
                       << RuntimeVectorText(plan.forearm.forward)
                       << ";upper_rotation_axis="
                       << RuntimeVectorText(rotation_plan.upper_arm.axis)
                       << ";upper_rotation_degrees=" << rotation_plan.upper_arm.angle_degrees
                       << ";upper_rotation_no_op="
                       << (rotation_plan.upper_arm.no_op ? "true" : "false")
                       << ";forearm_rotation_axis="
                       << RuntimeVectorText(rotation_plan.forearm.axis)
                       << ";forearm_rotation_degrees=" << rotation_plan.forearm.angle_degrees
                       << ";forearm_rotation_no_op="
                       << (rotation_plan.forearm.no_op ? "true" : "false")
                       << ";upper_native_axis="
                       << RuntimeVectorText(applied_rotations.upper_arm.axis)
                       << ";forearm_native_axis="
                       << RuntimeVectorText(applied_rotations.forearm.axis)
                       << ";forearm_twist_native_axis="
                       << RuntimeVectorText(applied_forearm_twist.axis)
                       << ";forearm_twist_degrees="
                       << applied_forearm_twist.angle_degrees
                       << ";forearm_twist_no_op="
                       << (applied_forearm_twist.no_op ? "true" : "false")
                       << ";twist_owner=foretwist_element"
                       << ";hand_residual_source=post_foretwist_observed_basis_diagnostic"
                       << ";hand_residual_native_axis="
                       << RuntimeVectorText(diagnostic_hand_residual.axis)
                       << ";hand_residual_degrees="
                       << diagnostic_hand_residual.angle_degrees
                       << ";hand_rotation_mode=sibling_shared_roll"
                       << ";skinning_contract=foretwist_sibling_swing_hand_shared_roll"
                       << ";skinning_frames_reached=" << (write_ok ? "true" : "false")
                       << ";skinning_axis_error=" << skinning_axis_error
                       << ";hand_native_axis="
                       << RuntimeVectorText(applied_hand_rotation.axis)
                       << ";hand_rotation_degrees="
                       << applied_hand_rotation.angle_degrees
                       << ";hand_rotation_no_op="
                       << (applied_hand_rotation.no_op ? "true" : "false")
                       << ";native_axis_space=element_local"
                       << ";elbow_target_error=" << elbow_target_error
                       << ";wrist_target_error=" << wrist_target_error
                       << ";targets_reached=" << (targets_reached ? "true" : "false")
                       << ";hand_up_error=" << hand_up_error
                       << ";hand_forward_error=" << hand_forward_error
                       << ";hand_orientation_reached="
                       << (hand_orientation_reached ? "true" : "false")
                       << ";controller_orientation_valid=true"
                       << ";orientation_calibration_recenter_sequence="
                       << orientation_calibration.recenter_sequence
                       << ";rotation_plan_valid="
                       << (rotation_plan.valid ? "true" : "false")
                       << ";upper_length=" << plan.upper_length
                       << ";lower_length=" << plan.lower_length
                       << ";plan_valid=" << (plan.valid ? "true" : "false")
                       << ";target_clamped=" << (plan.target_clamped ? "true" : "false")
                       << ";write_enabled=" << (body_ik_enabled ? "true" : "false")
                       << ";write_allowed=" << (write_allowed ? "true" : "false")
                       << ";write_ok=" << (write_ok ? "true" : "false")
                       << ";rollback_attempted=" << (rollback_attempted ? "true" : "false")
                       << ";rollback_ok=" << (rollback_ok ? "true" : "false")
                       << ";tracking_forward=-z_to_negative_native_forward"
                       << ";hand_orientation=calibrated_controller_delta_sibling_shared_roll"
                       << ";basis_source=GetElementPos/GetElementLeftVector/GetElementUpVector"
                       << ";writer=RotateElementWithChildren";
                if (body_ik_enabled && write_allowed && plan.valid && !write_ok) {
                    detail << ";detail=" << write_error;
                } else if (body_ik_enabled && g_arm_rotation_faulted) {
                    detail << ";detail=" << write_error;
                }
                const char* result = "observed";
                if (body_ik_enabled && write_allowed) {
                    result = write_ok
                        ? "applied"
                        : (rollback_attempted && !rollback_ok ? "rollback_failed" : "write_failed");
                } else if (body_ik_enabled && g_arm_rotation_faulted) {
                    result = "write_failed";
                }
                EmitEvent("body_arm_tracking", result, detail.str());
            }
        } catch (...) {
            EmitEvent(
                "body_arm_tracking", "exception",
                "frame_sequence=" + std::to_string(frame_sequence) + ";side=" + side);
        }
    };

    update_arm(true, tracked_body.left_hand);
    update_arm(false, tracked_body.right_hand);
    if (g_arm_rotation_faulted) {
        RestoreAppliedArmRotations(frame_sequence, "fail_closed_before_render");
    }
}

void UpdatePlayerLowerBodyObservation(
    const CameraRenderStateSnapshot& natural_render_state,
    const JavaPlayerPosition player_position,
    const std::uint64_t frame_sequence) noexcept {
    if (!ShouldObserveBodyFrame(frame_sequence)) return;

    try {
        CameraProbeBasis basis{};
        CameraProbeVector camera_position{};
        if (!ReadNaturalCameraFrame(natural_render_state, basis, camera_position)) {
            EmitEvent(
                "body_lower_tracking", "unavailable",
                "frame_sequence=" + std::to_string(frame_sequence) +
                    ";stage=camera_basis;write_enabled=false");
            return;
        }
        (void)camera_position;

        const SkeletonBinding binding = ExactGameSkeletonBinding();
        JavaPlayerPosition pelvis_joint{};
        std::string pelvis_error;
        if (!g_java_player_bridge.TryGetBoneJointPosition(
                static_cast<std::int8_t>(binding.pelvis), pelvis_joint, &pelvis_error)) {
            EmitEvent(
                "body_lower_tracking", "unavailable",
                "frame_sequence=" + std::to_string(frame_sequence) +
                    ";stage=pelvis;detail=" + pelvis_error + ";write_enabled=false");
            return;
        }

        const PelvisLocomotionAnchor pelvis_anchor = BuildPelvisLocomotionAnchor(
            RuntimeVector(player_position), RuntimeVector(pelvis_joint));
        if (!pelvis_anchor.valid) {
            EmitEvent(
                "body_lower_tracking", "invalid",
                "frame_sequence=" + std::to_string(frame_sequence) +
                    ";stage=pelvis_anchor;write_enabled=false");
            return;
        }

        const auto& tracked_body = g_body_tracker.pose();
        const auto observe_leg = [&](const bool left, const cojvr::runtime::Pose& foot_pose) noexcept {
            const char* side = left ? "left" : "right";
            if (!foot_pose.position_valid) {
                EmitEvent(
                    "body_lower_tracking", "unavailable",
                    "frame_sequence=" + std::to_string(frame_sequence) +
                        ";side=" + side + ";stage=foot_target;write_enabled=false");
                return;
            }
            try {
                std::string error;
                LegGeometrySample geometry{};
                if (!ReadLegGeometry(left, geometry, &error)) {
                    EmitEvent(
                        "body_lower_tracking", "unavailable",
                        "frame_sequence=" + std::to_string(frame_sequence) +
                            ";side=" + side + ";stage=skeleton;detail=" + error +
                            ";write_enabled=false");
                    return;
                }

                bool foot_target_valid = false;
                const cojvr::runtime::Vec3 foot_target = BuildTrackedFootTarget(
                    pelvis_anchor.world_target,
                    {basis.right.x, basis.right.y, basis.right.z},
                    {basis.up.x, basis.up.y, basis.up.z},
                    {basis.forward.x, basis.forward.y, basis.forward.z},
                    tracked_body.pelvis.position,
                    foot_pose.position,
                    kGameUnitsPerMeter,
                    foot_target_valid);
                if (!foot_target_valid) {
                    EmitEvent(
                        "body_lower_tracking", "invalid",
                        "frame_sequence=" + std::to_string(frame_sequence) +
                            ";side=" + side + ";stage=foot_target_mapping;write_enabled=false");
                    return;
                }
                const LegIkPlan plan = BuildLegIkPlan(geometry, foot_target);
                std::ostringstream detail;
                detail << "frame_sequence=" << frame_sequence
                       << ";being_generation=" << g_java_player_bridge.being_generation()
                       << ";side=" << side
                       << ";actor_position=" << RuntimeVectorText(pelvis_anchor.actor_position)
                       << ";pelvis_offset=" << RuntimeVectorText(pelvis_anchor.pelvis_offset)
                       << ";pelvis_target=" << RuntimeVectorText(pelvis_anchor.world_target)
                       << ";hip=" << RuntimeVectorText(geometry.hip)
                       << ";knee=" << RuntimeVectorText(geometry.knee)
                       << ";ankle=" << RuntimeVectorText(geometry.ankle)
                       << ";foot_target=" << RuntimeVectorText(foot_target)
                       << ";thigh_length=" << plan.thigh_length
                       << ";shin_length=" << plan.shin_length
                       << ";plan_valid=" << (plan.valid ? "true" : "false")
                       << ";target_clamped=" << (plan.target_clamped ? "true" : "false")
                       << ";knee_plane_valid=" << (plan.knee_plane_valid ? "true" : "false")
                       << ";write_enabled=false"
                       << ";foot_orientation=natural"
                       << ";basis_source=GetBoneDirVector/GetBonePerpVector"
                       << ";writer=disabled_preflight";
                EmitEvent("body_lower_tracking", plan.valid ? "observed" : "invalid", detail.str());
            } catch (...) {
                EmitEvent(
                    "body_lower_tracking", "exception",
                    "frame_sequence=" + std::to_string(frame_sequence) +
                        ";side=" + side + ";write_enabled=false");
            }
        };

        observe_leg(true, tracked_body.left_foot);
        observe_leg(false, tracked_body.right_foot);
    } catch (...) {
        EmitEvent(
            "body_lower_tracking", "exception",
            "frame_sequence=" + std::to_string(frame_sequence) + ";write_enabled=false");
    }
}

struct PlayerBodyPositionUpdate {
    cojvr::runtime::Pose render_head_pose{};
    bool positional_6dof = false;
    const char* translation_mode = "rotation_only_actor_unresolved";
};

PlayerBodyPositionUpdate UpdatePlayerBodyPosition(
    const CameraRenderStateSnapshot& natural_render_state,
    const cojvr::runtime::Pose& relative_head_pose,
    const bool recentered,
    const std::uint64_t frame_sequence) noexcept {
    PlayerBodyPositionUpdate result{};
    result.render_head_pose = SuppressPhysicalTrackingTranslation(relative_head_pose);
    auto& render_head_pose = result.render_head_pose;
    if (!relative_head_pose.position_valid ||
        !g_body_tracker.PlayerSpace().position_valid) {
        result.translation_mode = relative_head_pose.position_valid
            ? "rotation_only_player_space_unavailable"
            : "tracking_position_invalid";
        return result;
    }

    try {
        std::string error;
        if (!g_java_player_bridge.Refresh(&error)) {
            g_body_player_space_applied = false;
            g_body_player_generation = 0;
            if (ShouldObserveBodyFrame(frame_sequence)) {
                EmitEvent(
                    "body_player_reconciliation", "unavailable",
                    "frame_sequence=" + std::to_string(frame_sequence) +
                        ";stage=player;detail=" + error);
            }
            result.translation_mode = "rotation_only_actor_unresolved";
            return result;
        }

        JavaPlayerPosition player_position{};
        if (!g_java_player_bridge.TryGetPosition(player_position, &error)) {
            g_body_player_space_applied = false;
            if (ShouldObserveBodyFrame(frame_sequence)) {
                EmitEvent(
                    "body_player_reconciliation", "unavailable",
                    "frame_sequence=" + std::to_string(frame_sequence) +
                        ";stage=position;detail=" + error);
            }
            result.translation_mode = "rotation_only_actor_position_unavailable";
            return result;
        }

        ObservePlayerSkeleton(frame_sequence, player_position);

        const UpperBodyTrackingOffsets upper_body =
            UpperBodyOffsetsFromHeadPose(relative_head_pose);
        bool upper_body_write_ok = false;
        std::string upper_body_error;
        if (upper_body.valid) {
            upper_body_write_ok = g_java_player_bridge.TryApplyUpperBodyTracking(
                upper_body.head_horizontal_degrees,
                upper_body.spine_horizontal_degrees,
                upper_body.head_vertical_degrees,
                &upper_body_error);
        } else {
            upper_body_error = "relative HMD orientation is invalid";
        }
        if (ShouldObserveBodyFrame(frame_sequence) || !upper_body_write_ok) {
            std::ostringstream detail;
            detail << "frame_sequence=" << frame_sequence
                   << ";being_generation=" << g_java_player_bridge.being_generation()
                   << ";head_yaw_offset=" << upper_body.head_horizontal_degrees
                   << ";spine_yaw_offset=" << upper_body.spine_horizontal_degrees
                   << ";head_pitch_offset=" << upper_body.head_vertical_degrees
                   << ";source=ArmedPlayerBeing.UpdateBodyRotation(FZZ)"
                   << ";recompute_angles=false"
                   << ";state_restored=true";
            if (!upper_body_write_ok) detail << ";detail=" << upper_body_error;
            EmitEvent(
                "body_upper_tracking",
                upper_body_write_ok ? "ok" : (upper_body.valid ? "write_failed" : "invalid"),
                detail.str());
        }

        CameraProbeVector camera_right{};
        CameraProbeVector camera_forward{};
        static_assert(kCameraSourceForwardOffset - kCameraSourceRightOffset == sizeof(float) * 8);
        std::memcpy(
            &camera_right,
            natural_render_state.source_world.data(),
            sizeof(camera_right));
        std::memcpy(
            &camera_forward,
            natural_render_state.source_world.data() +
                (kCameraSourceForwardOffset - kCameraSourceRightOffset),
            sizeof(camera_forward));

        const std::uint64_t generation = g_java_player_bridge.being_generation();
        const bool same_player = g_body_player_space_applied &&
            g_body_player_generation == generation;
        const auto reconciliation = ReconcilePlayerSpace(
            {player_position.x, player_position.y, player_position.z},
            {camera_right.x, camera_right.y, camera_right.z},
            {camera_forward.x, camera_forward.y, camera_forward.z},
            relative_head_pose.position,
            g_body_tracker.PlayerSpace().position,
            g_body_applied_world_offset,
            g_body_applied_tracking_offset,
            same_player,
            recentered,
            kGameUnitsPerMeter);
        if (!reconciliation.valid) {
            if (ShouldObserveBodyFrame(frame_sequence)) {
                EmitEvent(
                    "body_player_reconciliation", "invalid",
                    "frame_sequence=" + std::to_string(frame_sequence));
            }
            result.translation_mode = "rotation_only_reconciliation_invalid";
            return result;
        }

        const float dx = reconciliation.desired_actor_position.x - player_position.x;
        const float dy = reconciliation.desired_actor_position.y - player_position.y;
        const float dz = reconciliation.desired_actor_position.z - player_position.z;
        constexpr float kPositionWriteEpsilonSquared = 0.0001F;
        const bool position_write_needed = dx * dx + dy * dy + dz * dz >
            kPositionWriteEpsilonSquared;
        bool position_write_ok = true;
        if (position_write_needed) {
            position_write_ok = g_java_player_bridge.TrySetPosition(
                {
                    reconciliation.desired_actor_position.x,
                    reconciliation.desired_actor_position.y,
                    reconciliation.desired_actor_position.z,
                },
                &error);
        }

        if (position_write_ok) {
            render_head_pose.position = reconciliation.render_head_position;
            g_body_applied_world_offset = reconciliation.applied_world_offset;
            g_body_applied_tracking_offset = reconciliation.applied_tracking_offset;
            g_body_player_generation = generation;
            g_body_player_space_applied = true;
            result.positional_6dof = true;
            result.translation_mode = "actor_reconciled";
        } else {
            render_head_pose = SuppressPhysicalTrackingTranslation(relative_head_pose);
            result.translation_mode = "rotation_only_actor_write_failed";
        }

        const bool body_ik_enabled = CurrentCommand().command.body_ik_enabled;
        UpdatePlayerArmTracking(
            natural_render_state,
            body_ik_enabled,
            position_write_ok && upper_body_write_ok,
            frame_sequence);
        UpdatePlayerLowerBodyObservation(
            natural_render_state,
            player_position,
            frame_sequence);

        if (ShouldObserveBodyFrame(frame_sequence) || !position_write_ok) {
            std::ostringstream detail;
            detail << "frame_sequence=" << frame_sequence
                   << ";being_generation=" << generation
                   << ";same_player=" << (same_player ? "true" : "false")
                   << ";recentered=" << (recentered ? "true" : "false")
                   << ";write_needed=" << (position_write_needed ? "true" : "false")
                   << ";write_ok=" << (position_write_ok ? "true" : "false")
                   << ";player_before=(" << player_position.x << ',' << player_position.y << ','
                   << player_position.z << ')'
                   << ";player_desired=(" << reconciliation.desired_actor_position.x << ','
                   << reconciliation.desired_actor_position.y << ','
                   << reconciliation.desired_actor_position.z << ')'
                   << ";tracking_offset="
                   << RuntimeVectorText(reconciliation.applied_tracking_offset)
                   << ";world_offset=" << RuntimeVectorText(reconciliation.applied_world_offset)
                   << ";render_head_position=" << RuntimeVectorText(render_head_pose.position)
                   << ";game_units_per_meter=" << kGameUnitsPerMeter;
            if (!position_write_ok) detail << ";detail=" << error;
            EmitEvent(
                "body_player_reconciliation",
                position_write_ok ? "ok" : "write_failed",
                detail.str());
        }
    } catch (...) {
        EmitEvent(
            "body_player_reconciliation", "exception",
            "frame_sequence=" + std::to_string(frame_sequence));
        result.render_head_pose = SuppressPhysicalTrackingTranslation(relative_head_pose);
        result.positional_6dof = false;
        result.translation_mode = "rotation_only_reconciliation_exception";
    }
    return result;
}

void ConfigureStereoEyeOverride(
    void* camera,
    const cojvr::runtime::Pose& relative_head_pose,
    const cojvr::runtime::EyeView& eye,
    const std::uint64_t frame_sequence,
    const std::uint64_t pose_sequence) noexcept {
    g_stereo_eye_override.active = true;
    g_stereo_eye_override.view_hook_active = true;
    g_stereo_eye_override.camera = camera;
    g_stereo_eye_override.relative_head_pose = relative_head_pose;
    g_stereo_eye_override.eye = &eye;
    g_stereo_eye_override.frame_sequence = frame_sequence;
    g_stereo_eye_override.pose_sequence = pose_sequence;
    g_stereo_eye_override.camera_applied = false;
    g_stereo_eye_override.projection_applied = false;
    g_stereo_eye_override.renderer_camera_match = false;
}

void __fastcall HookRenderView(void* owner, void*, void* view) {
    const auto original = g_original_render_view;
    if (!original) return;
    if (g_stereo_eye_override.view_hook_active) {
        original(owner, view);
        return;
    }

    const CommandSnapshot snapshot = CurrentCommand();
    if (!snapshot.command.tracking_enabled || !StereoCallbacksReady()) {
        (void)g_java_player_bridge.TryApplyGameplayInput({}, nullptr);
        original(owner, view);
        return;
    }

    void* camera = CameraForView(view);
    if (!camera) {
        original(owner, view);
        return;
    }

    CameraRenderStateSnapshot natural_render_state{};
    if (!CaptureCameraRenderState(camera, natural_render_state)) {
        EmitEvent(
            "camera_native_stereo_state", "unavailable",
            "camera=" + std::to_string(reinterpret_cast<std::uintptr_t>(camera)));
        original(owner, view);
        return;
    }

    CameraStereoFrameSample sample{};
    if (!g_stereo_callbacks.begin_frame(g_stereo_callbacks.context, sample) ||
        !sample.hmd_pose.pose.orientation_valid || sample.hmd_pose.sequence == 0 ||
        sample.eyes[0].eye != cojvr::runtime::Eye::left ||
        sample.eyes[1].eye != cojvr::runtime::Eye::right) {
        (void)g_java_player_bridge.TryApplyGameplayInput({}, nullptr);
        original(owner, view);
        return;
    }

    g_pose_tracker.SetEnabled(true);
    bool explicit_recenter_requested = false;
    if (sample.recenter_requested) {
        g_pose_tracker.RequestRecenter();
        explicit_recenter_requested = true;
        EmitEvent(
            "camera_hmd_recenter_requested", "ok",
            "source=openvr_global_action;pose_sequence=" +
                std::to_string(sample.hmd_pose.sequence));
    }
    if (snapshot.command.recenter &&
        FirstForGeneration(g_recenter_command_generation, snapshot.generation)) {
        g_pose_tracker.RequestRecenter();
        explicit_recenter_requested = true;
    }
    const std::uint64_t prior_recenter = g_pose_tracker.last_recenter_sequence();
    cojvr::runtime::Pose relative_head_pose{};
    if (!g_pose_tracker.Update(sample.hmd_pose) ||
        !g_pose_tracker.CurrentPose(relative_head_pose)) {
        (void)g_java_player_bridge.TryApplyGameplayInput({}, nullptr);
        original(owner, view);
        return;
    }
    g_body_tracker.SetHeadPose(relative_head_pose);
    g_body_tracker.UpdatePlayerSpace(relative_head_pose);
    cojvr::runtime::Pose relative_left_controller{};
    cojvr::runtime::Pose relative_right_controller{};
    (void)g_pose_tracker.TransformPose(sample.left_controller, relative_left_controller);
    (void)g_pose_tracker.TransformPose(sample.right_controller, relative_right_controller);
    g_body_tracker.SetHandPoses(relative_left_controller, relative_right_controller);
    const bool recentered = g_pose_tracker.last_recenter_sequence() != 0 &&
        g_pose_tracker.last_recenter_sequence() != prior_recenter;
    if (recentered) {
        EmitEvent(
            "camera_hmd_recentered", "ok",
            "generation=" + std::to_string(snapshot.generation) +
                ";pose_sequence=" + std::to_string(sample.hmd_pose.sequence) +
                ";stereo=true");
        if (explicit_recenter_requested) {
            (void)RecoverArmTrackingAfterRecenter(
                g_stereo_frame_sequence.load(std::memory_order_acquire) + 1,
                g_pose_tracker.last_recenter_sequence());
        }
    }

    const std::uint64_t frame_sequence =
        g_stereo_frame_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
    const bool gameplay_changed = !g_gameplay_telemetry_initialized ||
        GameplayInputChanged(sample.gameplay, g_last_gameplay_telemetry);
    std::string gameplay_input_error;
    const bool gameplay_input_ok = g_java_player_bridge.TryApplyGameplayInput(
        sample.gameplay, &gameplay_input_error);
    if (!gameplay_input_ok || gameplay_changed || ShouldObserveBodyFrame(frame_sequence)) {
        try {
            std::ostringstream gameplay_detail;
            gameplay_detail << "frame_sequence=" << frame_sequence
                            << ";active=" << (sample.gameplay.active ? "true" : "false")
                            << ";move=" << sample.gameplay.move.x << ',' << sample.gameplay.move.y
                            << ";turn=" << sample.gameplay.turn.x << ',' << sample.gameplay.turn.y
                            << ";fire_left=" << (sample.gameplay.fire_left ? "true" : "false")
                            << ";fire_right=" << (sample.gameplay.fire_right ? "true" : "false")
                            << ";jump=" << (sample.gameplay.jump ? "true" : "false")
                            << ";reload=" << (sample.gameplay.reload ? "true" : "false")
                            << ";run=" << (sample.gameplay.run ? "true" : "false")
                            << ";crouch=" << (sample.gameplay.crouch ? "true" : "false")
                            << ";interact=" << (sample.gameplay.interact ? "true" : "false")
                            << ";weapon_next="
                            << (sample.gameplay.weapon_next ? "true" : "false")
                            << ";weapon_previous="
                            << (sample.gameplay.weapon_previous ? "true" : "false")
                            << ";kick=" << (sample.gameplay.kick ? "true" : "false")
                            << ";route=GameInputController.InputAction.Translate";
            if (!gameplay_input_error.empty()) {
                gameplay_detail << ";detail=" << gameplay_input_error;
            }
            EmitEvent(
                "gameplay_input",
                gameplay_input_ok ? "applied" : "unavailable",
                gameplay_detail.str());
        } catch (...) {
        }
    }
    if (gameplay_input_ok) {
        g_last_gameplay_telemetry = sample.gameplay;
        g_gameplay_telemetry_initialized = true;
    }
    ObservePostLoadLiveness(natural_render_state, frame_sequence);
    if (ShouldObserveBodyFrame(frame_sequence)) {
        try {
            std::ostringstream body_input;
            body_input << "frame_sequence=" << frame_sequence
                       << ";controller_pose_source=handgrip"
                       << ";raw_role_fallback=false"
                       << ";left_position_valid="
                       << (relative_left_controller.position_valid ? "true" : "false")
                       << ";left_orientation_valid="
                       << (relative_left_controller.orientation_valid ? "true" : "false")
                       << ";left_position=" << RuntimeVectorText(relative_left_controller.position)
                       << ";right_position_valid="
                       << (relative_right_controller.position_valid ? "true" : "false")
                       << ";right_orientation_valid="
                       << (relative_right_controller.orientation_valid ? "true" : "false")
                       << ";right_position=" << RuntimeVectorText(relative_right_controller.position);
            EmitEvent("body_tracking_input", "observed", body_input.str());
        } catch (...) {
        }
    }
    const PlayerBodyPositionUpdate body_position_update = UpdatePlayerBodyPosition(
        natural_render_state, relative_head_pose,
        recentered,
        frame_sequence);
    const cojvr::runtime::Pose& render_head_pose = body_position_update.render_head_pose;
    std::uint64_t left_hash = 0;
    std::uint64_t right_hash = 0;
    bool left_captured = false;
    bool right_captured = false;
    bool submitted = false;
    bool right_rendered = false;
    bool right_full_view_pass = false;
    bool right_view_guard_restored = false;

    ConfigureStereoEyeOverride(
        camera, render_head_pose, sample.eyes[0], frame_sequence, sample.hmd_pose.sequence);
    original(owner, view);
    ObserveAppliedArmRenderState(
        true, g_left_arm_rotation, frame_sequence, "left_eye_complete");
    ObserveAppliedArmRenderState(
        false, g_right_arm_rotation, frame_sequence, "left_eye_complete");
    const bool left_camera_applied = g_stereo_eye_override.camera_applied;
    const bool left_projection_applied = g_stereo_eye_override.projection_applied;
    const bool left_renderer_camera_match = g_stereo_eye_override.renderer_camera_match;
    const CameraProbeVector left_applied_position = g_stereo_eye_override.applied_position;
    const CameraProbeFrustum left_applied_frustum = g_stereo_eye_override.applied_frustum;
    if (left_camera_applied && left_projection_applied) {
        left_captured = g_stereo_callbacks.capture_eye(
            g_stereo_callbacks.context, cojvr::runtime::Eye::left,
            frame_sequence, &left_hash);
    }
    const bool left_state_restored = RestoreCameraRenderState(camera, natural_render_state);

    bool right_state_restored = false;
    bool right_renderer_camera_match = false;
    CameraProbeVector right_applied_position{};
    CameraProbeFrustum right_applied_frustum{};
    if (left_captured && left_state_restored && RenderOwnerStillUsesView(owner, view)) {
        ConfigureStereoEyeOverride(
            camera, render_head_pose, sample.eyes[1], frame_sequence, sample.hmd_pose.sequence);
        right_full_view_pass = ReplayFullRenderViewForStereoEye(
            original, owner, view, right_view_guard_restored);
        ObserveAppliedArmRenderState(
            true, g_left_arm_rotation, frame_sequence, "right_eye_complete");
        ObserveAppliedArmRenderState(
            false, g_right_arm_rotation, frame_sequence, "right_eye_complete");
        right_rendered = right_full_view_pass &&
            g_stereo_eye_override.camera_applied &&
            g_stereo_eye_override.projection_applied;
        right_renderer_camera_match = g_stereo_eye_override.renderer_camera_match;
        right_applied_position = g_stereo_eye_override.applied_position;
        right_applied_frustum = g_stereo_eye_override.applied_frustum;
        if (right_rendered) {
            right_captured = g_stereo_callbacks.capture_eye(
                g_stereo_callbacks.context, cojvr::runtime::Eye::right,
                frame_sequence, &right_hash);
        }
        right_state_restored = RestoreCameraRenderState(camera, natural_render_state);
    }

    // The element overlay is relative to the live animated render hierarchy.
    // Keep it only for the two eye draws, then undo child before parent so the
    // next game frame starts from the engine's natural animation pose.
    RestoreAppliedArmRotations(frame_sequence);

    if (left_captured && right_captured && left_state_restored && right_state_restored) {
        submitted = g_stereo_callbacks.submit_frame(
            g_stereo_callbacks.context, frame_sequence, sample.hmd_pose);
    }

    g_stereo_eye_override = {};
    if (ShouldLogStereoFrame(frame_sequence) || !submitted) {
        try {
            const auto& left_eye = sample.eyes[0];
            const auto& right_eye = sample.eyes[1];
            std::ostringstream detail;
            detail << "frame_sequence=" << frame_sequence
                   << ";pose_sequence=" << sample.hmd_pose.sequence
                   << ";camera=0x" << std::hex << reinterpret_cast<std::uintptr_t>(camera)
                   << ";view=0x" << reinterpret_cast<std::uintptr_t>(view)
                   << std::dec
                   << ";left_camera_applied=" << (left_camera_applied ? "true" : "false")
                   << ";left_projection_applied=" << (left_projection_applied ? "true" : "false")
                   << ";left_captured=" << (left_captured ? "true" : "false")
                   << ";left_state_restored=" << (left_state_restored ? "true" : "false")
                   << ";right_rendered=" << (right_rendered ? "true" : "false")
                   << ";right_full_view_pass=" << (right_full_view_pass ? "true" : "false")
                   << ";right_view_guard_restored="
                   << (right_view_guard_restored ? "true" : "false")
                   << ";right_captured=" << (right_captured ? "true" : "false")
                   << ";right_state_restored=" << (right_state_restored ? "true" : "false")
                   << ";submitted=" << (submitted ? "true" : "false")
                   << ";transport_accepted=" << (submitted ? "true" : "false")
                   << ";content_hash_deferred="
                   << ((left_hash == 0 && right_hash == 0) ? "true" : "false")
                   << ";left_renderer_camera_match="
                   << (left_renderer_camera_match ? "true" : "false")
                   << ";right_renderer_camera_match="
                   << (right_renderer_camera_match ? "true" : "false")
                   << ";left_hash=" << left_hash
                   << ";right_hash=" << right_hash
                   << ";distinct_eye_content="
                   << ((left_hash != 0 && right_hash != 0 && left_hash != right_hash) ? "true" : "false")
                   << ";left_eye_x=" << left_eye.eye_to_head.position.x
                   << ";right_eye_x=" << right_eye.eye_to_head.position.x
                   << ";left_eye_position=" << RuntimeVectorText(left_eye.eye_to_head.position)
                   << ";right_eye_position=" << RuntimeVectorText(right_eye.eye_to_head.position)
                   << ";relative_head_position="
                   << RuntimeVectorText(relative_head_pose.position)
                   << ";render_head_position="
                   << RuntimeVectorText(render_head_pose.position)
                   << ";head_position_valid="
                   << (relative_head_pose.position_valid ? "true" : "false")
                   << ";positional_6dof="
                   << (body_position_update.positional_6dof ? "true" : "false")
                   << ";translation_mode=" << body_position_update.translation_mode
                   << ";game_units_per_meter=" << kGameUnitsPerMeter
                   << ";left_applied_position=" << VectorText(left_applied_position)
                   << ";right_applied_position=" << VectorText(right_applied_position)
                   << ";left_fov=" << left_eye.fov.angle_left << ',' << left_eye.fov.angle_right
                   << ',' << left_eye.fov.angle_down << ',' << left_eye.fov.angle_up
                   << ";right_fov=" << right_eye.fov.angle_left << ',' << right_eye.fov.angle_right
                   << ',' << right_eye.fov.angle_down << ',' << right_eye.fov.angle_up
                   << ";left_frustum=" << left_applied_frustum.left << ','
                   << left_applied_frustum.right << ',' << left_applied_frustum.bottom << ','
                   << left_applied_frustum.top << ',' << left_applied_frustum.near_plane << ','
                   << left_applied_frustum.far_plane
                   << ";right_frustum=" << right_applied_frustum.left << ','
                   << right_applied_frustum.right << ',' << right_applied_frustum.bottom << ','
                   << right_applied_frustum.top << ',' << right_applied_frustum.near_plane << ','
                   << right_applied_frustum.far_plane
                   << ";render_view_rva=0x" << std::hex << kRenderViewRva
                   << ";render_core_rva=0x" << kRenderViewCoreRva;
            EmitEvent("camera_native_stereo_frame", submitted ? "ok" : "incomplete", detail.str());
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
    const bool stereo_projection = render_update && g_stereo_eye_override.active &&
        g_stereo_eye_override.camera == camera && g_stereo_eye_override.eye != nullptr &&
        g_projection_builder != nullptr;
    const bool diagnostic_fov = render_update && !snapshot.command.tracking_enabled &&
        snapshot.command.enabled && snapshot.command.override_fov;
    const float applied_radians = diagnostic_fov
        ? snapshot.command.fov_degrees * kDegreesToRadians
        : fov_radians;
    original(
        camera, applied_radians, projection_arg_1, projection_arg_2, projection_flags);

    if (stereo_projection) {
        auto* bytes = static_cast<std::byte*>(camera);
        auto* left = reinterpret_cast<float*>(bytes + kCameraFrustumLeftOffset);
        auto* right = reinterpret_cast<float*>(bytes + kCameraFrustumRightOffset);
        auto* bottom = reinterpret_cast<float*>(bytes + kCameraFrustumBottomOffset);
        auto* top = reinterpret_cast<float*>(bytes + kCameraFrustumTopOffset);
        auto* near_plane = reinterpret_cast<float*>(bytes + kCameraNearOffset);
        auto* far_plane = reinterpret_cast<float*>(bytes + kCameraFarOffset);
        CameraProbeFrustum frustum{};
        if (IsWritable(left, sizeof(float)) && IsWritable(right, sizeof(float)) &&
            IsWritable(bottom, sizeof(float)) && IsWritable(top, sizeof(float)) &&
            IsReadable(near_plane, sizeof(float)) && IsReadable(far_plane, sizeof(float)) &&
            BuildCameraProbeFrustum(
                g_stereo_eye_override.eye->fov, *near_plane, *far_plane, frustum)) {
            *left = frustum.left;
            *right = frustum.right;
            *bottom = frustum.bottom;
            *top = frustum.top;
            g_projection_builder(camera);
            g_stereo_eye_override.projection_applied = true;
            g_stereo_eye_override.applied_frustum = frustum;
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
    bool body_ik_present = false;
    if (!ReadBool(text, "bodyIkEnabled", parsed.body_ik_enabled, body_ik_present)) {
        SetError(error, "bodyIkEnabled must be true or false");
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
    (void)body_ik_present;
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
    return {forward, up, right};
}

CameraProbeBasis ApplyCameraPoseOrientation(
    CameraProbeVector forward,
    CameraProbeVector up,
    const cojvr::runtime::Quaternion relative_orientation) noexcept {
    const RelativeAngles physical = HeadAngles(relative_orientation);

    // HeadAngles is positive when physical -Z forward turns toward tracking-space +X
    // (the user's right). Exact-build disassembly still requires the native source basis
    // at camera+0x44 to be RIGHT/UP/FORWARD with right = up x forward. However, live run
    // 20260916T104036Z-24b3e3010d4c proved that feeding that physical yaw with the same
    // sign into the paired CoJ world/view source rotates the visible camera horizontally
    // in the opposite direction, while pitch, handedness and scene visibility remain
    // correct. Negate only the HMD yaw at this game-specific adapter. Pitch keeps the
    // live-observed sign. Tracking -Z maps to native +Z, which is a reflection, so the
    // physical roll sign is inverted when it is applied around the native forward axis.
    // Keeping roll in the rendered basis also keeps the image orientation consistent
    // with the full HMD render pose submitted for OpenVR reprojection.
    CameraProbeBasis applied = ApplyCameraProbeOrientation(
        forward, up, -physical.yaw_degrees, physical.pitch_degrees);
    const float roll = -physical.roll_degrees * kDegreesToRadians;
    applied.up = Normalize(RotateAroundAxis(applied.up, applied.forward, roll), applied.up);
    applied.right = Normalize(Cross(applied.up, applied.forward), applied.right);
    applied.up = Normalize(Cross(applied.forward, applied.right), applied.up);
    return applied;
}

float CameraProbeBasisDeterminant(const CameraProbeBasis basis) noexcept {
    return Dot(basis.right, Cross(basis.up, basis.forward));
}

bool IsCameraProbeBasisRigidRightHanded(const CameraProbeBasis basis) noexcept {
    constexpr float kUnitTolerance = 0.01F;
    constexpr float kOrthogonalTolerance = 0.01F;
    constexpr float kDeterminantTolerance = 0.02F;
    const float right_length = Length(basis.right);
    const float up_length = Length(basis.up);
    const float forward_length = Length(basis.forward);
    const float determinant = CameraProbeBasisDeterminant(basis);
    return std::isfinite(right_length) && std::isfinite(up_length) &&
        std::isfinite(forward_length) && std::isfinite(determinant) &&
        std::fabs(right_length - 1.0F) <= kUnitTolerance &&
        std::fabs(up_length - 1.0F) <= kUnitTolerance &&
        std::fabs(forward_length - 1.0F) <= kUnitTolerance &&
        std::fabs(Dot(basis.right, basis.up)) <= kOrthogonalTolerance &&
        std::fabs(Dot(basis.right, basis.forward)) <= kOrthogonalTolerance &&
        std::fabs(Dot(basis.up, basis.forward)) <= kOrthogonalTolerance &&
        std::fabs(determinant - 1.0F) <= kDeterminantTolerance;
}

bool BuildCameraProbeFrustum(
    const cojvr::runtime::EyeFov fov,
    const float near_plane,
    const float far_plane,
    CameraProbeFrustum& frustum) noexcept {
    frustum = {};
    constexpr float kMaxAngle = 1.55334306F; // 89 degrees; rejects tan singularities.
    if (!std::isfinite(fov.angle_left) || !std::isfinite(fov.angle_right) ||
        !std::isfinite(fov.angle_up) || !std::isfinite(fov.angle_down) ||
        !std::isfinite(near_plane) || !std::isfinite(far_plane) ||
        near_plane <= 0.0F || far_plane <= near_plane ||
        std::fabs(fov.angle_left) >= kMaxAngle ||
        std::fabs(fov.angle_right) >= kMaxAngle ||
        std::fabs(fov.angle_up) >= kMaxAngle ||
        std::fabs(fov.angle_down) >= kMaxAngle) {
        return false;
    }

    CameraProbeFrustum candidate{
        std::tan(fov.angle_left) * near_plane,
        std::tan(fov.angle_right) * near_plane,
        std::tan(fov.angle_down) * near_plane,
        std::tan(fov.angle_up) * near_plane,
        near_plane,
        far_plane,
    };
    if (!std::isfinite(candidate.left) || !std::isfinite(candidate.right) ||
        !std::isfinite(candidate.bottom) || !std::isfinite(candidate.top) ||
        candidate.left >= candidate.right || candidate.bottom >= candidate.top) {
        return false;
    }
    frustum = candidate;
    return true;
}

CameraProbeVector ApplyCameraEyeOffset(
    const CameraProbeVector head_position,
    const CameraProbeBasis head_basis,
    const cojvr::runtime::Vec3 eye_to_head_position) noexcept {
    // Neutral XR eye coordinates are +X right, +Y up and -Z forward. CoJ's
    // exact source basis stores +forward. The runtime position is in metres,
    // while Call of Juarez gameplay/world coordinates are centimetres.
    const cojvr::runtime::Vec3 game_offset{
        eye_to_head_position.x * kGameUnitsPerMeter,
        eye_to_head_position.y * kGameUnitsPerMeter,
        eye_to_head_position.z * kGameUnitsPerMeter,
    };
    return {
        head_position.x + head_basis.right.x * game_offset.x +
            head_basis.up.x * game_offset.y - head_basis.forward.x * game_offset.z,
        head_position.y + head_basis.right.y * game_offset.x +
            head_basis.up.y * game_offset.y - head_basis.forward.y * game_offset.z,
        head_position.z + head_basis.right.z * game_offset.x +
            head_basis.up.z * game_offset.y - head_basis.forward.z * game_offset.z,
    };
}

CameraProbeVector ApplyCameraTrackingOffset(
    const CameraProbeVector camera_position,
    const CameraProbeBasis recenter_basis,
    const cojvr::runtime::Vec3 relative_head_position) noexcept {
    // Relative tracking positions use the same neutral +X right, +Y up,
    // -Z forward convention as eye-to-head offsets, but remain anchored to
    // the natural/recentered game-camera basis rather than rotating with the
    // current head orientation.
    return ApplyCameraEyeOffset(camera_position, recenter_basis, relative_head_position);
}

cojvr::runtime::Pose SuppressPhysicalTrackingTranslation(
    cojvr::runtime::Pose pose) noexcept {
    if (pose.position_valid) {
        pose.position = {};
    }
    return pose;
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
    cojvr::runtime::PoseSource* pose_source,
    const CameraStereoRuntimeCallbacks stereo_callbacks) noexcept {
    g_event_callback = callback;
    g_pose_source = pose_source;
    g_stereo_callbacks = stereo_callbacks;
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
        std::vector<wchar_t> engine_path_buffer(32768);
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
        g_invert_matrix = reinterpret_cast<InvertMatrixFn>(g_engine_base + kInvertMatrixRva);
        g_projection_builder = reinterpret_cast<ProjectionBuilderFn>(
            g_engine_base + kProjectionBuilderRva);
        const auto expected_render_update = reinterpret_cast<void*>(g_engine_base + kRenderCameraUpdateRva);
        const auto expected_set_fov = reinterpret_cast<void*>(g_engine_base + kSetFovRva);
        if (!IsReadable(g_camera_vtable, sizeof(void*) * (kSetFovSlot + 1)) ||
            g_camera_vtable[kRenderCameraUpdateSlot] != expected_render_update ||
            g_camera_vtable[kSetFovSlot] != expected_set_fov) {
            EmitEvent(
                "camera_probe_install", "profile_mismatch",
                "CBaseCamera vtable does not match inspected RVAs");
            g_camera_vtable = nullptr;
            g_invert_matrix = nullptr;
            g_projection_builder = nullptr;
            g_engine_base = nullptr;
            return CameraProbeInstallStatus::profile_mismatch;
        }

        if (StereoCallbacksReady()) {
            g_render_view_entry = reinterpret_cast<void**>(g_engine_base + kRenderViewEntryRva);
            const auto expected_render_view = reinterpret_cast<void*>(g_engine_base + kRenderViewRva);
            if (!IsReadable(g_render_view_entry, sizeof(void*)) ||
                *g_render_view_entry != expected_render_view) {
                EmitEvent(
                    "camera_probe_install", "profile_mismatch",
                    "ChromeEngine render-view vtable entry does not match inspected RVA");
                g_render_view_entry = nullptr;
                g_projection_builder = nullptr;
                g_camera_vtable = nullptr;
                g_invert_matrix = nullptr;
                g_engine_base = nullptr;
                return CameraProbeInstallStatus::profile_mismatch;
            }
            g_original_render_view = reinterpret_cast<RenderViewFn>(*g_render_view_entry);
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
        if (g_render_view_entry) {
            const std::array view_requests{
                cojvr::backends::d3d9::HookSlotRequest{
                    0, reinterpret_cast<void*>(&HookRenderView)},
            };
            const auto view_outcome = g_render_view_hooks.Install(
                g_render_view_entry, view_requests);
            if (view_outcome.result != cojvr::backends::d3d9::HookRegistryResult::Installed &&
                view_outcome.result != cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled) {
                (void)g_camera_hooks.Restore(g_camera_vtable);
                EmitEvent(
                    "camera_probe_install", "hook_failed",
                    "render_view_modified_slots=" +
                        std::to_string(view_outcome.modified_slots));
                return CameraProbeInstallStatus::hook_failed;
            }
        }
        g_installed.store(true, std::memory_order_release);
        std::ostringstream detail;
        detail << "engine_sha256=" << *engine_hash
               << ";vtable_rva=0x" << std::hex << kBaseCameraVtableRva
               << ";render_update_rva=0x" << kRenderCameraUpdateRva
               << ";set_fov_rva=0x" << kSetFovRva
               << ";render_view_rva=0x" << kRenderViewRva
               << ";render_view_core_rva=0x" << kRenderViewCoreRva
               << ";control_path=" << control_path.string()
               << ";pose_source=" << (g_pose_source ? "available" : "none")
               << ";native_stereo=" << (g_render_view_entry ? "available" : "disabled");
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
    if (g_render_view_entry) {
        const auto view_slots = g_render_view_hooks.Inspect(g_render_view_entry);
        state.render_view_owned = view_slots.size() == 1 && view_slots.front().owned;
    }
    return state;
}

void ShutdownCameraProbe() noexcept {
    try {
        bool view_restored = true;
        std::size_t view_restored_slots = 0;
        if (g_render_view_entry) {
            const auto view_outcome = g_render_view_hooks.Restore(g_render_view_entry);
            view_restored =
                view_outcome.result == cojvr::backends::d3d9::HookRegistryResult::Installed ||
                view_outcome.result == cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled;
            view_restored_slots = view_outcome.modified_slots;
        }
        if (g_camera_vtable) {
            const auto outcome = g_camera_hooks.Restore(g_camera_vtable);
            const bool camera_restored =
                outcome.result == cojvr::backends::d3d9::HookRegistryResult::Installed ||
                outcome.result == cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled;
            EmitEvent(
                "camera_probe_restore",
                camera_restored && view_restored ? "restored" : "incomplete",
                "camera_restored_slots=" + std::to_string(outcome.modified_slots) +
                    ";view_restored_slots=" + std::to_string(view_restored_slots));
        }
    } catch (...) {
        EmitEvent("camera_probe_restore", "exception", "restore_exception");
    }
    g_installed.store(false, std::memory_order_release);
    g_initialized.store(false, std::memory_order_release);
    g_camera_vtable = nullptr;
    g_render_view_entry = nullptr;
    g_engine_base = nullptr;
    g_original_render_camera_update = nullptr;
    g_invert_matrix = nullptr;
    g_projection_builder = nullptr;
    g_original_render_view = nullptr;
    g_original_set_fov = nullptr;
    g_render_update_override = {};
    g_stereo_eye_override = {};
    g_pose_tracker.SetEnabled(false);
    g_java_player_bridge.Reset();
    g_body_applied_world_offset = {};
    g_body_applied_tracking_offset = {};
    g_body_player_generation = 0;
    g_body_player_space_applied = false;
    g_arm_element_cache = {};
    g_left_hand_orientation = {};
    g_right_hand_orientation = {};
    g_left_arm_rotation = {};
    g_right_arm_rotation = {};
    g_arm_rotation_faulted = false;
    g_last_gameplay_telemetry = {};
    g_gameplay_telemetry_initialized = false;
    g_pose_source = nullptr;
    g_stereo_callbacks = {};
    g_recenter_command_generation.store(0, std::memory_order_release);
    g_last_hmd_logged_sequence.store(0, std::memory_order_release);
    g_hmd_logged_count.store(0, std::memory_order_release);
    g_meaningful_roll_logged.store(false, std::memory_order_release);
    g_stereo_frame_sequence.store(0, std::memory_order_release);
    g_stereo_logged_count.store(0, std::memory_order_release);
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
