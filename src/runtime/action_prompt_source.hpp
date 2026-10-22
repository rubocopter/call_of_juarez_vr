#pragma once
#include "runtime/action_prompts.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace cojvr::runtime {
// Input-owner callbacks only. Origins are opaque; the name callback must also
// verify that its origin still belongs to a connected runtime device.
struct ActionPromptSource {
    void* context=nullptr;
    bool (*origins)(void*,PromptAction,std::array<std::uint64_t,16>&) noexcept=nullptr;
    bool (*name)(void*,std::uint64_t,std::array<char,256>&) noexcept=nullptr;
};
class ActionPromptOriginCache {
public:
    const ActionPromptSnapshot& Update(std::uint64_t generation,std::uint64_t now,
        const ActionPromptSource& source) noexcept {
        if(!generation){snapshot_={};have_sample_=false;return snapshot_;}
        const bool changed=!have_sample_||generation!=snapshot_.binding_generation||now<last_now_;
        last_now_=now;
        if(!changed&&(!unresolved_||now<retry_at_))return snapshot_;
        have_sample_=true;++revision_;
        if(changed){snapshot_={};pending_.fill(true);}
        snapshot_.active=true;snapshot_.binding_generation=generation;
        unresolved_=false;
        for(std::size_t i=0;i<snapshot_.labels.size();++i){
            if(!pending_[i])continue;
            const auto action=static_cast<PromptAction>(i);
            std::array<std::uint64_t,16> origins{};
            if(!source.origins||!source.name||!source.origins(source.context,action,origins)){
                unresolved_=true;continue;
            }
            std::uint64_t sole=0;bool ambiguous=false;
            for(auto origin:origins)if(origin){if(sole&&sole!=origin){ambiguous=true;break;}sole=origin;}
            // Multiple simultaneous sources/chords have no proven one-button
            // label. Prefer a semantic fallback over a misleading controller.
            if(ambiguous){pending_[i]=false;continue;}
            if(!sole){unresolved_=true;continue;}
            std::array<char,256> label{};
            if(!source.name(source.context,sole,label)){unresolved_=true;continue;}
            const auto end=std::find(label.begin(),label.end(),'\0');
            if(end==label.end()||!SetActionPromptLabel(snapshot_,action,
                std::string_view(label.data(),static_cast<std::size_t>(end-label.begin()))))unresolved_=true;
            else pending_[i]=false;
        }
        retry_at_=now>UINT64_MAX-1000?UINT64_MAX:now+1000;
        return snapshot_;
    }
    [[nodiscard]] std::uint64_t revision() const noexcept {return revision_;}
private:
    ActionPromptSnapshot snapshot_{};
    std::array<bool,static_cast<std::size_t>(PromptAction::count)> pending_{};
    std::uint64_t retry_at_=0,last_now_=0,revision_=0;
    bool have_sample_=false,unresolved_=false;
};
}
