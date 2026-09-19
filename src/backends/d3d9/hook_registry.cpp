#include "backends/d3d9/hook_registry.hpp"

#include <algorithm>
#include <mutex>

namespace cojvr::backends::d3d9 {
namespace {

HookRegistryResult ToRegistryResult(const VtablePatchResult result) noexcept {
    switch (result) {
    case VtablePatchResult::ProtectionFailure:
        return HookRegistryResult::ProtectionFailure;
    case VtablePatchResult::Conflict:
        return HookRegistryResult::Conflict;
    case VtablePatchResult::RollbackIncomplete:
        return HookRegistryResult::RollbackIncomplete;
    case VtablePatchResult::InvalidArgument:
        return HookRegistryResult::InvalidArgument;
    case VtablePatchResult::NoModification:
        return HookRegistryResult::AlreadyInstalled;
    case VtablePatchResult::Applied:
        return HookRegistryResult::Installed;
    }
    return HookRegistryResult::InvalidArgument;
}

} // namespace

HookRegistry::HookRegistry(VtableMemoryOperations operations) noexcept
    : operations_(operations) {}

HookRegistryOutcome HookRegistry::Install(
    void** vtable,
    std::span<const HookSlotRequest> requests) noexcept {
    HookRegistryOutcome outcome{};
    if (!vtable || requests.empty() || !operations_.protect || !operations_.compare_exchange) {
        return outcome;
    }
    for (std::size_t request_index = 0; request_index < requests.size(); ++request_index) {
        if (!requests[request_index].replacement || !vtable[requests[request_index].index]) {
            return outcome;
        }
        for (std::size_t other = request_index + 1; other < requests.size(); ++other) {
            if (requests[request_index].index == requests[other].index) return outcome;
        }
    }

    try {
        std::unique_lock lock(mutex_);
        if (const auto existing = records_.find(vtable); existing != records_.end()) {
            const bool same = existing->second.state == RecordState::Active &&
                existing->second.slots.size() == requests.size() &&
                std::all_of(
                    requests.begin(), requests.end(), [&](const HookSlotRequest& request) {
                        const auto slot = std::find_if(
                            existing->second.slots.begin(), existing->second.slots.end(),
                            [&](const SlotRecord& value) { return value.index == request.index; });
                        return slot != existing->second.slots.end() &&
                            slot->replacement == request.replacement && slot->patch.owns_entry() &&
                            vtable[slot->index] == slot->replacement;
                    });
            outcome.result = same
                ? HookRegistryResult::AlreadyInstalled
                : HookRegistryResult::Conflict;
            outcome.ownership_record_retained = true;
            return outcome;
        }

        Record record{};
        record.slots.reserve(requests.size());
        for (const HookSlotRequest& request : requests) {
            record.slots.emplace_back(
                request.index, vtable[request.index], request.replacement, operations_);
        }
        auto [record_iterator, inserted] = records_.emplace(vtable, std::move(record));
        if (!inserted) {
            outcome.result = HookRegistryResult::Conflict;
            outcome.ownership_record_retained = true;
            return outcome;
        }

        Record& installed = record_iterator->second;
        HookRegistryResult failure = HookRegistryResult::Installed;
        bool failed = false;
        for (SlotRecord& slot : installed.slots) {
            const VtablePatchOutcome patch_outcome = slot.patch.Install(
                &vtable[slot.index], slot.original, slot.replacement);
            if (patch_outcome.modified) ++outcome.modified_slots;
            if (patch_outcome.result != VtablePatchResult::Applied) {
                failure = ToRegistryResult(patch_outcome.result);
                failed = true;
                break;
            }
        }

        if (!failed) {
            installed.state = RecordState::Active;
            outcome.result = HookRegistryResult::Installed;
            outcome.ownership_record_retained = true;
            return outcome;
        }

        bool rollback_incomplete = false;
        for (auto slot = installed.slots.rbegin(); slot != installed.slots.rend(); ++slot) {
            if (!slot->patch.owns_entry() && !slot->patch.protection_restore_pending()) continue;
            const VtablePatchOutcome rollback = slot->patch.Restore();
            if (rollback.result != VtablePatchResult::Applied ||
                !rollback.protection_restored || slot->patch.owns_entry()) {
                rollback_incomplete = true;
            }
        }

        if (outcome.modified_slots == 0 && !rollback_incomplete) {
            records_.erase(record_iterator);
            outcome.ownership_record_retained = false;
        } else {
            installed.state = rollback_incomplete
                ? RecordState::Incomplete
                : RecordState::Retired;
            outcome.ownership_record_retained = true;
        }
        outcome.result = rollback_incomplete
            ? HookRegistryResult::RollbackIncomplete
            : failure;
        return outcome;
    } catch (...) {
        outcome.result = HookRegistryResult::RollbackIncomplete;
        outcome.ownership_record_retained = true;
        return outcome;
    }
}

HookRegistryOutcome HookRegistry::Restore(void** vtable) noexcept {
    HookRegistryOutcome outcome{};
    if (!vtable) return outcome;

    try {
        std::unique_lock lock(mutex_);
        const auto record_iterator = records_.find(vtable);
        if (record_iterator == records_.end()) {
            outcome.result = HookRegistryResult::AlreadyInstalled;
            return outcome;
        }

        bool incomplete = false;
        for (auto slot = record_iterator->second.slots.rbegin();
             slot != record_iterator->second.slots.rend(); ++slot) {
            if (!slot->patch.owns_entry() && !slot->patch.protection_restore_pending()) continue;
            const VtablePatchOutcome restored = slot->patch.Restore();
            if (restored.modified) ++outcome.modified_slots;
            if (restored.result != VtablePatchResult::Applied ||
                !restored.protection_restored || slot->patch.owns_entry()) {
                incomplete = true;
            }
        }
        record_iterator->second.state = incomplete
            ? RecordState::Incomplete
            : RecordState::Retired;
        outcome.result = incomplete
            ? HookRegistryResult::RollbackIncomplete
            : HookRegistryResult::Installed;
        outcome.ownership_record_retained = true;
        return outcome;
    } catch (...) {
        outcome.result = HookRegistryResult::RollbackIncomplete;
        outcome.ownership_record_retained = true;
        return outcome;
    }
}

HookRegistryOutcome HookRegistry::Reacquire(void** vtable) noexcept {
    HookRegistryOutcome outcome{};
    if (!vtable) return outcome;

    try {
        std::unique_lock lock(mutex_);
        const auto record_iterator = records_.find(vtable);
        if (record_iterator == records_.end() ||
            record_iterator->second.state != RecordState::Active) {
            return outcome;
        }

        Record& record = record_iterator->second;
        outcome.ownership_record_retained = true;
        for (const SlotRecord& slot : record.slots) {
            void* current = vtable[slot.index];
            if (current != slot.original && current != slot.replacement) {
                outcome.result = HookRegistryResult::Conflict;
                return outcome;
            }
        }

        std::vector<SlotRecord*> reacquired;
        reacquired.reserve(record.slots.size());
        for (SlotRecord& slot : record.slots) {
            if (vtable[slot.index] == slot.replacement) continue;
            const VtablePatchOutcome patch_outcome = slot.patch.Reacquire();
            if (patch_outcome.modified) {
                ++outcome.modified_slots;
                reacquired.push_back(&slot);
            }
            if (patch_outcome.result == VtablePatchResult::Applied ||
                patch_outcome.result == VtablePatchResult::NoModification) {
                continue;
            }

            bool rollback_incomplete = false;
            for (auto restored = reacquired.rbegin(); restored != reacquired.rend(); ++restored) {
                const VtablePatchOutcome rollback = (*restored)->patch.Restore();
                if (rollback.result != VtablePatchResult::Applied ||
                    !rollback.protection_restored || (*restored)->patch.owns_entry()) {
                    rollback_incomplete = true;
                }
            }
            outcome.modified_slots = 0;
            outcome.result = rollback_incomplete
                ? HookRegistryResult::RollbackIncomplete
                : ToRegistryResult(patch_outcome.result);
            return outcome;
        }

        outcome.result = outcome.modified_slots == 0
            ? HookRegistryResult::AlreadyInstalled
            : HookRegistryResult::Installed;
        return outcome;
    } catch (...) {
        outcome.result = HookRegistryResult::RollbackIncomplete;
        outcome.ownership_record_retained = true;
        return outcome;
    }
}

void* HookRegistry::OriginalTarget(void** vtable, std::size_t index) const noexcept {
    try {
        std::shared_lock lock(mutex_);
        const auto record = records_.find(vtable);
        if (record == records_.end()) return nullptr;
        const auto slot = std::find_if(
            record->second.slots.begin(), record->second.slots.end(),
            [index](const SlotRecord& value) { return value.index == index; });
        return slot == record->second.slots.end() ? nullptr : slot->original;
    } catch (...) {
        return nullptr;
    }
}

std::vector<HookSlotStatus> HookRegistry::Inspect(void** vtable) const noexcept {
    try {
        std::shared_lock lock(mutex_);
        const auto record = records_.find(vtable);
        if (record == records_.end()) return {};
        std::vector<HookSlotStatus> result;
        result.reserve(record->second.slots.size());
        for (const SlotRecord& slot : record->second.slots) {
            result.push_back(HookSlotStatus{
                .index = slot.index,
                .original = slot.original,
                .replacement = slot.replacement,
                .current = vtable[slot.index],
                .owned = slot.patch.owns_entry() && vtable[slot.index] == slot.replacement,
            });
        }
        return result;
    } catch (...) {
        return {};
    }
}

std::vector<void**> HookRegistry::RegisteredVtables() const noexcept {
    try {
        std::shared_lock lock(mutex_);
        std::vector<void**> result;
        result.reserve(records_.size());
        for (const auto& [vtable, record] : records_) {
            (void)record;
            result.push_back(vtable);
        }
        return result;
    } catch (...) {
        return {};
    }
}

} // namespace cojvr::backends::d3d9
