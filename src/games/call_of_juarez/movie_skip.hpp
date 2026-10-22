#pragma once
#include "games/call_of_juarez/presentation_owner.hpp"
#include <cstdint>

namespace cojvr::games::call_of_juarez {
// Optional read-only final active-module proof, owned by the bridge. Compare
// the retained module with the current static module using IsSameObject, never
// numeric IDs. The callback owns cleanup of any locals it creates; a pending
// Java exception on return is cleared here and rejects delivery.
using CoJMovieSkipValidateModule=bool(*)(void* context,void* env,void* retained_module) noexcept;

namespace movie_skip_detail {
#if defined(_WIN32)
#define COJ_MOVIE_SKIP_JNICALL __stdcall
#else
#define COJ_MOVIE_SKIP_JNICALL
#endif
union Value {std::int64_t alignment;std::int32_t i;std::uint8_t z;std::uint16_t c;void* l;};
static_assert(sizeof(Value)==8);
using Object=void*(COJ_MOVIE_SKIP_JNICALL*)(void*,void*);
using Lookup=void*(COJ_MOVIE_SKIP_JNICALL*)(void*,void*,const char*,const char*);
using FieldObject=void*(COJ_MOVIE_SKIP_JNICALL*)(void*,void*,void*);
using Same=std::uint8_t(COJ_MOVIE_SKIP_JNICALL*)(void*,void*,void*);
using Boolean=std::uint8_t(COJ_MOVIE_SKIP_JNICALL*)(void*,void*,void*,const void*);
using Key=void(COJ_MOVIE_SKIP_JNICALL*)(void*,void*,void*,const Value*);
using Exception=void*(COJ_MOVIE_SKIP_JNICALL*)(void*);
using Clear=void(COJ_MOVIE_SKIP_JNICALL*)(void*);
using Delete=void(COJ_MOVIE_SKIP_JNICALL*)(void*,void*);
#undef COJ_MOVIE_SKIP_JNICALL
struct Reader {
    void* env;void** table;bool ok=true;
    template<class T>T Fn(int index) const noexcept {return reinterpret_cast<T>(table[index]);}
    bool Check() noexcept {
        void* exception=Fn<Exception>(15)(env);
        if(exception){Fn<Clear>(17)(env);Fn<Delete>(23)(env,exception);ok=false;}
        return ok;
    }
    struct Local {
        Reader& reader;void* value;
        Local(Reader& r,void* v) noexcept:reader(r),value(v){}
        Local(const Local&)=delete;
        ~Local(){if(value)reader.Fn<Delete>(23)(reader.env,value);}
        operator void*() const noexcept {return value;}
    };
    Local ObjectRef(int index,void* object) noexcept {
        void* value=Fn<Object>(index)(env,object);
        Check();if(!value)ok=false;return {*this,value};
    }
    void* LookupId(int index,void* cls,const char* name,const char* signature) noexcept {
        void* value=Fn<Lookup>(index)(env,cls,name,signature);
        Check();if(!value)ok=false;return value;
    }
    Local Movie(void* module,void* field) noexcept {
        void* value=Fn<FieldObject>(95)(env,module,field);
        Check();if(!value)ok=false;return {*this,value};
    }
    bool SameMovie(void* module,void* field,void* movie) noexcept {
        auto current=Movie(module,field);if(!ok)return false;
        const bool same=Fn<Same>(24)(env,current,movie)!=0;
        return Check()&&same;
    }
};
} // namespace movie_skip_detail

// Caller has exact-build-admitted the active LawmanModuleSingle and calls on
// the game thread with a qualified module reference. Without validate, caller
// must establish current active-module identity immediately before this call;
// the native getters are read-only on that same thread. Nothing is cached.
// True means one Escape press and release completed, not that a movie skipped.
// False never permits a retry/fallback key for this request: a throwing native
// press may already have taken effect. The caller owns input-edge suppression.
inline bool DispatchCoJMovieSkip(void* env,void* module,
    CoJMovieSkipValidateModule validate=nullptr,void* context=nullptr) noexcept {
    using namespace movie_skip_detail;
    if(!env||!module)return false;
    auto table=*static_cast<void***>(env);
    if(!table||!table[15]||!table[17]||!table[23])return false;
    Reader r{env,table};if(!r.Check())return false;
    // Includes every entry used by ReadCoJPresentationOwner. No body reader,
    // global references, direct MovieBreak/skip callback, or mutation fallback.
    constexpr int required[]{24,25,31,33,39,63,94,95};
    for(int index:required)if(!table[index])return false;
    auto retained=r.ObjectRef(25,module);if(!r.ok)return false;
    void* movie_field=nullptr;void* can_skip=nullptr;void* input=nullptr;
    {
        auto cls=r.ObjectRef(31,retained);if(!r.ok)return false;
        movie_field=r.LookupId(94,cls,"m_cMoviePlaying","LMovie;");if(!r.ok)return false;
        can_skip=r.LookupId(33,cls,"CanSkipMovie","()Z");if(!r.ok)return false;
        input=r.LookupId(33,cls,"OnInputKey","(IZC)V");if(!r.ok)return false;
    }
    auto movie=r.Movie(retained,movie_field);if(!r.ok)return false;
    CoJPresentationOwner owner{};
    if(!ReadCoJPresentationOwner(env,retained,owner)||!owner.immersive_movie_allowed())return false;
    const bool eligible=r.Fn<Boolean>(39)(env,retained,can_skip,nullptr)!=0;
    if(!r.Check()||!eligible)return false;
    // CanSkipMovie includes native mission-ended, hint and precache policy.
    // Recheck camera ownership too; a movie boolean alone can include black belts.
    if(!ReadCoJPresentationOwner(env,retained,owner)||!owner.immersive_movie_allowed())return false;
    if(validate){
        const bool current=validate(context,env,retained);
        if(!r.Check()||!current)return false;
    }
    if(!r.SameMovie(retained,movie_field,movie))return false;
    Value args[3]{};args[0].i=1;args[1].z=1;args[2].c=0;
    r.Fn<Key>(63)(env,retained,input,args);
    if(!r.Check())return false; // No retry or release after a throwing press.
    // Original OnInputKey returns at PC0..4 on keyup. A completed press may
    // end/replace the movie/module; release exactly once to this retained owner.
    args[1].z=0;
    r.Fn<Key>(63)(env,retained,input,args);
    return r.Check();
}
} // namespace cojvr::games::call_of_juarez
