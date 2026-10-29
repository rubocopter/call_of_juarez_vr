#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <type_traits>

// Standalone experimental Neural laboratory. Pair identity is independent of
// the Super Resolution protocol. No game layout or pointer crosses this ABI.
namespace cojvr::backends::openvr::neural {
inline constexpr std::uint32_t magic=0x524e4a43,version=1,max_extent=1280;
struct Identity {
    std::uint64_t frame=0,pose=0,device=0,generation=0,revision=0,epoch=0;
};
struct View {std::array<float,4> orientation{0,0,0,1};std::array<float,4> fov{-0.7F,0.7F,0.7F,-0.7F};}; // radians L/R/U/D
struct alignas(8) Header {
    std::uint32_t signature=magic,protocol=version,width=0,height=0;
    Identity input{},output{};
    std::array<View,2> views{};
    std::uint64_t request=0,completed=0;
    std::uint32_t result=0,process_bits=0;
};
static_assert(sizeof(Identity)==48 && sizeof(View)==32 && sizeof(Header)==200);
static_assert(std::is_trivially_copyable_v<Header>);
inline constexpr std::size_t eye_bytes(std::uint32_t w,std::uint32_t h) noexcept {return std::size_t(w)*h*4;}
inline constexpr std::size_t pair_capacity=std::size_t(max_extent)*max_extent*8;
inline constexpr std::size_t mapping_bytes=sizeof(Header)+2*pair_capacity;
inline std::uint8_t* input_pixels(Header* h) noexcept{return reinterpret_cast<std::uint8_t*>(h+1);}
inline std::uint8_t* output_pixels(Header* h) noexcept{return input_pixels(h)+pair_capacity;}
inline bool Same(const Identity& a,const Identity& b) noexcept {
    return a.frame==b.frame&&a.pose==b.pose&&a.device==b.device&&a.generation==b.generation&&a.revision==b.revision&&a.epoch==b.epoch;
}
inline bool Valid(const Header& h) noexcept {
    if(h.signature!=magic||h.protocol!=version||h.width<32||h.height<32||h.width>max_extent||h.height>max_extent||
       !h.request||!h.input.frame||!h.input.pose||!h.input.device||!h.input.generation||!h.input.revision||!h.input.epoch)return false;
    for(const auto& v:h.views){
        float n=0;for(float x:v.orientation){if(!std::isfinite(x))return false;n+=x*x;}
        if(std::abs(n-1)>0.01F)return false;
        for(float f:v.fov)if(!std::isfinite(f)||std::abs(f)>=1.56F)return false;
        if(v.fov[0]>=0||v.fov[1]<=0||v.fov[2]<=0||v.fov[3]>=0)return false;
    }
    return true;
}
inline bool Completed(const Header& h) noexcept {
    return Valid(h)&&h.completed==h.request&&h.result==1&&h.process_bits==64&&Same(h.input,h.output);
}
}
