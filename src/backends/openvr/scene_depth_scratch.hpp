#pragma once
#include <array>
#include <d3d11.h>
#include <wrl/client.h>

namespace cojvr::backends::openvr::dlss {
// Private worker storage. Prepare is called only after prior GPU use has retired.
struct SceneDepthScratch {
    std::array<Microsoft::WRL::ComPtr<ID3D11Texture2D>,4> planes;
    std::array<Microsoft::WRL::ComPtr<ID3D11Texture2D>,2> masks;
    unsigned width=0,height=0;
    template<class Device>
    bool Prepare(Device* device,unsigned w,unsigned h) noexcept {
        if(width==w&&height==h&&planes[3])return true;
        // Retire the old extent before allocating the next one. At native VR
        // sizes, keeping four old FP16 planes resident while allocating four
        // replacements can exceed the bounded worker's source allowance.
        planes={};masks={};width=height=0;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;
        desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
        desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        for(auto& plane:planes)if(FAILED(device->CreateTexture2D(&desc,nullptr,&plane))){planes={};return false;}
        width=w;height=h;return true;
    }
};
}
