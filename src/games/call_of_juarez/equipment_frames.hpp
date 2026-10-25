#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace cojvr::games::call_of_juarez {
// Exact-game MSH convention: row vectors, translation at [12..14], native cm.
// Keep all three authored axes and positive scale. No XR conversion, native
// pointer, setter, attachment, inventory permission or contact policy lives here.
struct CoJEquipmentFrame {
    std::array<double,16> values{};
};

inline CoJEquipmentFrame CoJEquipmentIdentity() noexcept {
    return {{{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}}};
}

namespace equipment_frame_detail {
inline CoJEquipmentFrame Multiply(const CoJEquipmentFrame& a,const CoJEquipmentFrame& b) noexcept {
    CoJEquipmentFrame result;
    for(std::size_t i=0;i<4;++i)for(std::size_t j=0;j<4;++j)
        for(std::size_t k=0;k<4;++k)result.values[i*4+j]+=a.values[i*4+k]*b.values[k*4+j];
    return result;
}
inline bool IdentityWithin(const CoJEquipmentFrame& m) noexcept {
    for(std::size_t i=0;i<16;++i)
        if(!std::isfinite(m.values[i]) || std::abs(m.values[i]-(i%5==0?1.0:0.0))>1e-6)return false;
    return true;
}
} // namespace equipment_frame_detail

inline bool ValidateCoJEquipmentFrame(const CoJEquipmentFrame& frame) noexcept {
    const auto& a=frame.values;
    for(double v:a)if(!std::isfinite(v))return false;
    if(std::abs(a[3])>1e-9 || std::abs(a[7])>1e-9 ||
       std::abs(a[11])>1e-9 || std::abs(a[15]-1)>1e-9)return false;
    double scale_product=1;
    for(std::size_t i=0;i<3;++i){
        const auto o=i*4;
        const double scale=std::sqrt(a[o]*a[o]+a[o+1]*a[o+1]+a[o+2]*a[o+2]);
        if(scale<1e-4 || scale>1e4)return false;
        scale_product*=scale;
    }
    const double det=a[0]*(a[5]*a[10]-a[6]*a[9])-a[1]*(a[4]*a[10]-a[6]*a[8])+
                     a[2]*(a[4]*a[9]-a[5]*a[8]);
    // Geometry supports positive affine scale/shear, with a conservative
    // normalized-volume floor. Reflection/near-collinearity never normalize away.
    return det/scale_product>=1e-6;
}

inline bool ComposeCoJEquipmentFrames(const CoJEquipmentFrame& a,const CoJEquipmentFrame& b,
                                      CoJEquipmentFrame& out) noexcept {
    if(!ValidateCoJEquipmentFrame(a)||!ValidateCoJEquipmentFrame(b)){out={};return false;}
    const auto result=equipment_frame_detail::Multiply(a,b);
    if(!ValidateCoJEquipmentFrame(result)){out={};return false;}
    out=result;return true;
}

inline bool InvertCoJEquipmentFrame(const CoJEquipmentFrame& source,CoJEquipmentFrame& out) noexcept {
    // Work in a copy so input/output aliases are safe, including rejection.
    const auto original=source;
    if(!ValidateCoJEquipmentFrame(original)){out={};return false;}
    double rows[4][8]{};
    for(std::size_t i=0;i<4;++i){
        for(std::size_t j=0;j<4;++j)rows[i][j]=original.values[i*4+j];
        rows[i][i+4]=1;
    }
    for(std::size_t j=0;j<4;++j){
        std::size_t pivot=j;
        for(std::size_t i=j+1;i<4;++i)if(std::abs(rows[i][j])>std::abs(rows[pivot][j]))pivot=i;
        if(std::abs(rows[pivot][j])<1e-12){out={};return false;}
        for(std::size_t k=0;k<8;++k)std::swap(rows[j][k],rows[pivot][k]);
        const double divisor=rows[j][j];
        for(double& v:rows[j])v/=divisor;
        for(std::size_t i=0;i<4;++i)if(i!=j){
            const double scale=rows[i][j];
            for(std::size_t k=0;k<8;++k)rows[i][k]-=scale*rows[j][k];
        }
    }
    CoJEquipmentFrame result;
    for(std::size_t i=0;i<4;++i)for(std::size_t j=0;j<4;++j)result.values[i*4+j]=rows[i][j+4];
    if(!ValidateCoJEquipmentFrame(result) ||
       !equipment_frame_detail::IdentityWithin(equipment_frame_detail::Multiply(original,result)) ||
       !equipment_frame_detail::IdentityWithin(equipment_frame_detail::Multiply(result,original))){out={};return false;}
    out=result;return true;
}

// Preserve the CURRENT animated element relative to its object. Bind matrices
// are not substituted for the current animation. Reverse source/target to undo
// the value mapping; this function does not mutate/restore a native object.
inline bool RemapCoJEquipmentElement(const CoJEquipmentFrame& source_root,
    const CoJEquipmentFrame& target_root,const CoJEquipmentFrame& element,CoJEquipmentFrame& out) noexcept {
    CoJEquipmentFrame inverse,relative,result;
    if(!InvertCoJEquipmentFrame(source_root,inverse) ||
       !ComposeCoJEquipmentFrames(element,inverse,relative) ||
       !ComposeCoJEquipmentFrames(relative,target_root,result)){out={};return false;}
    out=result;return true;
}
} // namespace cojvr::games::call_of_juarez
