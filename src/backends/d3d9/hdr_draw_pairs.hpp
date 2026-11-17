#pragma once
#include "backends/d3d9/hdr_vertex_programs.hpp"
#include <iterator>

namespace cojvr::backends::d3d9 {

inline constexpr struct {
    D3DRENDERSTATETYPE type;
    const char* name;
} hdr_draw_pair_states[] = {
    {D3DRS_COLORWRITEENABLE, "colorWrite"},
    {D3DRS_COLORWRITEENABLE1, "secondaryColorWrite"},
    {D3DRS_ALPHATESTENABLE, "alphaTest"}, {D3DRS_ALPHAFUNC, "alphaFunc"}, {D3DRS_ALPHAREF, "alphaRef"},
    {D3DRS_ALPHABLENDENABLE, "alphaBlend"}, {D3DRS_SRCBLEND, "srcBlend"}, {D3DRS_DESTBLEND, "dstBlend"},
    {D3DRS_BLENDOP, "blendOp"}, {D3DRS_BLENDFACTOR, "blendFactor"},
    {D3DRS_SEPARATEALPHABLENDENABLE, "separateAlpha"}, {D3DRS_SRCBLENDALPHA, "srcBlendAlpha"},
    {D3DRS_DESTBLENDALPHA, "dstBlendAlpha"}, {D3DRS_BLENDOPALPHA, "blendOpAlpha"},
    {D3DRS_ZENABLE, "depthTest"}, {D3DRS_ZWRITEENABLE, "depthWrite"}, {D3DRS_ZFUNC, "depthFunc"},
    {D3DRS_DEPTHBIAS, "depthBiasBits"}, {D3DRS_SLOPESCALEDEPTHBIAS, "slopeBiasBits"},
    {D3DRS_STENCILENABLE, "stencilTest"}, {D3DRS_CULLMODE, "cullMode"},
    {D3DRS_SCISSORTESTENABLE, "scissorTest"}, {D3DRS_SRGBWRITEENABLE, "srgbWrite"}
};

struct HdrDrawPair {
    unsigned ordinal=0;
    HdrShaderProgram vertex{},pixel{};
    bool secondary_target_bound=false,depth_bound=false;
    std::array<DWORD,std::size(hdr_draw_pair_states)> states{};
};

// First eight draws and every sixteenth through 1024. Positions are per
// same-extent FP16 producer and account only for successful RT0 write intents.
struct HdrDrawPairs {
    static constexpr unsigned early=8,stride=16,limit=1024;
    std::array<std::shared_ptr<const HdrDrawPair>,early+limit/stride> samples{};
    unsigned observed=0,read_failures=0;
};

inline void ObserveHdrDrawPair(IDirect3DDevice9* device,IDirect3DPixelShader9* pixel,
    std::uint64_t ordinal,DWORD color_write,HdrDrawPairs& ledger) noexcept {
    using Microsoft::WRL::ComPtr;
    if(!ordinal||ordinal>HdrDrawPairs::limit||
       (ordinal>HdrDrawPairs::early&&ordinal%HdrDrawPairs::stride))return;
    HdrDrawPair pair;
    pair.ordinal=static_cast<unsigned>(ordinal);
    const auto slot=pair.ordinal<=HdrDrawPairs::early?pair.ordinal-1:
        HdrDrawPairs::early+pair.ordinal/HdrDrawPairs::stride-1;
    ComPtr<IDirect3DSurface9> secondary,depth;
    ComPtr<IDirect3DVertexShader9> vertex;
    const auto secondary_result=device->GetRenderTarget(1,&secondary);
    const auto depth_result=device->GetDepthStencilSurface(&depth);
    if((secondary_result!=D3DERR_NOTFOUND&&(FAILED(secondary_result)||!secondary))||
       (depth_result!=D3DERR_NOTFOUND&&(FAILED(depth_result)||!depth))||
       FAILED(device->GetVertexShader(&vertex))||
       !ReadHdrShaderProgramCode(vertex.Get(),pair.vertex)||
       !ReadHdrShaderProgramCode(pixel,pair.pixel)){
        ++ledger.read_failures;return;
    }
    pair.secondary_target_bound=secondary.Get()!=nullptr;
    pair.depth_bound=depth.Get()!=nullptr;
    pair.states[0]=color_write;
    for(unsigned i=1;i<pair.states.size();++i){
        if(FAILED(device->GetRenderState(hdr_draw_pair_states[i].type,&pair.states[i]))){
            ++ledger.read_failures;return;
        }
    }
    try{
        ledger.samples[slot]=std::make_shared<const HdrDrawPair>(std::move(pair));
        ++ledger.observed;
    }catch(...){++ledger.read_failures;}
}

inline void AppendHdrDrawPairs(std::ostream& out,const HdrDrawPairs& ledger){
    const auto append_program=[&](const HdrShaderProgram& p){
        out<<"{\"shader\":"<<p.shader<<",\"shaderBytes\":"<<p.shader_bytes<<",\"bytecode\":[";
        for(unsigned i=0;i<p.shader_bytes/sizeof(DWORD);++i)out<<(i?",":"")<<p.bytecode[i];
        out<<"]}";
    };
    out<<"{\"schema\":1,\"observed\":"<<ledger.observed<<",\"readFailures\":"<<ledger.read_failures
       <<",\"coverage\":\"first_8_then_every_16_through_1024\",\"samples\":[";
    bool comma=false;
    for(const auto& sample:ledger.samples){
        if(!sample)continue;
        out<<(comma?",":"")<<"{\"ordinal\":"<<sample->ordinal
           <<",\"secondaryTargetBound\":"<<(sample->secondary_target_bound?"true":"false")
           <<",\"depthBound\":"<<(sample->depth_bound?"true":"false")<<",\"vertex\":";
        append_program(sample->vertex);
        out<<",\"pixel\":";
        append_program(sample->pixel);
        out<<",\"states\":{";
        for(unsigned i=0;i<sample->states.size();++i)
            out<<(i?",":"")<<'"'<<hdr_draw_pair_states[i].name<<"\":"<<sample->states[i];
        out<<"}}";
        comma=true;
    }
    out<<"]}";
}

}
