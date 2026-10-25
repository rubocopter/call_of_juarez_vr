#include "runtime/action_prompt_source.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace cojvr::runtime;
namespace {
void Check(bool v,const char* why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
struct Source {
    int origins_calls=0,name_calls=0,glyph_calls=0;
    bool disconnected=false,failed=false,ambiguous=false,bad_name=false,duplicate=false,absent=false,all_bound=false;
    const char* label="Left trigger";
    std::uint64_t origin=17;
    static bool Origins(void* p,PromptAction a,std::array<std::uint64_t,16>& out) noexcept {
        auto& s=*static_cast<Source*>(p);++s.origins_calls;
        if(s.failed)return false;
        if(a==PromptAction::left_fire&&!s.absent){out[0]=s.origin;if(s.ambiguous)out[1]=19;else if(s.duplicate)out[1]=s.origin;}
        else if(a!=PromptAction::left_fire&&s.all_bound)out[0]=s.origin;
        return true;
    }
    static bool Name(void* p,std::uint64_t id,std::array<char,256>& out) noexcept {
        auto& s=*static_cast<Source*>(p);++s.name_calls;
        if(s.disconnected||id!=s.origin)return false;
        if(s.bad_name){out.fill('x');return true;}
        std::memcpy(out.data(),s.label,std::strlen(s.label)+1);return true;
    }
    static PromptGlyph Glyph(void* p,PromptAction,std::uint64_t origin) noexcept {
        auto& s=*static_cast<Source*>(p);++s.glyph_calls;
        if(s.disconnected)return {};
        return QualifyPromptGlyph("playstation_vr2_sense","/user/hand/right","button_b",
            "/input/circle","button","click","button",origin);
    }
    ActionPromptSource api(bool glyphs=false){return {this,Origins,Name,glyphs?Glyph:nullptr};}
};
}
int main(){
    // These cases require the actual qualified source tuple; an action name or
    // a localized button name alone must never select a controller pictogram.
    const auto sense=QualifyPromptGlyph("playstation_vr2_sense","/user/hand/right",
        "button_b","/input/circle","button","click","button",17);
    Check(sense.kind==PromptGlyphKind::circle&&sense.hand==PromptGlyphHand::right,
        "verified Sense circle component did not qualify");
    Check(QualifyPromptGlyph("oculus_touch","/user/hand/right","button_b",
        "/input/circle","button","click","button",17).kind==PromptGlyphKind::none,
        "unknown controller borrowed Sense art");
    Check(QualifyPromptGlyph("playstation_vr2_sense","/user/hand/left","button_b",
        "/input/circle","button","click","button",17).kind==PromptGlyphKind::none,
        "wrong hand/path qualified");
    Check(QualifyPromptGlyph("playstation_vr2_sense","/user/hand/right","button_b",
        "/input/circle","chord","click","button",17).kind==PromptGlyphKind::none,
        "chord was presented as one button");
    Check(QualifyPromptGlyph("playstation_vr2_sense","/user/hand/right","button_a",
        "/input/circle","button","click","button",17).kind==PromptGlyphKind::none,
        "mismatched actual component qualified");
    Check(QualifyPromptGlyph("playstation_vr2_sense","/user/hand/right","button_b",
        "/input/circle","button","touch","button",17).kind==PromptGlyphKind::none,
        "touch binding borrowed click instruction");
    ActionPromptSnapshot graphical{};graphical.active=true;graphical.binding_generation=9;
    graphical.glyphs[0]=sense;graphical.glyph_sample_ms=100;graphical.glyph_deadline_ms=1600;
    Check(ResolveActionPromptGlyph(graphical,PromptAction::left_fire,9,101).kind==PromptGlyphKind::circle,
        "fresh captured glyph rejected");
    Check(ResolveActionPromptGlyph(graphical,PromptAction::left_fire,10,101).kind==PromptGlyphKind::none&&
        ResolveActionPromptGlyph(graphical,PromptAction::left_fire,9,1600).kind==PromptGlyphKind::none&&
        ResolveActionPromptGlyph(graphical,PromptAction::left_fire,9,99).kind==PromptGlyphKind::none,
        "stale generation/deadline/clock regression retained glyph");
    Source art_source;art_source.all_bound=true;ActionPromptOriginCache art_cache;
    auto art=art_cache.Update(1,100,art_source.api(true));
    Check(ResolveActionPromptGlyph(art,PromptAction::reload,1,101).kind==PromptGlyphKind::circle,
        "source did not publish qualified glyph");
    const auto name_calls=art_source.name_calls,glyph_calls=art_source.glyph_calls;
    (void)art_cache.Update(1,1099,art_source.api(true));
    Check(art_source.glyph_calls==glyph_calls,"glyphs re-queried before refresh budget");
    art_source.disconnected=true;art=art_cache.Update(1,1100,art_source.api(true));
    Check(art_source.name_calls==name_calls&&art.glyphs[0].kind==PromptGlyphKind::none,
        "disconnect retained art or re-read settled labels");
    art_source.disconnected=false;art=art_cache.Update(2,1101,art_source.api(true));
    Check(ResolveActionPromptGlyph(art,PromptAction::reload,1,1102).kind==PromptGlyphKind::none&&
        ResolveActionPromptGlyph(art,PromptAction::reload,2,2601).kind==PromptGlyphKind::none,
        "old binding generation or retained frame escaped glyph expiry");
    art_source.ambiguous=true;art=art_cache.Update(3,1102,art_source.api(true));
    Check(art.glyphs[0].kind==PromptGlyphKind::none,"multiple origins advertised one symbol");
    Check(!art_cache.Update(0,1103,art_source.api(true)).active,"invalid epoch retained art");
    Source rebound;rebound.all_bound=true;ActionPromptOriginCache rebound_cache;
    (void)rebound_cache.Update(1,100,rebound.api(true));
    rebound.origin=23;rebound.label="Rebound Circle";
    auto rebound_snapshot=rebound_cache.Update(1,1100,rebound.api(true));
    Check(rebound_snapshot.glyphs[0].origin==23&&
        ResolveActionPrompt(rebound_snapshot,PromptAction::left_fire)==u"Rebound Circle",
        "changed origin paired a fresh glyph with an old cached label");
    rebound.absent=true;rebound_snapshot=rebound_cache.Update(1,2100,rebound.api(true));
    Check(rebound_snapshot.glyphs[0].kind==PromptGlyphKind::none&&rebound_snapshot.labels[0].view().empty(),
        "unavailable binding retained cached control presentation");
    Source source;ActionPromptOriginCache cache;
    auto snapshot=cache.Update(1,100,source.api());
    Check(snapshot.active&&snapshot.binding_generation==1&&ResolveActionPrompt(snapshot,PromptAction::left_fire)==u"Left trigger","real origin label was not captured");
    Check(source.name_calls==1,"unbound actions queried nonexistent origin names");
    const int calls=source.origins_calls;
    (void)cache.Update(1,9000,source.api());
    Check(source.name_calls==1&&source.origins_calls==calls+static_cast<int>(PromptAction::count)-1,
        "retry must query missing origins without re-reading settled labels");
    Source settled;settled.all_bound=true;ActionPromptOriginCache settled_cache;
    (void)settled_cache.Update(1,100,settled.api());const auto settled_calls=settled.origins_calls;
    (void)settled_cache.Update(1,9000,settled.api());
    Check(settled.origins_calls==settled_calls,"fully settled metadata made recurring native calls");
    source.label="Left X";snapshot=cache.Update(2,9001,source.api());
    Check(ResolveActionPrompt(snapshot,PromptAction::left_fire)==u"Left X","binding generation retained the old button");
    source.disconnected=true;snapshot=cache.Update(3,9002,source.api());
    Check(snapshot.labels[0].view().empty(),"disconnected device retained its old label");
    const int missing=source.name_calls;(void)cache.Update(3,9999,source.api());
    Check(missing==source.name_calls,"missing device retried every frame");
    source.disconnected=false;snapshot=cache.Update(3,10002,source.api());
    Check(ResolveActionPrompt(snapshot,PromptAction::left_fire)==u"Left X","bounded retry did not recover available metadata");
    source.ambiguous=true;snapshot=cache.Update(4,10003,source.api());
    Check(snapshot.labels[0].view().empty(),"ambiguous multi-origin binding advertised an arbitrary button");
    source.ambiguous=false;source.bad_name=true;snapshot=cache.Update(5,10004,source.api());
    Check(snapshot.labels[0].view().empty(),"unterminated runtime name escaped its bound");
    source.bad_name=false;source.failed=true;snapshot=cache.Update(6,10005,source.api());
    Check(snapshot.active&&snapshot.labels[0].view().empty(),"failed lookup retained old device text");
    Check(!cache.Update(0,10006,source.api()).active,"missing generation retained a source");
    source.failed=false;source.duplicate=true;snapshot=cache.Update(7,10007,source.api());
    Check(ResolveActionPrompt(snapshot,PromptAction::left_fire)==u"Left X",
        "duplicate reports of one origin were treated as different controls");
    const auto before_regression=source.origins_calls;source.label="Changed after clock reset";
    snapshot=cache.Update(7,1,source.api());
    Check(source.origins_calls>before_regression&&
        ResolveActionPrompt(snapshot,PromptAction::left_fire)==u"Changed after clock reset",
        "clock regression retained cached origin metadata");
    Source transient;transient.all_bound=true;transient.absent=true;ActionPromptOriginCache transient_cache;
    snapshot=transient_cache.Update(9,100,transient.api());
    Check(snapshot.labels[0].view().empty(),"empty origin invented a controller");
    const auto pending_calls=transient.origins_calls;transient.absent=false;
    (void)transient_cache.Update(9,1099,transient.api());
    Check(transient.origins_calls==pending_calls,"transient empty origin retried too soon");
    snapshot=transient_cache.Update(9,1100,transient.api());
    Check(ResolveActionPrompt(snapshot,PromptAction::left_fire)==u"Left trigger"&&
        transient.origins_calls==pending_calls+1,"empty origin never recovered or queried settled controls again");
    std::cout<<"action prompt source/cache checks passed\n";
}
