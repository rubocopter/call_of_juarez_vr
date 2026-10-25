#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace cojvr::runtime {
// Row-vector, row-major clip transforms; runtime metres, never game addresses.
using TemporalMatrix=std::array<float,16>;
inline TemporalMatrix TemporalIdentity() noexcept {return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
[[nodiscard]] inline bool TemporalReprojection(const TemporalMatrix& current,
    const TemporalMatrix& previous,TemporalMatrix& output) noexcept {
    double augmented[4][8]{};
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column) {
        if(!std::isfinite(current[row*4+column])||!std::isfinite(previous[row*4+column]))return false;
        augmented[row][column]=current[row*4+column];augmented[row][4+column]=row==column?1:0;
    }
    for(unsigned column=0;column<4;++column) {
        unsigned pivot=column;
        for(unsigned row=column+1;row<4;++row)if(std::abs(augmented[row][column])>std::abs(augmented[pivot][column]))pivot=row;
        if(std::abs(augmented[pivot][column])<1e-12)return false;
        for(unsigned k=0;k<8;++k)std::swap(augmented[column][k],augmented[pivot][k]);
        const double divisor=augmented[column][column];for(auto& v:augmented[column])v/=divisor;
        for(unsigned row=0;row<4;++row)if(row!=column) {
            const double factor=augmented[row][column];
            for(unsigned k=0;k<8;++k)augmented[row][k]-=factor*augmented[column][k];
        }
    }
    TemporalMatrix result{};
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column) {
        double value=0;for(unsigned k=0;k<4;++k)value+=augmented[row][4+k]*previous[k*4+column];
        result[row*4+column]=static_cast<float>(value);
        if(!std::isfinite(result[row*4+column]))return false;
    }
    output=result;return true;
}
struct TemporalFrameIdentity {
    std::uint64_t device=0,generation=0,frame=0,pose=0,owner=0;
    std::uint32_t width=0,height=0,output_width=0,output_height=0,quality=0;
    bool native_stereo=false,tracking_valid=false;
};
class TemporalHistory final {
    TemporalFrameIdentity previous_{};bool have_=false;
public:
    [[nodiscard]] bool Admit(const TemporalFrameIdentity& frame,bool& reset) const noexcept {
        reset=true;
        if(!frame.device||!frame.generation||!frame.frame||!frame.pose||!frame.owner||
           !frame.width||!frame.height||frame.output_width<frame.width||frame.output_height<frame.height||
           !frame.native_stereo||!frame.tracking_valid)return false;
        // Repeats/delayed packets must not advance either eye's history. A new
        // generation owns its independent ordering domain, including frame=1.
        const bool same=have_&&previous_.device==frame.device&&previous_.generation==frame.generation;
        if(same&&frame.frame<=previous_.frame)return false;
        reset=!same||previous_.owner!=frame.owner||frame.frame!=previous_.frame+1||frame.pose<=previous_.pose||
            previous_.width!=frame.width||previous_.height!=frame.height||
            previous_.output_width!=frame.output_width||previous_.output_height!=frame.output_height||
            previous_.quality!=frame.quality;
        return true;
    }
    // Call only after BOTH eyes have completed reconstruction successfully.
    void Commit(const TemporalFrameIdentity& frame) noexcept {previous_=frame;have_=true;}
    void Invalidate() noexcept {have_=false;previous_={};}
};
}
