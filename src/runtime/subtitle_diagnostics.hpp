#pragma once
#include <cstdint>
#include <string_view>

namespace cojvr::runtime {
// Observation only. These values never authorize text, cameras or submission.
enum class SubtitleSource : std::uint8_t { none, dialogue, cinematic };
struct SubtitleCaptureDiagnostic {
    SubtitleSource source=SubtitleSource::none;
    std::uint64_t owner_epoch=0, pose_sequence=0, recenter_sequence=0;
    bool read_valid=false;
};
inline std::uint64_t SubtitleFingerprint(std::u16string_view text) noexcept {
    std::uint64_t hash=14695981039346656037ULL;
    for(auto c:text){hash^=static_cast<std::uint16_t>(c);hash*=1099511628211ULL;}
    return hash; // Correlation only, not a cryptographic identity.
}
// Independent per-stream lifetime cap; no resets on chapter/recenter. Reserve
// records for later owners, and never format duplicate/repeated captures.
class SubtitleDiagnosticBudget {
public:
    bool Admit(std::uint64_t frame,SubtitleSource source,std::uint64_t owner,
        std::uint64_t signature,unsigned eye_status,bool visible) noexcept {
        if(!frame||frame<=last_seen_||total_>=128)return false;
        last_seen_=frame;
        const bool new_owner=!have_owner_||owner!=owner_||source!=source_;
        if(!visible&&!was_visible_&&!new_owner)return false;
        if(new_owner||(visible&&!was_visible_)) {
            owner_=owner;source_=source;per_owner_=0;have_owner_=true;
        }
        const bool geometry_changed=eye_status!=eye_status_||visible!=was_visible_;
        const bool admit=new_owner||geometry_changed||
            (per_owner_<32&&(per_owner_<3||signature!=signature_||frame-last_emitted_>=180));
        was_visible_=visible;
        if(!admit)return false;
        signature_=signature;eye_status_=eye_status;last_emitted_=frame;++per_owner_;++total_;return true;
    }
private:
    std::uint64_t owner_=0,signature_=0,last_seen_=0,last_emitted_=0;
    SubtitleSource source_=SubtitleSource::none;
    unsigned total_=0,per_owner_=0,eye_status_=0;
    bool have_owner_=false,was_visible_=false;
};
}
