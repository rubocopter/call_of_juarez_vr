#pragma once
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
    runtime::Pose head{};
    std::array<runtime::Pose,2> grips{}; // right, left
    std::uint64_t player=0,context=0,sequence=0,now_us=0;
    bool allowed=false,native_ready=false;
};
class CoJVirtualHolsterState final {
public:
    // Preserve consumed physical holds through loss, not just their old meaning.
    void Invalidate() noexcept {known_=false;armed_={};}
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
            s.raw.input_context_generation&&Valid(s.head);
        const bool same=known_&&player_==s.player&&context_==s.context&&
            input_==s.raw.input_context_generation;
        if(!valid||!fresh||!same)armed_={};
        if(s.sequence&&s.now_us&&(!clock_known_||s.sequence!=sequence_)){
            clock_known_=true;sequence_=s.sequence;time_=s.now_us;
        }
        known_=valid;player_=s.player;context_=s.context;input_=s.raw.input_context_generation;
        const bool conflict=s.raw.weapon_radial||out.weapon_radial||s.raw.reload||
            s.raw.fire_left||s.raw.fire_right||s.raw.quick_load||s.raw.quick_save||
            s.raw.hands||s.raw.discard_weapon||s.raw.weapon_next||s.raw.weapon_previous||
            s.raw.objectives||s.raw.logs||s.raw.alternate_fire||
            AnyEquipment(s.raw)||AnyEquipment(out);
        int selected=-1;
        for(int h=0;h<2;++h){
            if(!available[h]||!Valid(s.grips[h])){armed_[h]=false;continue;}
            if(valid&&fresh&&!conflict&&!held[h])armed_[h]=true;
            if(!edge[h])continue;
            const bool admitted=valid&&fresh&&same&&armed_[h]&&mapped[h]&&!conflict;
            armed_[h]=false;
            if(!admitted)continue;
            const int channel=Zone(s.head,s.grips[h],h);
            if(channel<0)continue;
            claimed_[h]=true;
            const unsigned bit=1U<<channel;
            if(selected<0&&s.native_ready&&s.inventory.valid&&(s.inventory.owned_mask&bit)&&
                (s.inventory.available_mask&bit))selected=channel;
        }
        suppress();
        // An unclaimed other-hand Action/Kick keeps its ordinary meaning;
        // never combine it with inventory mutation in this dispatch.
        if(out.interact||out.kick||out.run)selected=-1;
        if(selected>=0){
            // Also cancel an earlier failed/queued shoulder pulse on a left
            // holster selection before CaptureShoulder can merge it again.
            out.digital_available&=~(1U<<9);
            runtime::RequestEquipmentAction(out,static_cast<runtime::EquipmentAction>(selected));
        }
        return selected;
    }
private:
    static bool Valid(const runtime::Pose& p) noexcept {
        const auto q=p.orientation;
        const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
        return p.position_valid&&p.orientation_valid&&std::isfinite(p.position.x)&&
            std::isfinite(p.position.y)&&std::isfinite(p.position.z)&&
            std::isfinite(norm)&&norm>.9F&&norm<1.1F;
    }
    static bool AnyEquipment(const runtime::GameplayInputState& input) noexcept {
        for(bool pressed:input.equipment_select)if(pressed)return true;
        return false;
    }
    static int Zone(const runtime::Pose& head,const runtime::Pose& hand,int h) noexcept {
        const auto forward=runtime::RotateVector(head.orientation,{0,0,-1});
        float x=-forward.z,z=forward.x;
        float length=std::hypot(x,z);
        if(length<.01F){
            // Looking at the hip must not pitch its zone into the floor.
            const auto right=runtime::RotateVector(head.orientation,{1,0,0});
            x=right.x;z=right.z;length=std::hypot(x,z);
        }
        if(!std::isfinite(length)||length<.01F)return -1;
        x/=length;z/=length;
        const auto p=runtime::Vec3{hand.position.x-head.position.x,
            hand.position.y-head.position.y,hand.position.z-head.position.z};
        const runtime::Vec3 local{p.x*x+p.z*z,p.y,-p.x*z+p.z*x};
        const auto in_zone=[&](runtime::Vec3 centre,float radius){
            const auto a=local.x-centre.x,b=local.y-centre.y,c=local.z-centre.z;
            return a*a+b*b+c*c<=radius*radius;
        };
        if(h==0&&local.z>=.05F&&in_zone({.22F,-.10F,.20F},.24F))return 2;
        if(in_zone({h==0?.22F:-.22F,-.70F,0},.20F))return h==0?1:0;
        return -1;
    }
    std::array<bool,2> armed_{},down_{},claimed_{};
    bool known_=false,clock_known_=false;
    std::uint64_t player_=0,context_=0,input_=0,sequence_=0,time_=0;
};
}
