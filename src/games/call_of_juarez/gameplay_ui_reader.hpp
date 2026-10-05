#pragma once
#include "runtime/gameplay_ui.hpp"

namespace cojvr::games::call_of_juarez {
struct CoJCompassWaypoint {
    runtime::UiLabel label{};
    runtime::Vec3 position_cm{};
    // Native rotor convention: forward is 90 degrees; increasing angles turn
    // towards the native left. Read after the game's own HUD update.
    float angle_degrees = 0;
    bool visible = false;
};
struct CoJGameplayUiSnapshot {
    runtime::EquipmentWheelSnapshot inventory{};
    runtime::WristStatusSnapshot status{};
    bool compass_valid = false;
    bool compass_visible = false;
    float map_angle_degrees = 0;
    runtime::Vec3 player_position_cm{};
    runtime::Vec3 player_forward{};
    // Shipped m_vPlayerRight is filled from GetLeftVector and later flattened
    // to (-forward.z,0,forward.x) by CalculateWaypointPos.
    runtime::Vec3 player_left{};
    std::array<CoJCompassWaypoint, 16> waypoints{};
    std::uint32_t waypoint_count = 0;
};
// Caller must enter through the exact-build game-owner bridge. This routine
// never invokes gameplay updates/selection or exports JNI references.
// Inventory availability is ownership plus observed native permission gates;
// the native selection consumer retains transient/ammunition eligibility.
bool ReadCoJGameplayUi(void* env, void* player, CoJGameplayUiSnapshot& out) noexcept;
struct CoJNoShootSnapshot {
    runtime::Vec3 trace_start_cm{}, trace_end_cm{};
    float age_seconds = 0;
    int hand = -1, reason = -1;
    bool valid = false, warning_visible = false;
};
// Fresh native presentation/trace observation; no cached JNI references and no
// update/trace replay. The trace producer tags its natural weapon owner.
bool ReadCoJNoShoot(void* env, void* player, int hand, CoJNoShootSnapshot& out) noexcept;
} // namespace cojvr::games::call_of_juarez
