#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace cojvr::games::call_of_juarez {
struct CoJRenderMemory {
    using Read=bool(*)(void*,std::uint32_t,void*,std::size_t) noexcept;
    using Write=bool(*)(void*,std::uint32_t,const void*,std::size_t) noexcept;
    Read read=nullptr;Write write=nullptr;void* context=nullptr;
};
// Exact adapter supplies already-qualified fields, never native ownership/list
// structures. Keep this object alive while restoration_pending() is true.
class CoJNativeRenderTransaction {
public:
    static constexpr std::size_t capacity=192,field_bytes=80;
    explicit CoJNativeRenderTransaction(CoJRenderMemory io) noexcept:io_(io){}
    ~CoJNativeRenderTransaction(){(void)Restore();}
    CoJNativeRenderTransaction(const CoJNativeRenderTransaction&)=delete;
    CoJNativeRenderTransaction& operator=(const CoJNativeRenderTransaction&)=delete;
    bool Clear() noexcept {
        if(touched_||applied_)return false;
        count_=0;valid_=true;return true;
    }
    bool Snapshot(std::uint32_t address,std::size_t size) noexcept {
        std::array<unsigned char,field_bytes> bytes{};
        if(!io_.read||size>field_bytes||!size||
           !io_.read(io_.context,address,bytes.data(),size)){valid_=false;return false;}
        if(!Stage(address,bytes.data(),size))return false;
        patches_[count_-1].snapshot=true;return true;
    }
    bool Stage(std::uint32_t address,const void* data,std::size_t size) noexcept {
        if(touched_||applied_||!valid_||!io_.read||!io_.write||!data||!address||
            address==0xffffffffu||!size||size>field_bytes||
            size-1>=std::uint64_t(0xffffffffu)-address||count_==capacity){valid_=false;return false;}
        for(std::size_t i=0;i<count_;++i) {
            const auto& p=patches_[i];
            if(std::uint64_t(address)<std::uint64_t(p.address)+p.size&&
                std::uint64_t(p.address)<std::uint64_t(address)+size){valid_=false;return false;}
        }
        auto& p=patches_[count_];p.address=address;p.size=size;p.snapshot=false;
        if(!io_.read(io_.context,address,p.before.data(),size)){valid_=false;return false;}
        std::memcpy(p.after.data(),data,size);++count_;return true;
    }
    bool Apply() noexcept {
        if(!valid_||applied_||touched_||!count_)return false;
        // Validate the entire preimage before the first write.
        for(std::size_t i=0;i<count_;++i)
            if(!Matches(patches_[i],false))return false;
        for(std::size_t i=0;i<count_;++i) {
            touched_=i+1;const auto& p=patches_[i];
            if(p.snapshot)continue;
            if(!io_.write(io_.context,p.address,p.after.data(),p.size)||!Matches(p,true)) {
                (void)Restore();return false;
            }
        }
        applied_=true;return true;
    }
    bool Restore() noexcept {
        if(!touched_){applied_=false;return true;}
        bool ok=true;
        for(std::size_t i=touched_;i>0;--i) {
            const auto& p=patches_[i-1];
            const bool written=io_.write(io_.context,p.address,p.before.data(),p.size);
            const bool matched=Matches(p,false);ok=written&&matched&&ok;
        }
        if(ok){touched_=0;applied_=false;}
        return ok;
    }
    bool restoration_pending() const noexcept{return touched_!=0;}
private:
    struct Patch{std::uint32_t address=0;std::size_t size=0;bool snapshot=false;
        std::array<unsigned char,field_bytes> before{},after{};};
    bool Matches(const Patch& p,bool after) noexcept {
        std::array<unsigned char,field_bytes> bytes{};
        return io_.read(io_.context,p.address,bytes.data(),p.size)&&
            std::memcmp(bytes.data(),after?p.after.data():p.before.data(),p.size)==0;
    }
    CoJRenderMemory io_;std::array<Patch,capacity> patches_{};
    std::size_t count_=0,touched_=0;bool valid_=true,applied_=false;
};
}
