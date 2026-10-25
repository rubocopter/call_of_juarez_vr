#include "backends/openvr/spatial_image_processor.hpp"
#include "backends/openvr/reticle_pixels.hpp"
#include "backends/d3d9/stereo_frame.hpp"
#include "runtime/scope_view.hpp"
#include <d3d11_1.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace cojvr::backends::openvr;
using cojvr::runtime::GraphicsSettings;
namespace {
void Require(bool condition,const char* message) { if(!condition)throw std::runtime_error(message); }
struct Gpu {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    Gpu() {
        Require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device");
    }
    ComPtr<ID3D11Texture2D> Texture(unsigned width,unsigned height,const std::vector<std::uint32_t>& pixels,
                                  DXGI_FORMAT format=DXGI_FORMAT_B8G8R8A8_UNORM,UINT misc=0) {
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=1;
        d.Format=format;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;d.MiscFlags=misc;
        D3D11_SUBRESOURCE_DATA data{pixels.data(),width*4,0};
        ComPtr<ID3D11Texture2D> texture;
        Require(SUCCEEDED(device->CreateTexture2D(&d,pixels.empty()?nullptr:&data,&texture)),"texture");return texture;
    }
    std::vector<std::uint32_t> Read(ID3D11Texture2D* texture) {
        D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);const auto width=d.Width,height=d.Height;
        d.Usage=D3D11_USAGE_STAGING;d.BindFlags=d.MiscFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;Require(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"staging");
        context->CopyResource(staging.Get(),texture);D3D11_MAPPED_SUBRESOURCE m{};
        Require(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)),"readback");
        std::vector<std::uint32_t> result(width*height);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)
            result[y*width+x]=reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(m.pData)+y*m.RowPitch)[x];
        context->Unmap(staging.Get(),0);return result;
    }
};
unsigned Channel(std::uint32_t pixel,unsigned shift){return(pixel>>shift)&255U;}
bool ScopePass(SpatialImageProcessor& processor,Gpu& gpu,const std::array<ID3D11Texture2D*,2>& eyes,
               const GraphicsSettings& options,bool scope=true) {
    return processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options,0.F,scope);
}
void ProjectedScopePixels() {
    // Mirrored off-centre physical optics, with different horizontal/vertical
    // FOV scales. A texture-centred pixel circle must fail this fixture.
    constexpr unsigned width=401,height=301;
    const std::array<cojvr::runtime::ScopeLens,2> lenses{{
        {true,128.5F/width,140.5F/height,.24F,.36F},
        {true,272.5F/width,160.5F/height,.28F,.30F}}};
    Gpu gpu;
    for(unsigned mode=0;mode<3;++mode) {
        std::array<std::vector<std::uint32_t>,2> input;
        std::array<ComPtr<ID3D11Texture2D>,2> textures,reference;
        std::array<ID3D11Texture2D*,2> pair{},reference_pair{};
        for(unsigned eye=0;eye<2;++eye) {
            auto& pixels=input[eye];pixels.resize(width*height);
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)
                pixels[y*width+x]=(((x+y+eye*67)%256)<<24)|
                    ((32+x%96)<<16)|((48+y%112)<<8)|(64+eye*32);
            textures[eye]=gpu.Texture(width,height,pixels);
            reference[eye]=gpu.Texture(width,height,pixels);
            pair[eye]=textures[eye].Get();reference_pair[eye]=reference[eye].Get();
        }
        GraphicsSettings options{};options.enabled=mode!=0;
        if(mode!=1){options.sharpness=1;options.edge_smoothing=true;options.gamma=1.7F;options.saturation=0;options.contrast=.6F;}
        SpatialImageProcessor processor,baseline;
        Require(baseline.Process(gpu.device.Get(),gpu.context.Get(),reference_pair,options),"scope world reference");
        Require(processor.Process(gpu.device.Get(),gpu.context.Get(),pair,options,.85F,true,&lenses),"projected scope GPU pass");
        std::array<std::vector<std::uint32_t>,2> first;
        for(unsigned eye=0;eye<2;++eye) {
            const auto out=gpu.Read(pair[eye]);const auto world=gpu.Read(reference_pair[eye]);
            const auto& lens=lenses[eye];const int cx=eye?272:128,cy=eye?160:140;
            const auto at=[&](int x,int y){return out[unsigned(y)*width+unsigned(x)];};
            Require((at(eye?140:235,cy)&0xFFFFFFU)==0,"fixed-centre circle leaks world outside off-centre projected lens");
            Require((at(cx,cy)&0xFFFFFFU)==0xFFFFFFU,"scope dot missed per-eye marker centre");
            Require((at(cx+16,cy)&0xFFFFFFU)==0xFFFFFFU,"scope thin hair missing after filters");
            Require((at(cx+16,cy+1)&0xFFFFFFU)==0,"scope hair needs a dark outline");
            Require(at(cx+16,cy+3)==world[unsigned(cy+3)*width+unsigned(cx+16)],"scope hair blurred into clear world");
            Require(at(cx+4,cy)==world[unsigned(cy)*width+unsigned(cx+4)],"scope sight must retain an open centre gap");
            Require(at(cx+14,cy+17)==world[unsigned(cy+17)*width+unsigned(cx+14)],"scope changed still-world lens interior");
            unsigned rim_white=0,rim_dark=0;
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
                const auto i=y*width+x;
                Require(Channel(out[i],24)==Channel(input[eye][i],24),"projected scope changed source alpha");
                const float dx=((float(x)+.5F)/width-lens.center_u)/lens.radius_u;
                const float dy=((float(y)+.5F)/height-lens.center_v)/lens.radius_v;
                const float distance=std::sqrt(dx*dx+dy*dy);
                if(distance>=1)Require((out[i]&0xFFFFFFU)==0,"projected aperture leaked outside physical-FOV ellipse");
                if(distance>.95F&&distance<1&&std::abs(dx)>.3F&&std::abs(dy)>.3F) {
                    if((out[i]&0xFFFFFFU)==0xFFFFFFU)++rim_white;
                    if((out[i]&0xFFFFFFU)==0)++rim_dark;
                }
            }
            Require(rim_white>0&&rim_dark>0,"scope rim needs a crisp light edge and dark outline");
            first[eye]=out;
            gpu.context->UpdateSubresource(pair[eye],0,nullptr,input[eye].data(),width*4,0);
        }
        Require(processor.Process(gpu.device.Get(),gpu.context.Get(),pair,options,0,true,&lenses)&&
            gpu.Read(pair[0])==first[0]&&gpu.Read(pair[1])==first[1]&&processor.scratch_allocations()==1,
            "projected scope fresh capture changed optics or allocated extra scratch");
        for(unsigned eye=0;eye<2;++eye)gpu.context->UpdateSubresource(pair[eye],0,nullptr,input[eye].data(),width*4,0);
        Require(processor.Process(gpu.device.Get(),gpu.context.Get(),pair,options,0,false,&lenses)&&
            gpu.Read(pair[0])==gpu.Read(reference_pair[0])&&gpu.Read(pair[1])==gpu.Read(reference_pair[1]),
            "projected scope exit retained reticle or mask on fresh world");
    }
    std::cout<<"Projected scope mirrored centres, ellipses, post-filter sight/rim, alpha, world interior and fresh exit passed\n";
}
void ScopeAdmission() {
    using cojvr::runtime::ScopeLens;
    cojvr::backends::d3d9::StereoReticleOverlay overlay{};
    overlay.frame_sequence=23;overlay.scope_view=true;
    auto& right=overlay.weapons[0];right.active=true;
    overlay.scope_lenses={{{true,.32F,.47F,.24F,.36F},{true,.68F,.53F,.28F,.30F}}};
    right.eyes={{{true,.32F,.47F},{true,.68000006F,.52999997F}}};
    Require(IsCurrentCenteredScopeReticle(overlay,23),"current mirrored off-centre physical lenses rejected");
    Require(!IsCurrentCenteredScopeReticle(overlay,24)&&!IsCurrentCenteredScopeReticle(overlay,0),"stale/zero scope capture admitted");
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    const std::array<ScopeLens,15> invalid{{
        {},{false,.5F,.5F,.2F,.2F},{true,nan,.5F,.2F,.2F},{true,.5F,inf,.2F,.2F},
        {true,.5F,.5F,nan,.2F},{true,.5F,.5F,.2F,inf},{true,.5F,.5F,0,.2F},{true,.5F,.5F,.2F,0},
        {true,.5F,.5F,-.1F,.2F},{true,.5F,.5F,.2F,-.1F},{true,.1F,.5F,.2F,.2F},
        {true,.5F,.1F,.2F,.2F},{true,.9F,.5F,.2F,.2F},{true,.5F,.9F,.2F,.2F},
        {true,.5F,.5F,inf,nan}}};
    Gpu gpu;
    const std::vector<std::uint32_t> a(10000,0x6A3979C1U),b(10000,0x91B7562DU);
    auto left=gpu.Texture(100,100,a),second=gpu.Texture(100,100,b);
    const std::array<ID3D11Texture2D*,2> pair{left.Get(),second.Get()};
    GraphicsSettings options{};SpatialImageProcessor processor;
    for(unsigned eye=0;eye<2;++eye) {
        const auto good_lens=overlay.scope_lenses[eye];
        for(const auto& bad_lens:invalid) {
            overlay.scope_lenses[eye]=bad_lens;
            Require(!IsCurrentCenteredScopeReticle(overlay,23),"incomplete/nonfinite/unfitted scope lens admitted");
            Require(!processor.Process(gpu.device.Get(),gpu.context.Get(),pair,options,0,true,&overlay.scope_lenses)&&
                processor.last_result()==E_INVALIDARG&&processor.scratch_allocations()==0&&
                gpu.Read(left.Get())==a&&gpu.Read(second.Get())==b,"invalid optical pair wrote or allocated before rejection");
        }
        overlay.scope_lenses[eye]=good_lens;
        const auto good_point=right.eyes[eye];
        right.eyes[eye].u+=.00005F;
        Require(IsCurrentCenteredScopeReticle(overlay,23),"marker tolerance rejected optical centre rounding");
        right.eyes[eye].u+=.0002F;
        Require(!IsCurrentCenteredScopeReticle(overlay,23),"marker detached from lens admitted");
        right.eyes[eye]=good_point;right.eyes[eye].v=nan;
        Require(!IsCurrentCenteredScopeReticle(overlay,23),"NaN scope marker admitted");
        right.eyes[eye]=good_point;right.eyes[eye].valid=false;
        Require(!IsCurrentCenteredScopeReticle(overlay,23),"partial scope marker pair admitted");
        right.eyes[eye]=good_point;
    }
    overlay.scope_view=false;
    Require(!IsCurrentCenteredScopeReticle(overlay,23),"inactive scope view admitted");overlay.scope_view=true;
    overlay.weapons[1]=right;right.active=false;
    Require(!IsCurrentCenteredScopeReticle(overlay,23),"left hand mistaken for scoped right-hand owner");right.active=true;
    // Projected optics use the same private pipeline state transaction as the
    // legacy pass, including caller input/output bindings and untouched CB slots.
    D3D11_VIEWPORT viewport{3,4,7,8,.2F,.8F};gpu.context->RSSetViewports(1,&viewport);
    gpu.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ComPtr<ID3D11ShaderResourceView> srv;ComPtr<ID3D11RenderTargetView> rtv;ComPtr<ID3D11Buffer> cb;
    D3D11_BUFFER_DESC cb_desc{};cb_desc.ByteWidth=64;cb_desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    Require(SUCCEEDED(gpu.device->CreateShaderResourceView(left.Get(),nullptr,&srv))&&
        SUCCEEDED(gpu.device->CreateRenderTargetView(second.Get(),nullptr,&rtv))&&
        SUCCEEDED(gpu.device->CreateBuffer(&cb_desc,nullptr,&cb)),"projected scope sentinel resources");
    auto* input=srv.Get();auto* output=rtv.Get();auto* constants=cb.Get();
    gpu.context->PSSetShaderResources(7,1,&input);gpu.context->OMSetRenderTargets(1,&output,nullptr);
    gpu.context->PSSetConstantBuffers(0,1,&constants);
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),pair,options,0,true,&overlay.scope_lenses),"projected scope with caller state");
    D3D11_PRIMITIVE_TOPOLOGY topology{};gpu.context->IAGetPrimitiveTopology(&topology);
    D3D11_VIEWPORT restored{};UINT count=1;gpu.context->RSGetViewports(&count,&restored);
    ComPtr<ID3D11ShaderResourceView> restored_srv;ComPtr<ID3D11RenderTargetView> restored_rtv;ComPtr<ID3D11Buffer> restored_cb;
    gpu.context->PSGetShaderResources(7,1,&restored_srv);gpu.context->OMGetRenderTargets(1,&restored_rtv,nullptr);
    gpu.context->PSGetConstantBuffers(0,1,&restored_cb);
    Require(topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST&&count==1&&restored.TopLeftX==3&&restored.Width==7&&
        restored.MinDepth==.2F&&restored_srv.Get()==srv.Get()&&restored_rtv.Get()==rtv.Get()&&restored_cb.Get()==cb.Get(),
        "projected scope failed caller pipeline restoration");
    gpu.context->ClearState();processor.Reset();
    Require(processor.scratch_allocations()==0,"projected scope reset retained scratch");
    overlay.scope_lenses={};
    gpu.context->UpdateSubresource(left.Get(),0,nullptr,a.data(),400,0);
    gpu.context->UpdateSubresource(second.Get(),0,nullptr,b.data(),400,0);
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),pair,options,0,false,&overlay.scope_lenses)&&
        gpu.Read(left.Get())==a&&gpu.Read(second.Get())==b&&processor.scratch_allocations()==0,
        "inactive scope validated stale lenses or failed neutral bypass");
    std::cout<<"Scope optical/marker admission, invalid-pair no-write/no-allocation, caller state and neutral exit passed\n";
}
void SubpixelScopeSight() {
    Gpu gpu;SpatialImageProcessor processor;GraphicsSettings options{};
    const std::vector<std::uint32_t> input(200*150,0xA5303030U);
    auto left=gpu.Texture(200,150,input),right=gpu.Texture(200,150,input);
    const std::array<ID3D11Texture2D*,2> pair{left.Get(),right.Get()};
    // Real projected markers can fall between pixel centres. Thin hairs must
    // retain antialiased coverage there instead of disappearing into the black.
    const std::array<cojvr::runtime::ScopeLens,2> lenses{{
        {true,.32F,.48F,.24F,.36F},{true,.68F,.52F,.28F,.30F}}};
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),pair,options,0,true,&lenses),"subpixel scope pass");
    for(unsigned eye=0;eye<2;++eye) {
        const auto out=gpu.Read(pair[eye]);const unsigned cx=eye?136:64,cy=eye?78:72;
        Require(Channel(out[cy*200+cx+16],0)>=110&&Channel(out[(cy-1)*200+cx+16],0)>=110,
            "scope white hair disappeared between pixel centres");
        Require(Channel(out[cy*200+cx],0)>=110,"scope dot disappeared between pixel centres");
        Require(out[(cy+3)*200+cx+16]==input[(cy+3)*200+cx+16],"subpixel sight bled into still world");
    }
    std::cout<<"Scope subpixel hair/dot antialias coverage passed\n";
}
void ScopePixels() {
    Gpu gpu;
    // Square, wide, tall, odd and single-row/column captures must describe the
    // same circle in native pixels, never an ellipse in normalized eye UVs.
    for(const auto size:{std::array<unsigned,2>{100,100},{200,100},{100,200},{113,79},{97,1},{1,97},{2804,2860}})
    for(bool enabled:{false,true}) {
        const auto width=size[0],height=size[1];
        std::vector<std::uint32_t> a(width*height),b(width*height);
        for(unsigned i=0;i<a.size();++i) {
            a[i]=((i%256U)<<24)|0x003979C1U;
            b[i]=(((i+91U)%256U)<<24)|0x00B7562DU;
        }
        auto left=gpu.Texture(width,height,a),right=gpu.Texture(width,height,b);
        std::array<ID3D11Texture2D*,2> pair{left.Get(),right.Get()};
        GraphicsSettings options{};options.enabled=enabled;
        // Latent color settings must stay off even when the scope needs a pass.
        if(!enabled){options.saturation=0;options.gamma=2;options.sharpness=1;options.edge_smoothing=true;}
        SpatialImageProcessor processor;
        Require(ScopePass(processor,gpu,pair,options),"scope GPU pass with color off/neutral");
        const auto l=gpu.Read(left.Get()),r=gpu.Read(right.Get());
        Require((l[0]&0xFFFFFFU)==0&&(r[0]&0xFFFFFFU)==0,"scope outside must be solid black with graphics off");
        const float radius=.42F*float((std::min)(width,height));
        unsigned clear=0,black=0,feather=0;
        for(unsigned i=0;i<a.size();++i) {
            const float x=float(i%width)+.5F-float(width)*.5F;
            const float y=float(i/width)+.5F-float(height)*.5F;
            const float distance=std::sqrt(x*x+y*y);
            Require(Channel(l[i],24)==Channel(a[i],24)&&Channel(r[i],24)==Channel(b[i],24),"scope changed source alpha");
            if(distance<=(std::max)(0.F,radius-1.F)) {
                ++clear;Require(l[i]==a[i]&&r[i]==b[i],"scope changed clear lens color or pixel radius");
            } else if(distance>=radius) {
                ++black;Require((l[i]&0xFFFFFFU)==0&&(r[i]&0xFFFFFFU)==0,"scope leaked color outside native-pixel circle");
            } else if((l[i]&0xFFFFFFU)!=0&&(l[i]&0xFFFFFFU)!=(a[i]&0xFFFFFFU))++feather;
        }
        const auto center=(height/2)*width+width/2;
        Require(l[center]==a[center]&&r[center]==b[center]&&black>0,"scope center or surround incorrect");
        if(width>1&&height>1)Require(clear>0&&feather>0,"scope aperture needs a clear interior and thin antialiased edge");
        Require(processor.scratch_allocations()==1,"scope allocates one pair-reused scratch");
        gpu.context->UpdateSubresource(left.Get(),0,nullptr,a.data(),width*4,0);
        gpu.context->UpdateSubresource(right.Get(),0,nullptr,b.data(),width*4,0);
        Require(ScopePass(processor,gpu,pair,options)&&gpu.Read(left.Get())==l&&gpu.Read(right.Get())==r&&
            processor.scratch_allocations()==1,"fresh repeated scope capture changed pixels or reallocated");
        gpu.context->UpdateSubresource(left.Get(),0,nullptr,a.data(),width*4,0);
        gpu.context->UpdateSubresource(right.Get(),0,nullptr,b.data(),width*4,0);
        Require(ScopePass(processor,gpu,pair,options,false)&&gpu.Read(left.Get())==a&&gpu.Read(right.Get())==b&&
            processor.scratch_allocations()==1,"scope exit failed exact neutral restoration on fresh capture");
    }

    const std::vector<std::uint32_t> a(10000,0x6A3979C1U),b(10000,0x91B7562DU);
    auto left=gpu.Texture(100,100,a),right=gpu.Texture(100,100,b);
    std::array<ID3D11Texture2D*,2> pair{left.Get(),right.Get()};
    GraphicsSettings options{};SpatialImageProcessor processor;
    // An invalid second eye may never leave a masked first eye for submission.
    auto smaller=gpu.Texture(50,50,std::vector<std::uint32_t>(2500,0xFFFFFFFF));
    pair[1]=smaller.Get();
    Require(!ScopePass(processor,gpu,pair,options)&&gpu.Read(left.Get())==a&&processor.scratch_allocations()==0,
        "scope mismatched pair wrote or allocated before validation");
    pair[1]=nullptr;Require(!ScopePass(processor,gpu,pair,options)&&gpu.Read(left.Get())==a,"scope null second eye wrote first");
    pair[1]=left.Get();Require(!ScopePass(processor,gpu,pair,options)&&gpu.Read(left.Get())==a,"scope aliased pair accepted");
    Gpu foreign;auto other=foreign.Texture(100,100,b);pair[1]=other.Get();
    Require(!ScopePass(processor,gpu,pair,options)&&gpu.Read(left.Get())==a,"scope foreign second eye wrote first");
    auto unsupported=gpu.Texture(100,100,b,DXGI_FORMAT_R8G8B8A8_UNORM);pair[1]=unsupported.Get();
    Require(!ScopePass(processor,gpu,pair,options)&&gpu.Read(left.Get())==a,"scope unsupported second eye wrote first");
    auto shared=gpu.Texture(100,100,b,DXGI_FORMAT_B8G8R8A8_UNORM,D3D11_RESOURCE_MISC_SHARED);pair[1]=shared.Get();
    Require(!ScopePass(processor,gpu,pair,options)&&gpu.Read(left.Get())==a,"scope shared endpoint wrote first");
    pair[1]=right.Get();
    options.gamma=std::numeric_limits<float>::quiet_NaN();
    Require(!ScopePass(processor,gpu,pair,options)&&gpu.Read(left.Get())==a,"scope invalid settings wrote first");
    options={};
    // Restore the caller's input AND output bindings, not only its viewport.
    D3D11_VIEWPORT viewport{3,4,7,8,.2F,.8F};gpu.context->RSSetViewports(1,&viewport);
    gpu.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ComPtr<ID3D11ShaderResourceView> srv;ComPtr<ID3D11RenderTargetView> rtv;
    Require(SUCCEEDED(gpu.device->CreateShaderResourceView(left.Get(),nullptr,&srv))&&
        SUCCEEDED(gpu.device->CreateRenderTargetView(right.Get(),nullptr,&rtv)),"scope sentinel views");
    auto* input=srv.Get();auto* output=rtv.Get();
    gpu.context->PSSetShaderResources(7,1,&input);gpu.context->OMSetRenderTargets(1,&output,nullptr);
    Require(ScopePass(processor,gpu,pair,options),"scope with borrowed caller state");
    D3D11_PRIMITIVE_TOPOLOGY topology{};gpu.context->IAGetPrimitiveTopology(&topology);
    D3D11_VIEWPORT restored{};UINT count=1;gpu.context->RSGetViewports(&count,&restored);
    ComPtr<ID3D11ShaderResourceView> restored_srv;ComPtr<ID3D11RenderTargetView> restored_rtv;
    gpu.context->PSGetShaderResources(7,1,&restored_srv);gpu.context->OMGetRenderTargets(1,&restored_rtv,nullptr);
    Require(topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST&&count==1&&restored.TopLeftX==3&&restored.Width==7&&
        restored.MinDepth==.2F&&restored_srv.Get()==srv.Get()&&restored_rtv.Get()==rtv.Get(),"scope failed caller state restoration");
    gpu.context->ClearState();processor.Reset();
    Require(processor.scratch_allocations()==0,"scope reset retained resources");
    auto other_right=foreign.Texture(100,100,a);pair={other.Get(),other_right.Get()};
    Require(ScopePass(processor,foreign,pair,options)&&processor.scratch_allocations()==1,"scope did not rebuild after device switch/reset");
    pair={left.Get(),right.Get()};
    gpu.context->UpdateSubresource(left.Get(),0,nullptr,a.data(),400,0);
    gpu.context->UpdateSubresource(right.Get(),0,nullptr,b.data(),400,0);
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),pair,options,.85F,true)&&processor.scratch_allocations()==1,
        "scope did not rebuild when moving back to a different device");
    const auto comfort_scope=gpu.Read(left.Get());
    Require(comfort_scope[50*100+85]==a[50*100+85],"comfort dimming darkened the clear scope lens");

    // Existing enabled color controls still compose with the scope; native
    // BGRX must be admitted without changing the RGB order or black surround.
    auto bgrx_left=gpu.Texture(100,100,a,DXGI_FORMAT_B8G8R8X8_UNORM);
    auto bgrx_right=gpu.Texture(100,100,b,DXGI_FORMAT_B8G8R8X8_UNORM);
    pair={bgrx_left.Get(),bgrx_right.Get()};options.enabled=true;options.saturation=0;
    Require(ScopePass(processor,gpu,pair,options),"scope with native BGRX color processing");
    const auto color_scope=gpu.Read(bgrx_left.Get());const auto center=color_scope[5050];
    Require((color_scope[0]&0xFFFFFFU)==0&&Channel(center,0)==Channel(center,8)&&Channel(center,8)==Channel(center,16)&&
        Channel(center,0)>0,"scope bypassed enabled color processing or BGRX black surround");
    std::cout<<"Scope centered pixel circles, black surround, color-off, alpha, pair admission, reuse/exit and state restoration passed\n";
}
}
int main() try {
    {
        Gpu warning_gpu;SpatialImageProcessor warning_processor;GraphicsSettings neutral{};
        const std::vector<std::uint32_t> scene(10000,0x7F3979C1U);
        auto a=warning_gpu.Texture(100,100,scene),b=warning_gpu.Texture(100,100,scene);
        const std::array<ID3D11Texture2D*,2> pair{a.Get(),b.Get()};
        const std::array<cojvr::runtime::ScopeLens,2> lenses{{
            {true,.325F,.475F,.3F,.3F},{true,.675F,.525F,.3F,.3F}}};
        for(bool warning:{false,true}){
            warning_gpu.context->UpdateSubresource(a.Get(),0,nullptr,scene.data(),400,0);
            warning_gpu.context->UpdateSubresource(b.Get(),0,nullptr,scene.data(),400,0);
            Require(warning_processor.Process(warning_gpu.device.Get(),warning_gpu.context.Get(),pair,neutral,0,true,&lenses,warning),
                "protected target scope pass unavailable");
            for(unsigned eye=0;eye<2;++eye){
                const auto pixels=warning_gpu.Read(pair[eye]);
                const auto center=unsigned(lenses[eye].center_v*100)*100+unsigned(lenses[eye].center_u*100);
                const auto color=pixels[center];
                Require(warning?(Channel(color,16)>Channel(color,8)+100&&Channel(color,16)>Channel(color,0)+100):
                    ((color&0xFFFFFFU)==0xFFFFFFU),"native protected-target warning disappeared in scope sight");
                Require(Channel(color,24)==127&&(pixels[0]&0xFFFFFFU)==0,
                    "scope warning altered alpha or repainted black surround");
            }
        }
    }
    SubpixelScopeSight();
    ProjectedScopePixels();
    ScopeAdmission();
    ScopePixels();
    Gpu gpu;SpatialImageProcessor processor;GraphicsSettings options{};
    constexpr unsigned n=16;
    std::vector<std::uint32_t> left(n*n),right(n*n);
    for(unsigned y=0;y<n;++y)for(unsigned x=0;x<n;++x) {
        left[y*n+x]=0x65000000U|((x*12+20)<<16)|((y*10+10)<<8)|40U;
        right[y*n+x]=0xA5000000U|(30U<<16)|((x*8+50)<<8)|(y*10+40);
    }
    auto l=gpu.Texture(n,n,left),r=gpu.Texture(n,n,right);
    std::array<ID3D11Texture2D*,2> eyes{l.Get(),r.Get()};
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options),"disabled bypass");
    Require(processor.scratch_allocations()==0&&gpu.Read(l.Get())==left&&gpu.Read(r.Get())==right,"disabled exact no-allocation bypass");
    options.enabled=true;
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options),"neutral bypass");
    Require(processor.scratch_allocations()==0&&gpu.Read(l.Get())==left,"neutral exact bypass");

    // Real shader output, independent eyes, alpha preservation and original input policy.
    options.saturation=0;
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options),"desaturate pair");
    auto gl=gpu.Read(l.Get()),gr=gpu.Read(r.Get());
    Require(gl!=left&&gr!=right&&gl!=gr,"both independent eye images processed");
    for(std::size_t i=0;i<gl.size();++i)for(const auto pixel:{gl[i],gr[i]})
        Require(std::abs(int(Channel(pixel,0))-int(Channel(pixel,8)))<=1&&
                std::abs(int(Channel(pixel,8))-int(Channel(pixel,16)))<=1,"saturation zero is grayscale");
    for(std::size_t i=0;i<gl.size();++i)
        Require((gl[i]&0xFF000000U)==(left[i]&0xFF000000U)&&
                (gr[i]&0xFF000000U)==(right[i]&0xFF000000U),"alpha preserved");
    Require(processor.scratch_allocations()==1,"one reused eye scratch");

    // Stateful caller with unusual viewport/topology and an output SRV outside slot zero.
    D3D11_VIEWPORT viewport{3,4,7,8,.2F,.8F};gpu.context->RSSetViewports(1,&viewport);
    gpu.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ComPtr<ID3D11ShaderResourceView> sentinel;Require(SUCCEEDED(gpu.device->CreateShaderResourceView(l.Get(),nullptr,&sentinel)),"sentinel SRV");
    auto* srv=sentinel.Get();gpu.context->PSSetShaderResources(7,1,&srv);
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options),"repeat new capture");
    Require(processor.scratch_allocations()==1,"stable-size scratch reused");
    D3D11_PRIMITIVE_TOPOLOGY topology{};gpu.context->IAGetPrimitiveTopology(&topology);
    D3D11_VIEWPORT restored{};UINT count=1;gpu.context->RSGetViewports(&count,&restored);
    ComPtr<ID3D11ShaderResourceView> restored_srv;gpu.context->PSGetShaderResources(7,1,&restored_srv);
    Require(topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST&&count==1&&restored.TopLeftX==3&&restored.Width==7&&
            restored.MinDepth==.2F&&restored_srv.Get()==sentinel.Get(),"complete caller pipeline restored");
    gpu.context->ClearState();

    // Pair validation is completed before touching the first eye.
    const auto before=gpu.Read(l.Get());
    auto undersized=gpu.Texture(8,8,std::vector<std::uint32_t>(64,0xFF123456));
    eyes[1]=undersized.Get();
    Require(!processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options)&&gpu.Read(l.Get())==before,"mismatched second eye leaves first untouched");
    eyes[1]=l.Get();Require(!processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options),"same-resource eye alias rejected");
    eyes[1]=r.Get();Gpu foreign;
    Require(!processor.Process(gpu.device.Get(),foreign.context.Get(),eyes,options),"foreign context rejected");
    auto f=foreign.Texture(n,n,right);eyes[1]=f.Get();
    Require(!processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options)&&gpu.Read(l.Get())==before,"foreign second eye rejected before write");
    eyes[1]=r.Get();options.gamma=std::numeric_limits<float>::quiet_NaN();
    Require(!processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options)&&gpu.Read(l.Get())==before,"nonfinite configuration rejected");

    // Sharpening leaves a uniform patch unchanged, including black/white borders.
    options={};options.enabled=true;options.sharpness=1;
    auto flat=gpu.Texture(n,n,std::vector<std::uint32_t>(n*n,0x61707070));
    auto black=gpu.Texture(n,n,std::vector<std::uint32_t>(n*n,0xFF000000));eyes={flat.Get(),black.Get()};
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options),"uniform sharpen");
    Require(gpu.Read(flat.Get())==std::vector<std::uint32_t>(n*n,0x61707070)&&
            gpu.Read(black.Get())==std::vector<std::uint32_t>(n*n,0xFF000000),"no flat-field or border halo");
    std::vector<std::uint32_t> edge(n*n,0xFF606060);edge[8*n+8]=0xFF909090;
    auto detail=gpu.Texture(n,n,edge),detail2=gpu.Texture(n,n,edge);eyes={detail.Get(),detail2.Get()};
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options),"detail sharpening");
    const auto sharpened=gpu.Read(detail.Get());
    Require(Channel(sharpened[8*n+8],0)>Channel(edge[8*n+8],0),"sharpen increases local detail contrast");

    // Edge smoothing reduces a diagonal staircase; colors stay in range by UNORM.
    for(unsigned y=0;y<n;++y)for(unsigned x=0;x<n;++x)edge[y*n+x]=x>=y?0xFFFFFFFF:0xFF000000;
    detail=gpu.Texture(n,n,edge);detail2=gpu.Texture(n,n,edge);eyes={detail.Get(),detail2.Get()};
    options.sharpness=0;options.edge_smoothing=true;
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options),"edge smoothing");
    const auto smooth=gpu.Read(detail.Get());
    Require(smooth[8*n+8]!=edge[8*n+8]&&smooth[0*n+15]==edge[0*n+15],"diagonal edge softened, flat field retained");

    // Color controls work separately from smoothing/sharpening and handle odd extents.
    options={};options.enabled=true;options.gamma=2;
    auto dark=gpu.Texture(7,5,std::vector<std::uint32_t>(35,0xFF404040));
    auto dark2=gpu.Texture(7,5,std::vector<std::uint32_t>(35,0xFF404040));eyes={dark.Get(),dark2.Get()};
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options),"odd extent gamma");
    const auto bright=gpu.Read(dark.Get());
    Require(bright.size()==35&&Channel(bright[17],0)>64&&Channel(bright[17],0)<200,"gamma brightens midtones");
    Require(processor.scratch_allocations()==2,"resize rebuilds scratch once");
    options.gamma=1;options.contrast=1.5F;
    dark=gpu.Texture(7,5,std::vector<std::uint32_t>(35,0xFF404040));
    dark2=gpu.Texture(7,5,std::vector<std::uint32_t>(35,0xFFC0C0C0));eyes={dark.Get(),dark2.Get()};
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options),"contrast pair");
    Require(Channel(gpu.Read(dark.Get())[17],0)<64&&Channel(gpu.Read(dark2.Get())[17],0)>192,"contrast separates dark and light");

    // Native X8R8G8B8 transport is a distinct admitted format, not an RGBA guess.
    dark=gpu.Texture(7,5,std::vector<std::uint32_t>(35,0x004080C0),DXGI_FORMAT_B8G8R8X8_UNORM);
    dark2=gpu.Texture(7,5,std::vector<std::uint32_t>(35,0x00804020),DXGI_FORMAT_B8G8R8X8_UNORM);
    eyes={dark.Get(),dark2.Get()};options.contrast=1;options.saturation=0;
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options),"native BGRX guides");
    const auto bgrx=gpu.Read(dark.Get())[17];
    Require(std::abs(int(Channel(bgrx,0))-int(Channel(bgrx,16)))<=1,"BGRX colors processed");
    const auto bgrx_before=gpu.Read(dark.Get());
    auto unsupported=gpu.Texture(7,5,std::vector<std::uint32_t>(35,0xFF404040),DXGI_FORMAT_R8G8B8A8_UNORM);
    eyes[1]=unsupported.Get();
    Require(!processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options)&&gpu.Read(dark.Get())==bgrx_before,"unsupported format rejects before first eye");
    ComPtr<ID3D11DeviceContext> deferred;
    Require(SUCCEEDED(gpu.device->CreateDeferredContext(0,&deferred)),"deferred fixture");eyes[1]=dark2.Get();
    Require(!processor.Process(gpu.device.Get(),deferred.Get(),eyes,options),"deferred context rejected");
    Require(processor.scratch_allocations()==3,"format change rebuilds scratch once");
    auto shared_left=gpu.Texture(n,n,left,DXGI_FORMAT_B8G8R8A8_UNORM,D3D11_RESOURCE_MISC_SHARED);
    auto shared_right=gpu.Texture(n,n,right,DXGI_FORMAT_B8G8R8A8_UNORM,D3D11_RESOURCE_MISC_SHARED);
    eyes={shared_left.Get(),shared_right.Get()};
    Require(!processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options)&&gpu.Read(shared_left.Get())==left,
        "shared endpoints rejected before write; COM identity does not establish allocation uniqueness");
    processor.Reset();Require(processor.scratch_allocations()==0,"reset drops owned resources");
    // Comfort is independent of optional sharpening/color controls.
    options={};options.sharpness=1;options.gamma=2;options.horse_comfort=2;
    dark=gpu.Texture(32,32,std::vector<std::uint32_t>(1024,0x7FC0C0C0));
    dark2=gpu.Texture(32,32,std::vector<std::uint32_t>(1024,0x7F808080));eyes={dark.Get(),dark2.Get()};
    Require(processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options,.85F),"horse comfort GPU pass");
    const auto comfort=gpu.Read(dark.Get());
    Require(Channel(comfort[16*32+16],0)==192&&Channel(comfort[0],0)<40&&Channel(comfort[0],24)==127,
        "horse comfort changed center/alpha or failed peripheral dimming");
    Require(!processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options,std::numeric_limits<float>::quiet_NaN()),
        "invalid comfort strength accepted");
    // Exercise each comfort choice on fresh captures, with the color switch
    // both off and on. A reused/dimmed fixture would conceal toggle failures.
    // A corner can be outside the lens-visible area. Regress the complete
    // mid-peripheral annuli in eye UV space, including the actual captured
    // 2804x2860 extent and wide/tall fixtures. Normalizing by each eye's extent
    // must keep the aperture in the same submitted eye region after resizing.
    for(const auto size:{std::array<unsigned,2>{256,256},{320,180},{181,321},{2804,2860}})
    for(bool enabled:{false,true}) {
        const auto width=size[0],height=size[1];
        const auto pixel_count=width*height;
        std::array<unsigned,3> corners{};
        std::array<std::array<double,3>,3> transmission{};
        for(unsigned mode=0;mode<3;++mode) {
            options={};options.enabled=enabled;options.horse_comfort=static_cast<std::uint8_t>(mode);
            const std::vector<std::uint32_t> a(pixel_count,0x7FC0C0C0),b(pixel_count,0x59808080);
            dark=gpu.Texture(width,height,a);dark2=gpu.Texture(width,height,b);eyes={dark.Get(),dark2.Get()};
            const float strength=mode==1?.55F:mode==2?.85F:0.F;
            Require(processor.Process(gpu.device.Get(),gpu.context.Get(),eyes,options,strength),"comfort choice GPU pass");
            const auto left_pixels=gpu.Read(dark.Get()),right_pixels=gpu.Read(dark2.Get());
            corners[mode]=Channel(left_pixels[0],0);
            std::array<unsigned,3> samples{};
            std::array<double,3> right_transmission{};
            for(unsigned i=0;i<pixel_count;++i) {
                Require(Channel(left_pixels[i],24)==127&&Channel(right_pixels[i],24)==89,"comfort changed either eye alpha");
                const float x=2.F*(float(i%width)+.5F)/float(width)-1.F;
                const float y=2.F*(float(i/width)+.5F)/float(height)-1.F;
                const float radius=std::sqrt(x*x+y*y);
                if(radius<=.25F)Require(left_pixels[i]==a[i]&&right_pixels[i]==b[i],"comfort changed the clear central aperture");
                for(unsigned ring=0;ring<3;++ring) {
                    const float center=.5F+.1F*float(ring);
                    if(std::abs(radius-center)>.02F)continue;
                    ++samples[ring];transmission[mode][ring]+=double(Channel(left_pixels[i],0))/192.;
                    right_transmission[ring]+=double(Channel(right_pixels[i],0))/128.;
                }
            }
            for(unsigned ring=0;ring<3;++ring) {
                Require(samples[ring]>0,"annulus fixture has no samples");
                transmission[mode][ring]/=samples[ring];right_transmission[ring]/=samples[ring];
                Require(std::abs(transmission[mode][ring]-right_transmission[ring])<.01,
                    "comfort aperture differs between eyes");
            }
            if(mode==0)Require(left_pixels==a&&right_pixels==b,"Off did not preserve fresh native pixels");
            else Require(Channel(right_pixels[0],0)<128,"comfort missed right-eye periphery");
            // The presenter supplies zero strength after the native motion lease
            // expires (stationary/dismounted/unavailable). The selected setting
            // alone must never dim a fresh capture or allocate processor state.
            dark=gpu.Texture(width,height,a);dark2=gpu.Texture(width,height,b);eyes={dark.Get(),dark2.Get()};
            SpatialImageProcessor idle;
            Require(idle.Process(gpu.device.Get(),gpu.context.Get(),eyes,options,0)&&
                idle.scratch_allocations()==0&&gpu.Read(dark.Get())==a&&gpu.Read(dark2.Get())==b,
                "no-motion comfort must retain exact native identity without resources");
        }
        Require(corners[0]==192&&corners[1]>=85&&corners[1]<=87&&corners[2]>=28&&corners[2]<=30,
            "Off/Mild/Strong did not produce distinct expected peripheral pixels");
        std::cout<<"Comfort "<<width<<'x'<<height<<" color="<<enabled<<" transmission r=.50/.60/.70: mild=";
        for(const auto value:transmission[1])std::cout<<value<<' ';
        std::cout<<"strong=";for(const auto value:transmission[2])std::cout<<value<<' ';std::cout<<'\n';
        Require(transmission[1][0]>.80&&transmission[1][0]<.95&&
                transmission[1][1]>.66&&transmission[1][1]<.79&&transmission[1][2]<.55,
            "Mild aperture does not reach the visible mid-periphery");
        Require(transmission[2][0]<.45&&transmission[2][1]<.20&&transmission[2][2]<.17,
            "Strong aperture does not form a meaningful visible tunnel");
        for(unsigned ring=0;ring<3;++ring)
            Require(transmission[0][ring]==1&&transmission[1][ring]-transmission[2][ring]>.30,
                "Off/Mild/Strong do not separate across the visible annulus");
    }
    std::cout<<"Spatial stereo GPU pixels, neutral bypass, independent eyes, alpha, pipeline restoration, pair admission and reuse passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
