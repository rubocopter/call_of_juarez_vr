#include "games/call_of_juarez/ppfx_resource_telemetry.hpp"
#include <algorithm>
#include <bit>
#include <sstream>

namespace cojvr::games::call_of_juarez {
namespace {
template<std::size_t N> void Name(std::ostream& out,const std::array<char,N>& name) {
    constexpr char digits[]="0123456789abcdef";
    out << '"';
    for(const unsigned char c:name) {
        if(!c)break;
        if(c=='"'||c=='\\')out << '\\' << static_cast<char>(c);
        else if(c<0x20||c>=0x7f)out << "\\u00" << digits[c>>4] << digits[c&15];
        else out << static_cast<char>(c);
    }
    out << '"';
}
void Texture(std::ostream& out,const CoJPostprocessTextureObservation& t) {
    out << "{\"handle\":" << t.handle << ",\"selectedInterface\":" << t.selected_interface
        << ",\"texture2d\":" << t.texture_2d
        << ",\"textureCube\":" << t.texture_cube << ",\"textureVolume\":" << t.texture_volume
        << ",\"usage\":" << t.usage << ",\"format\":" << t.format << ",\"flags\":" << t.flags
        << ",\"kind\":" << t.kind << ",\"requestedWidth\":" << t.requested_width
        << ",\"requestedHeight\":" << t.requested_height << ",\"requestedDepth\":" << t.requested_depth
        << ",\"width\":" << t.width << ",\"height\":" << t.height << ",\"depth\":" << t.depth
        << ",\"inverseSizeBits\":[" << std::bit_cast<std::uint32_t>(t.inverse_width) << ','
        << std::bit_cast<std::uint32_t>(t.inverse_height) << ','
        << std::bit_cast<std::uint32_t>(t.inverse_depth) << "]}";
}
}
std::string FormatCoJPostprocessObservation(const CoJPostprocessResourceSnapshot& s,
    const CoJPostprocessObservationContext& c) {
    const bool truncated=s.effect_count>s.max_effects||s.buffer_count>s.max_buffers;
    const bool context_valid=c.scope&&c.frame&&c.owner&&c.device&&c.device_generation&&
        c.probe_epoch&&c.thread&&c.eye<2&&c.route<3;
    const bool complete=s.valid&&s.status==CoJPostprocessObservationStatus::observed&&
        !truncated&&context_valid&&s.device==c.device;
    std::ostringstream out;out << std::boolalpha;
    out << "native_ppfx_resource_probe: {\"schema\":1,\"phase\":\"" << (c.begin?"begin":"end")
        << "\",\"coverage\":\"native_ppfx_structural_snapshot\",\"readOnly\":true,\"resizingAdmitted\":false"
        << ",\"scope\":" << c.scope << ",\"thread\":" << c.thread << ",\"frame\":" << c.frame
        << ",\"owner\":" << c.owner << ",\"device\":" << c.device
        << ",\"deviceGeneration\":" << c.device_generation << ",\"generation\":" << c.probe_epoch
        << ",\"eye\":" << c.eye << ",\"route\":" << c.route
        << ",\"bindingScopeComplete\":" << c.scope_complete << ",\"complete\":" << complete
        << ",\"truncated\":" << truncated << ",\"status\":" << static_cast<unsigned>(s.status)
        << ",\"engineBase\":" << s.engine_base << ",\"renderer\":" << s.renderer
        << ",\"manager\":" << s.manager << ",\"nativeDevice\":" << s.device
        << ",\"rendererVtable\":" << s.renderer_vtable << ",\"managerVtable\":" << s.manager_vtable
        << ",\"listSentinel\":" << s.list_sentinel << ",\"loadedEffectCount\":" << s.loaded_effect_count
        << ",\"globalBufferCount\":" << s.global_buffer_count
        << ",\"managerDirty\":" << static_cast<unsigned>(s.manager_dirty)
        << ",\"effectCount\":" << s.effect_count << ",\"bufferCount\":" << s.buffer_count;
    const auto& r=s.renderer_state;
    out << ",\"rendererState\":{\"logicalWidth\":" << r.logical_width
        << ",\"logicalHeight\":" << r.logical_height
        << ",\"viewportClampWidth\":" << r.viewport_clamp_width
        << ",\"viewportClampHeight\":" << r.viewport_clamp_height
        << ",\"currentTarget\":" << r.current_target
        << ",\"defaultColorSurface\":" << r.default_color_surface
        << ",\"defaultDepthSurface\":" << r.default_depth_surface
        << ",\"internalColorSurface\":" << r.internal_color_surface
        << ",\"internalDepthSurface\":" << r.internal_depth_surface
        << ",\"additionalDepthSurface\":" << r.additional_depth_surface
        << ",\"placeholderTexture\":" << r.placeholder_texture << ",\"pixelSamplerHandles\":[";
    for(std::size_t i=0;i<r.pixel_samplers.size();++i) {
        if(i)out << ',';out << r.pixel_samplers[i];
    }
    out << "],\"vertexSamplerHandles\":[";
    for(std::size_t i=0;i<r.vertex_samplers.size();++i) {
        if(i)out << ',';out << r.vertex_samplers[i];
    }
    out << "]},\"effects\":[";
    for(std::size_t i=0;i<std::min(s.effect_count,s.max_effects);++i) {
        if(i)out << ',';
        const auto& e=s.effects[i];
        out << "{\"node\":" << e.node << ",\"next\":" << e.next << ",\"previous\":" << e.previous
            << ",\"effect\":" << e.effect << ",\"vtable\":" << e.vtable
            << ",\"bufferCount\":" << e.buffer_count << ",\"name\":";
        Name(out,e.name);out << '}';
    }
    out << "],\"buffers\":[";
    for(std::size_t i=0;i<std::min(s.buffer_count,s.max_buffers);++i) {
        if(i)out << ',';
        const auto& b=s.buffers[i];
        out << "{\"container\":" << b.container << ",\"entry\":" << b.entry
            << ",\"index\":" << b.index << ",\"global\":" << b.global << ",\"name\":";
        Name(out,b.name);
        out << ",\"logicalWidth\":" << b.logical_width << ",\"logicalHeight\":" << b.logical_height
            << ",\"rgbBits\":" << b.rgb_bits << ",\"alphaBits\":" << b.alpha_bits
            << ",\"widthDivisor\":" << b.width_divisor << ",\"heightDivisor\":" << b.height_divisor
            << ",\"internalColor\":" << static_cast<unsigned>(b.internal_color)
            << ",\"depthAttachment\":" << static_cast<unsigned>(b.depth_attachment)
            << ",\"nativeBpp\":" << static_cast<unsigned>(b.native_bpp)
            << ",\"tempShareable\":" << static_cast<unsigned>(b.temp_shareable)
            << ",\"linkedBorrowed\":" << static_cast<unsigned>(b.linked_borrowed) << ",\"texture\":";
        Texture(out,b.texture);out << '}';
    }
    out << "]}";
    return out.str();
}
}
