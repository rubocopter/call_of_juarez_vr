#include "games/call_of_juarez/independent_resolution_recipe.hpp"
#include <cstring>
#include <iostream>
using namespace cojvr::games::call_of_juarez;
CoJPostprocessResourceSnapshot Sample(){
    CoJPostprocessResourceSnapshot s;s.valid=true;s.status=CoJPostprocessObservationStatus::observed;
    s.engine_base=0x10000000;s.renderer=0x90000000;s.manager=0xa0000000;s.device=0xb0000000;
    s.effect_count=s.loaded_effect_count=5;s.buffer_count=14;
    s.renderer_state.logical_width=2560;s.renderer_state.logical_height=1440;
    s.renderer_state.viewport_clamp_width=2560;s.renderer_state.viewport_clamp_height=1440;
    const char* effects[]{"null.ppfx","hsm.ppfx","scene30.ppfx","scene30simple.ppfx","clear_z.ppfx"};
    for(unsigned i=0;i<5;++i){auto& e=s.effects[i];std::strcpy(e.name.data(),effects[i]);
        e.effect=0xc0000000+i*0x1000;e.vtable=s.engine_base+0x3180ac;e.buffer_count=i==2?14:0;}
    const char* names[]{"1A_16","1B_16","4A_16","1xScreenY_16","1x1_16","1A","1B","2A","4A","4B","8A","16A","16B","16Prev"};
    const int dx[]{1,1,4,0,0,1,1,2,4,4,8,16,16,16};
    for(unsigned i=0;i<14;++i){auto& b=s.buffers[i];std::strcpy(b.name.data(),names[i]);
        b.container=s.effects[2].effect;b.index=i;b.entry=b.container+4+i*0x64;
        b.width_divisor=dx[i];b.height_divisor=i==3?4:dx[i];
        b.logical_width=dx[i]?2560/dx[i]:1;b.logical_height=i==3?360:dx[i]?1440/dx[i]:1;
        b.temp_shareable=i!=4&&i!=13;b.internal_color=b.depth_attachment=i<2;b.native_bpp=i>=5;
        auto& t=b.texture;t.handle=0xd0000000+i*0x1000;t.texture_2d=t.selected_interface=0xe0000000+i*0x1000;
        t.width=t.requested_width=b.logical_width;t.height=t.requested_height=b.logical_height;
        t.kind=0;t.format=i<5?113:21;t.usage=1;t.flags=i<2?0x1d:5;
    }return s;
}
int main(){bool ok=true;CoJIndependentResolutionRecipe p{};
    auto s=Sample();ok&=BuildCoJIndependentResolutionRecipe(s,1920,p);
    ok&=p.width==2560&&p.height==1920&&p.textures[0].height==1920&&
        p.textures[3].width==1&&p.textures[3].height==480&&p.textures[13].height==120&&
        p.textures[4].history&&p.textures[13].history&&!p.textures[12].history;
    ok&=BuildCoJIndependentResolutionRecipe(s,1440,p)&&p.textures[13].height==90;
    for(unsigned bad:{0u,1080u,2560u,2860u})ok&=!BuildCoJIndependentResolutionRecipe(s,bad,p);
    for(unsigned bad=0;bad<11;++bad){auto f=Sample();
        switch(bad){case 0:f.valid=false;break;case 1:f.manager_dirty=1;break;
        case 2:f.renderer_state.current_target=4;break;case 3:f.effects[0].name[0]='x';break;
        case 4:f.buffers[2].linked_borrowed=1;break;case 5:f.buffers[2].texture.kind=1;break;
        case 6:f.buffers[3].width_divisor=1;break;case 7:f.buffers[6].texture.handle=f.buffers[5].texture.handle;break;
        case 8:f.buffers[0].texture.width=4096;break;case 9:f.global_buffer_count=1;break;
        case 10:f.buffers[13].temp_shareable=1;break;}
        ok&=!BuildCoJIndependentResolutionRecipe(f,1920,p);
    }
    if(!ok)std::cerr<<"independent recipe failed size/history/unsupported-family admission\n";
    return ok?0:1;
}
