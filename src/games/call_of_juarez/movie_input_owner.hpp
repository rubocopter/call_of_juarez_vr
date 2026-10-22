#pragma once
#include <cstdint>

namespace cojvr::games::call_of_juarez {
namespace movie_input_owner_detail {
#if defined(_WIN32)
#define COJ_MOVIE_OWNER_JNICALL __stdcall
#else
#define COJ_MOVIE_OWNER_JNICALL
#endif
using Exception=void*(COJ_MOVIE_OWNER_JNICALL*)(void*);
using ClearException=void(COJ_MOVIE_OWNER_JNICALL*)(void*);
using NewGlobal=void*(COJ_MOVIE_OWNER_JNICALL*)(void*,void*);
using Delete=void(COJ_MOVIE_OWNER_JNICALL*)(void*,void*);
using Same=std::uint8_t(COJ_MOVIE_OWNER_JNICALL*)(void*,void*,void*);
#undef COJ_MOVIE_OWNER_JNICALL
struct Api {
    void* env=nullptr;void** table=nullptr;
    bool Open(void* environment) noexcept {
        if(!environment)return false;
        auto functions=*static_cast<void***>(environment);if(!functions)return false;
        constexpr int required[]{15,17,21,22,23,24};
        for(int slot:required)if(!functions[slot])return false;
        env=environment;table=functions;return true;
    }
    template<class T>T Fn(int slot) const noexcept {return reinterpret_cast<T>(table[slot]);}
    bool Check() const noexcept {
        void* exception=Fn<Exception>(15)(env);
        if(!exception)return true;
        Fn<ClearException>(17)(env);
        Fn<Delete>(23)(env,exception);return false;
    }
    void Release(void* global) const noexcept {
        // JNI DeleteGlobalRef does not throw. Do not infer a freed/not-freed
        // result from a nonconforming throwing replacement of this void API.
        if(global)Fn<Delete>(22)(env,global);
    }
};
} // namespace movie_input_owner_detail

// Exact-game, game-thread-only identity lease. Caller supplies borrowed module
// and current movie locals after exact-build admission. No script/native game
// methods, numeric IDs or raw local-handle cache participate in identity.
// Call Clear successfully before destroying this object or detaching the VM;
// destruction deliberately performs no environment-dependent cleanup.
class CoJMovieInputOwnerEpoch final {
public:
    CoJMovieInputOwnerEpoch() noexcept=default;
    CoJMovieInputOwnerEpoch(const CoJMovieInputOwnerEpoch&)=delete;
    CoJMovieInputOwnerEpoch& operator=(const CoJMovieInputOwnerEpoch&)=delete;
    CoJMovieInputOwnerEpoch(CoJMovieInputOwnerEpoch&&)=delete;
    CoJMovieInputOwnerEpoch& operator=(CoJMovieInputOwnerEpoch&&)=delete;

    // Zero is always unavailable. A failure invalidates the published epoch;
    // allocation/API faults retain the old global lease for recovery/Clear.
    // Recovery publishes a fresh epoch even when those old objects still match.
    // Null module/movie instead requests Clear when cleanup APIs are available.
    [[nodiscard]] std::uint64_t Update(void* env,void* module,void* movie) noexcept {
        movie_input_owner_detail::Api api;
        if(!api.Open(env)||!api.Check())return Invalidate();
        if(!module||!movie){(void)Clear(env);return 0;}
        if(module_&&movie_){
            const bool same_module=api.Fn<movie_input_owner_detail::Same>(24)(env,module_,module)!=0;
            if(!api.Check())return Invalidate();
            const bool same_movie=api.Fn<movie_input_owner_detail::Same>(24)(env,movie_,movie)!=0;
            if(!api.Check())return Invalidate();
            if(same_module&&same_movie){if(!epoch_)epoch_=Advance();return epoch_;}
        }
        void* next_module=api.Fn<movie_input_owner_detail::NewGlobal>(21)(env,module);
        if(!api.Check()||!next_module){api.Release(next_module);return Invalidate();}
        void* next_movie=api.Fn<movie_input_owner_detail::NewGlobal>(21)(env,movie);
        if(!api.Check()||!next_movie){
            api.Release(next_movie);api.Release(next_module);return Invalidate();
        }
        // Acquire the whole replacement before releasing the old pair. At most
        // two retained plus two temporary globals exist; partial pairs roll back.
        api.Release(movie_);api.Release(module_);
        module_=next_module;movie_=next_movie;epoch_=Advance();return epoch_;
    }

    // Always invalidate publication; leave references retryable when the API or
    // entry exception prevents cleanup. Serial intentionally survives Clear.
    [[nodiscard]] bool Clear(void* env) noexcept {
        epoch_=0;
        if(!module_&&!movie_)return true;
        movie_input_owner_detail::Api api;
        if(!api.Open(env)||!api.Check())return false;
        api.Release(movie_);api.Release(module_);movie_=module_=nullptr;return true;
    }
private:
    std::uint64_t Invalidate() noexcept {epoch_=0;return 0;}
    std::uint64_t Advance() noexcept {
        ++serial_;if(!serial_)++serial_;return serial_;
    }
    void* module_=nullptr;void* movie_=nullptr; // Owned GLOBAL references only.
    std::uint64_t serial_=0,epoch_=0;
};
} // namespace cojvr::games::call_of_juarez
