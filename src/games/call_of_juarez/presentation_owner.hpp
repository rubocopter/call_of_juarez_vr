#pragma once
#include <cstddef>
#include <cstdint>

namespace cojvr::games::call_of_juarez {
struct CoJPresentationOwner {
    bool valid=false;
    bool timer_frozen=false;
    bool player_alive=false;
    bool mission_end_visible=false;
    [[nodiscard]] bool blocked() const noexcept {
        return !valid || timer_frozen || !player_alive || mission_end_visible;
    }
};

// Caller supplies the verified active LawmanModuleSingle. These exact script
// getters are read-only. MenuMissionEnd (38) does not require TimerStop, so the
// timer alone cannot authorize tracked camera/body/gameplay ownership.
inline bool ReadCoJPresentationOwner(void* env,void* module,
    CoJPresentationOwner& out) noexcept {
    out={};
    if(!env || !module)return false;
#if defined(_WIN32)
#define COJVR_PRESENTATION_JNICALL __stdcall
#else
#define COJVR_PRESENTATION_JNICALL
#endif
    using ObjectFn=void*(COJVR_PRESENTATION_JNICALL*)(void*,void*);
    using MethodFn=void*(COJVR_PRESENTATION_JNICALL*)(void*,void*,const char*,const char*);
    using BooleanFn=std::uint8_t(COJVR_PRESENTATION_JNICALL*)(void*,void*,void*,const void*);
    using ExceptionFn=void*(COJVR_PRESENTATION_JNICALL*)(void*);
    using ClearFn=void(COJVR_PRESENTATION_JNICALL*)(void*);
    using DeleteFn=void(COJVR_PRESENTATION_JNICALL*)(void*,void*);
#undef COJVR_PRESENTATION_JNICALL
    const auto table=*static_cast<void***>(env);
    if(!table)return false;
    const auto cls_fn=reinterpret_cast<ObjectFn>(table[31]);
    const auto method_fn=reinterpret_cast<MethodFn>(table[33]);
    const auto boolean_fn=reinterpret_cast<BooleanFn>(table[39]);
    const auto exception_fn=reinterpret_cast<ExceptionFn>(table[15]);
    const auto clear_fn=reinterpret_cast<ClearFn>(table[17]);
    const auto delete_fn=reinterpret_cast<DeleteFn>(table[23]);
    if(!cls_fn || !method_fn || !boolean_fn || !exception_fn || !clear_fn || !delete_fn)return false;
    const auto failed=[&]() noexcept {
        void* exception=exception_fn(env);
        if(!exception)return false;
        clear_fn(env);delete_fn(env,exception);return true;
    };
    if(failed())return false;
    void* cls=cls_fn(env,module);
    if(failed() || !cls){if(cls)delete_fn(env,cls);return false;}
    CoJPresentationOwner observed{};
    const auto read=[&](const char* name,bool& value) noexcept {
        void* method=method_fn(env,cls,name,"()Z");
        if(failed() || !method)return false;
        const auto result=boolean_fn(env,module,method,nullptr);
        if(failed())return false;
        value=result!=0;return true;
    };
    const bool ok=read("IsTimerFreezed",observed.timer_frozen) &&
        read("IsMainPlayerAlive",observed.player_alive) &&
        read("IsMenuMissionEndVisible",observed.mission_end_visible);
    delete_fn(env,cls);
    if(!ok)return false;
    observed.valid=true;out=observed;return true;
}
}
