#pragma once
#include <array>
#include <cstdint>
namespace cojvr::games::call_of_juarez {
// Optional content evidence, independent of the much larger binding census.
// Stable owner windows prevent the census's startup/level-transition budget
// from being the only source of temporal-input observations. Never admission.
class CoJTemporalProbeSchedule {
    std::uint64_t owner_=0,generation_=0,observed_=0,next_=0,frame_=0;
    unsigned count_=0;
public:
    bool Acquire(std::uint64_t now,std::uint64_t frame,std::uint64_t owner,
                 std::uint64_t generation,bool gameplay) noexcept {
        if(!gameplay||!owner||!generation||!frame||now<observed_) {
            owner_=generation_=next_=frame_=0;observed_=now;return false;
        }
        observed_=now;
        if(owner!=owner_||generation!=generation_) {
            owner_=owner;generation_=generation;next_=now+2000;frame_=0;return false;
        }
        if(count_>=8||now<next_||frame<=frame_)return false;
        frame_=frame;next_=now+10000;++count_;return true;
    }
};
class CoJTemporalProbePhases {
    struct Key {std::uintptr_t target=0,shader=0;unsigned sampler=0;
        bool operator==(const Key&) const = default;};
    std::array<Key,3> seen_{};unsigned count_=0;
public:
    unsigned Observe(std::uintptr_t target,std::uintptr_t shader,unsigned sampler) noexcept {
        if(!target||!shader||sampler>=16||count_==seen_.size())return 0;
        const Key value{target,shader,sampler};
        for(unsigned i=0;i<count_;++i)if(seen_[i]==value)return 0;
        seen_[count_++]=value;return count_;
    }
};
}
