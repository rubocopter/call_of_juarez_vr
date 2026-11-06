// Exercise the real bridge: native black belts can own movie presentation
// without m_cMoviePlaying, which is only required for the skip-input lease.
#include <windows.h>
#include <array>
#include <string>
#include <vector>
#include <span>
#include "runtime/vr_types.hpp"
#include "runtime/hud_text.hpp"
#include "games/call_of_juarez/body_adapter.hpp"
#define private public
#include "games/call_of_juarez/java_player_bridge.hpp"
#undef private
#include "../src/games/call_of_juarez/java_player_bridge.cpp"
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace cojvr::games::call_of_juarez;
namespace {
int module,cls,movie,sequence_start,exception,game_class,active_field,movie_field,sequence_field;
void* table[176]{};void** env=table;
void* vm_table[8]{};void** vm=vm_table;
bool pending=false,timer=false,alive=true,mission=false,belt_movie=true,has_movie=false;
bool has_sequence=false;
bool field_fault=false,read_fault=false,global_fault=false;
bool sequence_lookup_fault=false,sequence_read_fault=false;
int globals=0,locals=0;
void Need(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
void Business(){Need(!pending,"JNI business operation with pending exception");}
int __stdcall GetEnv(void*,void** out,int){*out=&env;return 0;}
void* __stdcall Exception(void*){if(!pending)return nullptr;++locals;return &exception;}
void __stdcall Clear(void*){pending=false;}
void __stdcall FixtureDeleteLocal(void*,void*){Need(!pending,"local deleted before exception cleared");--locals;}
void* __stdcall StaticObject(void*,void*,void*){Business();++locals;return &module;}
std::uint8_t __stdcall Instance(void*,void* object,void*){Business();return object==&module;}
void* __stdcall Class(void*,void* object){Business();Need(object==&module,"wrong presentation owner");++locals;return &cls;}
void* __stdcall Method(void*,void*,const char* name,const char* signature){
    Business();Need(!std::strcmp(signature,"()Z"),"unexpected presentation signature");
    return const_cast<char*>(name);
}
std::uint8_t __stdcall Boolean(void*,void*,void* method,const void*){
    Business();const auto name=static_cast<const char*>(method);
    if(!std::strcmp(name,"IsTimerFreezed"))return timer;
    if(!std::strcmp(name,"IsMainPlayerAlive"))return alive;
    if(!std::strcmp(name,"IsMenuMissionEndVisible"))return mission;
    Need(!std::strcmp(name,"IsAnyMoviePlaying"),"unexpected presentation getter");return belt_movie;
}
void* __stdcall Field(void*,void*,const char* name,const char* signature){
    Business();
    const bool primary=!std::strcmp(name,"m_cMoviePlaying")&&!std::strcmp(signature,"LMovie;");
    Need(primary||(!std::strcmp(name,"m_CurrentSkipableMovieStart")&&
        !std::strcmp(signature,"LInstantSkipableMovieStart;")),"wrong movie/sequence field");
    if(field_fault||(!primary&&sequence_lookup_fault)){pending=true;return nullptr;}
    return primary?&movie_field:&sequence_field;
}
void* __stdcall Object(void*,void*,void* field){
    Business();if(read_fault||(field==&sequence_field&&sequence_read_fault)){pending=true;return nullptr;}
    if(field==&sequence_field){if(!has_sequence)return nullptr;++locals;return &sequence_start;}
    Need(field==&movie_field,"wrong input target field");
    if(!has_movie)return nullptr;++locals;return &movie;
}
void* __stdcall NewGlobal(void*,void* object){
    Business();if(global_fault){pending=true;return nullptr;}++globals;return object;
}
void __stdcall DeleteGlobal(void*,void*){Business();--globals;}
std::uint8_t __stdcall Same(void*,void* a,void* b){Business();return a==b;}
template<class T>void Set(int index,T fn){table[index]=reinterpret_cast<void*>(fn);}
void CheckClean(){Need(!pending&&locals==0,"presentation observation leaked exception/local references");}
}

int main(){
    vm_table[6]=reinterpret_cast<void*>(GetEnv);
    Set(15,Exception);Set(17,Clear);Set(21,NewGlobal);Set(22,DeleteGlobal);Set(23,FixtureDeleteLocal);Set(24,Same);
    Set(31,Class);Set(32,Instance);Set(33,Method);Set(39,Boolean);Set(94,Field);Set(95,Object);Set(145,StaticObject);
    JavaPlayerBridge bridge;
    bridge.vm_=&vm;bridge.lawman_game_class_=&game_class;
    bridge.lawman_module_single_class_=&cls;bridge.active_game_module_field_=&active_field;
    CoJPresentationOwner owner{};std::string error;
    Need(bridge.TryGetActiveGamePresentationOwner(owner,&error)&&owner.valid&&owner.movie_playing,
        "native belt-only movie was rejected because no skip target exists");
    Need(owner.blocked()&&!owner.immersive_movie_allowed()&&owner.movie_input_epoch==0,
        "unbound black belts must preserve theater/subtitles and deny gameplay/skip identity");CheckClean();
    has_sequence=true;
    Need(bridge.TryGetActiveGamePresentationOwner(owner)&&owner.immersive_movie_allowed()&&owner.movie_input_epoch!=0&&globals==2,
        "native skippable sequence with no Movie object did not get a distinct input lease");CheckClean();
    const auto sequence_epoch=owner.movie_input_epoch;
    has_movie=true;
    Need(bridge.TryGetActiveGamePresentationOwner(owner)&&owner.movie_input_epoch>sequence_epoch&&globals==2,
        "real Movie did not replace the fallback sequence identity");CheckClean();
    has_movie=has_sequence=false;
    Need(bridge.TryGetActiveGamePresentationOwner(owner)&&owner.movie_input_epoch==0&&globals==0,
        "no-target transition kept a Movie/sequence skip lease");CheckClean();
    for(int blocker=0;blocker<3;++blocker){
        timer=blocker==0;alive=blocker!=1;mission=blocker==2;
        Need(bridge.TryGetActiveGamePresentationOwner(owner)&&owner.blocked()&&!owner.immersive_movie_allowed(),
            "belt-only movie bypassed pause/loading/death/mission blocker");CheckClean();
    }
    timer=mission=false;alive=true;has_movie=true;
    Need(bridge.TryGetActiveGamePresentationOwner(owner)&&owner.movie_input_epoch!=0&&globals==2,
        "real movie failed to retain its separate skip lease");CheckClean();
    const auto first_epoch=owner.movie_input_epoch;
    has_movie=false;
    Need(bridge.TryGetActiveGamePresentationOwner(owner)&&owner.movie_playing&&owner.movie_input_epoch==0&&globals==0,
        "real-to-belt-only transition rejected presentation or kept stale skip globals");CheckClean();
    has_movie=true;
    Need(bridge.TryGetActiveGamePresentationOwner(owner)&&owner.movie_input_epoch>first_epoch,
        "returning real movie reused the retired input epoch");CheckClean();
    read_fault=true;
    Need(!bridge.TryGetActiveGamePresentationOwner(owner)&&!owner.valid&&owner.movie_input_epoch==0&&globals==2,
        "failed native read published the previous movie lease or lost retryable globals");CheckClean();
    read_fault=false;has_movie=false;
    Need(bridge.TryGetActiveGamePresentationOwner(owner)&&owner.movie_input_epoch==0&&globals==0,
        "null movie did not recover and retire the old skip lease after a read fault");CheckClean();
    Need(bridge.movie_input_owner_.Clear(&env)&&globals==0,"fixture input-owner cleanup failed");
    has_movie=true;
    for(int fault=0;fault<3;++fault){
        bridge.movie_input_field_=nullptr;field_fault=fault==0;read_fault=fault==1;global_fault=fault==2;
        Need(!bridge.TryGetActiveGamePresentationOwner(owner)&&!owner.valid&&owner.blocked()&&owner.movie_input_epoch==0,
            "actual JNI failure authorized a partial movie owner");CheckClean();
        Need(globals==0,"failed input lease acquisition leaked globals");
    }
    field_fault=read_fault=global_fault=false;belt_movie=false;has_movie=false;
    belt_movie=true;has_sequence=true;
    for(int fault=0;fault<2;++fault){
        sequence_lookup_fault=fault==0;sequence_read_fault=fault==1;
        Need(!bridge.TryGetActiveGamePresentationOwner(owner)&&!owner.valid&&owner.movie_input_epoch==0,
            "failed sequence owner lookup/read was treated as known absent input");CheckClean();
        Need(globals==0,"failed sequence owner observation retained new globals");
    }
    sequence_lookup_fault=sequence_read_fault=false;belt_movie=has_sequence=false;
    Need(bridge.TryGetActiveGamePresentationOwner(owner)&&!owner.blocked()&&!owner.immersive_movie_allowed(),
        "movie end did not restore ordinary presentation");CheckClean();
    std::cout<<"presentation bridge ownership checks passed\n";
}
