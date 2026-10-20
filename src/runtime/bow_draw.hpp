#pragma once

#include "runtime/vr_types.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cojvr::runtime {

// Ergonomic UX defaults, not measurements of an authored bow or body mount.
struct BowDrawParameters {
    float capture_radius_m = .12F;
    float min_draw_m = .12F;
    float max_draw_m = .55F; // Full-tension saturation, not a reach limit.
    float max_lateral_m = .20F; // Shot alignment guard, not a held-grab cancellation radius.
    float max_reach_m = .90F; // Unqualified ergonomic reach guard; not a measured bow dimension.
};

struct BowDrawInput {
    // Adapter admits exact bow/player/context, gameplay and tracked hands.
    // There is deliberately no left-trigger requirement or authored mount here.
    bool eligible = false;
    bool right_trigger_available = false;
    bool right_trigger_held = false;
    bool paused = false;
    bool recentered = false;
    std::uint64_t owner = 0, player = 0, native_context = 0;
    std::uint64_t input_generation = 0, sequence = 0, now_ms = 0;
    // All points are current observations in the SAME metre space. Head position
    // supplies a common moving reference for per-hand continuity checks.
    Vec3 head_position_m{}, left_position_m{}, right_position_m{};
    Vec3 string_point_m{};
    Vec3 aim_forward{}; // Normalized; authored/aim geometry comes from adapter.
};

struct BowDrawHapticIntent {
    bool grabbed = false, tension_step = false, released = false;
    float pulse_strength = 0; // Optional, modest [0,1] intent; owns no device.
};

struct BowDrawResult {
    bool grab = false, drawing = false, release = false, fire = false;
    bool cancelled = false;
    const char* reason = "none";
    float axial_draw_m = 0, lateral_m = 0;
    float normalized_draw = 0;
    BowDrawHapticIntent haptics{};
};

class BowDrawPolicy final {
public:
    explicit BowDrawPolicy(BowDrawParameters parameters = {}) noexcept : parameters_(parameters) {}
    // Returned events are already consumed. Deliver immediately; never queue or
    // retry a native fire/haptic failure. Duplicated poses produce no events.
    [[nodiscard]] BowDrawResult Update(const BowDrawInput& in) noexcept {
        BowDrawResult out{};
        const bool clock_reversed = have_clock_ && in.now_ms < delivery_ms_;
        if (!have_clock_ || in.now_ms >= delivery_ms_) delivery_ms_ = in.now_ms;
        have_clock_ = true;

        const bool identity_valid = in.owner && in.player && in.native_context;
        const bool identity_changed = have_identity_ && identity_valid &&
            (in.owner != owner_ || in.player != player_ || in.native_context != context_ ||
             in.input_generation != input_generation_);
        if (identity_valid && (!have_identity_ || identity_changed)) {
            owner_ = in.owner; player_ = in.player; context_ = in.native_context;
            input_generation_ = in.input_generation;
            have_identity_ = true;
            have_sequence_ = have_motion_ = false;
        }

        const bool repeated = have_sequence_ && in.sequence == sequence_;
        const bool sequence_reversed = have_sequence_ && in.sequence < sequence_;
        const bool stale_pose = have_sequence_ && in.now_ms>=sample_ms_ && in.now_ms-sample_ms_>100;
        const bool time_fault = have_sequence_ &&
            (in.now_ms < sample_ms_ || stale_pose ||
             (!repeated && in.now_ms == sample_ms_));
        const Vec3 left = Sub(in.left_position_m, in.head_position_m);
        const Vec3 right = Sub(in.right_position_m, in.head_position_m);
        const Vec3 relative = Sub(in.right_position_m, in.string_point_m);
        const bool geometry_valid = Finite(in.head_position_m) && Finite(in.left_position_m) &&
            Finite(in.right_position_m) && Finite(in.string_point_m) && Finite(left) &&
            Finite(right) && Finite(relative) && Finite(in.aim_forward) &&
            std::fabs(Dot(in.aim_forward, in.aim_forward) - 1) <= .001;
        const bool fault = !in.eligible || in.paused || in.recentered || !identity_valid ||
            identity_changed || !in.right_trigger_available || !in.sequence || clock_reversed ||
            sequence_reversed || time_fault || !geometry_valid || !ValidParameters();

        // Consume the observation even when it is unusable. Neither a duplicate
        // release nor a failed native effect can turn it into another event.
        if (in.sequence && (!have_sequence_ || in.sequence > sequence_)) {
            sequence_ = in.sequence; sample_ms_ = in.now_ms; have_sequence_ = true;
        }
        if (fault) {
            out.reason = !in.eligible ? "ineligible" : in.paused ? "paused" :
                in.recentered ? "recentered" : (!identity_valid || identity_changed) ? "owner_changed" :
                !in.right_trigger_available ? "trigger_unavailable" :
                (!in.sequence || sequence_reversed) ? "sequence_fault" :
                clock_reversed ? "clock_reversed" : time_fault ?
                    (stale_pose?"pose_stale":"pose_timestamp_repeated") :
                !geometry_valid ? "invalid_geometry" : "invalid_parameters";
            Cancel(out);
            have_motion_ = false;
            return out;
        }
        if (repeated) return Observe();

        const Vec3 aim = Scale(in.aim_forward, 1 / std::sqrt(Dot(in.aim_forward, in.aim_forward)));
        const bool motion_fault = have_motion_ &&
            (Distance(left, previous_left_) > .35 + kEpsilon || Distance(right, previous_right_) > .35 + kEpsilon ||
             Dot(aim, previous_aim_) <= 0);
        const Vec3 right_step = Sub(right, previous_right_);
        const Vec3 previous_aim = previous_aim_;
        previous_left_ = left; previous_right_ = right; previous_aim_ = aim;
        have_motion_ = true;
        if (motion_fault) { out.reason="tracking_jump_or_axis_flip";Cancel(out); return out; }

        if (!drawing_) {
            if (!in.right_trigger_held) {
                released_ = true;
                return out;
            }
            const bool fresh_press = released_;
            released_ = false; // A distant press is spent, even if held near later.
            if (!fresh_press || Distance(in.right_position_m, in.string_point_m) > parameters_.capture_radius_m)
                return out;
            drawing_ = true;
            grab_axial_m_ = -Dot(relative, aim);
            right_draw_m_ = draw_m_ = 0;
            tension_step_ = 0;
            out.grab = out.drawing = out.haptics.grabbed = true;
            out.haptics.pulse_strength = .12F;
            return out;
        }

        // Keep the signed grab offset along the continuously observed axis.
        // It is a distance, never a frozen world vector that aim rotation can
        // project into newly manufactured tension.
        const double axial = -Dot(relative, aim);
        const double relative_draw = axial - grab_axial_m_;
        const Vec3 lateral = Add(relative, Scale(aim, axial));
        out.axial_draw_m=static_cast<float>(relative_draw);
        out.lateral_m=static_cast<float>(Distance(lateral, {}));
        if (relative_draw > parameters_.max_reach_m + kEpsilon) {
            out.reason="overreach";
            Cancel(out);
            return out;
        }
        const Vec3 mid = Add(previous_aim, aim);
        const Vec3 mid_aim = Scale(mid, 1 / std::sqrt(Dot(mid, mid)));
        right_draw_m_ -= Dot(right_step, mid_aim);
        // Current separation from the CURRENT string point, anchored at grab,
        // with independently observed right-hand retraction as an upper bound.
        // Common body lean and moving only the bow cannot create tension.
        draw_m_ = (std::max)(0.0, (std::min)(relative_draw, right_draw_m_));
        draw_m_ = (std::min)(draw_m_, double(parameters_.max_draw_m));
        // A captured string stays held until R2 release or a hard ownership /
        // tracking fault. Slack and continuous sideways movement relax tension;
        // they cannot silently release native alternate fire while R2 is held.
        if (out.lateral_m > parameters_.max_lateral_m + kEpsilon) {
            draw_m_=0;out.reason="off_axis_relaxed";
        } else if (relative_draw<0) out.reason="slack";
        out.normalized_draw = static_cast<float>(draw_m_ / parameters_.max_draw_m);
        if (!in.right_trigger_held) {
            out.release = out.haptics.released = true;
            out.fire = draw_m_ + kEpsilon >= parameters_.min_draw_m;
            out.haptics.pulse_strength = out.fire ? .18F : .08F;
            // Terminal state is consumed BEFORE the adapter sees the intent.
            drawing_ = false; released_ = true; draw_m_ = right_draw_m_ = 0;
            return out;
        }
        out.drawing = true;
        const auto step = static_cast<unsigned>(out.normalized_draw * 10);
        if (step > tension_step_) {
            tension_step_ = step;
            out.haptics.tension_step = true;
            out.haptics.pulse_strength = .05F + .10F * out.normalized_draw;
        }
        return out;
    }

    [[nodiscard]] bool drawing() const noexcept { return drawing_; }
    // No effects are delivered by Reset; the bridge retires any native claim.
    // Startup/recovery always needs an available released observation.
    void Reset() noexcept {
        const auto parameters = parameters_;
        *this = BowDrawPolicy(parameters);
    }

private:
    BowDrawParameters parameters_{};
    bool drawing_ = false, released_ = false, have_identity_ = false;
    bool have_sequence_ = false, have_clock_ = false, have_motion_ = false;
    std::uint64_t owner_ = 0, player_ = 0, context_ = 0, input_generation_ = 0;
    std::uint64_t sequence_ = 0, sample_ms_ = 0, delivery_ms_ = 0;
    Vec3 previous_left_{}, previous_right_{}, previous_aim_{};
    double grab_axial_m_ = 0, right_draw_m_ = 0, draw_m_ = 0;
    unsigned tension_step_ = 0;
    static constexpr double kEpsilon = 1.e-6; // Floating-point boundary tolerance only.

    static bool Finite(Vec3 v) noexcept {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }
    static Vec3 Sub(Vec3 a, Vec3 b) noexcept { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
    static Vec3 Add(Vec3 a, Vec3 b) noexcept { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
    static Vec3 Scale(Vec3 v, double scale) noexcept {
        return {float(v.x*scale), float(v.y*scale), float(v.z*scale)};
    }
    static double Dot(Vec3 a, Vec3 b) noexcept {
        return double(a.x)*b.x + double(a.y)*b.y + double(a.z)*b.z;
    }
    static double Distance(Vec3 a, Vec3 b) noexcept {
        const double x = double(a.x)-b.x, y = double(a.y)-b.y, z = double(a.z)-b.z;
        return std::sqrt(x*x+y*y+z*z);
    }
    bool ValidParameters() const noexcept {
        return std::isfinite(parameters_.capture_radius_m) && parameters_.capture_radius_m > 0 &&
            std::isfinite(parameters_.min_draw_m) && parameters_.min_draw_m > 0 &&
            std::isfinite(parameters_.max_draw_m) && parameters_.max_draw_m >= parameters_.min_draw_m &&
            std::isfinite(parameters_.max_reach_m) && parameters_.max_reach_m >= parameters_.max_draw_m &&
            std::isfinite(parameters_.max_lateral_m) && parameters_.max_lateral_m > 0;
    }
    BowDrawResult Observe() const noexcept {
        BowDrawResult out{};
        out.drawing = drawing_;
        if (drawing_) out.normalized_draw = static_cast<float>(draw_m_ / parameters_.max_draw_m);
        return out;
    }
    void Cancel(BowDrawResult& out) noexcept {
        out.cancelled = drawing_;
        if (drawing_) { out.release = out.haptics.released = true; out.haptics.pulse_strength = .06F; }
        drawing_ = released_ = false;
        draw_m_ = right_draw_m_ = 0;
        tension_step_ = 0;
    }
};

} // namespace cojvr::runtime
