#include "backends/openvr/scene_depth_transfer.hpp"
#include <algorithm>

namespace cojvr::backends::openvr::dlss {
struct SceneDepthTransfer::Impl {
    struct Held {
        std::array<d3d9::D3D9SampledDepthSnapshot,2> snapshots;
        std::array<std::shared_ptr<d3d9::ProducerFrameReleaseState>,2> states;
        SceneDepthStamp stamp{};
        std::shared_ptr<std::atomic_bool> copies_accepted;
        bool invalidated=false;
        bool Retired() const noexcept {
            return states[0]->consumer_done.load(std::memory_order_acquire)&&
                states[1]->consumer_done.load(std::memory_order_acquire);
        }
    };
    DWORD thread=GetCurrentThreadId();
    SceneDepthStamp previous{};
    std::uint64_t previous_sequence=0;
    std::unique_ptr<Held> held;
};
SceneDepthTransfer::SceneDepthTransfer():impl_(std::make_unique<Impl>()) {}
SceneDepthTransfer::~SceneDepthTransfer() {
    if(impl_->held&&(!impl_->held->Retired()||GetCurrentThreadId()!=impl_->thread))
        (void)impl_->held.release();
}
bool SceneDepthTransfer::Queue(const std::array<SceneDepthEye,2>& eyes,SceneDepthWork& work) noexcept {
    if(GetCurrentThreadId()!=impl_->thread||impl_->held||work.color||work.depth||
        work.stamp!=SceneDepthStamp{})return false;
    const auto stamp=eyes[0].stamp;
    if(!stamp.device||!stamp.generation||!stamp.frame||!stamp.pose||!stamp.owner||
        !stamp.integrity||!stamp.epoch||eyes[1].stamp!=stamp)return false;
    const auto& previous=impl_->previous;
    if(previous.device) {
        if(stamp.generation<previous.generation||stamp.epoch<previous.epoch)return false;
        if((stamp.device!=previous.device||stamp.generation!=previous.generation)&&
            stamp.epoch<=previous.epoch)return false;
        if(stamp.generation==previous.generation && (stamp.device!=previous.device||
            stamp.frame<=previous.frame||stamp.pose<previous.pose))return false;
    }
    std::array<HANDLE,4> handles{};
    for(unsigned eye=0;eye<2;++eye) {
        const auto& source=eyes[eye].snapshot;
        if(eyes[eye].eye!=eye||!source.valid()||source.producer_device()!=stamp.device||
            source.producer_thread()!=impl_->thread||
            source.generation()!=stamp.generation||!source.sequence()||!source.width()||!source.height()||
            source.width()!=eyes[0].snapshot.width()||source.height()!=eyes[0].snapshot.height())return false;
        if(stamp.device==previous.device&&stamp.generation==previous.generation&&
            source.sequence()<=impl_->previous_sequence)return false;
        handles[eye]=source.color_shared_handle();handles[eye+2]=source.shared_handle();
    }
    if(eyes[0].snapshot.sequence()==eyes[1].snapshot.sequence())return false;
    for(unsigned i=0;i<4;++i) {
        if(!handles[i])return false;
        for(unsigned j=0;j<i;++j)if(handles[i]==handles[j])return false;
    }
    try {
        auto held=std::make_unique<Impl::Held>();
        SceneDepthWork candidate;
        candidate.stamp=stamp;held->stamp=stamp;
        if(eyes[0].capture_time!=std::chrono::steady_clock::time_point{}&&
            eyes[1].capture_time!=std::chrono::steady_clock::time_point{})
            candidate.capture_time=std::min(eyes[0].capture_time,eyes[1].capture_time);
        candidate.copies_accepted_=held->copies_accepted=std::make_shared<std::atomic_bool>(false);
        for(unsigned plane=0;plane<2;++plane) {
            auto frame=std::make_unique<d3d9::StereoCpuFrame>();
            frame->device_id=stamp.device;frame->generation=stamp.epoch;
            frame->capture_sequence=stamp.frame;frame->render_pose_sequence=stamp.pose;
            frame->transport=d3d9::StereoFrameTransport::d3d9ex_shared_texture;
            held->states[plane]=std::make_shared<d3d9::ProducerFrameReleaseState>();
            frame->producer_lease=d3d9::ProducerFrameLease(held->states[plane]);
            for(unsigned eye=0;eye<2;++eye)
                frame->shared_eyes[eye]={reinterpret_cast<std::uintptr_t>(handles[2*plane+eye]),
                    eyes[eye].snapshot.width(),eyes[eye].snapshot.height(),d3d9::D3D9SampledDepthSnapshot::format};
            (plane?candidate.depth:candidate.color)=std::move(frame);
        }
        for(unsigned eye=0;eye<2;++eye) {
            held->snapshots[eye]=eyes[eye].snapshot;
            candidate.input_tokens[eye]=eyes[eye].snapshot.sequence();
        }
        // Both eyes share the producer's sequence domain. A previous right eye
        // must not become a later frame's left eye under newly supplied stamps.
        impl_->previous_sequence=std::max(eyes[0].snapshot.sequence(),eyes[1].snapshot.sequence());
        impl_->previous=stamp;impl_->held=std::move(held);work=std::move(candidate);
        return true;
    } catch(...) {return false;}
}
void SceneDepthTransfer::Invalidate() noexcept {
    if(GetCurrentThreadId()==impl_->thread&&impl_->held)impl_->held->invalidated=true;
}
SceneDepthDelivery SceneDepthTransfer::Poll(const SceneDepthStamp& current,SceneDepthStamp& completed) noexcept {
    if(GetCurrentThreadId()!=impl_->thread||!impl_->held)return SceneDepthDelivery::idle;
    if(!impl_->held->Retired())return SceneDepthDelivery::pending;
    const bool accepted=!impl_->held->invalidated&&impl_->held->stamp==current&&
        impl_->held->copies_accepted->load(std::memory_order_acquire);
    if(accepted)completed=impl_->held->stamp;
    impl_->held.reset(); // Both producer snapshots release on their owner thread.
    return accepted?SceneDepthDelivery::ready:SceneDepthDelivery::discarded;
}
} // namespace cojvr::backends::openvr::dlss
