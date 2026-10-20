#include "runtime/vr_settings.hpp"
#include "backends/openvr/hud_text_overlay.hpp"
#include <wrl/client.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
using namespace cojvr::runtime;
namespace {
void Require(bool value, const char* why) {
    if (!value) { std::cerr << why << '\n'; std::exit(1); }
}
SettingsMenuInput Input(std::uint64_t time) {
    SettingsMenuInput input{};
    input.available = true;input.global_available=0xFF;
    input.context_token = 1; input.monotonic_ms = time;
    input.gameplay.active = true;
    return input;
}
void Open(VrSettingsMenu& menu, std::uint64_t time) {
    (void)menu.Update(Input(time));
    auto input = Input(time+1); input.options = true;
    Require(!menu.Update(input).native_pause, "Options press dispatched native pause");
    input.monotonic_ms = time+1001;
    const auto result = menu.Update(input);
    Require(result.active && result.input_captured && !result.native_pause,
        "deliberate Options hold did not exclusively open settings");
}
}
int main(int argc,char** argv) {
    Require(CurrentCinematicMode()==CinematicsMode::theater,"startup must default to theater");
    VrSettingsMenu menu;
    auto input = Input(0); input.options=true;
    Require(!menu.Update(input).active,"held startup Options opened settings");
    input.monotonic_ms=2000;
    Require(!menu.Update(input).native_pause,"unarmed held Options dispatched pause");
    (void)menu.Update(Input(2001));
    input=Input(2010);input.options=true;
    Require(!menu.Update(input).native_pause,"short pause fired on press");
    auto result=menu.Update(Input(2100));
    Require(result.native_pause&&!result.active,"short released Options must request native pause once");
    Require(!menu.Update(Input(2101)).native_pause,"pause replayed");
    Open(menu,3000);
    input=Input(4010);input.accept=true;input.gameplay.turn.x=1;
    Require(!menu.Update(input).mode_changed,"entry carried held accept/stick into setting");
    (void)menu.Update(Input(4020));
    input=Input(4030);input.accept=true;
    result=menu.Update(input);
    Require(result.mode_changed&&result.requested_mode==CinematicsMode::immersive,"fresh Cross did not select immersive");
    Require(!menu.Update(input).mode_changed,"held Cross changed setting twice");
    (void)menu.Update(Input(4040));
    input=Input(4050);input.gameplay.turn.x=-1;
    result=menu.Update(input);
    Require(result.mode_changed&&result.requested_mode==CinematicsMode::theater,"left stick direction did not select theater");
    input=Input(4060);input.back=true;input.gameplay.fire_right=true;
    result=menu.Update(input);
    Require(!result.active&&result.input_captured&&!result.native_pause,"Circle close leaked input");
    input=Input(4070);input.gameplay.weapon_radial=true;
    Require(menu.Update(input).input_captured,"held wheel escaped exit barrier");
    input=Input(4080);input.global_available=0;input.available=false;
    Require(menu.Update(input).input_captured,"unavailable controls fabricated release");
    Require(menu.Update(Input(4090)).input_captured,"release baseline was forwarded");
    Require(!menu.Update(Input(4091)).input_captured,"fresh neutral controls did not recover");

    Open(menu,5000);
    (void)menu.Update(Input(6010));
    input=Input(6020);input.available=false;input.back=true;
    Require(!menu.Update(input).active,"focus loss kept settings active");
    input=Input(6030);input.options=true;
    Require(menu.Update(input).input_captured,"held Options escaped focus-loss barrier");
    input.monotonic_ms=8000;
    Require(!menu.Update(input).active,"interrupted Options hold resumed");
    (void)menu.Update(Input(8010));
    Open(menu,9000);
    input=Input(10010);input.context_token=2;
    result=menu.Update(input);
    Require(!result.active&&result.input_captured,"changed game owner kept menu or leaked input");
    Open(menu,11000);
    input=Input(12001);input.monotonic_ms=11000;
    Require(!menu.Update(input).active,"regressing monotonic time retained menu");
    (void)menu.Update(Input(13000));
    input=Input(13001);input.options=true;(void)menu.Update(input);
    result=menu.Update(Input(14001));
    Require(result.active&&!result.native_pause,"unsampled hold threshold incorrectly dispatched short pause");
    menu.Invalidate();
    Require(menu.input_captured()&&!menu.active(),"invalidate did not fence input");
    input=Input(14010);input.gameplay.turn.x=std::numeric_limits<float>::quiet_NaN();
    Require(menu.Update(input).input_captured,"NaN stick counted as centered release");
    input=Input(14011);input.gameplay.move.y=.9F;
    Require(menu.Update(input).input_captured,"held locomotion escaped capture barrier");
    menu.Invalidate();
    input=Input(15000);input.gameplay.fire_right=true;(void)menu.Update(input);
    input=Input(15001);input.gameplay.digital_available&=~2U;
    Require(menu.Update(input).input_captured,"unavailable gameplay action fabricated held-trigger release");
    input.monotonic_ms=15002;
    Require(menu.Update(input).input_captured,"missing trigger source cleared release history");
    (void)menu.Update(Input(15003));
    Require(!menu.Update(Input(15004)).input_captured,"available trigger release did not clear barrier");
    VrSettingsMenu unbound;
    input=Input(15010);input.gameplay.radial_available=false;
    (void)unbound.Update(input);input.monotonic_ms=15011;
    Require(!unbound.Update(input).input_captured,"never-bound optional radial froze native startup");
    input.options=true;input.monotonic_ms=15012;(void)unbound.Update(input);
    input.options=false;input.monotonic_ms=15013;
    Require(unbound.Update(input).native_pause,"unbound radial disabled native short pause");
    unbound.Invalidate();
    input=Input(15020);input.gameplay.weapon_radial=true;(void)unbound.Update(input);
    input=Input(15021);input.gameplay.radial_available=false;
    Require(unbound.Update(input).input_captured,"previously held unavailable radial fabricated release");
    input.monotonic_ms=15022;
    Require(unbound.Update(input).input_captured,"unavailable radial discarded release history");
    (void)unbound.Update(Input(15023));
    Require(!unbound.Update(Input(15024)).input_captured,"available radial release did not recover");
    unbound.Invalidate();
    input=Input(15030);input.gameplay.weapon_radial=true;(void)unbound.Update(input);
    input=Input(15031);input.available=false;(void)unbound.Update(input);
    input=Input(15032);input.gameplay.radial_available=false;
    Require(unbound.Update(input).input_captured,"failed/focus-lost sample cleared held-radial history");
    VrSettingsMenu no_axes;
    input=Input(15040);input.gameplay.move_available=input.gameplay.turn_available=false;
    (void)no_axes.Update(input);input.monotonic_ms=15041;
    Require(!no_axes.Update(input).input_captured,"never-bound axes froze native baseline");
    no_axes.Invalidate();
    input=Input(15042);input.gameplay.move.y=.9F;input.gameplay.turn.x=1;(void)no_axes.Update(input);
    input=Input(15043);input.gameplay.move_available=input.gameplay.turn_available=false;
    Require(no_axes.Update(input).input_captured,"unavailable axes fabricated previously-held center");
    input=Input(15044);input.available=false;(void)no_axes.Update(input);
    input=Input(15045);input.gameplay.move_available=input.gameplay.turn_available=false;
    Require(no_axes.Update(input).input_captured,"failed sample cleared prior held-axis history");
    (void)no_axes.Update(Input(15046));
    Require(!no_axes.Update(Input(15047)).input_captured,"fresh centered axes did not recover baseline");
    // Missing left-hand bindings must preserve one-hand native UI and pause.
    VrSettingsMenu one_hand;
    input=Input(16000);input.global_available=0x97; // Options/Cross/Circle/right select/right shoulder
    (void)one_hand.Update(input);input.monotonic_ms=16001;
    Require(!one_hand.Update(input).input_captured,"missing left-hand bindings froze native baseline");
    input.options=true;input.monotonic_ms=16002;(void)one_hand.Update(input);
    input.options=false;input.monotonic_ms=16003;
    Require(one_hand.Update(input).native_pause,"one-hand availability disabled native pause");
    VrSettingsMenu no_bindings;
    input=Input(16010);input.global_available=0;input.gameplay.digital_available=0;
    input.gameplay.radial_available=input.gameplay.move_available=input.gameplay.turn_available=false;
    (void)no_bindings.Update(input);input.monotonic_ms=16011;
    Require(!no_bindings.Update(input).input_captured,"no VR bindings froze desktop native baseline");
    VrSettingsMenu newly_bound;
    input=Input(16020);input.global_available&=~SettingsMenuInput::options_bit;
    (void)newly_bound.Update(input); // Only one unbound neutral sample.
    input=Input(16021);input.options=true;
    Require(!newly_bound.Update(input).native_pause,"new Options source dispatched pause on held arrival");
    input.monotonic_ms=18000;
    Require(!newly_bound.Update(input).active,"newly bound held Options opened settings without released baseline");
    result=newly_bound.Update(Input(18001));
    Require(!result.native_pause&&!result.active,"newly bound hold dispatched pause on first release");
    input=Input(18002);input.options=true;(void)newly_bound.Update(input);
    Require(newly_bound.Update(Input(18003)).native_pause,"fresh Options tap did not recover after binding release");
    newly_bound.Invalidate();(void)newly_bound.Update(Input(18004));
    input=Input(18005);input.global_available&=~SettingsMenuInput::options_bit;(void)newly_bound.Update(input);
    input=Input(18006);input.options=true;(void)newly_bound.Update(input);input.monotonic_ms=20006;
    Require(!newly_bound.Update(input).active,"idle lost/reacquired held Options resumed arming");
    Require(!newly_bound.Update(Input(20007)).native_pause,"idle Options reacquisition replayed pause");
    VrSettingsMenu no_accept;
    input=Input(16100);input.global_available&=~SettingsMenuInput::accept_bit;
    (void)no_accept.Update(input);input.monotonic_ms=16101;
    Require(!no_accept.Update(input).input_captured,"missing settings accept froze native UI");
    input.options=true;input.monotonic_ms=16102;(void)no_accept.Update(input);input.monotonic_ms=17200;
    Require(!no_accept.Update(input).active,"missing accept admitted settings opening");
    input.options=false;input.monotonic_ms=17201;
    Require(!no_accept.Update(input).native_pause,"unavailable-menu hold dispatched native pause");
    VrSettingsMenu lost_global;
    Open(lost_global,18000);(void)lost_global.Update(Input(19010));
    input=Input(19011);input.select_left=true;(void)lost_global.Update(input);
    input=Input(19012);input.global_available&=~8U;
    result=lost_global.Update(input);
    Require(!result.active&&result.input_captured,"active menu did not close on hand/source loss");
    input.monotonic_ms=19013;
    Require(lost_global.Update(input).input_captured,"lost held global source fabricated release");
    (void)lost_global.Update(Input(19014));
    Require(!lost_global.Update(Input(19015)).input_captured,"fresh global release did not recover native UI");
    input=Input(19016);input.options=true;(void)lost_global.Update(input);
    input=Input(19017);input.global_available&=~SettingsMenuInput::options_bit;(void)lost_global.Update(input);
    input.monotonic_ms=19018;
    Require(lost_global.Update(input).input_captured,"unavailable held Options fabricated release");
    result=lost_global.Update(Input(19019));
    Require(!result.native_pause,"lost Options gesture replayed pause on release");
    // Production poll omits unbound optional digital channels. They must not
    // prevent a neutral baseline unless that channel was actually held.
    input=Input(15012);input.gameplay.digital_available=0xF;
    (void)menu.Update(input);input.monotonic_ms=15013;
    Require(!menu.Update(input).input_captured,"unbound optional actions locked out Sense settings");

    const auto root=std::filesystem::temp_directory_path()/
        ("cojvr-settings-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto file=root/"settings.ini";
    auto loaded=LoadVrSettings(file);
    Require(loaded.mode==CinematicsMode::theater&&loaded.status==SettingsLoadStatus::missing,
        "missing configuration did not default safely");
    Require(SaveVrSettings(file,CinematicsMode::immersive).ok,"valid settings failed to save");
    loaded=LoadVrSettings(file);
    Require(loaded.mode==CinematicsMode::immersive&&loaded.status==SettingsLoadStatus::loaded,"persisted immersive did not survive reload");
    {std::ofstream out(file);out<<"version=1\ncinematics=immersive\ncinematics=theater\n";}
    loaded=LoadVrSettings(file);
    Require(loaded.status==SettingsLoadStatus::invalid&&loaded.mode==CinematicsMode::theater,"duplicate setting admitted ambiguous immersive");
    {std::ofstream out(file);out<<"version=2\ncinematics=immersive\n";}
    Require(LoadVrSettings(file).status==SettingsLoadStatus::invalid,"unknown config version admitted");
    {std::ofstream out(file);out<<std::string(5000,'x');}
    Require(LoadVrSettings(file).status==SettingsLoadStatus::invalid,"oversized config admitted");
    {std::ofstream out(file);out<<"version=1\ncinematics=unknown\n";}
    Require(LoadVrSettings(file).mode==CinematicsMode::theater,"unknown mode enabled immersive");
    auto saved=SaveVrSettings(root,CinematicsMode::immersive);
    Require(!saved.ok&&saved.error_code!=0,"save to directory falsely reported success");
    Require(!SaveVrSettings({},CinematicsMode::immersive).ok,"empty persistence path accepted");
    Require(!SaveVrSettings(file,static_cast<CinematicsMode>(9)).ok,"invalid enum serialized");
    Require(SaveVrSettings(file,CinematicsMode::theater).ok,"replacement save failed");
    Require(LoadVrSettings(file).mode==CinematicsMode::theater,"replacement failed to persist theater");
    HANDLE locked=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    Require(locked!=INVALID_HANDLE_VALUE,"settings write-denial fixture failed to lock destination");
    saved=SaveVrSettings(file,CinematicsMode::immersive);
    Require(!saved.ok&&saved.error_code!=0,"locked destination save falsely reported persisted selection");
    CloseHandle(locked);
    Require(LoadVrSettings(file).mode==CinematicsMode::theater,"failed replacement destroyed prior persisted setting");
    for(const auto& entry:std::filesystem::directory_iterator(root))
        Require(entry.path().filename()=="settings.ini","failed save left an owned temporary file behind");
    {std::ofstream out(file);out<<"  version = 1\r\n# local preference\r\ncinematics = immersive\r\n";}
    loaded=LoadVrSettings(file);
    Require(loaded.status==SettingsLoadStatus::loaded&&loaded.mode==CinematicsMode::immersive,"ordinary CRLF config failed to parse");
    Require(DefaultVrSettingsPath().filename()=="settings.ini","local settings path missing stable config filename");
    PublishCinematicMode(CinematicsMode::immersive);
    std::thread reader([]{Require(CurrentCinematicMode()==CinematicsMode::immersive,"game thread cannot read published mode");});reader.join();
    PublishCinematicMode(static_cast<CinematicsMode>(99));
    Require(CurrentCinematicMode()==CinematicsMode::theater,"invalid published mode did not fail safe");
    PublishSettingsInputCapture(true);
    Require(SettingsInputCaptured(),"published capture unavailable to consumer");
    PublishSettingsInputCapture(false);
    Require(!SettingsInputCaptured(),"capture publication failed to clear");
    auto text=BuildVrSettingsText(CinematicsMode::immersive,SettingsLanguage::spanish,true,false,5);
    Require(text.view().find(u"Inmersivo")!=std::u16string_view::npos&&text.view().find(u"guard")!=std::u16string_view::npos,
        "Spanish setting/save-failure feedback absent");
    Require(text.view().find(u"Windows 5")!=std::u16string_view::npos,"menu omitted actual save error");
    Require(text.view().find(u"experimental")!=std::u16string_view::npos,
        "Spanish immersive choice omitted experimental status");
    const auto immersive_english=BuildVrSettingsText(CinematicsMode::immersive,SettingsLanguage::english,false,false);
    Require(immersive_english.view().find(u"experimental")!=std::u16string_view::npos,
        "English immersive choice omitted experimental status");
    auto english=BuildVrSettingsText(CinematicsMode::theater,SettingsLanguage::english,false,true);
    Require(english.view().find(u"Theater")!=std::u16string_view::npos&&english.view().find(u"release")!=std::u16string_view::npos,
        "entry release prompt absent");
    using namespace cojvr::backends::openvr;
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,nullptr,&context)),"settings GPU host device unavailable");
    SettingsTextTexture texture;
    Require(texture.Update(device.Get(),english)&&texture.texture(),"settings text GPU upload failed");
    // The overlay is consumed by another D3D11 device/process. A local SRV
    // and CPU readback alone do not establish that transport contract.
    ComPtr<IDXGIResource> shared_resource;
    Require(SUCCEEDED(texture.texture()->QueryInterface(IID_PPV_ARGS(&shared_resource))),
        "settings texture has no DXGI resource");
    HANDLE shared_handle=nullptr;
    const HRESULT shared_hr=shared_resource->GetSharedHandle(&shared_handle);
    std::cout<<"settings GetSharedHandle HRESULT=0x"<<std::hex<<shared_hr<<std::dec
        <<";handle_valid="<<(shared_handle?"true":"false")<<'\n';
    Require(SUCCEEDED(shared_hr)&&shared_handle,"settings texture cannot cross the compositor device boundary");
    ComPtr<ID3D11Device> consumer;ComPtr<ID3D11DeviceContext> consumer_context;
    Require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&consumer,nullptr,&consumer_context)),"settings consumer device unavailable");
    ComPtr<ID3D11Texture2D> consumed;
    Require(SUCCEEDED(consumer->OpenSharedResource(shared_handle,IID_PPV_ARGS(&consumed))),
        "compositor device cannot open settings texture");
    auto* first=texture.texture();
    Require(texture.Update(device.Get(),english)&&texture.texture()==first,"unchanged settings rebuilt GPU texture");
    HudTextRaster raster;
    const auto& panel=raster.Render(english);
    D3D11_TEXTURE2D_DESC desc{};texture.texture()->GetDesc(&desc);
    Require(desc.Width==panel.width&&desc.Height==panel.height&&desc.Height<=350,
        "settings upload changed measured panel bounds");
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> readback;
    Require(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&readback)),"settings readback texture unavailable");
    context->CopyResource(readback.Get(),texture.texture());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Require(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)),"settings GPU readback unavailable");
    for(unsigned row=0;row<desc.Height;++row)
        Require(std::equal(panel.pixels.begin()+row*desc.Width,panel.pixels.begin()+(row+1)*desc.Width,
            reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(mapped.pData)+row*mapped.RowPitch)),
            "settings GPU pixels differ from measured CPU text");
    context->Unmap(readback.Get(),0);
    // Read through the independently opened resource, rather than merely
    // testing flags or that GetSharedHandle happened to succeed.
    context->Flush();
    readback.Reset();
    Require(SUCCEEDED(consumer->CreateTexture2D(&desc,nullptr,&readback)),"consumer readback unavailable");
    consumer_context->CopyResource(readback.Get(),consumed.Get());
    Require(SUCCEEDED(consumer_context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)),"consumer map failed");
    for(unsigned row=0;row<desc.Height;++row)
        Require(std::equal(panel.pixels.begin()+row*desc.Width,panel.pixels.begin()+(row+1)*desc.Width,
            reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(mapped.pData)+row*mapped.RowPitch)),
            "compositor device received different settings pixels");
    consumer_context->Unmap(readback.Get(),0);
    std::cout<<"settings independent-device shared open and pixel readback passed\n";
    Require(texture.Update(device.Get(),text),"Spanish save-error texture upload failed");
    Require(!texture.Update(nullptr,text)&&!texture.texture(),"lost device exposed stale settings texture");
    Require(texture.status().hr==E_POINTER&&std::string_view(texture.status().stage)=="device",
        "lost device discarded actual settings upload failure stage/HRESULT");
    Require(texture.Update(device.Get(),text),"settings upload failed to recover after device loss");
    ComPtr<ID3D11Device> replacement;ComPtr<ID3D11DeviceContext> replacement_context;
    Require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&replacement,nullptr,&replacement_context)),"replacement settings device unavailable");
    Require(texture.Update(replacement.Get(),text),"settings texture failed device replacement");
    ComPtr<ID3D11Device> owner;texture.texture()->GetDevice(&owner);
    Require(owner.Get()==replacement.Get(),"settings cache retained old-device texture");
    Require(texture.Update(device.Get(),{})&&!texture.texture(),"hidden settings retained visible GPU image");
    const auto& spanish=raster.Render(text);
    Require(!spanish.pixels.empty(),"Spanish Unicode settings raster empty");
    EyeView eye{};eye.eye_to_head.position_valid=eye.eye_to_head.orientation_valid=true;
    eye.fov={-.8F,.8F,.8F,-.8F};
    HudPanelPlacement placement{1.25F,0.F,1.5F};ProjectedHudPanel quad{};
    for(float ipd:{-.032F,.032F}){
        eye.eye_to_head.position.x=ipd;
        Require(ProjectHudPanel(spanish,eye,1200,1200,placement,quad),"settings text failed binocular projection");
        for(const auto& p:quad.clip_positions)
            Require(std::abs(p[0])<p[3]&&std::abs(p[1])<p[3],"settings controls clipped in one eye");
    }
    if(argc>1){
        BITMAPFILEHEADER header{};header.bfType=0x4D42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
        header.bfSize=header.bfOffBits+static_cast<DWORD>(spanish.pixels.size()*4);
        BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=spanish.width;
        info.biHeight=-static_cast<LONG>(spanish.height);info.biPlanes=1;info.biBitCount=32;
        std::ofstream out(argv[1],std::ios::binary);out.write(reinterpret_cast<const char*>(&header),sizeof(header));
        out.write(reinterpret_cast<const char*>(&info),sizeof(info));
        out.write(reinterpret_cast<const char*>(spanish.pixels.data()),spanish.pixels.size()*4);
    }
    std::filesystem::remove_all(root);
    std::cout<<"VR settings menu, capture, release barriers and persistence passed\n";
}
