#include "backends/openvr/hud_text_overlay.hpp"
#include "runtime/vr_math.hpp"
#include "runtime/action_prompts.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <wrl/client.h>
#include <fstream>

using Microsoft::WRL::ComPtr;

std::vector<std::uint32_t> ReadEye(ID3D11Device* device,
    ID3D11DeviceContext* context, ID3D11Texture2D* target) {
    D3D11_TEXTURE2D_DESC desc{}; target->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) std::exit(1);
    context->CopyResource(staging.Get(), target);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) std::exit(1);
    std::vector<std::uint32_t> result(static_cast<std::size_t>(desc.Width) * desc.Height);
    for (UINT y = 0; y < desc.Height; ++y)
        std::copy_n(reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(mapped.pData) + y*mapped.RowPitch), desc.Width, result.data() + y*desc.Width);
    context->Unmap(staging.Get(), 0);
    return result;
}

using namespace cojvr::backends::openvr;
using namespace cojvr::runtime;
void Require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
template<class Compositor> bool DrawGuideGate(Compositor& compositor,ID3D11Device* device,
    ID3D11DeviceContext* context,const StereoHudTextOverlay& overlay,
    const std::array<ID3D11Texture2D*,2>& targets,bool enabled){
    if constexpr(requires{compositor.Draw(device,context,overlay,7,targets,enabled);})
        return compositor.Draw(device,context,overlay,7,targets,enabled);
    return compositor.Draw(device,context,overlay,7,targets);
}
HudText Text(std::u16string_view value) {
    HudText text{};
    text.length = static_cast<std::uint32_t>(value.size());
    std::copy(value.begin(), value.end(), text.characters.begin());
    return text;
}
int main(int argc, char** argv) {
    HudTextRaster raster;
    const auto text = Text(u"Interacción: recoge el revólver. ¡Apunta con cuidado!\nSegunda línea: caballo, niño, acción.");
    Require(text.view().find(u'\u00F3') != std::u16string_view::npos &&
        text.view().find(u'\u00F1') != std::u16string_view::npos,
        "fixture source must preserve actual UTF16 accented code points");
    const auto& panel = raster.Render(text);
    Require(panel.width > 0 && panel.height > 0 && !panel.pixels.empty(), "visible native Unicode text must rasterize");
    Require(std::all_of(panel.pixels.begin(), panel.pixels.end(), [](auto p) { return (p >> 24) == 255; }), "text panel must have defined opaque coverage");
    Require(std::any_of(panel.pixels.begin(), panel.pixels.end(), [](auto p) { return (p & 0xFFFFFF) != 0x151A20; }), "text glyphs must exist");
    const auto* cached = panel.pixels.data();
    const auto count = raster.rebuilds();
    Require(raster.Render(text).pixels.data() == cached && raster.rebuilds() == count, "unchanged text must reuse raster");
    EyeView left{}, right{};
    left.eye_to_head.orientation_valid = left.eye_to_head.position_valid = true;
    left.eye_to_head.orientation.w = 1;
    left.eye_to_head.position.x = -0.032F;
    left.fov = {-0.75F, 0.85F, 0.75F, -0.8F};
    right = left;
    right.eye_to_head.position.x = 0.032F;
    ProjectedHudPanel a{}, b{};
    Require(ProjectHudPanel(panel, left, 1600, 1600, {}, a) && ProjectHudPanel(panel, right, 1600, 1600, {}, b), "valid stereo optics must project");
    Require(a.left > b.left, "finite depth panel must have correct binocular disparity");
    Require(a.left + a.width <= 1600 && a.top + a.height <= 1600, "projection bounds must remain within eye");
    HudPanelPlacement wrist{};
    wrist.captured_quad = true;
    wrist.head_corners = {{{-.15F,.05F,-.45F},{-.05F,.05F,-.45F},
        {-.15F,-.05F,-.45F},{-.05F,-.05F,-.45F}}};
    ProjectedHudPanel wrist_left{}, wrist_right{};
    Require(ProjectHudPanel(panel,left,1600,1600,wrist,wrist_left) &&
        ProjectHudPanel(panel,right,1600,1600,wrist,wrist_right),
        "tracked wrist surface must project at close physical distance");
    Require(wrist_left.left + wrist_left.width < a.left + a.width/2 &&
        wrist_left.left - wrist_right.left > a.left - b.left,
        "captured wrist position and close binocular disparity must replace head panel geometry");
    wrist.head_corners[0].z = .1F;
    Require(!ProjectHudPanel(panel,left,1600,1600,wrist,wrist_left),
        "quad crossing behind an eye must fail closed");
    wrist.head_corners[0].z = -.45F;
    wrist.head_corners[2].x = std::numeric_limits<float>::quiet_NaN();
    Require(!ProjectHudPanel(panel,left,1600,1600,wrist,wrist_left),
        "invalid captured wrist corner must fail closed");
    left.eye_to_head.orientation = {0, 0.087156F, 0, 0.996195F};
    Require(ProjectHudPanel(panel, left, 1600, 1600, {}, a), "canted eye projection must be supported");
    Require(a.clip_positions[0][3] != a.clip_positions[1][3], "canted panel must preserve perspective-correct corner depths");
    left.eye_to_head.position.x = std::numeric_limits<float>::quiet_NaN();
    Require(!ProjectHudPanel(panel, left, 1600, 1600, {}, a) && a.width == 0, "invalid optics must clear projected panel");
    const auto& empty = raster.Render({});
    Require(empty.pixels.empty() && empty.width == 0, "hidden text must clear cached panel");
    auto bad = text; bad.length = HudText::capacity;
    Require(raster.Render(bad).pixels.empty(), "invalid bounded text must remain hidden");

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    Require(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        &device, nullptr, &context)), "host WARP compositor device must initialize");
    std::array<ComPtr<ID3D11Texture2D>, 2> targets;
    D3D11_TEXTURE2D_DESC desc{}; desc.Width = desc.Height = 1600;
    desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    const float world_color[]{0.2F, 0.3F, 0.6F, 1};
    const auto clear_world = [&]() {
        for (auto& target : targets) {
            if (!target) Require(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &target)), "owned eye texture must initialize");
            ComPtr<ID3D11RenderTargetView> rtv;
            Require(SUCCEEDED(device->CreateRenderTargetView(target.Get(),nullptr,&rtv)), "owned eye RTV must initialize");
            context->ClearRenderTargetView(rtv.Get(),world_color);
        }
    };
    clear_world();
    const auto world = ReadEye(device.Get(), context.Get(), targets[0].Get());
    StereoHudTextOverlay overlay{}; overlay.frame_sequence = 7;
    overlay.eyes = {right, right};
    overlay.eyes[0].eye_to_head.position.x = -.032F;
    overlay.eyes[0].eye_to_head.orientation = {0, .087156F, 0, .996195F};
    overlay.eyes[1].eye_to_head.orientation = {0, -.087156F, 0, .996195F};
    overlay.text.hint = Text(u"Tutorial: usa el entorno para avanzar.\nAgáchate, salta y recoge los objetos que encuentres.");
    overlay.text.interaction = Text(u"Pulsa F para recoger el revólver");
    overlay.text.subtitle = Text(u"¿Quién anda ahí? ¡Sal de ahí, muchacho!");
    HudTextCompositor compositor;
    const std::array<ID3D11Texture2D*, 2> raw{targets[0].Get(),targets[1].Get()};
    Require(compositor.Draw(device.Get(), context.Get(), overlay,7,raw), "production text shaders must render on WARP");
    const auto rendered_left = ReadEye(device.Get(),context.Get(),targets[0].Get());
    const auto rendered_right = ReadEye(device.Get(),context.Get(),targets[1].Get());
    Require(rendered_left != world && rendered_left != rendered_right, "real composition must show text and distinct stereo geometry");
    std::size_t changed = 0;
    for (std::size_t i=0; i<world.size(); ++i) changed += world[i]!=rendered_left[i];
    Require(changed > 1000 && changed < world.size()/5, "bounded HUD panels must preserve most world pixels");
    Require(world[0]==rendered_left[0] && world[world.size()/2]==rendered_left[world.size()/2], "text must leave outside-panel world untouched");
    if (argc > 1) {
        BITMAPFILEHEADER file{}; file.bfType = 0x4D42;
        file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
        file.bfSize = file.bfOffBits + static_cast<DWORD>(rendered_left.size()*4);
        BITMAPINFOHEADER header{}; header.biSize = sizeof(header);
        header.biWidth = 1600; header.biHeight = -1600; header.biPlanes = 1; header.biBitCount = 32;
        std::ofstream out(argv[1], std::ios::binary);
        out.write(reinterpret_cast<const char*>(&file),sizeof(file));
        out.write(reinterpret_cast<const char*>(&header),sizeof(header));
        out.write(reinterpret_cast<const char*>(rendered_left.data()),rendered_left.size()*4);
    }
    clear_world();
    Require(!compositor.Draw(device.Get(),context.Get(),overlay,8,raw) &&
        ReadEye(device.Get(),context.Get(),targets[0].Get()) == world,
        "mismatched frame text must never contaminate a fresh world copy");
    overlay.text = {};
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,raw) &&
        ReadEye(device.Get(),context.Get(),targets[0].Get()) == world,
        "empty native snapshot must retain fresh world with no stale HUD");
    overlay.text.hint = Text(u"New scene: fresh native hint");
    const auto valid_eyes = overlay.eyes;
    overlay.eyes[1].eye_to_head.position.x = std::numeric_limits<float>::quiet_NaN();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,raw) &&
        ReadEye(device.Get(),context.Get(),targets[0].Get()) == world,
        "invalid second eye must suppress the whole stereo panel");
    overlay.eyes = valid_eyes;
    compositor.Reset();
    Require(compositor.Prepare(device.Get()) && compositor.Draw(device.Get(),context.Get(),overlay,7,raw),
        "explicit renderer shutdown must allow clean text-pipeline reinitialization");
    targets = {};
    desc.Width = desc.Height = 512;
    clear_world();
    const auto resized_world = ReadEye(device.Get(),context.Get(),targets[0].Get());
    const std::array<ID3D11Texture2D*,2> resized{targets[0].Get(),targets[1].Get()};
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,resized) &&
        ReadEye(device.Get(),context.Get(),targets[0].Get()) != resized_world,
        "same-device texture resize must rebuild owned target views");
    targets = {}; context.Reset(); device.Reset();
    Require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),
        "replacement presenter device must initialize");
    clear_world();
    const std::array<ID3D11Texture2D*,2> replaced{targets[0].Get(),targets[1].Get()};
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced) &&
        ReadEye(device.Get(),context.Get(),targets[0].Get()) != resized_world,
        "device replacement must upload cached text onto the new owner");
    clear_world();overlay.text={};
    overlay.ui.wheel.active=overlay.ui.wheel.valid=true;
    overlay.ui.wheel.available_mask=3;overlay.ui.wheel.selected=1;
    overlay.ui.compass.active=true;overlay.ui.compass.north_direction={0,1};
    overlay.ui.compass.marker_count=1;overlay.ui.compass.markers[0].direction={1,0};
    overlay.compass_surface_valid=true;
    overlay.compass_head_corners={{{-.22F,-.10F,-.4F},{-.10F,-.10F,-.4F},
        {-.22F,-.22F,-.4F},{-.10F,-.22F,-.4F}}};
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced),
        "wheel and tracked wrist UI must compose onto replacement presenter device");
    const auto ui_left=ReadEye(device.Get(),context.Get(),targets[0].Get());
    const auto ui_right=ReadEye(device.Get(),context.Get(),targets[1].Get());
    Require(ui_left!=resized_world&&ui_left!=ui_right&&ui_left[0]==resized_world[0],
        "real UI pixels must preserve outside world and distinct binocular geometry");
    clear_world();overlay.ui={};overlay.compass_surface_valid=false;
    overlay.ui.status.active=true;overlay.ui.status.line_count=1;
    const std::u16string_view status=u"Salud  087";
    overlay.ui.status.lines[0].length=static_cast<std::uint32_t>(status.size());
    std::copy(status.begin(),status.end(),overlay.ui.status.lines[0].characters.begin());
    overlay.status_surface_valid=true;
    overlay.status_head_corners={{{-.22F,-.10F,-.4F},{-.06F,-.10F,-.4F},
        {-.22F,-.15F,-.4F},{-.06F,-.15F,-.4F}}};
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced),"status-only native HUD must compose");
    const auto status_left=ReadEye(device.Get(),context.Get(),targets[0].Get());
    Require(status_left!=resized_world&&status_left!=ReadEye(device.Get(),context.Get(),targets[1].Get())&&status_left[0]==resized_world[0],
        "status must have real binocular wrist pixels while preserving the world outside");
    clear_world();overlay.status_surface_valid=false;
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced)&&ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,
        "invalid status surface resurrected a stale wrist panel");
    clear_world();overlay.ui={};overlay.compass_surface_valid=false;
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,
        "UI owner loss must remove all graphics from a fresh world frame");
    overlay.mission_timer=Text(u"Time left\n51");
    HudTextRaster timer_raster,hint_raster,notice_raster;
    const auto& long_timer=timer_raster.Render(Text(u"Time remaining to escape the mine\n51"),true);
    const auto& long_hint=hint_raster.Render(Text(std::u16string(800,u'h')));
    const auto& long_notice=notice_raster.Render(Text(std::u16string(800,u'n')));
    const std::array<const HudTextPanel*,3> long_panels{&long_timer,&long_hint,&long_notice};
    std::array<HudPanelPlacement,3> fitted{};
    Require(FitCriticalHudPanels(long_panels,overlay.eyes,fitted),"concurrent critical text stack did not fit captured optics");
    float lower_edge=.465F;
    for(unsigned i=0;i<3;++i){
        const auto& p=*long_panels[i];const auto& f=fitted[i];const float half=f.width_m*p.height/p.width*.5F;
        Require(f.center_y_m-half>=lower_edge,"long timer/hint/notices overlapped threat ring or another panel");
        lower_edge=f.center_y_m+half;
        for(const auto& e:overlay.eyes){ProjectedHudPanel projected{};
            Require(ProjectHudPanel(p,e,512,512,f,projected),"fitted critical panel projection failed");
            for(const auto& c:projected.clip_positions)Require(std::abs(c[0]/c[3])<=.96F&&std::abs(c[1]/c[3])<=.96F,
                "critical text clipped outside one eye after fit");
        }
    }
    overlay.mission_notices=Text(u"Objectives updated\nEscape before time runs out");
    overlay.threats.count=2;overlay.threats.markers[0]={{-1,0},1,false};
    overlay.threats.markers[1]={{1,0},.5F,true};
    clear_world();Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced),"critical HUD composition failed");
    const auto alerts_left=ReadEye(device.Get(),context.Get(),targets[0].Get());
    Require(alerts_left!=resized_world&&alerts_left!=ReadEye(device.Get(),context.Get(),targets[1].Get())&&alerts_left[0]==resized_world[0],
        "critical alerts must have binocular pixels without obscuring outside world");
    Require(std::any_of(alerts_left.begin(),alerts_left.end(),[](auto p){return ((p>>8)&255)>((p>>16)&255)+80;})&&
        std::any_of(alerts_left.begin(),alerts_left.end(),[](auto p){return ((p>>16)&255)>((p>>8)&255)+60;}),
        "direction green and damage red must both reach real GPU output");
    overlay.threats.markers[1].alpha=1;
    clear_world();Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced),"full-alpha threat draw failed");
    const auto opaque_alerts=ReadEye(device.Get(),context.Get(),targets[0].Get());
    bool native_fade=false;
    for(std::size_t i=0;i<opaque_alerts.size();++i)
        if(((opaque_alerts[i]>>16)&255)>230&&((opaque_alerts[i]>>8)&255)<100&&
            ((alerts_left[i]>>16)&255)>130&&((alerts_left[i]>>16)&255)<190)native_fade=true;
    Require(native_fade,"observed native half-alpha must blend without a new renderer fade clock");
    if(argc>2){
        BITMAPFILEHEADER file{};file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);
        file.bfSize=file.bfOffBits+static_cast<DWORD>(alerts_left.size()*4);
        BITMAPINFOHEADER header{};header.biSize=sizeof(header);header.biWidth=512;header.biHeight=-512;
        header.biPlanes=1;header.biBitCount=32;
        std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));
        out.write(reinterpret_cast<const char*>(&header),sizeof(header));
        out.write(reinterpret_cast<const char*>(alerts_left.data()),alerts_left.size()*4);
    }
    overlay.mission_timer={};overlay.mission_notices={};overlay.threats={};
    clear_world();Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,"critical owner loss resurrected cached alerts");
    overlay.threats.count=1;overlay.threats.markers[0]={{1,0},1,true};
    overlay.eyes[1].eye_to_head.position.x=std::numeric_limits<float>::quiet_NaN();
    clear_world();Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,"invalid second eye must suppress both threat eyes");
    HudPanelPlacement threat_place{};
    Require(!BuildThreatPanelPlacement({{1,0},1.1F,true},threat_place)&&
        !BuildThreatPanelPlacement({{0,0},1,true},threat_place),"invalid threat alpha/direction accepted");
    overlay={};overlay.frame_sequence=7;overlay.eyes=valid_eyes;
    overlay.holster_guides.count=1;
    auto& guide=overlay.holster_guides.zones[0];
    guide.head_corners={{{-.2F,.1F,-.6F},{.2F,.1F,-.6F},{-.2F,-.3F,-.6F},{.2F,-.3F,-.6F}}};
    clear_world();Require(DrawGuideGate(compositor,device.Get(),context.Get(),overlay,replaced,true),"guide composition failed");
    const auto guide_left=ReadEye(device.Get(),context.Get(),targets[0].Get());
    Require(guide_left!=resized_world&&guide_left!=ReadEye(device.Get(),context.Get(),targets[1].Get())&&
        guide_left.front()==resized_world.front(),"holster guides need bounded real binocular pixels");
    guide.kind=GestureGuideKind::support_grip;clear_world();
    Require(DrawGuideGate(compositor,device.Get(),context.Get(),overlay,replaced,true)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())!=guide_left,
        "support grip must show its own L1 glyph instead of a holster ring");
    const auto support_left=ReadEye(device.Get(),context.Get(),targets[0].Get());
    if(argc>3){
        BITMAPFILEHEADER file{};file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);
        file.bfSize=file.bfOffBits+static_cast<DWORD>(support_left.size()*4);
        BITMAPINFOHEADER header{};header.biSize=sizeof(header);header.biWidth=512;header.biHeight=-512;
        header.biPlanes=1;header.biBitCount=32;
        std::ofstream preview(argv[3],std::ios::binary);preview.write(reinterpret_cast<const char*>(&file),sizeof(file));
        preview.write(reinterpret_cast<const char*>(&header),sizeof(header));
        preview.write(reinterpret_cast<const char*>(support_left.data()),support_left.size()*4);
    }
    guide.state=1;clear_world();
    Require(DrawGuideGate(compositor,device.Get(),context.Get(),overlay,replaced,true)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())!=support_left,"near-hand guide must change its visible state");
    clear_world();Require(DrawGuideGate(compositor,device.Get(),context.Get(),overlay,replaced,false)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,"disabled preference resurrected a captured guide");
    overlay.eyes[1].eye_to_head.position_valid=false;clear_world();
    Require(DrawGuideGate(compositor,device.Get(),context.Get(),overlay,replaced,true)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,"invalid second eye retained a monocular guide");
    overlay.eyes=valid_eyes;guide.head_corners[0].x=std::numeric_limits<float>::quiet_NaN();clear_world();
    Require(DrawGuideGate(compositor,device.Get(),context.Get(),overlay,replaced,true)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,"invalid guide corner retained optional pixels");
    // Ammunition has its own pixels and cannot survive a native owner change,
    // a missing owner, disabled preference or invalid binocular geometry.
    guide.head_corners={{{-.2F,.1F,-.6F},{.2F,.1F,-.6F},{-.2F,-.3F,-.6F},{.2F,-.3F,-.6F}}};
    guide.kind=GestureGuideKind::ammunition_pickup;guide.state=0;
    overlay.reload_feedback.owner_token=42;clear_world();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,true,42),"ammo composition failed");
    const auto ammo_left=ReadEye(device.Get(),context.Get(),targets[0].Get());
    Require(ammo_left!=resized_world&&ammo_left!=guide_left&&ammo_left!=support_left&&
        ammo_left!=ReadEye(device.Get(),context.Get(),targets[1].Get()),"ammo needs distinct real binocular pixels");
    guide.state=1;clear_world();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,true,42)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())!=ammo_left,"ready ammo must change its pixels");
    for(const auto token:{0ULL,43ULL}){clear_world();
        Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,true,token)&&
            ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,"stale/absent owner replayed an ammo marker");}
    clear_world();Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,false,42)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,"disabled guides replayed ammo");
    compositor.Reset();clear_world();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,true,42)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())!=resized_world,"reset lost valid ammo pixels");
    overlay.eyes[1].eye_to_head.position_valid=false;clear_world();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,true,42)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world&&
        ReadEye(device.Get(),context.Get(),targets[1].Get())==resized_world,"invalid second eye retained monocular ammo pixels");
    overlay.eyes=valid_eyes;
    guide.head_corners[0].x=std::numeric_limits<float>::quiet_NaN();clear_world();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,true,42)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,"invalid ammo geometry retained optional pixels");
    overlay.text.hint=Text(u"Native hint");clear_world();
    Require(DrawGuideGate(compositor,device.Get(),context.Get(),overlay,replaced,true)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())!=resized_world,"bad optional guide starved essential HUD text");
    // A queued control label must not survive its runtime binding generation.
    overlay={};overlay.frame_sequence=7;overlay.eyes=valid_eyes;
    overlay.text.hint=Text(u"Press left menu");
    overlay.text.interaction=Text(u"Release right trigger");
    overlay.text.controller_prompt_generation=17;
    ActionPromptSnapshot prompts{};prompts.active=true;prompts.binding_generation=17;
    PublishActionPrompts(prompts);clear_world();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())!=resized_world,
        "current generation control text was hidden");
    prompts.binding_generation=18;PublishActionPrompts(prompts);clear_world();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world&&
        ReadEye(device.Get(),context.Get(),targets[1].Get())==resized_world,
        "binding replacement replayed queued control text");
    overlay.text.subtitle=Text(u"Native dialogue");clear_world();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced),"subtitle draw failed");
    const auto subtitle_only=ReadEye(device.Get(),context.Get(),targets[0].Get());
    Require(subtitle_only!=resized_world,"binding replacement hid native dialogue");
    overlay.text.hint={};overlay.text.interaction={};overlay.text.controller_prompt_generation=0;
    clear_world();Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())==subtitle_only,
        "stale bindings changed dialogue pixels");
    overlay.text.subtitle={};overlay.text.interaction=Text(u"Bring the round to the gun");
    overlay.reload_instruction=true;overlay.reload_feedback.owner_token=42;
    overlay.reload_instruction_until_ms=GetTickCount64()+60000;
    clear_world();Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,true,42)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())!=resized_world,
        "current reload instruction was hidden");
    for(const auto token:{0ULL,43ULL}){clear_world();
        Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,true,token)&&
            ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,
            "missing/replaced owner replayed a queued reload instruction");}
    clear_world();Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,false,42)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,
        "disabled guides replayed a queued reload instruction");
    for(const auto until:{0ULL,1ULL}){
        overlay.reload_instruction_until_ms=until;clear_world();
        Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,true,42)&&
            ReadEye(device.Get(),context.Get(),targets[0].Get())==resized_world,
            "expired/unbounded reload instruction survived its deadline");}
    overlay.reload_instruction=false;clear_world();
    Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,false,0)&&
        ReadEye(device.Get(),context.Get(),targets[0].Get())!=resized_world,
        "reload ownership gating hid ordinary native text");
    // Repeat submissions never call Draw: retirement must undo only the text,
    // preserving the world, subtitle and even an overlapping HUD surface.
    overlay={};overlay.frame_sequence=7;overlay.eyes=valid_eyes;
    overlay.text.subtitle=Text(u"Native dialogue survives retirement");
    overlay.ui.status.active=true;overlay.ui.status.line_count=1;
    overlay.ui.status.lines[0].length=static_cast<std::uint32_t>(status.size());
    std::copy(status.begin(),status.end(),overlay.ui.status.lines[0].characters.begin());
    overlay.status_surface_valid=true;
    overlay.status_head_corners={{{-.7F,.8F,-1.5F},{.7F,.8F,-1.5F},
        {-.7F,-.5F,-1.5F},{.7F,-.5F,-1.5F}}};
    clear_world();Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced),
        "retirement background HUD failed");
    const std::array<std::vector<std::uint32_t>,2> background{
        ReadEye(device.Get(),context.Get(),targets[0].Get()),
        ReadEye(device.Get(),context.Get(),targets[1].Get())};
    const auto expect_pixels=[&](const auto& expected,const char* message){
        for(unsigned eye=0;eye<2;++eye)
            Require(ReadEye(device.Get(),context.Get(),targets[eye].Get())==expected[eye],message);
    };
    overlay.text.hint=Text(u"Press left menu");
    overlay.text.interaction=Text(u"Release right trigger");
    overlay.text.controller_prompt_generation=101;
    prompts.active=true;prompts.binding_generation=101;PublishActionPrompts(prompts);
    const auto draw_controlled=[&](){
        clear_world();
        Require(compositor.Draw(device.Get(),context.Get(),overlay,7,replaced,true,42),
            "controlled repeat fixture failed");
        for(unsigned eye=0;eye<2;++eye)
            Require(ReadEye(device.Get(),context.Get(),targets[eye].Get())!=background[eye],
                "controlled repeat fixture drew no text");
    };
    draw_controlled();
    const std::array<std::vector<std::uint32_t>,2> controlled{
        ReadEye(device.Get(),context.Get(),targets[0].Get()),
        ReadEye(device.Get(),context.Get(),targets[1].Get())};
    compositor.RetireStaleText(device.Get(),context.Get(),replaced,true,42);
    expect_pixels(controlled,"valid repeated control text was retired");
    prompts.binding_generation=102;PublishActionPrompts(prompts);
    compositor.RetireStaleText(device.Get(),context.Get(),replaced,true,42);
    expect_pixels(background,"repeat retirement damaged world/subtitle/overlapping HUD pixels");
    // Once consumed, a preimage can never replay over subsequent output.
    clear_world();compositor.RetireStaleText(device.Get(),context.Get(),replaced,true,42);
    for(const auto& target:targets)Require(ReadEye(device.Get(),context.Get(),target.Get())==resized_world,
        "repeat retirement replayed a consumed preimage");
    // Inactive tombstones still carry a new epoch and invalidate old pixels.
    prompts.binding_generation=101;PublishActionPrompts(prompts);draw_controlled();
    prompts.active=false;prompts.binding_generation=103;PublishActionPrompts(prompts);
    compositor.RetireStaleText(device.Get(),context.Get(),replaced,true,42);
    expect_pixels(background,"inactive tombstone retained old control pixels");
    // Swapping the pair, replacing one target or changing context/device must
    // never copy a saved rectangle to a different output identity.
    prompts.active=true;prompts.binding_generation=101;PublishActionPrompts(prompts);
    ComPtr<ID3D11Texture2D> foreign;
    Require(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&foreign)),"foreign eye allocation failed");
    context->CopyResource(foreign.Get(),targets[0].Get());
    const auto foreign_pixels=ReadEye(device.Get(),context.Get(),foreign.Get());
    ComPtr<ID3D11DeviceContext> foreign_context;
    Require(SUCCEEDED(device->CreateDeferredContext(0,&foreign_context)),"foreign context failed");
    ComPtr<ID3D11Device> foreign_device;ComPtr<ID3D11DeviceContext> other_context;
    Require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,
        &foreign_device,nullptr,&other_context)),"foreign device failed");
    for(unsigned mismatch=0;mismatch<4;++mismatch){
        draw_controlled();prompts.binding_generation=102;PublishActionPrompts(prompts);
        auto wrong=replaced;
        if(mismatch==0)std::swap(wrong[0],wrong[1]);
        if(mismatch==1)wrong[1]=foreign.Get();
        compositor.RetireStaleText(mismatch==3?foreign_device.Get():device.Get(),
            mismatch==2?foreign_context.Get():mismatch==3?other_context.Get():context.Get(),wrong,true,42);
        expect_pixels(controlled,"mismatched identity wrote original targets");
        Require(ReadEye(device.Get(),context.Get(),foreign.Get())==foreign_pixels,
            "mismatched identity restored into a foreign target");
        compositor.RetireStaleText(device.Get(),context.Get(),replaced,true,42);
        expect_pixels(controlled,"mismatched identity retained a replayable preimage");
        prompts.binding_generation=101;PublishActionPrompts(prompts);
    }
    draw_controlled();compositor.DiscardRepeatedText();
    clear_world();prompts.binding_generation=102;PublishActionPrompts(prompts);
    compositor.RetireStaleText(device.Get(),context.Get(),replaced,true,42);
    for(const auto& target:targets)Require(ReadEye(device.Get(),context.Get(),target.Get())==resized_world,
        "discard restored an old preimage over a fresh non-HUD frame");
    prompts.binding_generation=101;PublishActionPrompts(prompts);draw_controlled();
    auto fresh=overlay;fresh.text.hint={};fresh.text.interaction={};
    fresh.text.controller_prompt_generation=0;
    clear_world();Require(compositor.Draw(device.Get(),context.Get(),fresh,7,replaced),"fresh HUD failed");
    prompts.binding_generation=102;PublishActionPrompts(prompts);
    compositor.RetireStaleText(device.Get(),context.Get(),replaced,true,42);
    expect_pixels(background,"fresh Draw retained the preceding frame's preimage");
    // Rolled eyes make the two text bounding rectangles overlap. Restoring
    // hint before interaction would put the saved hint pixels back on screen.
    auto rolled=overlay;rolled.text.hint={};rolled.text.interaction={};
    rolled.text.controller_prompt_generation=0;
    for(auto& eye:rolled.eyes)eye.eye_to_head.orientation={0,0,.38268343F,.92387953F};
    clear_world();Require(compositor.Draw(device.Get(),context.Get(),rolled,7,replaced),"rolled HUD failed");
    const std::array<std::vector<std::uint32_t>,2> rolled_background{
        ReadEye(device.Get(),context.Get(),targets[0].Get()),
        ReadEye(device.Get(),context.Get(),targets[1].Get())};
    std::u16string tall_label;
    for(unsigned line=0;line<30;++line)tall_label+=u"Hold left grip\n";
    rolled.text.hint=Text(tall_label);rolled.text.interaction=Text(tall_label);
    rolled.text.controller_prompt_generation=101;
    prompts.binding_generation=101;PublishActionPrompts(prompts);
    clear_world();Require(compositor.Draw(device.Get(),context.Get(),rolled,7,replaced),"rolled controls failed");
    Require(ReadEye(device.Get(),context.Get(),targets[0].Get())!=rolled_background[0],"rolled controls hidden");
    prompts.binding_generation=102;PublishActionPrompts(prompts);
    compositor.RetireStaleText(device.Get(),context.Get(),replaced,true,42);
    expect_pixels(rolled_background,"overlapping patches were not restored in reverse order");
    prompts.binding_generation=101;PublishActionPrompts(prompts);draw_controlled();
    compositor.Reset();prompts.binding_generation=102;PublishActionPrompts(prompts);
    compositor.RetireStaleText(device.Get(),context.Get(),replaced,true,42);
    expect_pixels(controlled,"Reset replayed a retired frame's preimage");
    // A valid render target whose format the bounded backup does not support
    // must suppress BOTH controlled eyes while still drawing essential HUD.
    std::array<ComPtr<ID3D11Texture2D>,2> unfenced;
    for(unsigned eye=0;eye<2;++eye){
        auto fallback_desc=desc;
        if(eye==1)fallback_desc.Format=DXGI_FORMAT_R10G10B10A2_UNORM;
        Require(SUCCEEDED(device->CreateTexture2D(&fallback_desc,nullptr,&unfenced[eye])),
            "unfenced target allocation failed");
    }
    const std::array<ID3D11Texture2D*,2> unfenced_raw{unfenced[0].Get(),unfenced[1].Get()};
    const auto clear_unfenced=[&](){for(const auto& target:unfenced){
        ComPtr<ID3D11RenderTargetView> rtv;
        Require(SUCCEEDED(device->CreateRenderTargetView(target.Get(),nullptr,&rtv)),"unfenced RTV failed");
        context->ClearRenderTargetView(rtv.Get(),world_color);
    }};
    clear_unfenced();Require(compositor.Draw(device.Get(),context.Get(),fresh,7,unfenced_raw),
        "essential HUD failed on unfenced targets");
    const std::array<std::vector<std::uint32_t>,2> unfenced_background{
        ReadEye(device.Get(),context.Get(),unfenced[0].Get()),
        ReadEye(device.Get(),context.Get(),unfenced[1].Get())};
    prompts.binding_generation=101;PublishActionPrompts(prompts);
    clear_unfenced();Require(compositor.Draw(device.Get(),context.Get(),overlay,7,unfenced_raw),
        "optional backup failure rejected the essential HUD");
    for(unsigned eye=0;eye<2;++eye)Require(
        ReadEye(device.Get(),context.Get(),unfenced[eye].Get())==unfenced_background[eye],
        "failed second-eye backup drew unfenced control text");
    auto native=overlay;native.text.controller_prompt_generation=0;
    // Native text retains its original draw order, so the intentionally large
    // overlapping status card would obscure it. Remove that card in BOTH the
    // native baseline and assertion instead of mistaking occlusion for a veto.
    native.status_surface_valid=false;
    auto native_background=native;native_background.text.hint={};native_background.text.interaction={};
    clear_unfenced();Require(compositor.Draw(device.Get(),context.Get(),native_background,7,unfenced_raw),
        "native text baseline failed");
    const std::array<std::vector<std::uint32_t>,2> native_background_pixels{
        ReadEye(device.Get(),context.Get(),unfenced[0].Get()),
        ReadEye(device.Get(),context.Get(),unfenced[1].Get())};
    clear_unfenced();Require(compositor.Draw(device.Get(),context.Get(),native,7,unfenced_raw),
        "backup restrictions affected native text");
    for(unsigned eye=0;eye<2;++eye)Require(
        ReadEye(device.Get(),context.Get(),unfenced[eye].Get())!=native_background_pixels[eye],
        "ordinary native text incorrectly required backup resources");
    // Production X8 targets and the other supported four-byte formats all
    // need exact-format patch copies, including their SRGB variants.
    for(const auto format:{DXGI_FORMAT_B8G8R8X8_UNORM,DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
        DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB}){
        compositor.DiscardRepeatedText();unfenced={};
        auto supported_desc=desc;supported_desc.Format=format;
        for(auto& target:unfenced)Require(SUCCEEDED(device->CreateTexture2D(&supported_desc,nullptr,&target)),
            "supported target format allocation failed");
        const std::array<ID3D11Texture2D*,2> supported{unfenced[0].Get(),unfenced[1].Get()};
        clear_unfenced();Require(compositor.Draw(device.Get(),context.Get(),fresh,7,supported),
            "supported format background failed");
        const std::array<std::vector<std::uint32_t>,2> supported_background{
            ReadEye(device.Get(),context.Get(),unfenced[0].Get()),
            ReadEye(device.Get(),context.Get(),unfenced[1].Get())};
        prompts.binding_generation=101;PublishActionPrompts(prompts);
        clear_unfenced();Require(compositor.Draw(device.Get(),context.Get(),overlay,7,supported),
            "supported format controls failed");
        for(unsigned eye=0;eye<2;++eye)Require(
            ReadEye(device.Get(),context.Get(),unfenced[eye].Get())!=supported_background[eye],
            "supported format suppressed controlled text");
        prompts.binding_generation=102;PublishActionPrompts(prompts);
        compositor.RetireStaleText(device.Get(),context.Get(),supported,true,42);
        for(unsigned eye=0;eye<2;++eye)Require(
            ReadEye(device.Get(),context.Get(),unfenced[eye].Get())==supported_background[eye],
            "supported format patch restoration changed background bytes");
    }
    // Coaching has an independent owner/preference/deadline fence, even with
    // generation zero (ordinary native text otherwise remains unchanged).
    overlay.text.hint={};overlay.text.controller_prompt_generation=0;
    overlay.text.interaction=Text(u"Bring the round to the gun");
    overlay.reload_instruction=true;overlay.reload_feedback.owner_token=42;
    for(unsigned cause=0;cause<4;++cause){
        overlay.reload_instruction_until_ms=GetTickCount64()+60000;
        draw_controlled();
        compositor.RetireStaleText(device.Get(),context.Get(),replaced,cause!=2,
            cause==0?0:cause==1?43:42);
        if(cause<3)expect_pixels(background,"repeat coaching survived owner loss/replacement/guide disable");
        else {
            const auto now=GetTickCount64();
            // Redraw with a short but initially valid deadline; no fresh draw
            // or clear is allowed between the expiry and retirement.
            overlay.reload_instruction_until_ms=now+2000;draw_controlled();
            const auto after_draw=GetTickCount64();
            if(after_draw<overlay.reload_instruction_until_ms)
                Sleep(static_cast<DWORD>(overlay.reload_instruction_until_ms-after_draw+1));
            compositor.RetireStaleText(device.Get(),context.Get(),replaced,true,42);
            expect_pixels(background,"repeat coaching survived its deadline");
        }
        compositor.RetireStaleText(device.Get(),context.Get(),replaced,false,0);
        expect_pixels(background,"coaching retirement was not idempotent");
    }
    overlay.text.hint=Text(u"Press left menu");overlay.text.controller_prompt_generation=101;
    overlay.reload_instruction_until_ms=GetTickCount64()+60000;
    prompts.binding_generation=101;PublishActionPrompts(prompts);draw_controlled();
    compositor.RetireStaleText(device.Get(),context.Get(),replaced,true,0);
    expect_pixels(background,"reload invalidation left a partially restored controlled stack");
    ClearActionPrompts();
    std::cout << "HUD Unicode raster, cache and stereo projection passed\n";
}
