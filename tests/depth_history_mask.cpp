#include "backends/openvr/depth_history_mask.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace cojvr::backends::openvr::dlss;
using Microsoft::WRL::ComPtr;
namespace {
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
ComPtr<ID3D11Texture2D> Make(ID3D11Device* device,DXGI_FORMAT format,const void* data,UINT pitch) {
    D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=16;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
    d.Format=format;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{data,pitch,0};ComPtr<ID3D11Texture2D> texture;
    Check(SUCCEEDED(device->CreateTexture2D(&d,&initial,&texture)),"fixture texture");return texture;
}
std::vector<unsigned char> Read(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* input) {
    Check(input!=nullptr,"missing mask output");D3D11_TEXTURE2D_DESC d{};input->GetDesc(&d);
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=d.MiscFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"staging");
    context->CopyResource(staging.Get(),input);D3D11_MAPPED_SUBRESOURCE map{};
    Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)),"map");
    std::vector<unsigned char> out(256);
    for(unsigned y=0;y<16;++y)std::memcpy(out.data()+y*16,static_cast<const unsigned char*>(map.pData)+y*map.RowPitch,16);
    context->Unmap(staging.Get(),0);return out;
}
}
int main()try {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_SINGLETHREADED,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP");
    std::vector<float> current(256,.5F),previous(256,.5F);std::vector<unsigned char> exclusions(256,0);
    for(unsigned y=5;y<=8;++y)for(unsigned x=5;x<=8;++x)current[y*16+x]=.25F;
    for(unsigned y=5;y<=8;++y)for(unsigned x=3;x<=6;++x)previous[y*16+x]=.25F;
    exclusions[12*16+12]=255;exclusions[11*16+11]=128;
    auto now=Make(device.Get(),DXGI_FORMAT_R32_FLOAT,current.data(),64);
    auto old=Make(device.Get(),DXGI_FORMAT_R32_FLOAT,previous.data(),64);
    auto excluded=Make(device.Get(),DXGI_FORMAT_R8_UNORM,exclusions.data(),16);
    std::array<DepthHistoryMaskSource,2> sources{};
    for(unsigned eye=0;eye<2;++eye) {
        auto& s=sources[eye];s.depth=now.Get();s.previous_depth=old.Get();s.exclusions=excluded.Get();
        s.current_token=100+eye;s.previous_token=90+eye;s.previous_near_metres=.5F;s.previous_far_metres=100;
        s.relative_tolerance=.05F;s.source_qualified=true;
    }
    PairIdentity before{{reinterpret_cast<std::uintptr_t>(device.Get()),1,10,20,30,16,16,16,16,0,true,true},1,1,1};
    auto after=before;++after.frame.frame;++after.frame.pose;
    DepthHistoryMask mask;
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    const D3D11_VIEWPORT sentinel{2,3,7,9,.1F,.9F};context->RSSetViewports(1,&sentinel);
    Check(mask.Generate(device.Get(),context.Get(),after,before,sources),"mask generation");
    auto pixels=Read(device.Get(),context.Get(),mask.EyeMask(0));
    Check(pixels[6*16+4]==255&&pixels[6*16+7]==255,"moving occluder or exposed background consumed camera-only history");
    Check(pixels[6*16+5]==0&&pixels[3*16+10]==0,"unchanged geometry rejected");
    Check(pixels[12*16+12]==255&&pixels[11*16+11]==128,"native exclusion values discarded");
    sources[1].erosion_radius=1;
    Check(mask.Generate(device.Get(),context.Get(),after,before,sources),"mask halo generation");
    pixels=Read(device.Get(),context.Get(),mask.EyeMask(1));
    Check(pixels[4*16+7]==255&&pixels[0]==255&&pixels[3*16+10]==0,"uncertain region halo/border not conservative");
    // The same world surface moved one pixel in the previous camera image.
    std::fill(current.begin(),current.end(),.5F);std::fill(previous.begin(),previous.end(),.5F);
    current[8*16+6]=.25F;previous[8*16+7]=.25F;
    context->UpdateSubresource(now.Get(),0,nullptr,current.data(),64,0);
    context->UpdateSubresource(old.Get(),0,nullptr,previous.data(),64,0);
    sources[0].clip_to_previous[12]=.125F;
    Check(mask.Generate(device.Get(),context.Get(),after,before,sources),"camera mask generation");
    Check(Read(device.Get(),context.Get(),mask.EyeMask(0))[8*16+6]==0,"camera reprojection sign/scale wrong");
    // Current and previous jitter affect opposite sides of the lookup. Neither
    // an omitted offset nor a reversed offset reaches this previous texel.
    std::fill(current.begin(),current.end(),.5F);std::fill(previous.begin(),previous.end(),.5F);
    current[6*16+6]=.25F;previous[5*16+5]=.25F;
    context->UpdateSubresource(now.Get(),0,nullptr,current.data(),64,0);
    context->UpdateSubresource(old.Get(),0,nullptr,previous.data(),64,0);
    sources[0].clip_to_previous=cojvr::runtime::TemporalIdentity();
    sources[0].jitter_x=sources[0].jitter_y=.25F;
    sources[0].previous_jitter_x=sources[0].previous_jitter_y=-.5F;
    Check(mask.Generate(device.Get(),context.Get(),after,before,sources),"jitter mask generation");
    Check(Read(device.Get(),context.Get(),mask.EyeMask(0))[6*16+6]==0,"current/previous jitter mapping wrong");
    sources[0].jitter_x=sources[0].jitter_y=sources[0].previous_jitter_x=sources[0].previous_jitter_y=0;
    current[6*16+6]=.75F;previous[6*16+6]=.5F;
    context->UpdateSubresource(now.Get(),0,nullptr,current.data(),64,0);
    context->UpdateSubresource(old.Get(),0,nullptr,previous.data(),64,0);
    for(auto& s:sources){s.previous_near_metres=.5F;s.previous_far_metres=1.5F;s.relative_tolerance=.5F;s.erosion_radius=0;}
    sources[1].previous_infinite_far=true;
    Check(mask.Generate(device.Get(),context.Get(),after,before,sources),"finite/infinite optics mask");
    Check(Read(device.Get(),context.Get(),mask.EyeMask(0))[6*16+6]==0&&
        Read(device.Get(),context.Get(),mask.EyeMask(1))[6*16+6]==255,"previous finite/infinite axial-depth interpretation lost");
    for(auto& s:sources){s.previous_infinite_far=false;s.relative_tolerance=.05F;}
    current[0]=std::numeric_limits<float>::quiet_NaN();current[1]=1;previous[2]=-1;
    context->UpdateSubresource(now.Get(),0,nullptr,current.data(),64,0);
    context->UpdateSubresource(old.Get(),0,nullptr,previous.data(),64,0);
    Check(mask.Generate(device.Get(),context.Get(),after,before,sources),"invalid depth mask");
    pixels=Read(device.Get(),context.Get(),mask.EyeMask(0));
    Check(pixels[0]==255&&pixels[1]==255&&pixels[2]==255,"invalid/far/absent depth consumed history");
    D3D11_PRIMITIVE_TOPOLOGY topology{};context->IAGetPrimitiveTopology(&topology);
    D3D11_VIEWPORT restored{};UINT count=1;context->RSGetViewports(&count,&restored);
    Check(topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST&&count==1&&std::memcmp(&sentinel,&restored,sizeof(sentinel))==0,"mask changed caller pipeline");
    ComPtr<ID3D11Texture2D> retained=mask.EyeMask(0);
    const auto retained_pixels=Read(device.Get(),context.Get(),retained.Get());
    for(unsigned fault=0;fault<26;++fault) {
        auto c=after,p=before;auto bad=sources;
        switch(fault) {
        case 0:bad[1].source_qualified=false;break;
        case 1:bad[1].current_token=0;break;
        case 2:bad[1].previous_token=bad[1].current_token;break;
        case 3:bad[1].previous_depth=bad[1].depth;break;
        case 4:bad[1].jitter_x=.51F;break;
        case 5:bad[1].previous_jitter_y=std::numeric_limits<float>::infinity();break;
        case 6:bad[1].relative_tolerance=-.1F;break;
        case 7:bad[1].relative_tolerance=std::numeric_limits<float>::quiet_NaN();break;
        case 8:bad[1].erosion_radius=3;break;
        case 9:bad[1].previous_near_metres=0;break;
        case 10:bad[1].previous_far_metres=bad[1].previous_near_metres;break;
        case 11:bad[1].clip_to_previous.fill(0);break;
        case 12:bad[1].exclusions=retained.Get();break;
        case 13:bad[1].exclusions=now.Get();break;
        case 14:bad[1].previous_depth=nullptr;break;
        case 15:p.frame.frame-=1;break;
        case 16:p.frame.pose=c.frame.pose;break;
        case 17:p.frame.owner+=1;break;
        case 18:p.frame.generation+=1;break;
        case 19:p.frame.width+=1;break;
        case 20:c.frame.tracking_valid=false;break;
        case 21:c.frame.device+=1;break;
        case 22:c.frame.output_width-=1;break;
        case 23:c.history_epoch+=1;break;
        case 24:p.settings_revision+=1;break;
        case 25:c.input_context=0;break;
        }
        Check(!mask.Generate(device.Get(),context.Get(),c,p,bad)&&!mask.EyeMask(0)&&!mask.EyeMask(1),"bad source/identity exposed masks");
        Check(Read(device.Get(),context.Get(),retained.Get())==retained_pixels,"invalid second eye wrote first-eye output");
    }
    Check(mask.Generate(device.Get(),context.Get(),after,before,sources),"valid mask did not recover after rejection");
    mask.Reset();Check(!mask.EyeMask(0)&&!mask.EyeMask(2),"mask reset/out-of-range retained output");
    std::cout<<"GPU depth-history mask moving occluders, camera reprojection and conservative exclusions passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
