#include "games/call_of_juarez/presentation_owner.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace cojvr::games::call_of_juarez;
namespace {
void Check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int module,cls,exception,refs,step,fail=-1;
bool pending,timer,alive=true,mission,movie,null_failure;
void* table[174]{};void** env=table;
bool Fault(){Check(!pending,"JNI called after pending exception");if(++step!=fail)return false;pending=!null_failure;return true;}
void* __stdcall Class(void*,void* object){Check(object==&module,"wrong module owner");if(Fault())return nullptr;++refs;return &cls;}
void* __stdcall Method(void*,void* object,const char* name,const char* descriptor){
    Check(object==&cls&&!std::strcmp(descriptor,"()Z"),"wrong class/getter signature");
    Check(!std::strcmp(name,"IsTimerFreezed")||!std::strcmp(name,"IsMainPlayerAlive")||!std::strcmp(name,"IsMenuMissionEndVisible")||!std::strcmp(name,"IsAnyMoviePlaying"),"unapproved presentation getter");
    if(Fault())return nullptr;return const_cast<char*>(name);
}
std::uint8_t __stdcall Boolean(void*,void* object,void* method,const void* args){
    Check(object==&module&&args==nullptr,"wrong getter arguments");if(Fault())return 0;
    const auto name=static_cast<const char*>(method);
    return !std::strcmp(name,"IsTimerFreezed")?timer:!std::strcmp(name,"IsMainPlayerAlive")?alive:!std::strcmp(name,"IsMenuMissionEndVisible")?mission:movie;
}
void* __stdcall Exception(void*){if(!pending)return nullptr;++refs;return &exception;}
void __stdcall Clear(void*){pending=false;}
void __stdcall Delete(void*,void* object){Check(object==&cls||object==&exception,"unexpected local ref");--refs;}
template<class T>void Set(int slot,T fn){table[slot]=reinterpret_cast<void*>(fn);}
void Reset(){pending=timer=mission=movie=null_failure=false;alive=true;step=refs=0;fail=-1;}
}
int main(){
    Set(31,Class);Set(33,Method);Set(39,Boolean);Set(15,Exception);Set(17,Clear);Set(23,Delete);
    CoJPresentationOwner state{};
    Check(state.blocked(),"unknown presentation must not grant native stereo/gameplay");
    Check(!state.immersive_movie_allowed(),"unknown owner cannot grant cinematic camera rendering");
    Reset();Check(ReadCoJPresentationOwner(&env,&module,state)&&!state.blocked(),"live ordinary gameplay rejected");
    const int operations=step;
    Reset();alive=false;Check(ReadCoJPresentationOwner(&env,&module,state)&&state.blocked()&&!state.timer_frozen,"death with running timer must yield stereo and gameplay");
    Reset();mission=true;Check(ReadCoJPresentationOwner(&env,&module,state)&&state.blocked()&&state.player_alive&&!state.timer_frozen,"failed mission with living player must show native menu");
    Reset();timer=true;Check(ReadCoJPresentationOwner(&env,&module,state)&&state.blocked(),"pause/loading must retain flat presentation");
    Reset();Check(ReadCoJPresentationOwner(&env,&module,state)&&!state.blocked(),"fresh live owner must recover after mission UI closes");
    Reset();movie=true;Check(ReadCoJPresentationOwner(&env,&module,state)&&state.blocked()&&state.player_alive&&!state.timer_frozen,
        "scripted movie with running timer must yield camera/body to native flat presentation");
    Check(refs==0&&!pending,"movie observation leaked JNI state");
    Check(state.immersive_movie_allowed(),"verified live scripted movie must admit opt-in camera rendering");
    // Cinematic eligibility never grants gameplay/body ownership. Cover every
    // native blocker, including frozen movies whose pause ownership is unknown.
    for (unsigned mask=0;mask<32;++mask) {
        CoJPresentationOwner owner{};
        owner.valid=(mask&1)!=0;
        owner.timer_frozen=(mask&2)!=0;
        owner.player_alive=(mask&4)!=0;
        owner.mission_end_visible=(mask&8)!=0;
        owner.movie_playing=(mask&16)!=0;
        Check(owner.immersive_movie_allowed()==(mask==21),
            "cinematic camera must reject dead, frozen, missing, mission-end or non-movie owners");
        if(owner.movie_playing)Check(owner.blocked(),"movie camera permission leaked into gameplay/body");
        Check(owner.settings_hands_allowed(true)==(mask==5),
            "settings capture must retain tracked hand presentation only for live gameplay owner");
        Check(!owner.settings_hands_allowed(false),"ordinary ownership cannot enter settings visual transaction");
    }
    Reset();Check(ReadCoJPresentationOwner(&env,&module,state)&&!state.blocked(),"movie end must restore ordinary eligibility");
    Check(!state.immersive_movie_allowed(),"movie exit must retire cinematic camera permission");
    Reset();pending=true;
    Check(!ReadCoJPresentationOwner(&env,&module,state)&&!pending&&refs==0&&step==0,
        "preexisting JNI exception must clear before further observation");
    Reset();void* lookup=table[33];table[33]=nullptr;
    Check(!ReadCoJPresentationOwner(&env,&module,state)&&state.blocked()&&step==0,
        "missing JNI function cannot authorize stereo");table[33]=lookup;
    for(bool null_only:{false,true})for(int operation=1;operation<=operations;++operation){
        Reset();null_failure=null_only;fail=operation;
        const bool ok=ReadCoJPresentationOwner(&env,&module,state);
        // Boolean getter zero without an exception is a valid native false.
        const bool zero_boolean=null_only&&(operation==3||operation==5||operation==7||operation==9);
        Check(ok==zero_boolean,"JNI failure must not publish a partial presentation owner");
        Check(refs==0&&!pending,"presentation observation leaked reference/exception");
        if(!ok)Check(!state.valid&&state.blocked()&&!state.immersive_movie_allowed(),"failed read retained live authorization");
    }
    std::cout<<"presentation owner checks passed\n";
}
