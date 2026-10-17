#include "games/call_of_juarez/native_render_queue_witness.hpp"
#include "backends/d3d9/hook_registry.hpp"
#include <windows.h>
#include <intrin.h>
#include <mutex>
#include <new>

namespace cojvr::games::call_of_juarez {
static_assert(sizeof(void*)==4,"CoJ native queue hooks require x86");
namespace {
using backends::d3d9::HookRegistry;
using backends::d3d9::HookRegistryResult;
using Event=CoJNativeQueueEvent;
// Qualified from immutable SHA-pinned bytes: ret 4, ret 4, ret 28h.
using BeginFn=void(__thiscall*)(void*,void*);
using PlaybackFn=void(__thiscall*)(void*,bool);
using EnqueueFn=void(__thiscall*)(void*,std::uint32_t,std::uint32_t,
    std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,
    std::uint32_t,std::uint32_t,std::uint32_t);
void __fastcall BeginHook(void*,void*,void*);
void __fastcall PlaybackHook(void*,void*,bool);
void __fastcall EnqueueHook(void*,void*,std::uint32_t,std::uint32_t,
    std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,
    std::uint32_t,std::uint32_t,std::uint32_t);

struct HookSet {
    HookRegistry registry;
    void** table=nullptr;
    HookSet* previous=nullptr; // Keep every retired original for in-flight calls.
};
struct State {
    std::mutex mutex;
    CoJNativeQueuePolicy policy;
    HookSet* hooks=nullptr;
    CoJNativeRenderQueueWitness* boundary_owner=nullptr;
    bool installed=false,pending_restore=false;
};
State& Global(){
    // The hook module is pinned before install. No static destructor may retire
    // original targets while native callbacks are still unwinding at shutdown.
    static State* state=new State;return *state;
}
template<class T> std::uint32_t Address(T* p) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}
bool Read(std::uint32_t base,std::uint32_t offset,void* out,std::size_t size) noexcept {
    if(!base||!size||std::uint64_t(base)+offset+size>0xffffffffu)return false;
    const DWORD error=GetLastError();SIZE_T copied=0;
    const bool ok=ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(base)+offset),out,size,&copied)&&copied==size;
    SetLastError(error);return ok;
}
bool ReadTuple(const CoJNativeQueueIdentity& id,CoJNativeQueueTuple& q) noexcept {
    std::uint32_t renderer=0,device=0,table=0;
    return Read(id.engine,0x590074,&renderer,4)&&renderer==id.renderer&&
        Read(id.engine,0x590078,&device,4)&&device==id.device&&
        Read(id.renderer,0,&table,4)&&table==id.engine+0x312334&&
        Read(id.renderer,0x1dcc,q.words.data(),sizeof(q.words))&&
        Read(id.engine,0x5900a8,&q.pending_a,4)&&
        Read(id.engine,0x5900b8,&q.pending_b,4);
}
std::array<void*,3> Replacements() noexcept {
    return {reinterpret_cast<void*>(&BeginHook),reinterpret_cast<void*>(&PlaybackHook),
        reinterpret_cast<void*>(&EnqueueHook)};
}
bool SlotsOwned(const State& s) noexcept {
    if(!s.installed||!s.hooks)return false;
    std::array<std::uint32_t,3> slots{};
    if(!Read(Address(s.hooks->table),0x80,slots.data(),sizeof(slots)))return false;
    const auto replacements=Replacements();
    for(unsigned i=0;i<3;++i){
        if(slots[i]!=Address(replacements[i])||
            Address(s.hooks->registry.OriginalTarget(s.hooks->table,32+i))!=
                s.policy.identity().engine+kCoJQueueFunctionRvas[i])return false;
    }
    return true;
}
bool Install(State& s,std::uint32_t base,std::uint32_t table) noexcept {
    if(s.installed)return Address(s.hooks->table)==table&&SlotsOwned(s);
    if(s.pending_restore)return false;
    std::array<std::uint32_t,3> slots{};
    if(!Read(table,0x80,slots.data(),sizeof(slots))||
        !CoJNativeQueueFactoryValid(base,table,slots))return false;
    if(!backends::d3d9::PinModuleForAddress(reinterpret_cast<void*>(&BeginHook)))return false;
    auto* hooks=new(std::nothrow) HookSet;
    if(!hooks)return false;
    hooks->table=reinterpret_cast<void**>(static_cast<std::uintptr_t>(table));
    hooks->previous=s.hooks;s.hooks=hooks;
    const auto replacements=Replacements();
    const std::array<backends::d3d9::HookSlotRequest,3> requests{{
        {32,replacements[0]},{33,replacements[1]},{34,replacements[2]}}};
    const auto result=hooks->registry.Install(hooks->table,requests);
    s.pending_restore=result.ownership_record_retained;
    bool ok=result.result==HookRegistryResult::Installed;
    // HookRegistry retains the actual captured originals, not guessed RVAs.
    // Recheck them after install to reject a foreign precheck/install race.
    for(unsigned i=0;i<3;++i)ok=ok&&
        Address(hooks->registry.OriginalTarget(hooks->table,32+i))==base+kCoJQueueFunctionRvas[i];
    if(ok){s.installed=true;return true;}
    const auto restored=hooks->registry.Restore(hooks->table);
    s.pending_restore=restored.result!=HookRegistryResult::Installed&&
        restored.result!=HookRegistryResult::AlreadyInstalled;
    s.policy.Fail();return false;
}

// No mutex is held across native execution. Exceptions reject observation and
// unwind depth without inventing a normal native return/completion token.
class Callback {
public:
    Callback(void* self,Event event,void* return_address):event_(event){
        auto& s=Global();std::lock_guard lock(s.mutex);
        id_=s.policy.identity();id_.renderer=Address(self);id_.thread=GetCurrentThreadId();
        const auto address=Address(return_address);
        caller_=address>=id_.engine?address-id_.engine:0xffffffffu;
        if(s.hooks)original_=s.hooks->registry.OriginalTarget(s.hooks->table,
            32+static_cast<unsigned>(event));
        if(!original_||!s.installed)s.policy.Fail();
        CoJNativeQueueTuple q{};
        if(!ReadTuple(id_,q))s.policy.Fail();
        s.policy.Enter(event_,id_,caller_);
    }
    ~Callback(){
        if(returned_)return;
        const DWORD error=GetLastError();auto& s=Global();std::lock_guard lock(s.mutex);
        s.policy.Fail();s.policy.Leave(event_,id_,{},caller_);SetLastError(error);
    }
    void Returned(bool sort=true) noexcept {
        const DWORD error=GetLastError();auto& s=Global();std::lock_guard lock(s.mutex);
        CoJNativeQueueTuple q{};
        if(!ReadTuple(id_,q))s.policy.Fail();
        s.policy.Leave(event_,id_,q,caller_,sort);returned_=true;SetLastError(error);
    }
    void* original() const noexcept{return original_;}
private:
    CoJNativeQueueIdentity id_{};
    Event event_;std::uint32_t caller_=0;
    void* original_=nullptr;bool returned_=false;
};
__declspec(noinline) void __fastcall BeginHook(void* self,void*,void* effect){
    const DWORD error=GetLastError();Callback c(self,Event::begin_effect,_ReturnAddress());
    SetLastError(error);
    if(const auto original=reinterpret_cast<BeginFn>(c.original()))original(self,effect);
    c.Returned();
}
__declspec(noinline) void __fastcall PlaybackHook(void* self,void*,bool sort){
    const DWORD error=GetLastError();Callback c(self,Event::playback,_ReturnAddress());
    SetLastError(error);
    if(const auto original=reinterpret_cast<PlaybackFn>(c.original()))original(self,sort);
    c.Returned(sort);
}
__declspec(noinline) void __fastcall EnqueueHook(void* self,void*,
    std::uint32_t a1,std::uint32_t a2,std::uint32_t a3,std::uint32_t a4,
    std::uint32_t a5,std::uint32_t a6,std::uint32_t a7,std::uint32_t a8,
    std::uint32_t a9,std::uint32_t a10){
    const DWORD error=GetLastError();Callback c(self,Event::enqueue,_ReturnAddress());
    SetLastError(error);
    if(const auto original=reinterpret_cast<EnqueueFn>(c.original()))
        original(self,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10);
    c.Returned();
}
} // namespace

CoJNativeRenderQueueWitness::~CoJNativeRenderQueueWitness(){(void)Invalidate();}
bool CoJNativeRenderQueueWitness::Begin(const CoJPostprocessResourceSnapshot& snapshot,
    std::uint64_t generation,std::uint64_t frame,runtime::Eye eye) noexcept {
    try{
        auto& s=Global();std::lock_guard lock(s.mutex);awaiting_=false;
        if(boundary_||s.boundary_owner){s.policy.Fail();return false;}
        if(!snapshot.valid||snapshot.status!=CoJPostprocessObservationStatus::observed||
            !generation||!frame||!snapshot.renderer||!snapshot.device||
            snapshot.engine_base>0xffffffffu-0x5900bcu||
            snapshot.renderer_vtable!=snapshot.engine_base+0x312334)return false;
        const CoJNativeQueueIdentity id{snapshot.engine_base,snapshot.renderer,
            snapshot.device,GetCurrentThreadId(),generation};
        const auto& previous=s.policy.identity();
        if(previous.renderer==id.renderer&&previous.generation==id.generation&&
            previous.thread&&previous.thread!=id.thread){s.policy.Fail();return false;}
        s.policy.Configure(id);
        // Fault-safe roots/class reads precede Registry's bounded vtable access.
        CoJNativeQueueTuple q{};
        if(!ReadTuple(id,q)){s.policy.Fail();return false;}
        if(!Install(s,id.engine,snapshot.renderer_vtable))return false;
        if(!s.policy.Begin(q,frame,eye)){
            awaiting_=s.policy.awaiting_natural_completion();return false;
        }
        boundary_=true;s.boundary_owner=this;return true;
    }catch(...){return false;}
}
bool CoJNativeRenderQueueWitness::Complete() noexcept {
    awaiting_=false;
    try{
        auto& s=Global();std::lock_guard lock(s.mutex);
        const bool mine=boundary_&&s.boundary_owner==this;boundary_=false;
        if(!mine)return false;
        s.boundary_owner=nullptr;
        CoJNativeQueueTuple q{};
        if(GetCurrentThreadId()!=s.policy.identity().thread||!SlotsOwned(s)||
            !ReadTuple(s.policy.identity(),q))s.policy.Fail();
        return s.policy.Complete(q); // Always closes, including failed reads.
    }catch(...){boundary_=false;return false;}
}
bool CoJNativeRenderQueueWitness::Invalidate() noexcept {
    const bool closed=!boundary_||Complete();awaiting_=false;
    try{
        auto& s=Global();std::lock_guard lock(s.mutex);
        if(s.boundary_owner&&s.boundary_owner!=this){s.policy.Fail();return false;}
        return s.policy.Invalidate()&&closed;
    }catch(...){return false;}
}
bool CoJNativeRenderQueueWitness::Restore() noexcept {
    const bool closed=Invalidate();
    try{
        auto& s=Global();std::lock_guard lock(s.mutex);
        if(!s.policy.idle()||s.boundary_owner)return false;
        if(!s.hooks)return closed;
        const auto outcome=s.hooks->registry.Restore(s.hooks->table);
        const bool restored=outcome.result==HookRegistryResult::Installed||
            outcome.result==HookRegistryResult::AlreadyInstalled;
        if(restored){s.installed=false;s.pending_restore=false;}
        else{s.pending_restore=true;s.policy.Fail();}
        return restored&&closed;
    }catch(...){return false;}
}
} // namespace cojvr::games::call_of_juarez
