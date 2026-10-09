#pragma once
#include <d3d9.h>
#include <cstdint>
#include <wrl/client.h>

namespace cojvr::backends::d3d9 {
// Diagnostic values only. Layout agreement does not establish that depth belongs
// to the completed eye, contains hands, or can be sampled/shared with D3D11.
struct D3D9DepthObservation {
    bool target_valid=false,depth_valid=false,state_valid=false,layout_matches=false;
    std::uintptr_t target_identity=0,depth_identity=0;
    D3DSURFACE_DESC target{},depth{};
    D3DVIEWPORT9 viewport{};
    DWORD z_enable=0,z_write=0,z_function=0;
    D3DMATRIX view{},projection{};
    bool transforms_valid=false;
};
inline D3D9DepthObservation ObserveD3D9Depth(IDirect3DDevice9* device) noexcept {
    D3D9DepthObservation out{};
    if(!device)return out;
    Microsoft::WRL::ComPtr<IDirect3DSurface9> target,depth;
    if(SUCCEEDED(device->GetRenderTarget(0,&target))&&target){
        out.target_identity=reinterpret_cast<std::uintptr_t>(target.Get());
        out.target_valid=SUCCEEDED(target->GetDesc(&out.target));
    }
    if(SUCCEEDED(device->GetDepthStencilSurface(&depth))&&depth){
        out.depth_identity=reinterpret_cast<std::uintptr_t>(depth.Get());
        out.depth_valid=SUCCEEDED(depth->GetDesc(&out.depth));
    }
    out.state_valid=SUCCEEDED(device->GetViewport(&out.viewport))&&
        SUCCEEDED(device->GetRenderState(D3DRS_ZENABLE,&out.z_enable))&&
        SUCCEEDED(device->GetRenderState(D3DRS_ZWRITEENABLE,&out.z_write))&&
        SUCCEEDED(device->GetRenderState(D3DRS_ZFUNC,&out.z_function));
    out.transforms_valid=SUCCEEDED(device->GetTransform(D3DTS_VIEW,&out.view))&&
        SUCCEEDED(device->GetTransform(D3DTS_PROJECTION,&out.projection));
    out.layout_matches=out.target_valid&&out.depth_valid&&
        out.depth.Width>=out.target.Width&&out.depth.Height>=out.target.Height&&
        out.depth.MultiSampleType==out.target.MultiSampleType&&
        out.depth.MultiSampleQuality==out.target.MultiSampleQuality;
    return out;
}
}
