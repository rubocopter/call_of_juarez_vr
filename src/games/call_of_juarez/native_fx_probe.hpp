#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace cojvr::games::call_of_juarez {

enum class CoJNativeFxPhase { after_seed, frame, left_eye, right_eye };
enum class CoJNativeFxReadReason {
    observed,
    unrecognized_engine,
    invalid_engine_base,
    read_failed,
    partial_read,
    changed_during_read,
    missing_owner_lookup_boundary,
    missing_plane_extent_boundary
};

struct CoJNativeFxReadOperations {
    // Must copy into destination and return the exact number of bytes copied.
    // Returning success after a short copy is not permitted. No native calls.
    using Read = std::size_t (*)(void* context, std::uintptr_t address,
        void* destination, std::size_t size) noexcept;
    Read read = nullptr;
    void* context = nullptr;
    [[nodiscard]] static CoJNativeFxReadOperations Native() noexcept;
};

struct CoJNativeFxRequest {
    std::uintptr_t engine_base = 0;
    // Caller must have recognized ChromeEngine3 SHA-256
    // db69bc35919fe57187766771a2452aca11090474f6d63df1a85a80eded131ec8.
    bool exact_build_validated = false;
    std::uint32_t handle = 0;
    CoJNativeFxPhase phase = CoJNativeFxPhase::frame;
};

struct CoJNativeFxSnapshot {
    // Correlation only: the reader never resolves this manager-local handle.
    std::uint32_t handle = 0;
    CoJNativeFxPhase phase = CoJNativeFxPhase::frame;
    CoJNativeFxReadReason lookup_reason =
        CoJNativeFxReadReason::missing_owner_lookup_boundary;
    CoJNativeFxReadReason global_reason = CoJNativeFxReadReason::read_failed;
    std::optional<std::uint32_t> global_fx;
    CoJNativeFxReadReason camera_reason = CoJNativeFxReadReason::read_failed;
    // Diagnostic numeric addresses only; never borrowed pointers to retain/use.
    std::optional<std::uint32_t> renderer_address;
    std::optional<std::uint32_t> fx_camera_address;
    CoJNativeFxReadReason plane_reason =
        CoJNativeFxReadReason::missing_plane_extent_boundary;
    // No plane bytes are read: a readable extent at camera+0x2C4 is not proven.
};

// Stateless global/camera reader. Rechecks independent globals/camera before
// publication. Does not resolve a handle via a guessed global manager, install
// hooks, dereference native pointers, or call Update/Draw/Attack/JNI.
// Caller controls cadence. Rechecks detect changes, not an atomic snapshot or
// object lifetime guarantee; consume addresses as diagnostic values only.
[[nodiscard]] CoJNativeFxSnapshot ReadCoJNativeFxSnapshot(
    const CoJNativeFxRequest& request,
    CoJNativeFxReadOperations operations = CoJNativeFxReadOperations::Native()) noexcept;
[[nodiscard]] const char* CoJNativeFxReadReasonName(CoJNativeFxReadReason reason) noexcept;

} // namespace cojvr::games::call_of_juarez
