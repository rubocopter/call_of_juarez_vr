#pragma once
#include "backends/d3d9/hdr_source_writes.hpp"
#include <d3d9.h>
#include <wrl/client.h>
#include <array>
#include <cmath>
#include <cstdint>

namespace cojvr::backends::d3d9 {
// A native effect's logical sampler number is not a D3D9 register. Only
// current device-owned bindings authorize dereferencing the candidate.
inline bool FindHdrAlphaSource(IDirect3DDevice9* device,std::uintptr_t identity,
    unsigned width,unsigned height,Microsoft::WRL::ComPtr<IDirect3DTexture9>& output,
    unsigned& sampler) noexcept {
    output.Reset();sampler=~0U;
    if(!device||!identity||!width||!height)return false;
    for(unsigned stage=0;stage<16;++stage) {
        Microsoft::WRL::ComPtr<IDirect3DBaseTexture9> bound;
        Microsoft::WRL::ComPtr<IDirect3DTexture9> texture;D3DSURFACE_DESC desc{};
        if(SUCCEEDED(device->GetTexture(stage,&bound))&&bound&&SUCCEEDED(bound.As(&texture))&&
           reinterpret_cast<std::uintptr_t>(texture.Get())==identity&&
           SUCCEEDED(texture->GetLevelDesc(0,&desc))&&desc.Width==width&&desc.Height==height&&
           desc.Format==D3DFMT_A16B16G16R16F) {
            output=std::move(texture);sampler=stage;return true;
        }
    }
    return false;
}
struct HdrAlphaSample {
    std::uint64_t frame=0,generation=0,scope=0,owner=0;
    std::uintptr_t device=0,source=0;
    std::uint64_t pose=0;
    std::uintptr_t target=0,shader=0;
    unsigned phase=0,sampler=0;
    std::uint64_t source_write_calls=0,source_alpha_write_calls=0;
    std::uintptr_t source_last_writer_shader=0;
    bool source_write_observation_complete=false;
    std::uint64_t source_copy_calls=0;
    std::uintptr_t source_copy_source=0;
    std::uint32_t source_copy_source_format=0;
    std::uint32_t source_copy_source_width=0,source_copy_source_height=0;
    std::uint64_t source_copy_write_call=0;
    bool source_copy_boundary_current=false;
    RECT source_copy_source_rect{},source_copy_destination_rect{};
    bool source_copy_rectangles_observed=false;
    HdrProducerWrites source_copy_producer{};
    bool source_copy_producer_complete=false;
    unsigned eye=0,width=0,height=0;
    std::array<float,256> alpha{};
    std::array<float,768> color{}; // Co-sampled HDR RGB; never exposed as reconstruction input.
};
// Bounded opt-in content observation. Caller must identify an already bound
// texture with the copied exact-build native 1B_16 descriptor before Queue.
// Captures 16x16 POINT samples, never rebinds/writes native scene state, never
// transports sampled data as a qualified temporal input.
class D3D9HdrAlphaProbe final {
    template<class T>using ComPtr=Microsoft::WRL::ComPtr<T>;
    ComPtr<IDirect3DDevice9> device_;
    std::array<ComPtr<IDirect3DSurface9>,6> target_,cpu_;
    std::array<ComPtr<IDirect3DQuery9>,6> query_;
    std::array<HdrAlphaSample,6> samples_{};
    unsigned capacity_=2;
    unsigned head_=0,count_=0;
    bool unavailable_=false;
    HRESULT result_=S_OK;
    bool Fail(HRESULT hr) noexcept {result_=hr;unavailable_=true;return false;}
public:
    explicit D3D9HdrAlphaProbe(unsigned capacity=2) noexcept:capacity_(capacity>=1&&capacity<=6?capacity:2){}
    static float Half(std::uint16_t value) noexcept {
        const int exponent=(value>>10)&31,mantissa=value&1023;float result;
        if(exponent==31)result=mantissa?NAN:INFINITY;
        else if(!exponent)result=std::ldexp(static_cast<float>(mantissa),-24);
        else result=std::ldexp(static_cast<float>(1024+mantissa),exponent-25);
        return(value&0x8000)?-result:result;
    }
    [[nodiscard]] bool Queue(IDirect3DDevice9* device,IDirect3DTexture9* source,HdrAlphaSample sample) noexcept {
        if(unavailable_||!device||!source||!sample.frame||!sample.generation||!sample.scope||!sample.owner||sample.eye>1)return false;
        if(count_==capacity_){result_=S_FALSE;return false;}
        ComPtr<IDirect3DDevice9> owner;source->GetDevice(&owner);D3DSURFACE_DESC desc{};
        if(owner.Get()!=device||FAILED(source->GetLevelDesc(0,&desc))||desc.Format!=D3DFMT_A16B16G16R16F||
           desc.Pool!=D3DPOOL_DEFAULT||!(desc.Usage&D3DUSAGE_RENDERTARGET)||!desc.Width||!desc.Height||
           desc.MultiSampleType!=D3DMULTISAMPLE_NONE||desc.Width>8192||desc.Height>8192)return false;
        if(device_&&device_.Get()!=device)return false;
        if(!device_) {
            device_=device;
            for(unsigned i=0;i<capacity_;++i) {
                HRESULT hr=device->CreateRenderTarget(16,16,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&target_[i],nullptr);
                if(SUCCEEDED(hr))hr=device->CreateOffscreenPlainSurface(16,16,desc.Format,D3DPOOL_SYSTEMMEM,&cpu_[i],nullptr);
                if(SUCCEEDED(hr))hr=device->CreateQuery(D3DQUERYTYPE_EVENT,&query_[i]);
                if(FAILED(hr))return Fail(hr);
            }
        }
        ComPtr<IDirect3DSurface9> surface;HRESULT hr=source->GetSurfaceLevel(0,&surface);if(FAILED(hr))return Fail(hr);
        const auto slot=(head_+count_)%capacity_;
        hr=device->StretchRect(surface.Get(),nullptr,target_[slot].Get(),nullptr,D3DTEXF_POINT);
        if(SUCCEEDED(hr))hr=query_[slot]->Issue(D3DISSUE_END);
        if(FAILED(hr))return Fail(hr);
        sample.device=reinterpret_cast<std::uintptr_t>(device);sample.source=reinterpret_cast<std::uintptr_t>(source);
        sample.width=desc.Width;sample.height=desc.Height;samples_[slot]=sample;++count_;result_=S_OK;return true;
    }
    [[nodiscard]] bool Poll(HdrAlphaSample& output) noexcept {
        if(unavailable_||!count_)return false;
        BOOL done=FALSE;const auto polled=query_[head_]->GetData(&done,sizeof(done),0);
        if(polled==S_FALSE||!done){if(FAILED(polled))Fail(polled);return false;}
        if(FAILED(polled))return Fail(polled);
        auto hr=device_->GetRenderTargetData(target_[head_].Get(),cpu_[head_].Get());if(FAILED(hr))return Fail(hr);
        D3DLOCKED_RECT locked{};hr=cpu_[head_]->LockRect(&locked,nullptr,D3DLOCK_READONLY);if(FAILED(hr))return Fail(hr);
        auto sample=samples_[head_];
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x) {
            const auto* row=reinterpret_cast<const std::uint16_t*>(static_cast<const unsigned char*>(locked.pBits)+y*locked.Pitch);
            sample.alpha[y*16+x]=Half(row[x*4+3]);
            for(unsigned channel=0;channel<3;++channel)sample.color[(y*16+x)*3+channel]=Half(row[x*4+channel]);
        }
        hr=cpu_[head_]->UnlockRect();if(FAILED(hr))return Fail(hr);
        output=sample;head_=(head_+1)%capacity_;--count_;result_=S_OK;return true;
    }
    void Reset() noexcept {
        for(auto& q:query_)q.Reset();for(auto& c:cpu_)c.Reset();for(auto& t:target_)t.Reset();
        device_.Reset();samples_={};head_=count_=0;unavailable_=false;result_=S_OK;
    }
    [[nodiscard]] HRESULT last_result() const noexcept{return result_;}
};
}
