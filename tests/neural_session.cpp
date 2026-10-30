#include "backends/openvr/neural_session.hpp"
#include <windows.h>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace cojvr::backends::openvr::neural;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void RequireSession(bool value,const Session& s){if(!value)throw std::runtime_error(s.Error());}
Header Frame(std::uint64_t number=1){Header h;h.width=h.height=64;h.request=number;h.input={number,number,7,1,1,1};return h;}
void WaitReady(Session& s){auto end=GetTickCount64()+10000;while(s.Poll()==State::starting&&GetTickCount64()<end)Sleep(1);RequireSession(s.Poll()==State::ready,s);}
int wmain(int argc,wchar_t** argv)try{
    auto h=Frame();Require(Valid(h),"valid header rejected");
    for(unsigned fault=0;fault<10;++fault){auto bad=h;switch(fault){
        case 0:bad.input.pose=0;break;case 1:bad.width=max_extent+1;break;
        case 2:bad.views[0].orientation[0]=std::numeric_limits<float>::quiet_NaN();break;
        case 3:bad.views[1].fov[0]=.2F;break;case 4:bad.input.device=0;break;
        case 5:bad.input.revision=0;break;case 6:bad.protocol++;break;
        case 7:bad.views[1].orientation={0,0,0,0};break;case 8:bad.height=0;break;case 9:bad.input.epoch=0;break;
    }Require(!Valid(bad),"malformed identity/optics admitted");}
    h.completed=h.request;h.output=h.input;h.result=1;h.process_bits=64;Require(Completed(h),"valid completion rejected");
    h.output.generation++;Require(!Completed(h),"stale output admitted");
    if(argc==1){Session s;Require(!s.Start(L"missing.exe",{},L"identity"),"missing helper admitted");std::cout<<"Neural protocol/dependency checks passed\n";return 0;}
    const std::filesystem::path helper=argv[1];
    std::vector<std::uint8_t> pixels(eye_bytes(64,64)*2);
    for(std::size_t i=0;i<pixels.size();i+=4){pixels[i]=i<pixels.size()/2?33:177;pixels[i+1]=75;pixels[i+2]=99;pixels[i+3]=std::uint8_t((i/4)%256);}
    for(const auto* mode:{L"identity",L"edit",L"delay",L"fault",L"stale"}){
        Session s;const bool started=s.Start(helper,{},mode);RequireSession(started,s);WaitReady(s);
        RequireSession(s.Process(Frame(),pixels,5000),s);
        auto expected=pixels;
        if(std::wstring(mode)==L"edit")for(std::size_t i=0;i<expected.size();++i)if(i%4!=3)expected[i]=std::uint8_t(std::min(255,int(expected[i])+16));
        Require(s.Output()==expected,"identity/edit lost alpha or per-eye pixels");
        Require(!s.Process(Frame(),pixels,10)&&s.Output().empty(),"repeat exposed stale output");
        if(std::wstring(mode)==L"delay"){
            auto begin=GetTickCount64();Require(!s.Process(Frame(2),pixels,2),"delayed frame published");
            Require(GetTickCount64()-begin<80,"deadline wait was unbounded");
            const auto end=GetTickCount64()+2000;while(s.Poll()==State::recovering&&GetTickCount64()<end)Sleep(1);
            Require(s.Poll()==State::ready&&s.Output().empty(),"late output not drained/discarded");
            Require(!s.Process(Frame(3),pixels,1000),"timeout history reused without reset");
            auto fresh=Frame(3);fresh.input.epoch=2;
            Require(s.Process(fresh,pixels,1000)&&s.Output()==pixels,"fresh frame failed after timeout");
        }else if(std::wstring(mode)==L"fault"||std::wstring(mode)==L"stale"){
            Require(!s.Process(Frame(2),pixels,1000)&&s.Output().empty(),"fault/stale result exposed");
            Require(s.Poll()==State::failed,"fault did not quarantine session");
        }
        if(std::wstring(mode)==L"identity"){
            auto changed=Frame(2);changed.input.revision=2;
            Require(!s.Process(changed,pixels,1000),"settings changed without reset");
            changed.input.epoch=2;Require(s.Process(changed,pixels,1000),"new settings/history rejected");
            changed=Frame(3);Require(!s.Process(changed,pixels,1000),"old revision/history accepted");
            changed=Frame(3);changed.input={3,3,8,2,2,3};Require(s.Process(changed,pixels,1000),"new device/generation reset rejected");
            auto invalid=Frame(4);invalid.input=changed.input;invalid.input.frame=invalid.input.pose=4;invalid.views[0].orientation={0,0,0,0};
            Require(!s.Process(invalid,pixels,1000)&&s.Output().empty(),"invalid tracking retained output");
        }
        s.Stop();Require(s.Output().empty(),"stop retained output");
    }
    {
        Session s;RequireSession(s.Start(helper,{},L"delay"),s);WaitReady(s);
        RequireSession(s.Process(Frame(),pixels,1000),s);
        const auto begin=GetTickCount64();
        RequireSession(s.Warmup(Frame(2),pixels),s);
        Require(GetTickCount64()-begin<80,"warmup blocked caller");
        Require(s.Output().empty(),"warmup exposed disposable pixels");
        Require(!s.Warmup(Frame(3),pixels),"pending warmup deadline was restarted");
        Require(!s.Process(Frame(3),pixels,1000),"pending warmup input overwritten");
        auto end=GetTickCount64()+2000;while(s.Poll()==State::warming&&GetTickCount64()<end)Sleep(1);
        Require(s.Poll()==State::ready&&s.Output().empty(),"warmup result was not discarded");
        Require(!s.Process(Frame(3),pixels,1000),"warmup reused stale history");
        auto next=Frame(3);next.input.epoch=2;
        RequireSession(s.Process(next,pixels,1000),s);
        s.Stop();Require(s.Poll()==State::stopped&&s.Output().empty(),"stop exposed output");
        RequireSession(s.Start(helper,{},L"identity"),s);WaitReady(s);
        RequireSession(s.Process(Frame(),pixels,1000),s);
    }
    {
        Session s;RequireSession(s.Start(helper,{},L"delay"),s);WaitReady(s);
        RequireSession(s.Process(Frame(),pixels,1000),s);
        const auto begin=GetTickCount64();RequireSession(s.Submit(Frame(2),pixels),s);
        Require(GetTickCount64()-begin<80,"asynchronous submit blocked caller");
        Require(!s.Submit(Frame(3),pixels),"outstanding input overwritten");
        auto end=GetTickCount64()+2000;while(s.Poll()==State::pending&&GetTickCount64()<end)Sleep(1);
        Require(s.Poll()==State::ready&&s.Output()==pixels,"asynchronous matching pair missing");
        RequireSession(s.Submit(Frame(3),pixels),s);s.Cancel();
        end=GetTickCount64()+2000;while(s.Poll()==State::recovering&&GetTickCount64()<end)Sleep(1);
        Require(s.Poll()==State::ready&&s.Output().empty(),"cancelled result published");
        auto fresh=Frame(4);fresh.input.epoch=2;RequireSession(s.Process(fresh,pixels,1000),s);
    }
    std::cout<<"Neural x86/x64 pair identity/edit, async/cancel, warmup, deadline recovery, fault and stale rejection passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
