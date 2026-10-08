#include "runtime/interaction_gaze.hpp"
#include "runtime/vr_math.hpp"
#include "backends/openvr/gameplay_ui_raster.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>
#include <wrl/client.h>

using namespace cojvr::runtime;
using namespace cojvr::backends::openvr;
using Microsoft::WRL::ComPtr;

namespace {
void Require(bool ok, const char* why) {
    if (!ok) { std::cerr << why << '\n'; std::exit(1); }
}
bool Near(float a, float b) { return std::abs(a-b)<1e-5F; }
bool Near(Vec3 a, Vec3 b) { return Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.z,b.z); }
Pose ValidPose() noexcept {
    Pose pose{};
    pose.position_valid=pose.orientation_valid=true;
    return pose;
}
EyeView EyeAt(float x) {
    EyeView eye{};
    eye.eye_to_head.position_valid=eye.eye_to_head.orientation_valid=true;
    eye.eye_to_head.position.x=x;
    eye.fov=FovFromTangents(-1,1,1,-1);
    return eye;
}
std::vector<std::uint32_t> ReadEye(ID3D11Device* device,
    ID3D11DeviceContext* context, ID3D11Texture2D* target) {
    D3D11_TEXTURE2D_DESC desc{}; target->GetDesc(&desc);
    desc.BindFlags=0; desc.Usage=D3D11_USAGE_STAGING;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    Require(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)),"readback allocation failed");
    context->CopyResource(staging.Get(),target);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Require(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"readback map failed");
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(desc.Width)*desc.Height);
    for(std::uint32_t y=0;y<desc.Height;++y) {
        const auto* row=reinterpret_cast<const std::uint32_t*>(
            static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch);
        std::copy_n(row,desc.Width,pixels.data()+y*desc.Width);
    }
    context->Unmap(staging.Get(),0);
    return pixels;
}
void GeometryAndProjection() {
    static_assert(std::is_trivially_copyable_v<StereoHudTextOverlay>);
    static_assert(noexcept(BuildInteractionHandOverlay(ValidPose(),ValidPose(),true,std::declval<StereoHudTextOverlay&>())));
    StereoHudTextOverlay overlay{};
    overlay.frame_sequence=41; overlay.feedback_context_token=73;
    overlay.eyes={EyeAt(-.032F),EyeAt(.032F)}; overlay.eyes[1].eye=Eye::right;
    overlay.text.hint.characters[0]=u'H'; overlay.text.hint.length=1;
    overlay.compass_surface_valid=overlay.status_surface_valid=true;
    overlay.compass_head_corners[0]={.1F,.2F,-.3F};
    overlay.status_head_corners[0]={.4F,.5F,-.6F};
    overlay.reload_cartridge_visible=true; overlay.reload_cartridge_surface_count=1;
    overlay.reload_cartridge_surfaces[0].head_corners[0]={.7F,.8F,-.9F};
    Require(BuildInteractionHandOverlay(ValidPose(),ValidPose(),true,overlay)&&overlay.interaction_gaze_visible,
        "visible gaze must produce captured metadata");
    for(const auto corner:overlay.interaction_gaze_head_corners)
        Require(Near(corner.z,-1.5F)&&std::isfinite(corner.x)&&std::isfinite(corner.y),
            "gaze ring must stay at finite head-space -Z depth");
    const auto& corners=overlay.interaction_gaze_head_corners;
    Require(corners[0].x<0&&corners[0].y>0&&corners[1].x>0&&corners[1].y>0&&
        corners[2].x<0&&corners[2].y<0&&corners[3].x>0&&corners[3].y<0,
        "gaze corners must retain triangle-strip order");
    Require(Near(corners[0].x,-corners[3].x)&&Near(corners[0].y,-corners[3].y)&&
        corners[1].x-corners[0].x<.05F,"gaze footprint must be small and centered on HMD forward");
    const auto preserve_other_surfaces=[&] {
        Require(overlay.frame_sequence==41&&overlay.feedback_context_token==73&&
            overlay.text.hint.view()==u"H"&&overlay.eyes[0].eye_to_head.position.x==-.032F&&
            overlay.compass_surface_valid&&overlay.status_surface_valid&&
            Near(overlay.compass_head_corners[0],{.1F,.2F,-.3F})&&
            Near(overlay.status_head_corners[0],{.4F,.5F,-.6F})&&
            overlay.reload_cartridge_visible&&overlay.reload_cartridge_surface_count==1&&
            Near(overlay.reload_cartridge_surfaces[0].head_corners[0],{.7F,.8F,-.9F}),
            "gaze helper must preserve the producing frame and existing HUD surfaces");
    };
    preserve_other_surfaces();
    GameplayUiRaster raster;
    const auto& panel=raster.InteractionGaze();
    HudPanelPlacement placement{}; placement.captured_quad=true; placement.head_corners=corners;
    ProjectedHudPanel left{},right{};
    Require(ProjectHudPanel(panel,overlay.eyes[0],1024,1024,placement,left)&&
        ProjectHudPanel(panel,overlay.eyes[1],1024,1024,placement,right),"gaze must project in both eyes");
    const auto center_x=[](const ProjectedHudPanel& p) {
        float center=0; for(const auto& c:p.clip_positions) center+=c[0]/c[3]; return center*.25F;
    };
    // Hand-derived: +/-32mm eye displacement at 1.5m gives +/-0.021333 NDC.
    Require(Near(center_x(left),.021333333F)&&Near(center_x(right),-.021333333F),
        "finite-depth gaze must converge with the correct binocular disparity");
    auto canted=overlay.eyes[0];
    canted.eye_to_head.orientation={0,.087155743F,0,.996194698F};
    canted.fov=FovFromTangents(-1.2F,.9F,1.1F,-.8F);
    Require(ProjectHudPanel(panel,canted,1024,1024,placement,left)&&
        left.clip_positions[0][3]!=left.clip_positions[1][3],
        "captured ring must preserve perspective under canted asymmetric optics");
    const auto captured=overlay;
    Require(!BuildInteractionHandOverlay(ValidPose(),ValidPose(),false,overlay)&&!overlay.interaction_gaze_visible,
        "hidden gaze must clear its visibility");
    for(auto corner:overlay.interaction_gaze_head_corners)
        Require(Near(corner,{}),"hidden gaze must clear stale captured geometry");
    preserve_other_surfaces();
    Require(captured.interaction_gaze_visible&&captured.interaction_gaze_head_corners[0].z==-1.5F,
        "a later hidden frame must not mutate an earlier capture");
}

// A head-centered fallback, omission of either translation, use of the head
// instead of the hand's rotation, or the wrong inverse-head rotation must fail.
// Expected corners below are hand-derived for +90deg hand yaw and +90deg
// head roll. They do not use the production rotation helper.
void HandCaptureAndInvalidation() {
    Pose head=ValidPose(), hand=ValidPose();
    head.position={1.F,2.F,3.F};
    head.orientation={0,0,.707106781F,.707106781F};
    hand.position={1.3F,1.6F,2.2F};
    hand.orientation={0,.707106781F,0,.707106781F};
    StereoHudTextOverlay overlay{};
    overlay.frame_sequence=11; overlay.feedback_context_token=29;
    overlay.text.hint.characters[0]=u'H'; overlay.text.hint.length=1;
    overlay.reload_cartridge_visible=true;
    Require(BuildInteractionHandOverlay(head,hand,true,overlay),
        "valid translated and rotated hand must capture a ring");
    constexpr std::array<Vec3,4> expected{{{-.388F,1.2F,-.788F},
        {-.388F,1.2F,-.812F},{-.412F,1.2F,-.788F},{-.412F,1.2F,-.812F}}};
    for(std::size_t i=0;i<expected.size();++i)
        Require(Near(overlay.interaction_gaze_head_corners[i],expected[i]),
            "ring must follow left-hand aim through translated and rotated head space");
    // Changing only the hand must change direction, without an HMD fallback.
    hand.orientation={};
    Require(BuildInteractionHandOverlay(head,hand,true,overlay),"second hand direction must capture");
    constexpr std::array<Vec3,4> straight{{{-.388F,-.288F,-2.3F},
        {-.388F,-.312F,-2.3F},{-.412F,-.288F,-2.3F},{-.412F,-.312F,-2.3F}}};
    for(std::size_t i=0;i<straight.size();++i)
        Require(Near(overlay.interaction_gaze_head_corners[i],straight[i]),
            "hand direction must remain independent of head forward");
    const auto captured=overlay;
    const auto check_cleared=[&] {
        Require(!overlay.interaction_gaze_visible,"invalid hand/head must hide the marker");
        for(const auto p:overlay.interaction_gaze_head_corners)
            Require(Near(p,{}),"invalid hand/head must clear all stale marker geometry");
        Require(overlay.frame_sequence==11&&overlay.feedback_context_token==29&&
            overlay.text.hint.view()==u"H"&&overlay.reload_cartridge_visible,
            "invalid hand marker must preserve unrelated frame and HUD metadata");
    };
    for(int owner=0;owner<2;++owner) for(int fault=0;fault<12;++fault) {
        auto bad_head=head, bad_hand=hand;
        auto& bad=owner==0?bad_head:bad_hand;
        if(fault==0) bad.position_valid=false;
        if(fault==1) bad.orientation_valid=false;
        if(fault==2) bad.position.x=std::numeric_limits<float>::quiet_NaN();
        if(fault==3) bad.position.y=std::numeric_limits<float>::infinity();
        if(fault==4) bad.position.z=-std::numeric_limits<float>::infinity();
        if(fault==5) bad.orientation.x=std::numeric_limits<float>::quiet_NaN();
        if(fault==6) bad.orientation.y=std::numeric_limits<float>::infinity();
        if(fault==7) bad.orientation.z=-std::numeric_limits<float>::infinity();
        if(fault==8) bad.orientation.w=std::numeric_limits<float>::quiet_NaN();
        if(fault==9) bad.orientation={0,0,0,0};
        if(fault==10) bad.orientation={0,0,0,2};
        if(fault==11) bad.orientation={0,0,0,.5F};
        overlay=captured;
        Require(!BuildInteractionHandOverlay(bad_head,bad_hand,true,overlay),
            "invalid pose must fail closed without normalizing or head fallback");
        check_cleared();
    }
    overlay=captured;
    Require(!BuildInteractionHandOverlay(head,hand,false,overlay),"hidden hand must clear a valid capture");
    check_cleared();
    Require(captured.interaction_gaze_visible&&Near(captured.interaction_gaze_head_corners[0],straight[0]),
        "invalid later frame must not alter an earlier hand capture");
}

void StereoComposition() {
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
    Require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP device creation failed");
    D3D11_TEXTURE2D_DESC desc{}; desc.Width=desc.Height=1024;
    desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM; desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    std::array<ComPtr<ID3D11Texture2D>,2> targets;
    std::array<ComPtr<ID3D11RenderTargetView>,2> views;
    for(std::size_t eye=0;eye<2;++eye)
        Require(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&targets[eye]))&&
            SUCCEEDED(device->CreateRenderTargetView(targets[eye].Get(),nullptr,&views[eye])),
            "eye target creation failed");
    const float world_color[]{.2F,.3F,.6F,1.F};
    const auto clear=[&] { for(auto& view:views) context->ClearRenderTargetView(view.Get(),world_color); };
    const auto read=[&] { return std::array<std::vector<std::uint32_t>,2>{
        ReadEye(device.Get(),context.Get(),targets[0].Get()),
        ReadEye(device.Get(),context.Get(),targets[1].Get())}; };
    clear(); const auto world=read();
    StereoHudTextOverlay overlay{}; overlay.frame_sequence=41;
    overlay.eyes={EyeAt(-.032F),EyeAt(.032F)}; overlay.eyes[1].eye=Eye::right;
    Require(BuildInteractionHandOverlay(ValidPose(),ValidPose(),true,overlay),"gaze capture fixture failed");
    HudTextCompositor compositor;
    const std::array<ID3D11Texture2D*,2> raw{targets[0].Get(),targets[1].Get()};
    Require(!compositor.Draw(device.Get(),context.Get(),overlay,42,raw)&&read()==world,
        "marker from a different captured frame must not render");
    Require(compositor.Draw(device.Get(),context.Get(),overlay,41,raw),"marker-only draw failed");
    const auto ring=read(); std::array<double,2> centers{};
    for(std::size_t eye=0;eye<2;++eye) {
        std::size_t changed=0,cyan=0; double sum_x=0;
        for(std::size_t i=0;i<ring[eye].size();++i) if(ring[eye][i]!=world[eye][i]) {
            ++changed; sum_x+=i%1024;
            const auto pixel=ring[eye][i];
            cyan+=((pixel>>8)&255)>((pixel>>16)&255)+20&&(pixel&255)>((pixel>>16)&255)+20;
        }
        Require(changed>0&&changed<200&&cyan>0,"small cyan ring must preserve almost all scene pixels");
        centers[eye]=sum_x/changed;
        const std::size_t center=eye==0?523:501;
        Require(ring[eye][512*1024+center]==world[eye][512*1024+center]&&
            ring[eye][0]==world[eye][0],"transparent center and outside footprint must preserve world");
    }
    Require(centers[0]>centers[1]&&ring[0]!=ring[1],"real stereo marker must retain correct convergence");
    clear(); Require(compositor.Draw(device.Get(),context.Get(),overlay,41,raw)&&read()==ring,
        "repeating the same capture must reuse an unchanged ring");
    const auto gaze_capture=overlay;
    (void)BuildInteractionHandOverlay(ValidPose(),ValidPose(),false,overlay); clear();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,41,raw)&&read()==world,
        "hidden marker must leave no stale pixels in either eye");
    overlay=gaze_capture; clear();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,41,raw)&&read()==ring,
        "hidden cache must recover without changing marker pixels");

    // Exercise all seven existing slots before introducing slot eight.
    overlay=gaze_capture; (void)BuildInteractionHandOverlay(ValidPose(),ValidPose(),false,overlay);
    std::copy_n(u"Keep HUD",8,overlay.text.hint.characters.begin()); overlay.text.hint.length=8;
    std::copy_n(u"Action",6,overlay.text.interaction.characters.begin()); overlay.text.interaction.length=6;
    std::copy_n(u"Dialogue",8,overlay.text.subtitle.characters.begin()); overlay.text.subtitle.length=8;
    overlay.ui.wheel.active=overlay.ui.wheel.valid=true;
    overlay.ui.wheel.available_mask=overlay.ui.wheel.owned_mask=1; overlay.ui.wheel.selected=0;
    std::copy_n(u"Pistol",6,overlay.ui.wheel.labels[0].characters.begin()); overlay.ui.wheel.labels[0].length=6;
    overlay.ui.compass.active=overlay.compass_surface_valid=true;
    overlay.ui.compass.north_direction={0,1};
    overlay.compass_head_corners={Vec3{-.3F,.1F,-.7F},Vec3{-.2F,.1F,-.7F},
        Vec3{-.3F,0,-.7F},Vec3{-.2F,0,-.7F}};
    overlay.ui.status.active=overlay.status_surface_valid=true; overlay.ui.status.line_count=1;
    std::copy_n(u"Ammo 3",6,overlay.ui.status.lines[0].characters.begin()); overlay.ui.status.lines[0].length=6;
    overlay.status_head_corners={Vec3{.2F,.1F,-.7F},Vec3{.3F,.1F,-.7F},
        Vec3{.2F,0,-.7F},Vec3{.3F,0,-.7F}};
    overlay.reload_cartridge_visible=true;
    overlay.reload_cartridge_head_corners={Vec3{.10F,-.1F,-.7F},Vec3{.13F,-.1F,-.7F},
        Vec3{.10F,-.18F,-.7F},Vec3{.13F,-.18F,-.7F}};
    overlay.eyes[1].eye_to_head.position.z=-.004F;
    clear(); Require(compositor.Draw(device.Get(),context.Get(),overlay,41,raw),"existing HUD baseline failed");
    const auto essential=read();
    Require(essential[0]!=world[0]&&essential[1]!=world[1],"essential HUD must render before marker faults");
    // No existing slot is merely populated but silently lost: removing each
    // one must change actual output before testing marker fault isolation.
    for(int slot=0;slot<7;++slot) {
        auto missing=overlay;
        if(slot==0) missing.text.hint={};
        if(slot==1) missing.text.interaction={};
        if(slot==2) missing.text.subtitle={};
        if(slot==3) missing.ui.wheel.active=false;
        if(slot==4) missing.ui.compass.active=false;
        if(slot==5) missing.ui.status.active=false;
        if(slot==6) missing.reload_cartridge_visible=false;
        clear(); Require(compositor.Draw(device.Get(),context.Get(),missing,41,raw)&&read()!=essential,
            "all seven original compositor slots must still contribute pixels");
    }
    const auto essential_capture=overlay;
    for(int fault=0;fault<4;++fault) {
        overlay=essential_capture; (void)BuildInteractionHandOverlay(ValidPose(),ValidPose(),true,overlay);
        if(fault==0) overlay.interaction_gaze_head_corners[0].x=std::numeric_limits<float>::quiet_NaN();
        if(fault==1) overlay.interaction_gaze_head_corners[0].z=.1F;
        if(fault==2) {
            overlay.interaction_gaze_head_corners[0].z=-.051F;
            Require(overlay.interaction_gaze_head_corners[0].z-overlay.eyes[0].eye_to_head.position.z<-.05F&&
                overlay.interaction_gaze_head_corners[0].z-overlay.eyes[1].eye_to_head.position.z>=-.05F,
                "one-eye clipping fixture must invalidate only one eye");
        }
        if(fault==3) overlay.interaction_gaze_head_corners={};
        clear(); Require(compositor.Draw(device.Get(),context.Get(),overlay,41,raw)&&read()==essential,
            "invalid optional gaze must hide in both eyes and preserve existing HUD exactly");
    }
    // Invalid right-eye optics must also suppress the otherwise valid left ring,
    // while the existing HUD follows its own captured-eye projection gates.
    for(int fault=0;fault<3;++fault) {
        overlay=essential_capture;
        if(fault==0) overlay.eyes[1].eye_to_head.orientation_valid=false;
        if(fault==1) overlay.eyes[1].eye_to_head.position.x=std::numeric_limits<float>::infinity();
        if(fault==2) overlay.eyes[1].fov.angle_right=std::numeric_limits<float>::quiet_NaN();
        clear(); Require(compositor.Draw(device.Get(),context.Get(),overlay,41,raw),"invalid optics HUD baseline failed");
        const auto invalid_optics_baseline=read();
        (void)BuildInteractionHandOverlay(ValidPose(),ValidPose(),true,overlay); clear();
        Require(compositor.Draw(device.Get(),context.Get(),overlay,41,raw)&&read()==invalid_optics_baseline,
            "one invalid eye must suppress both ring images without altering existing HUD");
    }
    overlay=essential_capture; (void)BuildInteractionHandOverlay(ValidPose(),ValidPose(),true,overlay); clear();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,41,raw),"combined marker/HUD draw failed");
    const auto combined=read();
    Require(combined[0]!=essential[0]&&combined[1]!=essential[1],"marker must coexist with essential HUD");
    // All changed marker pixels remain near head-forward; old slot pixels stay intact.
    for(std::size_t eye=0;eye<2;++eye) for(std::size_t i=0;i<combined[eye].size();++i)
        if(i%1024<480||i%1024>544||i/1024<496||i/1024>528)
            Require(combined[eye][i]==essential[eye][i],"eighth slot must not corrupt existing compositor slots");
    compositor.Reset(); clear();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,41,raw)&&read()==combined,
        "resource reset must reconstruct the immutable gaze cache");
}
}
int main() {
    GeometryAndProjection(); HandCaptureAndInvalidation(); StereoComposition();
    std::cout<<"interaction hand capture, stereo ring and optional-failure isolation passed\n";
}
