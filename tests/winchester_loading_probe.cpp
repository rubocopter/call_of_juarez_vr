#include "games/call_of_juarez/winchester_loading_probe.hpp"
#include "games/call_of_juarez/winchester_reload_approach.hpp"
#include "runtime/reload_visual.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
using namespace cojvr::games::call_of_juarez;
void Check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<.0001F;}

namespace {
void CheckReceiverApproach() {
    using cojvr::runtime::Vec3;
    constexpr ElementWorldBasisTarget identity{{},{0,1,0},{0,0,1},true};
    // Proper, non-axis-aligned rotation with the receiver facing world -X.
    const auto rotate=[](Vec3 v) -> Vec3 {
        return {(-2*v.x+2*v.y-v.z)/3,(v.x+2*v.y+2*v.z)/3,(2*v.x+v.y-2*v.z)/3};
    };
    constexpr Vec3 translation{120,-72,35};
    const auto point=[&](Vec3 v) -> Vec3 {
        const auto r=rotate(v);
        return {r.x+translation.x,r.y+translation.y,r.z+translation.z};
    };
    constexpr Vec3 centre{1.228641F,-17.347876F,-4.344577F};
    constexpr Vec3 outward{.999935744F,-.002132640F,.011133682F};
    for (const auto model : {CoJWinchesterLoadingModel::ordinary, CoJWinchesterLoadingModel::scoped}) {
        const ElementWorldBasisTarget body{translation,rotate(identity.up),rotate(identity.forward),true};
        CoJReloadMouthPreview preview{};
        Check(BuildCoJWinchesterLoadingLip(model,body,preview.points_cm),"measured preview lip failed");
        preview.count=preview.points_cm.size();
        const auto expected_centre=point(centre);
        auto approach=BuildCoJWinchesterReloadApproach(preview);
        Check(approach.mouth_valid && !approach.valid && !approach.within_capture &&
            Near(approach.centre_cm.x,expected_centre.x) && Near(approach.centre_cm.y,expected_centre.y) &&
            Near(approach.centre_cm.z,expected_centre.z),"preview lost its measured centre without a current nose");

        const auto sample=[&](Vec3 lip, float distance, Vec3 axis) {
            // Put the requested projection on the measured best-fit plane.
            lip.x=centre.x-(outward.y*(lip.y-centre.y)+outward.z*(lip.z-centre.z))/outward.x;
            preview.tip_cm={point({lip.x+outward.x*distance,lip.y+outward.y*distance,
                lip.z+outward.z*distance}),rotate(identity.up),rotate(axis),true};
            return BuildCoJWinchesterReloadApproach(preview);
        };
        constexpr Vec3 inward{-1,0,0};
        const auto outside=sample(centre,2.F,inward);
        const auto behind=sample(centre,-.25F,inward);
        Check(outside.valid && outside.within_capture && Near(outside.outside_distance_cm,2.F) &&
            behind.valid && behind.within_capture && Near(behind.outside_distance_cm,-.25F),
            "capture did not span both sides of the rotated lip with signed diagnostics");
        const auto reversed=sample(centre,2.F,{1,0,0});
        const auto tangent=sample(centre,-2.F,{0,0,1});
        Check(reversed.valid && reversed.within_capture && tangent.valid && tangent.within_capture &&
            Near(outside.axis_alignment,outward.x) && Near(reversed.axis_alignment,-outward.x) &&
            Near(tangent.axis_alignment,-outward.z),
            "nose alignment must be signed and separate from the capture geometry");

        for (const float side : {-1.F,1.F}) {
            const auto near=sample(centre,side*6.99F,inward);
            const auto far=sample(centre,side*7.01F,inward);
            Check(near.valid && near.within_capture && Near(near.outside_distance_cm,side*6.99F) &&
                far.valid && !far.within_capture && Near(far.outside_distance_cm,side*7.01F),
                "the rotated lip must have a 7 cm normal reach on both sides");

            // The measured rim ends near Y=-20.0688, 1.93 cm from near_edge.
            const Vec3 near_edge{0,-22.F,-4.4F};
            const Vec3 far_edge{0,-27.2F,-4.4F};
            Check(sample(near_edge,side*6.5F,inward).within_capture &&
                !sample(near_edge,side*6.85F,inward).within_capture &&
                !sample(far_edge,side*.25F,inward).within_capture,
                "capture must use combined distance to the finite measured rim on both sides");
            Check(sample({0,-14.65F,-4.3F},side*6.999F,inward).within_capture &&
                !sample({0,-14.65F,-3.58F},side*6.999F,inward).within_capture,
                "capture lost the measured tab or filled the adjacent concave notch");
        }

        auto invalid=preview;
        invalid.tip_cm.valid=false;
        approach=BuildCoJWinchesterReloadApproach(invalid);
        Check(approach.mouth_valid && !approach.valid && !approach.within_capture,
            "unread tip frame admitted receiver capture");
        invalid=preview;
        invalid.tip_cm.position.x=std::numeric_limits<float>::infinity();
        approach=BuildCoJWinchesterReloadApproach(invalid);
        Check(approach.mouth_valid && !approach.valid && !approach.within_capture,
            "nonfinite tip position admitted receiver capture");
        invalid=preview;
        invalid.tip_cm.forward={};
        approach=BuildCoJWinchesterReloadApproach(invalid);
        Check(approach.mouth_valid && !approach.valid && !approach.within_capture,
            "invalid tip axis admitted receiver capture");
        invalid=preview; invalid.count=2;
        Check(!BuildCoJWinchesterReloadApproach(invalid).mouth_valid,"shotgun preview admitted as Winchester");
        invalid=preview; invalid.points_cm.fill(expected_centre);
        Check(!BuildCoJWinchesterReloadApproach(invalid).mouth_valid,"collapsed polygon admitted as Winchester");
        invalid=preview; std::reverse(invalid.points_cm.begin(),invalid.points_cm.end());
        Check(!BuildCoJWinchesterReloadApproach(invalid).mouth_valid,"reversed winding could invert receiver exterior");
        invalid=preview; invalid.points_cm[7].x=std::numeric_limits<float>::quiet_NaN();
        Check(!BuildCoJWinchesterReloadApproach(invalid).mouth_valid,"nonfinite receiver corner admitted");
    }
}
}

int main(){
    CheckReceiverApproach();
    // A legacy controller-tip proxy or missing mesh centre correction puts
    // this nose 2.1088 cm away from the actual displayed imported mesh.
    ElementWorldBasisTarget pinch{{100,200,300},{0,1,0},{1,0,0},true};
    auto tip=BuildCoJTexturedReloadNoseFrame(pinch);
    Check(tip.valid&&Near(tip.position.x,97.358826F)&&Near(tip.position.y,200)&&
        Near(tip.position.z,300)&&tip.forward.x==-1&&tip.up.y==1,
        "textured nose lost mesh centre/forward convention or centimetres");
    pinch.forward.x=2;
    Check(!BuildCoJTexturedReloadNoseFrame(pinch).valid,"scaled pinch supplied a nose");
    pinch={{100,200,300},{0,1,0},{0,0,1},true};
    CoJWinchesterLoadingGeometry geometry{};
    geometry.model=CoJWinchesterLoadingModel::ordinary;
    geometry.source=CoJReloadGeometrySource::post_overlay;
    geometry.root=geometry.body=geometry.loader=geometry.holder=geometry.barrel=pinch;
    Check(BuildCoJWinchesterLoadingLip(geometry.model,geometry.body,geometry.lip),"fixture lip failed");
    geometry.valid=true;
    CoJReloadTraceSnapshot native{};
    native.valid=native.probe_valid=native.reloading=true;
    native.weapon_id=0x80000042U;native.weapon_class="WeaponRifleWinchester";
    native.armed_hand=0;native.probe_status=2;native.probe_until=5;
    native.states[0]={21,21,21,21};native.animation_id[0]=31;
    native.current_time[0]=2;native.start_time[0]=1;native.play_time[0]=1;native.duration[0]=2;
    native.loaded={3,0};native.reserves={0,10,0};native.capacities={12,0};
    native.ammo_readback=3;native.reserve_readback=10;
    CoJWinchesterLoadingSample sample{};
    Check(BuildCoJWinchesterLoadingSample(geometry,native,native,10,7,5,pinch,sample),
        "same-frame retained rifle sample rejected");
    Check(sample.valid&&sample.tip.valid&&sample.native.weapon_id==0x80000042U,
        "high-bit owner or actual tip lost");
    Check(Near(sample.tip.position.z,pinch.position.z+100.F*cojvr::runtime::ReloadCartridgeNoseOffsetM(
        cojvr::runtime::ReloadCartridgeAppearance::rifle))&&
        !Near(sample.tip.position.z,pinch.position.z+100.F*cojvr::runtime::ReloadCartridgeNoseOffsetM(
            cojvr::runtime::ReloadCartridgeAppearance::pistol)),
        "rifle observation did not track the displayed rifle nose");
    Check(CoJWinchesterLoadingSampleCanPublish(sample,10,7,5,true,true,true),"restored frame rejected");
    auto publication=QualifyCoJWinchesterLoadingPublication(sample,10,7,5,true,true,true,true,true,true);
    Check(publication.valid&&publication.tip.valid,"accepted drawn mesh lost nose observation");
    Check(!QualifyCoJWinchesterLoadingPublication(sample,10,7,5,true,true,true,false,true,true).valid,
        "mirror rejection published an observation");
    Check(!QualifyCoJWinchesterLoadingPublication(sample,10,7,5,true,true,true,true,false,true).valid,
        "pending graphics rollback published an observation");
    Check(!QualifyCoJWinchesterLoadingPublication(sample,10,7,5,true,true,false,true,true,true).valid,
        "failed final submission published an observation");
    cojvr::runtime::Pose head{},behind{};
    head.position_valid=head.orientation_valid=true;behind=head;behind.position.z=.1F;
    cojvr::runtime::StereoHudTextOverlay overlay{};
    const bool mesh_drawn=cojvr::runtime::BuildReloadCartridgeOverlay(head,behind,true,overlay);
    Check(!mesh_drawn,"behind-head imported cartridge fixture unexpectedly drew");
    publication=QualifyCoJWinchesterLoadingPublication(sample,10,7,5,true,true,true,true,true,mesh_drawn);
    Check(publication.valid&&publication.geometry.valid&&!publication.tip.valid&&!publication.tip_observation.valid,
        "hidden imported cartridge retained a displayed nose claim");
    for(int fault=0;fault<6;++fault)Check(!CoJWinchesterLoadingSampleCanPublish(sample,
        fault==0?11:10,fault==1?8:7,fault==2?9:5,fault!=3,fault!=4,fault!=5),
        "stale/unrestored/uncaptured frame published loading observation");
    for(int fault=0;fault<20;++fault){
        auto after=native;auto shape=geometry;std::uint64_t frame=10,generation=7,context=5;
        switch(fault){
        case 0:after.weapon_id=43;break;case 1:after.armed_hand=1;break;
        case 2:after.weapon_class="WeaponRifleWinchesterScope";break;
        case 3:after.states[0][0]=22;break;case 4:after.current_time[0]+=.01F;break;
        case 5:after.animation_id[0]=32;break;case 6:after.probe_status=3;break;
        case 7:after.probe_valid=false;break;case 8:after.loaded[0]++;break;
        case 9:after.reserves[1]--;break;case 10:after.play_time[0]=std::numeric_limits<float>::quiet_NaN();break;
        case 11:shape.model=CoJWinchesterLoadingModel::unknown;break;
        case 12:frame=0;break;case 13:generation=0;break;case 14:context=4;break;
        case 15:shape.source=CoJReloadGeometrySource::unknown;break;
        case 16:shape.lip[0].x+=1;break;
        case 17:after.drum_phase=60;break;
        case 18:after.drum_phase=std::numeric_limits<float>::infinity();break;
        case 19:after.weapon_id=native.weapon_id=0xffffffffU;break;
        }
        Check(!BuildCoJWinchesterLoadingSample(shape,native,after,frame,generation,context,pinch,sample),
            "changed native phase/owner or unqualified geometry retained a sample");
        Check(!sample.valid&&!sample.tip.valid&&!sample.geometry.valid,"failed sample retained stale measurements");
    }
    native.weapon_id=0x80000042U;
    // Natural pose is a separate baseline, never a displayed cartridge/socket.
    geometry.source=CoJReloadGeometrySource::pre_native;
    Check(BuildCoJWinchesterLoadingSample(geometry,native,native,10,7,5,pinch,sample)&&
        !sample.tip.valid,"natural baseline acquired a displayed tip");
    geometry.source=CoJReloadGeometrySource::post_overlay;
    Check(BuildCoJWinchesterLoadingSample(geometry,native,native,10,7,5,{},sample)&&
        !sample.tip.valid,"missing current pinch reused an older displayed tip");
    geometry.model=CoJWinchesterLoadingModel::scoped;
    native.weapon_class="WeaponRifleWinchesterScope";
    Check(BuildCoJWinchesterLoadingSample(geometry,native,native,10,7,5,pinch,sample),
        "separate exact scoped profile rejected");
    std::cout<<"Winchester same-frame loading observation passed\n";
}
