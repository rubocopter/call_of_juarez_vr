#pragma once
#include "runtime/vr_settings.hpp"
#include <cstdint>
#include <string>

namespace cojvr::games::call_of_juarez {
// JNI 1.4 A-call argument layout. No native object crosses the game thread.
union NativeSettingsJValue {
    std::uint8_t z; std::int32_t i; std::int64_t j; double d; void* l;
};
static_assert(sizeof(NativeSettingsJValue)==8);

// The caller supplies only the current, passively resolved UI (including modal
// ownership). Exact-build admission belongs to the owning game adapter.
class NativeSettingsBridge final {
public:
    void Invalidate() noexcept { known_=false; }
    [[nodiscard]] bool Update(void* env,void* ui,runtime::SettingsLanguage language,
        runtime::VrPreferenceStore& store,std::string* error=nullptr) noexcept {
        struct Locals {
            void* env;void* values[4]{};void(__stdcall* drop)(void*,void*)=nullptr;
            ~Locals(){if(drop)for(auto* value:values)if(value)drop(env,value);}
        } locals{env};
        const auto fail=[&](const char* why) noexcept {
            known_=false;if(error)try{*error=why;}catch(...){}return false;
        };
        if(error)error->clear();
        if(!env||!ui){Invalidate();return true;}
        try {
            auto** table=*static_cast<void***>(env);
            if(!table)return fail("Native settings JNI table unavailable");
            using Find=void*(__stdcall*)(void*,const char*);
            using Obj=void*(__stdcall*)(void*,void*);
            using Same=std::uint8_t(__stdcall*)(void*,void*,void*);
            using Ex=void*(__stdcall*)(void*);
            using Clear=void(__stdcall*)(void*);
            using Field=void*(__stdcall*)(void*,void*,const char*,const char*);
            using Bool=std::uint8_t(__stdcall*)(void*,void*,void*,const NativeSettingsJValue*);
            using ReadBool=std::uint8_t(__stdcall*)(void*,void*,void*);
            using Int=std::int32_t(__stdcall*)(void*,void*,void*,const NativeSettingsJValue*);
            using ReadInt=std::int32_t(__stdcall*)(void*,void*,void*);
            using WriteInt=void(__stdcall*)(void*,void*,void*,std::int32_t);
            using WriteBool=void(__stdcall*)(void*,void*,void*,std::uint8_t);
            using Call=void(__stdcall*)(void*,void*,void*,const NativeSettingsJValue*);
            auto find=reinterpret_cast<Find>(table[6]);auto exception=reinterpret_cast<Ex>(table[15]);
            auto clear=reinterpret_cast<Clear>(table[17]);locals.drop=reinterpret_cast<decltype(locals.drop)>(table[23]);
            auto same=reinterpret_cast<Same>(table[24]);auto klass=reinterpret_cast<Obj>(table[31]);
            auto method=reinterpret_cast<Field>(table[33]);auto boolean=reinterpret_cast<Bool>(table[39]);
            auto integer=reinterpret_cast<Int>(table[51]);auto call=reinterpret_cast<Call>(table[63]);
            auto field=reinterpret_cast<Field>(table[94]);auto read_bool=reinterpret_cast<ReadBool>(table[96]);
            auto read_int=reinterpret_cast<ReadInt>(table[100]);auto write_int=reinterpret_cast<WriteInt>(table[109]);
            auto write_bool=reinterpret_cast<WriteBool>(table[105]);
            auto string=reinterpret_cast<Find>(table[167]);
            if(!find||!exception||!clear||!locals.drop||!same||!klass||!method||!boolean||!integer||
                !call||!field||!read_bool||!read_int||!write_int||!write_bool||!string)
                return fail("Native settings JNI access unavailable");
            const auto clean=[&]() noexcept {
                void* thrown=exception(env);if(!thrown)return true;
                clear(env);locals.drop(env,thrown);return false;
            };
            if(!clean())return fail("Native settings entered with an exception");
            locals.values[0]=klass(env,ui);if(!clean()||!locals.values[0])return fail("Native settings UI class unavailable");
            locals.values[1]=find(env,"MenuOptionsGame");if(!clean()||!locals.values[1])return fail("Native options class unavailable");
            const bool exact=same(env,locals.values[0],locals.values[1])!=0;
            if(!clean())return fail("Native options class identity failed");
            if(!exact){Invalidate();return true;}
            void* type=locals.values[0];
            auto visible=method(env,type,"IsActuallyVisible","()Z");
            if(!clean()||!visible)return fail("Native options visibility unavailable");
            const bool shown=boolean(env,ui,visible,nullptr)!=0;
            if(!clean())return fail("Native options visibility failed");
            if(!shown){Invalidate();return true;}
            auto identity=method(env,type,"GetThisID","()I");
            if(!clean()||!identity)return fail("Native options identity unavailable");
            const auto id=static_cast<std::uint32_t>(integer(env,ui,identity,nullptr));
            if(!clean()||!id||id==0xffffffffU)return fail("Native options identity invalid");
            auto ready=field(env,type,"cojvrSettingsReady","Z");
            if(!clean()||!ready)return fail("Native options patch unavailable");
            auto serial=field(env,type,"cojvrSettingsOpenSerial","I");
            if(!clean()||!serial)return fail("Native options open epoch unavailable");
            auto request=field(env,type,"cojvrSettingsRequest","I");
            if(!clean()||!request)return fail("Native options request unavailable");
            const auto epoch=static_cast<std::uint32_t>(read_int(env,ui,serial));
            if(!clean())return fail("Native options open epoch failed");
            const bool initialized=read_bool(env,ui,ready)!=0;
            if(!clean())return fail("Native options readiness failed");
            const auto pending=read_int(env,ui,request);
            if(!clean())return fail("Native options request failed");
            auto sync=method(env,type,"cojvrSyncSettings","(III)V");
            if(!clean()||!sync)return fail("Native options synchronizer unavailable");
            auto complete=method(env,type,"cojvrCompleteSettings","(ZLjava/lang/String;)V");
            if(!clean()||!complete)return fail("Native options completion unavailable");
            const auto synchronize=[&](const runtime::VrPreferenceSnapshot& snapshot) {
                write_bool(env,ui,ready,0);
                if(!clean())return false;
                NativeSettingsJValue args[3]{};
                args[0].i=static_cast<std::int32_t>(snapshot.values.cinematics);
                args[1].i=static_cast<std::int32_t>(snapshot.values.turning);
                args[2].i=language==runtime::SettingsLanguage::spanish?1:0;
                call(env,ui,sync,args);
                if(!clean())return false;
                owner_=id;epoch_=epoch;revision_=snapshot.revision;known_=true;return true;
            };
            // Consume before every possible side effect. An interrupted owner
            // cannot replay an old pending OK after focus/menu reacquisition.
            if(pending){
                const bool admitted=pending==1&&known_&&owner_==id&&epoch_==epoch&&initialized;
                write_int(env,ui,request,admitted?2:0);
                if(!clean())return fail("Native options request consumption failed");
            }
            if(!known_||owner_!=id||epoch_!=epoch||!initialized){
                return synchronize(store.Read())||fail("Native options synchronization failed");
            }
            if(!pending)return true;
            const bool es=language==runtime::SettingsLanguage::spanish;
            const auto finish=[&](bool accepted,const char* message) {
                locals.values[2]=string(env,message);
                if(!clean()||!locals.values[2])return false;
                NativeSettingsJValue args[2]{};args[0].z=accepted?1:0;args[1].l=locals.values[2];
                call(env,ui,complete,args);return clean();
            };
            auto cinema=method(env,type,"cojvrCinematicsValue","()I");
            if(!clean()||!cinema)return fail("Native cinematic selection unavailable");
            auto turning=method(env,type,"cojvrTurningValue","()I");
            if(!clean()||!turning)return fail("Native turning selection unavailable");
            const auto cinematic_value=integer(env,ui,cinema,nullptr);
            if(!clean())return fail("Native cinematic selection failed");
            const auto turning_value=integer(env,ui,turning,nullptr);
            if(!clean())return fail("Native turning selection failed");
            if(pending!=1||cinematic_value<0||cinematic_value>1||turning_value<0||turning_value>1){
                return finish(false,es?"Seleccion VR no valida":"Invalid VR selection")||fail("Native options rejection failed");
            }
            const auto applied=store.Apply({static_cast<runtime::CinematicsMode>(cinematic_value),
                static_cast<runtime::TurnMode>(turning_value)},revision_);
            revision_=applied.snapshot.revision;
            if(applied.conflict){
                if(!synchronize(applied.snapshot))return fail("Native options conflict synchronization failed");
                return finish(false,es?"Ajustes actualizados; revisa la seleccion":"Settings changed; review the selection")||
                    fail("Native options conflict feedback failed");
            }
            if(!applied.accepted)return finish(false,es?"Seleccion VR no valida":"Invalid VR selection")||
                fail("Native options rejection failed");
            if(!applied.saved)return finish(false,es?"No se pudo guardar; solo esta sesion. Aceptar reintenta.":
                "Save failed; this session only. Accept to retry.")||fail("Native options save feedback failed");
            if(!finish(true,""))return fail("Native options completion failed");
            Invalidate();return true;
        }catch(...){return fail("Native options bridge exception");}
    }
private:
    bool known_=false;
    std::uint32_t owner_=0,epoch_=0;
    std::uint64_t revision_=0;
};
}
