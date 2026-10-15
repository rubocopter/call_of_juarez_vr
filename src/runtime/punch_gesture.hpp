#pragma once
#include "runtime/vr_types.hpp"
#include "runtime/vr_math.hpp"
#include <array>
#include <cstdint>
#include <cmath>
namespace cojvr::runtime {
struct MotionPunchInput {
    bool allowed=false,recentered=false;
    // Delivery time from a high-resolution monotonic source, converted to ms.
    // Coarse system ticks can alias distinct 90 Hz poses and erase the guard.
    // Repeated sequences retain the last unique pose's time inside the policy.
    std::uint64_t owner=0,context=0,sequence=0,now_ms=0;
    std::uint64_t player_generation=0,input_generation=0;
    Pose head{};
    std::array<Pose,2> grip{};
    std::array<bool,2> claim_available{},closed{};
};
enum class MotionPunchStage { blocked,awaiting_release,open,guard,armed,collecting,recovery };
enum class MotionPunchReason { none,not_allowed,recentered,invalid_owner,invalid_head,
    owner_changed,repeated_pose,sequence_regressed,time_discontinuity,head_discontinuity,
    claim_unavailable,invalid_grip,grip_out_of_range,motion_discontinuity,path_limit,prelaunch_expired };
inline const char* MotionPunchStageName(MotionPunchStage stage) noexcept {
    switch(stage){
    case MotionPunchStage::blocked:return "blocked";
    case MotionPunchStage::awaiting_release:return "awaiting_release";
    case MotionPunchStage::open:return "open";
    case MotionPunchStage::guard:return "guard";
    case MotionPunchStage::armed:return "armed";
    case MotionPunchStage::collecting:return "collecting";
    case MotionPunchStage::recovery:return "recovery";
    }return "unknown";
}
inline const char* MotionPunchReasonName(MotionPunchReason reason) noexcept {
    switch(reason){
    case MotionPunchReason::none:return "none";
    case MotionPunchReason::not_allowed:return "not_allowed";
    case MotionPunchReason::recentered:return "recentered";
    case MotionPunchReason::invalid_owner:return "invalid_owner";
    case MotionPunchReason::invalid_head:return "invalid_head";
    case MotionPunchReason::owner_changed:return "owner_changed";
    case MotionPunchReason::repeated_pose:return "repeated_pose";
    case MotionPunchReason::sequence_regressed:return "sequence_regressed";
    case MotionPunchReason::time_discontinuity:return "time_discontinuity";
    case MotionPunchReason::head_discontinuity:return "head_discontinuity";
    case MotionPunchReason::claim_unavailable:return "claim_unavailable";
    case MotionPunchReason::invalid_grip:return "invalid_grip";
    case MotionPunchReason::grip_out_of_range:return "grip_out_of_range";
    case MotionPunchReason::motion_discontinuity:return "motion_discontinuity";
    case MotionPunchReason::path_limit:return "path_limit";
    case MotionPunchReason::prelaunch_expired:return "prelaunch_expired";
    }return "unknown";
}
struct MotionPunchOutput {
    std::array<bool,2> strike{};
    std::array<Vec3,2> start{},end{};
    struct Sweep{std::array<Vec3,64> points{};std::uint8_t count=0;};
    std::array<Sweep,2> sweep{};
    // Value-only observations; never grant admission or a native hit verdict.
    struct Diagnostic {
        MotionPunchStage stage=MotionPunchStage::blocked;
        MotionPunchReason reason=MotionPunchReason::none;
        float speed_mps=0,world_speed_mps=0,travel_m=0,world_travel_m=0,path_length_m=0;
        std::uint8_t points=0;
    };
    std::array<Diagnostic,2> diagnostic{};
    std::uint64_t dt_ms=0;
};
// Feed the state admitted by logical input ownership, never raw button claims.
inline void ApplyMotionPunchClaims(MotionPunchInput& punch,const GameplayInputState& admitted) noexcept{
    punch.allowed=punch.allowed&&admitted.active&&!admitted.weapon_radial;
    punch.closed={admitted.fire_right,admitted.fire_left};
    punch.claim_available={(admitted.digital_available&2U)!=0,(admitted.digital_available&1U)!=0};
    punch.input_generation=admitted.input_context_generation;
}
class MotionPunchGesture {
public:
    // One physical stroke, never an input held for native animation repeat.
    // Endpoints retain measured reference-space grip positions in metres.
    // Admission requires both real grip and head-relative movement: neither
    // head recoil nor shared body translation can invent a hand trajectory.
    MotionPunchOutput Update(const MotionPunchInput& in) noexcept {
        MotionPunchOutput out{};
        out.dt_ms=in.now_ms>=now_?in.now_ms-now_:0;
        if(!in.allowed||in.recentered||!in.owner||!in.sequence||!Valid(in.head)||!ValidHeadRotation(in.head.orientation)){
            const auto reason=!in.allowed?MotionPunchReason::not_allowed:
                in.recentered?MotionPunchReason::recentered:!in.owner?MotionPunchReason::invalid_owner:
                !in.sequence?MotionPunchReason::sequence_regressed:MotionPunchReason::invalid_head;
            for(auto& d:out.diagnostic)d.reason=reason;
            Reset();return out;
        }
        const bool changed=in.owner!=owner_||in.context!=context_||
            in.player_generation!=player_||in.input_generation!=input_;
        if(changed){Reset();owner_=in.owner;context_=in.context;
            for(auto& d:out.diagnostic)d.reason=MotionPunchReason::owner_changed;}
        if(sequence_&&in.sequence<=sequence_){
            // Geometry cannot advance on a repeated pose, but tracking/input
            // loss or a release must still cancel a collected physical stroke.
            for(std::size_t h=0;h<2;++h){
                out.diagnostic[h].reason=MotionPunchReason::repeated_pose;
                if(!in.claim_available[h]||!Valid(in.grip[h])){
                    hands_[h]={};out.diagnostic[h].reason=!in.claim_available[h]?
                        MotionPunchReason::claim_unavailable:!Valid(in.grip[h])?
                        MotionPunchReason::invalid_grip:MotionPunchReason::repeated_pose;
                }else if(!in.closed[h]){
                    // A valid available release is an input observation even
                    // when geometry repeats. Cancel any old stroke, but retain
                    // the confirmed open baseline for a later deliberate close.
                    // Keep the last unique geometry; do not advance its clock.
                    const auto previous=hands_[h].previous;
                    const auto previous_world=hands_[h].previous_world;
                    hands_[h]={};hands_[h].released=true;
                    hands_[h].previous=previous;hands_[h].previous_world=previous_world;
                }}
            // The game can consume one presenter pose more than once. A fresh
            // duplicate is not tracking loss: keep the release/guard/path and
            // its original timestamp, so the next pose measures the full dt.
            // Do not let repeated deliveries extend that freshness window.
            if(in.sequence<sequence_||in.now_ms<now_||in.now_ms-now_>80){
                const auto reason=in.sequence<sequence_?MotionPunchReason::sequence_regressed:
                    MotionPunchReason::time_discontinuity;
                Reset();for(auto& d:out.diagnostic)d.reason=reason;
            }
            Observe(out);
            return out;
        }
        const auto dt=in.now_ms>now_?in.now_ms-now_:0;
        const bool fresh=sequence_&&dt>=5&&dt<=80&&Distance(in.head.position,head_)<=.12F;
        out.dt_ms=dt;
        for(std::size_t h=0;h<2;++h){
            auto& s=hands_[h];
            auto& d=out.diagnostic[h];
            if(!in.claim_available[h]||!Valid(in.grip[h])){s={};
                d.reason=!in.claim_available[h]?MotionPunchReason::claim_unavailable:MotionPunchReason::invalid_grip;
                continue;}
            auto inverse=NormalizeQuaternion(in.head.orientation);
            inverse.x=-inverse.x;inverse.y=-inverse.y;inverse.z=-inverse.z;
            const Vec3 relative=RotateVector(inverse,Sub(in.grip[h].position,in.head.position));
            if(Length(relative)>1.1F){s={};d.reason=MotionPunchReason::grip_out_of_range;continue;}
            if(!fresh){s={};s.previous=relative;s.previous_world=in.grip[h].position;
                if(sequence_)d.reason=dt<5||dt>80?MotionPunchReason::time_discontinuity:
                    MotionPunchReason::head_discontinuity;}
            const float step=Distance(relative,s.previous);
            const float speed=fresh?step*1000.F/static_cast<float>(dt):0.F;
            const float world_step=Distance(in.grip[h].position,s.previous_world);
            const float world_speed=fresh?world_step*1000.F/static_cast<float>(dt):0.F;
            d.speed_mps=speed;d.world_speed_mps=world_speed;
            d.travel_m=Distance(relative,s.anchor);d.world_travel_m=Distance(in.grip[h].position,s.anchor_world);
            if(fresh&&(step>.25F||speed>8.F||world_step>.25F||world_speed>8.F)){
                d.reason=MotionPunchReason::motion_discontinuity;
                s={};s.previous=relative;s.previous_world=in.grip[h].position;continue;}
            if(!in.closed[h]){s={};s.released=true;s.previous=relative;s.previous_world=in.grip[h].position;continue;}
            if(!s.released){s.previous=relative;s.previous_world=in.grip[h].position;continue;}
            // A confirmed available release is sufficient button admission.
            // Closing while moving starts at the last measured open pose.
            if(!s.closed){s.closed=true;s.armed=true;Anchor(s,s.previous,s.previous_world);}
            const Vec3 delta=Sub(in.grip[h].position,s.previous_world);
            const Vec3 local_step=Sub(relative,s.previous);
            if(s.spent){
                // Recovery is signed travel against the completed stroke, not
                // radial retraction or a timed stationary guard. Both spaces
                // must agree so head recoil cannot manufacture recovery.
                QualifyReturn(s,relative,in.grip[h].position);
                // Keep the qualified return direction through pauses and
                // further retraction. Only movement out of that direction
                // starts a new stroke; a perpendicular combination qualifies.
                const bool turned=speed>=.25F&&world_speed>=.25F&&
                    LeavesReturn(local_step,s.return_delta)&&LeavesReturn(delta,s.return_world_delta);
                if(s.returned&&turned){
                    s.spent=false;s.armed=true;
                    // Seed exactly at the return apex. The first segment of
                    // the next attack is current, never a replay of recovery.
                    Anchor(s,s.previous,s.previous_world);
                }
            }
            if(!s.spent){
                const float travel=Distance(relative,s.anchor);
                if(s.swing){
                    const bool reversed=Reversed(delta,s.last_delta);
                    if(reversed){
                        Finish(s,out,h);
                        // The first recoil is recovery evidence even when it
                        // already crosses the return threshold. Never append
                        // it to the completed outgoing sweep.
                        QualifyReturn(s,relative,in.grip[h].position);
                    }else if(!Append(s,in.grip[h].position,relative)){s={};d.reason=MotionPunchReason::path_limit;}
                    else{
                        // Submit the complete physical curve only at its end.
                        // A timer must not truncate a still-moving hook.
                        if(world_speed<.5F)Finish(s,out,h);
                        else if(world_step>=.0001F)s.last_delta=delta;
                    }
                }else if(speed<.25F||world_speed<.25F){Anchor(s,relative,in.grip[h].position);}
                else{
                    if(!s.started_ms)s.started_ms=in.now_ms;
                    if(!Append(s,in.grip[h].position,relative)){s={};d.reason=MotionPunchReason::path_limit;}
                    else{
                        const Vec3 local_delta=Sub(relative,s.anchor);
                        const float lateral=std::sqrt(local_delta.x*local_delta.x+local_delta.y*local_delta.y);
                        // Forward jabs and front-of-head hooks/uppercuts. A
                        // curved hook may gain Z; a predominantly backward
                        // pull cannot become an attack by crossing 8 cm.
                        const bool forward=relative.z<=-.15F&&local_delta.z<0;
                        const bool cross=relative.z<=-.15F&&lateral>=.06F&&lateral>=local_delta.z;
                        if(speed>=.75F&&world_speed>=.75F&&travel>=.08F&&
                            Distance(in.grip[h].position,s.anchor_world)>=.08F&&(forward||cross)){
                            s.swing=true;s.last_delta=delta;
                        }else if(travel>.35F||in.now_ms-s.started_ms>=240){
                            d.reason=MotionPunchReason::prelaunch_expired;
                            Anchor(s,relative,in.grip[h].position);
                        }
                    }
                }
            }
            s.previous=relative;
            s.previous_world=in.grip[h].position;
        }
        owner_=in.owner;context_=in.context;player_=in.player_generation;input_=in.input_generation;
        sequence_=in.sequence;now_=in.now_ms;head_=in.head.position;
        Observe(out);
        return out;
    }
    void Reset() noexcept{hands_={};owner_=context_=sequence_=now_=player_=input_=0;head_={};}
private:
    struct Hand {bool released=false,closed=false,armed=false,spent=false,swing=false,returned=false;
        Vec3 previous{},anchor{},previous_world{},anchor_world{},end_world{},end_relative{},last_delta{},return_delta{},return_world_delta{};
        MotionPunchOutput::Sweep path{};float path_length=0;
        std::uint64_t started_ms=0;};
    void Observe(MotionPunchOutput& out)const noexcept{
        for(std::size_t h=0;h<2;++h){const auto& s=hands_[h];auto& d=out.diagnostic[h];
            d.stage=!s.released?MotionPunchStage::awaiting_release:!s.closed?MotionPunchStage::open:
                s.spent?MotionPunchStage::recovery:s.swing?MotionPunchStage::collecting:
                s.armed?MotionPunchStage::armed:MotionPunchStage::guard;
            d.points=s.path.count;d.path_length_m=s.path_length;
        }
    }
    static void Anchor(Hand& s,Vec3 relative,Vec3 world)noexcept{
        s.anchor=relative;s.anchor_world=world;s.path={};s.path.points[0]=world;s.path.count=1;
        s.path_length=0;s.started_ms=0;s.end_world=world;s.end_relative=relative;s.last_delta={};
    }
    static bool Append(Hand& s,Vec3 world,Vec3 relative)noexcept{
        if(!s.path.count)return false;
        const float step=Distance(world,s.path.points[s.path.count-1]);
        if(step<.0001F)return true;
        if(s.path.count>=s.path.points.size()||s.path_length+step>.8F)return false;
        s.path.points[s.path.count++]=world;s.path_length+=step;
        s.end_world=world;s.end_relative=relative;return true;
    }
    static void Finish(Hand& s,MotionPunchOutput& out,std::size_t hand)noexcept{
        if(s.path.count>=2){out.strike[hand]=true;out.sweep[hand]=s.path;
            out.start[hand]=s.path.points[0];out.end[hand]=s.path.points[s.path.count-1];}
        s.spent=true;s.swing=false;s.armed=false;s.returned=false;s.return_delta={};s.return_world_delta={};
    }
    static void QualifyReturn(Hand& s,Vec3 relative,Vec3 world)noexcept{
        if(s.returned)return;
        const Vec3 stroke=Sub(s.end_relative,s.anchor);
        const Vec3 world_stroke=Sub(s.end_world,s.anchor_world);
        const Vec3 returned=Sub(relative,s.end_relative);
        const Vec3 world_returned=Sub(world,s.end_world);
        if(Dot(returned,stroke)<=-.08F*Length(stroke)&&
           Dot(world_returned,world_stroke)<=-.08F*Length(world_stroke)){
            s.returned=true;s.return_delta=returned;s.return_world_delta=world_returned;
        }
    }
    static bool LeavesReturn(Vec3 step,Vec3 direction)noexcept{
        return Length(step)>=.0001F&&Length(direction)>=.0001F&&Dot(step,direction)<=0;
    }
    static bool Reversed(Vec3 a,Vec3 b)noexcept{
        const float al=Length(a),bl=Length(b);
        // Match Append's measured-point precision. A fixed 1cm segment
        // floor misses valid velocity turns at normal high-rate pose cadence.
        return al>=.0001F&&bl>=.0001F&&Dot(a,b)<-.5F*al*bl;
    }
    static Vec3 Sub(Vec3 a,Vec3 b)noexcept{return {a.x-b.x,a.y-b.y,a.z-b.z};}
    static float Dot(Vec3 a,Vec3 b)noexcept{return a.x*b.x+a.y*b.y+a.z*b.z;}
    static float Length(Vec3 v)noexcept{return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
    static float Distance(Vec3 a,Vec3 b)noexcept{return Length(Sub(a,b));}
    static bool Valid(const Pose& p)noexcept{return p.position_valid&&p.orientation_valid&&
        std::isfinite(p.position.x)&&std::isfinite(p.position.y)&&std::isfinite(p.position.z);}
    static bool ValidHeadRotation(Quaternion q)noexcept{
        const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
        return std::isfinite(n)&&n>.5F&&n<1.5F;
    }
    std::array<Hand,2> hands_{};
    std::uint64_t owner_=0,context_=0,sequence_=0,now_=0;
    std::uint64_t player_=0,input_=0;
    Vec3 head_{};
};
}
