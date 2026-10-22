// Compile the actual render-hook restoration blocks and bridge; only the JNI/native
// boundary is simulated. Removing owner quarantine or failure retention must fail.
#include <windows.h>
#undef near
#undef far
#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <span>
#define private public
#include "games/call_of_juarez/java_player_bridge.hpp"
#undef private
#include "../src/games/call_of_juarez/java_player_bridge.cpp"
#include "../src/games/call_of_juarez/camera_probe.cpp"

using namespace cojvr::games::call_of_juarez;
namespace {
struct Vector { float v[3]{}; };
struct Actor { JavaPlayerPosition position{}; std::array<ElementWorldBasisTarget,32> frames{}; int refs=0; } old_actor,new_actor;
int type,vector_type,exception;
void* env_table[180]{};void** env_holder=env_table;
void* vm_table[8]{};void** vm_holder=vm_table;
bool pending=false,fail_write=false,fail_read=false,fail_position=false,wrong_readback=false;
int writes=0,fail_write_at=-1;
int failures=0;
int natural_renders=0;
bool mounted=false,mounted_fault=false,replace_during_mount_observation=false;
int actor_turns=0;
void __fastcall Natural(void*,void*,void*){++natural_renders;}
void Check(bool ok,const char* why){if(!ok){std::cerr<<"FAIL "<<why<<'\n';++failures;}}
void* Id(int i){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(i));}
std::int32_t __stdcall Env(void*,void** env,std::int32_t){*env=&env_holder;return 0;}
void* __stdcall Exception(void*){return pending?&exception:nullptr;}
void __stdcall Clear(void*){pending=false;}
void __stdcall Drop(void*,void* p){if(p!=&type&&p!=&exception&&p!=&old_actor&&p!=&new_actor)delete static_cast<Vector*>(p);}
void* __stdcall Global(void*,void* p){if(p==&old_actor||p==&new_actor)++static_cast<Actor*>(p)->refs;return p;}
void __stdcall DeleteGlobal(void*,void* p){if(p==&old_actor||p==&new_actor)--static_cast<Actor*>(p)->refs;}
void* __stdcall Class(void*,void*){return &type;}
void* __stdcall Field(void*,void*,const char*,const char*){return nullptr;}
void* __stdcall Method(void*,void*,const char* name,const char*){
    if(!std::strcmp(name,"GetPositionVectorVolatile"))return Id(10);
    if(!std::strcmp(name,"FromUpForwardPosElementWorld"))return Id(11);
    if(!std::strcmp(name,"GetElementPos"))return Id(12);
    if(!std::strcmp(name,"GetElementLeftVector"))return Id(13);
    if(!std::strcmp(name,"GetElementUpVector"))return Id(14);
    if(!std::strcmp(name,"GetBoneJointPos"))return Id(15);
    if(!std::strcmp(name,"IsRidingHorse"))return Id(16);
    return Id(99);
}
void* __stdcall New(void*,void*,void*,const void*){return new Vector;}
float __stdcall Float(void*,void* obj,void* field){return static_cast<Vector*>(obj)->v[reinterpret_cast<std::uintptr_t>(field)-1];}
void __stdcall SetFloat(void*,void* obj,void* field,float value){static_cast<Vector*>(obj)->v[reinterpret_cast<std::uintptr_t>(field)-1]=value;}
void* __stdcall Object(void*,void*,void*){return nullptr;}
void __stdcall SetObject(void*,void*,void*,void*){}
// JNI jvalue occupies eight bytes on x86.
union JArg {void* l;std::int32_t i;float f;double d;std::int64_t j;};
void* __stdcall CallObject(void*,void* obj,void*,const JArg*){
    if(fail_position){pending=true;return nullptr;}
    const auto p=static_cast<Actor*>(obj)->position;return new Vector{{p.x,p.y,p.z}};
}
std::uint8_t __stdcall Boolean(void*,void* obj,void* method,const JArg* args){
    if(method==Id(16)){
        if(mounted_fault)pending=true;
        if(replace_during_mount_observation)g_java_player_bridge.being_generation_=8;
        return mounted?1:0;
    }
    if(fail_read){pending=true;return 0;}
    auto& frame=static_cast<Actor*>(obj)->frames[args[0].i];
    auto v=frame.position;
    if(method==Id(15)) {
        const int bone=args[0].i;
        const int element=bone==7?1:bone==8?2:bone==10?4:0;
        v=static_cast<Actor*>(obj)->frames[element].position;
    }
    if(method==Id(13))v={1,0,0};
    if(method==Id(14))v=frame.up;
    if(wrong_readback&&method==Id(12))v.x+=10;
    auto* out=static_cast<Vector*>(args[1].l);*out={{v.x,v.y,v.z}};return 1;
}
void __stdcall CallVoid(void*,void* obj,void* method,const JArg* args){
    if(method==Id(17)){++actor_turns;return;}
    if(method!=Id(11))return;
    if(fail_write||++writes==fail_write_at){pending=true;return;}
    auto& frame=static_cast<Actor*>(obj)->frames[args[0].i];
    const auto vec=[](void* p){auto* v=static_cast<Vector*>(p);return cojvr::runtime::Vec3{v->v[0],v->v[1],v->v[2]};};
    frame.up=vec(args[1].l);frame.forward=vec(args[2].l);frame.position=vec(args[3].l);frame.valid=true;
}
void Bind(){
    env_table[15]=(void*)&Exception;env_table[17]=(void*)&Clear;env_table[21]=(void*)&Global;env_table[22]=(void*)&DeleteGlobal;
    env_table[23]=(void*)&Drop;env_table[30]=(void*)&New;env_table[31]=(void*)&Class;env_table[33]=(void*)&Method;
    env_table[36]=(void*)&CallObject;env_table[39]=(void*)&Boolean;env_table[63]=(void*)&CallVoid;
    env_table[94]=(void*)&Field;env_table[95]=(void*)&Object;env_table[102]=(void*)&Float;env_table[104]=(void*)&SetObject;env_table[111]=(void*)&SetFloat;
    vm_table[6]=(void*)&Env;
    auto& b=g_java_player_bridge;b.vm_=&vm_holder;b.being_=&old_actor;b.being_generation_=7;old_actor.refs=1;
    b.vector_class_=&vector_type;b.vector_constructor_=Id(99);b.vector_x_field_=Id(1);b.vector_y_field_=Id(2);b.vector_z_field_=Id(3);
    b.get_position_vector_method_=Id(10);b.set_element_world_basis_method_=Id(11);b.element_world_basis_lookup_attempted_=true;
    b.get_element_position_method_=Id(12);b.get_element_left_method_=Id(13);b.get_element_up_method_=Id(14);b.element_world_read_lookup_attempted_=true;
    b.get_bone_joint_method_=Id(15);b.get_bone_direction_method_=Id(13);b.get_bone_perpendicular_method_=Id(14);b.bone_read_lookup_attempted_=true;
    for(auto* actor:{&old_actor,&new_actor})for(auto& f:actor->frames)f={{0,0,0},{0,1,0},{0,0,1},true};
}
void MountedYaw(){
    auto& b=g_java_player_bridge;b.rotate_horizontally_method_=Id(17);
    PlayerBodyPositionUpdate update{};update.actor_yaw_update_valid=true;
    update.actor_yaw_generation=7;update.pending_actor_yaw_delta_degrees=25;
    update.pending_actor_yaw_target_degrees=35;
    g_body_yaw_owned=true;g_body_yaw_generation=7;g_body_owned_yaw_degrees=10;
    mounted=true;CommitBodyYawOwnership(update,1);
    Check(actor_turns==0&&g_body_owned_yaw_degrees==10,
        "late mount allowed HMD body-follow to rotate the rider or consume camera compensation");
    mounted=false;mounted_fault=true;CommitBodyYawOwnership(update,2);
    Check(actor_turns==0&&g_body_owned_yaw_degrees==10&&!pending,
        "unknown mount owner allowed HMD actor yaw or leaked JNI exception");
    mounted_fault=false;update.actor_yaw_generation=8;CommitBodyYawOwnership(update,3);
    Check(actor_turns==0&&g_body_yaw_generation==7,
        "body yaw mutated a replacement actor generation");
    update.actor_yaw_generation=7;replace_during_mount_observation=true;
    CommitBodyYawOwnership(update,4);
    Check(actor_turns==0&&g_body_yaw_generation==7,
        "actor replacement during mount observation escaped the commit guard");
    replace_during_mount_observation=false;b.being_generation_=7;
    update.actor_yaw_generation=7;CommitBodyYawOwnership(update,4);
    Check(actor_turns==1&&g_body_owned_yaw_degrees==35,
        "verified on-foot body-follow failed after dismount");
    update.actor_yaw_update_valid=false;CommitBodyYawOwnership(update,5);
    Check(actor_turns==1,"cancelled yaw update reached native rotation");
    mounted=true;
    Check(b.TryRotateHorizontally(5,nullptr)&&actor_turns==2,
        "mounted HMD guard disabled the explicit rider-look/native callback route");
}
void Pelvis(){
    auto& b=g_java_player_bridge;auto& tx=g_room_scale_body_offset;
    tx={};tx.active=true;tx.being_generation=7;tx.pelvis_element=12;tx.natural_position={100,200,300};tx.natural_up={0,1,0};tx.natural_forward={0,0,1};
    Check(b.RetainBodyRestoreOwner(7),"retain pelvis owner");old_actor.frames[12].position={101,200,300};
    fail_write=true;Check(!RestoreRoomScaleBodyOffset(1,"failure")&&tx.active,"failed pelvis restore must remain active");fail_write=false;
    const bool replaced=b.ResolveBeingMethods(&env_holder,&new_actor,nullptr);
    Check(!replaced&&b.being_==&old_actor&&old_actor.refs==1,"pending pelvis restore must quarantine bridge replacement");
    if(replaced)return;
    b.ClearBeing(&env_holder);b.Reset();
    Check(b.being_==&old_actor&&old_actor.refs==1&&b.body_restore_owner_count_==1,
        "ClearBeing/Reset dropped pending actor or lease");
    Check(!b.ReleaseBodyRestoreOwner(8)&&!b.RetainBodyRestoreOwner(8),"foreign generation changed lease ownership");
    tx.being_generation=8;const int before=writes;
    Check(!RestoreRoomScaleBodyOffset(2,"wrong_generation")&&writes==before&&tx.active,
        "wrong-generation pelvis restore wrote native state");tx.being_generation=7;
    wrong_readback=true;
    Check(!RestoreRoomScaleBodyOffset(2,"bad_readback")&&tx.active&&b.body_restore_owner_count_==1,
        "successful pelvis setter with bad readback retired the lease");wrong_readback=false;
    fail_read=true;
    Check(!RestoreRoomScaleBodyOffset(2,"read_fault")&&tx.active&&b.body_restore_owner_count_==1,
        "pelvis JNI read failure retired the lease");fail_read=false;
    fail_position=true;const int before_position_fault=writes;
    Check(!RestoreRoomScaleBodyOffset(2,"actor_unavailable")&&tx.active&&writes==before_position_fault&&!pending,
        "pelvis restore wrote without the retained actor position");fail_position=false;
    old_actor.position={5,0,0};
    Check(b.RetainBodyRestoreOwner(7),"retain sibling body transaction");
    Check(RestoreRoomScaleBodyOffset(2,"retry")&&!tx.active,"pelvis restore must retry original owner");
    Check(old_actor.frames[12].position.x==105&&new_actor.frames[12].position.x==0,"restore touched replacement or lost original preimage/rebase");
    Check(b.body_restore_owner_count_==1&&!b.ResolveBeingMethods(&env_holder,&new_actor,nullptr),
        "restoring pelvis released the sibling transaction's actor");
    Check(b.ReleaseBodyRestoreOwner(7)&&b.body_restore_owner_count_==0&&!b.ReleaseBodyRestoreOwner(7),
        "pelvis lease leaked or underflowed");
    Check(b.ResolveBeingMethods(&env_holder,&new_actor,nullptr)&&b.being_==&new_actor,"verified restore must allow replacement");
}
void PelvisApply(){
    auto& b=g_java_player_bridge;auto& tx=g_room_scale_body_offset;
    tx={};tx.being_generation=7;tx.pelvis_element=12;tx.model_observed=true;
    old_actor.frames[12].position={100,200,300};
    CameraRenderStateSnapshot natural{};
    const std::array<float,16> matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::memcpy(natural.source_world.data(),matrix.data(),sizeof(matrix));
    cojvr::runtime::Pose head{{0,1.7F,0},{},true,true};
    auto invalid=head;invalid.position_valid=false;
    Check(!ApplyRoomScaleBodyOffset(natural,invalid,1,nullptr)&&b.body_restore_owner_count_==0,
        "skipped pelvis apply acquired an owner lease");
    Check(ApplyRoomScaleBodyOffset(natural,head,1,nullptr)&&tx.active&&b.body_restore_owner_count_==1,
        "actual zero-horizontal-offset pelvis apply did not capture its transaction");
    Check(RestoreRoomScaleBodyOffset(1,"zero_offset")&&!tx.active&&b.body_restore_owner_count_==0,
        "zero-horizontal-offset transaction leaked a lease");
    head.position.x=.1F;fail_write=true;
    Check(!ApplyRoomScaleBodyOffset(natural,head,1,nullptr)&&tx.active&&b.body_restore_owner_count_==1,
        "failed first pelvis mutation lost the captured owner/preimage");
    g_original_render_view=reinterpret_cast<RenderViewFn>(&Natural);
    HookRenderViewImpl(nullptr,nullptr,nullptr);
    Check(tx.active&&b.body_restore_owner_count_==1&&natural_renders==0,
        "render hook forwarded native work with a pending pelvis restore");
    fail_write=false;
    g_long_gun_quarantined=true;
    HookRenderViewImpl(nullptr,nullptr,nullptr);
    Check(!tx.active&&b.body_restore_owner_count_==0&&natural_renders==1&&
        old_actor.frames[12].position.x==100,"failed pelvis apply did not recover its original preimage");
    Check(!g_long_gun_quarantined,"verified cleanup permanently blocked ordinary fire");
    cojvr::runtime::GameplayInputState input{};input.active=true;input.input_context_generation=1;
    input.digital_available=~0U;input.fire_right=true;
    Check(!g_gameplay_controls.Update(input).fire_right,"held fire escaped cleanup release barrier");
    input.fire_right=false;(void)g_gameplay_controls.Update(input);
    input.fire_right=true;
    Check(g_gameplay_controls.Update(input).fire_right,"fresh ordinary fire failed after verified cleanup/release");
    g_original_render_view=nullptr;natural_renders=0;
}
void Arm(){
    auto& b=g_java_player_bridge;auto& tx=g_left_arm_rotation;tx={};tx.active=true;tx.being_generation=7;tx.elements={1,2,3,4};
    Check(b.RetainBodyRestoreOwner(7),"retain arm owner");
    g_arm_element_cache.left_upper_arm=1;g_arm_element_cache.left_forearm=2;g_arm_element_cache.left_foretwist=3;g_arm_element_cache.left_hand=4;
    tx.natural_geometry.upper_element_position={10,0,0};tx.natural_geometry.forearm_element_position={20,0,0};
    tx.natural_geometry.foretwist_element_position={30,0,0};tx.natural_geometry.hand_element_position={40,0,0};
    tx.natural_geometry.upper_element_up=tx.natural_geometry.forearm_element_up=tx.natural_geometry.foretwist_element_up=tx.natural_geometry.hand_element_up={0,1,0};
    tx.natural_geometry.upper_element_forward=tx.natural_geometry.forearm_element_forward=tx.natural_geometry.foretwist_element_forward=tx.natural_geometry.hand_element_forward={0,0,1};
    tx.natural_geometry.shoulder={10,0,0};tx.natural_geometry.elbow={20,0,0};tx.natural_geometry.wrist={40,0,0};
    fail_write=true;
    Check(!RestoreAppliedArmRotation(true,tx,1,"failure"),"injected arm restore should fail");
    Check(tx.active,"failed diagnostic arm restore must remain active");
    fail_write=false;
    const int before=writes;fail_position=true;
    Check(!RestoreAppliedArmRotation(true,tx,1,"actor_unavailable")&&tx.active&&writes==before,
        "unavailable actor position allowed arm writes or retired transaction");fail_position=false;
    tx.being_generation=8;
    Check(!RestoreAppliedArmRotation(true,tx,1,"wrong_generation")&&tx.active&&writes==before,
        "wrong-generation arm restore wrote native state");tx.being_generation=7;
    fail_write_at=writes+2;
    Check(!RestoreAppliedArmRotation(true,tx,1,"partial_write")&&tx.active&&b.body_restore_owner_count_==1,
        "partial arm restore lost owner or preimage");fail_write_at=-1;
    fail_read=true;
    Check(!RestoreAppliedArmRotation(true,tx,1,"read_fault")&&tx.active&&b.body_restore_owner_count_==1,
        "arm read fault retired pending restore");fail_read=false;
    fail_read=true;g_original_render_view=reinterpret_cast<RenderViewFn>(&Natural);
    HookRenderViewImpl(nullptr,nullptr,nullptr);
    Check(tx.active&&natural_renders==0,"render hook forwarded native work with a pending arm restore");
    fail_read=false;g_original_render_view=nullptr;
    wrong_readback=true;
    Check(!RestoreAppliedArmRotation(true,tx,1,"bad_readback")&&tx.active&&b.body_restore_owner_count_==1,
        "arm readback disagreement retired pending restore");wrong_readback=false;
    b.ClearBeing(&env_holder);b.Reset();
    Check(b.being_==&old_actor&&old_actor.refs==1&&b.body_restore_owner_count_==1,
        "arm lease did not quarantine ClearBeing/Reset");
    Check(!b.ResolveBeingMethods(&env_holder,&new_actor,nullptr),"unverified arm restore allowed replacement");
    fail_write=true;
    ShutdownCameraProbe();
    Check(tx.active&&b.being_==&old_actor&&b.body_restore_owner_count_==1,
        "shutdown discarded pending arm preimage/owner");
    fail_write=false;
    old_actor.position={5,0,0};
    Check(RestoreAppliedArmRotation(true,tx,1,"retry")&&!tx.active&&b.body_restore_owner_count_==0,
        "arm restoration could not retry and verify");
    Check(old_actor.frames[1].position.x==15&&old_actor.frames[4].position.x==45&&
        new_actor.frames[1].position.x==0,"arm retry lost actor rebase or touched replacement");
    Check(b.ResolveBeingMethods(&env_holder,&new_actor,nullptr),"verified arm restore still blocked replacement");
}
}
int main(int argc,char** argv){Bind();if(argc>1&&std::string(argv[1])=="yaw")MountedYaw();else if(argc>1&&std::string(argv[1])=="arm")Arm();else {PelvisApply();Pelvis();}
std::cout<<(failures?"FAILED":"PASS")<<" body restoration production/JNI fixture\n";return failures?1:0;}
