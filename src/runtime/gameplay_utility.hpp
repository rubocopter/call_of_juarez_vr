#pragma once
#include "runtime/vr_types.hpp"

namespace cojvr::runtime {
struct AuxiliaryButtonResult { bool objectives=false; bool recenter=false; };
// Create resolves at the presenter sample rate. Taps are decided on release;
// interrupted gestures require a real available release before reacquisition.
class AuxiliaryButtonGesture final {
public:
    [[nodiscard]] AuxiliaryButtonResult Update(bool available, bool held,
        std::uint64_t monotonic_ms, bool objectives_allowed = true) noexcept;
private:
    bool armed_=false, pressed_=false, held_action_=false;
    bool tap_allowed_=false;
    std::uint64_t started_ms_=0;
};
// Retain a tap between presenter polls and game consumption. Use the gesture's
// availability gate so any intervening loss cancels an already queued tap.
class AuxiliaryObjectivesPending final {
public:
    void Observe(bool available, bool objectives_allowed, bool requested) noexcept;
    [[nodiscard]] bool Consume() noexcept;
private:
    bool pending_=false;
};
// Resolves radial ownership, direct button toggles and release barriers.
// Physical paths and exact native actions remain in their respective adapters.
class GameplayControlMapper final {
public:
    [[nodiscard]] GameplayInputState Update(const GameplayInputState& raw,
        bool gameplay_context_allowed = true, bool objectives_requested = false) noexcept;
private:
    std::uint32_t release_required_ = 0xFFFFFFFFU;
    bool was_radial_ = false;
    bool radial_armed_ = false;
    bool focus_latched_ = false;
    bool crouch_latched_ = false;
    bool focus_pressed_ = false;
    bool crouch_pressed_ = false;
    bool turn_release_required_ = true;
    int radial_selection_ = -1;
    std::uint64_t input_context_generation_ = 0;
};
} // namespace cojvr::runtime
