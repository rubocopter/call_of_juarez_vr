#include "games/call_of_juarez/hud_boundary_probe.hpp"
#include <atomic>
#include <mutex>
#include <windows.h>

namespace cojvr::games::call_of_juarez {
namespace {
using SpriteFn = void(__thiscall*)(void*);
using FlushFn = void(__thiscall*)(void*, std::uint32_t);
backends::d3d9::VtablePatch sprites_patch, flush_patch;
CoJHudBoundaryHookTargets active_targets{};
std::atomic<SpriteFn> original_sprites{nullptr};
std::atomic<FlushFn> original_flush{nullptr};
std::atomic<CoJHudBoundaryObserver> observer{nullptr};
std::atomic<void*> observer_context{nullptr};
std::atomic<CoJHudPermissionProvider> permission_provider{nullptr};
std::atomic<void*> permission_context{nullptr};
std::mutex integrity_mutex;
std::atomic_uint64_t hook_epoch{0};
bool integrity_lost = false;
std::atomic_bool installed{false};
std::atomic_uint64_t sequence{0};
struct Pending { std::uint64_t sequence = 0, epoch = 0; bool faulted = false; std::uintptr_t owner = 0; };
thread_local Pending pending;
thread_local unsigned sprite_depth = 0, flush_depth = 0;
thread_local std::uintptr_t current_flush_owner = 0;
thread_local std::uint32_t current_flush_option = 0;
struct LastErrorScope { DWORD value = GetLastError(); ~LastErrorScope() { SetLastError(value); } };
void __fastcall HookSprites(void*, void*);
void __fastcall HookFlush(void*, void*, std::uint32_t);

CoJHudHookContinuity InspectLocked() noexcept {
    const bool intact = installed.load(std::memory_order_acquire) && !integrity_lost &&
        active_targets.sprites_entry && active_targets.flush_entry &&
        *active_targets.sprites_entry == reinterpret_cast<void*>(&HookSprites) &&
        *active_targets.flush_entry == reinterpret_cast<void*>(&HookFlush);
    if (!intact && installed.load() && !integrity_lost) {
        integrity_lost = true; hook_epoch.fetch_add(1, std::memory_order_acq_rel);
    }
    return {hook_epoch.load(std::memory_order_acquire), intact};
}

void Observe(CoJHudBoundaryStage stage, void* owner, std::uint32_t option = 0) noexcept {
    LastErrorScope error;
    const auto callback = observer.load(std::memory_order_acquire);
    if (!callback || !pending.sequence) return;
    const auto continuity = InspectCoJHudHookContinuity();
    const bool fault = pending.faulted || !continuity.intact || pending.epoch != continuity.epoch;
    pending.faulted = fault;
    const bool selected = pending.sequence <= 8 || pending.sequence % 127 < 8;
    callback(observer_context.load(std::memory_order_acquire),
        {stage, pending.sequence, owner, option, sprite_depth, flush_depth,
         continuity.epoch, continuity.intact, fault, selected});
}
void __fastcall HookSprites(void* owner, void*) {
    auto outer = pending;
    const bool nested = sprite_depth != 0 || flush_depth != 0;
    if (nested) outer.faulted = true;
    const auto continuity = InspectCoJHudHookContinuity();
    pending = {sequence.fetch_add(1, std::memory_order_relaxed) + 1, continuity.epoch,
        nested || !continuity.intact, reinterpret_cast<std::uintptr_t>(owner)};
    ++sprite_depth;
    struct Exit {
        void* owner; Pending outer; bool completed = false;
        ~Exit() {
            if (!completed) pending.faulted = true;
            Observe(CoJHudBoundaryStage::sprites_end, owner);
            const bool fault = pending.faulted;
            --sprite_depth;
            if (sprite_depth || flush_depth) { pending = outer; pending.faulted |= fault; }
        }
    } exit{owner, outer};
    Observe(CoJHudBoundaryStage::sprites_begin, owner);
    original_sprites.load(std::memory_order_acquire)(owner); // exactly once
    exit.completed = true;
}
void __fastcall HookFlush(void* owner, void*, const std::uint32_t option) {
    const auto outer_owner = current_flush_owner;
    const auto outer_option = current_flush_option;
    current_flush_owner = reinterpret_cast<std::uintptr_t>(owner); current_flush_option = option;
    ++flush_depth;
    if (sprite_depth || flush_depth != 1) pending.faulted = true;
    struct Exit {
        void* owner; std::uint32_t option; std::uintptr_t outer_owner; std::uint32_t outer_option;
        bool completed = false;
        ~Exit() {
            if (!completed) pending.faulted = true;
            Observe(CoJHudBoundaryStage::flush_end, owner, option);
            --flush_depth;
            current_flush_owner = outer_owner; current_flush_option = outer_option;
            if (!sprite_depth && !flush_depth) pending = {};
        }
    } exit{owner, option, outer_owner, outer_option};
    Observe(CoJHudBoundaryStage::flush_begin, owner, option);
    original_flush.load(std::memory_order_acquire)(owner, option); // native stack word unchanged
    exit.completed = true;
}
bool Success(const backends::d3d9::VtablePatchOutcome& result) {
    using backends::d3d9::VtablePatchResult;
    return (result.result == VtablePatchResult::Applied ||
        result.result == VtablePatchResult::NoModification) && result.protection_restored;
}
bool RestoreLocked() noexcept {
    installed.store(false, std::memory_order_release);
    hook_epoch.fetch_add(1, std::memory_order_acq_rel);
    if (!active_targets.sprites_entry && !active_targets.flush_entry) return true;
    const bool sprite = Success(sprites_patch.Restore()) &&
        *active_targets.sprites_entry == active_targets.expected_sprites;
    const bool flush = Success(flush_patch.Restore()) &&
        *active_targets.flush_entry == active_targets.expected_flush;
    if (sprite && flush) active_targets = {};
    return sprite && flush;
}
}

CoJHudHookContinuity InspectCoJHudHookContinuity() noexcept {
    LastErrorScope error;
    std::unique_lock lock(integrity_mutex, std::try_to_lock);
    if (!lock.owns_lock()) return {hook_epoch.load(std::memory_order_acquire), false};
    return InspectLocked();
}
CoJHudDrawScope InspectCoJHudDrawScope() noexcept {
    const auto continuity = InspectCoJHudHookContinuity();
    return {pending.sequence, pending.owner, current_flush_owner, current_flush_option,
        sprite_depth, flush_depth, continuity.epoch, continuity.intact,
        pending.faulted || !pending.sequence || pending.epoch != continuity.epoch};
}
CoJHudDiagnosticPermission ReadCoJHudDiagnosticPermission() noexcept {
    LastErrorScope error;
    const auto provider = permission_provider.load(std::memory_order_acquire);
    if (!installed.load(std::memory_order_acquire) || !provider) return {};
    return provider(permission_context.load(std::memory_order_acquire));
}

bool InstallCoJHudBoundaryProbe(const CoJHudBoundaryHookTargets& targets,
    const bool exact_build_validated, CoJHudBoundaryObserver callback, void* context,
    const backends::d3d9::VtableMemoryOperations operations,
    CoJHudPermissionProvider provider, void* provider_context) noexcept {
    LastErrorScope error;
    std::lock_guard lock(integrity_mutex);
    if (!exact_build_validated || !callback || !targets.sprites_entry || !targets.flush_entry ||
        !targets.expected_sprites || !targets.expected_flush) return false;
    if (installed.load(std::memory_order_acquire))
        return targets.sprites_entry == active_targets.sprites_entry &&
            targets.flush_entry == active_targets.flush_entry &&
            *targets.sprites_entry == reinterpret_cast<void*>(&HookSprites) &&
            *targets.flush_entry == reinterpret_cast<void*>(&HookFlush) && InspectLocked().intact;
    if (active_targets.sprites_entry || active_targets.flush_entry) return false;
    if (*targets.sprites_entry != targets.expected_sprites ||
        *targets.flush_entry != targets.expected_flush) return false;
    // Foreign wrappers can retain our forwarding entry after restoration.
    // Never change the original call targets underneath those wrappers.
    if ((original_sprites.load() && reinterpret_cast<void*>(original_sprites.load()) != targets.expected_sprites) ||
        (original_flush.load() && reinterpret_cast<void*>(original_flush.load()) != targets.expected_flush)) return false;
    original_sprites = reinterpret_cast<SpriteFn>(targets.expected_sprites);
    original_flush = reinterpret_cast<FlushFn>(targets.expected_flush);
    observer = callback;
    observer_context = context;
    permission_context = provider_context;
    permission_provider = provider;
    sprites_patch = backends::d3d9::VtablePatch(operations);
    flush_patch = backends::d3d9::VtablePatch(operations);
    active_targets = targets;
    hook_epoch.fetch_add(1, std::memory_order_acq_rel);
    // The native target is the expected pointer of the actual CAS, not just
    // a preliminary check. Concurrent foreign installation must remain intact.
    if (!Success(sprites_patch.Install(targets.sprites_entry, targets.expected_sprites,
            reinterpret_cast<void*>(&HookSprites)))) {
        (void)RestoreLocked();
        return false;
    }
    if (!Success(flush_patch.Install(targets.flush_entry, targets.expected_flush,
            reinterpret_cast<void*>(&HookFlush)))) {
        (void)RestoreLocked();
        return false;
    }
    sequence.store(0, std::memory_order_release);
    pending = {};
    integrity_lost = false;
    installed.store(true, std::memory_order_release);
    return true;
}

bool RestoreCoJHudBoundaryProbe() noexcept {
    LastErrorScope error;
    std::lock_guard lock(integrity_mutex);
    // Keep original forwarding functions for any foreign wrapper retaining us.
    return RestoreLocked();
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
