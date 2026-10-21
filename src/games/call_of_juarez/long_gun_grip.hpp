#pragma once
#include "games/call_of_juarez/long_gun_observer.hpp"
#include "runtime/two_hand_grip.hpp"
#include <cstring>
namespace cojvr::games::call_of_juarez {
struct CoJLongGunGripGeometry {
    CoJLongGunSupportSnapshot native{};
    runtime::Vec3 dominant_socket_cm{},muzzle_cm{},muzzle_direction{};
    runtime::Vec3 dominant_grip_cm{},direction{},up{},support_grip_cm{};
    const char* read_failure="not_requested"; // Static diagnostic literal, no JNI reference.
};
struct CoJLongGunGripFrame {
    bool valid=false;TrackedWeaponFramePlan plan{};ElementWorldBasisTarget mapped_support_frame{};
    runtime::TwoHandGripSolution solution{};float contact_distance_m=0,support_error_m=0;
    bool support_on_original_side=false;
};
// Ergonomic prototype limits, not a qualified native contact volume. Reject a
// collapsed pair or >20 cm controller/contact mismatch rather than stretching
// the weapon or letting a detached physical hand retain a remote native grip.
inline constexpr float kCoJLongGunGripMinimumSpanM=.05F;
inline constexpr float kCoJLongGunGripMaximumMismatchM=.20F;
// The source has a sequence but no producer timestamp. Date first observation
// once; repeated render reads must neither cancel a grip nor renew pose age.
class CoJLongGunGripSampleClock {
public:
    std::uint64_t Observe(std::uint64_t generation,std::uint64_t sequence,std::uint64_t now) noexcept {
        if(generation!=generation_||!sequence_||sequence>sequence_){
            generation_=generation;sequence_=sequence;time_=now;
        }
        return time_;
    }
private:
    std::uint64_t generation_=0,sequence_=0,time_=0;
};
inline bool IsCoJLongGunGripFrameAdmitted(const CoJLongGunGripFrame& frame) noexcept {
    return frame.valid&&frame.support_on_original_side&&
        frame.solution.source_span_m>=kCoJLongGunGripMinimumSpanM&&
        frame.solution.target_span_m>=kCoJLongGunGripMinimumSpanM&&
        frame.support_error_m<=kCoJLongGunGripMaximumMismatchM;
}
namespace long_gun_grip_detail {
inline int Kind(const char* name) noexcept {
    if(name)for(int i=0;i<3;++i)if(!std::strcmp(name,long_gun_observer_detail::Classes[i]))return i+1;
    return 0;
}
inline bool Qualified(const CoJLongGunSupportSnapshot& n) noexcept {
    if(!n.valid||!n.object_epoch||!Kind(n.weapon_class)||!long_gun_observer_detail::ValidId(n.player_id)||
        !long_gun_observer_detail::ValidId(n.weapon_id)||n.holder_element<0||n.support_element<=0)return false;
    bool fire=false;
    for(int h=0;h<2;++h){
        for(int state:n.states[h]){
            if(h==1?state!=0:(state!=1&&(state<30||state>38)))return false;
            if(h==0&&state!=1)fire=true;
        }
        for(bool two:n.two_handed[h])if(two!=(h==0))return false;
    }
    CoJReloadLocalFrame holder{},support{};
    return fire==n.firing&&BuildCoJReloadLocalFrame(n.root,n.holder,holder)&&
        BuildCoJReloadLocalFrame(n.root,n.support,support)&&n.support_in_weapon.valid;
}
inline runtime::Vec3 Scale(runtime::Vec3 p,float s) noexcept{return {p.x*s,p.y*s,p.z*s};}
inline std::array<float,12> Matrix(const ElementWorldBasisTarget& f,runtime::Vec3 anchor) noexcept {
    const auto x=runtime::two_hand_detail::Cross(f.up,f.forward);
    return {x.x,f.up.x,f.forward.x,anchor.x*.01F,x.y,f.up.y,f.forward.y,anchor.y*.01F,
        x.z,f.up.z,f.forward.z,anchor.z*.01F};
}
inline runtime::Vec3 Direction(const std::array<float,12>& m,runtime::Vec3 p) noexcept {
    return {m[0]*p.x+m[1]*p.y+m[2]*p.z,m[4]*p.x+m[5]*p.y+m[6]*p.z,m[8]*p.x+m[9]*p.y+m[10]*p.z};
}
inline runtime::Vec3 Point(const std::array<float,12>& m,runtime::Vec3 p) noexcept {
    const auto v=Direction(m,Scale(p,.01F));return Scale({v.x+m[3],v.y+m[7],v.z+m[11]},100.F);
}
inline ElementWorldBasisTarget LocalWorld(const ElementWorldBasisTarget& root,const CoJReloadLocalFrame& local) noexcept {
    const auto m=Matrix(root,root.position);
    const auto delta=Direction(m,local.position);
    return {{root.position.x+delta.x,root.position.y+delta.y,root.position.z+delta.z},
        Direction(m,local.up),Direction(m,local.forward),local.valid};
}
}
// All native values are cm. The neutral solver only receives metres. The
// authored support socket is supplied by exact natural animation, never guessed
// from a barrel axis, mesh bounds or a universal foregrip offset.
inline CoJLongGunGripFrame BuildCoJLongGunGripFrame(const CoJLongGunGripGeometry& g,
    const CoJReloadLocalFrame* frozen=nullptr) noexcept {
    using namespace long_gun_grip_detail;
    if(!Qualified(g.native))return {};
    const auto support=LocalWorld(g.native.root,frozen?*frozen:g.native.support_in_weapon);
    CoJReloadLocalFrame checked{};
    if(!BuildCoJReloadLocalFrame(g.native.root,support,checked))return {};
    const auto ordinary=BuildTrackedWeaponFrame(g.native.root,g.dominant_socket_cm,g.muzzle_cm,
        g.muzzle_direction,g.dominant_grip_cm,g.direction,g.up);
    if(!ordinary.valid)return {};
    const auto contact=TransformWeaponElementFrame(g.native.root,ordinary.root,support);
    const auto solution=runtime::SolveTwoHandGrip({Matrix(g.native.root,g.dominant_socket_cm),
        Matrix(ordinary.root,g.dominant_grip_cm),Scale(support.position,.01F),Scale(g.support_grip_cm,.01F)});
    if(!contact.valid||!solution.valid)return {};
    const auto& m=solution.target_from_source;
    CoJLongGunGripFrame out{};
    out.plan.root={Point(m,g.native.root.position),Direction(m,g.native.root.up),Direction(m,g.native.root.forward),true};
    out.plan.muzzle_origin=Point(m,g.muzzle_cm);
    out.plan.muzzle_direction=Direction(m,g.muzzle_direction);
    out.plan.valid=true;
    out.mapped_support_frame={Point(m,support.position),Direction(m,support.up),Direction(m,support.forward),true};
    const auto d=runtime::two_hand_detail::Sub(contact.position,g.support_grip_cm);
    out.contact_distance_m=float(std::sqrt(runtime::two_hand_detail::Dot(d,d))*.01);
    if(!std::isfinite(out.contact_distance_m)||!runtime::two_hand_detail::Finite(out.plan.muzzle_origin)||
        !runtime::two_hand_detail::Finite(out.plan.muzzle_direction))return {};
    // The tracked support must stay on the same side of the dominant grip as
    // the authored grip in the ordinary right-hand map. Length alone admits a
    // 180-degree inversion (left hand crossing through the right hand) that
    // flips the barrel without changing the spacing error. No angle is tuned:
    // crossing the perpendicular plane retires the claimed support instead.
    const auto expected=runtime::two_hand_detail::Sub(contact.position,g.dominant_grip_cm);
    const auto tracked=runtime::two_hand_detail::Sub(g.support_grip_cm,g.dominant_grip_cm);
    out.support_on_original_side=runtime::two_hand_detail::Dot(expected,tracked)>0;
    out.solution=solution;out.support_error_m=solution.support_error_m;out.valid=true;return out;
}
struct CoJLongGunGripInput {std::uint64_t player_generation=0,context=0;runtime::TwoHandGripInput controls{};};
struct CoJLongGunGripDiagnostic {
    bool native_qualified=false,frame_valid=false,spatially_admitted=false,support_on_original_side=false;
    float contact_distance_m=-1,source_span_m=-1,target_span_m=-1,support_error_m=-1;
};
struct CoJLongGunGripOutput {bool attached=false;CoJLongGunGripFrame frame{};runtime::TwoHandGripOutput claim{};
    CoJLongGunGripDiagnostic diagnostic{};};
class CoJLongGunGripState {
public:
    CoJLongGunGripOutput Update(const CoJLongGunGripInput& in,const CoJLongGunGripGeometry& g) noexcept {
        const bool qualified=in.player_generation&&in.context&&long_gun_grip_detail::Qualified(g.native);
        const bool same=qualified&&Same(in.player_generation,in.context,g.native);
        if(!same){
            generation_=in.player_generation;context_=in.context;owner_=g.native;
            ++epoch_;if(!epoch_)++epoch_;
        }
        const auto frame=qualified?BuildCoJLongGunGripFrame(g,attached_&&same?&frozen_:nullptr):CoJLongGunGripFrame{};
        auto controls=in.controls;controls.owner=qualified?epoch_:0;
        const bool spatial=IsCoJLongGunGripFrameAdmitted(frame);
        // Spatial approach is not loss of player/item/tracking ownership. A
        // released L1 outside contact must still arm a fresh press on arrival.
        // Attached grips retain every spatial cancellation fence; an outside
        // press remains unqueued because the policy clears its release baseline.
        controls.eligible=controls.eligible&&qualified&&(!attached_||spatial)&&
            ((attached_&&same)||!g.native.firing);
        controls.contact_qualified=!attached_||frame.valid;
        controls.contact_distance_m=spatial?frame.contact_distance_m:std::numeric_limits<float>::infinity();
        CoJLongGunGripOutput out{};out.claim=policy_.Update(controls);
        out.diagnostic={qualified,frame.valid,spatial,frame.support_on_original_side,
            frame.valid?frame.contact_distance_m:-1.F,
            frame.valid?frame.solution.source_span_m:-1.F,
            frame.valid?frame.solution.target_span_m:-1.F,
            frame.valid?frame.support_error_m:-1.F};
        attached_=out.claim.stage==runtime::TwoHandGripStage::attached;
        if(out.claim.acquired)frozen_=g.native.support_in_weapon;
        out.attached=attached_;
        if(attached_)out.frame=frame;
        return out;
    }
    CoJLongGunGripFrame Refresh(std::uint64_t generation,std::uint64_t context,const CoJLongGunGripGeometry& g) const noexcept {
        if(!attached_||!Same(generation,context,g.native))return {};
        const auto frame=BuildCoJLongGunGripFrame(g,&frozen_);
        return IsCoJLongGunGripFrameAdmitted(frame)?frame:CoJLongGunGripFrame{};
    }
    // Cancels authority without forgetting the physical controller's release
    // barriers. Call Update with real available samples to discharge them.
    void Cancel() noexcept{(void)policy_.Update({});attached_=false;}
    bool attached() const noexcept{return attached_;}
private:
    bool Same(std::uint64_t generation,std::uint64_t context,const CoJLongGunSupportSnapshot& n) const noexcept {
        return generation_==generation&&context_==context&&owner_.object_epoch==n.object_epoch&&
            owner_.player_id==n.player_id&&
            owner_.weapon_id==n.weapon_id&&owner_.holder_element==n.holder_element&&
            owner_.support_element==n.support_element&&long_gun_grip_detail::Kind(owner_.weapon_class)==
            long_gun_grip_detail::Kind(n.weapon_class);
    }
    runtime::TwoHandGripPolicy policy_{};CoJReloadLocalFrame frozen_{};
    CoJLongGunSupportSnapshot owner_{};
    std::uint64_t generation_=0,context_=0,epoch_=0;bool attached_=false;
};
inline void FilterCoJLongGunGripInput(const runtime::TwoHandGripOutput& claim,runtime::GameplayInputState& input) noexcept {
    if(claim.consume_grip){input.interact=false;input.digital_available&=~(1U<<6);}
    if(claim.consume_support_trigger){input.fire_left=false;input.digital_available&=~1U;}
}
}
