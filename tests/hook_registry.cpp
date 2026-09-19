#include "backends/d3d9/hook_registry.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>
#include <unordered_set>

namespace {

using namespace cojvr::backends::d3d9;

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

struct FakeMemory {
    std::size_t protect_calls = 0;
    std::uint32_t current_protection = 0x20;
    std::unordered_set<std::size_t> failing_protect_calls{};
    std::thread transition_callback{};
    void (*on_replaced)() = nullptr;
};

bool FakeProtect(
    void*, std::size_t, std::uint32_t new_protection, std::uint32_t* old_protection,
    void* context) noexcept {
    auto& memory = *static_cast<FakeMemory*>(context);
    ++memory.protect_calls;
    if (old_protection) *old_protection = memory.current_protection;
    if (memory.failing_protect_calls.contains(memory.protect_calls)) return false;
    memory.current_protection = new_protection;
    return true;
}

void* FakeCompareExchange(
    void** entry, void* replacement, void* expected, void* context) noexcept {
    void* observed = *entry;
    if (observed == expected) {
        *entry = replacement;
        auto& memory = *static_cast<FakeMemory*>(context);
        if (memory.on_replaced && !memory.transition_callback.joinable()) {
            memory.transition_callback = std::thread(memory.on_replaced);
        }
    }
    return observed;
}

VtableMemoryOperations Operations(FakeMemory& memory) {
    return VtableMemoryOperations{
        .protect = FakeProtect,
        .compare_exchange = FakeCompareExchange,
        .context = &memory,
    };
}

std::atomic_uint32_t g_original_a_marker{0};
std::atomic_uint32_t g_original_b_marker{0};
std::atomic_uint32_t g_replacement_a_marker{0};
std::atomic_uint32_t g_replacement_b_marker{0};
std::atomic_uint32_t g_replacement_c_marker{0};

__declspec(noinline) void OriginalA() { g_original_a_marker.fetch_add(1); }
__declspec(noinline) void OriginalB() { g_original_b_marker.fetch_add(1); }
__declspec(noinline) void ReplacementA() { g_replacement_a_marker.fetch_add(1); }
__declspec(noinline) void ReplacementB() { g_replacement_b_marker.fetch_add(1); }
__declspec(noinline) void ReplacementC() { g_replacement_c_marker.fetch_add(1); }

std::atomic_uint32_t g_transition_original_calls{0};
std::atomic_uint32_t g_transition_wrapper_calls{0};
HookRegistry* g_transition_registry = nullptr;
void** g_transition_vtable = nullptr;

void TransitionOriginal() {
    g_transition_original_calls.fetch_add(1, std::memory_order_relaxed);
}

void TransitionWrapper() {
    g_transition_wrapper_calls.fetch_add(1, std::memory_order_relaxed);
    const auto original = reinterpret_cast<void (*)()>(
        g_transition_registry->OriginalTarget(g_transition_vtable, 0));
    if (original) original();
}

int TestPatchFailures() {
    {
        FakeMemory memory{};
        memory.failing_protect_calls.insert(1);
        void* entry = reinterpret_cast<void*>(&OriginalA);
        VtablePatch patch(Operations(memory));
        const auto outcome = patch.Install(
            &entry, reinterpret_cast<void*>(&OriginalA), reinterpret_cast<void*>(&ReplacementA));
        if (outcome.result != VtablePatchResult::ProtectionFailure || outcome.modified ||
            patch.owns_entry() || entry != reinterpret_cast<void*>(&OriginalA)) {
            return Fail("failure before replacement changed ownership or target");
        }
    }

    {
        FakeMemory memory{};
        void* entry = reinterpret_cast<void*>(&OriginalA);
        VtablePatch patch(Operations(memory));
        const auto installed = patch.Install(
            &entry, reinterpret_cast<void*>(&OriginalA), reinterpret_cast<void*>(&ReplacementA));
        memory.current_protection = 0x40;
        const auto restored = patch.Restore();
        if (installed.result != VtablePatchResult::Applied ||
            restored.result != VtablePatchResult::Applied ||
            memory.current_protection != 0x40) {
            return Fail("restore did not preserve the protection observed before removal");
        }
    }

    {
        FakeMemory memory{};
        memory.failing_protect_calls.insert(2);
        void* entry = reinterpret_cast<void*>(&OriginalA);
        VtablePatch patch(Operations(memory));
        const auto outcome = patch.Install(
            &entry, reinterpret_cast<void*>(&OriginalA), reinterpret_cast<void*>(&ReplacementA));
        if (outcome.result != VtablePatchResult::ProtectionFailure || !outcome.modified ||
            !patch.owns_entry() || entry != reinterpret_cast<void*>(&ReplacementA)) {
            return Fail("post-replacement protection failure lost patch ownership");
        }
        const auto restored = patch.Restore();
        if (restored.result != VtablePatchResult::Applied || patch.owns_entry() ||
            patch.protection_restore_pending() || memory.current_protection != 0x20 ||
            entry != reinterpret_cast<void*>(&OriginalA)) {
            return Fail("owned patch did not recover after protection failure");
        }
    }

    {
        FakeMemory memory{};
        memory.failing_protect_calls.insert(2);
        void* entry = reinterpret_cast<void*>(&ReplacementC);
        VtablePatch patch(Operations(memory));
        const auto outcome = patch.Install(
            &entry, reinterpret_cast<void*>(&OriginalA), reinterpret_cast<void*>(&ReplacementA));
        if (outcome.result != VtablePatchResult::Conflict ||
            !patch.protection_restore_pending()) {
            return Fail("conflict did not retain failed protection restoration state");
        }
        const auto recovered = patch.Restore();
        if (recovered.result != VtablePatchResult::Applied ||
            patch.protection_restore_pending() || memory.current_protection != 0x20 ||
            entry != reinterpret_cast<void*>(&ReplacementC)) {
            return Fail("conflict protection recovery changed the foreign target or stayed pending");
        }
    }

    {
        FakeMemory memory{};
        void* entry = reinterpret_cast<void*>(&ReplacementC);
        VtablePatch patch(Operations(memory));
        const auto outcome = patch.Install(
            &entry, reinterpret_cast<void*>(&OriginalA), reinterpret_cast<void*>(&ReplacementA));
        if (outcome.result != VtablePatchResult::Conflict || outcome.modified ||
            patch.owns_entry() || entry != reinterpret_cast<void*>(&ReplacementC)) {
            return Fail("conflicting target was overwritten");
        }
    }
    return 0;
}

int TestRegistryOwnership() {
    FakeMemory memory{};
    HookRegistry registry(Operations(memory));
    void* first_vtable[]{reinterpret_cast<void*>(&OriginalA), reinterpret_cast<void*>(&OriginalB)};
    void* second_vtable[]{reinterpret_cast<void*>(&OriginalB), reinterpret_cast<void*>(&OriginalA)};
    const HookSlotRequest requests[]{
        {0, reinterpret_cast<void*>(&ReplacementA)},
        {1, reinterpret_cast<void*>(&ReplacementB)},
    };

    const auto first = registry.Install(first_vtable, requests);
    const auto second = registry.Install(second_vtable, requests);
    if (first.result != HookRegistryResult::Installed || first.modified_slots != 2 ||
        second.result != HookRegistryResult::Installed || second.modified_slots != 2) {
        return Fail("registry did not install two independent vtables");
    }
    if (registry.OriginalTarget(first_vtable, 0) != reinterpret_cast<void*>(&OriginalA) ||
        registry.OriginalTarget(second_vtable, 0) != reinterpret_cast<void*>(&OriginalB)) {
        return Fail("registry did not retain originals per vtable");
    }

    const auto reinstall = registry.Install(first_vtable, requests);
    if (reinstall.result != HookRegistryResult::AlreadyInstalled) {
        return Fail("identical reinstall was not reported explicitly");
    }
    const HookSlotRequest conflicting[]{
        {0, reinterpret_cast<void*>(&ReplacementC)},
        {1, reinterpret_cast<void*>(&ReplacementB)},
    };
    if (registry.Install(first_vtable, conflicting).result != HookRegistryResult::Conflict) {
        return Fail("conflicting reinstall was accepted");
    }

    first_vtable[0] = reinterpret_cast<void*>(&ReplacementC);
    const auto statuses = registry.Inspect(first_vtable);
    if (statuses.size() != 2 || statuses[0].owned || statuses[0].current != first_vtable[0]) {
        return Fail("integrity inspection did not expose replacement loss");
    }
    const auto restore_first = registry.Restore(first_vtable);
    if (restore_first.result != HookRegistryResult::RollbackIncomplete ||
        first_vtable[0] != reinterpret_cast<void*>(&ReplacementC) ||
        first_vtable[1] != reinterpret_cast<void*>(&OriginalB)) {
        return Fail("restore overwrote a foreign hook or missed an owned slot");
    }
    const auto restore_second = registry.Restore(second_vtable);
    if (restore_second.result != HookRegistryResult::Installed ||
        second_vtable[0] != reinterpret_cast<void*>(&OriginalB) ||
        second_vtable[1] != reinterpret_cast<void*>(&OriginalA)) {
        return Fail("second vtable did not restore independently");
    }
    return 0;
}

int TestSafeReacquire() {
    FakeMemory memory{};
    HookRegistry registry(Operations(memory));
    void* vtable[]{reinterpret_cast<void*>(&OriginalA)};
    const HookSlotRequest request{0, reinterpret_cast<void*>(&ReplacementA)};

    const auto installed = registry.Install(
        vtable, std::span<const HookSlotRequest>(&request, 1));
    if (installed.result != HookRegistryResult::Installed ||
        vtable[0] != reinterpret_cast<void*>(&ReplacementA)) {
        return Fail("reacquire setup did not install the replacement");
    }

    // Reproduce the live-game failure: another participant restores the exact
    // native target after our hook was installed.
    vtable[0] = reinterpret_cast<void*>(&OriginalA);
    const auto reacquired = registry.Reacquire(vtable);
    if (reacquired.result != HookRegistryResult::Installed ||
        reacquired.modified_slots != 1 ||
        vtable[0] != reinterpret_cast<void*>(&ReplacementA) ||
        registry.Inspect(vtable).front().owned == false) {
        return Fail("registry did not safely reacquire an externally restored original slot");
    }

    // A foreign hook is never ours to replace.
    vtable[0] = reinterpret_cast<void*>(&ReplacementC);
    const auto conflict = registry.Reacquire(vtable);
    if (conflict.result != HookRegistryResult::Conflict ||
        conflict.modified_slots != 0 ||
        vtable[0] != reinterpret_cast<void*>(&ReplacementC)) {
        return Fail("registry reacquire overwrote or accepted a foreign hook");
    }
    return 0;
}

int TestPartialRollback() {
    FakeMemory memory{};
    memory.failing_protect_calls.insert(3);
    memory.failing_protect_calls.insert(5);
    HookRegistry registry(Operations(memory));
    void* vtable[]{reinterpret_cast<void*>(&OriginalA), reinterpret_cast<void*>(&OriginalB)};
    const HookSlotRequest requests[]{
        {0, reinterpret_cast<void*>(&ReplacementA)},
        {1, reinterpret_cast<void*>(&ReplacementB)},
    };
    const auto outcome = registry.Install(vtable, requests);
    if (outcome.result != HookRegistryResult::RollbackIncomplete ||
        !outcome.ownership_record_retained || vtable[0] != reinterpret_cast<void*>(&OriginalA) ||
        vtable[1] != reinterpret_cast<void*>(&OriginalB)) {
        return Fail("partial rollback state was not retained and reported");
    }
    return 0;
}

int TestCallbackDuringTransition() {
    FakeMemory memory{};
    HookRegistry registry(Operations(memory));
    void* vtable[]{reinterpret_cast<void*>(&TransitionOriginal)};
    g_transition_registry = &registry;
    g_transition_vtable = vtable;
    memory.on_replaced = [] {
        reinterpret_cast<void (*)()>(g_transition_vtable[0])();
    };
    const HookSlotRequest request{0, reinterpret_cast<void*>(&TransitionWrapper)};
    const auto outcome = registry.Install(vtable, std::span<const HookSlotRequest>(&request, 1));
    if (memory.transition_callback.joinable()) memory.transition_callback.join();
    if (outcome.result != HookRegistryResult::Installed ||
        g_transition_wrapper_calls.load(std::memory_order_relaxed) != 1 ||
        g_transition_original_calls.load(std::memory_order_relaxed) != 1) {
        return Fail("callback during installation could not reach its retained original");
    }
    return 0;
}

} // namespace

int main() {
    if (const int result = TestPatchFailures()) return result;
    if (const int result = TestRegistryOwnership()) return result;
    if (const int result = TestSafeReacquire()) return result;
    if (const int result = TestPartialRollback()) return result;
    if (const int result = TestCallbackDuringTransition()) return result;
    std::cout << "vtable patch and hook registry failure/ownership tests passed\n";
    return 0;
}
