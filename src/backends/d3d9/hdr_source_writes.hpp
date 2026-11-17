#pragma once
#include "backends/d3d9/render_resource_probe.hpp"
#include "backends/d3d9/hdr_draw_pairs.hpp"
#include <algorithm>
#include <wrl/client.h>

namespace cojvr::backends::d3d9 {
// One first blend-enabled alpha draw per bounded producer/eye. Copy the
// currently owned shader, never revisit its logged address. Oversize/read
// failures retain unknown program semantics, without allocating or retrying.
struct HdrAlphaBlendWitness {
    bool observed=false,state_complete=false,separate_alpha=false,shader_complete=false;
    std::uintptr_t shader=0;
    DWORD source_blend=0,destination_blend=0,blend_operation=0,blend_factor=0;
    UINT shader_bytes=0;
    std::array<DWORD,1024> bytecode{};
};
inline HdrAlphaBlendWitness ReadHdrAlphaBlend(IDirect3DDevice9* device,IDirect3DPixelShader9* shader) noexcept {
    HdrAlphaBlendWitness out;out.observed=true;out.shader=reinterpret_cast<std::uintptr_t>(shader);
    DWORD separate=0;
    if(SUCCEEDED(device->GetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,&separate))) {
        out.separate_alpha=separate!=0;
        out.state_complete=SUCCEEDED(device->GetRenderState(separate?D3DRS_SRCBLENDALPHA:D3DRS_SRCBLEND,&out.source_blend))&&
            SUCCEEDED(device->GetRenderState(separate?D3DRS_DESTBLENDALPHA:D3DRS_DESTBLEND,&out.destination_blend))&&
            SUCCEEDED(device->GetRenderState(separate?D3DRS_BLENDOPALPHA:D3DRS_BLENDOP,&out.blend_operation))&&
            SUCCEEDED(device->GetRenderState(D3DRS_BLENDFACTOR,&out.blend_factor));
    }
    if(!shader){out.shader_complete=true;return out;} // Explicit fixed-function witness.
    if(FAILED(shader->GetFunction(nullptr,&out.shader_bytes))||!out.shader_bytes||
       out.shader_bytes%sizeof(DWORD)||out.shader_bytes>sizeof(out.bytecode))return out;
    UINT bytes=out.shader_bytes;
    out.shader_complete=SUCCEEDED(shader->GetFunction(out.bytecode.data(),&bytes))&&bytes==out.shader_bytes;
    return out;
}
struct HdrFirstColorClear {
    std::uint64_t draws_before=0;
    RenderClearRectangles rectangles=RenderClearRectangles::unknown;
    bool state_complete=false,full_viewport=false,scissor_enabled=false;
};
// Value-only RT0 call intents on a same-extent FP16 surface before it becomes
// the native copy source. No retained COM lifetime or material/depth admission.
struct HdrProducerWrites {
    std::uintptr_t surface=0,first_alpha_shader=0,last_alpha_shader=0;
    std::uint64_t draw_calls=0,alpha_draw_calls=0,alpha_blend_draw_calls=0,clear_calls=0;
    HdrAlphaBlendWitness first_alpha_blend{};
    HdrVertexPrograms vertex_programs{};
    HdrDrawPairs draw_pairs{};
    HdrFirstColorClear first_color_clear{};
    std::uintptr_t ingress_surface=0;
    std::uint64_t incoming_copies=0;
    bool incoming_copy_observation_complete=false;
    std::uint64_t direct_writes=0;
    bool direct_write_observation_complete=false;
    bool was_secondary_target=false,secondary_target_observation_complete=false;

    // Shape of the recorded clear only. Does not establish write coverage.
    bool HasLeadingFullClear() const noexcept {
        const auto& clear=first_color_clear;
        return clear_calls==1&&!clear.draws_before&&clear.rectangles==RenderClearRectangles::viewport&&
            clear.state_complete&&clear.full_viewport&&!clear.scissor_enabled;
    }
};
// Scope-local successful call intents, not proof of modified pixels, full
// coverage, depth semantics or an atomic lifetime. No retained native pointers.
struct HdrSourceWrites {
    std::uint64_t calls=0,alpha_calls=0;
    std::uintptr_t last_shader=0,source_surface=0;
    bool complete=true;
    std::uintptr_t source_texture_address=0,source_texture_identity=0;
    std::uint64_t copy_calls=0;
    std::uintptr_t copy_source=0;
    std::uint32_t copy_source_format=0;
    std::uint32_t copy_source_width=0,copy_source_height=0;
    std::uint64_t copy_write_call=0;
    RECT copy_source_rect{},copy_destination_rect{};
    bool copy_rectangles_observed=false;
    std::array<HdrProducerWrites,4> producers{};
    bool producer_observation_complete=true;
    // A typed write can precede any RT0 bind or target a different surface.
    // Keep these addresses separate from the RT0 producer budget.
    struct CopyDestination {std::uintptr_t surface=0;std::uint64_t calls=0,direct_writes=0;};
    std::array<CopyDestination,8> copy_destinations{};
    bool incoming_copy_observation_complete=true;
    bool direct_write_observation_complete=true;
    std::array<std::uintptr_t,3> secondary_slots{};
    std::array<bool,3> secondary_slots_known{};
    std::array<std::uintptr_t,8> seen_secondary_targets{};
    bool secondary_target_observation_complete=false;
    HdrProducerWrites copy_producer{};
    bool copy_producer_complete=false;
};
inline void ObserveHdrSecondaryBinding(DWORD index,std::uintptr_t target,HdrSourceWrites& out) noexcept {
    if(!index)return;
    if(index>out.secondary_slots.size()){
        out.secondary_target_observation_complete=false;
        out.secondary_slots_known.fill(false);
        return;
    }
    out.secondary_slots[index-1]=target;
    out.secondary_slots_known[index-1]=true;
    if(!target)return;
    for(auto& seen:out.seen_secondary_targets){
        if(seen&&seen!=target)continue;
        seen=target;
        return;
    }
    out.secondary_target_observation_complete=false;
}
inline void ObserveHdrInitialSecondaryTargets(IDirect3DDevice9* device,HdrSourceWrites& out) noexcept {
    D3DCAPS9 caps{};
    if(!device||FAILED(device->GetDeviceCaps(&caps))||!caps.NumSimultaneousRTs||
        caps.NumSimultaneousRTs>out.secondary_slots.size()+1)return;
    out.secondary_target_observation_complete=true;
    out.secondary_slots_known.fill(true);
    for(DWORD index=1;index<caps.NumSimultaneousRTs;++index){
        Microsoft::WRL::ComPtr<IDirect3DSurface9> target;
        const auto result=device->GetRenderTarget(index,&target);
        if(result==D3DERR_NOTFOUND&&!target)continue;
        if(FAILED(result)||!target){
            out.secondary_target_observation_complete=false;
            out.secondary_slots_known[index-1]=false;
            continue;
        }
        const auto identity=RenderResourceIdentity(target.Get());
        if(!identity){
            out.secondary_target_observation_complete=false;
            out.secondary_slots_known[index-1]=false;
            continue;
        }
        ObserveHdrSecondaryBinding(index,identity,out);
    }
}
inline void ObserveHdrDestinationWrite(std::uintptr_t surface,const D3DSURFACE_DESC& desc,
    unsigned width,unsigned height,bool direct,HdrSourceWrites& out) noexcept {
    if(desc.Width!=width||desc.Height!=height||desc.Format!=D3DFMT_A16B16G16R16F)return;
    if(!surface){
        (direct?out.direct_write_observation_complete:out.incoming_copy_observation_complete)=false;
        return;
    }
    for(auto& entry:out.copy_destinations){
        if(entry.surface&&entry.surface!=surface)continue;
        entry.surface=surface;
        ++(direct?entry.direct_writes:entry.calls);
        return;
    }
    (direct?out.direct_write_observation_complete:out.incoming_copy_observation_complete)=false;
}
inline void InvalidateHdrDirectWriteObservation(HdrSourceWrites& out) noexcept {
    out.direct_write_observation_complete=false;
    out.complete=false;
}
inline void ObserveHdrDirectSurfaceWrite(IDirect3DSurface9* destination,const D3DSURFACE_DESC& desc,
    unsigned width,unsigned height,HdrSourceWrites& out,bool alpha=true) noexcept {
    if(desc.Width!=width||desc.Height!=height||desc.Format!=D3DFMT_A16B16G16R16F)return;
    const auto surface=RenderResourceIdentity(destination);
    if(!surface){InvalidateHdrDirectWriteObservation(out);return;}
    if(surface==out.source_surface){
        ++out.calls;
        if(alpha)++out.alpha_calls;
        out.last_shader=0;
    }
    ObserveHdrDestinationWrite(surface,desc,width,height,true,out);
}
inline void ObserveHdrSecondarySourceWrite(std::uintptr_t rt0_identity, bool clear, HdrSourceWrites& out) noexcept {
    if(!out.source_surface)return;
    if(!rt0_identity){out.complete=false;return;}
    if(out.source_surface==rt0_identity)return;
    if(std::ranges::find(out.secondary_slots_known,false)!=out.secondary_slots_known.end())
        out.complete=false;
    if(std::ranges::find(out.secondary_slots,out.source_surface)!=out.secondary_slots.end()){
        ++out.calls;
        if(clear)++out.alpha_calls;
        out.last_shader=0;
    }
}
inline void ObserveHdrPatchWrite(IDirect3DDevice9* device,HRESULT result,unsigned width,unsigned height,
    HdrSourceWrites& out) noexcept {
    if(FAILED(result))return;
    Microsoft::WRL::ComPtr<IDirect3DSurface9> rt0;
    D3DSURFACE_DESC desc{};
    if(!device||FAILED(device->GetRenderTarget(0,&rt0))||!rt0||FAILED(rt0->GetDesc(&desc))){
        InvalidateHdrDirectWriteObservation(out);
        return;
    }
    ObserveHdrSecondarySourceWrite(RenderResourceIdentity(rt0.Get()),false,out);
    ObserveHdrDirectSurfaceWrite(rt0.Get(),desc,width,height,out,false);
}
template<class Texture,class ReadSurface>
inline void ObserveHdrTextureWrite(Texture* texture, unsigned faces, ReadSurface read_surface,
    unsigned width,unsigned height,HdrSourceWrites& out) noexcept {
    const auto levels=texture->GetLevelCount();
    if(!levels||levels>16){InvalidateHdrDirectWriteObservation(out);return;}
    for(UINT level=0;level<levels;++level){
        D3DSURFACE_DESC desc{};
        if(FAILED(texture->GetLevelDesc(level,&desc))){InvalidateHdrDirectWriteObservation(out);return;}
        if(desc.Format!=D3DFMT_A16B16G16R16F||desc.Width<width||desc.Height<height)return;
        if(desc.Width!=width||desc.Height!=height)continue;
        for(unsigned face=0;face<faces;++face){
            Microsoft::WRL::ComPtr<IDirect3DSurface9> surface;
            if(FAILED(read_surface(level,face,surface.GetAddressOf()))||!surface){
                InvalidateHdrDirectWriteObservation(out);
                return;
            }
            ObserveHdrDirectSurfaceWrite(surface.Get(),desc,width,height,out);
        }
        return;
    }
}
inline void ObserveHdrDeviceWrite(ResourceWriteKind kind,IDirect3DSurface9* surface,IDirect3DBaseTexture9* texture,
    HRESULT result,unsigned width,unsigned height,HdrSourceWrites& out) noexcept {
    if(FAILED(result))return;
    if(kind!=ResourceWriteKind::update_texture){
        D3DSURFACE_DESC desc{};
        if(!surface||FAILED(surface->GetDesc(&desc))){InvalidateHdrDirectWriteObservation(out);return;}
        ObserveHdrDirectSurfaceWrite(surface,desc,width,height,out);
        return;
    }
    if(!texture){InvalidateHdrDirectWriteObservation(out);return;}
    switch(texture->GetType()){
        case D3DRTYPE_TEXTURE: {
            auto* destination=static_cast<IDirect3DTexture9*>(texture);
            ObserveHdrTextureWrite(destination,1,[&](UINT level,unsigned,IDirect3DSurface9** target){
                return destination->GetSurfaceLevel(level,target);
            },width,height,out);
            break;
        }
        case D3DRTYPE_CUBETEXTURE: {
            auto* destination=static_cast<IDirect3DCubeTexture9*>(texture);
            ObserveHdrTextureWrite(destination,6,[&](UINT level,unsigned face,IDirect3DSurface9** target){
                return destination->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(face),level,target);
            },width,height,out);
            break;
        }
        case D3DRTYPE_VOLUMETEXTURE:
            break; // Volume levels do not expose IDirect3DSurface9.
        default:
            InvalidateHdrDirectWriteObservation(out);
            break;
    }
}
inline void ObserveHdrProducerWrites(IDirect3DDevice9* device,IDirect3DSurface9* target,std::uintptr_t identity,
    unsigned width,unsigned height,bool draw,const RenderResourceEvent& event,HdrSourceWrites& out) noexcept {
    D3DSURFACE_DESC desc{};
    if(FAILED(target->GetDesc(&desc))){out.producer_observation_complete=false;return;}
    if(desc.Width!=width||desc.Height!=height||desc.Format!=D3DFMT_A16B16G16R16F)return;
    DWORD mask=0,blend=0;
    Microsoft::WRL::ComPtr<IDirect3DPixelShader9> shader;
    if(draw) {
        if(FAILED(device->GetRenderState(D3DRS_COLORWRITEENABLE,&mask))||
           FAILED(device->GetPixelShader(&shader))||
           ((mask&D3DCOLORWRITEENABLE_ALPHA)&&FAILED(device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend)))) {
            out.producer_observation_complete=false;return;
        }
        if(!mask)return;
    }
    HdrProducerWrites* entry=nullptr;
    for(auto& value:out.producers)if(value.surface==identity){entry=&value;break;}
    if(!entry)for(auto& value:out.producers)if(!value.surface){entry=&value;value.surface=identity;break;}
    if(!entry){out.producer_observation_complete=false;return;}
    if(!draw){
        if(++entry->clear_calls!=1)return;
        auto& clear=entry->first_color_clear;
        clear.draws_before=entry->draw_calls;
        clear.rectangles=event.clear_rectangles;
        if(clear.rectangles!=RenderClearRectangles::viewport)return;
        D3DVIEWPORT9 viewport{};
        DWORD scissor=0;
        clear.state_complete=SUCCEEDED(device->GetViewport(&viewport))&&
            SUCCEEDED(device->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor));
        if(clear.state_complete){
            clear.full_viewport=viewport.X==0&&viewport.Y==0&&viewport.Width==width&&viewport.Height==height;
            clear.scissor_enabled=scissor!=FALSE;
        }
        return;
    }
    ++entry->draw_calls;
    ObserveHdrVertexPrograms(device,entry->vertex_programs);
    ObserveHdrDrawPair(device,shader.Get(),entry->draw_calls,mask,entry->draw_pairs);
    if(mask&D3DCOLORWRITEENABLE_ALPHA) {
        if(!entry->alpha_draw_calls)entry->first_alpha_shader=reinterpret_cast<std::uintptr_t>(shader.Get());
        ++entry->alpha_draw_calls;
        if(blend) {
            ++entry->alpha_blend_draw_calls;
            if(!entry->first_alpha_blend.observed)entry->first_alpha_blend=ReadHdrAlphaBlend(device,shader.Get());
        }
        entry->last_alpha_shader=reinterpret_cast<std::uintptr_t>(shader.Get());
    }
}
// Observation readiness only. The ledger resets at every eye scope. A complete
// equal-extent native FP16 copy must be the latest observed source write; a
// partial/scaled copy, later draw/clear, uncertainty or eye reset withdraws it.
// This does not prove source pixel/eye semantics or qualify a temporal input.
inline bool HasCurrentEyeHdrCopy(const HdrSourceWrites& value,unsigned width,unsigned height) noexcept {
    const auto full=[&](const RECT& rect) noexcept {
        return rect.left==0&&rect.top==0&&rect.right==LONG(width)&&rect.bottom==LONG(height);
    };
    return width&&height&&width<=8192&&height<=8192&&value.complete&&
        value.source_surface&&value.copy_source&&value.source_surface!=value.copy_source&&
        value.copy_calls&&value.copy_write_call&&value.copy_write_call==value.calls&&
        value.copy_rectangles_observed&&value.copy_source_format==D3DFMT_A16B16G16R16F&&
        value.copy_source_width==width&&value.copy_source_height==height&&
        full(value.copy_source_rect)&&full(value.copy_destination_rect);
}
inline bool IsHdrSourceTexture(IDirect3DTexture9* parent,std::uintptr_t candidate,
    std::uintptr_t surface,HdrSourceWrites& out) noexcept {
    const auto identity=RenderResourceIdentity(parent);
    if(!identity){out.complete=false;return false;}
    if(reinterpret_cast<std::uintptr_t>(parent)==candidate){
        if(out.source_texture_identity&&out.source_texture_identity!=identity){out.complete=false;return false;}
        out.source_texture_address=candidate;
        out.source_texture_identity=identity;
        return true;
    }
    if(out.source_texture_identity==identity)return true;
    // The exact-game candidate is a borrowed address. Learn its canonical
    // identity only from a live container, and fail closed on contradiction.
    if(out.source_surface&&surface==out.source_surface)out.complete=false;
    return false;
}
inline void ObserveHdrSourceCopy(IDirect3DDevice9* device,std::uintptr_t identity,unsigned width,unsigned height,
    IDirect3DSurface9* source,const RECT* source_rect,IDirect3DSurface9* destination,const RECT* destination_rect,
    HRESULT result,HdrSourceWrites& out) noexcept {
    using Microsoft::WRL::ComPtr;
    if(FAILED(result)||!device||!identity||
       (out.source_texture_address&&out.source_texture_address!=identity))return;
    if(!source||!destination){out.complete=false;out.incoming_copy_observation_complete=false;return;}
    D3DSURFACE_DESC to{};
    const bool destination_described=SUCCEEDED(destination->GetDesc(&to));
    const auto destination_identity=RenderResourceIdentity(destination);
    if(!destination_identity){out.complete=false;out.incoming_copy_observation_complete=false;return;}
    if(destination_described)ObserveHdrDestinationWrite(destination_identity,to,width,height,false,out);
    else out.incoming_copy_observation_complete=false;
    ComPtr<IDirect3DTexture9> parent;
    const auto container=destination->GetContainer(IID_PPV_ARGS(&parent));
    if(container==E_NOINTERFACE){if(destination_identity==out.source_surface)out.complete=false;return;}
    if(FAILED(container)||!parent){out.complete=false;return;}
    if(!IsHdrSourceTexture(parent.Get(),identity,destination_identity,out))return;
    ComPtr<IDirect3DDevice9> source_device,destination_device;
    D3DSURFACE_DESC from{};
    if(FAILED(source->GetDevice(&source_device))||FAILED(destination->GetDevice(&destination_device))||
        source_device.Get()!=device||destination_device.Get()!=device||
        FAILED(source->GetDesc(&from))||!destination_described||
        to.Width!=width||to.Height!=height||to.Format!=D3DFMT_A16B16G16R16F){out.complete=false;return;}
    const auto source_identity=RenderResourceIdentity(source);
    if(!source_identity){out.complete=false;return;}
    ++out.calls;++out.alpha_calls;++out.copy_calls;out.last_shader=0;
    out.source_surface=destination_identity;
    out.copy_source=source_identity;
    out.copy_source_format=static_cast<std::uint32_t>(from.Format);
    out.copy_source_width=from.Width;out.copy_source_height=from.Height;
    out.copy_write_call=out.calls;
    // These are still live native-call arguments, not decoded target addresses.
    out.copy_source_rect=source_rect?*source_rect:RECT{0,0,LONG(from.Width),LONG(from.Height)};
    out.copy_destination_rect=destination_rect?*destination_rect:RECT{0,0,LONG(to.Width),LONG(to.Height)};
    out.copy_rectangles_observed=true;
    // Freeze the producer observation at the copy, not at a later consumer.
    // Missing/overflowed/read-failed observations stay explicitly incomplete.
    out.copy_producer={};out.copy_producer_complete=false;
    for(const auto& producer:out.producers)if(producer.surface==out.copy_source) {
        out.copy_producer=producer;out.copy_producer_complete=out.producer_observation_complete;break;
    }
    if(from.Width==width&&from.Height==height&&from.Format==D3DFMT_A16B16G16R16F){
        out.copy_producer.ingress_surface=out.copy_source;
        out.copy_producer.incoming_copy_observation_complete=out.incoming_copy_observation_complete;
        out.copy_producer.direct_write_observation_complete=out.direct_write_observation_complete;
        out.copy_producer.secondary_target_observation_complete=out.secondary_target_observation_complete;
        out.copy_producer.was_secondary_target=
            std::ranges::find(out.seen_secondary_targets,out.copy_source)!=out.seen_secondary_targets.end();
        if(out.copy_producer.was_secondary_target)out.copy_producer.surface=out.copy_source;
        for(const auto& entry:out.copy_destinations)if(entry.surface==out.copy_source){
            out.copy_producer.surface=entry.surface;
            out.copy_producer.incoming_copies=entry.calls;
            out.copy_producer.direct_writes=entry.direct_writes;
            break;
        }
    }
}
inline void ObserveHdrSourceWrites(IDirect3DDevice9* device,std::uintptr_t source,unsigned width,unsigned height,
    const RenderResourceEvent& event,HdrSourceWrites& out) noexcept {
    using Microsoft::WRL::ComPtr;
    using Op=RenderResourceOperation;
    if(!device||!source||FAILED(event.result)||
       (out.source_texture_address&&out.source_texture_address!=source))return;
    if(event.operation==Op::set_target){
        if(!event.selector)return;
        if(!event.destination||event.selector>out.secondary_slots.size()){
            ObserveHdrSecondaryBinding(event.selector,0,out);
            return;
        }
        ComPtr<IDirect3DSurface9> target;
        const auto read=device->GetRenderTarget(event.selector,&target);
        const auto identity=SUCCEEDED(read)?RenderResourceIdentity(target.Get()):0;
        if(!identity){
            out.secondary_target_observation_complete=false;
            out.secondary_slots_known[event.selector-1]=false;
            return;
        }
        ObserveHdrSecondaryBinding(event.selector,identity,out);
        return;
    }
    const bool draw=event.operation==Op::draw||event.operation==Op::draw_indexed||
        event.operation==Op::draw_up||event.operation==Op::draw_indexed_up;
    const bool clear=event.operation==Op::clear&&(event.selector&D3DCLEAR_TARGET);
    // Copy intents require the typed synchronous callback above; event
    // addresses alone never authorize COM access or count a second copy.
    if(!draw&&!clear)return;
    ComPtr<IDirect3DSurface9> target;
    if(FAILED(device->GetRenderTarget(0,&target))||!target){out.complete=false;out.producer_observation_complete=false;return;}
    // A known secondary write advances the same source ordinal as a direct
    // write. A later full copy can restore currency; an unknown binding
    // instead leaves the write observation incomplete.
    const auto target_identity=RenderResourceIdentity(target.Get());
    if(!target_identity){out.complete=false;out.producer_observation_complete=false;return;}
    ObserveHdrSecondarySourceWrite(target_identity,clear,out);
    ObserveHdrProducerWrites(device,target.Get(),target_identity,width,height,draw,event,out);
    ComPtr<IDirect3DTexture9> parent;
    const auto container=target->GetContainer(IID_PPV_ARGS(&parent));
    if(container==E_NOINTERFACE){if(target_identity==out.source_surface)out.complete=false;return;}
    if(FAILED(container)||!parent){out.complete=false;return;}
    if(!IsHdrSourceTexture(parent.Get(),source,target_identity,out))return;
    D3DSURFACE_DESC desc{};
    if(FAILED(target->GetDesc(&desc))||desc.Width!=width||desc.Height!=height||
        desc.Format!=D3DFMT_A16B16G16R16F){out.complete=false;return;}
    out.source_surface=target_identity;
    DWORD mask=D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_ALPHA;
    ComPtr<IDirect3DPixelShader9> shader;
    if(draw){
        if(FAILED(device->GetRenderState(D3DRS_COLORWRITEENABLE,&mask))){out.complete=false;return;}
        if(FAILED(device->GetPixelShader(&shader)))out.complete=false;
    }
    if(!mask)return;
    ++out.calls;if(mask&D3DCOLORWRITEENABLE_ALPHA)++out.alpha_calls;
    out.last_shader=reinterpret_cast<std::uintptr_t>(shader.Get());
}
}
