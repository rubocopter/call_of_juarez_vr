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
#include <cstring>
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
std::atomic_uint64_t g_stereo_frame_sequence{0};
std::atomic_uint64_t g_stereo_logged_count{0};
std::atomic_bool g_initialized{false};
std::atomic_bool g_installed{false};

struct RenderUpdateOverride {
    void* camera = nullptr;
    bool orientation_applied = false;
};

thread_local RenderUpdateOverride g_render_update_override{};

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
    const CameraProbeVector applied_position = stereo_active
        ? ApplyCameraEyeOffset(
            natural_position, applied, g_stereo_eye_override.eye->pose.position)
        : natural_position;
    if (stereo_active &&
        (!g_stereo_eye_override.eye->pose.position_valid ||
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
        original(owner, view);
        return;
    }

    g_pose_tracker.SetEnabled(true);
    if (sample.recenter_requested) {
        g_pose_tracker.RequestRecenter();
        EmitEvent(
            "camera_hmd_recenter_requested", "ok",
            "source=openvr_global_action;pose_sequence=" +
                std::to_string(sample.hmd_pose.sequence));
    }
    if (snapshot.command.recenter &&
        FirstForGeneration(g_recenter_command_generation, snapshot.generation)) {
        g_pose_tracker.RequestRecenter();
    }
    const std::uint64_t prior_recenter = g_pose_tracker.last_recenter_sequence();
    cojvr::runtime::Pose relative_head_pose{};
    if (!g_pose_tracker.Update(sample.hmd_pose) ||
        !g_pose_tracker.CurrentPose(relative_head_pose)) {
        original(owner, view);
        return;
    }
    if (g_pose_tracker.last_recenter_sequence() != 0 &&
        g_pose_tracker.last_recenter_sequence() != prior_recenter) {
        EmitEvent(
            "camera_hmd_recentered", "ok",
            "generation=" + std::to_string(snapshot.generation) +
                ";pose_sequence=" + std::to_string(sample.hmd_pose.sequence) +
                ";stereo=true");
    }

    const std::uint64_t frame_sequence =
        g_stereo_frame_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
    std::uint64_t left_hash = 0;
    std::uint64_t right_hash = 0;
    bool left_captured = false;
    bool right_captured = false;
    bool submitted = false;
    bool right_rendered = false;
    bool right_full_view_pass = false;
    bool right_view_guard_restored = false;

    ConfigureStereoEyeOverride(
        camera, relative_head_pose, sample.eyes[0], frame_sequence, sample.hmd_pose.sequence);
    original(owner, view);
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
            camera, relative_head_pose, sample.eyes[1], frame_sequence, sample.hmd_pose.sequence);
        right_full_view_pass = ReplayFullRenderViewForStereoEye(
            original, owner, view, right_view_guard_restored);
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

    if (left_captured && right_captured && left_state_restored && right_state_restored) {
        submitted = g_stereo_callbacks.submit_frame(
            g_stereo_callbacks.context, frame_sequence);
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
                   << ";left_renderer_camera_match="
                   << (left_renderer_camera_match ? "true" : "false")
                   << ";right_renderer_camera_match="
                   << (right_renderer_camera_match ? "true" : "false")
                   << ";left_hash=" << left_hash
                   << ";right_hash=" << right_hash
                   << ";distinct_eye_content="
                   << ((left_hash != 0 && right_hash != 0 && left_hash != right_hash) ? "true" : "false")
                   << ";left_eye_x=" << left_eye.pose.position.x
                   << ";right_eye_x=" << right_eye.pose.position.x
                   << ";left_eye_position=" << RuntimeVectorText(left_eye.pose.position)
                   << ";right_eye_position=" << RuntimeVectorText(right_eye.pose.position)
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
    // live-observed sign and roll remains excluded from this gate.
    return ApplyCameraProbeOrientation(
        forward, up, -physical.yaw_degrees, physical.pitch_degrees);
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
    g_pose_source = nullptr;
    g_stereo_callbacks = {};
    g_recenter_command_generation.store(0, std::memory_order_release);
    g_last_hmd_logged_sequence.store(0, std::memory_order_release);
    g_hmd_logged_count.store(0, std::memory_order_release);
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
