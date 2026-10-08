#pragma once
#include "runtime/vr_types.hpp"

namespace cojvr::runtime {
// These intents share the existing inventory channel order. Native IDs and
// contextual tool identity are exclusively owned by the game adapter.
enum class EquipmentAction : std::uint8_t {
    SelectPistolLeft, SelectPistolRight, SelectRifle, SelectDynamite,
    SelectContextualTool, SelectBow, Hands, ThrowWeapon
};
inline constexpr std::array<EquipmentAction, 8> kWeaponRadialActions{
    EquipmentAction::SelectRifle, EquipmentAction::SelectBow,
    EquipmentAction::SelectPistolRight, EquipmentAction::SelectDynamite,
    EquipmentAction::Hands, EquipmentAction::ThrowWeapon,
    EquipmentAction::SelectPistolLeft, EquipmentAction::SelectContextualTool
};
inline void RequestEquipmentAction(GameplayInputState& state, EquipmentAction action) noexcept {
    const auto channel = static_cast<std::size_t>(action);
    if (channel < state.equipment_select.size()) state.equipment_select[channel] = true;
    else if (action == EquipmentAction::Hands) state.hands = true;
    else if (action == EquipmentAction::ThrowWeapon) state.discard_weapon = true;
}
} // namespace cojvr::runtime
