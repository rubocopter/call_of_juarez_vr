#include "backends/openvr/depth_mask_history.hpp"

namespace cojvr::backends::openvr::dlss {
namespace {
bool Same(const PairIdentity& a,const PairIdentity& b) noexcept {
    const auto& x=a.frame;const auto& y=b.frame;
    return x.device==y.device&&x.generation==y.generation&&x.frame==y.frame&&x.pose==y.pose&&x.owner==y.owner&&
        x.width==y.width&&x.height==y.height&&x.output_width==y.output_width&&x.output_height==y.output_height&&
        x.quality==y.quality&&x.native_stereo==y.native_stereo&&x.tracking_valid==y.tracking_valid&&
        a.settings_revision==b.settings_revision&&a.history_epoch==b.history_epoch&&a.input_context==b.input_context;
}
bool Texture(ID3D11Device* device,ID3D11Texture2D* texture,unsigned width,unsigned height,DXGI_FORMAT format) noexcept {
    if(!texture)return false;D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
    Microsoft::WRL::ComPtr<ID3D11Device> owner;texture->GetDevice(&owner);
    return owner.Get()==device&&d.Width==width&&d.Height==height&&d.Format==format&&d.ArraySize==1&&
        d.MipLevels==1&&d.SampleDesc.Count==1&&!d.MiscFlags&&(d.BindFlags&D3D11_BIND_SHADER_RESOURCE);
}
bool Context(ID3D11Device* device,ID3D11DeviceContext* context) noexcept {
    if(!device||!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
    Microsoft::WRL::ComPtr<ID3D11Device> owner;context->GetDevice(&owner);return owner.Get()==device;
}
}
void DepthMaskHistory::Invalidate() noexcept {
    have_previous_=have_pending_=false;previous_={};pending_={};pending_depths_={};
}
void DepthMaskHistory::Reset() noexcept {
    Invalidate();mask_.Reset();depths_={};white_={};white_views_={};owner_.Reset();width_=height_=0;
}
bool DepthMaskHistory::Resources(ID3D11Device* device,unsigned width,unsigned height) noexcept {
    if(owner_.Get()!=device||width_!=width||height_!=height){Reset();owner_=device;}
    if(depths_[1]&&white_views_[1])return true;
    for(unsigned eye=0;eye<2;++eye) {
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R32_FLOAT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        result_=device->CreateTexture2D(&d,nullptr,depths_[eye].ReleaseAndGetAddressOf());if(FAILED(result_))return false;
        d.Format=DXGI_FORMAT_R8_UNORM;d.BindFlags|=D3D11_BIND_RENDER_TARGET;
        result_=device->CreateTexture2D(&d,nullptr,white_[eye].ReleaseAndGetAddressOf());if(FAILED(result_))return false;
        result_=device->CreateRenderTargetView(white_[eye].Get(),nullptr,white_views_[eye].ReleaseAndGetAddressOf());if(FAILED(result_))return false;
    }
    width_=width;height_=height;return true;
}
bool DepthMaskHistory::Prepare(ID3D11Device* device,ID3D11DeviceContext* context,const PairFrame& frame,
    const std::array<Inputs,2>& inputs,std::array<ID3D11Texture2D*,2>& masks) noexcept {
    masks={};result_=E_INVALIDARG;
    const auto fail=[&]() noexcept {Invalidate();return false;};
    const auto& f=frame.identity.frame;
    runtime::TemporalHistory admission;bool reset=true;
    if(have_previous_)admission.Commit(previous_.identity.frame);
    if(have_pending_||!frame.depth_mask.enabled||!ValidDepthMaskPolicy(frame.depth_mask)||!Context(device,context)||
        f.device!=reinterpret_cast<std::uintptr_t>(device)||!frame.identity.settings_revision||
        !frame.identity.history_epoch||!frame.identity.input_context||!admission.Admit(f,reset)||
        f.width>8192||f.height>8192||std::uint64_t(f.width)*f.height>16ULL*1024*1024)return fail();
    Snapshot snapshot;snapshot.identity=frame.identity;snapshot.policy=frame.depth_mask;
    const bool use_history=frame.eyes[0].guides.history;
    if(use_history!=frame.eyes[1].guides.history)return fail();
    for(unsigned eye=0;eye<2;++eye) {
        const auto& e=frame.eyes[eye];const auto& g=e.guides;
        if(!Same(e.identity,frame.identity)||e.eye!=eye||!e.input_token||!g.source_qualified||
            !std::isfinite(g.near_metres)||!std::isfinite(g.far_metres)||g.near_metres<=0||g.far_metres<=g.near_metres||
            !std::isfinite(g.jitter_x)||!std::isfinite(g.jitter_y)||std::abs(g.jitter_x)>.5F||std::abs(g.jitter_y)>.5F||
            (g.depth_encoding!=DepthEncoding::axial_metres&&g.depth_encoding!=DepthEncoding::standard_device_z&&
                g.depth_encoding!=DepthEncoding::packed_device_z_rgb24)||
            (frame.depth_mask.infinite_far[eye]&&g.depth_encoding==DepthEncoding::axial_metres)||
            !Texture(device,inputs[eye].depth,f.width,f.height,DXGI_FORMAT_R32_FLOAT)||
            !Texture(device,inputs[eye].bias,f.width,f.height,DXGI_FORMAT_R8_UNORM))return fail();
        for(const auto& own:depths_)if(own.Get()==inputs[eye].depth)return fail();
        for(const auto& own:white_)if(own.Get()==inputs[eye].bias)return fail();
        for(unsigned owned_eye=0;owned_eye<2;++owned_eye)if(mask_.EyeMask(owned_eye)==inputs[eye].bias)return fail();
        runtime::TemporalMatrix inverse;
        if(!runtime::TemporalReprojection(e.unjittered_view_projection,runtime::TemporalIdentity(),inverse))return fail();
        snapshot.eyes[eye]={e.unjittered_view_projection,e.input_token,g.near_metres,g.far_metres,g.jitter_x,g.jitter_y};
    }
    if(use_history&&(!have_previous_||reset||frame.depth_mask!=previous_.policy||
        frame.identity.settings_revision!=previous_.identity.settings_revision||
        frame.identity.history_epoch!=previous_.identity.history_epoch||frame.identity.input_context!=previous_.identity.input_context))return fail();
    if(!Resources(device,f.width,f.height))return fail();
    std::array<ID3D11Texture2D*,2> result;
    if(use_history) {
        std::array<DepthHistoryMaskSource,2> sources;
        for(unsigned eye=0;eye<2;++eye) {
            const auto& now=snapshot.eyes[eye];const auto& old=previous_.eyes[eye];auto& s=sources[eye];
            s.depth=inputs[eye].depth;s.previous_depth=depths_[eye].Get();s.exclusions=inputs[eye].bias;
            s.current_token=now.token;s.previous_token=old.token;s.jitter_x=now.jitter_x;s.jitter_y=now.jitter_y;
            s.previous_jitter_x=old.jitter_x;s.previous_jitter_y=old.jitter_y;
            s.previous_near_metres=old.near_metres;s.previous_far_metres=old.far_metres;
            s.previous_infinite_far=previous_.policy.infinite_far[eye];s.relative_tolerance=frame.depth_mask.relative_tolerance;
            s.erosion_radius=frame.depth_mask.erosion_radius;s.source_qualified=true;
            if(!runtime::TemporalReprojection(now.matrix,old.matrix,s.clip_to_previous))return fail();
        }
        if(!mask_.Generate(device,context,frame.identity,previous_.identity,sources)){result_=mask_.last_result();return fail();}
        for(unsigned eye=0;eye<2;++eye)result[eye]=mask_.EyeMask(eye);
    }else {
        const float white[4]{1,1,1,1};
        for(unsigned eye=0;eye<2;++eye){context->ClearRenderTargetView(white_views_[eye].Get(),white);result[eye]=white_[eye].Get();}
    }
    result_=device->GetDeviceRemovedReason();if(FAILED(result_))return fail();
    for(unsigned eye=0;eye<2;++eye)pending_depths_[eye]=inputs[eye].depth;
    pending_=snapshot;have_pending_=true;masks=result;return true;
}
bool DepthMaskHistory::Commit(ID3D11DeviceContext* context) noexcept {
    result_=E_INVALIDARG;
    if(!have_pending_)return false;
    if(!Context(owner_.Get(),context)){Invalidate();return false;}
    for(unsigned eye=0;eye<2;++eye)context->CopyResource(depths_[eye].Get(),pending_depths_[eye].Get());
    result_=owner_->GetDeviceRemovedReason();if(FAILED(result_)){Invalidate();return false;}
    previous_=pending_;have_previous_=true;pending_={};pending_depths_={};have_pending_=false;return true;
}
}
