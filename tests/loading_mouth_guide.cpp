#include "games/call_of_juarez/loading_mouth_guide.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr;
using namespace cojvr::games::call_of_juarez;
void Need(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<.00001F;}
runtime::Vec3 Centre(const runtime::HolsterGuideSurface& g){runtime::Vec3 p{};
    for(auto v:g.head_corners){p.x+=v.x*.25F;p.y+=v.y*.25F;p.z+=v.z*.25F;}return p;}
CoJLoadingMouthGuideFrame Frame(){
    CoJLoadingMouthGuideFrame f{};f.head.position_valid=f.head.orientation_valid=true;
    f.origin_cm={100,200,300};f.right={1,0,0};f.up={0,1,0};f.forward={0,0,1};
    f.frame=10;f.player=7;f.context=5;f.weapon=0x80000042;f.armed_hand=0;f.allowed=f.restored=true;return f;
}
CoJReloadTraceSnapshot Native(bool indexed){
    CoJReloadTraceSnapshot n{};n.valid=n.probe_valid=n.reloading=true;n.probe_status=2;
    n.armed_hand=0;n.weapon_id=0x80000042;n.kind=indexed?CoJManualReloadKind::indexed:CoJManualReloadKind::unit;
    n.weapon_class=indexed?"WeaponRifleRemington":"WeaponRifleWinchester";
    n.index_count=indexed?2:1;n.capacities=indexed?std::array<int,2>{1,1}:std::array<int,2>{12,0};
    n.reserve_kind=indexed?CoJReloadReserveKind::shotgun:CoJReloadReserveKind::rifle;
    n.reserve_kinds.fill(n.reserve_kind);n.reserve_indices.fill(indexed?2:1);
    n.reserves=indexed?std::array<int,3>{0,0,10}:std::array<int,3>{0,10,0};
    n.reserve_readback=10;n.native_body_cycle=true;return n;
}
int main(){
    auto f=Frame();ElementWorldBasisTarget root{{100,200,240},{0,1,0},{0,0,1},true};
    CoJWinchesterLoadingGeometry g{};g.model=CoJWinchesterLoadingModel::ordinary;
    g.source=CoJReloadGeometrySource::post_overlay;g.root=g.body=g.loader=g.holder=g.barrel=root;
    Need(BuildCoJWinchesterLoadingLip(g.model,g.body,g.lip),"fixture lip failed");g.valid=true;
    auto n=Native(false);CoJWinchesterLoadingSample s{};
    Need(BuildCoJWinchesterLoadingSample(g,n,n,10,7,5,{},s),"fixture Winchester phase invalid");
    runtime::StereoHudTextOverlay out{};out.frame_sequence=10;out.reload_feedback.owner_token=77;
    Need(AppendCoJWinchesterLoadingGuide(f,s,true,out)&&out.holster_guides.count==1,
        "current displayed Winchester mouth did not get a guide");
    auto c=Centre(out.holster_guides.zones[0]);
    Need(Near(c.x,.01228641F)&&Near(c.y,-.17347876F)&&Near(c.z,-.64344577F),
        "mouth guide used grip/pivot, wrong units or wrong camera origin");
    Need(out.holster_guides.zones[0].state==0,"geometry reference claimed insertion readiness");
    for(int fault=0;fault<11;++fault){auto bad=f;auto sample=s;auto overlay=out;overlay.holster_guides.count=0;
        bool enabled=true;switch(fault){
        case 0:enabled=false;break;case 1:bad.restored=false;break;case 2:bad.frame++;break;
        case 3:bad.player++;break;case 4:bad.context++;break;case 5:bad.weapon++;break;
        case 6:sample.geometry.source=CoJReloadGeometrySource::pre_native;break;
        case 7:sample.native.probe_status=1;break;case 8:sample.geometry.lip[0].x+=1;break;
        case 9:bad.head.orientation_valid=false;break;case 10:overlay.reload_feedback.owner_token=0;break;}
        Need(!AppendCoJWinchesterLoadingGuide(bad,sample,enabled,overlay)&&!overlay.holster_guides.count,
            "unqualified/foreign/hidden mouth published a guide");}
    // Moving the root by a metre moves the marker by that metre, with no cached anchor.
    g.root.position.x+=100;g.body=g.loader=g.holder=g.barrel=g.root;
    Need(BuildCoJWinchesterLoadingLip(g.model,g.body,g.lip)&&BuildCoJWinchesterLoadingSample(g,n,n,11,7,5,{},s),"moved sample failed");
    out.holster_guides={};out.frame_sequence=f.frame=11;
    Need(AppendCoJWinchesterLoadingGuide(f,s,true,out)&&Near(Centre(out.holster_guides.zones[0]).x,1.01228641F),
        "guide did not follow current displayed weapon translation");
    // Rotate camera and weapon together. Use the supplied exact camera right
    // axis, rather than deriving one from another sign convention.
    f=Frame();f.right={0,0,-1};f.forward={1,0,0};
    g.model=CoJWinchesterLoadingModel::scoped;n.weapon_class="WeaponRifleWinchesterScope";
    g.root={{40,200,300},{0,1,0},{1,0,0},true};g.body=g.loader=g.holder=g.barrel=g.root;
    Need(BuildCoJWinchesterLoadingLip(g.model,g.body,g.lip)&&BuildCoJWinchesterLoadingSample(g,n,n,10,7,5,{},s),"scoped rotated fixture failed");
    out.frame_sequence=10;out.holster_guides={};
    Need(AppendCoJWinchesterLoadingGuide(f,s,true,out),"rotated scoped port hidden");
    c=Centre(out.holster_guides.zones[0]);
    Need(Near(c.x,.01228641F)&&Near(c.z,-.64344577F),"camera rotation changed the physical port relation");
    f.origin_cm.y+=20;f.head.position.y=.1F;out.holster_guides={};
    Need(AppendCoJWinchesterLoadingGuide(f,s,true,out)&&Near(Centre(out.holster_guides.zones[0]).y,-.47347876F),
        "physical camera correction or HMD translation applied twice/omitted");
    CoJRemingtonLoadingGeometry r{};r.model=CoJRemingtonLoadingModel::ordinary;r.source=CoJReloadGeometrySource::post_overlay;
    r.root=r.body=r.lock=r.holder=r.barrel=root;
    Need(BuildCoJRemingtonLoadingMouths(r.model,r.barrel,r.mouths),"fixture mouths failed");r.valid=true;
    n=Native(true);CoJRemingtonLoadingSample rs{};
    Need(BuildCoJRemingtonLoadingSample(r,n,n,10,7,5,{},rs),"fixture Remington invalid");
    f=Frame();out.frame_sequence=10;out.holster_guides={};
    Need(AppendCoJRemingtonLoadingGuides(f,rs,true,out)&&out.holster_guides.count==2,
        "both measured barrel mouths need independent location markers");
    const auto a=Centre(out.holster_guides.zones[0]),b=Centre(out.holster_guides.zones[1]);
    Need(Near(a.x,.01F)&&Near(b.x,-.01F)&&Near(a.y,.0298F)&&Near(a.z,-.6777F),
        "Remington markers did not follow measured animated barrel geometry");
    auto full=out;full.holster_guides.count=full.holster_guides.zones.size()-1;
    Need(!AppendCoJRemingtonLoadingGuides(f,rs,true,full)&&full.holster_guides.count==full.holster_guides.zones.size()-1,
        "two-port publication must reject capacity shortage atomically");
    rs.geometry.mouths[1].position.x+=1;out.holster_guides={};
    Need(!AppendCoJRemingtonLoadingGuides(f,rs,true,out)&&!out.holster_guides.count,"forged chamber location admitted");
    r.model=CoJRemingtonLoadingModel::sawed_off;n.weapon_class="WeaponRifleRemingtonSawOff";
    n.armed_hand=f.armed_hand=1;
    Need(BuildCoJRemingtonLoadingSample(r,n,n,10,7,5,{},rs),"left-owned SawOff fixture failed");
    Need(AppendCoJRemingtonLoadingGuides(f,rs,true,out)&&out.holster_guides.count==2,
        "left-owned SawOff lost its current barrel mouths");
    std::cout<<"Loading-mouth location, ownership, phase and publication checks passed\n";
}
