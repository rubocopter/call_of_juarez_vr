#include "backends/d3d9/vtable_patch.hpp"

#include <windows.h>

#include <utility>

namespace cojvr::backends::d3d9 {
namespace {

constexpr std::uint32_t kWritableProtection = PAGE_READWRITE;

bool NativeProtect(
    void* address,
    std::size_t size,
    std::uint32_t new_protection,
    std::uint32_t* old_protection,
    void*) noexcept {
    DWORD previous = 0;
    const BOOL result = VirtualProtect(
        address, size, static_cast<DWORD>(new_protection), &previous);
    if (old_protection) *old_protection = previous;
    return result != FALSE;
}

void* NativeCompareExchange(
    void** entry,
    void* replacement,
    void* expected,
    void*) noexcept {
    return InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(entry), replacement, expected);
}

} // namespace

VtableMemoryOperations VtableMemoryOperations::Native() noexcept {
    return VtableMemoryOperations{
        .protect = NativeProtect,
        .compare_exchange = NativeCompareExchange,
    };
}

bool PinModuleForAddress(void* address) noexcept {
    if (!address) return false;
    HMODULE module = nullptr;
    return GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(address), &module) != FALSE;
}

VtablePatch::VtablePatch(VtableMemoryOperations operations) noexcept
    : operations_(operations) {}

VtablePatch::VtablePatch(VtablePatch&& other) noexcept {
    *this = std::move(other);
}

VtablePatch& VtablePatch::operator=(VtablePatch&& other) noexcept {
    if (this == &other) return *this;
    operations_ = other.operations_;
    entry_ = std::exchange(other.entry_, nullptr);
    original_target_ = std::exchange(other.original_target_, nullptr);
    replacement_target_ = std::exchange(other.replacement_target_, nullptr);
    original_protection_ = std::exchange(other.original_protection_, 0);
    owns_entry_ = std::exchange(other.owns_entry_, false);
    protection_restore_pending_ =
        std::exchange(other.protection_restore_pending_, false);
    return *this;
}

VtablePatchOutcome VtablePatch::Install(
    void** entry,
    void* expected_target,
    void* replacement_target) noexcept {
    VtablePatchOutcome outcome{};
    if (!entry || !expected_target || !replacement_target ||
        !operations_.protect || !operations_.compare_exchange) {
        outcome.result = VtablePatchResult::InvalidArgument;
        return outcome;
    }
    if (entry_) {
        outcome.result = owns_entry_ && entry_ == entry &&
                replacement_target_ == replacement_target
            ? VtablePatchResult::NoModification
            : VtablePatchResult::Conflict;
        outcome.owns_entry = owns_entry_;
        outcome.observed_target = entry ? *entry : nullptr;
        return outcome;
    }

    entry_ = entry;
    original_target_ = expected_target;
    replacement_target_ = replacement_target;

    std::uint32_t old_protection = 0;
    if (!operations_.protect(
            entry_, sizeof(*entry_), kWritableProtection, &old_protection,
            operations_.context)) {
        outcome.result = VtablePatchResult::ProtectionFailure;
        outcome.protection_restored = false;
        return outcome;
    }
    original_protection_ = old_protection;

    outcome.observed_target = operations_.compare_exchange(
        entry_, replacement_target_, original_target_, operations_.context);
    if (outcome.observed_target != original_target_) {
        std::uint32_t ignored = 0;
        outcome.protection_restored = operations_.protect(
            entry_, sizeof(*entry_), original_protection_, &ignored, operations_.context);
        protection_restore_pending_ = !outcome.protection_restored;
        outcome.result = VtablePatchResult::Conflict;
        return outcome;
    }

    owns_entry_ = true;
    outcome.modified = true;
    outcome.owns_entry = true;
    std::uint32_t ignored = 0;
    outcome.protection_restored = operations_.protect(
        entry_, sizeof(*entry_), original_protection_, &ignored, operations_.context);
    protection_restore_pending_ = !outcome.protection_restored;
    outcome.result = outcome.protection_restored
        ? VtablePatchResult::Applied
        : VtablePatchResult::ProtectionFailure;
    return outcome;
}

VtablePatchOutcome VtablePatch::Restore() noexcept {
    VtablePatchOutcome outcome{};
    if (!entry_ || !replacement_target_ || !original_target_) {
        outcome.result = VtablePatchResult::NoModification;
        return outcome;
    }
    if (!owns_entry_) {
        outcome.observed_target = *entry_;
        if (!protection_restore_pending_) {
            outcome.result = VtablePatchResult::NoModification;
            return outcome;
        }
        std::uint32_t ignored = 0;
        outcome.protection_restored = operations_.protect(
            entry_, sizeof(*entry_), original_protection_, &ignored, operations_.context);
        protection_restore_pending_ = !outcome.protection_restored;
        outcome.result = outcome.protection_restored
            ? VtablePatchResult::Applied
            : VtablePatchResult::ProtectionFailure;
        return outcome;
    }

    std::uint32_t prior_protection = 0;
    if (!operations_.protect(
            entry_, sizeof(*entry_), kWritableProtection, &prior_protection,
            operations_.context)) {
        outcome.result = VtablePatchResult::ProtectionFailure;
        outcome.owns_entry = true;
        outcome.protection_restored = false;
        return outcome;
    }

    const std::uint32_t protection_to_restore =
        protection_restore_pending_ && prior_protection == kWritableProtection
        ? original_protection_ : prior_protection;

    outcome.observed_target = operations_.compare_exchange(
        entry_, original_target_, replacement_target_, operations_.context);
    if (outcome.observed_target != replacement_target_) {
        owns_entry_ = false;
        std::uint32_t ignored = 0;
        outcome.protection_restored = operations_.protect(
            entry_, sizeof(*entry_), protection_to_restore, &ignored, operations_.context);
        protection_restore_pending_ = !outcome.protection_restored;
        outcome.result = VtablePatchResult::Conflict;
        return outcome;
    }

    owns_entry_ = false;
    outcome.modified = true;
    std::uint32_t ignored = 0;
    outcome.protection_restored = operations_.protect(
        entry_, sizeof(*entry_), protection_to_restore, &ignored, operations_.context);
    protection_restore_pending_ = !outcome.protection_restored;
    outcome.result = outcome.protection_restored
        ? VtablePatchResult::Applied
        : VtablePatchResult::RollbackIncomplete;
    return outcome;
}

} // namespace cojvr::backends::d3d9
