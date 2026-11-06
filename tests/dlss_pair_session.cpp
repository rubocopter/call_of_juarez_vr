#include "backends/openvr/dlss_pair_session.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace cojvr::backends::openvr::dlss;
namespace {
void Check(bool ok,const char* reason) {if(!ok)throw std::runtime_error(reason);}
template<class T> T* Token(std::uintptr_t n) {return reinterpret_cast<T*>(n);}
ID3D11Device* const device=Token<ID3D11Device>(0x1000);
ID3D11DeviceContext* const context=Token<ID3D11DeviceContext>(0x2000);

// The real GPU/IPC boundary is the only substitute: policy/history/snapshot
// admission below always run through the production PairSession.
struct Executor final:PairExecutor {
    bool start_ok=true,generate_ok=true;
    int build_failure=-1,evaluate_failure=-1,missing_output=-1,incomplete_eye=-1;
    unsigned builds=0,generates=0,evaluations=0;
    std::array<GuideSource,2> generated{};
    std::array<Inputs,2> evaluated{};
    std::array<ID3D11Texture2D*,2> outputs{};
    PairSession* session=nullptr;
    bool inspect_partial=false,cancel=false;
    PairFrame inspecting{};
    bool Start(ID3D11Device*,const std::filesystem::path&) noexcept override {return start_ok;}
    bool Build(unsigned eye,const cojvr::runtime::TemporalFrameIdentity&) noexcept override {
        ++builds;outputs[eye]=nullptr;return int(eye)!=build_failure;
    }
    bool Generate(ID3D11Device*,ID3D11DeviceContext*,const std::array<GuideSource,2>& sources,
                  std::array<Inputs,2>& result) noexcept override {
        ++generates;generated=sources;
        for(unsigned e=0;e<2;++e) {
            const auto offset=0x3000+e*0x100;
            result[e]={Token<ID3D11Texture2D>(offset),Token<ID3D11Texture2D>(offset+1),
                Token<ID3D11Texture2D>(offset+2),Token<ID3D11Texture2D>(offset+3),
                !sources[e].history,sources[e].jitter_x,sources[e].jitter_y};
            if(int(e)==incomplete_eye)result[e].depth=nullptr;
        }
        return generate_ok;
    }
    bool Evaluate(ID3D11DeviceContext*,unsigned eye,const Inputs& input) noexcept override {
        ++evaluations;evaluated[eye]=input;
        if(session&&inspect_partial) {
            const auto pair=session->Outputs(inspecting);
            if(pair[0]||pair[1])return false;
        }
        if(session&&cancel&&eye==1)session->Invalidate();
        if(int(eye)==evaluate_failure)return false;
        outputs[eye]=int(eye)==missing_output?nullptr:Token<ID3D11Texture2D>(0x5000+eye*0x100);
        return true;
    }
    ID3D11Texture2D* Output(unsigned eye) const noexcept override {return outputs[eye];}
    std::string_view last_error() const noexcept override {return "fixture failure";}
    void Stop() noexcept override {outputs={};}
};
PairFrame Frame(std::uint64_t frame=1) {
    PairFrame result;
    result.identity.frame={0x1000,7,frame,frame+30,9,64,64,64,64,0,true,true};
    result.identity.settings_revision=3;result.identity.history_epoch=4;result.identity.input_context=5;
    for(unsigned eye=0;eye<2;++eye) {
        auto& e=result.eyes[eye];e.identity=result.identity;e.eye=eye;e.input_token=frame*2+eye;
        e.guides.color=Token<ID3D11Texture2D>(0x6000+eye*0x100);
        e.guides.linear_depth_alpha=Token<ID3D11Texture2D>(0x7000+eye*0x100);
        e.guides.current_color_mask=Token<ID3D11Texture2D>(0x8000+eye*0x100);
        e.guides.near_metres=.5F;e.guides.far_metres=100;e.guides.source_qualified=true;
        // Distinct eye transforms catch accidental sharing of left-eye history.
        e.unjittered_view_projection[12]=float(frame)*.125F*(eye?2:1);
    }
    return result;
}
void Stamp(PairFrame& frame) {for(auto& eye:frame.eyes)eye.identity=frame.identity;}
struct Fixture {
    Executor* executor=nullptr;
    PairSession session;
    Fixture():Fixture(std::make_unique<Executor>()) {}
    explicit Fixture(std::unique_ptr<Executor> e):executor(e.get()),session(std::move(e)) {
        Check(session.Start(device,L"C:/explicit-laboratory-helper.exe"),"fixture start rejected");
    }
    void Empty(const PairFrame& frame,const char* reason) {
        const auto output=session.Outputs(frame);Check(!output[0]&&!output[1],reason);
    }
};
void PairPublicationAndHistory() {
    Fixture f;auto first=Frame();f.executor->session=&f.session;f.executor->inspect_partial=true;f.executor->inspecting=first;
    Check(f.session.Reconstruct(context,first),"first qualified pair failed");
    auto pair=f.session.Outputs(first);
    Check(pair[0]==Token<ID3D11Texture2D>(0x5000)&&pair[1]==Token<ID3D11Texture2D>(0x5100),"pair not published together");
    Check(f.executor->evaluated[0].reset&&f.executor->evaluated[1].reset,"first frame reused history");
    auto second=Frame(2);f.executor->inspecting=second;
    Check(f.session.Reconstruct(context,second),"second pair failed");
    Check(!f.executor->evaluated[0].reset&&!f.executor->evaluated[1].reset,"consecutive frame lost history");
    Check(std::abs(f.executor->generated[0].clip_to_previous[12]+.125F)<1e-6F&&
          std::abs(f.executor->generated[1].clip_to_previous[12]+.25F)<1e-6F,"eyes shared transform history");
    Check(f.executor->builds==2,"stable features rebuilt each frame");
    Check(!f.session.Reconstruct(context,second),"duplicate frame advanced history");f.Empty(second,"duplicate retained output");
    auto third=Frame(3);f.executor->inspecting=third;
    Check(f.session.Reconstruct(context,third)&&f.executor->evaluated[0].reset&&f.executor->evaluated[1].reset,
          "duplicate rejection did not reset both histories");
}
void AtomicFailureRecovery() {
    for(int kind=0;kind<7;++kind) {
        Fixture f;auto first=Frame();Check(f.session.Reconstruct(context,first),"setup pair failed");
        auto second=Frame(2);
        if(kind==0)f.executor->evaluate_failure=0;
        if(kind==1)f.executor->evaluate_failure=1;
        if(kind==2)f.executor->missing_output=1;
        if(kind==3)f.executor->generate_ok=false;
        if(kind==4)f.executor->incomplete_eye=1;
        if(kind==5) {second.identity.frame.output_width=96;second.identity.frame.quality=1;Stamp(second);f.executor->build_failure=1;}
        if(kind==6) {f.executor->session=&f.session;f.executor->cancel=true;}
        Check(!f.session.Reconstruct(context,second),"failed/cancelled eye published a pair");
        f.Empty(second,"failure exposed partial output");f.Empty(first,"failure exposed previous pair");
        f.executor->evaluate_failure=f.executor->missing_output=f.executor->incomplete_eye=f.executor->build_failure=-1;
        f.executor->generate_ok=true;f.executor->cancel=false;
        auto third=Frame(3);Check(f.session.Reconstruct(context,third),"failure blocked fresh native pair");
        Check(f.executor->evaluated[0].reset&&f.executor->evaluated[1].reset,"half advanced history survived failure");
    }
}
void RejectInvalidSourcesBeforeWork() {
    // Each omission/mismatch must reject before Build/Generate/Evaluate. This
    // catches validation accidentally delegated to a forgiving test executor.
    for(unsigned fault=0;fault<27;++fault) {
        Fixture f;auto input=Frame();
        switch(fault) {
        case 0:input.eyes[1].guides.source_qualified=false;break;
        case 1:input.eyes[1].guides.color=nullptr;break;
        case 2:input.eyes[1].guides.linear_depth_alpha=nullptr;break;
        case 3:input.eyes[1].guides.current_color_mask=nullptr;break;
        case 4:input.eyes[1].identity.frame.frame++;break;
        case 5:input.eyes[1].identity.frame.pose++;break;
        case 6:input.eyes[1].identity.frame.device++;break;
        case 7:input.eyes[1].identity.frame.generation++;break;
        case 8:input.eyes[1].identity.settings_revision++;break;
        case 9:input.eyes[1].identity.history_epoch++;break;
        case 10:input.eyes[1].identity.input_context++;break;
        case 11:input.eyes[1].eye=0;break;
        case 12:input.eyes[1].input_token=0;break;
        case 13:input.eyes[1].guides.near_metres=0;break;
        case 14:input.eyes[1].guides.far_metres=.25F;break;
        case 15:input.eyes[1].guides.jitter_x=.6F;break;
        case 16:input.eyes[1].unjittered_view_projection.fill(0);break;
        case 17:input.eyes[1].unjittered_view_projection[2]=std::numeric_limits<float>::quiet_NaN();break;
        case 18:input.identity.settings_revision=0;Stamp(input);break;
        case 19:input.identity.history_epoch=0;Stamp(input);break;
        case 20:input.identity.input_context=0;Stamp(input);break;
        case 21:input.identity.frame.native_stereo=false;Stamp(input);break;
        case 22:input.identity.frame.tracking_valid=false;Stamp(input);break;
        case 23:input.identity.frame.device=0x9999;Stamp(input);break;
        case 24:input.identity.frame.width=16;Stamp(input);break;
        case 25:input.identity.frame.quality=4;Stamp(input);break;
        case 26:input.identity.frame.output_width=96;Stamp(input);break; // Invalid DLAA extent.
        }
        Check(!f.session.Reconstruct(context,input),"invalid source/identity accepted");
        f.Empty(input,"invalid source exposed output");
        Check(f.executor->builds==0&&f.executor->generates==0&&f.executor->evaluations==0,"invalid pair reached GPU/helper");
    }
}
void SnapshotAndOrderingFences() {
    for(unsigned change=0;change<18;++change) {
        Fixture f;auto input=Frame();Check(f.session.Reconstruct(context,input),"snapshot setup failed");auto current=input;
        switch(change) {
        case 0:current.identity.frame.frame++;break;
        case 1:current.identity.frame.pose++;break;
        case 2:current.identity.frame.generation++;break;
        case 3:current.identity.frame.device++;break;
        case 4:current.identity.settings_revision++;break;
        case 5:current.identity.history_epoch++;break;
        case 6:current.identity.input_context++;break;
        case 7:current.eyes[1].input_token++;break;
        case 8:current.eyes[1].guides.color=Token<ID3D11Texture2D>(0x9999);break;
        case 9:current.eyes[1].guides.linear_depth_alpha=nullptr;break;
        case 10:current.eyes[1].guides.current_color_mask=nullptr;break;
        case 11:current.eyes[1].guides.jitter_y=.25F;break;
        case 12:current.eyes[1].unjittered_view_projection[12]+=.25F;break;
        case 13:current.eyes[1].guides.source_qualified=false;break;
        case 14:current.eyes[1].guides.near_metres=.25F;break;
        case 15:current.eyes[1].identity.settings_revision++;break;
        case 16:current.eyes[1].eye=0;break;
        case 17:current.eyes[1].guides.depth_encoding=DepthEncoding::standard_device_z;break;
        }
        f.Empty(current,"changed current snapshot exposed stale pair");
        f.Empty(input,"retired output resurrected through old snapshot");
    }
    Fixture f;auto old=Frame(8);Check(f.session.Reconstruct(context,old),"ordering setup failed");
    f.session.Invalidate();Check(!f.session.Reconstruct(context,Frame(7)),"invalidate allowed older frame");
    auto next=Frame(1);++next.identity.frame.generation;Stamp(next);
    Check(f.session.Reconstruct(context,next),"fresh generation could not restart frame numbering");
    Check(!f.session.Reconstruct(context,Frame(99)),"retired generation reclaimed session");
}
void ContextChangesResetBothEyes() {
    for(unsigned change=0;change<9;++change) {
        Fixture f;auto first=Frame();Check(f.session.Reconstruct(context,first),"context setup failed");auto next=Frame(2);
        switch(change) {
        case 0:next.identity.settings_revision++;break;
        case 1:next.identity.history_epoch++;break;
        case 2:next.identity.input_context++;break;
        case 3:next.identity.frame.owner++;break;
        case 4:next.identity.frame.generation++;next.identity.frame.frame=1;break;
        case 5:next.identity.frame.pose=1;break;
        case 6:next.identity.frame.frame=4;break;
        case 7:next.identity.frame.quality=1;next.identity.frame.output_width=96;break;
        case 8:f.session.Invalidate();break;
        }
        Stamp(next);Check(f.session.Reconstruct(context,next),"changed context could not start fresh history");
        Check(f.executor->evaluated[0].reset&&f.executor->evaluated[1].reset,"context change reused old history");
    }
    Fixture f;auto input=Frame();input.eyes[1].guides.depth_encoding=DepthEncoding::standard_device_z;
    Check(f.session.Reconstruct(context,input)&&f.executor->generated[1].depth_encoding==DepthEncoding::standard_device_z,
        "raw device-Z encoding lost before guide generation");
    f.session.Stop();f.Empty(input,"stop retained old output");Check(!f.session.Reconstruct(context,Frame(2)),"stopped session evaluated");
    Check(f.session.Start(device,L"C:/explicit-laboratory-helper.exe")&&f.session.Reconstruct(context,Frame()),"explicit restart failed");
}
}
int main()try {
    PairPublicationAndHistory();AtomicFailureRecovery();RejectInvalidSourcesBeforeWork();SnapshotAndOrderingFences();ContextChangesResetBothEyes();
    std::cout<<"DLSS paired session CPU policy: atomic publication, independent histories, source qualification, identity/snapshot fences and fault recovery passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
