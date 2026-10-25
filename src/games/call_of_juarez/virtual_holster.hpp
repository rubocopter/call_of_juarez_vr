#pragma once
#include "games/call_of_juarez/interaction_selection.hpp"
#include "runtime/equipment_actions.hpp"
#include "runtime/gameplay_ui.hpp"
#include "runtime/vr_math.hpp"
#include <cmath>

namespace cojvr::games::call_of_juarez {
// Ergonomic gesture zones in tracking metres, not native holster/socket contact.
// Existing default grips are Action (left) and contextual Kick (right).
struct CoJVirtualHolsterSample {
    runtime::GameplayInputState raw{};
    runtime::EquipmentWheelSnapshot inventory{};
    CoJInteractionSelection interaction{}; // passive, current native Action owner
    runtime::Pose head{};
    std::array<runtime::Pose,2> grips{}; // right, left
    std::uint64_t player=0,context=0,sequence=0,now_us=0;
    bool allowed=false,native_ready=false,selection_blocked=false;
};
struct CoJVirtualHolsterZone {
    runtime::Vec3 offset{};
    float radius=0;
    int hand=-1,channel=-1;
    bool shoulder=false;
};
inline constexpr std::array<CoJVirtualHolsterZone,4> kCoJVirtualHolsterZones{{
    {{-.22F,-.70F,0},.11F,1,0,false},{{.22F,-.70F,0},.11F,0,1,false},
    {{.22F,-.10F,.20F},.24F,0,2,true},{{-.22F,-.10F,.20F},.24F,1,5,true}
}};
inline bool CoJVirtualHolsterWorldInteraction(const CoJInteractionSelection& s) noexcept {
    // Selection is not permission to execute Action. It only prevents an
    // equipment gesture from stealing the native interaction's physical grip.
    return s.valid&&(s.executing_id!=0 || (s.active_id!=0&&
        !(s.disabled_valid&&s.detection_disabled)));
}
inline bool CoJVirtualHolsterPoseValid(const runtime::Pose& p) noexcept {
    const auto q=p.orientation;const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return p.position_valid&&p.orientation_valid&&std::isfinite(p.position.x)&&
        std::isfinite(p.position.y)&&std::isfinite(p.position.z)&&
        std::isfinite(norm)&&norm>.9F&&norm<1.1F;
}
struct CoJVirtualHolsterLayout {
    std::array<runtime::Vec3,4> centres{};
    runtime::Vec3 right{},back{};
    bool valid=false;
};
inline CoJVirtualHolsterLayout BuildCoJVirtualHolsterLayout(const runtime::Pose& head) noexcept {
    CoJVirtualHolsterLayout out{};
    if(!CoJVirtualHolsterPoseValid(head))return out;
    const auto forward=runtime::RotateVector(head.orientation,{0,0,-1});
    float x=-forward.z,z=forward.x,length=std::hypot(x,z);
    if(length<.01F){const auto right=runtime::RotateVector(head.orientation,{1,0,0});
        x=right.x;z=right.z;length=std::hypot(x,z);}
    if(!std::isfinite(length)||length<.01F)return out;
    x/=length;z/=length;out.right={x,0,z};out.back={-z,0,x};
    for(unsigned i=0;i<out.centres.size();++i){const auto p=kCoJVirtualHolsterZones[i].offset;
        out.centres[i]={head.position.x+p.x*x-p.z*z,head.position.y+p.y,head.position.z+p.x*z+p.z*x};}
    out.valid=true;return out;
}
inline int CoJVirtualHolsterZoneForHand(const runtime::Pose& head,const runtime::Pose& hand,int h) noexcept {
    if(!CoJVirtualHolsterPoseValid(hand))return -1;
    const auto layout=BuildCoJVirtualHolsterLayout(head);if(!layout.valid)return -1;
    for(unsigned i=0;i<kCoJVirtualHolsterZones.size();++i){const auto zone=kCoJVirtualHolsterZones[i];
        if(zone.hand!=h)continue;
        const runtime::Vec3 d{hand.position.x-layout.centres[i].x,hand.position.y-layout.centres[i].y,
            hand.position.z-layout.centres[i].z};
        const float back=(hand.position.x-head.position.x)*layout.back.x+
            (hand.position.z-head.position.z)*layout.back.z;
        if((!zone.shoulder||back>=.05F)&&d.x*d.x+d.y*d.y+d.z*d.z<=zone.radius*zone.radius)return zone.channel;
    }
    return -1;
}
// Presentation and dispatch share the same conflict policy. A blocked zone
// remains a holster intent unless a positive native world Action owns left grip.
inline bool CoJVirtualHolsterInputConflict(const runtime::GameplayInputState& raw,
    const runtime::GameplayInputState& mapped) noexcept {
    for(unsigned i=0;i<raw.equipment_select.size();++i)
        if(raw.equipment_select[i]||mapped.equipment_select[i])return true;
    return raw.weapon_radial||mapped.weapon_radial||raw.reload||
        raw.fire_left||raw.fire_right||raw.quick_load||raw.quick_save||
        raw.hands||raw.discard_weapon||raw.weapon_next||raw.weapon_previous||
        raw.objectives||raw.logs||raw.alternate_fire;
}
class CoJVirtualHolsterState final {
public:
    // Preserve consumed physical holds through loss, not just their old meaning.
    void Invalidate() noexcept {known_=false;armed_={};}
    bool selection_blocked() const noexcept {return selection_blocked_;}
    int Update(const CoJVirtualHolsterSample& s,runtime::GameplayInputState& out) noexcept {
        const std::array<bool,2> held{s.raw.kick,s.raw.interact};
        const std::array<bool,2> mapped{out.kick,out.interact};
        constexpr std::array<unsigned,2> bits{9,6};
        std::array<bool,2> available{},edge{};
        for(int h=0;h<2;++h){
            available[h]=s.raw.active&&(s.raw.digital_available&(1U<<bits[h]))!=0;
            edge[h]=available[h]&&held[h]&&!down_[h];
            if(available[h]){
                if(!held[h])claimed_[h]=false;
                down_[h]=held[h];
            }else armed_[h]=false;
        }
        const auto suppress=[&]{
            if(claimed_[0]){out.kick=false;out.run=false;out.digital_available&=~(1U<<9);}
            if(claimed_[1]){out.interact=false;out.digital_available&=~(1U<<6);}
        };
        const bool fresh=s.sequence&&s.now_us&&(!clock_known_||
            (s.sequence>sequence_&&s.now_us>time_&&s.now_us-time_<=100000));
        const bool valid=s.allowed&&s.raw.active&&out.active&&s.player&&s.context&&
            s.raw.input_context_generation&&CoJVirtualHolsterPoseValid(s.head);
        const bool same=known_&&player_==s.player&&context_==s.context&&
            input_==s.raw.input_context_generation;
        if(!valid||!fresh||!same)armed_={};
        if(s.sequence&&s.now_us&&(!clock_known_||s.sequence!=sequence_)){
            clock_known_=true;sequence_=s.sequence;time_=s.now_us;
        }
        known_=valid;player_=s.player;context_=s.context;input_=s.raw.input_context_generation;
        const bool conflict=CoJVirtualHolsterInputConflict(s.raw,out);
        const bool world_interaction=CoJVirtualHolsterWorldInteraction(s.interaction);
        int selected=-1;
        for(int h=0;h<2;++h){
            if(!available[h]||!CoJVirtualHolsterPoseValid(s.grips[h])){armed_[h]=false;continue;}
            if(valid&&fresh&&!conflict&&!held[h])armed_[h]=true;
            if(!edge[h])continue;
            const int channel=valid?CoJVirtualHolsterZoneForHand(s.head,s.grips[h],h):-1;
            // Reserve a geometrically known zone press even when input is
            // stale, unarmed or busy. Selection still requires every proof
            // below; cancellation/exit must not turn this hold into a kick.
            // Fresh left grip belongs to an observed native world Action,
            // including carry/put-down. Never release a previously claimed
            // holster hold here; only an available physical release does that.
            const bool world_priority=world_interaction&&channel!=2&&channel!=5;
            const bool world_action=h==1&&world_priority&&!conflict;
            if(channel>=0&&!world_action)claimed_[h]=true;
            const bool admitted=valid&&fresh&&same&&armed_[h]&&mapped[h]&&!conflict&&!world_priority;
            armed_[h]=false;
            if(!admitted)continue;
            if(channel<0)continue;
            const unsigned bit=1U<<channel;
            if(selected<0&&s.native_ready&&s.inventory.valid&&(s.inventory.owned_mask&bit)&&
                (s.inventory.available_mask&bit))selected=channel;
        }
        suppress();
        // An unclaimed other-hand Action/Kick keeps its ordinary meaning;
        // never combine it with inventory mutation in this dispatch.
        selection_blocked_=conflict||out.interact||out.kick||out.run;
        if(selection_blocked_)selected=-1;
        if(selected>=0){
            // Also cancel an earlier failed/queued shoulder pulse on a left
            // holster selection before CaptureShoulder can merge it again.
            out.digital_available&=~(1U<<9);
            runtime::RequestEquipmentAction(out,static_cast<runtime::EquipmentAction>(selected));
        }
        return selected;
    }
private:
    std::array<bool,2> armed_{},down_{},claimed_{};
    bool known_=false,clock_known_=false,selection_blocked_=false;
    std::uint64_t player_=0,context_=0,input_=0,sequence_=0,time_=0;
};
}
