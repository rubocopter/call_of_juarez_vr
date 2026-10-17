#pragma once
#include "games/call_of_juarez/native_render_transaction.hpp"
#include "games/call_of_juarez/independent_resolution_recipe.hpp"
#include <algorithm>

namespace cojvr::games::call_of_juarez {
struct CoJIndependentFieldPayload {
    std::array<std::uint32_t,14> textures{};
    // final color/depth, internal color/depth, additional depth.
    std::array<std::uint32_t,5> surfaces{};
    std::uint32_t additional_width=0,additional_height=0;
};
// Only exact-build, value/cache fields. Native object identities, references,
// allocation metadata, registration and effect command graphs remain untouched.
inline bool StageCoJIndependentRenderFields(CoJNativeRenderTransaction& tx,
    CoJRenderMemory io,std::uint32_t renderer,const CoJIndependentResolutionRecipe& r,
    const CoJIndependentFieldPayload& p) noexcept {
    if(!io.read||!renderer||renderer>0xffffdfffu||!r.width||!r.height)return false;
    const auto read=[&](std::uint32_t a,void* out,std::size_t n){return io.read(io.context,a,out,n);};
    const auto stage=[&](std::uint32_t offset,const auto& value){
        return tx.Stage(renderer+offset,&value,sizeof(value));};
    const auto snapshot=[&](std::uint32_t offset,std::size_t n){
        while(n){const auto part=(std::min)(n,tx.field_bytes);
            if(!tx.Snapshot(renderer+offset,part))return false;
            offset+=static_cast<std::uint32_t>(part);n-=part;}return true;};
    for(std::size_t i=0;i<r.textures.size();++i){
        const auto& t=r.textures[i];
        if(!t.handle||t.handle>0xfffffe7fu||!t.entry||t.entry>0xffffffb3u||
           !t.width||!t.height||!p.textures[i])return false;
        const std::array<std::uint32_t,2> interfaces{p.textures[i],p.textures[i]};
        std::array<unsigned char,36> dims{};
        if(!read(t.handle+0x13c,dims.data(),dims.size()))return false;
        const float inverse_w=1.0f/t.width,inverse_h=1.0f/t.height;
        std::memcpy(dims.data(),&t.width,4);std::memcpy(dims.data()+4,&t.height,4);
        std::memcpy(dims.data()+12,&t.width,4);std::memcpy(dims.data()+16,&t.height,4);
        std::memcpy(dims.data()+24,&inverse_w,4);std::memcpy(dims.data()+28,&inverse_h,4);
        const std::array<std::uint32_t,2> logical{t.width,t.height};
        if(!tx.Stage(t.handle,interfaces.data(),sizeof(interfaces))||
           !tx.Stage(t.handle+0x13c,dims.data(),dims.size())||
           !tx.Stage(t.entry+0x44,logical.data(),sizeof(logical)))return false;
    }
    for(const auto surface:p.surfaces)if(!surface)return false;
    const std::array<std::uint32_t,2> dimensions{r.width,r.height};
    const std::array<std::uint32_t,2> internal{p.surfaces[2],p.surfaces[3]};
    const std::array<std::uint32_t,2> additional{p.additional_width,p.additional_height};
    std::array<unsigned char,24> target{};
    if(!read(renderer+0x1db4,target.data(),target.size()))return false;
    std::memcpy(target.data()+16,&p.surfaces[0],4);
    std::memcpy(target.data()+20,&p.surfaces[1],4);
    // D3DVIEWPORT9 is six 32-bit words, including raw float MinZ/MaxZ.
    std::array<std::uint32_t,6> viewport{0,0,r.width,r.height,0,0x3f800000};
    std::uint32_t dirty=0;
    if(!read(renderer+0x1bc,&dirty,4))return false;
    dirty|=0x00622000;
    if(!stage(0x1c8,dimensions)||!stage(0x15c4,dimensions)||!stage(0x1c18,internal)||
       !stage(0x1c24,dimensions)||!stage(0x1c2c,p.surfaces[4])||!stage(0x1c34,additional)||
       !stage(0x1c58,viewport)||!stage(0x1db4,target)||!stage(0x1bc,dirty))return false;
    // Cache block includes borrowed material/shader/state identities; only the
    // texture handle caches are cleared so the binder cannot skip SetTexture.
    std::array<unsigned char,0xe4> cache{};
    if(!read(renderer+0x1cd0,cache.data(),cache.size()))return false;
    std::fill_n(cache.data(),64,static_cast<unsigned char>(0));
    std::fill_n(cache.data()+0x80,16,static_cast<unsigned char>(0));
    for(std::size_t i=0;i<cache.size();i+=tx.field_bytes){
        const auto n=(std::min)(tx.field_bytes,cache.size()-i);
        if(!tx.Stage(renderer+0x1cd0+static_cast<std::uint32_t>(i),cache.data()+i,n))return false;
    }
    // Native draw vectors, transform source/cameras, scalar/force, materialized
    // CPU float4 parameters and logical texture inputs must match ALL GPU state.
    // Six initialized 64-byte value matrices, bounded by object header and
    // per-draw vectors (native 2136C0, 2143B0/400/440/4A0, 213A50).
    return snapshot(0x08,0x180)&&snapshot(0x188,0x34)&&snapshot(0x1f1,1)&&snapshot(0x3ac,1)&&
        snapshot(0x1c71,1)&&snapshot(0x1c74,0x18)&&
        snapshot(0x404,0x1080);
}
}
