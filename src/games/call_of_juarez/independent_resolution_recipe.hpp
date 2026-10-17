#pragma once
#include "games/call_of_juarez/ppfx_resource_observation.hpp"
#include <algorithm>
#include <string_view>

namespace cojvr::games::call_of_juarez {
struct CoJIndependentTextureRecipe {
    std::uint32_t handle=0,entry=0,original_interface=0,width=0,height=0,format=0;
    bool history=false;
};
struct CoJIndependentResolutionRecipe {
    std::uint32_t width=0,height=0;
    std::array<CoJIndependentTextureRecipe,14> textures{};
};
// Deliberately the single observed original-size family. New effects, aliases,
// formats, padding and layouts require qualification instead of guessed scaling.
inline bool BuildCoJIndependentResolutionRecipe(const CoJPostprocessResourceSnapshot& s,
    std::uint32_t height,CoJIndependentResolutionRecipe& output) noexcept {
    output={};
    if(!s.valid||s.status!=CoJPostprocessObservationStatus::observed||s.manager_dirty||
       (height!=1440&&height!=1920)||s.global_buffer_count||s.buffer_count!=14||
       s.effect_count!=5||s.loaded_effect_count!=5||s.renderer_state.current_target||
       s.renderer_state.logical_width!=2560||s.renderer_state.logical_height!=1440||
       s.renderer_state.viewport_clamp_width!=2560||s.renderer_state.viewport_clamp_height!=1440)return false;
    const auto name=[](const auto& chars) noexcept {
        const auto end=std::find(chars.begin(),chars.end(),0);
        return end==chars.end()?std::string_view{}:std::string_view(chars.data(),end-chars.begin());
    };
    constexpr std::array<std::string_view,5> effects{
        "null.ppfx","hsm.ppfx","scene30.ppfx","scene30simple.ppfx","clear_z.ppfx"};
    for(std::size_t i=0;i<effects.size();++i) {
        const auto& e=s.effects[i];
        if(name(e.name)!=effects[i]||e.vtable!=s.engine_base+0x3180ac||
           e.buffer_count!=(i==2?14:0))return false;
    }
    constexpr std::array<std::string_view,14> names{
        "1A_16","1B_16","4A_16","1xScreenY_16","1x1_16","1A","1B","2A","4A","4B","8A","16A","16B","16Prev"};
    constexpr int divisor[]{1,1,4,0,0,1,1,2,4,4,8,16,16,16};
    CoJIndependentResolutionRecipe result;result.width=2560;result.height=height;
    for(std::size_t i=0;i<names.size();++i) {
        const auto& b=s.buffers[i];const auto& t=b.texture;
        const auto dx=divisor[i],dy=i==3?4:dx;
        const auto old_w=static_cast<std::uint32_t>(dx?2560/dx:1);
        const auto old_h=static_cast<std::uint32_t>(dy?1440/dy:1);
        if(name(b.name)!=names[i]||b.global||b.container!=s.effects[2].effect||b.index!=i||
           b.entry!=b.container+4+i*0x64||b.width_divisor!=dx||b.height_divisor!=dy||
           b.logical_width!=static_cast<std::int32_t>(old_w)||
           b.logical_height!=static_cast<std::int32_t>(old_h)||b.linked_borrowed||
           b.temp_shareable!=(i!=4&&i!=13)||b.internal_color!=(i<2)||
           b.depth_attachment!=(i<2)||b.native_bpp!=(i>=5)||
           !t.handle||!t.texture_2d||t.texture_2d!=t.selected_interface||t.texture_cube||
           t.texture_volume||t.kind||t.usage!=1||t.format!=(i<5?113u:21u)||
           t.flags!=(i<2?0x1du:5u)||t.width!=old_w||t.height!=old_h||
           t.requested_width!=old_w||t.requested_height!=old_h)return false;
        for(std::size_t j=0;j<i;++j)
            if(result.textures[j].handle==t.handle||result.textures[j].original_interface==t.texture_2d)return false;
        result.textures[i]={t.handle,b.entry,t.texture_2d,
            static_cast<std::uint32_t>(dx?2560/dx:1),static_cast<std::uint32_t>(dy?height/dy:1),
            t.format,i==4||i==13};
    }
    output=result;return true;
}
}
