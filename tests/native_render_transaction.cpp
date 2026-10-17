#include "games/call_of_juarez/native_render_transaction.hpp"
#include <array>
#include <cstring>
#include <iostream>
using namespace cojvr::games::call_of_juarez;
namespace {
struct Memory {
    std::array<unsigned char,256> bytes{};
    unsigned writes=0, fail=0; bool persistent=false;
    static bool Read(void* p,std::uint32_t address,void* out,std::size_t size) noexcept {
        auto& m=*static_cast<Memory*>(p);
        if(address<0x90000000u || std::uint64_t(address)-0x90000000u+size>m.bytes.size())return false;
        std::memcpy(out,m.bytes.data()+address-0x90000000u,size);return true;
    }
    static bool Write(void* p,std::uint32_t address,const void* data,std::size_t size) noexcept {
        auto& m=*static_cast<Memory*>(p);++m.writes;
        if(address<0x90000000u || std::uint64_t(address)-0x90000000u+size>m.bytes.size())return false;
        const bool failure=m.fail&&(m.writes==m.fail||(m.persistent&&m.writes>=m.fail));
        // A failed OS write can still mutate a prefix; rollback must include it.
        std::memcpy(m.bytes.data()+address-0x90000000u,data,failure?size/2:size);
        return !failure;
    }
    CoJRenderMemory Io(){return {Read,Write,this};}
};
bool Check(bool v,const char* s){if(!v)std::cerr<<s<<'\n';return v;}
}
int main(){
    bool ok=true; const std::uint32_t a=0xdeadbeef,b=0x81234567;
    {
        Memory m;const auto original=m.bytes;CoJNativeRenderTransaction t(m.Io());
        ok&=Check(t.Stage(0x90000004,&a,4)&&t.Stage(0x90000020,&b,4),"stage LAA fields");
        ok&=Check(m.bytes==original&&t.Apply()&&m.bytes!=original,"staging must be read-only; apply changes fields");
        ok&=Check(!t.Apply()&&!t.Stage(0x90000030,&a,4),"active transaction cannot reenter");
        ok&=Check(t.Restore()&&m.bytes==original&&!t.restoration_pending(),"complete rollback");
    }
    for(unsigned failure=1;failure<=2;++failure){
        Memory m;m.fail=failure;const auto original=m.bytes;CoJNativeRenderTransaction t(m.Io());
        ok&=Check(t.Stage(0x90000004,&a,4)&&t.Stage(0x90000020,&b,4),"stage failure fixture");
        ok&=Check(!t.Apply()&&m.bytes==original&&!t.restoration_pending(),"partial failing write restored, including current field");
    }
    {
        Memory m;m.fail=2;m.persistent=true;CoJNativeRenderTransaction t(m.Io());
        t.Stage(0x90000004,&a,4);t.Stage(0x90000020,&b,4);
        ok&=Check(!t.Apply()&&t.restoration_pending(),"failed rollback remains latched");
        ok&=Check(!t.Stage(0x90000030,&a,4)&&!t.Apply(),"pending restore rejects new transaction");
        m.fail=0;ok&=Check(t.Restore()&&!t.restoration_pending()&&m.bytes==Memory{}.bytes,"retry only original rollback");
    }
    {
        Memory m;CoJNativeRenderTransaction t(m.Io());
        ok&=Check(t.Stage(0x90000004,&a,4)&&!t.Stage(0x90000005,&b,4),"overlapping writes rejected");
        m.bytes[4]=3;ok&=Check(!t.Apply()&&m.bytes[4]==3&&m.writes==0,"changed preimage preserved");
        ok&=Check(!t.Stage(0xffffffff,&a,4)&&!t.Stage(0,&a,4)&&
            !t.Stage(0xfffffffe,&a,4),"invalid pointer/overflow rejects");
    }
    {
        Memory m;const auto original=m.bytes;CoJNativeRenderTransaction t(m.Io());
        ok&=Check(t.Snapshot(0x90000040,80)&&t.Apply()&&m.writes==0,
            "snapshot-only sources are retained without entry writes");
        m.bytes[65]=123;
        ok&=Check(t.Restore()&&m.bytes==original&&t.Clear(),"wrapper source mutation rolls back and transaction rearms");
        ok&=Check(t.Stage(0x90000004,&a,4)&&t.Apply()&&!t.Clear(),"active transaction cannot discard rollback");
        ok&=Check(t.Restore()&&t.Clear(),"restored transaction can clear");
    }
    {
        Memory m;CoJNativeRenderTransaction t(m.Io());
        ok&=Check(t.Stage(0x90000004,&a,4),"stage independent preimage fixture");
        m.bytes[4]=3;ok&=Check(!t.Apply()&&m.bytes[4]==3&&m.writes==0,"changed preimage alone rejects before writes");
    }
    {
        Memory m;const auto original=m.bytes;
        {CoJNativeRenderTransaction t(m.Io());t.Stage(0x90000004,&a,4);t.Apply();}
        ok&=Check(m.bytes==original,"scope destruction restores fields");
    }
    return ok?0:1;
}
