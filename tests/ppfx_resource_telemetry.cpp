#include "games/call_of_juarez/ppfx_resource_telemetry.hpp"
#include <iostream>
#include <limits>
#include <string_view>

int main(int argc,char** argv) {
    using namespace cojvr::games::call_of_juarez;
    CoJPostprocessResourceSnapshot s;
    s.valid=true;s.status=CoJPostprocessObservationStatus::observed;
    s.engine_base=0x10000000;s.renderer=0xA0000000;s.manager=0xA0001000;
    s.device=0xA0002000;s.renderer_vtable=0x10312334;s.list_sentinel=0xA0003000;
    s.manager_vtable=0x10314C48;s.loaded_effect_count=1;
    s.global_buffer_count=0;s.effect_count=1;s.buffer_count=2;
    s.effects[0].effect=0xA0010000;s.effects[0].buffer_count=2;
    s.effects[0].vtable=0x103180AC;
    s.effects[0].name[0]='x';s.effects[0].name[1]=static_cast<char>(0x80);
    s.buffers[0].container=s.effects[0].effect;s.buffers[0].entry=0xA0010004;
    const char hostile[]="HDR\"\\\n\t\x01\x80";
    for(std::size_t i=0;i<sizeof(hostile);++i)s.buffers[0].name[i]=hostile[i];
    s.buffers[0].texture.handle=0xA0020000;s.buffers[0].texture.texture_2d=0xA0030000;
    s.buffers[0].texture.selected_interface=0xA0030000;
    s.buffers[0].texture.requested_width=2560;s.buffers[0].texture.requested_height=1440;
    s.buffers[0].texture.requested_depth=1;s.buffers[0].texture.depth=1;
    s.buffers[0].texture.width=2560;s.buffers[0].texture.height=1440;
    s.buffers[0].texture.inverse_width=1.0F/2560;s.buffers[0].texture.inverse_height=1.0F/1440;
    s.buffers[1]=s.buffers[0];s.buffers[1].index=1;s.buffers[1].entry+=0x64;
    s.buffers[1].linked_borrowed=1;
    s.renderer_state.pixel_samplers[15]=0xA0020000;
    s.renderer_state.vertex_samplers[0]=0xA0020000;
    CoJPostprocessObservationContext c;
    c.scope=1;c.frame=9;c.owner=0xA0040000;c.device=s.device;c.device_generation=2;
    c.probe_epoch=7;c.thread=3;c.eye=1;c.begin=true;
    const std::string_view mode=argc>1?argv[1]:"normal";
    if(mode=="mismatch")c.device+=4;
    if(mode=="overflow")s.buffer_count=s.max_buffers+1;
    if(mode=="invalid")s.valid=false;
    if(mode=="no_context")c.scope=0;
    if(mode=="zero_depth")for(auto& b:s.buffers) {
        b.texture.requested_depth=0;b.texture.depth=0;
        b.texture.inverse_depth=std::numeric_limits<float>::infinity();
    }
    std::cout<<FormatCoJPostprocessObservation(s,c)<<'\n';
}
