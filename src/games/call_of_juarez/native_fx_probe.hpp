#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <array>

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
    invalid_owner,
    invalid_slot,
    stale_handle,
    unknown_emitter_type,
    invalid_values,
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
    // Fresh GetThisID of the active weapon, not a cached native pointer.
    std::uint32_t owner_id = 0;
};

struct CoJNativeFxEmitter {
    std::uint32_t manager_address = 0, emitter_address = 0, vtable_rva = 0;
    float clock = 0, birth = 0, lifetime = 0, time_scale = 0;
    std::array<float, 3> position{}, previous_position{};
    std::uint32_t live = 0, emitted = 0, expired = 0;
    bool enabled = false, render_enabled = false;
};

// Per-process diagnostic budget, no native references. Context loss closes
// windows without replenishing the budget. Caller polls fresh weapon identity.
class CoJNativeFxObservationWindow {
public:
    [[nodiscard]] bool NeedsPoll(std::uint64_t now_ms) const noexcept {
        return remaining_ || now_ms<windows_[0].until || now_ms<windows_[1].until;
    }
    [[nodiscard]] bool HasOpenWindow(std::uint64_t now_ms) const noexcept {
        return now_ms<windows_[0].until || now_ms<windows_[1].until;
    }
    void Invalidate(int hand) noexcept {
        if (hand>=0 && hand<2) { windows_[hand].until=0; windows_[hand].next=0; }
    }
    [[nodiscard]] bool Observe(int hand, std::uint32_t owner, int serial,
        std::uint64_t now_ms) noexcept {
        if (hand<0 || hand>1 || !owner) return false;
        auto& w=windows_[hand];
        if (w.owner!=owner || w.serial!=serial) {
            w={owner,serial,0,0};
            if (serial>0 && remaining_) { --remaining_; w.until=now_ms+500; }
        }
        if (now_ms>=w.until || now_ms<w.next) return false;
        w.next=now_ms+20;
        return true;
    }
private:
    struct Window { std::uint32_t owner=0; int serial=0; std::uint64_t until=0,next=0; };
    std::array<Window,2> windows_{};
    unsigned remaining_=16;
};

struct CoJNativeFxSnapshot {
    // Numeric correlation only; native addresses are never retained for use.
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
    std::optional<CoJNativeFxEmitter> emitter;
    CoJNativeFxReadReason plane_reason =
        CoJNativeFxReadReason::missing_plane_extent_boundary;
    // Plane bytes/classification are not sampled. Camera identity alone does
    // not establish that native Draw classified or submitted these particles.
};

// Stateless reader of exact globals and owner-local emitter slots. Rechecks
// owner/manager/slot identity before publication; only the two measured native
// emitter types expose counters. Never installs hooks or calls Update/Draw/JNI.
// Caller controls cadence. Rechecks detect changes, not an atomic snapshot or
// object lifetime guarantee; consume addresses as diagnostic values only.
[[nodiscard]] CoJNativeFxSnapshot ReadCoJNativeFxSnapshot(
    const CoJNativeFxRequest& request,
    CoJNativeFxReadOperations operations = CoJNativeFxReadOperations::Native()) noexcept;
[[nodiscard]] const char* CoJNativeFxReadReasonName(CoJNativeFxReadReason reason) noexcept;

} // namespace cojvr::games::call_of_juarez
