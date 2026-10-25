#include "games/call_of_juarez/bow_visual_geometry.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace cojvr::games::call_of_juarez;
namespace {
void Check(bool value,const char* reason){if(!value){std::cerr<<reason<<'\n';std::exit(1);}}
void Near(const CoJEquipmentFrame& a,const CoJEquipmentFrame& b){
    for(std::size_t i=0;i<16;++i)Check(std::abs(a.values[i]-b.values[i])<1e-6,"frame mismatch");
}
CoJEquipmentFrame Product(const CoJEquipmentFrame& a,const CoJEquipmentFrame& b){
    CoJEquipmentFrame out;Check(ComposeCoJEquipmentFrames(a,b,out),"compose failed");return out;
}
}
int main(){
    const auto identity=CoJEquipmentIdentity();
    CoJEquipmentFrame scaled{{{0,2,0,0, -3,0,0,0, 0,0,4,0, 17,-3,12,1}}},inverse;
    Check(InvertCoJEquipmentFrame(scaled,inverse),"scaled/rolled inverse failed");
    Near(Product(scaled,inverse),identity);Near(Product(inverse,scaled),identity);
    auto aliased=scaled;
    Check(InvertCoJEquipmentFrame(aliased,aliased),"alias inverse failed");Near(aliased,inverse);
    for(double invalid:{0.0,1e-12,-1.0,std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}){
        auto frame=identity;frame.values[0]=invalid;
        Check(!InvertCoJEquipmentFrame(frame,inverse),"invalid scale admitted");
        Check(!ValidateCoJEquipmentFrame(inverse),"failed inverse retained output");
    }
    auto projective=identity;projective.values[3]=0.1;
    Check(!InvertCoJEquipmentFrame(projective,inverse),"projective admitted");
    auto parallel=identity;parallel.values[4]=1;parallel.values[5]=1e-9;
    Check(!InvertCoJEquipmentFrame(parallel,inverse),"near-collinear admitted");

    constexpr std::array<double,4> distances{{12.472265,9.770338,12.976699,10.743553}};
    std::size_t index=0;
    for(const auto& profile:kCoJHolsterAssetProfiles){
        for(auto body:{kCoJRayEquipmentMeshSha256,kCoJBillyEquipmentMeshSha256}){
            for(auto side:{CoJHolsterSide::right,CoJHolsterSide::left}){
                CoJHolsterGeometry geometry;
                const bool expected=body==kCoJRayEquipmentMeshSha256 || side==CoJHolsterSide::right;
                const bool built=BuildCoJHolsterGeometry(profile.mesh_sha256,body,side,scaled,geometry);
                Check(built==expected,"body mount availability mismatch");
                if(!built)continue;
                Near(Product(profile.holster,geometry.weapon_world),scaled);
                Near(Product(profile.holder,geometry.weapon_world),geometry.grasp_world);
                const auto& v=geometry.grasp_in_holster.values;
                Check(std::abs(std::sqrt(v[12]*v[12]+v[13]*v[13]+v[14]*v[14])-distances[index])<1e-5,
                      "measured grasp displacement changed");
                auto shift=identity;shift.values[12]=7;shift.values[13]=8;
                CoJHolsterGeometry moved;
                Check(BuildCoJHolsterGeometry(profile.mesh_sha256,body,side,Product(scaled,shift),moved),"moved mount failed");
                Near(Product(geometry.grasp_world,shift),moved.grasp_world);
                Check(!geometry.contact_admitted && !geometry.mesh_attach_admitted,"geometry acquired authority");
            }
        }
        ++index;
    }
    CoJHolsterGeometry rejected;
    const auto hash=kCoJHolsterAssetProfiles[0].mesh_sha256;
    Check(!BuildCoJHolsterGeometry("ColtPeacemaker.msh",kCoJRayEquipmentMeshSha256,CoJHolsterSide::right,identity,rejected),"filename admitted");
    Check(!BuildCoJHolsterGeometry(hash,"unknown",CoJHolsterSide::right,identity,rejected),"unknown body admitted");
    Check(!BuildCoJHolsterGeometry(hash,kCoJRayEquipmentMeshSha256,CoJHolsterSide::unknown,identity,rejected),"unknown side admitted");
    Check(!BuildCoJHolsterGeometry(hash,kCoJRayEquipmentMeshSha256,CoJHolsterSide::right,projective,rejected),"invalid mount admitted");
    Check(CoJHolsterBodyMountElement(kCoJRayEquipmentMeshSha256,CoJHolsterSide::left)==8,"Ray left index");
    Check(CoJHolsterBodyMountElement(kCoJBillyEquipmentMeshSha256,CoJHolsterSide::right)==16,"Billy right index");

    CoJBowVisualSample sample;
    sample.actor=0x81234567;sample.bow=12;sample.arrow=13;sample.parent=sample.actor;
    sample.generation=5;sample.frame=7;sample.parent_element=42;sample.resolved_right_element=42;
    sample.holder_element=0;sample.mesh_element=1;sample.player_sha256=kCoJRayEquipmentMeshSha256;
    sample.bow_sha256=kCoJBowVisualMeshSha256;sample.arrow_sha256=kCoJBowArrowVisualMeshSha256;
    sample.exact_weapon_bow=true;sample.visible=true;
    sample.hand_world=identity;sample.arrow_object_world=scaled;
    sample.holder_world=scaled;sample.holder_world.values[14]-=51; // current animation
    sample.mesh_world=scaled;sample.mesh_world.values[13]+=2;
    const auto original=sample;
    CoJBowVisualObservation observation;
    Check(ObserveCoJBowVisual(sample,sample,observation),"coherent bow observation rejected");
    Near(Product(observation.object_in_hand,sample.hand_world),sample.arrow_object_world);
    Near(Product(observation.holder_in_object,sample.arrow_object_world),sample.holder_world);
    Near(Product(observation.mesh_in_object,sample.arrow_object_world),sample.mesh_world);
    Near(original.holder_world,sample.holder_world);
    Check(!observation.writer_admitted && !observation.string_writer_admitted,"observer acquired writer authority");
    CoJEquipmentFrame mapped,restored;
    Check(RemapCoJEquipmentElement(scaled,identity,sample.holder_world,mapped),"animated map failed");
    Check(RemapCoJEquipmentElement(identity,scaled,mapped,restored),"reverse value map failed");Near(restored,sample.holder_world);
    for(int fault=0;fault<14;++fault){
        auto changed=sample;
        switch(fault){
        case 0:++changed.actor;break;case 1:++changed.bow;break;case 2:++changed.arrow;break;
        case 3:++changed.parent;break;case 4:++changed.generation;break;case 5:++changed.frame;break;
        case 6:++changed.parent_element;break;case 7:changed.arrow_sha256="unknown";break;
        case 8:changed.visible=false;break;case 9:changed.holder_element=-1;break;
        case 10:changed.exact_weapon_bow=false;break;case 11:changed.actor=0;break;
        case 12:changed.arrow=0xffffffffU;break;case 13:changed.mesh_world=projective;break;
        }
        Check(!ObserveCoJBowVisual(sample,changed,observation),"changed snapshot admitted");
        if(fault!=1 && fault!=2 && fault!=4 && fault!=5)
            Check(!ObserveCoJBowVisual(changed,changed,observation),"invalid coherent snapshot admitted");
    }
    std::cout<<"exact equipment inverse/scale, 12 holster mappings, bow observation/reversible value maps passed\n";
}
