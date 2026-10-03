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
};
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
        backends::d3d9::VtableMemoryOperations::Native()) noexcept;
[[nodiscard]] bool RestoreCoJHudBoundaryProbe() noexcept;
[[nodiscard]] const char* CoJHudBoundaryStageName(CoJHudBoundaryStage stage) noexcept;
}
