#pragma once
#include <d3d9.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <memory>
#include <bit>
#include <ostream>
#include <cstdint>

namespace cojvr::backends::d3d9 {
// Immutable values, never retained native shader pointers. Constants describe
// only the first observed draw for this exact program, not all actor instances.
struct HdrVertexProgram {
    std::uintptr_t shader=0;
    UINT shader_bytes=0;
    std::array<DWORD,1024> bytecode{};
    bool constants_complete=false;
    std::array<float,1024> constants{};
};
template<class Shader>
bool ReadHdrVertexProgramCode(Shader* shader,HdrVertexProgram& out) noexcept {
    out={};out.shader=reinterpret_cast<std::uintptr_t>(shader);
    if(!shader)return true; // Explicit fixed-function draw.
    if(FAILED(shader->GetFunction(nullptr,&out.shader_bytes))||!out.shader_bytes||
        out.shader_bytes%sizeof(DWORD)||out.shader_bytes>sizeof(out.bytecode))return false;
    UINT size=out.shader_bytes;
    return SUCCEEDED(shader->GetFunction(out.bytecode.data(),&size))&&size==out.shader_bytes;
}
struct HdrVertexPrograms {
    static constexpr unsigned draw_budget=64,program_budget=4;
    std::uint64_t draws=0,unobserved_draws=0,read_failures=0;
    std::array<std::shared_ptr<const HdrVertexProgram>,program_budget> programs{};
    std::array<std::uint64_t,program_budget> draw_counts{};
};
inline void ObserveHdrVertexPrograms(IDirect3DDevice9* device,HdrVertexPrograms& ledger) noexcept {
    ++ledger.draws;
    if(ledger.draws>HdrVertexPrograms::draw_budget){++ledger.unobserved_draws;return;}
    Microsoft::WRL::ComPtr<IDirect3DVertexShader9> shader;
    HdrVertexProgram program;
    if(!device||FAILED(device->GetVertexShader(&shader))||!ReadHdrVertexProgramCode(shader.Get(),program)){
        ++ledger.read_failures;return;
    }
    unsigned slot=0;
    for(;slot<ledger.programs.size()&&ledger.programs[slot];++slot){
        const auto& prior=*ledger.programs[slot];
        if(prior.shader_bytes==program.shader_bytes&&std::equal(program.bytecode.begin(),
            program.bytecode.begin()+program.shader_bytes/sizeof(DWORD),prior.bytecode.begin())){
            ++ledger.draw_counts[slot];return;
        }
    }
    if(slot==ledger.programs.size()){++ledger.unobserved_draws;return;}
    if(shader)program.constants_complete=SUCCEEDED(device->GetVertexShaderConstantF(0,program.constants.data(),256));
    try{ledger.programs[slot]=std::make_shared<const HdrVertexProgram>(std::move(program));++ledger.draw_counts[slot];}
    catch(...){++ledger.read_failures;}
}
inline void AppendHdrVertexPrograms(std::ostream& out,const HdrVertexPrograms& ledger){
    out<<"{\"schema\":1,\"draws\":"<<ledger.draws<<",\"unobservedDraws\":"<<ledger.unobserved_draws
       <<",\"readFailures\":"<<ledger.read_failures
       <<",\"coverage\":\"first_64_draws_first_4_distinct_programs\",\"programs\":[";
    for(unsigned slot=0;slot<ledger.programs.size()&&ledger.programs[slot];++slot){
        const auto& p=*ledger.programs[slot];
        out<<(slot?",":"")<<"{\"shader\":"<<p.shader<<",\"shaderBytes\":"<<p.shader_bytes
           <<",\"draws\":"<<ledger.draw_counts[slot]<<",\"constantsComplete\":"<<(p.constants_complete?"true":"false")
           <<",\"bytecode\":[";
        for(unsigned i=0;i<p.shader_bytes/sizeof(DWORD);++i)out<<(i?",":"")<<p.bytecode[i];
        out<<"],\"constantBits\":[";
        if(p.constants_complete)for(unsigned i=0;i<p.constants.size();++i)
            out<<(i?",":"")<<std::bit_cast<DWORD>(p.constants[i]);
        out<<"]}";
    }
    out<<"]}";
}
}
