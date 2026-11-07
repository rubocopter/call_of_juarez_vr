#include "backends/openvr/dlss_pair_session.hpp"
#include <cmath>
#include <utility>

namespace cojvr::backends::openvr::dlss {
namespace {
bool SameFrame(const runtime::TemporalFrameIdentity& a,const runtime::TemporalFrameIdentity& b) noexcept {
    return a.device==b.device&&a.generation==b.generation&&a.frame==b.frame&&a.pose==b.pose&&a.owner==b.owner&&
        a.width==b.width&&a.height==b.height&&a.output_width==b.output_width&&a.output_height==b.output_height&&
        a.quality==b.quality&&a.native_stereo==b.native_stereo&&a.tracking_valid==b.tracking_valid;
}
bool SameIdentity(const PairIdentity& a,const PairIdentity& b) noexcept {
    return SameFrame(a.frame,b.frame)&&a.settings_revision==b.settings_revision&&
        a.history_epoch==b.history_epoch&&a.input_context==b.input_context;
}
bool SameSnapshot(const PairFrame& a,const PairFrame& b) noexcept {
    if(!SameIdentity(a.identity,b.identity))return false;
    for(unsigned eye=0;eye<2;++eye) {
        const auto& x=a.eyes[eye];const auto& y=b.eyes[eye];const auto& s=x.guides;const auto& t=y.guides;
        if(!SameIdentity(x.identity,y.identity)||x.input_token!=y.input_token||x.eye!=y.eye||
           x.unjittered_view_projection!=y.unjittered_view_projection||s.color!=t.color||
           s.linear_depth_alpha!=t.linear_depth_alpha||s.current_color_mask!=t.current_color_mask||
           s.near_metres!=t.near_metres||s.far_metres!=t.far_metres||s.jitter_x!=t.jitter_x||s.jitter_y!=t.jitter_y||
           s.source_qualified!=t.source_qualified||s.depth_encoding!=t.depth_encoding)return false;
        // history and clip_to_previous are session outputs, never caller inputs.
    }
    return true;
}
bool SameFeature(const runtime::TemporalFrameIdentity& a,const runtime::TemporalFrameIdentity& b) noexcept {
    return a.device==b.device&&a.generation==b.generation&&a.width==b.width&&a.height==b.height&&
        a.output_width==b.output_width&&a.output_height==b.output_height&&a.quality==b.quality;
}
bool Complete(const Inputs& input) noexcept {
    return input.color&&input.depth&&input.motion&&input.bias&&std::isfinite(input.jitter_x)&&
        std::isfinite(input.jitter_y)&&std::abs(input.jitter_x)<=.5F&&std::abs(input.jitter_y)<=.5F;
}
class ClientExecutor final:public PairExecutor {
    Client client_;
    TemporalGuides guides_;
    bool guide_error_=false;
public:
    bool Start(ID3D11Device* device,const std::filesystem::path& helper) noexcept override {
        guide_error_=false;return client_.Start(device,helper);
    }
    bool Build(unsigned eye,const runtime::TemporalFrameIdentity& f) noexcept override {
        guide_error_=false;return client_.Build(eye,f.width,f.height,f.output_width,f.output_height,static_cast<Quality>(f.quality));
    }
    bool Generate(ID3D11Device* device,ID3D11DeviceContext* context,const std::array<GuideSource,2>& sources,
                  std::array<Inputs,2>& result) noexcept override {
        result={};guide_error_=true;
        // TemporalGuides checks equal source sizes; also require the declared
        // feature extent before any helper input copy or first-eye evaluation.
        if(!guides_.Generate(device,context,sources))return false;
        for(unsigned eye=0;eye<2;++eye)result[eye]=guides_.EyeInputs(eye);
        guide_error_=false;return true;
    }
    bool Evaluate(ID3D11DeviceContext* context,unsigned eye,const Inputs& input) noexcept override {
        guide_error_=false;return client_.Evaluate(context,eye,input);
    }
    ID3D11Texture2D* Output(unsigned eye) const noexcept override {return client_.Output(eye);}
    std::string_view last_error() const noexcept override {
        return guide_error_?"DLSS temporal guide generation failed":client_.last_error();
    }
    void Stop() noexcept override {client_.Stop();guides_.Reset();}
};
}
struct PairSession::Impl {
    std::unique_ptr<PairExecutor> executor;
    ID3D11Device* device=nullptr; // executor retains actual COM ownership
    std::array<runtime::TemporalHistory,2> history;
    PairIdentity previous{},last_attempt{};
    std::array<runtime::TemporalMatrix,2> matrices{};
    runtime::TemporalFrameIdentity feature{};
    PairFrame published{};
    std::array<ID3D11Texture2D*,2> outputs{};
    std::uint64_t invalidation=0;
    bool started=false,built=false,have_previous=false,have_attempt=false,busy=false,executor_error=false;
    const char* error="DLSS pair session not started";
    void Invalidate() noexcept {
        outputs={};published={};have_previous=false;
        for(auto& eye:history)eye.Invalidate();
        ++invalidation;
    }
    bool Fail(const char* reason,bool from_executor=false) noexcept {
        Invalidate();error=reason;executor_error=from_executor;return false;
    }
};
PairSession::PairSession():PairSession(std::make_unique<ClientExecutor>()) {}
PairSession::PairSession(std::unique_ptr<PairExecutor> executor):impl_(std::make_unique<Impl>()) {
    impl_->executor=std::move(executor);
}
PairSession::~PairSession() {Stop();}
bool PairSession::Start(ID3D11Device* device,const std::filesystem::path& helper) noexcept {
    auto& p=*impl_;
    if(p.busy)return p.Fail("DLSS pair session is busy");
    Stop();
    if(!p.executor||!device||!helper.is_absolute())return p.Fail("DLSS pair missing device/executor/absolute helper path");
    if(!p.executor->Start(device,helper)) {
        p.executor->Stop();return p.Fail("DLSS pair helper startup failed",true);
    }
    p.device=device;p.started=true;p.error="";p.executor_error=false;return true;
}
bool PairSession::Reconstruct(ID3D11DeviceContext* context,const PairFrame& input,bool* history_reused) noexcept {
    if(history_reused)*history_reused=false;
    auto& p=*impl_;
    if(p.busy)return p.Fail("DLSS pair recursive reconstruction rejected");
    // Remove prior publication before any operation that can overwrite one eye.
    p.outputs={};p.published={};p.error="";p.executor_error=false;
    if(!p.started||!context)return p.Fail("DLSS pair missing running session/context");
    const PairFrame frame=input;
    const auto& identity=frame.identity;const auto& f=identity.frame;
    if(f.device!=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p.device))||
       !identity.settings_revision||!identity.history_epoch||!identity.input_context)
        return p.Fail("DLSS pair missing or foreign identity");
    Request extent;extent.width=f.width;extent.height=f.height;extent.out_width=f.output_width;
    extent.out_height=f.output_height;extent.quality=static_cast<Quality>(f.quality);
    if(!ValidExtent(extent)||std::uint64_t(f.width)*f.height>16ULL*1024*1024)
        return p.Fail("DLSS pair invalid feature extent/quality");
    if(p.have_attempt&&(f.generation<p.last_attempt.frame.generation||
       (f.generation==p.last_attempt.frame.generation&&f.frame<=p.last_attempt.frame.frame)))
        return p.Fail("DLSS pair repeated/stale frame or retired generation");
    std::array<bool,2> reset{};
    std::array<GuideSource,2> sources{};
    for(unsigned eye=0;eye<2;++eye) {
        const auto& source=frame.eyes[eye];const auto& s=source.guides;
        if(source.eye!=eye||!source.input_token||!SameIdentity(identity,source.identity)||
           !s.source_qualified||!s.color||!s.linear_depth_alpha||!s.current_color_mask||
           !std::isfinite(s.near_metres)||!std::isfinite(s.far_metres)||s.near_metres<=0||s.far_metres<=s.near_metres||
           !std::isfinite(s.jitter_x)||!std::isfinite(s.jitter_y)||std::abs(s.jitter_x)>.5F||std::abs(s.jitter_y)>.5F||
           (s.depth_encoding!=DepthEncoding::axial_metres&&s.depth_encoding!=DepthEncoding::standard_device_z&&
            s.depth_encoding!=DepthEncoding::packed_device_z_rgb24))
            return p.Fail("DLSS pair unqualified/incomplete guide source or mismatched eye identity");
        // Validate even the first/reset matrix: a reset cannot admit singular or
        // nonfinite geometry merely because this frame does not use old pixels.
        runtime::TemporalMatrix inverse_check{};
        if(!runtime::TemporalReprojection(source.unjittered_view_projection,runtime::TemporalIdentity(),inverse_check)||
           !p.history[eye].Admit(f,reset[eye]))return p.Fail("DLSS pair invalid matrix/history identity");
        reset[eye]=reset[eye]||!p.have_previous||identity.settings_revision!=p.previous.settings_revision||
            identity.history_epoch!=p.previous.history_epoch||identity.input_context!=p.previous.input_context;
        sources[eye]=s;sources[eye].history=!reset[eye];sources[eye].clip_to_previous=runtime::TemporalIdentity();
        if(!reset[eye]&&!runtime::TemporalReprojection(source.unjittered_view_projection,p.matrices[eye],sources[eye].clip_to_previous))
            return p.Fail("DLSS pair reprojection rejected");
    }
    // A failed/cancelled attempt consumes its ordering identity as well. It may
    // never be retried with changed inputs or regress to a prior generation.
    p.last_attempt=identity;p.have_attempt=true;
    p.busy=true;
    struct Exit {bool& busy;~Exit(){busy=false;}} exit{p.busy};
    const auto invalidation=p.invalidation;
    if(!p.built||!SameFeature(p.feature,f)) {
        p.built=false;
        if(!p.executor->Build(0,f)||!p.executor->Build(1,f))return p.Fail("DLSS pair feature build failed",true);
        p.feature=f;p.built=true;
    }
    if(p.invalidation!=invalidation)return p.Fail("DLSS pair cancelled during feature build");
    std::array<Inputs,2> guides{};
    if(!p.executor->Generate(p.device,context,sources,guides))return p.Fail("DLSS pair guide generation failed",true);
    if(!Complete(guides[0])||!Complete(guides[1]))return p.Fail("DLSS pair incomplete generated guides");
    if(p.invalidation!=invalidation)return p.Fail("DLSS pair cancelled during guide generation");
    for(unsigned eye=0;eye<2;++eye) {
        // Policy owns these values; an executor cannot silently reuse history
        // after a reset or evaluate a different sampling offset.
        guides[eye].reset=reset[eye];guides[eye].jitter_x=sources[eye].jitter_x;guides[eye].jitter_y=sources[eye].jitter_y;
        if(!p.executor->Evaluate(context,eye,guides[eye]))return p.Fail("DLSS pair eye evaluation failed",true);
        if(p.invalidation!=invalidation)return p.Fail("DLSS pair cancelled during evaluation");
    }
    const std::array<ID3D11Texture2D*,2> outputs{p.executor->Output(0),p.executor->Output(1)};
    if(!outputs[0]||!outputs[1]||outputs[0]==outputs[1])return p.Fail("DLSS pair missing or aliased output");
    // No fallible work after the transaction commits. Each eye's matrix and
    // feature history advance only here, after the complete pair is available.
    for(unsigned eye=0;eye<2;++eye) {
        p.history[eye].Commit(f);p.matrices[eye]=frame.eyes[eye].unjittered_view_projection;
    }
    p.previous=identity;p.have_previous=true;p.published=frame;p.outputs=outputs;
    if(history_reused)*history_reused=!reset[0]&&!reset[1];
    return true;
}
std::array<ID3D11Texture2D*,2> PairSession::Outputs(const PairFrame& current) noexcept {
    auto& p=*impl_;
    if(!p.outputs[0]||!p.outputs[1])return {};
    if(!SameSnapshot(current,p.published)) {p.Fail("DLSS pair current snapshot changed");return {};}
    return p.outputs;
}
void PairSession::Invalidate() noexcept {impl_->Invalidate();}
void PairSession::Stop() noexcept {
    auto& p=*impl_;
    if(p.busy) {p.Fail("DLSS pair stop deferred during reconstruction");return;}
    p.Invalidate();p.started=p.built=p.have_attempt=false;p.device=nullptr;
    if(p.executor)p.executor->Stop();
}
std::string_view PairSession::last_error() const noexcept {
    const auto& p=*impl_;
    if(p.executor_error&&p.executor) {
        const auto detail=p.executor->last_error();if(!detail.empty())return detail;
    }
    return p.error;
}
}
