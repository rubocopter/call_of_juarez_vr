#include "games/call_of_juarez/remington_loading_probe.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::games::call_of_juarez;
void Check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
bool Near(float a,float b){return std::abs(a-b)<.0002F;}
int main(){
    ElementWorldBasisTarget f{{100,200,300},{0,0,1},{1,0,0},true};
    CoJRemingtonLoadingGeometry g{};
    g.model=CoJRemingtonLoadingModel::ordinary;g.source=CoJReloadGeometrySource::post_overlay;
    g.root=g.body=g.lock=g.holder=g.barrel=f;
    Check(BuildCoJRemingtonLoadingMouths(g.model,f,g.mouths),"mouth transform failed");g.valid=true;
    Check(Near(g.mouths[0].position.x,92.23F)&&Near(g.mouths[0].position.y,201)&&
        Near(g.mouths[0].position.z,302.98F)&&Near(g.mouths[1].position.y,199),
        "mouths did not follow animated barrel basis in centimetres");
    CoJReloadTraceSnapshot n{};n.valid=n.probe_valid=n.reloading=true;
    n.weapon_id=0x80000042;n.weapon_class="WeaponRifleRemington";n.armed_hand=0;
    n.probe_status=2;n.kind=CoJManualReloadKind::indexed;n.index_count=2;n.capacities={1,1};
    n.reserve_kind=CoJReloadReserveKind::shotgun;n.reserve_kinds.fill(n.reserve_kind);
    n.reserve_indices={2,2};n.reserves={0,0,10};n.reserve_readback=10;
    CoJRemingtonLoadingSample s{};
    Check(BuildCoJRemingtonLoadingSample(g,n,n,10,7,5,f,s)&&s.tip.valid,"valid sample rejected");
    for(int fault=0;fault<15;++fault){
        auto a=n;auto shape=g;
        switch(fault){
        case 0:a.weapon_id++;break;case 1:a.armed_hand=1;break;
        case 2:a.selected_index=1;break;case 3:a.loaded[1]++;break;
        case 4:a.current_time[0]=.1F;break;case 5:a.animation_id[1]++;break;
        case 6:a.reserve_indices[1]=1;break;case 7:a.probe_status=3;break;
        case 8:a.play_time[1]=std::numeric_limits<float>::infinity();break;
        case 9:shape.barrel.up=shape.barrel.forward;break;
        case 10:shape.mouths[0].position.x++;break;
        case 11:a.index_count=1;break;case 12:a.capacities[0]=2;break;
        case 13:a.weapon_class="WeaponRifleRemingtonSawOff";break;
        case 14:a.native_body_cycle=!a.native_body_cycle;break;
        }
        Check(!BuildCoJRemingtonLoadingSample(shape,n,a,10,7,5,f,s)&&!s.valid,
            "changed phase/owner/counts/geometry admitted");
    }
    n.armed_hand=1;
    Check(!BuildCoJRemingtonLoadingSample(g,n,n,10,7,5,f,s),"full shotgun admitted in left hand");
    g.model=CoJRemingtonLoadingModel::sawed_off;n.weapon_class="WeaponRifleRemingtonSawOff";
    Check(BuildCoJRemingtonLoadingMouths(g.model,f,g.mouths),"sawed-off transform failed");
    for(int hand=0;hand<2;++hand)for(int index=0;index<2;++index){
        n.armed_hand=hand;n.selected_index=index;
        Check(BuildCoJRemingtonLoadingSample(g,n,n,10,7,5,f,s),"sawed-off hand/index rejected");
    }
    auto p=QualifyCoJRemingtonLoadingPublication(s,10,7,5,true,true,true,true,true,false);
    Check(p.valid&&!p.tip.valid,"suppressed mesh retained displayed tip");
    for(int fault=0;fault<8;++fault)
        Check(!QualifyCoJRemingtonLoadingPublication(s,fault==0?11:10,fault==1?8:7,
            fault==2?9:5,fault!=3,fault!=4,fault!=5,fault!=6,fault!=7,true).valid,
            "stale/unrestored/unsubmitted sample published");
    g.source=CoJReloadGeometrySource::pre_native;
    Check(BuildCoJRemingtonLoadingSample(g,n,n,10,7,5,f,s)&&!s.tip.valid,
        "natural pose acquired displayed tip");
    auto invalid=n;invalid.reserve_readback=-1;
    Check(!BuildCoJRemingtonLoadingSample(g,invalid,invalid,10,7,5,f,s),"invalid reserve readback admitted");
    invalid=n;invalid.ammo_readback=1;
    Check(!BuildCoJRemingtonLoadingSample(g,invalid,invalid,10,7,5,f,s),"inconsistent slot readback admitted");
    for(int fault=0;fault<3;++fault)
        Check(!BuildCoJRemingtonLoadingSample(g,n,n,fault==0?0:10,fault==1?0:7,fault==2?4:5,f,s),
            "missing frame/generation/gameplay context admitted");
    std::array<ElementWorldBasisTarget,2> mouths{};
    f.position.x=std::numeric_limits<float>::quiet_NaN();
    Check(!BuildCoJRemingtonLoadingMouths(g.model,f,mouths)&&!mouths[0].valid,
        "nonfinite barrel retained mouth geometry");
    std::cout<<"Remington loading observation checks passed\n";
}
