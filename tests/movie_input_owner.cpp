#include "games/call_of_juarez/movie_input_owner.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>

#if defined(_WIN32)
#define TEST_JNICALL __stdcall
#else
#define TEST_JNICALL
#endif
namespace {
using cojvr::games::call_of_juarez::CoJMovieInputOwnerEpoch;
void Need(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
void* P(int value){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));}
// Borrowed references: 10/11 are module A aliases, 12 is module B;
// 20/21 are movie A aliases, 22 is movie B. Both movies have fake native ID 77.
void* slots[224]{};void** env=slots;
std::map<void*,int> globals,exceptions;
int next_ref=1000,steps=0,fail_at=-1,null_at=-1,peak=0,deletes=0;
bool pending=false,ref_on_fault=false;
int Identity(void* ref){
    if(auto it=globals.find(ref);it!=globals.end())return it->second;
    if(ref==P(10)||ref==P(11))return 1;
    if(ref==P(12))return 2;
    if(ref==P(20)||ref==P(21))return 3;
    if(ref==P(22))return 4;
    Need(false,"unknown, released or nonborrowed identity");return 0;
}
bool Step(){Need(!pending,"JNI business operation with pending exception");
    if(++steps==fail_at){pending=true;return false;}return steps!=null_at;}
void* Global(int object){auto ref=P(next_ref++);globals.emplace(ref,object);
    if(static_cast<int>(globals.size())>peak)peak=static_cast<int>(globals.size());
    Need(globals.size()<=4,"more than two retained plus two temporary globals");return ref;}
void* TEST_JNICALL NewGlobal(void*,void* borrowed){
    const int object=Identity(borrowed);const bool ok=Step();
    return ok||(pending&&ref_on_fault)?Global(object):nullptr;
}
// JNI DeleteGlobalRef is nonthrowing. Only allocation and Same inject faults;
// there is no reliable freed/not-freed proof for a nonconforming delete hook.
void TEST_JNICALL DeleteGlobal(void*,void* ref){Need(!pending,"global deleted with pending exception");
    Need(globals.erase(ref)==1,"borrowed or already released global deleted");++deletes;}
std::uint8_t TEST_JNICALL Same(void*,void* a,void* b){
    const int first=Identity(a),second=Identity(b);return Step()&&first==second;}
void* TEST_JNICALL Exception(void*){if(!pending)return nullptr;
    auto ref=P(next_ref++);exceptions.emplace(ref,99);return ref;}
void TEST_JNICALL ClearException(void*){Need(pending,"cleared absent exception");pending=false;}
void TEST_JNICALL DeleteLocal(void*,void* ref){Need(!pending,"exception ref deleted before clearing");
    Need(exceptions.erase(ref)==1,"deleted borrowed/global instead of exception local");}
template<class T>void Set(int index,T fn){slots[index]=reinterpret_cast<void*>(fn);}
void ResetFaults(){Need(!pending&&exceptions.empty(),"exception state leaked");
    steps=0;fail_at=null_at=-1;ref_on_fault=false;}
std::uint64_t Update(CoJMovieInputOwnerEpoch& owner,int module=10,int movie=20){
    const auto epoch=owner.Update(&env,P(module),P(movie));
    Need(!pending&&exceptions.empty(),"Update leaked pending exception/local");
    Need(globals.size()<=2,"Update retained temporary replacement globals");return epoch;
}
void Clear(CoJMovieInputOwnerEpoch& owner){Need(owner.Clear(&env),"valid Clear failed");
    Need(globals.empty()&&exceptions.empty()&&!pending,"Clear leaked globals/exception");}
void StableAndReplacement(){
    CoJMovieInputOwnerEpoch owner;
    ResetFaults();const auto a=Update(owner);Need(a!=0&&globals.size()==2,"initial owner not retained");
    ResetFaults();Need(Update(owner,11,21)==a&&deletes==0,"alias handles advanced epoch or replaced lease");
    const auto b=Update(owner,10,22);Need(b>a&&globals.size()==2,"same-ID movie replacement kept old epoch");
    const auto c=Update(owner,12,22);Need(c>b,"module replacement kept old epoch");
    const auto d=Update(owner);Need(d>c,"returning to old Java objects reused their epoch");
    Clear(owner);Need(owner.Clear(nullptr),"empty Clear needs no environment");
    const auto e=Update(owner);Need(e>d,"Clear reset serial or reused identity");Clear(owner);
    Need(peak==4,"replacement fixture did not exercise temporary globals");
}
void FaultsAndRetry(){
    // Empty owner: first or second NewGlobalRef can fail/null, optionally
    // returning a usable global together with the exception. Roll back both.
    for(int failure=1;failure<=2;++failure)for(bool returned:{false,true})for(bool java_fault:{false,true}){
        CoJMovieInputOwnerEpoch owner;ResetFaults();
        if(java_fault)fail_at=failure;else null_at=failure;ref_on_fault=returned;
        Need(Update(owner)==0&&globals.empty(),"partial initial allocation leaked/published owner");
        Need(steps==failure,"allocation failure retried or continued JNI work");
        ResetFaults();Need(Update(owner)!=0,"initial allocation failure prevented retry");Clear(owner);
    }
    // Full replacement: Same(module), Same(movie), NewGlobal(module),
    // NewGlobal(movie). Every failure preserves the exact old global lease.
    for(int failure=1;failure<=4;++failure)for(bool returned:{false,true}){
        CoJMovieInputOwnerEpoch owner;ResetFaults();const auto a=Update(owner);
        const auto old=globals;ResetFaults();fail_at=failure;ref_on_fault=returned;
        Need(Update(owner,12,22)==0&&globals==old,"fault discarded old lease or retained partial replacement");
        Need(steps==failure,"Same/allocation fault continued JNI work");
        ResetFaults();Need(Update(owner)>a,"fault did not invalidate published epoch on old-lease recovery");Clear(owner);
    }
    for(int failure:{3,4}){
        CoJMovieInputOwnerEpoch owner;ResetFaults();const auto a=Update(owner);const auto old=globals;
        ResetFaults();null_at=failure;
        Need(Update(owner,12,22)==0&&globals==old,"null replacement allocation lost old lease");
        ResetFaults();Need(Update(owner,12,22)>a,"replacement retry failed");Clear(owner);
    }
    CoJMovieInputOwnerEpoch owner;ResetFaults();const auto a=Update(owner);const auto old=globals;
    ResetFaults();pending=true;
    Need(Update(owner)==0&&steps==0&&globals==old,"entry exception used JNI or lost lease");
    ResetFaults();Need(Update(owner)>a,"entry exception left stale published epoch");
    pending=true;Need(!owner.Clear(&env)&&!pending&&exceptions.empty()&&globals==old,
        "Clear entry exception did not preserve retryable lease");
    Clear(owner);
}
void MissingAndNull(){
    for(int slot:{15,17,21,22,23,24}){
        CoJMovieInputOwnerEpoch owner;ResetFaults();const auto a=Update(owner);const auto old=globals;
        auto saved=slots[slot];slots[slot]=nullptr;ResetFaults();
        Need(Update(owner)==0&&steps==0&&globals==old,"missing API used JNI or lost lease");
        Need(!owner.Clear(&env)&&globals==old,"Clear claimed success with incomplete API and live lease");
        slots[slot]=saved;ResetFaults();Need(Update(owner)>a,"API recovery reused stale epoch");Clear(owner);
    }
    CoJMovieInputOwnerEpoch owner;ResetFaults();const auto a=Update(owner);const auto old=globals;
    Need(owner.Update(nullptr,P(10),P(20))==0&&globals==old,"null env lost retryable lease");
    Need(!owner.Clear(nullptr)&&globals==old,"Clear lost lease without env");
    void** no_table=nullptr;
    Need(owner.Update(&no_table,P(10),P(20))==0,"null table published an epoch");
    Need(!owner.Clear(&no_table)&&globals==old,"Clear accepted null table with retained lease");
    Need(Update(owner)>a,"environment recovery reused invalidated epoch");
    Need(owner.Update(&env,nullptr,P(20))==0&&globals.empty(),"null module retained stale identity");
    const auto b=Update(owner);Need(b>a,"null module reset serial");
    Need(owner.Update(&env,P(10),nullptr)==0&&globals.empty(),"null movie retained stale identity");
    Need(Update(owner)>b,"null movie reset serial");Clear(owner);
    Need(!pending&&exceptions.empty(),"null/failed arguments leaked exception state");
}
} // namespace
int main(){
    Set(15,Exception);Set(17,ClearException);Set(21,NewGlobal);Set(22,DeleteGlobal);
    Set(23,DeleteLocal);Set(24,Same);
    StableAndReplacement();FaultsAndRetry();MissingAndNull();
    Need(globals.empty()&&exceptions.empty()&&!pending,"final JNI resources leaked");
    std::cout<<"Movie input owner identity epochs and transactional JNI leases passed\n";
}
#undef TEST_JNICALL
