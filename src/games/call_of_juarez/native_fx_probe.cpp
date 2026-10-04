#include "games/call_of_juarez/native_fx_probe.hpp"

#include <limits>
#include <bit>
#include <cmath>
#include <utility>

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
constexpr std::uint32_t EngineImageSize = 0x60C000;
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

void ReadEmitter(const CoJNativeFxRequest& request, CoJNativeFxReadOperations operations,
    CoJNativeFxSnapshot& result) noexcept {
    if (!request.owner_id) return;
    auto& reason=result.lookup_reason;
    std::array<std::pair<std::uintptr_t,std::uint32_t>,12> identities{};
    std::size_t identity_count=0;
    const auto word=[&](std::uint32_t base, std::uint32_t offset, std::uint32_t& value,
                        bool identity=false) {
        const auto address=static_cast<std::uint64_t>(base)+offset;
        if (!base || address>MaximumAddress-3u) {
            reason=CoJNativeFxReadReason::read_failed; return false;
        }
        reason=ReadWord(operations,static_cast<std::uintptr_t>(address),value);
        if (reason!=CoJNativeFxReadReason::observed) return false;
        if (identity) identities[identity_count++]={static_cast<std::uintptr_t>(address),value};
        return true;
    };
    const auto in_image=[&](std::uint32_t value) {
        // PE SizeOfImage for the recognized ChromeEngine3 build.
        return value>=request.engine_base && value-request.engine_base<EngineImageSize;
    };
    // GetThisID handle -> binding -> native GameObject is also demonstrated by
    // GetMousePos. FXIsEnabled (0x10F120) then uses owner+24 -> module+370.
    std::uint32_t binding=0,owner=0,module=0,manager=0,table=0,capacity=0,vt=0;
    if (!word(request.owner_id,4,binding,true) || !word(binding,4,owner,true) ||
        !word(owner,0,vt,true)) return;
    if (!in_image(vt)) { reason=CoJNativeFxReadReason::invalid_owner; return; }
    if (!word(owner,0x24,module,true) || !word(module,0,vt,true)) return;
    if (!in_image(vt)) { reason=CoJNativeFxReadReason::invalid_owner; return; }
    if (!word(module,0x370,manager,true) || !word(manager,0x18,table,true) ||
        !word(manager,0x20,capacity,true)) return;
    // Native grow 0xFCA80 allocates capacity*16; manager+1C is active count.
    const auto index=request.handle&0xFFFFu;
    if (!table || !capacity || capacity>65536 || index>=capacity ||
        static_cast<std::uint64_t>(table)+static_cast<std::uint64_t>(capacity)*16-1>MaximumAddress) {
        reason=CoJNativeFxReadReason::invalid_slot; return;
    }
    const std::uint32_t slot=table+index*16;
    std::uint32_t generation=0,emitter=0;
    if (!word(slot,0,generation,true) || !word(slot,4,emitter,true)) return;
    if (!emitter || !request.handle || generation!=(request.handle&0xFFFF0000u)) {
        reason=CoJNativeFxReadReason::stale_handle; return;
    }
    if (!word(emitter,0,vt,true)) return;
    if (vt!=request.engine_base+0x2FDE7C && vt!=request.engine_base+0x2FDEB8) {
        reason=CoJNativeFxReadReason::unknown_emitter_type; return;
    }
    std::uint32_t emitter_manager=0,emitter_handle=0,flags=0;
    if (!word(emitter,4,emitter_manager) || !word(emitter,0x58,emitter_handle)) return;
    if (emitter_manager!=manager || emitter_handle!=request.handle) {
        reason=CoJNativeFxReadReason::invalid_owner; return;
    }
    CoJNativeFxEmitter values{};
    values.manager_address=manager; values.emitter_address=emitter;
    values.vtable_rva=vt-static_cast<std::uint32_t>(request.engine_base);
    const auto scalar=[&](std::uint32_t base,std::uint32_t offset,float& destination) {
        std::uint32_t bits=0;
        if (!word(base,offset,bits)) return false;
        destination=std::bit_cast<float>(bits);
        if (!std::isfinite(destination)) { reason=CoJNativeFxReadReason::invalid_values; return false; }
        return true;
    };
    if (!scalar(manager,4,values.clock) || !scalar(emitter,0xC,values.birth) ||
        !scalar(emitter,0x10,values.lifetime) || !scalar(emitter,0x14,values.time_scale) ||
        !word(emitter,0x5C,flags)) return;
    values.enabled=(flags&0xFF)!=0; values.render_enabled=(flags&0xFF00)!=0;
    for (std::uint32_t axis=0;axis<3;++axis)
        if (!scalar(emitter,0xE0+axis*4,values.position[axis]) ||
            !scalar(emitter,0xF0+axis*4,values.previous_position[axis])) return;
    if (!word(emitter,0x40C,values.live) || !word(emitter,0x410,values.emitted) ||
        !word(emitter,0x414,values.expired)) return;
    // Recheck all owner, manager, table, generation, pointer and subtype roots.
    // This detects replacement; it does not make a concurrent update atomic.
    for (std::size_t i=0;i<identity_count;++i) {
        std::uint32_t check=0;
        reason=ReadWord(operations,identities[i].first,check);
        if (reason!=CoJNativeFxReadReason::observed) return;
        if (check!=identities[i].second) { reason=CoJNativeFxReadReason::changed_during_read; return; }
    }
    result.emitter=values;
    reason=CoJNativeFxReadReason::observed;
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
    if (!request.engine_base || request.engine_base > MaximumAddress - EngineImageSize + 1u) {
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
    ReadEmitter(request, operations, result);
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
    case CoJNativeFxReadReason::invalid_owner: return "invalid_owner";
    case CoJNativeFxReadReason::invalid_slot: return "invalid_slot";
    case CoJNativeFxReadReason::stale_handle: return "stale_handle";
    case CoJNativeFxReadReason::unknown_emitter_type: return "unknown_emitter_type";
    case CoJNativeFxReadReason::invalid_values: return "invalid_values";
    case CoJNativeFxReadReason::missing_plane_extent_boundary: return "missing_plane_extent_boundary";
    }
    return "unknown";
}
} // namespace cojvr::games::call_of_juarez
