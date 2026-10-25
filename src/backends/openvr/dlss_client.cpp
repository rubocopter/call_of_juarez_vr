#include "backends/openvr/dlss_client.hpp"
#include "backends/openvr/dlss_pipe.hpp"
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <string>

namespace cojvr::backends::openvr::dlss {
using Microsoft::WRL::ComPtr;
namespace {
constexpr std::array<DXGI_FORMAT,slots> formats{DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R8_UNORM,DXGI_FORMAT_R16G16B16A16_FLOAT};
std::atomic_uint32_t pipe_sequence{0};
struct Eye {
    Request description{};
    std::array<ComPtr<ID3D11Texture2D>,slots> textures;
    std::array<Handle,slots> handles;
    ComPtr<ID3D11Fence> incoming,outgoing;
    Handle input_handle,output_handle;
    std::uint64_t sequence=0;
    bool outstanding=false,output_valid=false;
};
}
struct Client::Impl {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11Device5> device5;
    Handle pipe,process,job;
    std::array<std::unique_ptr<Eye>,2> eyes;
    std::string error;
    std::uint32_t ngx=0,bits=0;
    bool running=false;
    bool Fail(const char* message) noexcept {try{error=message;}catch(...){}return false;}
    bool Exchange(Request& request,Response& response) noexcept {
        if(!running)return Fail("DLSS helper unavailable");
        if(!Transfer(pipe.value,&request,sizeof(request),true,3000,process.value)||
           !Transfer(pipe.value,&response,sizeof(response),false,5000,process.value)) {
            running=false;return Fail("DLSS helper communication failed or timed out");
        }
        ngx=response.ngx_result;bits=response.process_bits;
        if(response.signature!=magic||response.protocol!=version||response.operation!=request.operation||
           response.eye!=request.eye||response.sequence!=request.sequence||response.process_bits!=64) {
            running=false;return Fail("DLSS helper returned mismatched identity");
        }
        if(!response.ok){
            running=false;
            try{error="DLSS helper refused operation "+std::to_string(static_cast<unsigned>(request.operation))+
                ";system="+std::to_string(response.system_error);}catch(...){}
            return false;
        }
        return true;
    }
    bool Retired() noexcept {
        for(auto& eye:eyes)if(eye&&eye->outstanding) {
            const auto complete=eye->outgoing->GetCompletedValue();
            if(complete==UINT64_MAX||complete<eye->sequence)return false;
            eye->outstanding=false;
        }
        return true;
    }
};
Client::Client():impl_(std::make_unique<Impl>()){}
Client::~Client(){
    Stop();
    // A process exit/timeout is not GPU completion. Retain uncertain allocations
    // for process lifetime rather than recycle resources still used by the GPU.
    if(impl_&&!impl_->Retired())(void)impl_.release();
}
bool Client::Start(ID3D11Device* device,const std::filesystem::path& helper) noexcept {
    Stop();
    if(!impl_->Retired())return impl_->Fail("DLSS resources quarantined; process restart required");
    try {impl_=std::make_unique<Impl>();}
    catch(...) {return impl_->Fail("DLSS startup allocation failed");}
    auto& p=*impl_;
    try {
        struct StartupRollback {
            Client& client; Impl& state; bool committed=false;
            ~StartupRollback(){if(!committed){state.job.Reset();client.Stop();}}
        } rollback{*this,p};
        if(!device||!helper.is_absolute()||!std::filesystem::is_regular_file(helper))return p.Fail("DLSS x64 helper path unavailable");
        if(FAILED(device->QueryInterface(IID_PPV_ARGS(&p.device5))))return p.Fail("D3D11 shared fences unavailable");
        p.device=device;
        ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC desc{};
        if(FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi)))||FAILED(dxgi->GetAdapter(&adapter))||FAILED(adapter->GetDesc(&desc)))
            return p.Fail("DLSS consumer adapter identity unavailable");
        const auto name=L"\\\\.\\pipe\\cojvr-ngx-"+std::to_wstring(GetCurrentProcessId())+L"-"+
            std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(pipe_sequence.fetch_add(1));
        p.pipe.Reset(CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,4096,4096,0,nullptr));
        if(p.pipe.value==INVALID_HANDLE_VALUE)return p.Fail("DLSS private pipe creation failed");
        Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));if(!event.value)return p.Fail("DLSS connection event failed");
        OVERLAPPED connection{};connection.hEvent=event.value;
        const BOOL connected=ConnectNamedPipe(p.pipe.value,&connection);
        const auto connection_error=connected?ERROR_SUCCESS:GetLastError();
        if(!connected&&connection_error!=ERROR_IO_PENDING&&connection_error!=ERROR_PIPE_CONNECTED)return p.Fail("DLSS pipe connect failed");
        struct PendingConnection {
            HANDLE pipe; OVERLAPPED* io; bool pending;
            void Drain() noexcept {
                if(!pending)return;
                CancelIoEx(pipe,io);DWORD ignored=0;GetOverlappedResult(pipe,io,&ignored,TRUE);pending=false;
            }
            ~PendingConnection(){Drain();}
        } pending{p.pipe.value,&connection,!connected&&connection_error==ERROR_IO_PENDING};
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
        std::wstring command=L"\""+helper.wstring()+L"\" --pipe "+name+L" "+std::to_wstring(GetCurrentProcessId());
        p.job.Reset(CreateJobObjectW(nullptr,nullptr));
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!p.job.value||!SetInformationJobObject(p.job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) {
            return p.Fail("DLSS helper lifetime job unavailable");
        }
        const BOOL created=CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,
            nullptr,helper.parent_path().c_str(),&startup,&process);
        if(!created)return p.Fail("DLSS helper launch failed");
        p.process.Reset(process.hProcess);Handle thread(process.hThread);
        if(!AssignProcessToJobObject(p.job.value,p.process.value)) {
            TerminateProcess(p.process.value,7);return p.Fail("DLSS helper lifetime ownership failed");
        }
        if(ResumeThread(thread.value)==DWORD(-1))return p.Fail("DLSS helper resume failed");
        if(!connected&&connection_error==ERROR_IO_PENDING) {
            HANDLE waits[2]{event.value,p.process.value};const auto waited=WaitForMultipleObjects(2,waits,FALSE,5000);
            DWORD ignored=0;
            if(waited!=WAIT_OBJECT_0) {pending.Drain();Stop();return p.Fail("DLSS helper did not connect");}
            if(!GetOverlappedResult(p.pipe.value,&connection,&ignored,FALSE))return p.Fail("DLSS helper connection failed");
            pending.pending=false;
        }
        ULONG peer=0;
        if(!GetNamedPipeClientProcessId(p.pipe.value,&peer)||peer!=process.dwProcessId)return p.Fail("DLSS helper process identity mismatch");
        p.running=true;Request hello;hello.adapter_low=desc.AdapterLuid.LowPart;hello.adapter_high=desc.AdapterLuid.HighPart;
        Response response;rollback.committed=p.Exchange(hello,response);return rollback.committed;
    }catch(...){return p.Fail("DLSS helper startup exception");}
}
bool Client::Build(unsigned eye,unsigned width,unsigned height,unsigned output_width,unsigned output_height,Quality quality) noexcept {
    auto& p=*impl_;Request build;build.operation=Operation::build;build.eye=eye;build.width=width;build.height=height;
    if(eye<2&&p.eyes[eye])p.eyes[eye]->output_valid=false;
    build.out_width=output_width;build.out_height=output_height;build.quality=quality;
    if(eye>1||!ValidExtent(build)||!p.running||!p.Retired())return p.Fail("DLSS eye build rejected bounds/lifetime");
    try {
        auto next=std::make_unique<Eye>();
        for(std::size_t slot=0;slot<slots;++slot) {
            const bool output=slot==static_cast<std::size_t>(Slot::output);
            D3D11_TEXTURE2D_DESC desc{};desc.Width=output?output_width:width;desc.Height=output?output_height:height;
            desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=formats[slot];
            desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|(output?D3D11_BIND_UNORDERED_ACCESS:0);
            desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED_NTHANDLE|D3D11_RESOURCE_MISC_SHARED;
            ComPtr<IDXGIResource1> resource;
            HRESULT allocated=p.device->CreateTexture2D(&desc,nullptr,&next->textures[slot]);
            if(SUCCEEDED(allocated))allocated=next->textures[slot].As(&resource);
            if(SUCCEEDED(allocated))allocated=resource->CreateSharedHandle(nullptr,DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,nullptr,&next->handles[slot].value);
            if(FAILED(allocated)) {
                p.error="DLSS shared texture allocation failed;slot="+std::to_string(slot)+";hr="+std::to_string(static_cast<std::uint32_t>(allocated));
                return false;
            }
        }
        if(FAILED(p.device5->CreateFence(0,D3D11_FENCE_FLAG_SHARED,IID_PPV_ARGS(&next->incoming)))||
           FAILED(p.device5->CreateFence(0,D3D11_FENCE_FLAG_SHARED,IID_PPV_ARGS(&next->outgoing)))||
           FAILED(next->incoming->CreateSharedHandle(nullptr,GENERIC_ALL,nullptr,&next->input_handle.value))||
           FAILED(next->outgoing->CreateSharedHandle(nullptr,GENERIC_ALL,nullptr,&next->output_handle.value)))return p.Fail("DLSS shared fence creation failed");
        const auto duplicate=[&](HANDLE source,std::uint64_t& target) {
            HANDLE result=nullptr;if(!DuplicateHandle(GetCurrentProcess(),source,p.process.value,&result,0,FALSE,DUPLICATE_SAME_ACCESS))return false;
            target=reinterpret_cast<std::uintptr_t>(result);return true;
        };
        for(std::size_t n=0;n<slots;++n)if(!duplicate(next->handles[n].value,build.textures[n])) {Stop();return p.Fail("DLSS handle duplication failed");}
        if(!duplicate(next->input_handle.value,build.fence_in)||!duplicate(next->output_handle.value,build.fence_out)) {Stop();return p.Fail("DLSS fence duplication failed");}
        Response response;
        if(!p.Exchange(build,response))return false;
        next->description=build;p.eyes[eye]=std::move(next);return true;
    }catch(...){return p.Fail("DLSS eye resource exception");}
}
bool Client::Evaluate(ID3D11DeviceContext* context,unsigned eye,const Inputs& input) noexcept {
    auto& p=*impl_;
    if(eye<2&&p.eyes[eye])p.eyes[eye]->output_valid=false;
    if(!context||eye>1||!p.running||!p.eyes[eye]||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return p.Fail("DLSS missing context/eye");
    auto& target=*p.eyes[eye];target.output_valid=false;
    if(target.outstanding)return p.Fail("DLSS eye still has unretired work");
    ComPtr<ID3D11Device> owner;context->GetDevice(&owner);
    ComPtr<ID3D11DeviceContext4> context4;
    if(owner.Get()!=p.device.Get()||FAILED(context->QueryInterface(IID_PPV_ARGS(&context4))))return p.Fail("DLSS foreign consumer context");
    const std::array<ID3D11Texture2D*,4> inputs{input.color,input.depth,input.motion,input.bias};
    Request request=target.description;request.operation=Operation::evaluate;request.sequence=target.sequence+1;
    request.reset=input.reset?1:0;request.jitter_x=input.jitter_x;request.jitter_y=input.jitter_y;
    if(!ValidRequest(request))return p.Fail("DLSS invalid jitter or sequence");
    for(std::size_t n=0;n<inputs.size();++n) {
        if(!inputs[n])return p.Fail("DLSS incomplete temporal guides");
        D3D11_TEXTURE2D_DESC desc{};inputs[n]->GetDesc(&desc);owner.Reset();inputs[n]->GetDevice(&owner);
        if(owner.Get()!=p.device.Get()||desc.Width!=request.width||desc.Height!=request.height||desc.Format!=formats[n]||
           desc.ArraySize!=1||desc.MipLevels!=1||desc.SampleDesc.Count!=1)return p.Fail("DLSS guide ownership/format/extent mismatch");
    }
    for(std::size_t n=0;n<inputs.size();++n)context->CopyResource(target.textures[n].Get(),inputs[n]);
    target.sequence=request.sequence;target.outstanding=true;
    if(FAILED(context4->Signal(target.incoming.Get(),request.sequence)))return p.Fail("DLSS producer fence signal failed");
    context->Flush();Response response;
    if(!p.Exchange(request,response))return false;
    const auto completed=target.outgoing->GetCompletedValue();
    if(completed==UINT64_MAX||completed<request.sequence||FAILED(p.device->GetDeviceRemovedReason())) {
        p.running=false;return p.Fail("DLSS acknowledgement lacks completed GPU fence");
    }
    target.outstanding=false;target.output_valid=true;return true;
}
ID3D11Texture2D* Client::Output(unsigned eye) const noexcept {
    return eye<2&&impl_->eyes[eye]&&impl_->eyes[eye]->output_valid?
        impl_->eyes[eye]->textures[static_cast<std::size_t>(Slot::output)].Get():nullptr;
}
std::string_view Client::last_error() const noexcept{return impl_->error;}
std::uint32_t Client::last_ngx_result() const noexcept{return impl_->ngx;}
std::uint32_t Client::helper_bits() const noexcept{return impl_->bits;}
void Client::Stop() noexcept {
    if(!impl_)return;auto& p=*impl_;
    for(auto& eye:p.eyes)if(eye)eye->output_valid=false;
    if(p.running) {Request quit;quit.operation=Operation::quit;(void)Transfer(p.pipe.value,&quit,sizeof(quit),true,100,p.process.value);}
    p.running=false;p.pipe.Reset();
    if(p.process.value!=INVALID_HANDLE_VALUE&&WaitForSingleObject(p.process.value,3000)!=WAIT_OBJECT_0) {
        // The job contains only our hidden helper. Never owns game/SteamVR.
        p.job.Reset();WaitForSingleObject(p.process.value,1000);
    }
    p.process.Reset();p.job.Reset();
    if(p.Retired())for(auto& eye:p.eyes)eye.reset();
}
}
