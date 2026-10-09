#pragma once

#include "runtime/vr_types.hpp"

#include <algorithm>
#include <cstdint>
#include <array>
#include <cmath>
#include <optional>

namespace cojvr::runtime {

// All poses share one XR reference space (+X right, +Y up, -Z forward,
// metres). The adapter owns eligibility and must invalidate it or change a
// token on recenter/context loss. Identities are opaque stable owner tokens.
struct MotionReloadGestureInput {
    bool valid = false;
    bool eligible = false;
    std::uint64_t player_identity = 0;
    std::uint64_t weapon_identity = 0;
    std::uint8_t armed_hand = 0; // 0 right, 1 left; opposite hand supplies trigger.
    Pose head{};
    Pose left_grip{};
    Pose right_grip{};
    // Native hand convention: index 0 right, index 1 left. Both are needed
    // to release the original claim after eligibility/armed-hand changes.
    std::array<bool, 2> trigger_available{};
    std::array<bool, 2> trigger_held{};
    std::uint64_t input_generation = 0;
    std::uint64_t pose_sequence = 0;
    std::uint64_t monotonic_ms = 0;
    // Exact-game adapters may replenish a presentation-only cartridge after
    // native code has proven that the previous round entered and completed its
    // reload interval. This never implies ammunition ownership or reload intent.
    bool replenish_cartridge = false;
    // Adapter-authored socket and cartridge-tip poses in the same XR space/metres.
    // Local +Z on both poses points INTO the socket. The adapter owns tip offsets,
    // displayed socket motion, geometry and eligibility; no model is inferred here.
    // Supplying a target requires a valid tip. Absence keeps legacy grip semantics.
    // Target presence cannot change during a claimed gesture. Explicit insertion
    // requires a rear approach (3-12cm), <=2cm lateral error, axes within 30deg,
    // held entry into 0-4cm depth, then release there. Fresh consecutive samples
    // bound tip/socket steps to 12cm, 3m/s and 30deg per sample; tip travel contributes
    // at least 1cm inward to entry. These are UX assumptions, not chamber physics.
    // Recenter/socket-owner changes must invalidate eligibility or change a token.
    std::optional<Pose> insertion_target{};
    std::optional<Pose> cartridge_tip{};
};

struct MotionReloadGestureOutput {
    bool reload = false; // One sample only; never queued for later delivery.
    bool consume_free_trigger = false; // True exactly when the mask is nonzero.
    // Bit 0 right, bit 1 left. Suppress this physical input, not the current
    // opposite hand. A cancelled claim may now be the newly armed hand.
    std::uint8_t consumed_trigger_mask = 0;
    // Presentation only: no native ammunition/state ownership. Position is
    // the last fresh support grip, meaningful only while cartridge_held.
    bool cartridge_held = false;
    Vec3 cartridge_position{};
    // 0 right, 1 left, 2 unclaimed. Includes the consumed release sample and
    // cancelled physical claims even when no cartridge is presented.
    std::uint8_t claimed_hand = 2;
    // Presentation hand is independent from physical trigger ownership so a
    // replenished cartridge can sit in the support hand before a fresh press.
    std::uint8_t cartridge_hand = 2;
    bool insertion_ready = false; // A held cartridge's release would qualify now.
    bool insertion_zone_entered = false; // Fresh event, hysteresis prevents jitter pulses.
    bool insertion_zone_exited = false;
};

enum class MotionReloadGestureStage : std::uint8_t {
    awaiting_release,
    ready,
    carrying,
    cancel_held, // Original physical claim awaits a fresh available release.
    replenished_ready,
    replenished_carrying,
};

// Gesture radii/times are interaction UX defaults, not game physics.
// Keep this instance across eligibility/context loss; feed invalid samples
// rather than reconstructing it and discarding a physical release barrier.
class MotionReloadGesture {
public:
    [[nodiscard]] MotionReloadGestureOutput Update(
        const MotionReloadGestureInput& input) noexcept {
        const bool regression = have_sample_ &&
            (input.pose_sequence < sequence_ || input.monotonic_ms < observed_ms_);
        const bool gap = have_sample_ && input.monotonic_ms >= fresh_ms_ &&
            input.monotonic_ms - fresh_ms_ > 250;
        const bool fresh = !have_sample_ || input.pose_sequence > sequence_;
        const bool owner_changed = have_owner_ &&
            (input.player_identity != player_ || input.weapon_identity != weapon_ ||
             input.armed_hand != armed_ || input.input_generation != generation_);

        // Duplicate calls do not move the freshness deadline or trigger edges.
        // Faults still cancel on duplicate samples (including time/owner faults).
        if (!regression) observed_ms_ = input.monotonic_ms;
        if (fresh && !regression) {
            have_sample_ = true;
            sequence_ = input.pose_sequence;
            fresh_ms_ = input.monotonic_ms;
        }
        Vec3 source{};
        const bool usable = input.valid && input.eligible &&
            input.player_identity != 0 && input.weapon_identity != 0 &&
            input.armed_hand < 2 && ValidPose(input.head) &&
            ValidPose(input.left_grip) && ValidPose(input.right_grip) &&
            (!input.insertion_target || (ValidPose(*input.insertion_target) &&
                input.cartridge_tip && ValidPose(*input.cartridge_tip))) &&
            SourceCenter(input, source);
        const bool free_available = input.armed_hand < 2 &&
            input.trigger_available[1U - input.armed_hand];
        const bool insertion_mode_changed = claimed_hand_ < 2 &&
            explicit_insertion_ != input.insertion_target.has_value();
        if (regression || gap || owner_changed || insertion_mode_changed || !usable || !free_available ||
            ((stage_ == MotionReloadGestureStage::carrying ||
              stage_ == MotionReloadGestureStage::replenished_carrying) &&
             input.monotonic_ms >= started_ms_ && input.monotonic_ms - started_ms_ > 5000)) {
            stage_ = claimed_hand_ < 2 ? MotionReloadGestureStage::cancel_held
                                      : MotionReloadGestureStage::awaiting_release;
            cartridge_hand_ = 2;
            replenished_press_ready_ = false;
        }
        if (fresh && !regression) {
            have_owner_ = true;
            player_ = input.player_identity;
            weapon_ = input.weapon_identity;
            armed_ = input.armed_hand;
            generation_ = input.input_generation;
        }

        if (stage_ == MotionReloadGestureStage::cancel_held) {
            const auto output = CurrentOutput();
            // No eligibility/pose/armed-hand inference can release a physical
            // claim. Only the claimed input's own fresh available release can.
            if (fresh && !regression && !gap &&
                input.trigger_available[claimed_hand_] && !input.trigger_held[claimed_hand_]) {
                claimed_hand_ = 2;
                stage_ = MotionReloadGestureStage::awaiting_release;
                if (usable && free_available && !input.trigger_held[1U - input.armed_hand])
                    stage_ = MotionReloadGestureStage::ready;
            }
            return output;
        }
        if (!fresh || regression || gap || !usable || !free_available) return CurrentOutput();

        const auto free_hand = static_cast<std::uint8_t>(1U - input.armed_hand);
        const bool held = input.trigger_held[free_hand];
        const Vec3 support = free_hand == 0 ? input.right_grip.position : input.left_grip.position;
        const Vec3 armed = input.armed_hand == 0 ? input.right_grip.position : input.left_grip.position;
        if (input.replenish_cartridge && claimed_hand_ >= 2 &&
            stage_ != MotionReloadGestureStage::carrying &&
            stage_ != MotionReloadGestureStage::replenished_carrying) {
            cartridge_hand_ = free_hand;
            cartridge_position_ = support;
            replenished_press_ready_ = !held;
            stage_ = MotionReloadGestureStage::replenished_ready;
            return CurrentOutput();
        }
        if (stage_ == MotionReloadGestureStage::replenished_ready) {
            cartridge_hand_ = free_hand;
            cartridge_position_ = support;
            if (!held) {
                replenished_press_ready_ = true;
                return CurrentOutput();
            }
            if (!replenished_press_ready_) return CurrentOutput();
            claimed_hand_ = free_hand;
            replenished_press_ready_ = false;
            started_ms_ = input.monotonic_ms;
            BeginInsertion(input);
            stage_ = MotionReloadGestureStage::replenished_carrying;
            return CurrentOutput();
        }
        if (stage_ == MotionReloadGestureStage::replenished_carrying) {
            cartridge_position_ = support;
            const bool inserted = explicit_insertion_ && ObserveInsertion(input, held);
            auto output = CurrentOutput();
            QualifyInsertion(output,input,support,armed,held);
            if (!held) {
                output.reload = explicit_insertion_ ? inserted :
                    DistanceSquared(support, armed) <= 0.14 * 0.14;
                claimed_hand_ = 2;
                replenished_press_ready_ = true;
                if (output.reload) {
                    output.cartridge_held = false;
                    output.cartridge_position = {};
                    output.cartridge_hand = 2;
                    cartridge_hand_ = 2;
                    stage_ = MotionReloadGestureStage::ready;
                } else {
                    stage_ = MotionReloadGestureStage::replenished_ready;
                }
            }
            return output;
        }
        if (stage_ == MotionReloadGestureStage::awaiting_release) {
            if (!held) stage_ = MotionReloadGestureStage::ready;
            return {};
        }
        if (stage_ == MotionReloadGestureStage::ready) {
            if (!held) return {};
            // A press outside the zone must be released before entering it.
            stage_ = MotionReloadGestureStage::awaiting_release;
            if (static_cast<double>(input.head.position.y) - support.y >= 0.30 &&
                DistanceSquared(support, source) <= 0.20 * 0.20) {
                claimed_hand_ = free_hand;
                cartridge_hand_ = free_hand;
                source_ = source;
                start_support_ = support;
                cartridge_position_ = support;
                started_ms_ = input.monotonic_ms;
                left_source_ = false;
                BeginInsertion(input);
                stage_ = MotionReloadGestureStage::carrying;
                return CurrentOutput();
            }
            return {};
        }
        left_source_ = left_source_ || DistanceSquared(support, source_) > 0.28 * 0.28;
        cartridge_position_ = support;
        const bool inserted = explicit_insertion_ && ObserveInsertion(input, held);
        auto output = CurrentOutput();
        const bool journey=left_source_&&DistanceSquared(support,start_support_)>=0.20*0.20&&
            input.monotonic_ms-started_ms_>=100;
        QualifyInsertion(output,input,support,armed,held&&journey);
        if (!held) {
            output.reload = left_source_ &&
                DistanceSquared(support, start_support_) >= 0.20 * 0.20 &&
                (explicit_insertion_ ? inserted : DistanceSquared(support, armed) <= 0.14 * 0.14) &&
                input.monotonic_ms - started_ms_ >= 100;
            output.cartridge_held = false;
            output.cartridge_position = {};
            output.cartridge_hand = 2;
            claimed_hand_ = 2;
            cartridge_hand_ = 2;
            stage_ = MotionReloadGestureStage::ready;
        }
        return output;
    }

    [[nodiscard]] MotionReloadGestureStage stage() const noexcept {
        return stage_;
    }

private:
    void QualifyInsertion(MotionReloadGestureOutput& output,const MotionReloadGestureInput& input,
        Vec3 support,Vec3 armed,bool qualified) noexcept {
        output.insertion_ready=qualified&&(explicit_insertion_?
            insertion_inside_&&entered_&&!trajectory_fault_:DistanceSquared(support,armed)<=0.14*0.14);
        // A deliberate withdrawal, not millimetre jitter at the acceptance
        // radius, permits a second entry cue on this same physical press.
        const bool outside=explicit_insertion_?
            DistanceSquared(input.cartridge_tip->position,input.insertion_target->position)>0.08*0.08:
            DistanceSquared(support,armed)>0.18*0.18;
        output.insertion_zone_exited=outside&&zone_notified_;
        if(outside)zone_notified_=false;
        output.insertion_zone_entered=output.insertion_ready&&!zone_notified_;
        if(output.insertion_zone_entered)zone_notified_=true;
    }
    static Vec3 Axis(const Pose& pose) noexcept {
        const auto& q = pose.orientation;
        const double n = static_cast<double>(q.x)*q.x + static_cast<double>(q.y)*q.y +
            static_cast<double>(q.z)*q.z + static_cast<double>(q.w)*q.w;
        return {static_cast<float>(2.0*(static_cast<double>(q.x)*q.z + static_cast<double>(q.w)*q.y)/n),
            static_cast<float>(2.0*(static_cast<double>(q.y)*q.z - static_cast<double>(q.w)*q.x)/n),
            static_cast<float>(1.0-2.0*(static_cast<double>(q.x)*q.x + static_cast<double>(q.y)*q.y)/n)};
    }

    static double Dot(Vec3 a, Vec3 b) noexcept {
        return static_cast<double>(a.x)*b.x + static_cast<double>(a.y)*b.y + static_cast<double>(a.z)*b.z;
    }

    static Vec3 Difference(Vec3 a, Vec3 b) noexcept {
        return {a.x-b.x, a.y-b.y, a.z-b.z};
    }

    void BeginInsertion(const MotionReloadGestureInput& input) noexcept {
        zone_notified_=false;
        explicit_insertion_ = input.insertion_target.has_value();
        approach_ = entered_ = trajectory_fault_ = false;
        inward_travel_ = 0.0;
        if (!explicit_insertion_) return;
        previous_tip_ = input.cartridge_tip->position;
        previous_target_ = input.insertion_target->position;
        previous_target_axis_ = Axis(*input.insertion_target);
        previous_tip_axis_ = Axis(*input.cartridge_tip);
        insertion_ms_ = input.monotonic_ms;
        (void)ObserveInsertion(input, true);
    }

    [[nodiscard]] bool ObserveInsertion(const MotionReloadGestureInput& input, bool held) noexcept {
        const auto& target = *input.insertion_target;
        const auto& tip = *input.cartridge_tip;
        const Vec3 axis = Axis(target);
        const Vec3 relative = Difference(tip.position, target.position);
        const double depth = Dot(relative, axis);
        const double lateral_squared = Dot(relative, relative) - depth*depth;
        const bool aligned = Dot(axis, Axis(tip)) >= 0.866025403784;
        const bool corridor = aligned && lateral_squared <= 0.02*0.02;
        const bool inside = corridor && depth >= 0.0 && depth <= 0.04;
        insertion_inside_=inside;
        const auto elapsed = input.monotonic_ms - insertion_ms_;
        const double bound = std::min(0.12, 0.003*static_cast<double>(elapsed));
        const Vec3 travel = Difference(tip.position, previous_tip_);
        const double previous_depth = Dot(Difference(previous_tip_, previous_target_), axis);
        if (depth < 0.0) entered_ = false;
        if (DistanceSquared(tip.position, previous_tip_) > bound*bound ||
            DistanceSquared(target.position, previous_target_) > bound*bound ||
            Dot(axis, previous_target_axis_) < 0.866025403784 ||
            Dot(Axis(tip), previous_tip_axis_) < 0.866025403784)
            trajectory_fault_ = true; // A discontinuity invalidates the entire press.
        if (!corridor || depth < -0.12 || depth > 0.04) {
            approach_ = entered_ = false;
            inward_travel_ = 0.0;
        } else if (held && !trajectory_fault_) {
            if (approach_) inward_travel_ = std::max(0.0, inward_travel_ + Dot(travel, axis));
            if (depth <= -0.03) approach_ = true;
            if (inside && previous_depth < 0.0 && approach_ && inward_travel_ >= 0.01 &&
                Dot(travel, axis) > 0.0) entered_ = true;
        }
        previous_tip_ = tip.position;
        previous_target_ = target.position;
        previous_target_axis_ = axis;
        previous_tip_axis_ = Axis(tip);
        insertion_ms_ = input.monotonic_ms;
        return !held && inside && entered_ && !trajectory_fault_;
    }

    [[nodiscard]] static bool ValidPose(const Pose& pose) noexcept {
        const auto& p = pose.position;
        const auto& q = pose.orientation;
        if (!pose.position_valid || !pose.orientation_valid ||
            !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
            !std::isfinite(q.x) || !std::isfinite(q.y) ||
            !std::isfinite(q.z) || !std::isfinite(q.w)) return false;
        return static_cast<double>(q.x) * q.x + static_cast<double>(q.y) * q.y +
            static_cast<double>(q.z) * q.z + static_cast<double>(q.w) * q.w > 1.0e-12;
    }

    [[nodiscard]] static double DistanceSquared(Vec3 a, Vec3 b) noexcept {
        const double x = static_cast<double>(a.x) - b.x;
        const double y = static_cast<double>(a.y) - b.y;
        const double z = static_cast<double>(a.z) - b.z;
        return x * x + y * y + z * z;
    }

    [[nodiscard]] static bool SourceCenter(
        const MotionReloadGestureInput& input, Vec3& center) noexcept {
        const auto& q = input.head.orientation;
        const double norm = static_cast<double>(q.x) * q.x + static_cast<double>(q.y) * q.y +
            static_cast<double>(q.z) * q.z + static_cast<double>(q.w) * q.w;
        // Rotate local -Z, then normalize its horizontal projection. Roll and
        // pitch cannot tilt/shorten the planar waist offsets. Vertical forward
        // has no trustworthy yaw and fails closed instead of inventing one.
        double x = -2.0 * (static_cast<double>(q.x) * q.z + static_cast<double>(q.w) * q.y) / norm;
        double z = -1.0 + 2.0 * (static_cast<double>(q.x) * q.x + static_cast<double>(q.y) * q.y) / norm;
        const double length = std::sqrt(x * x + z * z);
        if (!std::isfinite(length) || length < 1.0e-6) return false;
        x /= length;
        z /= length;
        const double side = input.armed_hand == 0 ? -0.20 : 0.20;
        center = {
            static_cast<float>(input.head.position.x - side * z + 0.10 * x),
            static_cast<float>(input.head.position.y - 0.55),
            static_cast<float>(input.head.position.z + side * x + 0.10 * z)};
        return std::isfinite(center.x) && std::isfinite(center.y) && std::isfinite(center.z);
    }

    [[nodiscard]] MotionReloadGestureOutput CurrentOutput() const noexcept {
        MotionReloadGestureOutput output{};
        if (claimed_hand_ < 2) {
            output.consume_free_trigger = true;
            output.consumed_trigger_mask = static_cast<std::uint8_t>(1U << claimed_hand_);
            output.claimed_hand = claimed_hand_;
        }
        output.cartridge_held = stage_ == MotionReloadGestureStage::carrying ||
            stage_ == MotionReloadGestureStage::replenished_ready ||
            stage_ == MotionReloadGestureStage::replenished_carrying;
        if (output.cartridge_held) output.cartridge_position = cartridge_position_;
        if (output.cartridge_held) output.cartridge_hand = cartridge_hand_;
        return output;
    }

    MotionReloadGestureStage stage_ = MotionReloadGestureStage::awaiting_release;
    std::uint8_t claimed_hand_ = 2;
    bool have_sample_ = false;
    bool have_owner_ = false;
    bool left_source_ = false;
    bool replenished_press_ready_ = false;
    std::uint64_t sequence_ = 0;
    std::uint64_t fresh_ms_ = 0;
    std::uint64_t observed_ms_ = 0;
    std::uint64_t player_ = 0;
    std::uint64_t weapon_ = 0;
    std::uint64_t generation_ = 0;
    std::uint8_t armed_ = 0;
    std::uint8_t cartridge_hand_ = 2;
    std::uint64_t started_ms_ = 0;
    double inward_travel_ = 0.0;
    bool explicit_insertion_ = false;
    bool approach_ = false;
    bool entered_ = false;
    bool trajectory_fault_ = false;
    bool insertion_inside_ = false;
    bool zone_notified_ = false;
    Vec3 previous_tip_{};
    Vec3 previous_target_{};
    Vec3 previous_target_axis_{};
    Vec3 previous_tip_axis_{};
    std::uint64_t insertion_ms_ = 0;
    Vec3 source_{};
    Vec3 start_support_{};
    Vec3 cartridge_position_{};
};

} // namespace cojvr::runtime
