// Adapted lifecycle policy from Penumbra VR Framework (GPL-3.0-or-later),
// revision 7b014c96ed928440abf4452664f35220d06c1fb6. Original CoJ pair ABI.
#include "backends/openvr/neural_session.hpp"
#include "backends/openvr/dlss_pipe.hpp"
#include <atomic>
#include <cstring>
#include <limits>

namespace cojvr::backends::openvr::neural {
using dlss::Handle;
namespace {std::atomic_uint64_t counter{0};}
struct Session::Impl {
    Handle mapping,ready,request,complete,stop,child;
    Header* header=nullptr;
    Header sent{};
    Identity previous{};
    bool have_previous=false,reset_required=false;
    std::uint64_t sequence=0,started=0;
    State state=State::stopped;
    std::vector<std::uint8_t> output;
    std::string error;
    ~Impl(){if(header)UnmapViewOfFile(header);}
    bool Fail(const char* why){error=why;state=State::failed;output.clear();if(stop.value!=INVALID_HANDLE_VALUE)SetEvent(stop.value);return false;}
    bool Alive()const{return child.value!=INVALID_HANDLE_VALUE&&WaitForSingleObject(child.value,0)==WAIT_TIMEOUT;}
    bool Match()const{
        return Completed(*header)&&header->request==sent.request&&Same(header->output,sent.input)&&
            header->width==sent.width&&header->height==sent.height&&
            std::memcmp(header->views.data(),sent.views.data(),sizeof(sent.views))==0;
    }
};
Session::Session():impl_(std::make_unique<Impl>()){}
Session::~Session(){Stop();}
void Session::Stop(){
    if(impl_->stop.value!=INVALID_HANDLE_VALUE)SetEvent(impl_->stop.value);
    // Only CPU mapping is shared; the child owns its own mapped view and GPU
    // resources. Unique names prevent reuse while asynchronous exit completes.
    impl_->output.clear();impl_->state=State::stopped;
}
bool Session::Start(const std::filesystem::path& helper,const std::filesystem::path& runtime,const std::wstring& mode){
    Stop();impl_=std::make_unique<Impl>();auto& p=*impl_;
    if(!helper.is_absolute()||!std::filesystem::is_regular_file(helper)||
       helper.wstring().find(L'"')!=std::wstring::npos||runtime.wstring().find(L'"')!=std::wstring::npos||
       (mode!=L"identity"&&mode!=L"edit"&&mode!=L"delay"&&mode!=L"fault"&&mode!=L"stale"&&mode!=L"model"))
        return p.Fail("Neural helper path or mode unavailable");
    const auto base=L"Local\\CoJNeural-"+std::to_wstring(GetCurrentProcessId())+L"-"+
        std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(++counter);
    p.mapping.Reset(CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,DWORD(mapping_bytes),(base+L"-map").c_str()));
    if(!p.mapping.value||p.mapping.value==INVALID_HANDLE_VALUE||GetLastError()==ERROR_ALREADY_EXISTS)return p.Fail("Neural mapping creation failed");
    p.header=static_cast<Header*>(MapViewOfFile(p.mapping.value,FILE_MAP_ALL_ACCESS,0,0,mapping_bytes));
    if(!p.header)return p.Fail("Neural mapping view failed");
    *p.header=Header{};
    const auto event=[&](Handle& out,const wchar_t* suffix,bool manual){out.Reset(CreateEventW(nullptr,manual,FALSE,(base+suffix).c_str()));return out.value&&GetLastError()!=ERROR_ALREADY_EXISTS;};
    if(!event(p.ready,L"-ready",true)||!event(p.request,L"-request",false)||!event(p.complete,L"-complete",true)||!event(p.stop,L"-stop",true))return p.Fail("Neural event creation failed");
    const auto directory=runtime.empty()?helper.parent_path():runtime;
    std::wstring command=L"\""+helper.wstring()+L"\" \""+base+L"\" "+std::to_wstring(GetCurrentProcessId())+
        L" \""+directory.wstring()+L"\" "+mode;
    STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    if(!CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,
            helper.parent_path().c_str(),&si,&pi))return p.Fail("Neural child creation failed");
    p.child.Reset(pi.hProcess);CloseHandle(pi.hThread);p.started=GetTickCount64();p.state=State::starting;return true;
}
State Session::Poll(){
    auto& p=*impl_;
    if(p.state==State::stopped||p.state==State::failed)return p.state;
    if(!p.Alive()){p.Fail("Neural child exited");return p.state;}
    if(p.state==State::starting){
        if(WaitForSingleObject(p.ready.value,0)==WAIT_OBJECT_0){
            if(p.header->signature!=magic||p.header->protocol!=version||p.header->process_bits!=64||p.header->result!=0)p.Fail("Neural startup rejected");
            else p.state=State::ready;
        }else if(GetTickCount64()-p.started>10000)p.Fail("Neural startup deadline exceeded");
    }else if(p.state==State::recovering){
        if(WaitForSingleObject(p.complete.value,0)==WAIT_OBJECT_0){
            if(!p.Match())p.Fail("Neural late completion identity rejected");
            else {p.state=State::ready;p.output.clear();p.reset_required=true;}
        }else if(GetTickCount64()-p.started>2000)p.Fail("Neural worker stalled after deadline");
    }
    return p.state;
}
bool Session::Process(const Header& frame,std::span<const std::uint8_t> pair,std::uint32_t timeout_ms){
    auto& p=*impl_;p.output.clear();
    const bool was_recovering=p.state==State::recovering;
    if(Poll()!=State::ready||was_recovering)return false;
    Header next=frame;next.request=p.sequence+1;next.completed=0;next.result=0;next.process_bits=0;next.output={};
    if(!Valid(next)||pair.size()!=eye_bytes(next.width,next.height)*2||timeout_ms>5000||
       p.sequence==std::numeric_limits<std::uint64_t>::max()) {p.error="Neural input rejected";return false;}
    if(p.have_previous&&next.input.device==p.previous.device&&next.input.generation==p.previous.generation&&
       (next.input.frame<=p.previous.frame||next.input.pose<=p.previous.pose)) {p.error="Neural repeated/stale frame rejected";return false;}
    // A discontinuity requires an explicitly new caller-owned history epoch.
    if(p.have_previous&&(next.input.epoch<p.previous.epoch||next.input.revision<p.previous.revision)){
        p.error="Neural old revision/history rejected";return false;
    }
    if(p.have_previous&&next.input.epoch==p.previous.epoch&&
       (p.reset_required||next.input.device!=p.previous.device||next.input.generation!=p.previous.generation||
        next.input.revision!=p.previous.revision||next.input.frame!=p.previous.frame+1||
        next.width!=p.sent.width||next.height!=p.sent.height)){
        p.error="Neural discontinuity requires fresh history epoch";return false;
    }
    if(!ResetEvent(p.complete.value))return p.Fail("Neural completion reset failed");
    *p.header=next;std::memcpy(input_pixels(p.header),pair.data(),pair.size());p.sent=next;
    p.sequence=next.request;p.previous=next.input;p.have_previous=true;p.reset_required=false;p.started=GetTickCount64();p.state=State::pending;
    if(!SetEvent(p.request.value))return p.Fail("Neural request signal failed");
    HANDLE waits[]{p.complete.value,p.child.value};
    const auto result=WaitForMultipleObjects(2,waits,FALSE,timeout_ms);
    if(result==WAIT_TIMEOUT){p.state=State::recovering;return false;}
    if(result!=WAIT_OBJECT_0||!p.Match())return p.Fail("Neural completion identity/worker rejected");
    p.output.assign(output_pixels(p.header),output_pixels(p.header)+pair.size());p.state=State::ready;return true;
}
const std::vector<std::uint8_t>& Session::Output()const{return impl_->output;}
const std::string& Session::Error()const{return impl_->error;}
}
