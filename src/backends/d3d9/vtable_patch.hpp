#pragma once

#include <cstddef>
#include <cstdint>

namespace cojvr::backends::d3d9 {

enum class VtablePatchResult {
    NoModification,
    Applied,
    ProtectionFailure,
    Conflict,
    RollbackIncomplete,
    InvalidArgument,
};

struct VtablePatchOutcome {
    VtablePatchResult result = VtablePatchResult::InvalidArgument;
    bool modified = false;
    bool owns_entry = false;
    bool protection_restored = true;
    void* observed_target = nullptr;
};

struct VtableMemoryOperations {
    using ProtectFn = bool (*)(
        void* address,
        std::size_t size,
        std::uint32_t new_protection,
        std::uint32_t* old_protection,
        void* context) noexcept;
    using CompareExchangeFn = void* (*)(
        void** entry,
        void* replacement,
        void* expected,
        void* context) noexcept;

    ProtectFn protect = nullptr;
    CompareExchangeFn compare_exchange = nullptr;
    void* context = nullptr;

    [[nodiscard]] static VtableMemoryOperations Native() noexcept;
};

[[nodiscard]] bool PinModuleForAddress(void* address) noexcept;

class VtablePatch {
public:
    explicit VtablePatch(
        VtableMemoryOperations operations = VtableMemoryOperations::Native()) noexcept;

    VtablePatch(const VtablePatch&) = delete;
    VtablePatch& operator=(const VtablePatch&) = delete;
    VtablePatch(VtablePatch&& other) noexcept;
    VtablePatch& operator=(VtablePatch&& other) noexcept;

    [[nodiscard]] VtablePatchOutcome Install(
        void** entry,
        void* expected_target,
        void* replacement_target) noexcept;
    [[nodiscard]] VtablePatchOutcome Restore() noexcept;

    [[nodiscard]] bool owns_entry() const noexcept { return owns_entry_; }
    [[nodiscard]] bool protection_restore_pending() const noexcept {
        return protection_restore_pending_;
    }
    [[nodiscard]] void** entry() const noexcept { return entry_; }
    [[nodiscard]] void* original_target() const noexcept { return original_target_; }
    [[nodiscard]] void* replacement_target() const noexcept { return replacement_target_; }

private:
    VtableMemoryOperations operations_{};
    void** entry_ = nullptr;
    void* original_target_ = nullptr;
    void* replacement_target_ = nullptr;
    std::uint32_t original_protection_ = 0;
    bool owns_entry_ = false;
    bool protection_restore_pending_ = false;
};

} // namespace cojvr::backends::d3d9
