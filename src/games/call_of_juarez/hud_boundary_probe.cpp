#include "games/call_of_juarez/hud_boundary_probe.hpp"
#include <atomic>

namespace cojvr::games::call_of_juarez {
namespace {
using SpriteFn = void(__thiscall*)(void*);
using FlushFn = void(__thiscall*)(void*, std::uint32_t);
backends::d3d9::VtablePatch sprites_patch, flush_patch;
CoJHudBoundaryHookTargets active_targets{};
SpriteFn original_sprites = nullptr;
FlushFn original_flush = nullptr;
CoJHudBoundaryObserver observer = nullptr;
void* observer_context = nullptr;
std::atomic_bool installed{false};
std::atomic_uint64_t sequence{0};
thread_local std::uint64_t pending_sequence = 0;
thread_local unsigned sprite_depth = 0;

void Observe(CoJHudBoundaryStage stage, void* owner, std::uint32_t option = 0) noexcept {
    if (!installed.load(std::memory_order_acquire) || !observer || pending_sequence == 0 ||
        (pending_sequence > 8 && pending_sequence % 127 >= 8)) return;
    observer(observer_context, {stage, pending_sequence, owner, option});
}
void __fastcall HookSprites(void* owner, void*) {
    const auto outer_sequence = pending_sequence;
    pending_sequence = sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    ++sprite_depth;
    Observe(CoJHudBoundaryStage::sprites_begin, owner);
    original_sprites(owner); // exactly once; native traversal is not pure
    Observe(CoJHudBoundaryStage::sprites_end, owner);
    --sprite_depth;
    if (sprite_depth != 0) pending_sequence = outer_sequence;
}
void __fastcall HookFlush(void* owner, void*, const std::uint32_t option) {
    Observe(CoJHudBoundaryStage::flush_begin, owner, option);
    original_flush(owner, option); // preserve the native stack word
    Observe(CoJHudBoundaryStage::flush_end, owner, option);
    if (sprite_depth == 0) pending_sequence = 0;
}
bool Success(const backends::d3d9::VtablePatchOutcome& result) {
    using backends::d3d9::VtablePatchResult;
    return (result.result == VtablePatchResult::Applied ||
        result.result == VtablePatchResult::NoModification) && result.protection_restored;
}
}

bool InstallCoJHudBoundaryProbe(const CoJHudBoundaryHookTargets& targets,
    const bool exact_build_validated, CoJHudBoundaryObserver callback, void* context,
    const backends::d3d9::VtableMemoryOperations operations) noexcept {
    if (!exact_build_validated || !callback || !targets.sprites_entry || !targets.flush_entry ||
        !targets.expected_sprites || !targets.expected_flush) return false;
    if (installed.load(std::memory_order_acquire))
        return targets.sprites_entry == active_targets.sprites_entry &&
            targets.flush_entry == active_targets.flush_entry &&
            *targets.sprites_entry == reinterpret_cast<void*>(&HookSprites) &&
            *targets.flush_entry == reinterpret_cast<void*>(&HookFlush);
    if (active_targets.sprites_entry || active_targets.flush_entry) return false;
    if (*targets.sprites_entry != targets.expected_sprites ||
        *targets.flush_entry != targets.expected_flush) return false;
    // Foreign wrappers can retain our forwarding entry after restoration.
    // Never change the original call targets underneath those wrappers.
    if ((original_sprites && reinterpret_cast<void*>(original_sprites) != targets.expected_sprites) ||
        (original_flush && reinterpret_cast<void*>(original_flush) != targets.expected_flush)) return false;
    original_sprites = reinterpret_cast<SpriteFn>(targets.expected_sprites);
    original_flush = reinterpret_cast<FlushFn>(targets.expected_flush);
    observer = callback;
    observer_context = context;
    sprites_patch = backends::d3d9::VtablePatch(operations);
    flush_patch = backends::d3d9::VtablePatch(operations);
    active_targets = targets;
    // The native target is the expected pointer of the actual CAS, not just
    // a preliminary check. Concurrent foreign installation must remain intact.
    if (!Success(sprites_patch.Install(targets.sprites_entry, targets.expected_sprites,
            reinterpret_cast<void*>(&HookSprites)))) {
        (void)RestoreCoJHudBoundaryProbe();
        return false;
    }
    if (!Success(flush_patch.Install(targets.flush_entry, targets.expected_flush,
            reinterpret_cast<void*>(&HookFlush)))) {
        (void)RestoreCoJHudBoundaryProbe();
        return false;
    }
    sequence.store(0, std::memory_order_release);
    pending_sequence = 0;
    installed.store(true, std::memory_order_release);
    return true;
}

bool RestoreCoJHudBoundaryProbe() noexcept {
    installed.store(false, std::memory_order_release);
    if (!active_targets.sprites_entry && !active_targets.flush_entry) return true;
    const bool sprite = Success(sprites_patch.Restore()) &&
        *active_targets.sprites_entry == active_targets.expected_sprites;
    const bool flush = Success(flush_patch.Restore()) &&
        *active_targets.flush_entry == active_targets.expected_flush;
    if (sprite && flush) active_targets = {};
    // Keep original forwarding functions for any foreign wrapper retaining us.
    return sprite && flush;
}
const char* CoJHudBoundaryStageName(const CoJHudBoundaryStage stage) noexcept {
    switch (stage) {
    case CoJHudBoundaryStage::sprites_begin: return "sprites_begin";
    case CoJHudBoundaryStage::sprites_end: return "sprites_end";
    case CoJHudBoundaryStage::flush_begin: return "flush_begin";
    case CoJHudBoundaryStage::flush_end: return "flush_end";
    }
    return "unknown";
}
}
