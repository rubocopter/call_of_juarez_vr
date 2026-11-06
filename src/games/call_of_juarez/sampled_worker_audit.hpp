#pragma once
#include "backends/openvr/scene_depth_worker.hpp"
#include <optional>
#include <algorithm>

namespace cojvr::games::call_of_juarez {
struct CoJSampledWorkerAuditReport {
    backends::openvr::dlss::SceneWorkerReport worker;
    bool pixels_checked=false,pixels_match=false;
};
// Developer-only observer. It never admits reconstruction or publishes pixels
// to the visor. The exact scene adapter supplies packets; current native UI and
// passive XR input authority are supplied by the game boundary on every Present.
class CoJSampledWorkerAudit final {
    using Eye=backends::openvr::dlss::SceneDepthEye;
    using Stamp=backends::openvr::dlss::SceneDepthStamp;
    using Worker=backends::openvr::dlss::SceneDepthWorker;
    using Authority=backends::openvr::dlss::SceneWorkerAuthority;
    std::unique_ptr<Worker> worker_;
    std::array<Eye,2> partial_{};
    std::array<backends::d3d9::D3D9SampledDepthPoint,2> points_{},expected_{};
    Stamp current_{},expected_source_{};
    Authority authority_{};
    std::uint64_t wanted_frame_=0;
    DWORD owner_=0;
    bool disabled_=false;
    const char* error_="none";
    bool Owner()const noexcept{return owner_&&owner_==GetCurrentThreadId();}
    void Clear()noexcept{partial_={};points_={};}
public:
    enum class OfferResult {pending,queued,skipped,failed};
    [[nodiscard]] bool Start(IDirect3DDevice9* device)noexcept{
        if(disabled_||!device||(owner_&&!Owner()))return false;
        if(worker_)return true;
        try{owner_=GetCurrentThreadId();worker_=std::make_unique<Worker>();
            if(worker_->Start(device))return true;
        }catch(...){}
        disabled_=true;error_="worker_start_failed";return false;
    }
    void BeginFrame(std::uint64_t frame)noexcept{
        if(!Owner()||!frame)return;
        if(frame!=wanted_frame_)Clear();wanted_frame_=frame;
    }
    [[nodiscard]] OfferResult Offer(const Eye& packet)noexcept{
        if(!Owner()||!worker_||disabled_)return OfferResult::skipped;
        if(packet.stamp.frame!=wanted_frame_){error_="stale_source_frame";return OfferResult::skipped;}
        if(!authority_.available||!authority_.tracking_valid||!authority_.settings_revision||
            !authority_.input_context||!authority_.history_epoch){
            Clear();error_="authority_unavailable";return OfferResult::skipped;
        }
        if(worker_->status().phase!=backends::openvr::dlss::SceneWorkerPhase::idle){
            Clear();error_="worker_busy_or_starting";return OfferResult::skipped;
        }
        if(packet.eye>1||!packet.snapshot.valid()){Clear();error_="invalid_source_packet";return OfferResult::failed;}
        const auto other=1-packet.eye;
        if(partial_[packet.eye].snapshot.valid()||
            (partial_[other].snapshot.valid()&&partial_[other].stamp!=packet.stamp)){
            Clear();error_="source_pair_identity";return OfferResult::failed;
        }
        backends::d3d9::D3D9SampledDepthPoint point;
        if(FAILED(packet.snapshot.ReadDiagnosticPoint(point))){
            Clear();error_="point_readback_failed";return OfferResult::failed;
        }
        partial_[packet.eye]=packet;points_[packet.eye]=point;
        if(!partial_[0].snapshot.valid()||!partial_[1].snapshot.valid())return OfferResult::pending;
        current_=packet.stamp;
        backends::openvr::dlss::SceneWorkerRequest request;
        request.authority=authority_;request.capture_time=std::min(partial_[0].capture_time,partial_[1].capture_time);
        request.max_age=std::chrono::seconds(2); // Diagnostic expiry only; no visor image is delivered.
        const bool queued=worker_->Submit(partial_,std::move(request));
        if(queued){expected_source_=current_;expected_=points_;error_="none";}
        else error_="worker_not_available";
        Clear();return queued?OfferResult::queued:OfferResult::skipped;
    }
    [[nodiscard]] std::optional<CoJSampledWorkerAuditReport> Poll(Authority authority)noexcept{
        if(!Owner()||!worker_)return {};
        authority_=authority;(void)worker_->SetAuthority(authority);
        if(!authority.available||!authority.tracking_valid||!authority.settings_revision||
            !authority.input_context||!authority.history_epoch)Clear();
        if(worker_->status().phase==backends::openvr::dlss::SceneWorkerPhase::faulted){
            disabled_=true;Clear();error_="worker_faulted";
        }
        backends::openvr::dlss::SceneWorkerReport report;
        if(!worker_->Poll(current_,report))return {};
        CoJSampledWorkerAuditReport result;result.worker=report;
        result.pixels_checked=report.outcome==backends::openvr::dlss::SceneWorkerOutcome::copied&&
            report.source==expected_source_;
        if(result.pixels_checked){result.pixels_match=true;
            for(unsigned eye=0;eye<2;++eye)result.pixels_match=result.pixels_match&&
                report.center_rgba16f[eye]==expected_[eye].color&&report.center_depth_rgba16f[eye]==expected_[eye].depth;
        }
        if(report.outcome==backends::openvr::dlss::SceneWorkerOutcome::copied&&!result.pixels_match){
            disabled_=true;error_="worker_pixels_mismatch";
        }
        return result;
    }
    [[nodiscard]] backends::openvr::dlss::SceneWorkerStatus status()const noexcept{
        return worker_?worker_->status():backends::openvr::dlss::SceneWorkerStatus{};
    }
    [[nodiscard]] const char* last_error()const noexcept{return error_;}
    [[nodiscard]] bool Retire()noexcept{
        if(!owner_)return true;if(!Owner())return false;
        Clear();disabled_=true;return !worker_||worker_->Stop();
    }
};
} // namespace cojvr::games::call_of_juarez
