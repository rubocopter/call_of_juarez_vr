#include "runtime/action_prompt_source.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace cojvr::runtime;
namespace {
void Check(bool v,const char* why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
struct Source {
    int origins_calls=0,name_calls=0;
    bool disconnected=false,failed=false,ambiguous=false,bad_name=false,duplicate=false,absent=false,all_bound=false;
    const char* label="Left trigger";
    static bool Origins(void* p,PromptAction a,std::array<std::uint64_t,16>& out) noexcept {
        auto& s=*static_cast<Source*>(p);++s.origins_calls;
        if(s.failed)return false;
        if(a==PromptAction::left_fire&&!s.absent){out[0]=17;if(s.ambiguous)out[1]=19;else if(s.duplicate)out[1]=17;}
        else if(a!=PromptAction::left_fire&&s.all_bound)out[0]=17;
        return true;
    }
    static bool Name(void* p,std::uint64_t id,std::array<char,256>& out) noexcept {
        auto& s=*static_cast<Source*>(p);++s.name_calls;
        if(s.disconnected||id!=17)return false;
        if(s.bad_name){out.fill('x');return true;}
        std::memcpy(out.data(),s.label,std::strlen(s.label)+1);return true;
    }
    ActionPromptSource api(){return {this,Origins,Name};}
};
}
int main(){
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
