#include "backends/d3d9/hdr_draw_pairs.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "backends/d3d9/factory_vtable_hook.hpp"
#include <d3dcompiler.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <fstream>
using Microsoft::WRL::ComPtr;
using namespace cojvr::backends::d3d9;
void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct FaultProgram {
    UINT size=20;unsigned reads=0;bool fail_query=false,fail_read=false,change_size=false;
    HRESULT GetFunction(void* destination,UINT* bytes){
        if(!destination){*bytes=size;return fail_query?E_FAIL:S_OK;}
        ++reads;if(fail_read)return E_FAIL;
        std::memset(destination,0,size);*bytes=change_size?size-4:size;return S_OK;
    }
};
int main(int argc,char** argv)try{
    Check(argc<=2,"unexpected arguments");
    for(unsigned failure=0;failure<6;++failure){
        FaultProgram shader;
        if(failure==0)shader.size=8192;
        if(failure==1)shader.size=19;
        if(failure==2)shader.size=0;
        if(failure==3)shader.fail_query=true;
        if(failure==4)shader.fail_read=true;
        if(failure==5)shader.change_size=true;
        HdrVertexProgram program;
        Check(!ReadHdrShaderProgramCode(&shader,program),"invalid program admitted");
        if(failure<4)Check(shader.reads==0,"invalid program size triggered bytecode read");
    }
    ComPtr<IDirect3D9Ex> api;ComPtr<IDirect3DDevice9> device;
    auto create=SystemDirect3DCreate9Ex();D3DPRESENT_PARAMETERS pp{};
    pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=GetDesktopWindow();
    pp.BackBufferWidth=pp.BackBufferHeight=64;
    if(!create||FAILED(create(D3D_SDK_VERSION,&api))){
        std::cout<<"SKIP: D3D9Ex HAL unavailable\n";return 77;
    }
    constexpr DWORD requested=D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_PUREDEVICE|D3DCREATE_FPU_PRESERVE;
    Check(InstallFactoryVtableHook(api.Get(),{.prefer_ex_device=true,.require_state_queries=true}),"diagnostic factory policy");
    const auto created=api->CreateDevice(0,D3DDEVTYPE_HAL,pp.hDeviceWindow,requested,&pp,&device);
    Check(RestoreFactoryVtableHook(api.Get()),"restore fixture factory");
    if(FAILED(created)){std::cout<<"SKIP: D3D9Ex HAL unavailable\n";return 77;}
    D3DDEVICE_CREATION_PARAMETERS actual{};
    Check(SUCCEEDED(device->GetCreationParameters(&actual))&&actual.BehaviorFlags==(requested&~D3DCREATE_PUREDEVICE),
        "diagnostic device did not preserve flags while enabling constant queries");
    HdrVertexPrograms ledger;
    const float constants[]={1,2,3,4};
    Check(SUCCEEDED(device->SetVertexShaderConstantF(0,constants,1)),"set sentinel constants");
    std::array<ComPtr<IDirect3DVertexShader9>,5> shaders;
    for(unsigned i=0;i<shaders.size();++i){
        std::string source="float4 k:register(c0);float4 main(float4 p:POSITION):POSITION{return p+k*"+
            std::to_string(i+1)+";}";
        ComPtr<ID3DBlob> code;
        Check(SUCCEEDED(D3DCompile(source.data(),source.size(),nullptr,nullptr,nullptr,"main","vs_3_0",0,0,&code,nullptr)),"compile VS");
        Check(SUCCEEDED(device->CreateVertexShader(static_cast<const DWORD*>(code->GetBufferPointer()),&shaders[i])),"create VS");
        Check(SUCCEEDED(device->SetVertexShader(shaders[i].Get())),"bind VS");
        ObserveHdrVertexPrograms(device.Get(),ledger);
    }
    Check(ledger.programs[0]&&ledger.programs[3]&&ledger.unobserved_draws==1&&ledger.read_failures==0,
        "distinct-program budget lost or unbounded");
    Check(ledger.programs[0]->constants_complete&&ledger.programs[0]->constants[0]==1&&ledger.programs[0]->constants[3]==4,
        "bound native constants not copied");
    const auto frozen=ledger;
    const DWORD pixel_code[]={0xffff0200,0x02000001,0x800f0800,0xa0e40000,0x0000ffff};
    ComPtr<IDirect3DPixelShader9> pixel;
    Check(SUCCEEDED(device->CreatePixelShader(pixel_code,&pixel))&&
        SUCCEEDED(device->SetPixelShader(pixel.Get())),"bind draw-pair PS");
    HdrDrawPairs draw_pairs;
    for(unsigned ordinal=1;ordinal<=1040;++ordinal)
        ObserveHdrDrawPair(device.Get(),pixel.Get(),ordinal,D3DCOLORWRITEENABLE_RED,draw_pairs);
    Check(draw_pairs.observed==72&&!draw_pairs.read_failures&&draw_pairs.samples[0]->ordinal==1&&
        draw_pairs.samples[8]->ordinal==16&&draw_pairs.samples[12]->ordinal==80&&
        draw_pairs.samples.back()->ordinal==1024,"draw-pair position budget lost or unbounded");
    if(argc==2){
        std::ofstream output(argv[1]);AppendHdrVertexPrograms(output,frozen);Check(bool(output),"write fixture JSON");
        HdrVertexPrograms maximum;maximum.draws=4;
        for(unsigned i=0;i<4;++i){
            HdrVertexProgram p;p.shader=i+1;p.shader_bytes=4096;p.bytecode.fill(0xffffffff);
            p.bytecode[0]=0xfffe0300;p.bytecode[1]-=i;p.bytecode.back()=0xffff;
            p.constants_complete=true;p.constants.fill(std::bit_cast<float>(DWORD{0xffffffff}));
            maximum.programs[i]=std::make_shared<const HdrVertexProgram>(p);maximum.draw_counts[i]=1;
        }
        std::ofstream large(std::string(argv[1])+".max.json");AppendHdrVertexPrograms(large,maximum);
        Check(bool(large),"write maximum fixture JSON");
        std::ofstream pairs(std::string(argv[1])+".pairs.json");AppendHdrDrawPairs(pairs,draw_pairs);
        Check(bool(pairs),"write draw-pair fixture JSON");
    }
    const float changed[]={5,6,7,8};device->SetVertexShaderConstantF(0,changed,1);
    device->SetVertexShader(shaders[0].Get());ObserveHdrVertexPrograms(device.Get(),ledger);
    Check(ledger.draw_counts[0]==2&&frozen.draw_counts[0]==1&&frozen.programs[0]->constants[0]==1,
        "producer snapshot aliased changed constants or counters");
    for(unsigned i=0;i<100;++i)ObserveHdrVertexPrograms(device.Get(),ledger);
    Check(ledger.draws==106&&ledger.unobserved_draws==43,"getter budget did not account for omitted draws");
    float after[4]{};ComPtr<IDirect3DVertexShader9> bound;
    device->GetVertexShaderConstantF(0,after,1);device->GetVertexShader(&bound);
    Check(after[0]==5&&after[3]==8&&bound.Get()==shaders[0].Get(),"observer mutated native state");
    ledger={};device->SetVertexShader(nullptr);ObserveHdrVertexPrograms(device.Get(),ledger);
    Check(ledger.programs[0]&&!ledger.programs[0]->shader&&ledger.programs[0]->shader_bytes==0,
        "fixed function confused with unavailable shader");
    std::cout<<"Bounded vertex programs/constants, frozen snapshots, faults and native state preservation passed. No material qualification.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
