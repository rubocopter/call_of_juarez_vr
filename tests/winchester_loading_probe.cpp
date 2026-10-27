#include "games/call_of_juarez/winchester_loading_probe.hpp"
#include "runtime/reload_visual.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
using namespace cojvr::games::call_of_juarez;
void Check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<.0001F;}
int main(){
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
