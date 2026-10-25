#pragma once
#include "backends/d3d9/render_resource_probe.hpp"
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
// Value-only RT0 call intents on a same-extent FP16 surface before it becomes
// the native copy source. No retained COM lifetime or material/depth admission.
struct HdrProducerWrites {
    std::uintptr_t surface=0,first_alpha_shader=0,last_alpha_shader=0;
    std::uint64_t draw_calls=0,alpha_draw_calls=0,alpha_blend_draw_calls=0,clear_calls=0;
    HdrAlphaBlendWitness first_alpha_blend{};
};
// Scope-local successful call intents, not proof of modified pixels, full
// coverage, depth semantics or an atomic lifetime. No retained native pointers.
struct HdrSourceWrites {
    std::uint64_t calls=0,alpha_calls=0;
    std::uintptr_t last_shader=0,source_surface=0;
    bool complete=true;
    std::uint64_t copy_calls=0;
    std::uintptr_t copy_source=0;
    std::uint32_t copy_source_format=0;
    std::uint32_t copy_source_width=0,copy_source_height=0;
    std::uint64_t copy_write_call=0;
    RECT copy_source_rect{},copy_destination_rect{};
    bool copy_rectangles_observed=false;
    std::array<HdrProducerWrites,4> producers{};
    bool producer_observation_complete=true;
    HdrProducerWrites copy_producer{};
    bool copy_producer_complete=false;
};
inline void ObserveHdrProducerWrites(IDirect3DDevice9* device,IDirect3DSurface9* target,
    unsigned width,unsigned height,bool draw,HdrSourceWrites& out) noexcept {
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
    const auto identity=reinterpret_cast<std::uintptr_t>(target);
    HdrProducerWrites* entry=nullptr;
    for(auto& value:out.producers)if(value.surface==identity){entry=&value;break;}
    if(!entry)for(auto& value:out.producers)if(!value.surface){entry=&value;value.surface=identity;break;}
    if(!entry){out.producer_observation_complete=false;return;}
    if(!draw){++entry->clear_calls;return;}
    ++entry->draw_calls;
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
inline void ObserveHdrSourceCopy(IDirect3DDevice9* device,std::uintptr_t identity,unsigned width,unsigned height,
    IDirect3DSurface9* source,const RECT* source_rect,IDirect3DSurface9* destination,const RECT* destination_rect,
    HRESULT result,HdrSourceWrites& out) noexcept {
    using Microsoft::WRL::ComPtr;
    if(FAILED(result)||!device||!identity)return;
    if(!source||!destination){out.complete=false;return;}
    ComPtr<IDirect3DTexture9> parent;
    const auto container=destination->GetContainer(IID_PPV_ARGS(&parent));
    if(container==E_NOINTERFACE)return;
    if(FAILED(container)||!parent){out.complete=false;return;}
    if(reinterpret_cast<std::uintptr_t>(parent.Get())!=identity)return;
    ComPtr<IDirect3DDevice9> source_device,destination_device;
    D3DSURFACE_DESC from{},to{};
    if(FAILED(source->GetDevice(&source_device))||FAILED(destination->GetDevice(&destination_device))||
        source_device.Get()!=device||destination_device.Get()!=device||
        FAILED(source->GetDesc(&from))||FAILED(destination->GetDesc(&to))||
        to.Width!=width||to.Height!=height||to.Format!=D3DFMT_A16B16G16R16F){out.complete=false;return;}
    ++out.calls;++out.alpha_calls;++out.copy_calls;out.last_shader=0;
    out.source_surface=reinterpret_cast<std::uintptr_t>(destination);
    out.copy_source=reinterpret_cast<std::uintptr_t>(source);
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
}
inline void ObserveHdrSourceWrites(IDirect3DDevice9* device,std::uintptr_t source,unsigned width,unsigned height,
    const RenderResourceEvent& event,HdrSourceWrites& out) noexcept {
    using Microsoft::WRL::ComPtr;
    using Op=RenderResourceOperation;
    if(!device||!source||FAILED(event.result))return;
    const bool draw=event.operation==Op::draw||event.operation==Op::draw_indexed||
        event.operation==Op::draw_up||event.operation==Op::draw_indexed_up;
    const bool clear=event.operation==Op::clear&&(event.selector&D3DCLEAR_TARGET);
    // Copy intents require the typed synchronous callback above; event
    // addresses alone never authorize COM access or count a second copy.
    if(!draw&&!clear)return;
    ComPtr<IDirect3DSurface9> target;
    if(FAILED(device->GetRenderTarget(0,&target))||!target){out.complete=false;out.producer_observation_complete=false;return;}
    ObserveHdrProducerWrites(device,target.Get(),width,height,draw,out);
    ComPtr<IDirect3DTexture9> parent;
    const auto container=target->GetContainer(IID_PPV_ARGS(&parent));
    if(container==E_NOINTERFACE)return; // Ordinary swapchain/plain surface.
    if(FAILED(container)||!parent){out.complete=false;return;}
    if(reinterpret_cast<std::uintptr_t>(parent.Get())!=source)return;
    D3DSURFACE_DESC desc{};
    if(FAILED(target->GetDesc(&desc))||desc.Width!=width||desc.Height!=height||
        desc.Format!=D3DFMT_A16B16G16R16F){out.complete=false;return;}
    out.source_surface=reinterpret_cast<std::uintptr_t>(target.Get());
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
