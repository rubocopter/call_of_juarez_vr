#pragma once
#include "games/call_of_juarez/gameplay_ui_reader.hpp"
#include "runtime/hud_text.hpp"
namespace cojvr::games::call_of_juarez {
bool CoJNoShootMatchesRay(const CoJNoShootSnapshot& native, int hand,
    runtime::Vec3 origin_cm, runtime::Vec3 direction) noexcept;
// Exact native compass directions enter here; renderer receives metres and
// value-only dial vectors. The two tracking axes come from the proven adapter.
void BuildCoJGameplayUiOverlay(const CoJGameplayUiSnapshot& native,
    const runtime::GameplayInputState& input,const runtime::Pose& head,
    const runtime::Pose& left_grip,runtime::Vec3 world_tracking_right,
    runtime::Vec3 world_tracking_back,runtime::StereoHudTextOverlay& out) noexcept;
// A native permission denial consumes that sector's current gesture. Restoring
// inventory permissions cannot synthesize a new press while it remains held.
class CoJWheelSelectionGate final {
public:
    void Update(runtime::GameplayInputState& input,
        const runtime::EquipmentWheelSnapshot& inventory) noexcept;
private:
    std::uint8_t denied_mask_ = 0;
};
}
