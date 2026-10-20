#pragma once

#include "runtime/vr_math.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace cojvr::runtime {

// Row-major, column-vector rigid frames; all positions are metres. The adapter
// supplies measured geometry, including the dominant socket's authored roll.
struct TwoHandGripGeometry {
    std::array<float, 12> source_from_dominant{};
    std::array<float, 12> target_from_dominant{};
    Vec3 source_support_m{};
    Vec3 target_support_m{};
};

struct TwoHandGripSolution {
    bool valid = false;
    std::array<float, 12> target_from_source{};
    float source_span_m = 0;
    float target_span_m = 0;
    float support_error_m = 0;
};

namespace two_hand_detail {
inline bool Finite(Vec3 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
inline Vec3 Sub(Vec3 a, Vec3 b) noexcept { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline double Dot(Vec3 a, Vec3 b) noexcept {
    return double(a.x)*b.x + double(a.y)*b.y + double(a.z)*b.z;
}
inline Vec3 Cross(Vec3 a, Vec3 b) noexcept {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
inline bool Unit(Vec3 v, double minimum, Vec3& unit, double& length) noexcept {
    length=std::sqrt(Dot(v,v));
    if (!Finite(v) || !std::isfinite(length) || length<=minimum ||
        length>(std::numeric_limits<float>::max)()) return false;
    unit={float(v.x/length),float(v.y/length),float(v.z/length)};
    return true;
}
inline bool SpanFrame(Vec3 span, Vec3 up, std::array<Vec3,3>& axes,
                      double& length) noexcept {
    // Numerical degeneracy limits, not game reach or contact thresholds.
    if (!Unit(span,1.e-5,axes[0],length)) return false;
    const double along=Dot(up,axes[0]);
    const Vec3 projected{float(up.x-along*axes[0].x),
                         float(up.y-along*axes[0].y),
                         float(up.z-along*axes[0].z)};
    double ignored=0;
    if (!Unit(projected,1.e-4,axes[1],ignored)) return false;
    axes[2]=Cross(axes[0],axes[1]);
    return true;
}
} // namespace two_hand_detail

// Takes the authored support span to the tracked span and aligns projected
// dominant up (roll). Neither span is the barrel axis. The dominant socket is
// anchored exactly; unequal spans remain unequal and their residual is exposed.
// Apply this SAME map to mesh, both hand frames, muzzle and other consumers.
// Failure returns an invalid, empty value, never a partially usable transform.
[[nodiscard]] inline TwoHandGripSolution SolveTwoHandGrip(
    const TwoHandGripGeometry& in) noexcept {
    using namespace two_hand_detail;
    const auto source=PoseFromRigidTransform3x4(in.source_from_dominant);
    const auto target=PoseFromRigidTransform3x4(in.target_from_dominant);
    if (!source.position_valid || !source.orientation_valid ||
        !target.position_valid || !target.orientation_valid ||
        !Finite(in.source_support_m) || !Finite(in.target_support_m)) return {};
    std::array<Vec3,3> a{},b{};
    double source_span=0,target_span=0;
    if (!SpanFrame(Sub(in.source_support_m,source.position),
                   {in.source_from_dominant[1],in.source_from_dominant[5],in.source_from_dominant[9]},
                   a,source_span) ||
        !SpanFrame(Sub(in.target_support_m,target.position),
                   {in.target_from_dominant[1],in.target_from_dominant[5],in.target_from_dominant[9]},
                   b,target_span)) return {};
    // R = target_span_frame * transpose(source_span_frame).
    const std::array<Vec3,3> source_rows{{{a[0].x,a[1].x,a[2].x},
                                        {a[0].y,a[1].y,a[2].y},
                                        {a[0].z,a[1].z,a[2].z}}};
    const std::array<Vec3,3> target_rows{{{b[0].x,b[1].x,b[2].x},
                                        {b[0].y,b[1].y,b[2].y},
                                        {b[0].z,b[1].z,b[2].z}}};
    TwoHandGripSolution out{};
    auto& m=out.target_from_source;
    for (std::size_t row=0;row<3;++row)
        for (std::size_t col=0;col<3;++col)
            m[row*4+col]=float(Dot(target_rows[row],source_rows[col]));
    m[3]=float(double(target.position.x)-Dot({m[0],m[1],m[2]},source.position));
    m[7]=float(double(target.position.y)-Dot({m[4],m[5],m[6]},source.position));
    m[11]=float(double(target.position.z)-Dot({m[8],m[9],m[10]},source.position));
    const auto pose=PoseFromRigidTransform3x4(m);
    if (!pose.position_valid || !pose.orientation_valid) return {};
    out.source_span_m=float(source_span);
    out.target_span_m=float(target_span);
    out.support_error_m=float(std::fabs(target_span-source_span));
    out.valid=true;
    return out;
}

enum class TwoHandGripStage { free, attached, release_required };

struct TwoHandGripInput {
    // Nonzero opaque owner identifies player, item, model and native context.
    // The adapter changes it whenever any of those ownership boundaries change.
    std::uint64_t owner = 0, input_generation = 0;
    std::uint64_t sequence = 0, sample_time_ms = 0, now_ms = 0;
    // eligible is the adapter's retained-owner gate, including reload/equip/UI,
    // fire/cycle permissions, geometry/write/restore health and separation bounds.
    // It must not simply reuse a presentation-only idle/finger predicate.
    bool enabled = false, eligible = false, tracking_valid = false;
    bool recentered = false, contact_qualified = false;
    bool grip_available = false, grip_held = false;
    bool support_trigger_available = false, support_trigger_held = false;
    float contact_distance_m = 0, acquisition_radius_m = 0;
};

struct TwoHandGripOutput {
    TwoHandGripStage stage = TwoHandGripStage::free;
    bool acquired = false, released = false, cancelled = false;
    // Consume the release sample too. Filter both native actions and any held
    // interaction reference before dispatch; do not synthesize a deferred edge.
    bool consume_grip = false, consume_support_trigger = false;
};

// Keep one instance bound to the same physical support-controller channels.
// A new weapon/context owner does not rebind an outstanding physical release
// barrier to the other hand. The adapter arbitrates handedness before routing.
class TwoHandGripPolicy final {
public:
    // One sequence identifies an entire input/pose observation. Re-delivery
    // cannot renew its age, acquire, or confirm release. Use a monotonic clock.
    [[nodiscard]] TwoHandGripOutput Update(const TwoHandGripInput& in) noexcept {
        TwoHandGripOutput out{};
        const bool changed=have_owner_ &&
            (in.owner!=owner_ || in.input_generation!=input_generation_);
        if (!have_owner_ || changed) {
            owner_=in.owner; input_generation_=in.input_generation;
            have_owner_=true; have_sample_=false; grip_released_=false;
        }
        const bool clock_regressed=have_now_ && in.now_ms<last_now_;
        if (!have_now_ || in.now_ms>=last_now_) last_now_=in.now_ms;
        have_now_=true;
        bool fresh=in.sequence && !clock_regressed && in.now_ms>=in.sample_time_ms &&
            in.now_ms-in.sample_time_ms<=kMaxAgeMs;
        const bool duplicate=have_sample_ && in.sequence==sequence_;
        if (have_sample_) {
            fresh=fresh && in.sequence>=sequence_ && in.sample_time_ms>=sample_time_ &&
                (duplicate ? in.sample_time_ms==sample_time_ : in.sample_time_ms>sample_time_);
        }
        const bool gap=fresh && have_sample_ && !duplicate &&
            in.sample_time_ms-sample_time_>kMaxGapMs;
        // Keep the latest admitted watermark even on a gap; old samples cannot
        // become new edges. A replacement input generation may restart sequence.
        if (fresh && !duplicate) {
            sequence_=in.sequence; sample_time_=in.sample_time_ms; have_sample_=true;
        }
        const bool available=in.grip_available && in.support_trigger_available;
        const bool allowed=in.enabled && in.eligible && in.owner && in.tracking_valid &&
            in.contact_qualified && !in.recentered && available;
        const bool interrupted=changed || !fresh || gap || !allowed;
        if (interrupted) {
            grip_released_=false;
            if (stage_==TwoHandGripStage::attached) {
                stage_=TwoHandGripStage::release_required;
                out.cancelled=true;
            }
        }
        // Snapshot consumption BEFORE discharging barriers. The actual release
        // belongs to the claim too, even when disabled or no longer eligible.
        out.consume_grip=stage_!=TwoHandGripStage::free;
        out.consume_support_trigger=stage_==TwoHandGripStage::attached || trigger_release_required_;
        if (fresh && !duplicate && !gap && !changed) {
            if (stage_==TwoHandGripStage::release_required &&
                in.grip_available && !in.grip_held) {
                stage_=TwoHandGripStage::free;
            }
            if (stage_!=TwoHandGripStage::attached && in.support_trigger_available &&
                !in.support_trigger_held) trigger_release_required_=false;
            if (!interrupted) {
                if (stage_==TwoHandGripStage::attached) {
                    if (!in.grip_held) {
                        stage_=TwoHandGripStage::free;
                        out.released=true;
                        grip_released_=true;
                        // Trigger release on the detach sample discharges it;
                        // an unavailable or still-held trigger remains consumed.
                        if (!in.support_trigger_held) trigger_release_required_=false;
                    }
                } else if (!in.grip_held) {
                    grip_released_=true;
                } else {
                    const bool near=std::isfinite(in.contact_distance_m) &&
                        std::isfinite(in.acquisition_radius_m) && in.contact_distance_m>=0 &&
                        in.acquisition_radius_m>0 && in.contact_distance_m<=in.acquisition_radius_m;
                    if (grip_released_ && near) {
                        stage_=TwoHandGripStage::attached;
                        trigger_release_required_=true;
                        out.acquired=out.consume_grip=out.consume_support_trigger=true;
                    }
                    // An outside press is never queued; the whole hold remains
                    // ordinary interaction even if it later enters the contact.
                    grip_released_=false;
                }
            }
        }
        out.stage=stage_;
        return out;
    }
private:
    static constexpr std::uint64_t kMaxAgeMs=100, kMaxGapMs=100;
    TwoHandGripStage stage_=TwoHandGripStage::free;
    bool grip_released_=false, trigger_release_required_=false;
    bool have_owner_=false, have_sample_=false, have_now_=false;
    std::uint64_t owner_=0, input_generation_=0, sequence_=0, sample_time_=0, last_now_=0;
};

} // namespace cojvr::runtime
