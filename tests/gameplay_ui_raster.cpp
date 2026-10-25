#include "backends/openvr/gameplay_ui_raster.hpp"
#include "backends/openvr/reticle_pixels.hpp"
#include "backends/openvr/holster_guide_pixels.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <fstream>
#include <string>
#include <cmath>
#include <set>
using namespace cojvr::runtime;
using namespace cojvr::backends::openvr;
void Require(bool ok,const char* why) { if(!ok){std::cerr<<why<<'\n';std::exit(1);} }
void SetLabel(UiLabel& label,std::u16string_view text){
    label={};label.length=static_cast<std::uint32_t>(text.size());
    std::copy(text.begin(),text.end(),label.characters.begin());
}
void Save(const HudTextPanel& panel,const char* path){
    std::ofstream out(path,std::ios::binary);
    out<<"P6\n"<<panel.width<<' '<<panel.height<<"\n255\n";
    for(const auto pixel:panel.pixels){const char rgb[]{static_cast<char>(pixel>>16),
        static_cast<char>(pixel>>8),static_cast<char>(pixel)};out.write(rgb,3);}
}
void WheelVisuals(GameplayUiRaster& raster,const EquipmentWheelSnapshot& source,const char* prefix){
    auto wheel=source;
    wheel.selected=-1;wheel.available_mask=255;
    for(auto& label:wheel.labels)label={};
    // Identical action text isolates the selection art from the native action label.
    SetLabel(wheel.cancel_label,u"Confirm");wheel.confirm_label=wheel.cancel_label;
    const auto neutral=raster.Wheel(wheel);
    std::set<std::uint32_t> material;
    std::size_t dark_warm=0,interior=0,rim_light=0,rim=0;
    for(int y=0;y<512;++y)for(int x=0;x<512;++x){
        const int dx=x-256,dy=y-256,r2=dx*dx+dy*dy;
        const auto p=neutral.pixels[y*512+x];
        Require(r2<246*246?(p>>24)==255:p==0,
            "wheel must retain exact circular opaque coverage and transparent exterior");
        const auto r=(p>>16)&255,g=(p>>8)&255,b=p&255;
        if(r2>110*110&&r2<210*210){
            material.insert(p);++interior;
            dark_warm+=r>g&&g>b&&r<110;
        }
        if(r2>237*237&&r2<243*243){++rim;rim_light+=r>100&&r>g&&g>b;}
    }
    Require(dark_warm>interior*8/10&&material.size()>12,
        "wheel needs a quiet warm leather material with procedural tonal variation");
    Require(rim_light>rim*7/10,"wheel needs a continuous readable brass rim");
    for(int slot=0;slot<8;++slot){
        wheel.selected=slot;
        const auto selected=raster.Wheel(wheel);
        std::size_t changed=0,outside=0,amber=0;
        for(int y=0;y<512;++y)for(int x=0;x<512;++x){
            const int dx=x-256,dy=y-256,r2=dx*dx+dy*dy;
            if(r2<95*95||r2>222*222)continue;
            float angle=std::atan2(static_cast<float>(dx),static_cast<float>(-dy));
            if(angle<0)angle+=6.28318530718F;
            const int sector=static_cast<int>(angle/.78539816339F+.5F)%8;
            const auto p=selected.pixels[y*512+x];
            if(p!=neutral.pixels[y*512+x]){++changed;outside+=sector!=slot;}
            amber+=((p>>16)&255)>150&&((p>>8)&255)>85&&(p&255)<110;
        }
        Require(changed>3000&&outside<changed/50,
            "selection art must follow each native sector without lighting adjacent slots");
        Require(amber>100&&amber<5000,"selection needs a restrained visible amber accent");
    }
    wheel.selected=-1;wheel.available_mask=254;
    const auto unavailable=raster.Wheel(wheel);
    std::size_t unavailable_mark=0;
    for(std::size_t i=0;i<neutral.pixels.size();++i)unavailable_mark+=neutral.pixels[i]!=unavailable.pixels[i];
    Require(unavailable_mark>30,"unavailable slots need a non-text cue even without a copied label");
    const auto unavailable_revision=raster.wheel_revision();
    wheel.available_mask=255;
    Require(raster.Wheel(wheel).pixels==neutral.pixels&&raster.wheel_revision()==unavailable_revision+1,
        "native availability alone must invalidate cache and restore the available presentation");
    wheel.selected=0;const auto allowed_selection=raster.Wheel(wheel);
    wheel.available_mask=254;const auto denied_selection=raster.Wheel(wheel);
    std::size_t allowed_amber=0,denied_amber=0;
    for(int y=0;y<512;++y)for(int x=0;x<512;++x){
        const int dx=x-256,dy=y-256,r2=dx*dx+dy*dy;
        if(r2<210*210||r2>222*222)continue;
        const auto amber=[](std::uint32_t p){return ((p>>16)&255)>150&&((p>>8)&255)>85&&(p&255)<110;};
        allowed_amber+=amber(allowed_selection.pixels[y*512+x]);
        denied_amber+=amber(denied_selection.pixels[y*512+x]);
    }
    Require(allowed_amber>denied_amber+500,
        "denied selection must not display the available slot's bright amber arc");
    wheel.labels=source.labels;wheel.cancel_label=source.cancel_label;wheel.confirm_label=source.confirm_label;
    wheel.available_mask=source.available_mask;wheel.selected=0;
    const auto labelled=raster.Wheel(wheel);
    auto no_labels=wheel;for(auto& label:no_labels.labels)label={};
    const auto blank=raster.Wheel(no_labels);
    for(int slot=0;slot<8;++slot){
        const float angle=slot*3.14159265359F/4;
        const int cx=256+static_cast<int>(165*std::sin(angle));
        const int cy=256-static_cast<int>(165*std::cos(angle));
        std::size_t glyphs=0;
        for(int y=cy-34;y<cy+34;++y)for(int x=cx-68;x<cx+68;++x)
            glyphs+=labelled.pixels[y*512+x]!=blank.pixels[y*512+x];
        Require(glyphs>100,"every native slot label, including unavailable labels, must remain readable");
    }
    if(prefix){
        const auto save=[&](const HudTextPanel& p,const char* name){Save(p,(std::string(prefix)+name+".ppm").c_str());};
        save(labelled,"selected");
        wheel.selected=2;save(raster.Wheel(wheel),"unavailable");
        wheel.selected=-1;save(raster.Wheel(wheel),"cancel");
        wheel.available_mask=255;wheel.selected=7;save(raster.Wheel(wheel),"all-available");
        wheel.available_mask=0;save(raster.Wheel(wheel),"none-available");
        const std::u16string_view spanish[]{u"Arma larga",u"Arco",u"Pistola derecha",u"Dinamita",
            u"Manos",u"Soltar",u"Pistola izquierda",u"Látigo"};
        for(int i=0;i<8;++i)SetLabel(wheel.labels[i],spanish[i]);
        wheel.available_mask=255;SetLabel(wheel.confirm_label,u"Confirmar");SetLabel(wheel.cancel_label,u"Cancelar");
        save(raster.Wheel(wheel),"spanish");
        wheel.selected=-1;save(raster.Wheel(wheel),"spanish-cancel");
        const std::u16string_view english[]{u"Long weapon",u"Bow",u"Right pistol",u"Dynamite",
            u"Hands",u"Discard",u"Left pistol",u"Bible"};
        for(int i=0;i<8;++i)SetLabel(wheel.labels[i],english[i]);
        wheel.confirm_label=source.confirm_label;wheel.cancel_label=source.cancel_label;
        wheel.selected=6;save(raster.Wheel(wheel),"english");
    }
    wheel=source;wheel.selected=0;
    const auto original=raster.Wheel(wheel);
    const auto revision=raster.wheel_revision();
    SetLabel(wheel.labels[0],u"Rifle & acción");
    Require(raster.Wheel(wheel).pixels!=original.pixels&&raster.wheel_revision()==revision+1,
        "copied accented native labels must invalidate the wheel cache");
    wheel=source;wheel.valid=false;
    Require(raster.Wheel(wheel).pixels.empty(),"invalid native wheel must clear its decorated surface");
    wheel.valid=true;
    for(int invalid:{-2,8}){wheel.selected=invalid;
        Require(raster.Wheel(wheel).pixels.empty(),"invalid selection must not resurrect decorated wheel pixels");}
}
int main(int argc,char** argv) {
    const auto& guides=HolsterGuidePanels();
    std::set<std::vector<std::uint32_t>> guide_states;
    for(const auto& guide:guides){
        Require(guide.width==128&&guide.height==128&&guide.pixels.size()==128*128,
            "holster ring must use a bounded higher-definition raster");
        std::size_t coverage=0,edge_levels=0;std::set<unsigned> alpha_levels;
        for(unsigned y=0;y<guide.height;++y)for(unsigned x=0;x<guide.width;++x){
            const unsigned alpha=guide.pixels[y*guide.width+x]>>24;
            const float dx=x+.5F-guide.width*.5F,dy=y+.5F-guide.height*.5F;
            const float radius=std::hypot(dx,dy)/(guide.width*.5F);
            if(radius<.94F||radius>=1.F)Require(alpha==0,"holster hint obscures its centre or exterior");
            coverage+=alpha;alpha_levels.insert(alpha);edge_levels+=alpha>0&&alpha<20;
        }
        Require(coverage<128*128*255/25,"holster outline has excessive pixel coverage");
        Require(alpha_levels.size()>12&&edge_levels>20,"holster ring needs smooth subpixel edge coverage");
        Require(HolsterGuidePanels()[&guide-guides.data()].pixels.data()==guide.pixels.data(),
            "immutable holster pixels rebuilt on repeat");
        guide_states.insert(guide.pixels);
    }
    Require(guide_states.size()==3,"holster guide availability states collapsed");
    Require(SupportGripGuidePanels()[0].width==64&&AmmunitionPickupGuidePanels()[0].width==64,
        "holster refinement changed support/ammunition glyph geometry");
    Require(NoShootReticlePixel(0,0)==0xFFFF4038U&&NoShootReticlePixel(6,-6)==0xFFFF4038U,"warning must be a red X at aim point");
    Require(NoShootReticlePixel(1,0)==0xFF000000U,"warning must retain contrasting outline");
    Require(NoShootReticlePixel(0,6)==0&&NoShootReticlePixel(9,9)==0,"warning must preserve world outside diagonals/footprint");
    GameplayUiRaster raster;
    ActionPromptSnapshot prompts{};prompts.active=true;prompts.binding_generation=7;
    prompts.glyph_sample_ms=100;prompts.glyph_deadline_ms=1600;
    Require(SetActionPromptLabel(prompts,PromptAction::alternate_fire,"Right Circle"),"control fixture label rejected");
    prompts.glyphs[static_cast<std::size_t>(PromptAction::alternate_fire)]=
        QualifyPromptGlyph("playstation_vr2_sense","/user/hand/right","button_b",
            "/input/circle","button","click","button",17);
    const auto with_art=raster.ControlHint(prompts,PromptAction::alternate_fire,7,101,false);
    const auto art_revision=raster.control_hint_revision();
    (void)raster.ControlHint(prompts,PromptAction::alternate_fire,7,102,false);
    Require(raster.control_hint_revision()==art_revision,"unchanged control row rebuilt its pixels");
    Require(with_art.width==512&&with_art.height==64&&with_art.pixels.size()==512*64,
        "control hint must remain a bounded renderer-owned row");
    auto text_only=prompts;text_only.glyphs={};
    const auto without_art=raster.ControlHint(text_only,PromptAction::alternate_fire,7,101,false);
    Require(raster.control_hint_revision()==art_revision+1,"glyph-only change did not invalidate upload cache");
    Require(with_art.pixels!=without_art.pixels&&!without_art.pixels.empty(),
        "qualified art must decorate, with readable text-only fallback");
    Require(raster.ControlHint(prompts,PromptAction::alternate_fire,7,1600,false).pixels==without_art.pixels,
        "retained snapshot must lose art at its exact deadline");
    const ActionPromptSnapshot fallback{};
    const auto semantic=raster.ControlHint(fallback,PromptAction::alternate_fire,0,101,false);
    Require(raster.ControlHint(prompts,PromptAction::alternate_fire,8,101,false).pixels==semantic.pixels,
        "stale generation retained old glyph or old controller text");
    Require(raster.ControlHint(prompts,PromptAction::alternate_fire,7,101,true).pixels.empty(),
        "optional control row obscured higher-priority native text");
    Require(raster.ControlHint(prompts,PromptAction::alternate_fire,7,101,false).pixels==with_art.pixels,
        "priority withdrawal failed to restore current row");
    Require(raster.ControlHint(prompts,PromptAction::count,7,101,false).pixels.empty(),
        "invalid action resurrected cached control pixels");
    std::set<std::vector<std::uint32_t>> symbols;
    for(auto kind:{PromptGlyphKind::cross,PromptGlyphKind::circle,PromptGlyphKind::square,
        PromptGlyphKind::triangle,PromptGlyphKind::trigger,PromptGlyphKind::grip}){
        prompts.glyphs[6]={kind,PromptGlyphHand::right,17};
        const auto panel=raster.ControlHint(prompts,PromptAction::alternate_fire,7,101,false);
        Require(panel.pixels.front()==0,"optional symbol flooded transparent exterior");
        symbols.insert(panel.pixels);
    }
    Require(symbols.size()==6,"procedural identifiers collapsed to the same glyph");
    HudTextPanel coaching{};coaching.width=1024;coaching.height=72;
    coaching.pixels.assign(1024*72,0xFF113355U);
    const auto full=raster.DecorateControlHint(coaching,1,prompts,PromptAction::alternate_fire,7,101,true);
    Require(full.width==1104&&full.height==72,"coaching glyph must add a bounded margin to the complete text");
    for(unsigned y=0;y<72;++y)for(unsigned x=0;x<1024;++x)
        Require(full.pixels[y*1104+x+80]==coaching.pixels[y*1024+x],
            "glyph decoration damaged original instruction pixels");
    const auto full_revision=raster.control_decoration_revision();
    (void)raster.DecorateControlHint(coaching,1,prompts,PromptAction::alternate_fire,7,102,true);
    Require(raster.control_decoration_revision()==full_revision,"unchanged coaching decoration rebuilt");
    Require(&raster.DecorateControlHint(coaching,1,prompts,PromptAction::alternate_fire,7,1600,true)==&coaching&&
        raster.control_decoration_revision()>full_revision,"expired glyph did not return the complete text fallback");
    Require(&raster.DecorateControlHint(coaching,1,prompts,PromptAction::alternate_fire,7,101,false)==&coaching,
        "native priority changed original text pixels");
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
    SetLabel(wheel.cancel_label,u"Cancel");SetLabel(wheel.confirm_label,u"Confirm");
    const auto first=raster.Wheel(wheel);
    if(argc>1)Save(first,argv[1]);
    Require(first.width==512 && first.height==512 && first.pixels.size()==512*512,
        "eligible wheel must rasterize a bounded circular surface");
    Require(first.pixels[0]==0 && (first.pixels[256*512+256]>>24)==255,
        "wheel corners must remain transparent with defined dial coverage");
    const auto rev=raster.wheel_revision();
    (void)raster.Wheel(wheel);
    Require(rev==raster.wheel_revision(),"unchanged wheel must reuse cached pixels");
    WheelVisuals(raster,wheel,argc>7?argv[7]:nullptr);
    wheel.selected=-1;const auto english_cancel=raster.Wheel(wheel);
    const auto english_revision=raster.wheel_revision();
    SetLabel(wheel.cancel_label,u"Cancelar");const auto spanish_cancel=raster.Wheel(wheel);
    Require(spanish_cancel.pixels!=english_cancel.pixels&&raster.wheel_revision()==english_revision+1,
        "wheel locale must invalidate the raster cache and show copied action text");
    wheel.selected=0;SetLabel(wheel.confirm_label,u"Confirmar");
    Require(raster.Wheel(wheel).pixels!=first.pixels,"translated confirm text did not reach the wheel");
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
    const std::u16string_view optional[]{u"Cuenta atrás  10",u"Caballo · Salud 75% · Fatiga 40%",
        u"Concentración · No preparada",u"Agachado · En sombra"};
    status.line_count=6;
    for(int i=0;i<4;++i){status.lines[i+2].length=static_cast<std::uint32_t>(optional[i].size());
        std::copy(optional[i].begin(),optional[i].end(),status.lines[i+2].characters.begin());}
    const auto expanded=raster.Status(status);
    Require(expanded.height==320&&expanded.pixels.size()==512*320,
        "combined native optional feedback exceeded raster extent");
    if(argc>4)Save(expanded,argv[4]);
    using Span=WristStatusMeterSpan;
    status.meters[0]={3,75,Span::left,false};
    status.meters[1]={3,40,Span::right,true};
    status.badge={4,WristStatusBadgeKind::unavailable};
    const auto decorated=raster.Status(status);
    if(argc>6)Save(decorated,(std::string(argv[6])+"horse-75-40.ppm").c_str());
    Require(decorated.pixels!=expanded.pixels&&decorated.height==expanded.height,
        "optional native meters and state badge must be visible within existing card extent");
    const auto meter_coverage=[](const HudTextPanel& panel,int start){
        int lit=0;for(int x=start;x<start+224;++x){const auto pixel=panel.pixels[202*512+x];
            lit+=std::max({(pixel>>16)&255,(pixel>>8)&255,pixel&255})>100;}return lit;
    };
    Require(meter_coverage(decorated,24)==168&&meter_coverage(decorated,264)==89,
        "paired meters must display health75 and fatigue40 without a full-width overlap");
    status.meters[1].percent=100;SetLabel(status.lines[3],u"Caballo · Salud 75% · Fatiga 100%");
    const auto exhausted=raster.Status(status);
    if(argc>6)Save(exhausted,(std::string(argv[6])+"horse-75-100.ppm").c_str());
    Require(meter_coverage(exhausted,264)==224&&
        ((exhausted.pixels[202*512+264]>>16)&255)>((exhausted.pixels[202*512+264]>>8)&255),
        "full fatigue must fill its own meter with warning colour, never mean full stamina");
    status.meters[1].percent=0;SetLabel(status.lines[3],u"Caballo · Salud 75% · Fatiga 0%");
    const auto rested=raster.Status(status);
    Require(meter_coverage(rested,264)==0,"zero fatigue must leave an empty meter");
    status.meters={};SetLabel(status.lines[3],optional[1]);
    const auto unavailable_badge=raster.Status(status);
    for(auto kind:{WristStatusBadgeKind::ready,WristStatusBadgeKind::active,WristStatusBadgeKind::cooldown}){
        status.badge.kind=kind;
        SetLabel(status.lines[4],kind==WristStatusBadgeKind::ready?u"Concentración · Lista":
            kind==WristStatusBadgeKind::active?u"Concentración · En uso":u"Concentración · Recargando");
        const auto badge=raster.Status(status);
        Require(badge.pixels!=unavailable_badge.pixels,"distinct committed status modes must have distinct glyphs");
        if(argc>6)Save(badge,(std::string(argv[6])+"badge-"+
            std::to_string(static_cast<int>(kind))+".ppm").c_str());
        const auto color=kind==WristStatusBadgeKind::ready?0xFF8FB883U:
            kind==WristStatusBadgeKind::active?0xFF5AC8EFU:0xFFE8BA62U;
        int glyph_pixels=0;
        for(int y=218;y<246;++y)for(int x=452;x<480;++x)glyph_pixels+=badge.pixels[y*512+x]==color;
        Require(glyph_pixels>30,"state feedback must draw a visible bounded glyph, not only clip text");
        for(int y=0;y<320;++y)if(y<208||y>=256)
            for(int x=0;x<512;++x)Require(badge.pixels[y*512+x]==expanded.pixels[y*512+x],
                "state badge changed unrelated health/ammo/countdown/horse/posture rows");
    }
    status.badge={};SetLabel(status.lines[4],optional[2]);
    for(auto bad: {WristStatusMeter{3,101,Span::left,false},WristStatusMeter{-1,50,Span::left,false},
        WristStatusMeter{6,50,Span::right,false},WristStatusMeter{3,50,static_cast<Span>(99),false}}){
        status.meters[0]=bad;
        Require(raster.Status(status).pixels==expanded.pixels,"invalid optional meter must retain ordinary text exactly");
    }
    status.meters={};status.badge={6,WristStatusBadgeKind::ready};
    Require(raster.Status(status).pixels==expanded.pixels,"badge outside native visible rows resurrected output");
    status.badge={4,static_cast<WristStatusBadgeKind>(99)};
    Require(raster.Status(status).pixels==expanded.pixels,"unknown badge must retain text fallback");
    status.badge={};
    status.meters[0]={3,40,Span::left,false};status.meters[1]={3,60,Span::left,false};
    Require(raster.Status(status).pixels==expanded.pixels,"ambiguous overlapping optional meters must preserve text");
    status.meters={};status.health_row=0;status.health_percent=50;
    const auto essential_health=raster.Status(status);
    status.meters[0]={0,100,Span::left,false};
    Require(raster.Status(status).pixels==essential_health.pixels,"optional meter overwrote the essential health gauge");
    status.meters={};status.health_row=status.health_percent=-1;
    status.line_count=2;for(int i=2;i<6;++i)status.lines[i]={};
    Require(raster.Status(status).pixels==status_pixels.pixels,"optional feedback left stale pixels");
    status.lines[1].characters[9]=u'2';
    Require(raster.Status(status).pixels!=status_pixels.pixels,"shot count changes must reach status pixels");
    status.line_count=11;Require(raster.Status(status).pixels.empty(),"oversized status resurrected pixels");
    status.line_count=1;status.lines[0].length=64;Require(raster.Status(status).pixels.empty(),"invalid status label accepted");
    status.lines[0].length=10;status.active=false;Require(raster.Status(status).pixels.empty(),"hidden status retained pixels");
    std::cout<<"Gameplay UI raster eligibility, objectives, cache and coverage passed\n";
}
