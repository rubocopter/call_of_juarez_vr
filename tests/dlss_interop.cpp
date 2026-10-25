#include "backends/openvr/dlss_client.hpp"
#include "backends/openvr/temporal_guides.hpp"
#include <wrl/client.h>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cojvr::backends::openvr::dlss;
using Microsoft::WRL::ComPtr;
namespace {
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
ComPtr<ID3D11Texture2D> Texture(ID3D11Device* device,UINT width,UINT height,DXGI_FORMAT format,
                              const void* pixels,UINT bytes_per_pixel) {
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.ArraySize=desc.MipLevels=1;
    desc.Format=format;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels,width*bytes_per_pixel,0};ComPtr<ID3D11Texture2D> texture;
    Require(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture)),"fixture texture failed");return texture;
}
std::vector<std::uint16_t> Read(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* texture) {
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);const auto width=desc.Width,height=desc.Height;
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=desc.MiscFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;Require(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)),"readback texture failed");
    context->CopyResource(staging.Get(),texture);D3D11_MAPPED_SUBRESOURCE mapped{};
    Require(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"output readback failed");
    std::vector<std::uint16_t> pixels(std::size_t(width)*height*4);
    for(UINT y=0;y<height;++y)std::copy_n(reinterpret_cast<const std::uint16_t*>(
        static_cast<const unsigned char*>(mapped.pData)+std::size_t(y)*mapped.RowPitch),std::size_t(width)*4,pixels.data()+std::size_t(y)*width*4);
    context->Unmap(staging.Get(),0);return pixels;
}
}
int main(int argc,char** argv) try {
    // An owned child that exits before connecting exercises startup cancellation.
    if(argc==4&&std::string_view(argv[1])=="--pipe")return 19;
    Client client;
    Require(client.Output(0)==nullptr&&!client.Build(0,64,64,64,64,Quality::dlaa),"uninitialized client admitted output/build");
    Require(!client.Start(nullptr,{}),"missing helper/device accepted");
    if(argc!=2) {std::cout<<"DLSS missing dependencies fail closed; GPU interop requires an explicit x64 helper path\n";return 0;}
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
    Require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        &level,1,D3D11_SDK_VERSION,&device,nullptr,&context)),"hardware D3D11 device failed");
    Require(!client.Start(device.Get(),std::filesystem::absolute(argv[0]))&&!client.Output(0),"exited helper admitted temporal output");
    {
        const auto directory=std::filesystem::temp_directory_path()/(L"cojvr-missing-ngx-"+std::to_wstring(GetCurrentProcessId()));
        Require(std::filesystem::create_directory(directory),"missing-runtime fixture already exists");
        struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove_all(path,e);}} cleanup{directory};
        const auto host=directory/L"cojvr_dlss_host64.exe";
        std::filesystem::copy_file(argv[1],host);
        Require(!client.Start(device.Get(),host)&&!client.Output(0),"missing NGX runtime admitted output");
        client.Stop();
    }
    if(!client.Start(device.Get(),std::filesystem::absolute(argv[1]))) {
        std::cerr<<client.last_error()<<";ngx=0x"<<std::hex<<client.last_ngx_result()<<'\n';return 1;
    }
    Require(sizeof(void*)==4&&client.helper_bits()==64,"test must cross x86 -> x64");
    constexpr UINT output_size=384;
    for(const auto quality:{Quality::dlaa,Quality::quality}) {
        const UINT size=quality==Quality::dlaa?output_size:256;
        std::vector<std::uint16_t> colors(std::size_t(size)*size*4);
        std::vector<unsigned char> bias(std::size_t(size)*size,0);
        auto mask=Texture(device.Get(),size,size,DXGI_FORMAT_R8_UNORM,bias.data(),1);
        std::array<ComPtr<ID3D11Texture2D>,2> color;
        std::array<ComPtr<ID3D11Texture2D>,2> depth_alpha;
        for(UINT eye=0;eye<2;++eye) {
            for(UINT y=0;y<size;++y)for(UINT x=0;x<size;++x) {
                const auto i=(std::size_t(y)*size+x)*4;
                colors[i]=eye?0x3000:0x3A00;colors[i+1]=(x/16+y/16)%2?0x3400:0x3000;
                colors[i+2]=eye?0x3A00:0x3000;colors[i+3]=0x3C00;
            }
            color[eye]=Texture(device.Get(),size,size,DXGI_FORMAT_R16G16B16A16_FLOAT,colors.data(),8);
            for(std::size_t p=3;p<colors.size();p+=4)colors[p]=0x4900;
            depth_alpha[eye]=Texture(device.Get(),size,size,DXGI_FORMAT_R16G16B16A16_FLOAT,colors.data(),8);
            if(!client.Build(eye,size,size,output_size,output_size,quality)) {
                std::cerr<<client.last_error()<<";ngx=0x"<<std::hex<<client.last_ngx_result()<<'\n';return 1;
            }
            Require(!client.Output(eye),"unevaluated output exposed");
        }
        TemporalGuides guides;
        for(unsigned frame=0;frame<6;++frame) {
            if(quality==Quality::dlaa&&frame==1)Sleep(11000); // Healthy pause must retain the helper/features.
            std::array<GuideSource,2> sources;
            for(unsigned eye=0;eye<2;++eye) {
                sources[eye].color=color[eye].Get();sources[eye].linear_depth_alpha=depth_alpha[eye].Get();
                sources[eye].current_color_mask=mask.Get();sources[eye].near_metres=.5F;sources[eye].far_metres=100;
                sources[eye].source_qualified=true; // Controlled fixture only, not the game.
                sources[eye].history=frame!=0;
            }
            Require(guides.Generate(device.Get(),context.Get(),sources),"GPU temporal guides failed");
            for(unsigned eye=0;eye<2;++eye) {
            const Inputs inputs=guides.EyeInputs(eye);
            if(!client.Evaluate(context.Get(),eye,inputs)) {
                std::cerr<<client.last_error()<<";ngx=0x"<<std::hex<<client.last_ngx_result()<<'\n';return 1;
            }
            }
        }
        const auto left=Read(device.Get(),context.Get(),client.Output(0));
        const auto right=Read(device.Get(),context.Get(),client.Output(1));
        const auto center=(std::size_t(output_size/2)*output_size+output_size/2)*4;
        Require(left.size()==std::size_t(output_size)*output_size*4&&left!=right,"NGX stereo output dimension/independence failed");
        Require(left[center]>left[center+2]&&right[center+2]>right[center],"NGX output lost per-eye primary color");
        // Half-float NaN/Inf bit patterns must not occur in returned colors.
        for(const auto* pixels:{&left,&right})for(auto half:*pixels)Require((half&0x7C00)!=0x7C00,"NGX output contains NaN/Inf");
        std::cout<<"Real NGX "<<(quality==Quality::dlaa?"DLAA":"DLSS Quality")<<" x86->x64;render="<<size<<"x"<<size
            <<";output="<<output_size<<"x"<<output_size<<";eyes=2;frames_per_eye=6;finite_distinct_output=true\n";
        Require(!client.Evaluate(nullptr,1,guides.EyeInputs(1))&&!client.Output(1),"missing context retained stale output");
        // Incomplete guides reject before sending work or exposing stale output.
        Inputs invalid=guides.EyeInputs(0);invalid.depth=nullptr;
        Require(!client.Evaluate(context.Get(),0,invalid)&&!client.Output(0),"invalid guides retained stale output");
    }
    client.Stop();Require(!client.Output(0),"stopped helper exposed old eye");
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
