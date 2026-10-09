#include "runtime/reload_visual.hpp"
#include "runtime/reload_gesture.hpp"
#include "backends/openvr/gameplay_ui_raster.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>
#include <wrl/client.h>
using namespace cojvr::runtime;
static_assert(std::is_trivially_copyable_v<ReloadCartridgeSurface>);
void Require(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
float Dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 Sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
bool Near(float a,float b){return std::abs(a-b)<1e-5F;}
bool Near(Vec3 a,Vec3 b){return Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.z,b.z);}

// Exercise the exported visual tip against the real explicit-socket policy.
// The negative journey changes only tip orientation, never its rendered apex.
void RenderedTipInsertionJourneys(){
    for(const auto armed_hand:{std::uint8_t{0},std::uint8_t{1}}){
        for(const bool reversed:{false,true}){
            MotionReloadGesture gesture;
            MotionReloadGestureInput input{};
            const auto tracked=[](Vec3 position){
                Pose p{};p.position=position;p.position_valid=p.orientation_valid=true;return p;
            };
            input.valid=input.eligible=true;
            input.trigger_available={true,true};
            input.player_identity=11;input.weapon_identity=29;input.input_generation=7;
            input.armed_hand=armed_hand;
            input.head=tracked({0,1.6F,0});
            input.left_grip=tracked({-.2F,1.35F,-.35F});
            input.right_grip=tracked({.2F,1.35F,-.35F});
            auto& support=armed_hand==0?input.left_grip:input.right_grip;
            const float side=armed_hand==0?-.2F:.2F;
            support.position={side,1.05F,-.1F}; // Valid fresh waist pickup.
            support.orientation={0,0,.70710678F,.70710678F}; // Rolled grip.
            input.insertion_target=tracked({0,1.45F,-.50F});
            input.insertion_target->orientation={0,1,0,0}; // Socket +Z is inward -Z.
            const auto sample=[&](bool held,std::uint64_t ms){
                input.cartridge_tip=BuildReloadCartridgeTipPose(support);
                Require(input.cartridge_tip.has_value(),"moving support must produce real tip pose");
                StereoHudTextOverlay rendered{};
                Require(BuildReloadCartridgeOverlay(input.head,support,true,rendered),
                    "insertion journey must retain valid rendered cartridge");
                const auto apex=Sub(input.cartridge_tip->position,input.head.position);
                for(std::size_t f=1;f<12;f+=2)
                    Require(Near(rendered.reload_cartridge_surfaces[f].head_corners[2],apex),
                        "gesture tip must coincide with rendered apex throughout journey");
                const auto inward=RotateVector(input.insertion_target->orientation,{0,0,1});
                Require(Near(RotateVector(input.cartridge_tip->orientation,{0,0,1}),inward),
                    "real exported tip must align with explicit socket inward axis");
                if(reversed)input.cartridge_tip->orientation=support.orientation;
                input.trigger_held[1U-armed_hand]=held;
                input.monotonic_ms=ms;++input.pose_sequence;
                return gesture.Update(input);
            };
            const auto baseline=sample(false,1000);
            Require(!baseline.reload&&!baseline.consume_free_trigger,"socket journey needs released baseline");
            const auto pickup=sample(true,1010);
            Require(!pickup.reload&&pickup.cartridge_held&&pickup.consumed_trigger_mask==(1U<<(1U-armed_hand)),
                "waist pickup must claim physical support trigger");
            for(int i=1;i<=8;++i){
                const float t=static_cast<float>(i)/8.F;
                support.position={side*(1.F-t),1.05F+.40F*t,-.10F-.3025F*t};
                const auto carry=sample(true,1010+i*50);
                Require(!carry.reload&&carry.cartridge_held,"bounded held rear approach must remain carrying");
            }
            Require(Near(input.cartridge_tip->position,{0,1.45F,-.45F}),"real tip must approach socket from 5cm behind");
            Require(!sample(true,1460).reload,"held rear approach cannot reload");
            support.position.z=-.4625F; // Actual rendered tip enters to -.51m.
            const auto entry=sample(true,1510);
            Require(!entry.reload&&entry.cartridge_held,"held tip entry must defer reload until release");
            Require(Near(input.cartridge_tip->position,{0,1.45F,-.51F}),"rendered tip must enter socket by 1cm");
            const auto armed=armed_hand==0?input.right_grip.position:input.left_grip.position;
            Require(Dot(Sub(support.position,armed),Sub(support.position,armed))>.14F*.14F,
                "success must depend on explicit tip insertion, not legacy grip proximity");
            const auto release=sample(false,1520);
            Require(release.reload==!reversed&&release.consume_free_trigger&&!release.cartridge_held,
                "real aligned rendered-tip entry/release succeeds; reversed tip orientation fails");
            Require(!sample(false,1530).reload,"rendered-tip insertion cannot repeat after release");
        }
    }
}
int main(){
    RenderedTipInsertionJourneys();
    Pose head{},grip{};head.position_valid=head.orientation_valid=true;
    grip=head;grip.position={.2F,-.3F,-.4F};
    StereoHudTextOverlay out{};out.frame_sequence=123;out.feedback_context_token=77;
    out.eyes[0].eye_to_head.position.x=-.032F;
    Require(BuildReloadCartridgeOverlay(head,grip,true,out),"valid carrying pose rejected");
    Require(out.reload_cartridge_visible&&out.frame_sequence==123&&out.feedback_context_token==77&&
        out.eyes[0].eye_to_head.position.x==-.032F,"capture metadata changed");
    Require(out.reload_cartridge_surface_count==16&&out.reload_cartridge_surface_count<=16,"bounded closed CC0-profile cartridge faces");
    const auto captured=out.reload_cartridge_surfaces;
    for(std::size_t i=0;i<16;++i){
        const auto& c=captured[i].head_corners;
        const auto a=Sub(c[1],c[0]),b=Sub(c[2],c[0]);
        const Vec3 normal{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
        Require(Dot(normal,Sub(c[0],{.2F,-.3F,-.43F}))>0,"convex surface winding must point outward");
    }
    auto tip=BuildReloadCartridgeTipPose(grip);
    Require(tip.has_value()&&Near(tip->position,{.2F,-.3F,-.4475F}),"tip offset contract");
    Require(Near(RotateVector(tip->orientation,{0,0,1}),{0,0,-1})&&
        Near(RotateVector(tip->orientation,{0,1,0}),{0,1,0}),"tip +Z must point inward with grip up preserved");
    for(std::size_t i=1;i<12;i+=2)
        Require(Near(captured[i].head_corners[2],tip->position)&&
            Near(captured[i].head_corners[3],tip->position),"tip does not match all tapered faces");
    auto edge=Sub(captured[0].head_corners[2],captured[0].head_corners[0]);
    Require(Near(edge,{0,0,.03264F}),"CC0-profile case axis must follow grip -Z rather than face head");
    for(std::size_t i=14;i<16;++i){
        Require(Near(captured[i].head_corners[0].z,grip.position.z-.00661F),"primer must lie on case base");
        Require(Near(captured[i].material_uv.y,108.F/140.F),"primer must select distinct material patch");
    }
    float minx=1,maxx=-1,miny=1,maxy=-1,minz=1,maxz=-1;
    for(std::size_t i=0;i<out.reload_cartridge_surface_count;++i)for(auto p:captured[i].head_corners){
        minx=std::min(minx,p.x);maxx=std::max(maxx,p.x);miny=std::min(miny,p.y);maxy=std::max(maxy,p.y);
        minz=std::min(minz,p.z);maxz=std::max(maxz,p.z);
    }
    Require(Near(maxx-minx,.01306F)&&Near(maxy-miny,.01131028F)&&
        Near(maxz-minz,.04089F),"simplified cartridge matches the inspected CC0 .44 Magnum envelope");
    head.position={1,2,3};grip.position={1.2F,1.7F,2.6F};
    Require(BuildReloadCartridgeOverlay(head,grip,true,out)&&
        Near(out.reload_cartridge_surfaces[0].head_corners[0],captured[0].head_corners[0]),"common translation changed geometry");
    head.orientation=grip.orientation={0,.70710678F,0,.70710678F};
    grip.position={.6F,1.7F,2.8F};
    Require(BuildReloadCartridgeOverlay(head,grip,true,out)&&
        Near(out.reload_cartridge_surfaces[0].head_corners[0],captured[0].head_corners[0]),"common rotation changed geometry");
    head.position={};head.orientation={};grip.position={.2F,-.3F,-.4F};
    for(auto rotation:std::array<Quaternion,3>{{{0,.70710678F,0,.70710678F},
        {.70710678F,0,0,.70710678F},{0,0,.70710678F,.70710678F}}}){
        grip.orientation=rotation;
        Require(BuildReloadCartridgeOverlay(head,grip,true,out),"rotated cartridge rejected");
        for(std::size_t f=0;f<16;++f)for(std::size_t c=0;c<4;++c){
            auto local=Sub(captured[f].head_corners[c],grip.position);
            auto expected=RotateVector(rotation,local);
            Require(Near(Sub(out.reload_cartridge_surfaces[f].head_corners[c],grip.position),expected),"geometry lost controller yaw/pitch/roll");
        }
        tip=BuildReloadCartridgeTipPose(grip);
        Require(tip&&Near(tip->position,out.reload_cartridge_surfaces[1].head_corners[2])&&
            Near(RotateVector(tip->orientation,{0,0,1}),RotateVector(rotation,{0,0,-1}))&&
            Near(RotateVector(tip->orientation,{0,1,0}),RotateVector(rotation,{0,1,0}))&&
            Near(RotateVector(tip->orientation,{1,0,0}),RotateVector(rotation,{-1,0,0})),
            "rotated tip must retain local-Y half turn and match rendered apex");
    }
    const auto hidden=[&]{
        Require(!out.reload_cartridge_visible&&out.reload_cartridge_surface_count==0,"hidden surface metadata persisted");
        for(auto p:out.reload_cartridge_head_corners)Require(Dot(p,p)==0,"legacy corners persisted");
        for(const auto& face:out.reload_cartridge_surfaces){
            for(auto p:face.head_corners)Require(Dot(p,p)==0,"hidden geometry persisted");
            Require(face.material_uv.x==0&&face.material_uv.y==0&&face.shade==0,"hidden material metadata persisted");
        }
    };
    Require(!BuildReloadCartridgeOverlay(head,grip,false,out),"hidden helper succeeded");hidden();
    grip.orientation.w=2;
    Require(!BuildReloadCartridgeTipPose(grip)&&!BuildReloadCartridgeOverlay(head,grip,true,out),"unnormalized pose accepted");hidden();
    grip=head;grip.position.x=std::numeric_limits<float>::infinity();
    Require(!BuildReloadCartridgeTipPose(grip)&&!BuildReloadCartridgeOverlay(head,grip,true,out),"nonfinite pose accepted");hidden();
    grip=head;grip.position_valid=false;
    Require(!BuildReloadCartridgeTipPose(grip)&&!BuildReloadCartridgeOverlay(head,grip,true,out),"untracked grip accepted");
    grip=head;grip.position={0,0,.5F};
    Require(!BuildReloadCartridgeOverlay(head,grip,true,out),"behind-head cartridge accepted");hidden();
    grip.position={0,0,-.02F};
    Require(!BuildReloadCartridgeOverlay(head,grip,true,out),"partially near-plane cartridge accepted");hidden();
    cojvr::backends::openvr::GameplayUiRaster raster;
    const auto& panel=raster.Cartridge();
    Require(panel.width==48&&panel.height==140&&panel.pixels.size()==48*140,"cartridge raster extent");
    auto body=panel.pixels[80*48+24],copper=panel.pixels[24*48+24];
    Require((body>>24)==255&&(copper>>24)==255&&body!=copper,"opaque brass/copper patches");
    auto primer=panel.pixels[108*48+24];
    Require((primer>>24)==255&&primer!=body&&primer!=copper,"distinct opaque primer patch");
    const auto* pixels=panel.pixels.data();Require(raster.Cartridge().pixels.data()==pixels,"static raster rebuilt");
    grip=head;grip.position={0,-.1F,-.4F};
    Require(BuildReloadCartridgeOverlay(head,grip,true,out),"stereo fixture pose rejected");
    EyeView left{},right_eye{};
    left.eye_to_head=head;left.eye_to_head.position.x=-.032F;
    left.fov=FovFromTangents(-1,1,1,-1);right_eye=left;right_eye.eye_to_head.position.x=.032F;
    cojvr::backends::openvr::HudPanelPlacement placement{};
    placement.captured_quad=true;placement.head_corners=out.reload_cartridge_surfaces[0].head_corners;
    cojvr::backends::openvr::ProjectedHudPanel l{},r{};
    Require(cojvr::backends::openvr::ProjectHudPanel(panel,left,1024,1024,placement,l)&&
        cojvr::backends::openvr::ProjectHudPanel(panel,right_eye,1024,1024,placement,r),"cartridge surface stereo projection");
    Require(l.left>r.left,"finite-depth cartridge must retain binocular disparity");
    const auto previous=placement.head_corners;
    grip.position.x=.1F;
    Require(BuildReloadCartridgeOverlay(head,grip,true,out)&&
        out.reload_cartridge_surfaces[0].head_corners[0].x>previous[0].x,"support motion must move next capture");
    Require(Near(placement.head_corners[0],previous[0]),"previous capture must remain value-only");
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device creation failed");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=1024;
    desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    std::array<ComPtr<ID3D11Texture2D>,2> targets;
    std::array<ComPtr<ID3D11RenderTargetView>,2> views;
    for(int i=0;i<2;++i){
        Require(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&targets[i]))&&
            SUCCEEDED(device->CreateRenderTargetView(targets[i].Get(),nullptr,&views[i])),"eye target creation failed");
    }
    const std::array<ID3D11Texture2D*,2> raw{targets[0].Get(),targets[1].Get()};
    const float clear[4]{};for(auto& v:views)context->ClearRenderTargetView(v.Get(),clear);
    out.eyes={left,right_eye};out.frame_sequence=123;
    cojvr::backends::openvr::HudTextCompositor compositor;
    Require(!compositor.Draw(device.Get(),context.Get(),out,124,raw),"mismatched capture sequence accepted");
    Require(compositor.Draw(device.Get(),context.Get(),out,123,raw),"cartridge-only compositor draw failed");
    desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;
    Require(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&readback)),"readback creation failed");
    std::array<std::uint64_t,2> sum_x{},fingerprint{};
    std::array<unsigned,2> copper_pixels{},primer_pixels{};
    const auto primer_rgb=panel.pixels[108*48+24];
    const auto primer_shade=out.reload_cartridge_surfaces[14].shade;
    const auto expected_primer=[&](int shift){
        return static_cast<int>(std::lround(((primer_rgb>>shift)&255)*primer_shade));
    };
    auto coverage=[&](int eye){
        context->CopyResource(readback.Get(),targets[eye].Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        Require(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)),"readback map failed");
        unsigned count=0;
        sum_x[eye]=fingerprint[eye]=0;copper_pixels[eye]=primer_pixels[eye]=0;
        for(unsigned y=0;y<1024;++y){auto* row=reinterpret_cast<const std::uint32_t*>(
            static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch);
            for(unsigned x=0;x<1024;++x)if(row[x]){
                ++count;sum_x[eye]+=x;
                fingerprint[eye]=fingerprint[eye]*1099511628211ULL+row[x]+x+1024ULL*y;
                if(((row[x]>>16)&255)>((row[x]>>8)&255)*1.7F)++copper_pixels[eye];
                if(std::abs(static_cast<int>((row[x]>>16)&255)-expected_primer(16))<=2&&
                    std::abs(static_cast<int>((row[x]>>8)&255)-expected_primer(8))<=2&&
                    std::abs(static_cast<int>(row[x]&255)-expected_primer(0))<=2)
                    ++primer_pixels[eye];
            }}
        context->Unmap(readback.Get(),0);return count;
    };
    auto left_count=coverage(0),right_count=coverage(1);
    Require(left_count>0&&right_count>0,"solid must produce visible pixels in both eyes");
    Require(primer_pixels[0]>0&&primer_pixels[1]>0,
        "WARP rear view must draw primer over the coplanar case base in both eyes");
    Require(static_cast<double>(sum_x[0])/left_count>static_cast<double>(sum_x[1])/right_count&&
        fingerprint[0]!=fingerprint[1],"WARP eyes must differ with correct stereo disparity");
    const auto initial_hash=fingerprint[0];
    Require(compositor.Draw(device.Get(),context.Get(),out,123,raw)&&coverage(0)>0&&
        fingerprint[0]==initial_hash,"repeated immutable capture changed rendering");
    for(auto& v:views)context->ClearRenderTargetView(v.Get(),clear);
    grip.orientation={0,.70710678F,0,.70710678F};
    Require(BuildReloadCartridgeOverlay(head,grip,true,out)&&
        compositor.Draw(device.Get(),context.Get(),out,123,raw),"side-on solid draw failed");
    Require(coverage(0)>0&&coverage(1)>0&&fingerprint[0]!=initial_hash&&
        copper_pixels[0]>0&&copper_pixels[1]>0,"side-on geometry must change silhouette and expose copper tip");
    const auto side_hash=fingerprint[0];
    for(auto& v:views)context->ClearRenderTargetView(v.Get(),clear);
    grip.orientation={.5F,.5F,.5F,.5F};
    Require(BuildReloadCartridgeOverlay(head,grip,true,out)&&
        compositor.Draw(device.Get(),context.Get(),out,123,raw)&&coverage(0)>0&&
        coverage(1)>0&&fingerprint[0]!=side_hash,"WARP must preserve grip roll about cartridge axis");
    const auto valid_overlay=out;
    for(auto& v:views)context->ClearRenderTargetView(v.Get(),clear);
    out.reload_cartridge_surface_count=17;
    Require(compositor.Draw(device.Get(),context.Get(),out,123,raw)&&coverage(0)==0&&coverage(1)==0,
        "unbounded optional surface count must suppress both cartridge eyes");
    out=valid_overlay;out.reload_cartridge_surfaces[0].head_corners[0].z=0;
    Require(compositor.Draw(device.Get(),context.Get(),out,123,raw)&&coverage(0)==0&&coverage(1)==0,
        "near-plane optional cartridge must suppress both eyes without failing scene draw");

    // Establish essential text/UI pixels, then require exactly the same images
    // with malformed optional geometry. This catches both missing HUD and a
    // cartridge erroneously surviving in the unclipped eye.
    out=valid_overlay;out.reload_cartridge_visible=false;
    out.eyes[1].eye_to_head.position.z=-.004F;
    std::copy_n(u"Keep HUD",8,out.text.hint.characters.begin());out.text.hint.length=8;
    out.ui.wheel.active=out.ui.wheel.valid=true;
    out.ui.wheel.available_mask=out.ui.wheel.owned_mask=1;out.ui.wheel.selected=0;
    std::copy_n(u"Pistol",6,out.ui.wheel.labels[0].characters.begin());out.ui.wheel.labels[0].length=6;
    for(auto& v:views)context->ClearRenderTargetView(v.Get(),clear);
    Require(compositor.Draw(device.Get(),context.Get(),out,123,raw)&&coverage(0)>0&&coverage(1)>0,
        "essential text and equipment UI baseline must render in both eyes");
    const auto essential_hash=fingerprint;
    const auto essential_overlay=out;
    for(int fault=0;fault<3;++fault){
        out=essential_overlay;out.reload_cartridge_visible=true;
        if(fault==0){
            out.reload_cartridge_surfaces[0].head_corners[0].z=-.051F;
            const auto corner=out.reload_cartridge_surfaces[0].head_corners[0];
            Require(corner.z-out.eyes[0].eye_to_head.position.z<-.05F&&
                corner.z-out.eyes[1].eye_to_head.position.z>=-.05F,
                "clipping regression fixture must invalidate only one captured eye");
        }else if(fault==1){
            out.reload_cartridge_surface_count=out.reload_cartridge_max_surfaces+1;
        }else{
            out.reload_cartridge_surfaces[0].head_corners[0].x=std::numeric_limits<float>::quiet_NaN();
        }
        for(auto& v:views)context->ClearRenderTargetView(v.Get(),clear);
        Require(compositor.Draw(device.Get(),context.Get(),out,123,raw)&&coverage(0)>0&&coverage(1)>0&&
            fingerprint==essential_hash,"invalid optional cartridge must preserve essential HUD exactly in both eyes");
    }
    out=valid_overlay;
    for(auto& v:views)context->ClearRenderTargetView(v.Get(),clear);
    Require(!BuildReloadCartridgeOverlay(head,grip,false,out),"hidden helper succeeded");
    Require(compositor.Draw(device.Get(),context.Get(),out,123,raw)&&coverage(0)==0&&coverage(1)==0,
        "hidden cartridge left visible pixels");
    std::cout<<"reload visual geometry/raster passed\n";
}
