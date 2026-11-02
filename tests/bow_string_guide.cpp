#include "games/call_of_juarez/bow_string_guide.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::runtime;
using namespace cojvr::games::call_of_juarez;
namespace {
void Check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
bool Near(Vec3 a,Vec3 b,float tol=1.e-4F){return std::abs(a.x-b.x)<tol&&
    std::abs(a.y-b.y)<tol&&std::abs(a.z-b.z)<tol;}
Vec3 Sum(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 Scale(Vec3 a,float f){return {a.x*f,a.y*f,a.z*f};}
Vec3 Centre(const HolsterGuideSurface& g){Vec3 centre{};
    for(const auto p:g.head_corners)centre=Sum(centre,Scale(p,.25F));return centre;}
CoJBowStringGuideSample Sample(){
    CoJBowStringGuideSample s{};
    s.allowed=true;s.pose_sequence=43;
    s.bow.owner_token=7;s.bow.nock_weapon_id=17;
    s.bow.nock_pose_sequence=s.pose_sequence;s.bow.nock_reference_valid=true;
    s.head.position={2,1.6F,-3};s.head.orientation={0,.70710678F,0,.70710678F};
    s.head.position_valid=s.head.orientation_valid=true;
    s.right_hand=s.head;
    s.camera_right={0,0,-1};s.camera_up={0,1,0};s.camera_forward={1,0,0};
    s.head_world_m={19,1.5F,-22};
    const Vec3 expected_head{-0.1F,-0.1F,-0.4F};
    const Vec3 tracking=RotateVector(s.head.orientation,expected_head);
    s.right_hand.position=Sum(s.head.position,tracking);
    s.bow.nock_world_m=Sum(s.head_world_m,
        Sum(Sum(Scale(s.camera_right,tracking.x),Scale(s.camera_up,tracking.y)),
            Scale(s.camera_forward,tracking.z)));
    return s;
}
void Geometry(){
    auto s=Sample();StereoHudTextOverlay overlay{};
    overlay.holster_guides.count=1;
    overlay.holster_guides.zones[0].kind=GestureGuideKind::support_grip;
    Check(AppendCoJBowStringGuide(s,true,overlay),"native nock should produce a guide");
    Check(overlay.holster_guides.count==2&&overlay.holster_guides.zones[0].kind==GestureGuideKind::support_grip,
        "bow guide must append without replacing existing guides");
    const auto& guide=overlay.holster_guides.zones[1];
    Check(guide.kind==GestureGuideKind::bow_string&&guide.state==1,"tracked hand contact must show near state");
    Check(Near(Centre(guide),{-.1F,-.1F,-.4F}),"world-to-head inverse must respect native basis and HMD quaternion");
    const auto span=guide.head_corners[1].x-guide.head_corners[0].x;
    Check(std::abs(span-.036F)<1.e-5F,"guide must remain a small bounded marker");
    s.right_hand.position.x+=.5F;
    overlay={};Check(AppendCoJBowStringGuide(s,true,overlay)&&overlay.holster_guides.zones[0].state==0,
        "distant hand must show available state");
    s.bow.gesture.drawing=true;
    overlay={};Check(AppendCoJBowStringGuide(s,true,overlay)&&overlay.holster_guides.zones[0].state==1,
        "active physical draw must show drawn state");
}
void Rejection(){
    const auto base=Sample();
    for(int fault=0;fault<18;++fault){
        auto s=base;bool enabled=true;StereoHudTextOverlay overlay{};
        switch(fault){
            case 0:enabled=false;break;
            case 1:s.allowed=false;break;
            case 2:s.bow.nock_reference_valid=false;break;
            case 3:s.bow.owner_token=0;break;
            case 4:s.bow.nock_weapon_id=0;break;
            case 5:s.bow.nock_pose_sequence++;break;
            case 6:s.bow.gesture.cancelled=true;break;
            case 7:s.bow.dispatch.cancelled=true;break;
            case 8:s.bow.dispatch.shot_requested=true;break;
            case 9:s.head.position_valid=false;break;
            case 10:s.right_hand.orientation_valid=false;break;
            case 11:s.head_world_m.x=std::numeric_limits<float>::quiet_NaN();break;
            case 12:s.bow.nock_world_m.z=std::numeric_limits<float>::infinity();break;
            case 13:s.camera_right={1,0,0};break; // non-orthogonal basis
            case 14:s.bow.nock_world_m=Sum(s.head_world_m,s.camera_right);break; // head-local +Z (behind)
            case 15:s.bow.nock_world_m=Sum(s.head_world_m,Scale(s.camera_right,-3));break; // head-local -Z (far)
            case 16:overlay.holster_guides.count=overlay.holster_guides.zones.size();break;
            case 17:s.pose_sequence=0;break;
        }
        const auto before=overlay.holster_guides.count;
        Check(!AppendCoJBowStringGuide(s,enabled,overlay)&&overlay.holster_guides.count==before,
            "invalid pose, ownership, geometry, cancellation or capacity admitted bow hint");
    }
}
}
int main(){Geometry();Rejection();std::cout<<"bow string guide passed\n";}
