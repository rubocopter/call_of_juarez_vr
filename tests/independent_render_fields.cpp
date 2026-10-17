#include "games/call_of_juarez/independent_render_fields.hpp"
#include <array>
#include <cstring>
#include <iostream>
using namespace cojvr::games::call_of_juarez;
namespace {
struct Memory {
    std::array<unsigned char,0x20000> bytes{};
    unsigned writes=0,fail=0;
    static bool Read(void* p,std::uint32_t a,void* out,std::size_t n) noexcept {
        auto& m=*static_cast<Memory*>(p);
        if(a<0x90000000u||std::uint64_t(a)-0x90000000u+n>m.bytes.size())return false;
        std::memcpy(out,m.bytes.data()+a-0x90000000u,n);return true;
    }
    static bool Write(void* p,std::uint32_t a,const void* in,std::size_t n) noexcept {
        auto& m=*static_cast<Memory*>(p);++m.writes;
        if(a<0x90000000u||std::uint64_t(a)-0x90000000u+n>m.bytes.size())return false;
        const bool failed=m.fail&&m.writes==m.fail;
        std::memcpy(m.bytes.data()+a-0x90000000u,in,failed?n/2:n);return !failed;
    }
    CoJRenderMemory Io(){return {Read,Write,this};}
    template<class T> T At(std::uint32_t a){T v{};Read(this,a,&v,sizeof(v));return v;}
};
}
int main(){
    Memory m;for(std::size_t i=0;i<m.bytes.size();++i)m.bytes[i]=static_cast<unsigned char>(i*17);
    const auto original=m.bytes;
    CoJIndependentResolutionRecipe recipe;recipe.width=2560;recipe.height=1920;
    CoJIndependentFieldPayload payload;
    for(unsigned i=0;i<14;++i){auto& t=recipe.textures[i];
        t.handle=0x90004000u+i*0x200;t.entry=0x90008000u+i*0x100;
        t.width=i==4?1:2560;t.height=i==4?1:1920;
        payload.textures[i]=0xa0000000u+i*0x1000;
    }
    for(unsigned i=0;i<5;++i)payload.surfaces[i]=0xb0000000u+i*0x1000;
    payload.additional_width=2560;payload.additional_height=2048;
    CoJNativeRenderTransaction tx(m.Io());
    if(!StageCoJIndependentRenderFields(tx,m.Io(),0x90000000,recipe,payload)||m.writes||!tx.Apply())return 1;
    bool ok=m.At<std::uint32_t>(0x900001cc)==1920&&m.At<std::uint32_t>(0x90001c64)==1920&&
        m.At<std::uint32_t>(0x90001dc4)==payload.surfaces[0]&&
        m.At<std::uint32_t>(0x90001c2c)==payload.surfaces[4];
    for(unsigned i=0;i<14;++i){const auto& t=recipe.textures[i];
        ok&=m.At<std::uint32_t>(t.handle)==payload.textures[i]&&
            m.At<std::uint32_t>(t.handle+4)==payload.textures[i]&&
            m.At<std::uint32_t>(t.handle+0x14c)==t.height&&
            m.At<float>(t.handle+0x158)==1.0f/t.height&&m.At<std::uint32_t>(t.entry+0x48)==t.height;
        // Native identity/refcount/pool/name and all three depth fields stay intact.
        for(unsigned offset:{8u,0x10u,0x130u,0x134u,0x138u,0x144u,0x150u,0x15cu,0x168u})
            ok&=std::memcmp(m.bytes.data()+t.handle-0x90000000+offset,
                original.data()+t.handle-0x90000000+offset,4)==0;
    }
    ok&=m.At<std::uint32_t>(0x90001cd0)==0&&m.At<std::uint32_t>(0x90001d50)==0;
    // Simulate wrapper CPU shader sources and caches, without native ownership writes.
    m.bytes[0x404]=91;m.bytes[0x188]=92;m.bytes[0x1404]=93;m.bytes[0x1db0]=94;
    ok&=tx.Restore()&&m.bytes==original;
    // Exercise partial OS writes at every entry/rollback position, including
    // fields that are snapshot-only at entry but changed by native commands.
    for(unsigned fault=1;fault<=180;++fault){
        m.bytes=original;m.writes=0;m.fail=fault;
        CoJNativeRenderTransaction attempt(m.Io());
        if(!StageCoJIndependentRenderFields(attempt,m.Io(),0x90000000,recipe,payload))return 1;
        if(attempt.Apply()){
            m.bytes[0x404]=91;m.bytes[8]=95;m.bytes[0x1c74]=96;
            if(!attempt.Restore())ok&=attempt.restoration_pending()&&attempt.Restore();
        }
        ok&=!attempt.restoration_pending()&&m.bytes==original;
    }
    if(!ok)std::cerr<<"native fields/source rollback or reserved identity changed\n";
    return ok?0:1;
}
