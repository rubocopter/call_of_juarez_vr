#pragma once
#include "games/call_of_juarez/gameplay_ui_reader.hpp"
#include "runtime/hud_text.hpp"
namespace cojvr::games::call_of_juarez {
enum class CoJNoShootRayReason : std::uint8_t {
    matched, unavailable, hidden, hand_mismatch, unsupported_reason,
    stale_trace, invalid_geometry, origin_drift, direction_drift
};
struct CoJNoShootRayAssessment {
    CoJNoShootRayReason reason = CoJNoShootRayReason::unavailable;
    float origin_drift_cm = -1;
    float direction_cosine = -2;
};
CoJNoShootRayAssessment AssessCoJNoShootRay(const CoJNoShootSnapshot& native, int hand,
    runtime::Vec3 origin_cm, runtime::Vec3 direction) noexcept;
const char* CoJNoShootRayReasonName(CoJNoShootRayReason reason) noexcept;
bool CoJNoShootMatchesRay(const CoJNoShootSnapshot& native, int hand,
    runtime::Vec3 origin_cm, runtime::Vec3 direction) noexcept;
// Exact native compass directions enter here; renderer receives metres and
// value-only dial vectors. The two tracking axes come from the proven adapter.
void BuildCoJGameplayUiOverlay(const CoJGameplayUiSnapshot& native,
    const runtime::GameplayInputState& input,const runtime::Pose& head,
    const runtime::Pose& left_grip,runtime::Vec3 world_tracking_right,
    runtime::Vec3 world_tracking_back,runtime::StereoHudTextOverlay& out) noexcept;
// A denied highlight stays denied until center/another sector. Confirmation on
// Triangle release still checks native inventory; highlights never dispatch.
class CoJWheelSelectionGate final {
public:
    void Update(runtime::GameplayInputState& input,
        const runtime::EquipmentWheelSnapshot& inventory) noexcept;
private:
    std::uint8_t denied_mask_ = 0;
};
}
