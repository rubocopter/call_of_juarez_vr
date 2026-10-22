#include "games/call_of_juarez/movie_skip.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>

namespace {
using cojvr::games::call_of_juarez::DispatchCoJMovieSkip;
#if defined(_WIN32)
#define TEST_JNICALL __stdcall
#else
#define TEST_JNICALL
#endif
void Require(bool value,const char* message) {
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}
void* P(int value){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));}
constexpr int module=100,replacement_module=101,movie_a=200,movie_b=201,clazz=300,exception=999;
// Movie A/B deliberately share a native fake ID. No GetThisID is exposed: only
// IsSameObject can distinguish replacement from a new handle to the same movie.
constexpr int fake_movie_ids[]{0xf123,0xf123};
void* table[224]{};void** env=table;
std::map<void*,int> locals;
int next_local=1000,steps=0,fail_step=-1,null_step=-1,presses=0,releases=0;
int press_step=0,release_step=0,field_reads=0,owner_rounds=0,eligibility_reads=0;
int current_movie=movie_a,active_module=module,change=0,change_at=0,change_kind=0,validations=0;
bool pending=false,frozen=false,alive=true,mission_end=false,movie_playing=true;
bool skippable=true,mission_ended=false,hint=false,precaching=false;
bool press_completed=false,finish_on_press=true,return_ref_on_throw=false;
int Identity(void* value) {
    if(!value)return 0;
    const auto it=locals.find(value);
    if(it!=locals.end())return it->second;
    Require(value==P(module),"borrowed or deleted JNI handle used");
    return module;
}
void* Local(int object) {
    if(!object)return nullptr;
    auto result=P(++next_local);locals.emplace(result,object);return result;
}
bool Step(bool key=false) {
    Require(!pending,"business JNI call with pending exception");
    Require(key||!press_completed,"eligibility/identity queried after completed press");
    ++steps;
    if(steps==fail_step){pending=true;return false;}
    return steps!=null_step;
}
void TEST_JNICALL Delete(void*,void* value) {
    Require(!pending,"local cleanup before clearing exception");
    Require(locals.erase(value)==1,"borrowed/double-deleted local reference");
}
// ExceptionOccurred/Clear/DeleteLocalRef cannot throw in this JNI contract.
void* TEST_JNICALL Exception(void*){return pending?Local(exception):nullptr;}
void TEST_JNICALL Clear(void*){Require(pending,"cleared absent exception");pending=false;}
void* TEST_JNICALL Retain(void*,void* value) {
    const int object=Identity(value);const bool ok=Step();
    return ok || (pending&&return_ref_on_throw)?Local(object):nullptr;
}
void* TEST_JNICALL Class(void*,void* value) {
    Require(Identity(value)==module,"class queried outside retained module");
    const bool ok=Step();
    return ok || (pending&&return_ref_on_throw)?Local(clazz):nullptr;
}
const char* names[]{"IsTimerFreezed","IsMainPlayerAlive","IsMenuMissionEndVisible",
    "IsAnyMoviePlaying","CanSkipMovie","OnInputKey"};
void* TEST_JNICALL Method(void*,void* type,const char* name,const char* descriptor) {
    Require(Identity(type)==clazz,"method resolved on unexpected class");
    if(!Step())return nullptr;
    for(int i=0;i<6;++i)if(!std::strcmp(name,names[i])){
        Require(!std::strcmp(descriptor,i==5?"(IZC)V":"()Z"),"wrong method signature");
        return P(i+1);
    }
    Require(false,"unpermitted getter, skip mutation, factory or fallback method");return nullptr;
}
void* TEST_JNICALL Field(void*,void* type,const char* name,const char* descriptor) {
    Require(Identity(type)==clazz,"field resolved on unexpected class");
    Require(!std::strcmp(name,"m_cMoviePlaying")&&!std::strcmp(descriptor,"LMovie;"),
        "unexpected field or numeric movie identity queried");
    return Step()?P(10):nullptr;
}
void ChangeOwner() {
    if(change==1)current_movie=movie_b;
    if(change==2)current_movie=0;
    if(change==3)movie_playing=false;
    if(change==4)frozen=true;
    if(change==5)alive=false;
    if(change==6)mission_end=true;
    if(change==7)active_module=replacement_module;
}
void* TEST_JNICALL ObjectField(void*,void* value,void* field) {
    Require(Identity(value)==module&&field==P(10),"wrong movie field owner");
    if(++field_reads==change_at&&change_kind==1)ChangeOwner();
    const bool ok=Step();
    return ok || (pending&&return_ref_on_throw)?Local(current_movie):nullptr;
}
std::uint8_t TEST_JNICALL Same(void*,void* a,void* b) {
    const int first=Identity(a),second=Identity(b);
    Require((first==movie_a||first==movie_b)&&(second==movie_a||second==movie_b),
        "same-object proof used outside retained/current movie");
    return Step()&&first==second;
}
// Independent JNI argument layout, including one-byte jboolean and jchar.
union Value {std::int64_t alignment;std::int32_t i;std::uint8_t z;std::uint16_t c;void* l;};
static_assert(sizeof(Value)==8);
std::uint8_t TEST_JNICALL Boolean(void*,void* value,void* method,const void* args) {
    Require(Identity(value)==module&&!args,"eligibility queried on wrong owner/with arguments");
    if(!Step())return !pending&&(method==P(1)||method==P(3))?1:0;
    if(method==P(1)){
        if(++owner_rounds==change_at&&change_kind==2)ChangeOwner();
        return frozen;
    }
    if(method==P(2))return alive;
    if(method==P(3))return mission_end;
    if(method==P(4))return movie_playing;
    if(method==P(5)){
        ++eligibility_reads;
        const bool eligible=skippable&&!mission_ended&&!mission_end&&!hint&&!precaching&&current_movie!=0;
        if(change_kind==3)ChangeOwner();
        return eligible;
    }
    Require(false,"unpermitted boolean getter");return 0;
}
void TEST_JNICALL Key(void*,void* value,void* method,const Value* args) {
    Require(Identity(value)==module&&method==P(6),"key retargeted away from retained module");
    Require(args&&args[0].i==1&&args[2].c==0,"wrong native Escape key/character");
    Require(args[1].z<=1,"invalid jboolean");
    if(args[1].z){
        ++presses;press_step=steps+1;
        Require(current_movie==movie_a&&movie_playing&&!frozen&&alive&&!mission_end,
            "Escape reached stale movie or changed presentation owner");
        Require(eligibility_reads>0,"native CanSkipMovie was bypassed");
        bool retained_module=false,retained_movie=false;
        for(const auto& local:locals){retained_module|=local.second==module;retained_movie|=local.second==movie_a;}
        Require(retained_module&&retained_movie,"module/movie locals not retained across dispatch");
        if(!Step(true))return;
        // Native press may finish the movie and replace the active module. The
        // release must still reach the retained original, without more getters.
        press_completed=true;
        if(finish_on_press){current_movie=0;movie_playing=false;active_module=replacement_module;}
    }else{
        ++releases;release_step=steps+1;
        Require(presses==1&&press_completed,
            "release not paired with one completed press");
        Require(!finish_on_press||active_module==replacement_module,
            "release fixture did not replace active module");
        Step(true);
    }
}
bool ValidateModule(void* context,void* environment,void* retained) noexcept {
    Require(context==P(777)&&environment==&env&&Identity(retained)==module,
        "final active-module validator received wrong context/environment/owner");
    ++validations;
    if(!Step())return false;
    if(change_kind==4)ChangeOwner();
    return active_module==module;
}
template<class T>void Set(int index,T function){table[index]=reinterpret_cast<void*>(function);}
void Reset() {
    Require(locals.empty()&&!pending,"previous dispatch leaked JNI state");
    steps=presses=releases=press_step=release_step=field_reads=owner_rounds=eligibility_reads=0;
    fail_step=null_step=-1;current_movie=movie_a;active_module=module;
    change=change_at=change_kind=0;
    validations=0;
    frozen=mission_end=mission_ended=hint=precaching=pending=false;
    alive=movie_playing=skippable=finish_on_press=true;press_completed=return_ref_on_throw=false;
}
bool Dispatch(bool validate=false) {
    const bool delivered=DispatchCoJMovieSkip(&env,P(module),validate?ValidateModule:nullptr,P(777));
    Require(locals.empty()&&!pending,"dispatch leaked locals or pending Java exception");
    return delivered;
}
void Reject() {
    Require(!Dispatch(),"ineligible or failed skip reported delivered");
    Require(presses==0&&releases==0,"rejected owner received key input");
}
} // namespace

int main() {
    Set(15,Exception);Set(17,Clear);Set(23,Delete);Set(24,Same);Set(25,Retain);
    Set(31,Class);Set(33,Method);Set(39,Boolean);Set(63,Key);Set(94,Field);Set(95,ObjectField);
    Reset();
    Require(Dispatch()&&presses==1&&releases==1,"eligible movie did not receive one press/release pair");
    const int total=steps,press=press_step,release=release_step;
    const int rounds=owner_rounds,reads=field_reads;
    Require(rounds>=2&&reads>=2,"eligibility was not bracketed by fresh owner/movie checks");
    Require(fake_movie_ids[0]==fake_movie_ids[1],"replacement identity fixture broken");
    Reset();finish_on_press=false;
    Require(Dispatch()&&presses==1&&releases==1&&current_movie==movie_a&&movie_playing,
        "delivery incorrectly depended on the native movie accepting skip");
    // Fresh handles to the same object passed above; a movie replacement with
    // the same fake ID must reject at each post-snapshot field observation.
    for(int n=2;n<=reads;++n)for(int kind:{1,2}){
        Reset();change=kind;change_kind=1;change_at=n;Reject();
    }
    for(int n=1;n<=rounds;++n)for(int kind=1;kind<=6;++kind){
        Reset();change=kind;change_kind=2;change_at=n;Reject();
    }
    for(int kind=1;kind<=6;++kind){Reset();change=kind;change_kind=3;Reject();}
    Reset();frozen=true;Reject();
    Reset();alive=false;Reject();
    Reset();mission_end=true;Reject();
    Reset();movie_playing=false;Reject();
    Reset();current_movie=0;Reject();
    Reset();skippable=false;Reject();
    Reset();mission_ended=true;Reject();
    Reset();hint=true;Reject();
    Reset();precaching=true;Reject();
    // The original CanSkipMovie owns hint/precache/mission-ended policy; these
    // must not be replaced by a direct IsPlayingSkipableMovie-only shortcut.
    Reset();pending=true;Reject();Require(steps==0,"called JNI business API after entry exception");
    // Every throwable business operation: lookup/getter/identity/ref/key. Also
    // simulate a returned local accompanying an exception, to exercise cleanup.
    for(bool returned_ref:{false,true})for(int i=1;i<=total;++i){
        Reset();fail_step=i;return_ref_on_throw=returned_ref;
        Require(!Dispatch(),"JNI exception reported delivery success");
        Require(steps==i,"business JNI retried or continued after exception");
        Require(presses==(i>=press?1:0)&&releases==(i==release?1:0),
            "throwing press retried/released or unexpected fallback key sent");
    }
    // Null class/method/field/ref returns without an exception also fail closed.
    // Boolean false and IsSameObject false are valid reject observations.
    for(int i=1;i<press;++i){Reset();null_step=i;Require(!Dispatch(),"null/false JNI result accepted");
        Require(presses==0&&releases==0,"failed observation delivered input");}
    for(int index:{15,17,23,24,25,31,33,39,63,94,95}){
        Reset();auto saved=table[index];table[index]=nullptr;Reject();table[index]=saved;
    }
    Reset();Require(Dispatch(true)&&validations==1,"final active-module validation bypassed");
    const int validated_total=steps,validated_press=press_step,validated_release=release_step;
    // Replacement may happen during the read-only JNI probes in this adversarial
    // fixture. The bridge callback must reject the old active-module reference.
    for(int kind:{2,3,4}){
        Reset();change=7;change_kind=kind;change_at=2;
        Require(!Dispatch(true)&&presses==0&&releases==0,"stale active module received Escape");
    }
    Reset();active_module=replacement_module;
    Require(!Dispatch(true)&&presses==0,"already stale active module received Escape");
    for(int kind=1;kind<=2;++kind){
        Reset();change=kind;change_kind=4;
        Require(!Dispatch(true)&&presses==0,"validator-side movie change escaped final identity proof");
    }
    for(int i=1;i<=validated_total;++i){
        Reset();fail_step=i;return_ref_on_throw=true;
        Require(!Dispatch(true)&&steps==i,"validated dispatch continued/retried after JNI exception");
        Require(presses==(i>=validated_press?1:0)&&releases==(i==validated_release?1:0),
            "validated dispatch retried or released a throwing press");
    }
    Reset();
    Require(!DispatchCoJMovieSkip(nullptr,P(module)),"null environment accepted");
    Require(!DispatchCoJMovieSkip(&env,nullptr),"null module accepted");
    void** no_table=nullptr;
    Require(!DispatchCoJMovieSkip(&no_table,P(module)),"null JNI table accepted");
    Require(steps==0&&locals.empty()&&!pending,"invalid API arguments touched JNI state");
    std::cout<<"movie skip native eligibility, identity, delivery and JNI cleanup passed\n";
}
