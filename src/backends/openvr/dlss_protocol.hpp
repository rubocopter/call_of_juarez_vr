#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace cojvr::backends::openvr::dlss {
// Original wire protocol. Fixed-width POD only: no pointer, size_t, native bool,
// COM object or compiler-dependent enum crosses the x86/x64 boundary.
constexpr std::uint32_t magic=0x43564E47,version=1;
enum class Operation:std::uint32_t {hello=1,build=2,evaluate=3,quit=4};
enum class Quality:std::uint32_t {dlaa=0,quality=1,balanced=2,performance=3};
enum class Slot:std::uint32_t {color,depth,motion,bias,output,count};
constexpr std::size_t slots=static_cast<std::size_t>(Slot::count);
#pragma pack(push,1)
struct Request {
    std::uint32_t signature=magic,protocol=version;
    Operation operation=Operation::hello;
    std::uint32_t eye=0;
    std::uint64_t sequence=0;
    std::uint32_t adapter_low=0;
    std::int32_t adapter_high=0;
    std::uint32_t width=0,height=0,out_width=0,out_height=0;
    Quality quality=Quality::dlaa;
    std::uint32_t reset=1;
    float jitter_x=0,jitter_y=0;
    std::array<std::uint64_t,slots> textures{};
    std::uint64_t fence_in=0,fence_out=0;
};
struct Response {
    std::uint32_t signature=magic,protocol=version;
    Operation operation=Operation::hello;
    std::uint32_t eye=0;
    std::uint64_t sequence=0;
    std::int32_t ok=0;
    std::uint32_t ngx_result=0,system_error=0;
    std::uint32_t process_bits=64;
};
#pragma pack(pop)
static_assert(sizeof(Request)==120&&offsetof(Request,textures)==64);
static_assert(sizeof(Response)==40);
[[nodiscard]] inline bool ValidExtent(const Request& r) noexcept {
    return r.width>=32&&r.height>=32&&r.out_width>=r.width&&r.out_height>=r.height&&
        r.out_width<=8192&&r.out_height<=8192&&
        std::uint64_t(r.out_width)*r.out_height<=32ULL*1024*1024&&
        r.quality>=Quality::dlaa&&r.quality<=Quality::performance&&
        (r.quality!=Quality::dlaa||(r.width==r.out_width&&r.height==r.out_height));
}
[[nodiscard]] inline bool ValidRequest(const Request& r) noexcept {
    if(r.signature!=magic||r.protocol!=version||r.eye>1)return false;
    if(r.operation==Operation::hello||r.operation==Operation::quit)return true;
    if(!ValidExtent(r))return false;
    if(r.operation==Operation::build) {
        for(auto handle:r.textures)if(!handle)return false;
        return r.fence_in&&r.fence_out;
    }
    return r.operation==Operation::evaluate&&r.sequence&&r.reset<=1&&
        std::isfinite(r.jitter_x)&&std::isfinite(r.jitter_y)&&
        std::abs(r.jitter_x)<=.5F&&std::abs(r.jitter_y)<=.5F;
}
}
