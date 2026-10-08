#include "runtime/gameplay_utility.hpp"
#include "runtime/equipment_actions.hpp"
#include <cmath>

namespace cojvr::runtime {
AuxiliaryButtonResult AuxiliaryButtonGesture::Update(const bool available, const bool held,
    const std::uint64_t monotonic_ms, const bool objectives_allowed) noexcept {
    if (!available || (pressed_ && monotonic_ms<started_ms_)) {
        armed_=pressed_=held_action_=tap_allowed_=false;return {};
    }
    AuxiliaryButtonResult result{};
    if (!held) {
        if (pressed_ && !held_action_) {
            if (monotonic_ms-started_ms_>=800) result.recenter=true;
            else result.objectives=tap_allowed_ && objectives_allowed;
        }
        armed_=true;pressed_=held_action_=false;return result;
    }
    if (!armed_) return {};
    if (!pressed_) {
        pressed_=true;started_ms_=monotonic_ms;tap_allowed_=objectives_allowed;
    }
    tap_allowed_ &= objectives_allowed;
    if (!held_action_ && monotonic_ms-started_ms_>=800) {
        held_action_=true;result.recenter=true;
    }
    return result;
}
void AuxiliaryObjectivesPending::Observe(const bool available,
    const bool objectives_allowed, const bool requested) noexcept {
    pending_=available && objectives_allowed && (pending_ || requested);
}
bool AuxiliaryObjectivesPending::Consume() noexcept {
    const bool result=pending_;
    pending_=false;
    return result;
}
GameplayInputState GameplayControlMapper::Update(const GameplayInputState& raw,
    const bool gameplay_context_allowed, const bool objectives_requested) noexcept {
    if (raw.active && raw.input_context_generation != input_context_generation_) {
        (void)Update({}, false);
        input_context_generation_ = raw.input_context_generation;
    }
    if (!raw.active || !gameplay_context_allowed) {
        release_required_ = 0xFFFFFFFFU;
        was_radial_ = radial_armed_ = false;
        focus_latched_ = crouch_latched_ = false;
        focus_pressed_ = crouch_pressed_ = false;
        turn_release_required_ = true;
        radial_selection_ = -1;
        return {};
    }
    const bool previously_radial = was_radial_;
    if (!raw.radial_available) radial_armed_ = false;
    else if (!raw.weapon_radial) radial_armed_ = true;
    const bool radial = raw.radial_available && raw.weapon_radial && radial_armed_;
    auto result = raw;
    result.weapon_radial = radial;
    result.radial_highlight = -1;
    result.radial_confirmed = false;
    const auto filter = [&](const bool value, bool& output, const std::size_t index) {
        const auto bit = 1U << index;
        if ((raw.digital_available & bit) == 0) {
            release_required_ |= bit; output = false;
        } else if (!value) {
            release_required_ &= ~bit;
        } else if (radial || previously_radial || (release_required_ & bit) != 0) {
            // A button consumed by the wheel cannot acquire a new meaning at close.
            release_required_ |= bit; output = false;
        }
    };
    for (std::size_t i=0; i<kGameplayDigitalMembers.size(); ++i) {
        const auto member=kGameplayDigitalMembers[i];
        filter(raw.*member,result.*member,i);
    }
    for (std::size_t i=0; i<result.equipment_select.size(); ++i)
        filter(raw.equipment_select[i],result.equipment_select[i],kGameplayDigitalMembers.size()+i);
    if (!radial && !previously_radial) result.objectives |= objectives_requested;
    if (!raw.move_available) result.move = {};

    const bool finite_turn=raw.turn_available && std::isfinite(raw.turn.x) && std::isfinite(raw.turn.y);
    const float magnitude=finite_turn ? std::hypot(raw.turn.x,raw.turn.y) : 0.0F;
    const bool neutral=finite_turn && magnitude<=.25F;
    if (!finite_turn) turn_release_required_=true;
    if (radial || previously_radial) {
        // Retain the old deliberate deflection threshold, add radial and
        // five-degree angular hysteresis, but never dispatch during traversal.
        if (!finite_turn || magnitude<=.55F) radial_selection_=-1;
        else {
            float angle=std::atan2(raw.turn.x,raw.turn.y)*57.295779513F;
            if (angle<0) angle+=360;
            const int sector=static_cast<int>(std::floor((angle+22.5F)/45.0F))%8;
            if (radial_selection_<0) {
                if (magnitude>=.65F) radial_selection_=sector;
            } else {
                const float delta=std::remainder(angle-radial_selection_*45.0F,360.0F);
                if (std::fabs(delta)>27.5F) radial_selection_=sector;
            }
        }
        result.turn={};
        turn_release_required_=true;
        if (radial) result.radial_highlight=radial_selection_;
        else if (raw.radial_available && !raw.weapon_radial && radial_selection_>=0) {
            RequestEquipmentAction(result,kWeaponRadialActions[radial_selection_]);
            result.radial_confirmed=true;
            result.radial_highlight=radial_selection_;
        }
    } else {
        if (neutral) turn_release_required_=false;
        if (!finite_turn || turn_release_required_) result.turn={};
    }
    if (!radial) radial_selection_=-1;
    was_radial_=radial;

    if ((raw.digital_available & (1U<<11))==0) focus_latched_=false;
    if ((raw.digital_available & (1U<<5))==0) crouch_latched_=false;
    if (result.focus && !focus_pressed_) focus_latched_=!focus_latched_;
    if (result.crouch && !crouch_pressed_) crouch_latched_=!crouch_latched_;
    focus_pressed_=result.focus;
    crouch_pressed_=result.crouch;
    result.focus=focus_latched_;
    result.crouch=crouch_latched_;
    return result;
}
} // namespace cojvr::runtime
