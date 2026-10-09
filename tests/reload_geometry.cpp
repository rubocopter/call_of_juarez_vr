#include "games/call_of_juarez/reload_geometry.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::games::call_of_juarez;
void Check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<1e-4F;}
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
    std::cout<<"exact reload cylinder geometry passed\n";
}
