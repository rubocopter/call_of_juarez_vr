#include "backends/openvr/flat_menu_composition.hpp"
#include <array>
#include <chrono>
#include <iostream>
#include <string_view>
#include <vector>
using Microsoft::WRL::ComPtr;
using namespace cojvr::backends::openvr;
int main(int argc,char** argv) {
    const bool benchmark=argc==2&&std::string_view(argv[1])=="--benchmark";
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    if(FAILED(D3D11CreateDevice(nullptr,benchmark?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,
        nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context))) return 1;
    const unsigned sw=benchmark?2804:8,sh=benchmark?2860:8,tw=benchmark?4226:16,th=benchmark?3862:16;
    std::vector<std::uint32_t> pixels(sw*sh,0xFF204080);
    if(!benchmark)for(unsigned y=0;y<sh;++y)for(unsigned x=0;x<sw;++x)
        pixels[y*sw+x]=(x<4?(y<4?0xFF0000FF:0xFFFF0000):(y<4?0xFF00FF00:0xFFFFFFFF));
    const auto original=pixels;
    std::array<ComPtr<ID3D11Texture2D>,2> targets;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=tw;desc.Height=th;desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    for(auto& target:targets)if(FAILED(device->CreateTexture2D(&desc,nullptr,&target)))return 2;
    const std::array<ID3D11Texture2D*,2> outputs{targets[0].Get(),targets[1].Get()};
    std::array<MenuDrawRectangle,2> rects{MenuDrawRectangle{benchmark?1830U:2U,benchmark?959U:3U,benchmark?1988U:8U,benchmark?1944U:8U},
        MenuDrawRectangle{benchmark?408U:4U,benchmark?959U:5U,benchmark?1988U:8U,benchmark?1944U:8U}};
    FlatMenuCompositor compositor;
    for(unsigned iteration=0;iteration<(benchmark?5U:2U);++iteration) {
        const auto begin=std::chrono::steady_clock::now();
        if(!compositor.Draw(device.Get(),context.Get(),reinterpret_cast<const std::uint8_t*>(pixels.data()),
            sw,sh,sw*4,outputs,rects,{},true)) {std::cerr<<"draw hr="<<std::hex<<compositor.last_result()<<'\n';return 3;}
        if(benchmark)std::cout<<"menu_gpu_cpu_queue_ms="<<std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-begin).count()<<";upload_allocations="<<compositor.source_allocations()<<'\n';
    }
    if(compositor.source_allocations()!=1||pixels!=original)return 4;
    if(benchmark)return 0;
    auto staging_desc=desc;staging_desc.BindFlags=0;staging_desc.Usage=D3D11_USAGE_STAGING;staging_desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    if(FAILED(device->CreateTexture2D(&staging_desc,nullptr,&staging)))return 5;
    for(unsigned eye=0;eye<2;++eye) {
        context->CopyResource(staging.Get(),targets[eye].Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return 6;
        const auto sample=[&](unsigned x,unsigned y){return *reinterpret_cast<const std::uint32_t*>(
            static_cast<const std::uint8_t*>(mapped.pData)+std::size_t(y)*mapped.RowPitch+x*4);};
        const auto& r=rects[eye];
        const bool correct=sample(0,0)==0xFF000000&&sample(r.left+1,r.top+1)==0xFF0000FF&&
            sample(r.left+6,r.top+1)==0xFF00FF00&&sample(r.left+1,r.top+6)==0xFFFF0000&&sample(r.left+6,r.top+6)==0xFFFFFFFF;
        context->Unmap(staging.Get(),0);if(!correct)return 7;
    }
    // Changing only the visible rectangle reuses the input and all output objects.
    rects[0]={4,4,4,4};rects[1]={6,6,4,4};
    if(!compositor.Draw(device.Get(),context.Get(),reinterpret_cast<const std::uint8_t*>(pixels.data()),sw,sh,sw*4,
        outputs,rects,{},true)||compositor.source_allocations()!=1)return 8;
    if(!compositor.Draw(device.Get(),context.Get(),reinterpret_cast<const std::uint8_t*>(pixels.data()),sw,sh,sw*4,
        outputs,rects,{true,3,3,0,7},true))return 11;
    context->CopyResource(staging.Get(),targets[0].Get());D3D11_MAPPED_SUBRESOURCE pointer_map{};
    if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&pointer_map)))return 12;
    const auto pointer_pixel=*reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(pointer_map.pData)+
        (rects[0].top+1)*pointer_map.RowPitch+(rects[0].left+1)*4);
    context->Unmap(staging.Get(),0);if(pointer_pixel!=0xFFFFFFFF)return 13;
    ComPtr<ID3D11Device> foreign;ComPtr<ID3D11DeviceContext> foreign_context;
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,0,D3D11_SDK_VERSION,&foreign,nullptr,&foreign_context)))return 14;
    if(compositor.Draw(device.Get(),foreign_context.Get(),reinterpret_cast<const std::uint8_t*>(pixels.data()),sw,sh,sw*4,
        outputs,rects,{},true)||compositor.Draw(device.Get(),context.Get(),nullptr,sw,sh,sw*4,outputs,rects,{},true))return 15;
    rects[1].width=100;
    if(compositor.Draw(device.Get(),context.Get(),reinterpret_cast<const std::uint8_t*>(pixels.data()),sw,sh,sw*4,
        outputs,rects,{},true)||compositor.last_result()!=E_INVALIDARG)return 9;
    compositor.Reset();if(compositor.source_allocations()!=0)return 10;
    std::cout<<"GPU menu pixels, both placements, native pointer, source preservation, geometry reuse and rejected foreign context/bounds passed\n";
}
