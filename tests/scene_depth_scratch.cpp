#include "backends/openvr/scene_depth_scratch.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
using cojvr::backends::openvr::dlss::SceneDepthScratch;
namespace {
void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
// A real texture retains this COM marker until its final reference is released.
const GUID marker_id={0x750db03e,0xbe43,0x4f65,{0xb2,0x77,0x44,0xef,0x61,0xdd,0x71,0x2a}};
struct Lifetime final:IUnknown {
    ULONG refs=1;unsigned& live;
    explicit Lifetime(unsigned& count):live(count){++live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(id!=__uuidof(IUnknown))return E_NOINTERFACE;
        *out=static_cast<IUnknown*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{const auto n=--refs;if(!n){--live;delete this;}return n;}
};
// Only allocation failure is injected; storage and COM lifetimes use real WARP.
struct Allocator {
    ComPtr<ID3D11Device> device;
    unsigned live=0,calls=0,fail_at=0,peak=0;
    HRESULT CreateTexture2D(const D3D11_TEXTURE2D_DESC* desc,const D3D11_SUBRESOURCE_DATA* data,
                           ID3D11Texture2D** out) noexcept {
        ++calls;if(fail_at==calls){*out=nullptr;return E_OUTOFMEMORY;}
        const auto hr=device->CreateTexture2D(desc,data,out);if(FAILED(hr))return hr;
        auto* marker=new Lifetime(live);
        const auto tagged=(*out)->SetPrivateDataInterface(marker_id,marker);marker->Release();
        if(FAILED(tagged)){(*out)->Release();*out=nullptr;return tagged;}
        peak=std::max(peak,live);return S_OK;
    }
};
void Empty(const SceneDepthScratch& scratch){
    Check(!scratch.width&&!scratch.height,"failed allocation advertised a usable extent");
    for(const auto& plane:scratch.planes)Check(!plane,"failed allocation retained partial planes");
    for(const auto& mask:scratch.masks)Check(!mask,"failed allocation retained old masks");
}
}
int main()try {
    Allocator allocator;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&allocator.device,nullptr,&context)),"WARP device");
    {
        SceneDepthScratch scratch;
        Check(scratch.Prepare(&allocator,16,16)&&allocator.live==4,"initial planes");
        auto* first=scratch.planes[0].Get();const auto calls=allocator.calls;
        Check(scratch.Prepare(&allocator,16,16)&&scratch.planes[0].Get()==first&&allocator.calls==calls,
            "unchanged extent allocated replacement planes");
        D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=16;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R8_UNORM;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        for(auto& mask:scratch.masks)Check(SUCCEEDED(allocator.CreateTexture2D(&d,nullptr,&mask)),"seed mask");
        allocator.peak=0;
        Check(scratch.Prepare(&allocator,24,20),"resized planes");
        Check(allocator.peak==4&&allocator.live==4,"old planes/masks overlapped replacement allocation");
        for(unsigned failure=1;failure<=4;++failure){
            allocator.fail_at=allocator.calls+failure;
            Check(!scratch.Prepare(&allocator,32,24),"injected plane allocation failure accepted");
            Empty(scratch);Check(!allocator.live,"allocation failure leaked texture ownership");
            allocator.fail_at=0;
            Check(scratch.Prepare(&allocator,24,20)&&allocator.live==4,"retry after allocation failure");
        }
    }
    Check(!allocator.live,"scratch destruction leaked textures");
    std::cout<<"Scratch allocation ordering, reuse and four partial-failure retries passed. No GPU residency claim.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
