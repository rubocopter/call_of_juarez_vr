#pragma once
#include "runtime/vr_types.hpp"

namespace cojvr::runtime {

// A logical utility layer: physical paths remain in the runtime binding asset,
// and equipment channel meanings remain in the game adapter. No native IDs here.
class GameplayUtilityMapper final {
public:
    [[nodiscard]] GameplayInputState Update(const GameplayInputState& raw,
        bool gameplay_context_allowed = true) noexcept;
private:
    std::uint32_t release_required_ = 0xFFFFFFFFU;
    std::uint32_t utility_consumed_ = 0;
    std::uint32_t previous_held_ = 0;
    bool was_utility_ = false;
    bool utility_armed_ = false;
    bool focus_latched_ = false;
    bool focus_gesture_held_ = false;
    bool turn_release_required_ = true;
    bool radial_ready_ = false;
    int radial_selection_ = -1;
    std::uint64_t input_context_generation_ = 0;
};

} // namespace cojvr::runtime
