// Original isolated NGX host. NVIDIA's SDK is an external build dependency;
// no SDK/game source or runtime is embedded in the repository.
#include "backends/openvr/dlss_protocol.hpp"
#include "backends/openvr/dlss_pipe.hpp"
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_helpers.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

using Microsoft::WRL::ComPtr;
using namespace cojvr::backends::openvr::dlss;
namespace {
constexpr std::array<DXGI_FORMAT,slots> formats{DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R8_UNORM,DXGI_FORMAT_R16G16B16A16_FLOAT};
struct Eye {
    Request build{};
    std::array<ComPtr<ID3D11Texture2D>,slots> textures;
    ComPtr<ID3D11Fence> incoming,outgoing;
    NVSDK_NGX_Handle* feature=nullptr;
    std::uint64_t previous=0;
    ~Eye(){if(feature)NVSDK_NGX_D3D11_ReleaseFeature(feature);}
};
struct Host {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11Device5> device5;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11DeviceContext4> context4;
    NVSDK_NGX_Parameter* parameters=nullptr;
    bool initialized=false;
    std::array<std::unique_ptr<Eye>,2> eyes;
    std::uint32_t ngx=0;
    ~Host(){
        for(auto& eye:eyes)eye.reset();
        if(parameters)NVSDK_NGX_D3D11_DestroyParameters(parameters);
        if(initialized)NVSDK_NGX_D3D11_Shutdown1(device.Get());
    }
    bool Ngx(NVSDK_NGX_Result result){ngx=static_cast<std::uint32_t>(result);return NVSDK_NGX_SUCCEED(result);}
    bool Init(const Request& request) {
        ComPtr<IDXGIFactory1> factory;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return false;
        ComPtr<IDXGIAdapter1> chosen;
        for(UINT i=0;;++i) {
            ComPtr<IDXGIAdapter1> adapter;if(factory->EnumAdapters1(i,&adapter)==DXGI_ERROR_NOT_FOUND)break;
            if(!adapter)return false;DXGI_ADAPTER_DESC1 desc{};if(FAILED(adapter->GetDesc1(&desc)))return false;
            if(desc.AdapterLuid.LowPart==request.adapter_low&&desc.AdapterLuid.HighPart==request.adapter_high) {
                chosen=adapter;break;
            }
        }
        if(!chosen)return false;
        const D3D_FEATURE_LEVEL feature=D3D_FEATURE_LEVEL_11_0;
        if(FAILED(D3D11CreateDevice(chosen.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            &feature,1,D3D11_SDK_VERSION,&device,nullptr,&context))||FAILED(device.As(&device5))||FAILED(context.As(&context4)))return false;
        wchar_t executable[32768]{};const auto length=GetModuleFileNameW(nullptr,executable,32768);
        if(!length||length>=32768)return false;
        const auto directory=std::filesystem::path(executable).parent_path();
        if(!std::filesystem::is_regular_file(directory/L"nvngx_dlss.dll"))return false;
        const auto logs=directory/L"ngx-local";std::error_code error;std::filesystem::create_directories(logs,error);if(error)return false;
        const wchar_t* paths[]{directory.c_str()};NVSDK_NGX_FeatureCommonInfo common{};
        common.PathListInfo={paths,1};
        // Stable project identity for this original experimental implementation.
        if(!Ngx(NVSDK_NGX_D3D11_Init_with_ProjectID("db7885ba-d08b-4d38-89db-a55a6a598ad4",
            NVSDK_NGX_ENGINE_TYPE_CUSTOM,"0.1.0",logs.c_str(),device.Get(),&common)))return false;
        initialized=true;
        NVSDK_NGX_Parameter* capabilities=nullptr;
        if(!Ngx(NVSDK_NGX_D3D11_GetCapabilityParameters(&capabilities)))return false;
        int available=0;const bool supported=Ngx(capabilities->Get(NVSDK_NGX_Parameter_SuperSampling_Available,&available))&&available!=0;
        NVSDK_NGX_D3D11_DestroyParameters(capabilities);
        return supported&&Ngx(NVSDK_NGX_D3D11_AllocateParameters(&parameters));
    }
    bool Wait(ID3D11Fence* fence,std::uint64_t value) {
        const auto end=GetTickCount64()+2000;
        while(GetTickCount64()<end) {
            const auto done=fence->GetCompletedValue();
            if(done==UINT64_MAX||FAILED(device->GetDeviceRemovedReason()))return false;
            if(done>=value)return true;Sleep(1);
        }
        return false;
    }
    bool Build(const Request& request) {
        auto next=std::make_unique<Eye>();next->build=request;
        // Adopt duplicated handles immediately, including failure paths.
        std::array<Handle,slots+2> handles;
        for(std::size_t n=0;n<slots;++n)handles[n].Reset(reinterpret_cast<HANDLE>(request.textures[n]));
        handles[slots].Reset(reinterpret_cast<HANDLE>(request.fence_in));
        handles[slots+1].Reset(reinterpret_cast<HANDLE>(request.fence_out));
        for(std::size_t n=0;n<slots;++n) {
            if(FAILED(device5->OpenSharedResource1(handles[n].value,IID_PPV_ARGS(&next->textures[n]))))return false;
            D3D11_TEXTURE2D_DESC desc{};next->textures[n]->GetDesc(&desc);const bool output=n==static_cast<std::size_t>(Slot::output);
            if(desc.Width!=(output?request.out_width:request.width)||desc.Height!=(output?request.out_height:request.height)||
               desc.Format!=formats[n]||desc.SampleDesc.Count!=1||desc.ArraySize!=1||desc.MipLevels!=1||
               desc.Usage!=D3D11_USAGE_DEFAULT||!(desc.BindFlags&D3D11_BIND_SHADER_RESOURCE)||
               (output&&!(desc.BindFlags&D3D11_BIND_UNORDERED_ACCESS)))return false;
        }
        if(FAILED(device5->OpenSharedFence(handles[slots].value,IID_PPV_ARGS(&next->incoming)))||
           FAILED(device5->OpenSharedFence(handles[slots+1].value,IID_PPV_ARGS(&next->outgoing))))return false;
        constexpr std::array<NVSDK_NGX_PerfQuality_Value,4> quality{NVSDK_NGX_PerfQuality_Value_DLAA,
            NVSDK_NGX_PerfQuality_Value_MaxQuality,NVSDK_NGX_PerfQuality_Value_Balanced,NVSDK_NGX_PerfQuality_Value_MaxPerf};
        NVSDK_NGX_DLSS_Create_Params create{};
        create.Feature={request.width,request.height,request.out_width,request.out_height,quality[static_cast<unsigned>(request.quality)]};
        create.InFeatureCreateFlags=NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
        if(!Ngx(NGX_D3D11_CREATE_DLSS_EXT(context.Get(),&next->feature,parameters,&create)))return false;
        eyes[request.eye]=std::move(next);return true;
    }
    bool Evaluate(const Request& request) {
        auto& eye=eyes[request.eye];if(!eye)return false;
        const auto& b=eye->build;
        if(request.width!=b.width||request.height!=b.height||request.out_width!=b.out_width||
            request.out_height!=b.out_height||request.quality!=b.quality||request.sequence<=eye->previous)return false;
        if(!Wait(eye->incoming.Get(),request.sequence))return false;
        const auto texture=[&](Slot slot){return eye->textures[static_cast<std::size_t>(slot)].Get();};
        NVSDK_NGX_D3D11_DLSS_Eval_Params evaluate{};
        evaluate.Feature.pInColor=texture(Slot::color);evaluate.Feature.pInOutput=texture(Slot::output);
        evaluate.pInDepth=texture(Slot::depth);evaluate.pInMotionVectors=texture(Slot::motion);
        evaluate.pInBiasCurrentColorMask=texture(Slot::bias);
        evaluate.InJitterOffsetX=request.jitter_x;evaluate.InJitterOffsetY=request.jitter_y;
        evaluate.InMVScaleX=float(b.width);evaluate.InMVScaleY=float(b.height);
        evaluate.InRenderSubrectDimensions={b.width,b.height};evaluate.InReset=request.reset||eye->previous==0;
        evaluate.InPreExposure=evaluate.InExposureScale=1;
        const bool ok=Ngx(NGX_D3D11_EVALUATE_DLSS_EXT(context.Get(),eye->feature,parameters,&evaluate));
        // Even a refused evaluate can queue GPU work. Complete before sending
        // any acknowledgement; an ack is never substituted for fence proof.
        if(FAILED(context4->Signal(eye->outgoing.Get(),request.sequence)))return false;
        context->Flush();if(!Wait(eye->outgoing.Get(),request.sequence))return false;
        eye->previous=request.sequence;return ok;
    }
};
}
int wmain(int argc,wchar_t** argv) try {
    if(argc!=4||std::wstring_view(argv[1])!=L"--pipe")return 2;
    const std::wstring name=argv[2];
    if(!name.starts_with(L"\\\\.\\pipe\\cojvr-ngx-")||name.size()>150)return 2;
    wchar_t* tail=nullptr;const auto parent=wcstoul(argv[3],&tail,10);if(!parent||!tail||*tail)return 2;
    Handle pipe(CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
    if(pipe.value==INVALID_HANDLE_VALUE)return 3;
    ULONG server=0;if(!GetNamedPipeServerProcessId(pipe.value,&server)||server!=parent)return 3;
    Handle peer(OpenProcess(SYNCHRONIZE,FALSE,parent));if(!peer.value)return 3;
    Host host;bool initialized=false;
    for(;;) {
        Request request{};
        // Idle is normal (menus/pause); parent lifetime and pipe closure still
        // wake this wait. Bound the remainder once the first byte arrives.
        if(!Transfer(pipe.value,&request,1,false,INFINITE,peer.value)||
           !Transfer(pipe.value,reinterpret_cast<unsigned char*>(&request)+1,sizeof(request)-1,false,3000,peer.value))return 4;
        Response response;response.operation=request.operation;response.eye=request.eye;response.sequence=request.sequence;
        if(!ValidRequest(request)) {response.system_error=ERROR_INVALID_DATA;}
        else if(request.operation==Operation::quit)return 0;
        else if(request.operation==Operation::hello&&!initialized) {initialized=host.Init(request);response.ok=initialized;}
        else if(initialized&&request.operation==Operation::build)response.ok=host.Build(request);
        else if(initialized&&request.operation==Operation::evaluate)response.ok=host.Evaluate(request);
        response.ngx_result=host.ngx;
        if(!Transfer(pipe.value,&response,sizeof(response),true,2000,peer.value))return 4;
        if(!initialized||!response.ok)return 5;
    }
} catch(...) {return 6;}
