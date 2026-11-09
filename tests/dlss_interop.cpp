#include "backends/openvr/dlss_client.hpp"
#include "backends/openvr/temporal_guides.hpp"
#include "backends/openvr/depth_history_mask.hpp"
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
        // Exercise the actual x64 output fence with fully protected pixels.
        // New colors make stale output observable. A later eye must not
        // overwrite the earlier shared output when it reuses private scratch.
        std::fill(bias.begin(),bias.end(),255);
        auto protected_mask=Texture(device.Get(),size,size,DXGI_FORMAT_R8_UNORM,bias.data(),1);
        std::array<std::vector<std::uint16_t>,2> protected_outputs;
        for(unsigned eye=0;eye<2;++eye) {
            const std::array<std::uint16_t,4> expected{eye?std::uint16_t{0x3400}:std::uint16_t{0x3800},
                0x3000,eye?std::uint16_t{0x3800}:std::uint16_t{0x3400},0x3c00};
            for(std::size_t p=0;p<colors.size();p+=4)std::copy(expected.begin(),expected.end(),colors.begin()+p);
            auto protected_color=Texture(device.Get(),size,size,DXGI_FORMAT_R16G16B16A16_FLOAT,colors.data(),8);
            auto protected_input=guides.EyeInputs(eye);
            protected_input.color=protected_color.Get();protected_input.bias=protected_mask.Get();
            Require(client.Evaluate(context.Get(),eye,protected_input),"protected NGX output failed");
            auto& pixels=protected_outputs[eye];pixels=Read(device.Get(),context.Get(),client.Output(eye));
            for(std::size_t p=0;p<pixels.size();p+=4)
                Require(std::equal(expected.begin(),expected.end(),pixels.begin()+p),
                    "fenced helper output did not preserve protected current color");
        }
        Require(Read(device.Get(),context.Get(),client.Output(0))==protected_outputs[0],
            "right-eye resolve overwrote the completed left eye");
        // Camera reprojection alone must not retain a newly exposed background.
        // Generate the protective mask on GPU from the actual current guide Z
        // and a controlled previous occluder, then exercise the helper resolve.
        const float current_z=100.F/99.5F-50.F/(99.5F*10.F);
        std::vector<float> old_depth(std::size_t(size)*size,current_z);
        for(unsigned y=size/2-8;y<size/2+8;++y)for(unsigned x=size/2-8;x<size/2+8;++x)old_depth[y*size+x]=.5F;
        auto previous_depth=Texture(device.Get(),size,size,DXGI_FORMAT_R32_FLOAT,old_depth.data(),4);
        std::array<DepthHistoryMaskSource,2> history_sources;
        for(unsigned eye=0;eye<2;++eye) {
            auto& s=history_sources[eye];s.depth=guides.EyeInputs(eye).depth;s.previous_depth=previous_depth.Get();
            s.exclusions=mask.Get();s.current_token=8+eye*10;s.previous_token=7+eye*10;
            s.previous_near_metres=.5F;s.previous_far_metres=100;s.relative_tolerance=.05F;s.erosion_radius=1;s.source_qualified=true;
        }
        PairIdentity previous_identity{{reinterpret_cast<std::uintptr_t>(device.Get()),1,7,7,1,
            size,size,output_size,output_size,static_cast<unsigned>(quality),true,true},1,1,1};
        auto current_identity=previous_identity;++current_identity.frame.frame;++current_identity.frame.pose;
        DepthHistoryMask history_mask;
        Require(history_mask.Generate(device.Get(),context.Get(),current_identity,previous_identity,history_sources),
            "GPU disocclusion mask generation failed");
        const std::array<std::uint16_t,4> fresh_color{0x3400,0x3800,0x3000,0x3c00};
        for(std::size_t p=0;p<colors.size();p+=4)std::copy(fresh_color.begin(),fresh_color.end(),colors.begin()+p);
        auto exposed_color=Texture(device.Get(),size,size,DXGI_FORMAT_R16G16B16A16_FLOAT,colors.data(),8);
        for(unsigned eye=0;eye<2;++eye) {
            auto input=guides.EyeInputs(eye);input.color=exposed_color.Get();input.bias=history_mask.EyeMask(eye);
            Require(client.Evaluate(context.Get(),eye,input),"GPU masked NGX evaluation failed");
            const auto pixels=Read(device.Get(),context.Get(),client.Output(eye));
            Require(std::equal(fresh_color.begin(),fresh_color.end(),pixels.begin()+center),
                "GPU disocclusion mask did not preserve newly exposed current color");
        }
        std::cout<<"Real NGX "<<(quality==Quality::dlaa?"DLAA":"DLSS Quality")<<" x86->x64;render="<<size<<"x"<<size
            <<";output="<<output_size<<"x"<<output_size<<";eyes=2;frames_per_eye=8;finite_distinct_output=true;protected_current_color=true;gpu_disocclusion_mask=true\n";
        Require(!client.Evaluate(nullptr,1,guides.EyeInputs(1))&&!client.Output(1),"missing context retained stale output");
        // Incomplete guides reject before sending work or exposing stale output.
        Inputs invalid=guides.EyeInputs(0);invalid.depth=nullptr;
        Require(!client.Evaluate(context.Get(),0,invalid)&&!client.Output(0),"invalid guides retained stale output");
    }
    client.Stop();Require(!client.Output(0),"stopped helper exposed old eye");
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
