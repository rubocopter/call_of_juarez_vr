#pragma once

#include "runtime/vr_math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cojvr::games::call_of_juarez {

struct CoJDynamiteMotionSample {
    std::uint64_t owner = 0, player = 0, context = 0, input_generation = 0;
    std::uint64_t sequence = 0, time_us = 0;
    runtime::Pose head{}, right{};
    bool allowed = false;
    bool physical_fire = false;
};

inline bool CoJDynamiteMotionControlsClear(const runtime::GameplayInputState& input) noexcept {
    return input.active && (input.digital_available & 3U) == 3U &&
        !input.fire_left && !input.fire_right && !input.reload && !input.interact &&
        !input.weapon_next && !input.weapon_previous && !input.alternate_fire &&
        !input.hands && !input.discard_weapon && !input.weapon_radial && !input.radial_confirmed &&
        !input.objectives && !input.logs && !input.quick_load && !input.quick_save &&
        std::none_of(input.equipment_select.begin(), input.equipment_select.end(),
                     [](bool selected) { return selected; });
}

class CoJDynamiteMotion final {
public:
    void Reset() noexcept { *this = {}; }

    [[nodiscard]] bool Update(const CoJDynamiteMotionSample& sample) noexcept {
        if (!sample.allowed || sample.physical_fire || !sample.owner || !sample.player ||
            !sample.context || !sample.input_generation || !sample.sequence || !sample.time_us ||
            !sample.head.position_valid || !sample.head.orientation_valid ||
            !sample.right.position_valid || !sample.right.orientation_valid) {
            Reset();
            return false;
        }

        const auto& q = sample.head.orientation;
        const auto q_length = std::hypot(std::hypot(q.x,q.y),std::hypot(q.z,q.w));
        const auto hand = Difference(sample.right.position,sample.head.position);
        auto heading = runtime::RotateVector(q,{0,0,-1});
        const float heading_length = std::hypot(heading.x,heading.z);
        if (!std::isfinite(q_length) || q_length < .9F || q_length > 1.1F ||
            !std::isfinite(heading_length) || heading_length < .45F ||
            !std::isfinite(Length(hand)) || Length(hand) > 1.25F) {
            Reset();
            return false;
        }
        heading = {heading.x/heading_length,0,heading.z/heading_length};

        const bool same_owner=sample.owner==owner_ && sample.player==player_ &&
            sample.context==context_ && sample.input_generation==input_generation_;
        const bool stale_time=sample.time_us<time_us_ || sample.time_us-time_us_>110000;
        if (same_owner && sample.sequence==sequence_) {
            if (stale_time) phase_=Phase::await_neutral;
            return false;
        }
        if (!same_owner || sample.sequence<sequence_ || stale_time) {
            Reset();
            owner_=sample.owner; player_=sample.player; context_=sample.context;
            input_generation_=sample.input_generation;
        }
        if (sample.time_us==time_us_) return false;
        const auto elapsed_us = time_us_ ? sample.time_us - time_us_ : 0;
        const auto delta = Difference(hand,hand_);
        const auto controller_delta=Difference(sample.right.position,controller_);
        sequence_=sample.sequence; time_us_=sample.time_us; hand_=hand;
        controller_=sample.right.position;
        const float elapsed_s = static_cast<float>(elapsed_us) / 1000000.0F;
        if (elapsed_us && (Length(delta) > .32F || Length(delta) > 11.0F * elapsed_s))
            phase_=Phase::await_neutral;
        if (phase_ != Phase::await_neutral && Dot(heading,forward_) < .85F)
            phase_=Phase::await_neutral;

        if (phase_ == Phase::await_neutral) {
            if (Dot(hand,heading) >= .12F) {
                forward_=heading;
                phase_=Phase::ready;
            }
            return false;
        }

        const float depth = Dot(hand,forward_);
        const float lateral = Dot(hand,{-forward_.z,0,forward_.x});
        const bool behind = depth <= -.14F && lateral >= .12F &&
            hand.y >= -.65F && hand.y <= .35F;
        if (phase_ == Phase::ready) {
            if (behind) {
                phase_=Phase::behind;
                behind_us_=sample.time_us;
            }
            return false;
        }
        if (sample.time_us - behind_us_ > 520000) {
            phase_=Phase::await_neutral;
            return false;
        }
        if (phase_ == Phase::behind) {
            if (!behind) phase_=Phase::ready;
            else if (sample.time_us - behind_us_ >= 55000) phase_=Phase::armed;
            return false;
        }
        if (depth < .22F) return false;
        phase_=Phase::await_neutral;
        return elapsed_us && lateral >= .02F && Dot(delta,forward_) >= 1.7F * elapsed_s &&
            Dot(controller_delta,forward_) >= 1.7F * elapsed_s;
    }

private:
    enum class Phase { await_neutral, ready, behind, armed };
    static runtime::Vec3 Difference(runtime::Vec3 a, runtime::Vec3 b) noexcept {
        return {a.x-b.x,a.y-b.y,a.z-b.z};
    }
    static float Dot(runtime::Vec3 a, runtime::Vec3 b) noexcept {
        return a.x*b.x+a.y*b.y+a.z*b.z;
    }
    static float Length(runtime::Vec3 v) noexcept { return std::hypot(v.x,v.y,v.z); }

    Phase phase_ = Phase::await_neutral;
    std::uint64_t owner_=0, player_=0, context_=0, input_generation_=0;
    std::uint64_t sequence_=0, time_us_=0, behind_us_=0;
    runtime::Vec3 hand_{}, controller_{}, forward_{};
};

} // namespace cojvr::games::call_of_juarez
