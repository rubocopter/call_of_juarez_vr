#include "games/call_of_juarez/native_fx_probe.hpp"

#include <limits>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace cojvr::games::call_of_juarez {
namespace {
// Exact ChromeEngine3 SHA-256 is documented on the request in the header.
// FXEnable(Z)V 0x22D90 writes global 0x37E6D0; update 0x28CB6 and
// render 0x30734 test it. simple Draw 0x11AD90 / rotated Draw 0x11B590
// load renderer from 0x590074 and camera from +0x1B0, fallback +0x1B8.
constexpr std::uint32_t GlobalFxRva = 0x37E6D0;
constexpr std::uint32_t RendererRva = 0x590074;
constexpr std::uint32_t CameraOffset = 0x1B0;
constexpr std::uint32_t FallbackCameraOffset = 0x1B8;
constexpr auto MaximumAddress = std::numeric_limits<std::uint32_t>::max();

std::size_t ReadLocalProcess(void*, std::uintptr_t address,
    void* destination, std::size_t size) noexcept {
#if defined(_WIN32)
    SIZE_T copied = 0;
    const BOOL success = ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<const void*>(address), destination, size, &copied);
    // An unsuccessful call may copy some bytes. Never treat it as a full read.
    return success ? static_cast<std::size_t>(copied) : 0;
#else
    (void)address; (void)destination; (void)size;
    return 0;
#endif
}

CoJNativeFxReadReason ReadWord(CoJNativeFxReadOperations operations,
    std::uintptr_t address, std::uint32_t& result) noexcept {
    if (!operations.read || !address || address > MaximumAddress - 3u)
        return CoJNativeFxReadReason::read_failed;
    std::uint32_t value = 0;
    const auto copied = operations.read(operations.context, address, &value, sizeof(value));
    if (!copied) return CoJNativeFxReadReason::read_failed;
    if (copied != sizeof(value)) return CoJNativeFxReadReason::partial_read;
    result = value;
    return CoJNativeFxReadReason::observed;
}

void ReadCamera(std::uintptr_t engine_base, CoJNativeFxReadOperations operations,
    CoJNativeFxSnapshot& result) noexcept {
    std::uint32_t renderer = 0, primary = 0, fallback = 0;
    result.camera_reason = ReadWord(operations, engine_base + RendererRva, renderer);
    if (result.camera_reason != CoJNativeFxReadReason::observed) return;
    if (renderer) {
        if (renderer > MaximumAddress - FallbackCameraOffset - 3u) {
            result.camera_reason = CoJNativeFxReadReason::read_failed;
            return;
        }
        result.camera_reason = ReadWord(operations, renderer + CameraOffset, primary);
        if (result.camera_reason != CoJNativeFxReadReason::observed) return;
        result.camera_reason = ReadWord(operations, renderer + FallbackCameraOffset, fallback);
        if (result.camera_reason != CoJNativeFxReadReason::observed) return;
    }
    // Do not publish mixed fields across renderer replacement or camera changes.
    std::uint32_t check_renderer = 0, check_primary = 0, check_fallback = 0;
    result.camera_reason = ReadWord(operations, engine_base + RendererRva, check_renderer);
    if (result.camera_reason != CoJNativeFxReadReason::observed) return;
    if (check_renderer != renderer) {
        result.camera_reason = CoJNativeFxReadReason::changed_during_read;
        return;
    }
    if (renderer) {
        result.camera_reason = ReadWord(operations, renderer + CameraOffset, check_primary);
        if (result.camera_reason != CoJNativeFxReadReason::observed) return;
        result.camera_reason = ReadWord(operations, renderer + FallbackCameraOffset, check_fallback);
        if (result.camera_reason != CoJNativeFxReadReason::observed) return;
        if (check_primary != primary || check_fallback != fallback) {
            result.camera_reason = CoJNativeFxReadReason::changed_during_read;
            return;
        }
        // Recheck root after reading the camera fields as well.
        result.camera_reason = ReadWord(operations, engine_base + RendererRva, check_renderer);
        if (result.camera_reason != CoJNativeFxReadReason::observed) return;
        if (check_renderer != renderer) {
            result.camera_reason = CoJNativeFxReadReason::changed_during_read;
            return;
        }
    }
    result.renderer_address = renderer;
    result.fx_camera_address = primary ? primary : fallback;
}
} // namespace

CoJNativeFxReadOperations CoJNativeFxReadOperations::Native() noexcept {
    return {ReadLocalProcess, nullptr};
}

CoJNativeFxSnapshot ReadCoJNativeFxSnapshot(const CoJNativeFxRequest& request,
    CoJNativeFxReadOperations operations) noexcept {
    CoJNativeFxSnapshot result;
    result.handle = request.handle;
    result.phase = request.phase;
    if (!request.exact_build_validated) {
        result.lookup_reason = result.global_reason = result.camera_reason =
            CoJNativeFxReadReason::unrecognized_engine;
        return result;
    }
    if (!request.engine_base || request.engine_base > MaximumAddress - RendererRva - 3u) {
        result.lookup_reason = result.global_reason = result.camera_reason =
            CoJNativeFxReadReason::invalid_engine_base;
        return result;
    }
    // FXSetStartXForm 0x10F250 and FXIsEnabled 0x10F120 first resolve the
    // GameObject ID via 0xBF870 (global wrapper 0x4A7020, virtual lookup +0x190).
    // They then follow resolved wrapper+4 -> owner+0x24 -> level+0x370(manager).
    // ONLY THEN: table=manager+0x18, slot=table+(handle&0xFFFF)*16,
    // slot.generation==(handle&0xFFFF0000), emitter=slot+4.
    // The table's capacity is at manager+0x20 (grow 0xFCA80: table+8), NOT
    // the active count at manager+0x1C. Handles are manager-local. Without an
    // owner ID or a proven owner/cache resolution, enginebase+handle cannot
    // select the correct manager; do not guess it from renderer/current level.
    result.lookup_reason = CoJNativeFxReadReason::missing_owner_lookup_boundary;
    std::uint32_t global = 0, check_global = 0;
    result.global_reason = ReadWord(operations, request.engine_base + GlobalFxRva, global);
    ReadCamera(request.engine_base, operations, result);
    if (result.global_reason == CoJNativeFxReadReason::observed) {
        result.global_reason = ReadWord(operations, request.engine_base + GlobalFxRva, check_global);
        if (result.global_reason == CoJNativeFxReadReason::observed) {
            if (global == check_global) result.global_fx = global;
            else result.global_reason = CoJNativeFxReadReason::changed_during_read;
        }
    }
    return result;
}

const char* CoJNativeFxReadReasonName(CoJNativeFxReadReason reason) noexcept {
    switch (reason) {
    case CoJNativeFxReadReason::observed: return "observed";
    case CoJNativeFxReadReason::unrecognized_engine: return "unrecognized_engine";
    case CoJNativeFxReadReason::invalid_engine_base: return "invalid_engine_base";
    case CoJNativeFxReadReason::read_failed: return "read_failed";
    case CoJNativeFxReadReason::partial_read: return "partial_read";
    case CoJNativeFxReadReason::changed_during_read: return "changed_during_read";
    case CoJNativeFxReadReason::missing_owner_lookup_boundary: return "missing_owner_lookup_boundary";
    case CoJNativeFxReadReason::missing_plane_extent_boundary: return "missing_plane_extent_boundary";
    }
    return "unknown";
}
} // namespace cojvr::games::call_of_juarez
