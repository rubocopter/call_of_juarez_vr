#include "backends/openvr/neural_presenter.hpp"
#include <wrl/client.h>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
namespace n=cojvr::backends::openvr::neural;
namespace rt=cojvr::runtime;
void Check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int wmain(int argc,wchar_t** argv)try{
    n::Options options;if(argc==3){options.enabled=true;options.helper=argv[1];options.runtime=argv[2];}
    n::Presenter processor(options);
    processor.SetRequested(argc==3,1);
    Check(!processor.Process(nullptr,nullptr,{}, {}, {},1,false),"invalid native inputs were processed");
    if(argc!=3){Check(!processor.enabled(),"missing opt-in enabled Neural");std::cout<<"Neural presenter default-off passed\n";return 0;}
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,&level,1,D3D11_SDK_VERSION,&device,nullptr,&context)),"D3D11 unavailable");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=2804;desc.Height=2860;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    std::array<ComPtr<ID3D11Texture2D>,2> textures;std::array<ComPtr<ID3D11RenderTargetView>,2> views;
    std::array<ID3D11Texture2D*,2> eyes{};
    for(unsigned i=0;i<2;++i){Check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&textures[i])),"native eye allocation failed");
        Check(SUCCEEDED(device->CreateRenderTargetView(textures[i].Get(),nullptr,&views[i])),"native eye view failed");eyes[i]=textures[i].Get();}
    std::array<rt::EyeView,2> optics{};
    for(unsigned i=0;i<2;++i){optics[i].eye=i?rt::Eye::right:rt::Eye::left;optics[i].eye_to_head.orientation_valid=true;optics[i].fov={-.7F,.7F,.7F,-.7F};}
    std::vector<double> durations,ages;unsigned applied=0,bypassed=0;
    const auto end=GetTickCount64()+60000;
    std::uint64_t active_frame=0;
    for(std::uint64_t frame_no=1;GetTickCount64()<end&&applied<30;++frame_no){
        const auto poll_begin=std::chrono::steady_clock::now();
        const bool completed=processor.Pump(device.Get(),context.Get(),eyes,active_frame,1,1,true);
        const auto poll_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-poll_begin).count();
        if(completed){++applied;durations.push_back(poll_ms);
            Check(processor.RetainedFrame().capture_sequence==active_frame,"processed pair borrowed newer frame metadata");
            ages.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-processor.RetainedFrame().capture_time).count());}
        if(processor.busy()){Sleep(3);continue;}
        for(unsigned i=0;i<2;++i){const float color[]{i?.7F:.2F,.3F,i?.2F:.7F,.75F};context->ClearRenderTargetView(views[i].Get(),color);}
        cojvr::backends::d3d9::StereoCpuFrame frame;
        frame.device_id=1;frame.generation=1;frame.capture_sequence=frame.render_pose_sequence=frame_no;
        frame.render_hmd_pose.orientation_valid=frame.render_hmd_pose.position_valid=true;frame.capture_time=std::chrono::steady_clock::now();
        const auto begin=std::chrono::steady_clock::now();
        const bool ok=processor.Process(device.Get(),context.Get(),eyes,frame,optics,1,true);
        active_frame=frame_no;
        const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        Check(!ok,"new source synchronously processed instead of retaining");++bypassed;
        if(processor.busy()){
            Check(!processor.RetainedFrame().producer_lease.valid(),"Neural retained a producer resource lease");
            for(const auto& eye:processor.RetainedFrame().eyes)Check(!eye.borrowed_pixels&&eye.pixels.empty(),"Neural retained source CPU storage");
        }
        if(processor.busy())Sleep(6); // Retain a query across the rejected old 2ms window.
        Check(SUCCEEDED(device->GetDeviceRemovedReason()),"D3D11 device removed");
        if(std::string(processor.status())=="helper_failed"||std::string(processor.status())=="helper_unavailable"||
           std::string(processor.status())=="disabled_after_failures"){
            std::cerr<<"terminal status="<<processor.status()<<'\n';break;}
        Sleep(3);
    }
    Check(applied==30,"candidate failed to deliver 30 current Neural pairs");
    std::sort(durations.begin(),durations.end());
    std::sort(ages.begin(),ages.end());Check(processor.applied_pairs()==applied,"applied count differs from commits");
    processor.Toggle();Check(!processor.enabled(),"A/B toggle did not disable");
    std::cout<<"Real candidate native=2804x2860;applied="<<applied<<";bypassed="<<bypassed
        <<";pump_cpu_p50_ms="<<durations[durations.size()/2]<<";pump_cpu_p95_ms="<<durations[durations.size()*95/100]
        <<";pair_age_p50_ms="<<ages[ages.size()/2]<<";pair_age_p95_ms="<<ages[ages.size()*95/100]
        <<";off_toggle=true;headset_acceptance=false\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
