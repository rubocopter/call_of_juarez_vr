#pragma once
#include <cstdint>
#include "backends/d3d9/vtable_patch.hpp"

namespace cojvr::games::call_of_juarez {
enum class CoJHudBoundaryStage { sprites_begin, sprites_end, flush_begin, flush_end };
struct CoJHudBoundaryEvent {
    CoJHudBoundaryStage stage{};
    std::uint64_t pass_sequence = 0;
    void* owner = nullptr; // borrowed only for this synchronous callback
    std::uint32_t option = 0;
    std::uint32_t sprite_depth = 0, flush_depth = 0;
    std::uint64_t hook_epoch = 0;
    bool hook_intact = false, interval_faulted = false;
    // Control callbacks are always delivered. Preserve the old selection ONLY
    // for diagnostic formatting/logging; never sample away nesting/closure.
    bool sample_selected = false;
};
struct CoJHudHookContinuity { std::uint64_t epoch = 0; bool intact = false; };
[[nodiscard]] CoJHudHookContinuity InspectCoJHudHookContinuity() noexcept;
struct CoJHudDrawScope {
    std::uint64_t pass_sequence = 0;
    std::uintptr_t sprite_owner = 0, flush_owner = 0;
    std::uint32_t option = 0, sprite_depth = 0, flush_depth = 0;
    std::uint64_t hook_epoch = 0;
    bool hook_intact = false, interval_faulted = true;
};
// Synchronous TLS + current hook inspection. No retained owner or caller guess.
[[nodiscard]] CoJHudDrawScope InspectCoJHudDrawScope() noexcept;
struct CoJHudDiagnosticPermission {
    bool available = false, enabled = false;
    std::uint64_t source_epoch = 0, frame = 0;
};
using CoJHudPermissionProvider = CoJHudDiagnosticPermission(*)(void*) noexcept;
// Cached command-owner state only. Absent/contended provider denies; no poll,
// file IO, command refresh, formatting, or general observer pipeline.
[[nodiscard]] CoJHudDiagnosticPermission ReadCoJHudDiagnosticPermission() noexcept;
struct CoJHudBoundaryHookTargets {
    void** sprites_entry = nullptr;
    void* expected_sprites = nullptr;
    void** flush_entry = nullptr;
    void* expected_flush = nullptr;
};
using CoJHudBoundaryObserver = void(*)(void*, const CoJHudBoundaryEvent&) noexcept;
[[nodiscard]] bool InstallCoJHudBoundaryProbe(
    const CoJHudBoundaryHookTargets& targets, bool exact_build_validated,
    CoJHudBoundaryObserver observer, void* context,
    backends::d3d9::VtableMemoryOperations operations =
        backends::d3d9::VtableMemoryOperations::Native(),
    CoJHudPermissionProvider permission_provider = nullptr, void* permission_context = nullptr) noexcept;
[[nodiscard]] bool RestoreCoJHudBoundaryProbe() noexcept;
[[nodiscard]] const char* CoJHudBoundaryStageName(CoJHudBoundaryStage stage) noexcept;
}
