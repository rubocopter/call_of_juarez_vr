#pragma once

#include "backends/d3d9/vtable_patch.hpp"

#include <cstddef>
#include <shared_mutex>
#include <span>
#include <unordered_map>
#include <vector>

namespace cojvr::backends::d3d9 {

enum class HookRegistryResult {
    Installed,
    AlreadyInstalled,
    ProtectionFailure,
    Conflict,
    RollbackIncomplete,
    InvalidArgument,
};

struct HookSlotRequest {
    std::size_t index = 0;
    void* replacement = nullptr;
};

struct HookRegistryOutcome {
    HookRegistryResult result = HookRegistryResult::InvalidArgument;
    std::size_t modified_slots = 0;
    bool ownership_record_retained = false;
};

struct HookSlotStatus {
    std::size_t index = 0;
    void* original = nullptr;
    void* replacement = nullptr;
    void* current = nullptr;
    bool owned = false;
};

class HookRegistry {
public:
    explicit HookRegistry(
        VtableMemoryOperations operations = VtableMemoryOperations::Native()) noexcept;

    HookRegistry(const HookRegistry&) = delete;
    HookRegistry& operator=(const HookRegistry&) = delete;

    [[nodiscard]] HookRegistryOutcome Install(
        void** vtable,
        std::span<const HookSlotRequest> requests) noexcept;
    [[nodiscard]] HookRegistryOutcome Reacquire(void** vtable) noexcept;
    [[nodiscard]] HookRegistryOutcome Restore(void** vtable) noexcept;

    [[nodiscard]] void* OriginalTarget(void** vtable, std::size_t index) const noexcept;
    [[nodiscard]] std::vector<HookSlotStatus> Inspect(void** vtable) const noexcept;
    [[nodiscard]] std::vector<void**> RegisteredVtables() const noexcept;

private:
    enum class RecordState { Installing, Active, Retired, Incomplete };

    struct SlotRecord {
        std::size_t index = 0;
        void* original = nullptr;
        void* replacement = nullptr;
        VtablePatch patch;

        SlotRecord(
            std::size_t slot_index,
            void* original_target,
            void* replacement_target,
            VtableMemoryOperations operations) noexcept
            : index(slot_index),
              original(original_target),
              replacement(replacement_target),
              patch(operations) {}
    };

    struct Record {
        RecordState state = RecordState::Installing;
        std::vector<SlotRecord> slots{};
    };

    VtableMemoryOperations operations_{};
    mutable std::shared_mutex mutex_{};
    std::unordered_map<void**, Record> records_{};
};

} // namespace cojvr::backends::d3d9
