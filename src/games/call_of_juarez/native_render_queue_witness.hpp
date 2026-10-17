#pragma once
#include "games/call_of_juarez/ppfx_resource_observation.hpp"
#include "runtime/vr_types.hpp"
#include <array>
#include <cstdint>
#include <limits>

namespace cojvr::games::call_of_juarez {
// Exact x86 factory contract. SHA admission is supplied by the snapshot caller.
inline constexpr std::array<std::uint32_t,3> kCoJQueueFunctionRvas{
    0x2466a0,0x247990,0x246f20};
inline constexpr bool CoJNativeQueueFactoryValid(std::uint32_t base,
    std::uint32_t table,const std::array<std::uint32_t,3>& slots) noexcept {
    if(!base||base>0xffffffffu-0x5900bcu||table!=base+0x312334)return false;
    for(unsigned i=0;i<3;++i)if(slots[i]!=base+kCoJQueueFunctionRvas[i])return false;
    return true;
}
struct CoJNativeQueueIdentity {
    std::uint32_t engine=0,renderer=0,device=0,thread=0;
    std::uint64_t generation=0;
    bool operator==(const CoJNativeQueueIdentity&) const=default;
};
struct CoJNativeQueueTuple {
    // [renderer+1DCC,renderer+1DF4): arena, cursor, backing, count,
    // borrowed effect, four pass selectors, pass count. Never written/restored.
    std::array<std::uint32_t,10> words{};
    std::uint32_t pending_a=0,pending_b=0; // engine+5900A8/+5900B8
    bool operator==(const CoJNativeQueueTuple&) const=default;
    bool Valid() const noexcept {
        return words[0]&&words[0]!=0xffffffffu&&words[2]&&words[2]!=0xffffffffu&&
            std::uint64_t(words[2])+std::uint64_t(words[3])*8+8<=0xffffffffu&&
            std::uint64_t(words[0])+words[1]<=0xffffffffu&&
            words[9]>=1&&words[9]<=4&&!pending_a&&!pending_b;
    }
};
enum class CoJNativeQueueEvent : unsigned { begin_effect,playback,enqueue };

// Pure event policy; the production observer serializes every call. Completion
// proves native CPU command consumption only, not GPU execution/retirement.
class CoJNativeQueuePolicy {
public:
    void Configure(const CoJNativeQueueIdentity& id) noexcept {
        if(id==identity_)return;
        if(open_||!idle()){Fail();return;}
        (void)Invalidate();identity_=id;
    }
    void Fail() noexcept {poisoned_=true;have_completion_=false;awaiting_=false;}
    void Enter(CoJNativeQueueEvent event,const CoJNativeQueueIdentity& id,
        std::uint32_t caller) noexcept {
        const auto i=static_cast<unsigned>(event);
        if(id!=identity_)Fail();
        if(depth_[i]==std::numeric_limits<unsigned>::max()){Fail();return;}
        if(event==CoJNativeQueueEvent::playback){
            if(!idle())Fail();
            playback_scope_=open_?scope_serial_:0;
        }else{
            if(producers_==std::numeric_limits<std::uint64_t>::max())Fail();
            else ++producers_;
            // Only the two initial immediate-batch flushes are established
            // enqueue callers during Playback. PP-tail reentry is not admitted.
            if(depth_[1]&&(event!=CoJNativeQueueEvent::enqueue||
                (caller!=0x214369&&caller!=0x2143a9)))Fail();
        }
        ++depth_[i];
    }
    void Leave(CoJNativeQueueEvent event,const CoJNativeQueueIdentity& id,
        const CoJNativeQueueTuple& tuple,std::uint32_t caller,bool sort=true) noexcept {
        const auto i=static_cast<unsigned>(event);
        if(!depth_[i]){Fail();return;}
        --depth_[i];
        if(id!=identity_){Fail();return;}
        if(event!=CoJNativeQueueEvent::playback)return;
        if(!idle()||!tuple.Valid()||(caller==0x30801&&!sort))Fail();
        if(poisoned_)return;
        if(sequence_==std::numeric_limits<std::uint64_t>::max()){Fail();return;}
        ++sequence_;completed_producers_=producers_;completed_=tuple;
        completion_caller_=caller;completion_scope_=playback_scope_;
        have_completion_=true;
    }
    bool Begin(const CoJNativeQueueTuple& tuple,std::uint64_t frame,
        runtime::Eye eye) noexcept {
        awaiting_=false;
        if(open_){Fail();return false;}
        if(poisoned_||!idle()||!frame||
            (eye!=runtime::Eye::left&&eye!=runtime::Eye::right))return false;
        if(!tuple.Valid()||(tuple.words[3]&&(!have_completion_||
            completed_producers_!=producers_||tuple!=completed_))){
            awaiting_=true;return false;
        }
        if(scope_serial_==std::numeric_limits<std::uint64_t>::max()){Fail();return false;}
        ++scope_serial_;entry_sequence_=sequence_;frame_=frame;eye_=eye;open_=true;
        return true;
    }
    bool Complete(const CoJNativeQueueTuple& tuple) noexcept {
        const bool was_open=open_;open_=false;awaiting_=false;
        const bool ok=was_open&&!poisoned_&&idle()&&have_completion_&&tuple.Valid()&&
            sequence_>entry_sequence_&&completion_scope_==scope_serial_&&
            completion_caller_==0x30801&&completed_producers_==producers_&&
            completed_==tuple;
        // A failed boundary cannot seed the next private admission.
        if(was_open&&!ok)have_completion_=false;
        return ok;
    }
    bool Invalidate() noexcept {
        open_=false;awaiting_=false;have_completion_=false;
        poisoned_=!idle();return idle();
    }
    bool open() const noexcept{return open_;}
    bool idle() const noexcept{return depth_[0]==0&&depth_[1]==0&&depth_[2]==0;}
    bool awaiting_natural_completion() const noexcept{return awaiting_;}
    const CoJNativeQueueIdentity& identity() const noexcept{return identity_;}
private:
    CoJNativeQueueIdentity identity_{};
    CoJNativeQueueTuple completed_{};
    std::array<unsigned,3> depth_{};
    std::uint64_t producers_=0,completed_producers_=0,sequence_=0,entry_sequence_=0;
    std::uint64_t scope_serial_=0,playback_scope_=0,completion_scope_=0,frame_=0;
    runtime::Eye eye_=runtime::Eye::left;
    std::uint32_t completion_caller_=0;
    bool have_completion_=false,open_=false,poisoned_=false,awaiting_=false;
};

// Process-global hooks with retained/pinned module lifetime; one render boundary
// at a time. Invalidating/resetting keeps hooks installed to observe warm-up.
// Caller supplies fresh exact-SHA-admitted snapshot and full CLevel wrapper route.
class CoJNativeRenderQueueWitness final {
public:
    CoJNativeRenderQueueWitness() noexcept=default;
    ~CoJNativeRenderQueueWitness();
    CoJNativeRenderQueueWitness(const CoJNativeRenderQueueWitness&)=delete;
    CoJNativeRenderQueueWitness& operator=(const CoJNativeRenderQueueWitness&)=delete;
    [[nodiscard]] bool Begin(const CoJPostprocessResourceSnapshot&,
        std::uint64_t generation,std::uint64_t frame,runtime::Eye) noexcept;
    [[nodiscard]] bool Complete() noexcept;
    [[nodiscard]] bool Invalidate() noexcept;
    [[nodiscard]] bool Restore() noexcept;
    [[nodiscard]] bool awaiting_natural_completion() const noexcept{return awaiting_;}
private:
    bool boundary_=false,awaiting_=false;
};
} // namespace cojvr::games::call_of_juarez
