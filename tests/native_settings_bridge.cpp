#include <windows.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "games/call_of_juarez/native_settings_bridge.hpp"
using namespace cojvr::runtime;
using namespace cojvr::games::call_of_juarez;
namespace {
void Check(bool value,const char* message) {
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}
struct Page { bool exact=true,visible=true,ready=false;std::int32_t id=0x81234567,
    serial=1,request=0,cinematics=0,turning=0;int syncs=0,completions=0;std::string status; } page;
struct Object { int references=0; } klass,other_class,exception,text;
bool pending=false,missing_marker=false,fail_complete=false;
int writes=0,callbacks=0;
enum Id { visible=1,identity,ready,serial,request,sync,cinematics,turning,complete };
void* Token(int id){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id));}
int Number(void* id){return static_cast<int>(reinterpret_cast<std::uintptr_t>(id));}
void* __stdcall Find(void*,const char* name){Check(std::string(name)=="MenuOptionsGame","unexpected class lookup");++klass.references;return &klass;}
void* __stdcall Class(void*,void* p){Check(p==&page,"foreign page");auto& k=page.exact?klass:other_class;++k.references;return &k;}
std::uint8_t __stdcall Same(void*,void* a,void* b){return a==b;}
void* __stdcall Exception(void*){if(!pending)return nullptr;++exception.references;return &exception;}
void __stdcall Clear(void*){pending=false;}
void __stdcall Delete(void*,void* p){if(p)--static_cast<Object*>(p)->references;}
void* __stdcall Field(void*,void*,const char* name,const char*) {
    if(missing_marker){pending=true;return nullptr;}
    const std::string n(name);
    if(n=="cojvrSettingsReady")return Token(ready);
    if(n=="cojvrSettingsOpenSerial")return Token(serial);
    if(n=="cojvrSettingsRequest")return Token(request);
    Check(false,"unexpected field");return nullptr;
}
void* __stdcall Method(void*,void*,const char* name,const char*) {
    const std::string n(name);
    if(n=="IsActuallyVisible")return Token(visible);
    if(n=="GetThisID")return Token(identity);
    if(n=="cojvrSyncSettings")return Token(sync);
    if(n=="cojvrCinematicsValue")return Token(cinematics);
    if(n=="cojvrTurningValue")return Token(turning);
    if(n=="cojvrCompleteSettings")return Token(complete);
    Check(false,"unexpected method");return nullptr;
}
std::uint8_t __stdcall Bool(void*,void*,void*,const void*) {return page.visible;}
std::uint8_t __stdcall ReadBool(void*,void*,void*){return page.ready;}
std::int32_t __stdcall Int(void*,void*,void* method,const void*) {
    if(Number(method)==identity)return page.id;
    if(Number(method)==cinematics)return page.cinematics;
    Check(Number(method)==turning,"unexpected int method");return page.turning;
}
std::int32_t __stdcall ReadInt(void*,void*,void* field){return Number(field)==serial?page.serial:page.request;}
void __stdcall WriteInt(void*,void*,void* field,std::int32_t value){
    Check(Number(field)==request,"unexpected integer write");page.request=value;++writes;
}
void __stdcall WriteBool(void*,void*,void* field,std::uint8_t value){
    Check(Number(field)==ready,"unexpected boolean write");page.ready=value!=0;
}
void* __stdcall String(void*,const char* value){page.status=value;++text.references;return &text;}
void __stdcall Void(void*,void*,void* method,const NativeSettingsJValue* values){
    ++callbacks;
    if(Number(method)==sync){
        page.cinematics=values[0].i;page.turning=values[1].i;
        page.ready=true;page.request=0;page.status.clear();++page.syncs;return;
    }
    Check(Number(method)==complete,"unexpected callback");
    if(values[0].z)Check(page.request==2,"request was not consumed before accepted callback");
    page.request=0;
    if(fail_complete){pending=true;return;}
    if(values[0].z){++page.completions;page.visible=false;page.ready=false;}
}
void Clean(){Check(!pending&&!klass.references&&!other_class.references&&!exception.references&&!text.references,"JNI reference/exception leak");}
}
int main(){
    void* functions[224]{};void** table=functions;void* env=&table;
    functions[6]=reinterpret_cast<void*>(&Find);functions[15]=reinterpret_cast<void*>(&Exception);
    functions[17]=reinterpret_cast<void*>(&Clear);functions[23]=reinterpret_cast<void*>(&Delete);
    functions[24]=reinterpret_cast<void*>(&Same);functions[31]=reinterpret_cast<void*>(&Class);
    functions[33]=reinterpret_cast<void*>(&Method);functions[39]=reinterpret_cast<void*>(&Bool);
    functions[51]=reinterpret_cast<void*>(&Int);functions[63]=reinterpret_cast<void*>(&Void);
    functions[94]=reinterpret_cast<void*>(&Field);functions[96]=reinterpret_cast<void*>(&ReadBool);
    functions[100]=reinterpret_cast<void*>(&ReadInt);functions[109]=reinterpret_cast<void*>(&WriteInt);
    functions[105]=reinterpret_cast<void*>(&WriteBool);
    functions[167]=reinterpret_cast<void*>(&String);
    const auto root=std::filesystem::temp_directory_path()/
        (L"cojvr-native-settings-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(root);const auto path=root/L"settings.ini";
    VrPreferenceStore store;(void)store.Initialize(path);NativeSettingsBridge bridge;std::string error;
    page.exact=false;Check(bridge.Update(env,&page,SettingsLanguage::english,store,&error),"ordinary native UI was blocked");
    Check(page.syncs==0&&writes==0,"foreign UI acquired settings");Clean();page.exact=true;
    page.visible=false;Check(bridge.Update(env,&page,SettingsLanguage::english,store,&error),"hidden page failed");
    Check(page.syncs==0,"hidden page acquired settings");Clean();page.visible=true;
    Check(bridge.Update(env,&page,SettingsLanguage::spanish,store,&error)&&page.ready&&page.syncs==1,"fresh visible page not synchronized");Clean();
    page.cinematics=1;page.turning=1;
    Check(bridge.Update(env,&page,SettingsLanguage::spanish,store,&error)&&page.cinematics==1&&page.syncs==1,"unsaved native edits overwritten");
    Check(store.Read().values.cinematics==CinematicsMode::theater,"highlight applied before OK");
    page.request=1;
    Check(bridge.Update(env,&page,SettingsLanguage::spanish,store,&error)&&page.completions==1,"accepted native OK did not complete");Clean();
    auto snapshot=store.Read();
    Check(snapshot.values.cinematics==CinematicsMode::immersive&&snapshot.values.turning==TurnMode::snap,"native choices not published together");
    Check(LoadVrSettings(path).turning==TurnMode::snap,"native choices not persisted");
    Check(bridge.Update(env,&page,SettingsLanguage::spanish,store,&error)&&page.completions==1,"completed request replayed");
    page.visible=true;page.ready=false;++page.serial;
    Check(bridge.Update(env,&page,SettingsLanguage::english,store,&error)&&page.syncs==2,"reopened page did not rebase");
    auto other=store.ApplyCinematics(CinematicsMode::theater,snapshot.revision);
    Check(other.accepted,"other editor setup failed");page.request=1;
    Check(bridge.Update(env,&page,SettingsLanguage::english,store,&error)&&page.completions==1&&!page.status.empty(),"stale editor overwrote newer preferences or silently closed");
    Check(store.Read().values.cinematics==CinematicsMode::theater&&page.cinematics==0,"conflict did not rebase to authoritative mode");Clean();
    page.cinematics=99;page.request=1;
    Check(bridge.Update(env,&page,SettingsLanguage::english,store,&error)&&page.request==0&&page.completions==1,"invalid native option committed/retried");Clean();
    page.cinematics=1;page.turning=0;page.request=1;
    HANDLE locked=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    Check(locked!=INVALID_HANDLE_VALUE,"save denial setup failed");
    Check(bridge.Update(env,&page,SettingsLanguage::english,store,&error)&&page.visible&&page.completions==1&&!page.status.empty(),"failed persistence hid error or left page");
    Check(store.Read().save_failed&&store.Read().values.cinematics==CinematicsMode::immersive,"save failure lost session value");
    Check(LoadVrSettings(path).mode==CinematicsMode::theater,"failed native save destroyed previous file");CloseHandle(locked);Clean();
    page.request=1;Check(bridge.Update(env,&page,SettingsLanguage::english,store,&error)&&page.completions==2,"fresh explicit save retry failed");Clean();
    page.visible=true;page.ready=false;++page.serial;Check(bridge.Update(env,&page,SettingsLanguage::english,store,&error),"partial callback setup failed");
    page.cinematics=0;page.request=1;fail_complete=true;
    Check(!bridge.Update(env,&page,SettingsLanguage::english,store,&error)&&page.request==0,"callback fault retained replayable request");Clean();fail_complete=false;
    const int completions=page.completions;
    Check(bridge.Update(env,&page,SettingsLanguage::english,store,&error)&&page.completions==completions,"partial completion automatically retried original OK");Clean();
    bridge.Invalidate();page.request=1;
    Check(bridge.Update(env,&page,SettingsLanguage::english,store,&error)&&page.request==0&&page.completions==completions,"owner recovery replayed pending request");Clean();
    missing_marker=true;bridge.Invalidate();
    Check(!bridge.Update(env,&page,SettingsLanguage::english,store,&error),"unpatched class admitted mutation");Clean();missing_marker=false;
    std::filesystem::remove_all(root);
    std::cout<<"native_settings_bridge: ownership, synchronization, commit, save-failure and cancellation passed\n";
}
