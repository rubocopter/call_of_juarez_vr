#include "games/call_of_juarez/reload_geometry.hpp"
#include "games/call_of_juarez/schofield_chamber_guide.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::games::call_of_juarez;
void Check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<1e-4F;}

void CheckSchofieldGuides() {
    using namespace cojvr::runtime;
    CoJReloadGeometrySnapshot geometry{};
    geometry.valid=true;
    geometry.model=CoJReloadModel::schofield;
    geometry.source=CoJReloadGeometrySource::post_overlay;
    geometry.drum={{0,0,50},{0,1,0},{0,0,1},true};
    geometry.gate=geometry.drum;
    // Live barrel-local +X is the gate/drum forward, and +Y is gate up.
    geometry.barrel=geometry.root={{0,0,50},{0,1,0},{-1,0,0},true};
    Check(BuildCoJReloadMouthFrames(geometry.model,geometry.drum,geometry.mouths),
        "Schofield fixture has no mouths");

    CoJLoadingMouthGuideFrame frame{};
    frame.head.position_valid=frame.head.orientation_valid=true;
    frame.origin_cm={0,0,100};frame.right={1,0,0};frame.up={0,1,0};frame.forward={0,0,1};
    frame.frame=10;frame.player=7;frame.context=5;frame.weapon=0x80000042;
    frame.armed_hand=0;frame.allowed=frame.restored=true;
    CoJReloadTraceSnapshot native{};
    native.valid=native.probe_valid=native.reloading=true;
    native.probe_status=2;native.weapon_class="WeaponPistolSchofield";
    native.weapon_id=frame.weapon;native.armed_hand=0;native.kind=CoJManualReloadKind::clip;
    native.capacities={6,0};native.reserves={10,0,0};native.reserve_readback=10;
    StereoHudTextOverlay overlay{};
    overlay.frame_sequence=frame.frame;overlay.reload_feedback.owner_token=17;

    const auto at_mouth=[&](std::size_t slot,std::size_t mouth,unsigned state=0) {
        const auto& surface=overlay.holster_guides.zones[slot];
        Vec3 centre{};
        for(const auto& p:surface.head_corners) {
            centre.x+=p.x*.25F;centre.y+=p.y*.25F;centre.z+=p.z*.25F;
        }
        const auto expected=geometry.mouths[mouth].position;
        return surface.kind==GestureGuideKind::loading_mouth&&surface.state==state&&
            Near(centre.x,expected.x*.01F)&&Near(centre.y,expected.y*.01F)&&
            Near(centre.z,(50.F+1.8852F-100.F)*.01F);
    };
    for(int loaded=0;loaded<=6;++loaded) {
        geometry.schofield_loaded=native.loaded[0]=native.ammo_readback=loaded;
        overlay.holster_guides={};
        const bool appended=AppendCoJSchofieldChamberGuides(frame,geometry,native,true,overlay);
        Check(appended&&overlay.holster_guides.count==6,
            "Schofield must publish all six chamber positions before ejection");
        for(std::size_t i=0;i<overlay.holster_guides.count;++i)
            Check(at_mouth(i,i,i<static_cast<unsigned>(6-loaded)?0:2),
                "Schofield guide did not distinguish loaded rounds in rear-view order");
    }

    geometry.schofield_loaded=native.loaded[0]=native.ammo_readback=0;
    overlay.holster_guides.count=2;
    Check(!AppendCoJSchofieldChamberGuides(frame,geometry,native,true,overlay)&&
        overlay.holster_guides.count==2,"six empty rings must append atomically");
    overlay.holster_guides.count=1;
    Check(AppendCoJSchofieldChamberGuides(frame,geometry,native,true,overlay)&&
        overlay.holster_guides.count==7&&at_mouth(1,0)&&at_mouth(6,5),
        "all six empty rings must fit after an existing guide");

    geometry.schofield_loaded=native.loaded[0]=native.ammo_readback=5;
    geometry.native_drum_phase_degrees=native.drum_phase=60;
    geometry.drum.up={-.8660254F,.5F,0};
    Check(BuildCoJReloadMouthFrames(geometry.model,geometry.drum,geometry.mouths),
        "Schofield phase fixture has no mouths");
    overlay.holster_guides={};
    Check(AppendCoJSchofieldChamberGuides(frame,geometry,native,true,overlay)&&
        overlay.holster_guides.count==6&&at_mouth(0,1),
        "a 60-degree drum turn should move a different mouth into the upper-right slot");
    auto no_reserve=native;
    no_reserve.reserve_readback=no_reserve.reserves[0]=0;
    auto empty_reserve_overlay=overlay;
    empty_reserve_overlay.holster_guides={};
    Check(AppendCoJSchofieldChamberGuides(frame,geometry,no_reserve,true,empty_reserve_overlay)&&
        empty_reserve_overlay.holster_guides.count==6,
        "a current waiting drum's empty count is independent of reserve");
    auto emptied_geometry=geometry;
    auto emptied_native=native;
    emptied_geometry.schofield_loaded=emptied_native.loaded[0]=emptied_native.ammo_readback=0;
    auto emptied=overlay;emptied.holster_guides={};
    Check(AppendCoJSchofieldChamberGuides(frame,emptied_geometry,emptied_native,true,emptied)&&
        emptied.holster_guides.count==6&&std::ranges::all_of(
            emptied.holster_guides.zones.begin(),emptied.holster_guides.zones.begin()+6,
            [](const auto& zone){return zone.state==0;}),
        "six empty rings must stay visible after the waiting drum is unloaded");

    for(int fault=0;fault<21;++fault) {
        auto view=frame;
        auto snapshot=geometry;
        auto current=native;
        auto out=overlay;
        bool enabled=true;
        switch(fault) {
        case 0:enabled=false;break;
        case 1:view.restored=false;break;
        case 2:view.allowed=false;break;
        case 3:view.frame++;break;
        case 4:view.player=0;break;
        case 5:view.context++;break;
        case 6:current.weapon_id++;break;
        case 7:current.armed_hand=1;break;
        case 8:current.probe_status=3;break;
        case 9:current.drum_phase=-60;break;
        case 10:current.loaded[0]=current.ammo_readback=4;break;
        case 11:snapshot.source=CoJReloadGeometrySource::pre_native;break;
        case 12:snapshot.mouths[0].position.x+=1;break;
        case 13:snapshot.gate.forward={1,0,0};break;
        case 14:out.reload_feedback.owner_token=0;break;
        case 15:out.holster_guides.count=static_cast<unsigned>(out.holster_guides.zones.size());break;
        case 16:view.origin_cm={0,0,0};view.right={-1,0,0};view.forward={0,0,-1};break;
        case 17:view.origin_cm={-50,0,48};view.right={0,0,1};view.forward={-1,0,0};break;
        case 18:out.frame_sequence++;break;
        case 19:current.weapon_class="WeaponPistolSchofield_A";break;
        case 20:out.eyes[1].eye_to_head.position={.5F,0,0};
            out.eyes[1].eye_to_head.position_valid=true;break;
        }
        const auto count=out.holster_guides.count;
        Check(!AppendCoJSchofieldChamberGuides(view,snapshot,current,enabled,out)&&
            out.holster_guides.count==count&&out.holster_guides.zones[0].state==0,
            "Schofield guide changed output for a foreign, invalid or culled capture");
    }
}

int main(){
    // Native mechanical motion must be measured in the weapon root frame.
    const ElementWorldBasisTarget root{{0,0,0},{0,1,0},{0,0,1},true};
    const ElementWorldBasisTarget gate{{0,2,0},{0,1,0},{0,0,1},true};
    const ElementWorldBasisTarget rotated_root{{10,20,30},{-1,0,0},{0,0,1},true};
    const ElementWorldBasisTarget rotated_gate{{8,20,30},{-1,0,0},{0,0,1},true};
    CoJReloadLocalFrame before{}, after{};
    Check(BuildCoJReloadLocalFrame(root,gate,before),"native baseline invalid");
    Check(BuildCoJReloadLocalFrame(rotated_root,rotated_gate,after),"rigidly moved weapon invalid");
    Check(Near(before.position.x,after.position.x)&&Near(before.position.y,after.position.y)&&
          Near(before.position.z,after.position.z),"global VR motion mistaken for gate travel");
    Check(Near(before.up.y,after.up.y)&&Near(before.forward.z,after.forward.z),
          "global VR rotation mistaken for gate hinge motion");
    auto opened=gate;opened.up={1,0,0};opened.forward={0,0,1};
    Check(BuildCoJReloadLocalFrame(root,opened,after),"independent gate rotation not observed");
    Check(!Near(before.up.y,after.up.y),"gate hinge motion hidden");
    opened.position.x=std::numeric_limits<float>::quiet_NaN();
    Check(!BuildCoJReloadLocalFrame(root,opened,after),"non-finite gate pose admitted");
    Check(!after.valid,"invalid native pose retained old measurement");
    ElementWorldBasisTarget drum{{100,200,300},{0,1,0},{0,0,1},true};
    std::array<ElementWorldBasisTarget,6> mouths{};
    Check(BuildCoJReloadMouthFrames(CoJReloadModel::peacemaker,drum,mouths),"known cylinder rejected");
    Check(Near(mouths[3].position.x,98.765282F)&&Near(mouths[3].position.y,199.287538F)&&Near(mouths[3].position.z,297.616722F),"measured Peacemaker rear mouth not transformed in centimetres");
    drum.up={-1,0,0}; drum.forward={0,0,1};
    Check(BuildCoJReloadMouthFrames(CoJReloadModel::frontier,drum,mouths),"rotated Frontier cylinder rejected");
    Check(Near(mouths[2].position.x,100.010422F)&&Near(mouths[2].position.y,198.814174F)&&Near(mouths[2].position.z,297.989515F),"native cylinder phase must rotate chamber geometry");
    Check(mouths[2].up.x==-1&&mouths[2].forward.z==1,"mouth must retain actual insertion basis");
    drum.forward={0,0,2};
    Check(!BuildCoJReloadMouthFrames(CoJReloadModel::frontier,drum,mouths),"scaled frame admitted");
    for(const auto& mouth:mouths)Check(!mouth.valid,"failed observation retained previous mouths");
    drum={{},{0,1,0},{0,0,1},true};
    Check(!BuildCoJReloadMouthFrames(CoJReloadModel::unknown,drum,mouths),"unknown model admitted");
    Check(BuildCoJReloadMouthFrames(CoJReloadModel::schofield,drum,mouths),"Schofield rear face unavailable");
    Check(mouths[0].position.x>0 && mouths[0].position.y>0 &&
          mouths[1].position.x>0 && mouths[1].position.y<0 &&
          mouths[2].position.y<-1.F && mouths[3].position.x<0 &&
          mouths[4].position.x<0 && mouths[5].position.y>1.F,
          "Schofield presentation must empty clockwise from top-right in model space");
    for(int loaded=0;loaded<=6;++loaded) {
        const auto mask=CoJSchofieldEmptyMask(loaded);
        Check(mask && *mask==(1U<<(6-loaded))-1U,"Schofield display must derive empties from loaded count");
    }
    Check(!CoJSchofieldEmptyMask(-1) && !CoJSchofieldEmptyMask(7),"invalid Schofield count admitted");
    drum.position.x=std::numeric_limits<float>::quiet_NaN();
    Check(!BuildCoJReloadMouthFrames(CoJReloadModel::peacemaker,drum,mouths),"nonfinite frame admitted");
    // Geometry ranking is diagnostic only: a gate element's origin is not a
    // measured loading-port center, and no result can admit a VR insertion.
    CoJReloadGeometrySnapshot snapshot{};
    snapshot.valid=true;snapshot.model=CoJReloadModel::peacemaker;
    snapshot.source=CoJReloadGeometrySource::post_overlay;
    snapshot.root=root;snapshot.barrel=root;
    snapshot.drum={{0,0,0},{0,1,0},{0,0,1},true};
    Check(BuildCoJReloadMouthFrames(snapshot.model,snapshot.drum,snapshot.mouths),"fixture mouths unavailable");
    snapshot.gate=snapshot.mouths[3];
    auto rank=RankCoJReloadGatePivotMouths(snapshot,true);
    Check(rank.status==CoJReloadPortRankStatus::pivot_only && rank.nearest_mouth==3,
        "closest native chamber mouth was not ranked");
    Check(!rank.can_accept && !rank.gate_open_confirmed,
        "unverified mechanical pivot authorized physical insertion");
    Check(Near(rank.nearest_distance_cm,0),"gate pivot distance must be in centimetres");
    auto shifted=snapshot;
    const cojvr::runtime::Vec3 offset{10,20,30};
    const auto move=[&](ElementWorldBasisTarget& f){
        f.position.x+=offset.x;f.position.y+=offset.y;f.position.z+=offset.z;};
    move(shifted.root);move(shifted.barrel);move(shifted.drum);move(shifted.gate);
    for(auto& mouth:shifted.mouths)move(mouth);
    auto moved_rank=RankCoJReloadGatePivotMouths(shifted,true);
    Check(moved_rank.status==rank.status && moved_rank.nearest_mouth==rank.nearest_mouth,
        "rigid world translation changed chamber ranking");
    auto turned=snapshot;
    const auto turn=[](ElementWorldBasisTarget& frame){
        const auto rotate=[](cojvr::runtime::Vec3 v){
            return cojvr::runtime::Vec3{-v.y,v.x,v.z};};
        frame.position=rotate(frame.position);
        frame.up=rotate(frame.up);
        frame.forward=rotate(frame.forward);
    };
    turn(turned.root);turn(turned.barrel);turn(turned.drum);turn(turned.gate);
    for(auto& mouth:turned.mouths)turn(mouth);
    auto turned_rank=RankCoJReloadGatePivotMouths(turned,true);
    Check(turned_rank.status==rank.status && turned_rank.nearest_mouth==rank.nearest_mouth,
        "rigid world rotation changed chamber ranking");
    auto rotated=snapshot;
    // Sixty-degree native phase moves another actual mouth into the same spot.
    rotated.drum.up={-.8660254F,.5F,0};
    Check(BuildCoJReloadMouthFrames(rotated.model,rotated.drum,rotated.mouths),"rotated phase rejected");
    auto rotated_rank=RankCoJReloadGatePivotMouths(rotated,true);
    Check(rotated_rank.status==CoJReloadPortRankStatus::pivot_only &&
        rotated_rank.nearest_mouth==1,"native cylinder phase ignored");
    auto frontier=snapshot;
    frontier.model=CoJReloadModel::frontier;
    Check(BuildCoJReloadMouthFrames(frontier.model,frontier.drum,frontier.mouths),
        "Frontier native mouth frame unavailable");
    frontier.gate=frontier.mouths[2];
    const auto frontier_rank=RankCoJReloadGatePivotMouths(frontier,true);
    Check(frontier_rank.status==CoJReloadPortRankStatus::pivot_only &&
        frontier_rank.nearest_mouth==2 && !frontier_rank.can_accept,
        "Frontier model incorrectly admitted or ranked");
    auto ambiguous=snapshot;
    ambiguous.gate.position={
        (snapshot.mouths[3].position.x+snapshot.mouths[4].position.x)*.5F,
        (snapshot.mouths[3].position.y+snapshot.mouths[4].position.y)*.5F,
        snapshot.mouths[3].position.z};
    auto tie=RankCoJReloadGatePivotMouths(ambiguous,true);
    Check(tie.status==CoJReloadPortRankStatus::ambiguous && tie.nearest_mouth==-1,
        "equidistant chamber mouths were treated as a socket");
    auto bad=snapshot;
    Check(RankCoJReloadGatePivotMouths(bad,false).status==CoJReloadPortRankStatus::invalid,
        "lost weapon ownership produced a ranked socket");
    bad.source=CoJReloadGeometrySource::pre_native;
    Check(RankCoJReloadGatePivotMouths(bad,true).status==CoJReloadPortRankStatus::invalid,
        "natural native observation mistaken for displayed VR geometry");
    bad=snapshot;bad.model=CoJReloadModel::unknown;
    Check(RankCoJReloadGatePivotMouths(bad,true).status==CoJReloadPortRankStatus::invalid,
        "unknown weapon model produced a ranked socket");
    bad=snapshot;bad.mouths[0].position.x+=1.F;
    Check(RankCoJReloadGatePivotMouths(bad,true).status==CoJReloadPortRankStatus::invalid,
        "stale chamber positions were accepted against drum pose");
    bad=snapshot;bad.gate.up={0,2,0};
    Check(RankCoJReloadGatePivotMouths(bad,true).status==CoJReloadPortRankStatus::invalid,
        "scaled gate frame admitted");
    bad=snapshot;bad.root.position.x=std::numeric_limits<float>::infinity();
    Check(RankCoJReloadGatePivotMouths(bad,true).status==CoJReloadPortRankStatus::invalid,
        "non-finite weapon identity frame admitted");
    CheckSchofieldGuides();
    std::cout<<"exact reload cylinder geometry passed\n";
}
