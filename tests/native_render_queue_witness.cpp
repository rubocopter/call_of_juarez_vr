#include "games/call_of_juarez/native_render_queue_witness.hpp"
#include <iostream>
using namespace cojvr::games::call_of_juarez;
namespace {
bool Check(bool v,const char* s){if(!v)std::cerr<<s<<'\n';return v;}
using Event=CoJNativeQueueEvent;
CoJNativeQueueIdentity Identity(){return {0x10000000,0x81000000,0x82000000,17,4};}
CoJNativeQueueTuple Tuple(std::uint32_t count=7){
    CoJNativeQueueTuple q{};q.words={0x83000000,0x100,0x84000000,count,
        0x85000000,1,2,3,4,4};return q;
}
void Playback(CoJNativeQueuePolicy& p,const CoJNativeQueueIdentity& id,
    const CoJNativeQueueTuple& q,std::uint32_t caller=0x30801){
    p.Enter(Event::playback,id,caller);
    p.Leave(Event::playback,id,q,caller);
}
}
int main(){
    bool ok=true;auto id=Identity();auto q=Tuple();
    ok&=Check(CoJNativeQueueFactoryValid(0x10000000,0x10312334,
        {0x102466a0,0x10247990,0x10246f20}),"exact factory addresses");
    ok&=Check(!CoJNativeQueueFactoryValid(0x10000000,0x10312334,
        {0x102466a0,0x10247991,0x10246f20}),"foreign playback rejects");
    ok&=Check(!CoJNativeQueueFactoryValid(0xfff00000,0x00212334,{}),"base overflow rejects");
    {
        CoJNativeQueuePolicy p;p.Configure(id);
        ok&=Check(!p.Begin(q,1,cojvr::runtime::Eye::left)&&p.awaiting_natural_completion(),"nonempty first install warms up");
        ok&=Check(!p.open(),"warmup leaves no boundary");
        Playback(p,id,q);
        ok&=Check(p.Begin(q,2,cojvr::runtime::Eye::left),"consumed retained nonzero count admits");
        Playback(p,id,q);
        ok&=Check(p.Complete(q)&&!p.open(),"fresh final completion closes");
        ok&=Check(!p.Complete(q),"completion cannot be reused");
        ok&=Check(p.Begin(q,2,cojvr::runtime::Eye::right),"other eye uses retained natural tuple");
        ok&=Check(!p.Complete(q)&&!p.open(),"old epoch cannot qualify new eye; failure closes");
    }
    {
        CoJNativeQueuePolicy p;p.Configure(id);auto empty=Tuple(0);
        ok&=Check(p.Begin(empty,3,cojvr::runtime::Eye::left),"empty queue admits without witness");
        Playback(p,id,q,0x2f390);
        ok&=Check(!p.Complete(q)&&!p.open(),"additional-map completion is not final playback");
    }
    {
        CoJNativeQueuePolicy p;p.Configure(id);Playback(p,id,q);
        ok&=Check(p.Begin(q,4,cojvr::runtime::Eye::left),"start producer fixture");
        p.Enter(Event::playback,id,0x30801);
        p.Enter(Event::enqueue,id,0x214369);p.Leave(Event::enqueue,id,q,0x214369);
        p.Leave(Event::playback,id,q,0x30801);
        ok&=Check(p.Complete(q),"initial flush enqueue inside playback counted before completion");
        ok&=Check(p.Begin(q,5,cojvr::runtime::Eye::left),"start late producer fixture");
        Playback(p,id,q);p.Enter(Event::enqueue,id,0x123);p.Leave(Event::enqueue,id,q,0x123);
        ok&=Check(!p.Complete(q)&&!p.open(),"late producer rejects even with identical tuple");
        ok&=Check(!p.Begin(q,6,cojvr::runtime::Eye::left),"late work blocks next mutation");
        Playback(p,id,q);
        ok&=Check(p.Begin(q,6,cojvr::runtime::Eye::left),"natural playback consumes late work");
        Playback(p,id,q);auto changed=q;++changed.words[1];
        ok&=Check(!p.Complete(changed)&&!p.open(),"unobserved tuple change rejects");
    }
    {
        CoJNativeQueuePolicy p;p.Configure(id);Playback(p,id,q);
        auto changed=q;changed.words[0]+=16;
        ok&=Check(!p.Begin(changed,7,cojvr::runtime::Eye::left),"payload arena identity fenced");
        changed=q;changed.words[2]+=16;
        ok&=Check(!p.Begin(changed,7,cojvr::runtime::Eye::left),"backing identity fenced");
        auto next=id;++next.generation;p.Configure(next);
        ok&=Check(!p.Begin(q,7,cojvr::runtime::Eye::left),"generation cannot inherit witness");
        Playback(p,next,q);ok&=Check(p.Begin(q,7,cojvr::runtime::Eye::left),"new generation can warm up");
        auto foreign=next;++foreign.thread;p.Enter(Event::enqueue,foreign,0);
        p.Leave(Event::enqueue,foreign,q,0);
        Playback(p,next,q);
        ok&=Check(!p.Complete(q)&&!p.open(),"foreign thread taints scope despite later completion");
    }
    {
        CoJNativeQueuePolicy p;p.Configure(id);auto empty=Tuple(0);empty.pending_a=1;
        ok&=Check(!p.Begin(empty,8,cojvr::runtime::Eye::left),"pending immediate batch rejects");
        empty.pending_a=0;p.Begin(empty,8,cojvr::runtime::Eye::left);
        p.Enter(Event::playback,id,0x30801);
        ok&=Check(!p.Complete(empty)&&!p.open(),"active callback rejects and closes boundary");
        p.Leave(Event::playback,id,q,0x30801);
        ok&=Check(p.Invalidate()&&!p.open(),"reset invalidates retained witness");
        ok&=Check(!p.Begin(q,9,cojvr::runtime::Eye::left),"reset requires fresh natural completion");
    }
    {
        CoJNativeQueuePolicy p;p.Configure(id);auto empty=Tuple(0);
        p.Begin(empty,10,cojvr::runtime::Eye::left);
        ok&=Check(!p.Begin(empty,10,cojvr::runtime::Eye::right),"nested boundary rejects");
        Playback(p,id,q);ok&=Check(!p.Complete(q)&&!p.open(),"nested attempt taints existing boundary");
        p.Invalidate();p.Configure(id);p.Begin(empty,11,cojvr::runtime::Eye::left);
        p.Enter(Event::playback,id,0x30801);
        p.Enter(Event::enqueue,id,0x12345);p.Leave(Event::enqueue,id,q,0x12345);
        p.Leave(Event::playback,id,q,0x30801);
        ok&=Check(!p.Complete(q),"unexpected enqueue during playback tail rejects");
    }
    return ok?0:1;
}
