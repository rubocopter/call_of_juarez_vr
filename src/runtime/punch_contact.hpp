#pragma once
#include "runtime/punch_gesture.hpp"
#include <cstddef>
#include <limits>

namespace cojvr::runtime {
enum class MotionPunchContactResult { failure=-1, clear=0, contact=1, attack_accepted=2 };
enum class MotionPunchContactStage { blocked, awaiting_release, open, ready, contact_latched };
enum class MotionPunchContactReason {
    none, not_allowed, recentered, invalid_owner, invalid_head, owner_changed,
    repeated_pose, sequence_regressed, time_discontinuity, claim_unavailable,
    invalid_grip, motion_discontinuity, missing_completion, native_failure
};
struct MotionPunchContactOutput {
    std::array<bool,2> request{},allow_attack{};
    std::array<std::uint64_t,2> request_sequence{};
    std::uint64_t pose_sequence=0;
    std::array<std::uint64_t,2> previous_pose_sequence{};
    std::array<bool,2> geometry_reset{true,true};
    std::array<Pose,2> previous_grip{},current_grip{};
    std::uint64_t dt_ms=0;
    struct Diagnostic {
        MotionPunchContactStage stage=MotionPunchContactStage::blocked;
        MotionPunchContactReason reason=MotionPunchContactReason::none;
        float speed_mps=0,world_speed_mps=0,relative_step_m=0,world_step_m=0;
        bool noise=true,released=false,contact_latched=false;
    };
    std::array<Diagnostic,2> diagnostic{};
};
class MotionPunchContact {
public:
    // Reference-space grip poses remain measured values in metres. The game
    // adapter maps its knuckle patch through each full native hand basis.
    // Every fresh closed pose probes current contact, including static poses;
    // permission to attempt native attack is separate from probe admission.
    MotionPunchContactOutput Update(const MotionPunchInput& in) noexcept {
        MotionPunchContactOutput out{};
        out.pose_sequence=in.sequence;
        out.dt_ms=in.now_ms>=now_?in.now_ms-now_:0;
        if(!in.allowed||in.recentered||!in.owner||!in.sequence||!Valid(in.head)){
            const auto reason=!in.allowed?MotionPunchContactReason::not_allowed:
                in.recentered?MotionPunchContactReason::recentered:
                !in.owner?MotionPunchContactReason::invalid_owner:
                !in.sequence?MotionPunchContactReason::sequence_regressed:MotionPunchContactReason::invalid_head;
            Reset();for(auto& d:out.diagnostic)d.reason=reason;
            return out;
        }
        const bool changed=in.owner!=owner_||in.context!=context_||
            in.player_generation!=player_||in.input_generation!=input_;
        if(changed){Reset();for(auto& d:out.diagnostic)d.reason=MotionPunchContactReason::owner_changed;}
        // Repeated deliveries do not advance geometry or renew its clock.
        // They still observe available release and all source-loss barriers.
        if(sequence_&&in.sequence<=sequence_){
            if(in.sequence<sequence_||in.now_ms<now_||in.now_ms-now_>80){
                const auto reason=in.sequence<sequence_?MotionPunchContactReason::sequence_regressed:
                    MotionPunchContactReason::time_discontinuity;
                Reset();for(auto& d:out.diagnostic)d.reason=reason;
                return out;
            }
            for(std::size_t h=0;h<2;++h){
                auto& s=hands_[h];out.diagnostic[h].reason=MotionPunchContactReason::repeated_pose;
                out.geometry_reset[h]=false;
                if(!in.claim_available[h]||!Valid(in.grip[h])){
                    Invalidate(s,!in.claim_available[h]?MotionPunchContactReason::claim_unavailable:
                        MotionPunchContactReason::invalid_grip);out.geometry_reset[h]=true;
                }else if(!in.closed[h]){
                    s.released=true;s.contact_latched=false;s.pending=0;
                    // Cancel contact, not the last unique measured skin pose.
                    // The adapter cannot rebuild it from a repeated delivery.
                    s.reason=MotionPunchContactReason::none;
                }
                Observe(s,in.closed[h],out.diagnostic[h]);
            }
            return out;
        }
        const bool had_sequence=sequence_!=0;
        const auto dt=in.now_ms>now_?in.now_ms-now_:0;
        const bool fresh=had_sequence&&dt>=5&&dt<=80;
        out.dt_ms=dt;
        if(had_sequence&&!fresh){
            for(auto& s:hands_)Invalidate(s,MotionPunchContactReason::time_discontinuity);
        }
        for(std::size_t h=0;h<2;++h){
            auto& s=hands_[h];auto& d=out.diagnostic[h];
            if(!in.claim_available[h]||!Valid(in.grip[h])){
                Invalidate(s,!in.claim_available[h]?MotionPunchContactReason::claim_unavailable:
                    MotionPunchContactReason::invalid_grip);Observe(s,in.closed[h],d);continue;
            }
            auto inverse=NormalizeQuaternion(in.head.orientation);
            inverse.x=-inverse.x;inverse.y=-inverse.y;inverse.z=-inverse.z;
            const auto relative=RotateVector(inverse,Sub(in.grip[h].position,in.head.position));
            out.previous_grip[h]=s.have_previous?s.previous:in.grip[h];
            out.current_grip[h]=in.grip[h];out.previous_pose_sequence[h]=s.pose_sequence;
            const bool continuous=fresh&&s.have_previous;
            if(continuous){
                d.world_step_m=Length(Sub(in.grip[h].position,s.previous.position));
                d.relative_step_m=Length(Sub(relative,s.previous_relative));
                d.world_speed_mps=d.world_step_m*1000.F/static_cast<float>(dt);
                d.speed_mps=d.relative_step_m*1000.F/static_cast<float>(dt);
                d.noise=d.world_step_m<=.0001F||d.relative_step_m<=.0001F;
            }
            const bool jump=continuous&&(d.world_step_m>.25F||d.relative_step_m>.25F);
            const bool missing=s.pending!=0;
            if(missing)Invalidate(s,MotionPunchContactReason::missing_completion);
            // Store every valid unique pose, even open poses and discarded
            // jumps. No curve, accumulated travel or synthetic reach survives.
            s.previous=in.grip[h];s.previous_relative=relative;s.pose_sequence=in.sequence;s.have_previous=true;
            if(!in.closed[h]){
                s.released=true;s.contact_latched=false;s.pending=0;s.reason=MotionPunchContactReason::none;
            }else if(jump){
                // A geometric discontinuity skips this pair without trapping
                // a held fist. Keep contact latched until a real probe clears it.
                d.reason=MotionPunchContactReason::motion_discontinuity;
            }else if(s.released&&continuous&&!missing){
                if(next_request_!=(std::numeric_limits<std::uint64_t>::max)()){
                    s.pending=++next_request_;
                    out.request[h]=true;out.request_sequence[h]=s.pending;out.geometry_reset[h]=false;
                    out.allow_attack[h]=!s.contact_latched&&!d.noise&&d.speed_mps>=.75F&&d.world_speed_mps>=.75F;
                }else Invalidate(s,MotionPunchContactReason::native_failure);
            }
            Observe(s,in.closed[h],d);
        }
        owner_=in.owner;context_=in.context;player_=in.player_generation;input_=in.input_generation;
        sequence_=in.sequence;now_=in.now_ms;
        return out;
    }
    // Call synchronously once for each request. Tokens are policy-issued and
    // never recycled on Reset, so an old result cannot affect a new owner.
    bool Complete(std::size_t hand,std::uint64_t request_sequence,MotionPunchContactResult result) noexcept {
        return Complete(hand,request_sequence,static_cast<int>(result));
    }
    bool Complete(std::size_t hand,std::uint64_t request_sequence,int result) noexcept {
        if(hand>=hands_.size()||!request_sequence||hands_[hand].pending!=request_sequence)return false;
        auto& s=hands_[hand];s.pending=0;
        if(result==0)s.contact_latched=false;
        else if(result==1||result==2)s.contact_latched=true;
        else Invalidate(s,MotionPunchContactReason::native_failure);
        return true;
    }
    // The adapter discarded an unsafe native-space pair and rebaselined its
    // geometry cache. This is neither a contact miss nor a native owner failure.
    bool SkipGeometry(std::size_t hand,std::uint64_t request_sequence) noexcept {
        if(hand>=hands_.size()||!request_sequence||hands_[hand].pending!=request_sequence)return false;
        hands_[hand].pending=0;
        return true;
    }
    void Reset() noexcept {hands_={};owner_=context_=player_=input_=sequence_=now_=0;}
private:
    struct Hand {
        bool released=false,contact_latched=false,have_previous=false;
        Pose previous{};Vec3 previous_relative{};
        std::uint64_t pose_sequence=0,pending=0;
        MotionPunchContactReason reason=MotionPunchContactReason::none;
    };
    static void Invalidate(Hand& s,MotionPunchContactReason reason) noexcept{s={};s.reason=reason;}
    static void Observe(const Hand& s,bool closed,MotionPunchContactOutput::Diagnostic& d) noexcept {
        if(s.reason!=MotionPunchContactReason::none)d.reason=s.reason;
        d.released=s.released;d.contact_latched=s.contact_latched;
        d.stage=!s.released?MotionPunchContactStage::awaiting_release:!closed?MotionPunchContactStage::open:
            s.contact_latched?MotionPunchContactStage::contact_latched:MotionPunchContactStage::ready;
    }
    static Vec3 Sub(Vec3 a,Vec3 b) noexcept{return {a.x-b.x,a.y-b.y,a.z-b.z};}
    static float Length(Vec3 v) noexcept{return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
    static bool Valid(const Pose& pose) noexcept {
        const auto q=pose.orientation;const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
        return pose.position_valid&&pose.orientation_valid&&std::isfinite(pose.position.x)&&
            std::isfinite(pose.position.y)&&std::isfinite(pose.position.z)&&std::isfinite(n)&&n>.5F&&n<1.5F;
    }
    std::array<Hand,2> hands_{};
    std::uint64_t owner_=0,context_=0,player_=0,input_=0,sequence_=0,now_=0,next_request_=0;
};
}
