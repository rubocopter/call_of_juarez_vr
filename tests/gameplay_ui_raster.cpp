#include "backends/openvr/gameplay_ui_raster.hpp"
#include "backends/openvr/reticle_pixels.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <fstream>
#include <string>
using namespace cojvr::runtime;
using namespace cojvr::backends::openvr;
void Require(bool ok,const char* why) { if(!ok){std::cerr<<why<<'\n';std::exit(1);} }
void Save(const HudTextPanel& panel,const char* path){
    std::ofstream out(path,std::ios::binary);
    out<<"P6\n"<<panel.width<<' '<<panel.height<<"\n255\n";
    for(const auto pixel:panel.pixels){const char rgb[]{static_cast<char>(pixel>>16),
        static_cast<char>(pixel>>8),static_cast<char>(pixel)};out.write(rgb,3);}
}
int main(int argc,char** argv) {
    Require(NoShootReticlePixel(0,0)==0xFFFF4038U&&NoShootReticlePixel(6,-6)==0xFFFF4038U,"warning must be a red X at aim point");
    Require(NoShootReticlePixel(1,0)==0xFF000000U,"warning must retain contrasting outline");
    Require(NoShootReticlePixel(0,6)==0&&NoShootReticlePixel(9,9)==0,"warning must preserve world outside diagonals/footprint");
    GameplayUiRaster raster;
    const auto& gaze=raster.InteractionGaze();
    Require(gaze.width==32&&gaze.height==32&&gaze.pixels.size()==32*32,
        "gaze marker must be a bounded immutable raster");
    Require(gaze.pixels.front()==0&&gaze.pixels[16*32+16]==0,
        "gaze ring must preserve scene pixels at center and outside footprint");
    std::size_t cyan=0;
    for(auto pixel:gaze.pixels)cyan+=(pixel>>24)==255&&((pixel>>8)&255)>200&&
        (pixel&255)>200&&((pixel>>16)&255)<80;
    Require(cyan>0&&cyan<gaze.pixels.size()/2,"gaze ring must visibly distinguish cyan from gun/warning cross");
    const auto* gaze_pixels=gaze.pixels.data();
    Require(raster.InteractionGaze().pixels.data()==gaze_pixels,"unchanged gaze marker rebuilt immutable pixels");
    EquipmentWheelSnapshot wheel{};
    wheel.valid=wheel.active=true; wheel.available_mask=3; wheel.selected=0;
    const std::u16string_view names[]{u"Arma larga",u"Arco",u"Pistola dcha",u"Dinamita",u"Manos",u"Throw weapon",u"Pistola izq",u"Biblia / látigo"};
    for(std::size_t i=0;i<8;++i){wheel.labels[i].length=static_cast<std::uint32_t>(names[i].size());
        std::copy(names[i].begin(),names[i].end(),wheel.labels[i].characters.begin());}
    const std::u16string_view name=u"Arma larga";
    wheel.labels[0].length=static_cast<std::uint32_t>(name.size());
    std::copy(name.begin(),name.end(),wheel.labels[0].characters.begin());
    const auto first=raster.Wheel(wheel);
    if(argc>1)Save(first,argv[1]);
    Require(first.width==512 && first.height==512 && first.pixels.size()==512*512,
        "eligible wheel must rasterize a bounded circular surface");
    Require(first.pixels[0]==0 && (first.pixels[256*512+256]>>24)==255,
        "wheel corners must remain transparent with defined dial coverage");
    const auto rev=raster.wheel_revision();
    (void)raster.Wheel(wheel);
    Require(rev==raster.wheel_revision(),"unchanged wheel must reuse cached pixels");
    wheel.selected=2; const auto unavailable=raster.Wheel(wheel);
    Require(unavailable.pixels!=first.pixels,"native eligibility must affect selected sector pixels");
    wheel.active=false;
    Require(raster.Wheel(wheel).pixels.empty(),"context loss must remove wheel");
    WristCompassSnapshot compass{}; compass.active=true; compass.north_direction={0,1};
    compass.marker_count=1; compass.markers[0].direction={1,0};
    const auto compass_pixels=raster.Compass(compass);
    if(argc>2)Save(compass_pixels,argv[2]);
    Require(compass_pixels.width==512 && compass_pixels.pixels[0]==0,
        "native compass must render a transparent bounded dial");
    const auto compass_rev=raster.compass_revision();
    (void)raster.Compass(compass);
    Require(raster.compass_revision()==compass_rev,"unchanged compass must reuse pixels");
    compass.markers[0].direction={0,-1};
    Require(raster.Compass(compass).pixels!=compass_pixels.pixels,
        "objective marker direction must follow native value");
    compass.markers[0].direction={1,0};
    const auto bounded=raster.Compass(compass,1000);
    const auto bounded_revision=raster.compass_revision();
    compass.markers[0].direction={0,-1};
    Require(raster.Compass(compass,1050).pixels==bounded.pixels&&
        raster.compass_revision()==bounded_revision,
        "wrist motion must not rasterize/upload compass at frame cadence");
    Require(raster.Compass(compass,1100).pixels!=bounded.pixels,
        "bounded compass update must still display fresh objective direction");
    compass.north_direction.x=std::numeric_limits<float>::quiet_NaN();
    Require(raster.Compass(compass).pixels.empty(),"invalid compass bearing must hide surface");
    compass.north_direction={0,1}; compass.marker_count=17;
    Require(raster.Compass(compass).pixels.empty(),"unbounded waypoint metadata must be rejected");
    WristStatusSnapshot status{};status.active=true;status.line_count=2;
    const std::u16string_view lines[]{u"Salud  087",u"Derecha  3"};
    for(int i=0;i<2;++i){status.lines[i].length=static_cast<std::uint32_t>(lines[i].size());
        std::copy(lines[i].begin(),lines[i].end(),status.lines[i].characters.begin());}
    const auto status_pixels=raster.Status(status);
    if(argc>3)Save(status_pixels,argv[3]);
    Require(status_pixels.width==512&&status_pixels.height==128&&status_pixels.pixels.front()==0&&
        (status_pixels.pixels[64*512+256]>>24)==255,"status card must have defined interior and transparent outside");
    const auto status_rev=raster.status_revision();(void)raster.Status(status);
    Require(status_rev==raster.status_revision(),"unchanged health/ammo must reuse pixels");
    // Literal native percentages must produce a bounded visible meter while
    // retaining other rows, dimensions and hidden/invalid optional fallbacks.
    status.health_row=0;status.health_percent=50;
    const auto half_health=raster.Status(status);
    Require(half_health.pixels!=status_pixels.pixels&&half_health.height==status_pixels.height,
        "health meter must be visible without increasing wrist card extent");
    status.health_percent=100;const auto full_health=raster.Status(status);
    status.health_percent=0;const auto empty_health=raster.Status(status);
    Require(full_health.pixels!=half_health.pixels&&empty_health.pixels!=half_health.pixels,
        "full, half and empty health must have distinct meter coverage");
    const auto coverage=[](const HudTextPanel& panel){
        std::size_t lit=0;
        for(std::size_t x=24;x<488;++x){const auto pixel=panel.pixels[58*512+x];
            lit+=std::max({(pixel>>16)&255,(pixel>>8)&255,pixel&255})>100;}
        return lit;
    };
    Require(coverage(full_health)==464&&coverage(half_health)==232&&coverage(empty_health)==0,
        "meter fill must represent the native percentage, including zero");
    status.health_percent=25;const auto low_health=raster.Status(status);
    Require(coverage(low_health)==116,"quarter health meter must retain proportional coverage");
    const auto low_pixel=low_health.pixels[58*512+24];
    const auto full_pixel=full_health.pixels[58*512+24];
    Require(((low_pixel>>16)&255)>((low_pixel>>8)&255)&&
        ((full_pixel>>8)&255)>((full_pixel>>16)&255),"low health warning must differ from the normal green meter");
    for(std::size_t y=64;y<128;++y)for(std::size_t x=0;x<512;++x)
        Require(full_health.pixels[y*512+x]==status_pixels.pixels[y*512+x]&&
            empty_health.pixels[y*512+x]==status_pixels.pixels[y*512+x],"health meter changed the ammo row");
    if(argc>5){
        for(const int percent:{0,25,50,87,100}){
            status.health_percent=percent;
            const std::u16string value=std::u16string(u"Salud  ")+
                std::u16string(1,static_cast<char16_t>(u'0'+percent/100))+
                std::u16string(1,static_cast<char16_t>(u'0'+percent/10%10))+
                std::u16string(1,static_cast<char16_t>(u'0'+percent%10));
            status.lines[0]={};status.lines[0].length=static_cast<std::uint32_t>(value.size());
            std::copy(value.begin(),value.end(),status.lines[0].characters.begin());
            Save(raster.Status(status),(std::string(argv[5])+std::to_string(percent)+".ppm").c_str());
        }
        status.lines[0]={};status.lines[0].length=static_cast<std::uint32_t>(lines[0].size());
        std::copy(lines[0].begin(),lines[0].end(),status.lines[0].characters.begin());
    }
    (void)raster.Status(status);
    const auto meter_revision=raster.status_revision();(void)raster.Status(status);
    Require(meter_revision==raster.status_revision(),"unchanged gauge rebuilt pixels");
    for(int invalid:{-1,101,std::numeric_limits<int>::max()}){
        status.health_percent=invalid;
        Require(raster.Status(status).pixels==status_pixels.pixels,"invalid optional health meter must preserve ordinary text exactly");
    }
    status.health_percent=50;status.health_row=2;
    Require(raster.Status(status).pixels==status_pixels.pixels,"meter outside active rows must not resurrect health");
    status.health_row=status.health_percent=-1;
    const std::u16string_view stance=u"Agachado · En sombra";
    status.line_count=3;status.lines[2].length=static_cast<std::uint32_t>(stance.size());
    std::copy(stance.begin(),stance.end(),status.lines[2].characters.begin());
    const auto stance_pixels=raster.Status(status);
    Require(stance_pixels.height==176&&stance_pixels.pixels!=status_pixels.pixels,
        "stance/shadow feedback must reach the wrist raster without clipping");
    if(argc>3)Save(stance_pixels,argv[3]);
    status.line_count=2;status.lines[2]={};
    Require(raster.Status(status).pixels==status_pixels.pixels,"removed stance left stale wrist pixels");
    const std::u16string_view optional[]{u"Cuenta atrás  10",u"Caballo · Salud 100% · Fatiga 100%",
        u"Concentración · No preparada",u"Agachado · En sombra"};
    status.line_count=6;
    for(int i=0;i<4;++i){status.lines[i+2].length=static_cast<std::uint32_t>(optional[i].size());
        std::copy(optional[i].begin(),optional[i].end(),status.lines[i+2].characters.begin());}
    const auto expanded=raster.Status(status);
    Require(expanded.height==320&&expanded.pixels.size()==512*320,
        "combined native optional feedback exceeded raster extent");
    if(argc>4)Save(expanded,argv[4]);
    status.line_count=2;for(int i=2;i<6;++i)status.lines[i]={};
    Require(raster.Status(status).pixels==status_pixels.pixels,"optional feedback left stale pixels");
    status.lines[1].characters[9]=u'2';
    Require(raster.Status(status).pixels!=status_pixels.pixels,"shot count changes must reach status pixels");
    status.line_count=11;Require(raster.Status(status).pixels.empty(),"oversized status resurrected pixels");
    status.line_count=1;status.lines[0].length=64;Require(raster.Status(status).pixels.empty(),"invalid status label accepted");
    status.lines[0].length=10;status.active=false;Require(raster.Status(status).pixels.empty(),"hidden status retained pixels");
    std::cout<<"Gameplay UI raster eligibility, objectives, cache and coverage passed\n";
}
