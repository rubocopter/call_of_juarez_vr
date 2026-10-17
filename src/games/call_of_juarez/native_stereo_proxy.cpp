#include "games/call_of_juarez/camera_probe.hpp"
#include "runtime/native_render_fault.hpp"
#include "games/call_of_juarez/d3d9ex_legacy_resource_compat.hpp"
#include "games/call_of_juarez/game_shutdown_hook.hpp"
#include "games/call_of_juarez/java_player_bridge.hpp"
#include "games/call_of_juarez/ppfx_resource_telemetry.hpp"
#include "games/call_of_juarez/independent_render_scope.hpp"
#include "games/call_of_juarez/independent_resolution_recipe.hpp"

#include "backends/d3d9/d3d9_stereo_capture.hpp"
#include "backends/d3d9/depth_observation.hpp"
#include "backends/d3d9/device_vtable_hook.hpp"
#include "backends/d3d9/factory_vtable_hook.hpp"
#include "backends/d3d9/swapchain_vtable_hook.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "backends/openvr/stereo_presenter.hpp"
#include "runtime/log.hpp"
#include "runtime/hmd_frame_pacing.hpp"
#include "runtime/vr_settings.hpp"
#include "runtime/ui_pointer_ownership.hpp"
#include "backends/d3d9/vr_present_policy.hpp"
#include "backends/d3d9/source_capture_admission.hpp"
#include "backends/d3d9/frame_luma_probe.hpp"

#include <windows.h>
#include <psapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

std::once_flag g_start_once;
std::filesystem::path g_game_directory{};
std::string g_run_id{"unbound"};
std::atomic_bool g_finalization_started{false};
std::atomic_bool g_finalization_completed{false};
wchar_t g_process_exit_log_path[32768]{};

struct SourceCaptureAuthorization {
    std::uint64_t sequence = 0, source_epoch = 0, owner_epoch = 0;
    std::uint64_t owner = 0;
    std::uint64_t output_epoch = 0, generation = 0;
    std::uintptr_t device = 0;
};

struct NativeStereoState {
    struct LumaChannel {
        cojvr::backends::d3d9::D3D9FrameLumaProbe probe;
        cojvr::backends::d3d9::FrameLumaSample previous{};
        bool have_previous = false, disabled = false;
        std::uint64_t observations = 0, logged_time_ns = 0;
    };
    bool luma_enabled = false; // Startup-only diagnostic; never a visual writer.
    std::mutex luma_mutex;
    std::array<LumaChannel, 3> luma_channels;
    std::atomic_uint64_t luma_pose_sequence{0};
    std::atomic<std::uint64_t> frame_feedback_context_token{0};
    cojvr::backends::d3d9::D3D9StereoCapture capture;
    cojvr::backends::d3d9::D3D9StereoCapture flat_capture;
    cojvr::backends::openvr::OpenVrStereoPresenter presenter;
    // Startup-only opt-in. The deliberately leaked state also keeps this facade
    // out of DLL/static destruction; explicit retirement runs before DestroyGame.
    cojvr::backends::openvr::FixedOutputConfiguration fixed_outputs{};
    cojvr::games::call_of_juarez::CoJIndependentRenderScope independent_render;
    std::mutex source_mutex;
    std::uint64_t source_epoch = 1, source_owner = 0;
    bool source_owner_known = false, source_mode_known = false;
    cojvr::backends::d3d9::FramePresentationMode source_mode{};
    std::array<SourceCaptureAuthorization, 3> source_captures{};
    std::size_t next_source_capture = 0;
    cojvr::backends::d3d9::SourceCaptureAdmission source_capture_admission;
    std::atomic_bool independent_bank_faulted{false};
    std::atomic_bool natural_world_seen{false},natural_frame_ready{true};
    std::atomic_uint64_t natural_owner{0},natural_generation{0},reset_suspended_generation{0};
    std::atomic_bool independent_admission_blocked{true};
    std::uint64_t independent_frame = 0, independent_generation = 0;
    std::uint64_t independent_source_epoch = 0, independent_history_epoch = 0;
    cojvr::runtime::Eye independent_eye = cojvr::runtime::Eye::left;
    cojvr::runtime::HmdFramePacer frame_pacer;
    cojvr::runtime::VrFrameCadence frame_cadence;
    std::atomic_bool desktop_vsync_disabled{false};
    cojvr::backends::d3d9::VrPresentPolicy present_policy;
    bool late_rate_reported = false;
    float last_pacing_hz = 0.0F;
    std::uint64_t pacing_sequence = 0;
    std::array<cojvr::runtime::EyeView, 2> eyes{};
    std::mutex device_mutex;
    IDirect3D9* factory = nullptr;
    IDirect3DDevice9* device = nullptr;
    std::atomic_uint64_t device_generation{0};
    std::atomic<DWORD> native_render_thread{0};
    std::atomic_uint64_t present_telemetry_sequence{0};
    std::atomic_uint64_t capture_resources_generation_logged{0};
    std::atomic_uint64_t transport_sequence{0};
    std::atomic_uint64_t flat_capture_sequence{0};
    cojvr::backends::d3d9::NativeStereoQueuePolicy stereo_queue;
    std::atomic_uint64_t frame_owner_epoch{0};
    std::atomic_bool frame_settings_captured{false};
    std::atomic_bool flat_capture_active{false};
    std::atomic_uint64_t last_error_frame{0};
    std::atomic_uintptr_t game_window{0};
    std::mutex present_hook_worker_mutex;
    std::jthread present_hook_worker;
    std::atomic_bool present_hook_conflict_logged{false};
    std::mutex flat_ui_mutex;
    bool flat_ui_pointer_active = false;
    cojvr::runtime::UiPointerSelection flat_ui_pointer_selection;
    std::uint32_t flat_ui_pointer_x = 0;
    std::uint32_t flat_ui_pointer_y = 0;
    std::uint32_t flat_ui_source_width = 0, flat_ui_source_height = 0;
    std::uint64_t flat_ui_source_generation = 0;
    bool flat_ui_pointer_game_route_logged = false;
    bool flat_ui_pointer_game_error_logged = false;
    std::uint64_t flat_ui_pointer_delivery_sequence = 0;
    cojvr::runtime::UiPointerMousePriority flat_ui_mouse_priority;
    std::atomic_uint64_t flat_ui_mouse_override_until_ms{0};
    cojvr::runtime::UiPointerSelectionSnapshot flat_ui_applied_selection{};
    std::uint32_t flat_ui_applied_x = 0, flat_ui_applied_y = 0;
    bool flat_ui_target_applied = false;
    std::int32_t flat_ui_menu_index = 0;
    bool flat_ui_menu_known = false;
    bool flat_loading_resume_pending = false;
    bool flat_ui_select_release_required = false;
    cojvr::games::call_of_juarez::CoJUiSelectRetryState flat_ui_select_retry{};
    std::array<bool, 2> capture_source_logged{};
    std::atomic_bool capture_readback_enabled{true};
    bool runtime_ready = false;
};

// Finalize explicitly drains owners before DestroyGame. If abnormal exit bypasses
// that boundary, Windows owns the remaining allocations; DLL static destruction
// must not join killed threads or release GPU resources under the loader lock.
NativeStereoState& g_stereo = *new NativeStereoState;

std::filesystem::path GameDirectory() noexcept {
    try {
        if (!g_game_directory.empty()) return g_game_directory;
        std::vector<wchar_t> buffer(32768);
        const DWORD length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size()) return {};
        g_game_directory = std::filesystem::path(
            std::wstring_view(buffer.data(), length)).parent_path();
        return g_game_directory;
    } catch (...) {
        return {};
    }
}

std::filesystem::path LogPath() noexcept {
    const auto directory = GameDirectory();
    return directory.empty() ? std::filesystem::path{} : directory / L"cojvr.log";
}

void LogLine(const std::string_view line) noexcept {
    const auto path = LogPath();
    if (!path.empty()) cojvr::runtime::AppendLogLine(path, line);
}

void ResetFrameLumaProbes() noexcept {
    std::lock_guard lock(g_stereo.luma_mutex);
    for (auto& channel : g_stereo.luma_channels) {
        channel.probe.Reset();
        channel.have_previous = false;
        channel.logged_time_ns = 0;
    }
    g_stereo.luma_pose_sequence.store(0);
    if (g_stereo.luma_enabled) LogLine("native_stereo_luma_probe: status=reset;pending_samples=discarded");
}

void LogLumaSample(std::size_t stream, const cojvr::backends::d3d9::FrameLumaSample& sample) {
    static constexpr const char* names[]{"left", "right", "desktop"};
    std::ostringstream line;
    line << "native_stereo_luma_probe: status=sample;stream=" << names[stream]
         << ";sequence=" << sample.sequence << ";generation=" << sample.generation
         << ";pose_sequence=" << sample.pose_sequence << ";time_ns=" << sample.timestamp_ns
         << ";device=" << sample.device << ";source=" << sample.source
         << ";source_width=" << sample.source_width << ";source_height=" << sample.source_height
         << ";format=" << static_cast<unsigned>(sample.source_format)
         << std::fixed << std::setprecision(4)
         << ";mean=" << (sample.mean_r + sample.mean_g + sample.mean_b) / 3.0
         << ";mean_r=" << sample.mean_r << ";mean_g=" << sample.mean_g << ";mean_b=" << sample.mean_b
         << ";white_fraction=" << sample.white_fraction
         << ";queue_ms=" << sample.queue_ms << ";readback_ms=" << sample.readback_ms;
    LogLine(line.str());
}

void PollFrameLumaProbes() noexcept {
    if (!g_stereo.luma_enabled) return;
    try {
        std::lock_guard lock(g_stereo.luma_mutex);
        for (std::size_t stream = 0; stream < g_stereo.luma_channels.size(); ++stream) {
            auto& channel = g_stereo.luma_channels[stream];
            if (channel.disabled) continue;
            cojvr::backends::d3d9::FrameLumaSample sample{};
            // One non-flushing query poll per channel; never spin for completion.
            if (!channel.probe.Poll(sample)) {
                if (FAILED(channel.probe.last_result)) {
                    channel.disabled = true;
                    LogLine("native_stereo_luma_probe: status=disabled;reason=readback_failed;stream=" + std::to_string(stream));
                    channel.probe.Reset();
                }
                continue;
            }
            ++channel.observations;
            const bool same = channel.have_previous && sample.generation == channel.previous.generation &&
                sample.device == channel.previous.device && sample.source == channel.previous.source &&
                sample.source_width == channel.previous.source_width && sample.source_height == channel.previous.source_height;
            const auto mean = [](const auto& value) { return (value.mean_r+value.mean_g+value.mean_b)/3.0; };
            const bool changed = same && (std::abs(mean(sample)-mean(channel.previous)) >= 24.0 ||
                (sample.white_fraction >= .75) != (channel.previous.white_fraction >= .75));
            if (changed && channel.previous.timestamp_ns > channel.logged_time_ns) {
                LogLumaSample(stream, channel.previous);
                channel.logged_time_ns = channel.previous.timestamp_ns;
            }
            if (!same || changed || channel.observations % 90 == 0) {
                LogLumaSample(stream, sample);
                channel.logged_time_ns = sample.timestamp_ns;
            }
            channel.previous = sample;
            channel.have_previous = true;
        }
    } catch (...) {} // Diagnostic allocation/logging cannot alter rendering.
}

void QueueFrameLumaProbe(IDirect3DDevice9* device, IDirect3DSurface9* source,
                         std::size_t stream, std::uint64_t sequence) noexcept {
    if (!g_stereo.luma_enabled || !device || !source || !sequence ||
        g_finalization_started.load(std::memory_order_acquire)) return;
    try {
        std::lock_guard lock(g_stereo.luma_mutex);
        auto& channel = g_stereo.luma_channels[stream];
        if (channel.disabled) return;
        if (!channel.probe.Queue(device, source, sequence, g_stereo.device_generation.load(),
                g_stereo.luma_pose_sequence.load()) && FAILED(channel.probe.last_result)) {
            // Unavailable diagnostic is never a capture/gameplay failure or retry loop.
            channel.disabled = true;
            std::ostringstream line;
            line << "native_stereo_luma_probe: status=disabled;stream=" << stream
                 << ";hr=" << channel.probe.last_result;
            LogLine(line.str());
            channel.probe.Reset();
        }
    } catch (...) {}
}

// The source fence covers both producers. In particular, a blocked owner that
// remains blocked must not invalidate every natural Present and starve theater.
void InvalidateSourceLocked(NativeStereoState& state) noexcept {
    state.source_capture_admission.Invalidate();
    ++state.source_epoch;
    state.source_captures = {};
    state.presenter.InvalidateFixedOutputFrameAuthority();
}

void InvalidateSourceAuthority(NativeStereoState& state) noexcept {
    state.source_capture_admission.Invalidate();
    state.stereo_queue.Invalidate();
    if (!state.fixed_outputs.enabled) return;
    std::lock_guard lock(state.source_mutex);
    InvalidateSourceLocked(state);
}

void ObserveSourceOwner(NativeStereoState& state, std::uint64_t owner) noexcept {
    if (!state.fixed_outputs.enabled) return;
    std::lock_guard lock(state.source_mutex);
    if (state.source_owner_known && state.source_owner != owner)
        InvalidateSourceLocked(state);
    state.source_owner = owner;
    state.source_owner_known = true;
}

SourceCaptureAuthorization AuthorizeSourceCapture(NativeStereoState& state,
    std::uint64_t sequence, std::uint64_t owner_epoch, std::uint64_t generation,
    IDirect3DDevice9* device,
    cojvr::backends::d3d9::FramePresentationMode mode) noexcept {
    if (!state.fixed_outputs.enabled) return {};
    std::lock_guard lock(state.source_mutex);
    if (state.source_mode_known && state.source_mode != mode)
        InvalidateSourceLocked(state);
    state.source_mode = mode;
    state.source_mode_known = true;
    // This is the ORIGINAL presenter token, before CaptureEye/CaptureFlatFrame.
    const SourceCaptureAuthorization authorization{sequence, state.source_epoch,
        owner_epoch, state.source_owner, state.presenter.FixedOutputFrameAuthorityEpoch(), generation,
        reinterpret_cast<std::uintptr_t>(device)};
    if (mode == cojvr::backends::d3d9::FramePresentationMode::native_stereo)
        state.source_captures[state.next_source_capture++ % state.source_captures.size()] = authorization;
    return authorization;
}

SourceCaptureAuthorization FindSourceCapture(NativeStereoState& state,
    std::uint64_t sequence) noexcept {
    std::lock_guard lock(state.source_mutex);
    for (const auto& capture : state.source_captures)
        if (sequence && capture.sequence == sequence) return capture;
    return {};
}

bool SourceCaptureCurrent(NativeStereoState& state,
    const SourceCaptureAuthorization& capture) noexcept {
    std::lock_guard lock(state.source_mutex);
    return capture.sequence && capture.source_epoch == state.source_epoch &&
        capture.owner == state.source_owner &&
        capture.generation == state.device_generation.load(std::memory_order_acquire);
}

void IndependentBankFault(NativeStereoState& state, const char* stage) noexcept {
    InvalidateSourceAuthority(state);
    state.presenter.PublishGameplayFeedbackContext(false);
    // Allocation/unsupported-family faults latch until an explicit owner-thread
    // reset/retirement. Never retry a failed bank allocation on every eye/frame.
    if (state.independent_bank_faulted.exchange(true, std::memory_order_acq_rel)) return;
    try {
        LogLine(std::string("independent_eye_bank: status=failed stage=") + stage +
            " error=" + std::string(state.independent_render.last_error()));
    } catch (...) {}
}

bool RetireIndependentBank(NativeStereoState& state, const char* boundary) noexcept {
    if (!state.fixed_outputs.enabled) return true;
    InvalidateSourceAuthority(state);
    // The adapter owns restoration and release checks, including thread identity.
    // Always attempt retirement before forwarding Reset, even a failed Reset.
    if(state.independent_render.restoration_pending())(void)state.independent_render.End();
    const bool captures_retired=state.capture.TryInvalidateResources();
    const bool retired=captures_retired&&state.independent_render.InvalidateBeforeReset();
    if(!retired)state.independent_bank_faulted.store(true,std::memory_order_release);
    state.independent_history_epoch = 0;
    if (retired) state.independent_frame = 0;
    if (retired) state.capture.InvalidateResources();
    if (!retired) {
        try {
            LogLine(std::string("independent_eye_bank: status=retirement_failed boundary=") +
                boundary + " error=" + std::string(state.independent_render.last_error()));
        } catch (...) {}
    }
    return retired;
}

void CancelResourceProbe() noexcept;
bool AllowDeviceReset(IDirect3DDevice9*) noexcept {
    ResetFrameLumaProbes(); // Called before both Reset and ResetEx, even failed attempts.
    if (!g_stereo.fixed_outputs.enabled) return true;
    CancelResourceProbe();
    g_stereo.reset_suspended_generation.store(g_stereo.device_generation.load());
    g_stereo.natural_frame_ready.store(false);
    // Called by both Reset entry points on their caller/owner thread. Failure
    // vetoes forwarding, retaining all pending restoration references intact.
    return RetireIndependentBank(g_stereo, "Reset_or_ResetEx");
}


using ResourceProbe = cojvr::backends::d3d9::D3D9RenderResourceProbe;
using ResourceContext = cojvr::backends::d3d9::D3D9RenderResourceContext;
std::atomic_bool g_resource_probe_enabled{false};
cojvr::backends::d3d9::RenderResourceRouteBudget g_resource_probe_pairs;
std::atomic_uint64_t g_resource_probe_resets{0};
cojvr::backends::d3d9::RenderResourceCallBoundary g_resource_call_boundary;
struct ResourceProbeThread {
    std::unique_ptr<ResourceProbe> probe;
    IDirect3DDevice9* device = nullptr; // value-only; current wrapper owns the device
    std::uint64_t frame=0, generation=0;
    std::uintptr_t owner=0;
    unsigned route=0,eye=0;
    std::uint64_t integrity_epoch=0;
    bool active=false, selected=false;
};
thread_local ResourceProbeThread g_resource_probe_thread;

void WriteResourceInfo(std::ostream& out,
    const cojvr::backends::d3d9::D3D9RenderResourceInfo& r) {
    out << "{\"object\":" << r.object << ",\"parent\":" << r.parent
        << ",\"surface\":" << r.surface << ",\"width\":" << r.width
        << ",\"height\":" << r.height << ",\"depth\":" << r.depth
        << ",\"levels\":" << r.levels << ",\"format\":" << r.format
        << ",\"usage\":" << r.usage << ",\"pool\":" << r.pool
        << ",\"samples\":" << r.samples << ",\"quality\":" << r.quality
        << ",\"type\":" << r.type << ",\"bound\":" << r.bound
        << ",\"valid\":" << r.valid << '}';
}
template<class T> void WriteResourceArray(std::ostream& out, const T& values) {
    out << '['; bool first=true;
    for(const auto value:values){ if(!first)out<<',';first=false;out<<value; }
    out << ']';
}
void EmitNativePostprocessObservation(const ResourceContext& context,
    const cojvr::backends::d3d9::D3D9RenderResourceSummary* summary) noexcept {
    try {
        using namespace cojvr::games::call_of_juarez;
        const auto snapshot=ObserveInstalledCoJPostprocessResources();
        CoJPostprocessObservationContext provenance;
        provenance.scope=context.scope;provenance.thread=context.thread;
        provenance.frame=context.frame;provenance.owner=context.owner;
        provenance.device=context.device;provenance.device_generation=context.device_generation;
        provenance.probe_epoch=context.generation;provenance.eye=context.eye;
        provenance.route=context.route;provenance.begin=summary==nullptr;
        provenance.scope_complete=summary&&summary->complete;
        LogLine(FormatCoJPostprocessObservation(snapshot,provenance));
    } catch (...) { LogLine("native_ppfx_resource_probe_error: serialization_failed"); }
}
void ResourceProbeSink(void*, const ResourceContext& context,
    const cojvr::backends::d3d9::D3D9RenderResourceRecord* record,
    const cojvr::backends::d3d9::D3D9RenderResourceSummary* summary) noexcept {
    if(!record)EmitNativePostprocessObservation(context,summary);
    try {
        std::ostringstream out; out << std::boolalpha;
        out << "render_resource_probe: {\"schema\":1,\"event\":\""
            << (record?"record":summary?"end":"begin")
            << "\",\"coverage\":\"binding_state_only\",\"readOnly\":true,\"resizingAdmitted\":false"
            << ",\"scope\":" << context.scope << ",\"thread\":" << context.thread
            << ",\"frame\":" << context.frame << ",\"owner\":" << context.owner
            << ",\"device\":" << context.device << ",\"generation\":" << context.generation
            << ",\"eye\":" << context.eye << ",\"route\":" << context.route;
        if(context.device_generation)out << ",\"deviceGeneration\":" << context.device_generation;
        if(record) {
            const auto& r=*record;const auto& state=r.state;
            out << ",\"ordinal\":" << r.ordinal << ",\"repeat\":" << r.repeat
                << ",\"op\":" << static_cast<unsigned>(r.event.operation)
                << ",\"selector\":" << r.event.selector << ",\"hr\":" << static_cast<std::uint32_t>(r.event.result)
                << ",\"caller\":" << r.event.caller << ",\"source\":" << r.event.source
                << ",\"destination\":" << r.event.destination
                << ",\"state\":{\"valid\":" << state.valid
                << ",\"targetSlots\":" << state.target_slots << ",\"textureSlots\":" << state.texture_slots
                << ",\"targets\":[";
            for(unsigned i=0;i<state.target_slots;++i){if(i)out<<',';WriteResourceInfo(out,state.targets[i]);}
            out << "],\"depth\":";WriteResourceInfo(out,state.depth);
            out << ",\"textures\":[";
            for(unsigned i=0;i<state.texture_slots;++i){if(i)out<<',';WriteResourceInfo(out,state.textures[i]);}
            out << "],\"viewport\":";WriteResourceArray(out,state.viewport);
            out << ",\"viewportDepth\":";WriteResourceArray(out,state.viewport_depth);
            out << ",\"scissor\":";WriteResourceArray(out,state.scissor);
            out << ",\"scissorEnabled\":" << state.scissor_enabled;
            out << ",\"z\":";WriteResourceArray(out,state.z);
            out << ",\"vertexShader\":" << state.vertex_shader << ",\"pixelShader\":" << state.pixel_shader
                << ",\"transformsValid\":" << state.transforms_valid << ",\"transforms\":";
            WriteResourceArray(out,state.transforms);out << '}';
        }
        if(summary)out << ",\"calls\":" << summary->calls << ",\"records\":" << summary->records
            << ",\"readFailures\":" << summary->read_failures << ",\"complete\":" << summary->complete
            << ",\"truncated\":" << summary->truncated;
        out << '}'; LogLine(out.str());
    } catch (...) { LogLine("render_resource_probe_error: serialization_failed"); }
}
void ResourceProbeCall(IDirect3DDevice9* device,bool entering) noexcept {
    if(entering)g_resource_call_boundary.Enter(device);else g_resource_call_boundary.Leave();
}
void CancelResourceProbe() noexcept;
void ResourceProbeEvent(IDirect3DDevice9* device,
    const cojvr::backends::d3d9::RenderResourceEvent& event) noexcept {
    if(event.operation==cojvr::backends::d3d9::RenderResourceOperation::reset) {
        if (g_resource_probe_enabled.load(std::memory_order_acquire)) {
            CancelResourceProbe();g_resource_probe_resets.fetch_add(1,std::memory_order_acq_rel);
        }
        return;
    }
    if (g_stereo.fixed_outputs.enabled && g_stereo.independent_render.active() &&
        (g_stereo.independent_frame <= 8 || g_stereo.independent_frame % 90 == 0)) {
        using Operation = cojvr::backends::d3d9::RenderResourceOperation;
        if (event.operation == Operation::draw || event.operation == Operation::draw_indexed ||
            event.operation == Operation::draw_up || event.operation == Operation::draw_indexed_up)
            g_stereo.independent_render.RecordNativeDraw(device, event.caller);
    }
    if (!g_resource_probe_enabled.load(std::memory_order_acquire)) return;
    auto& trace=g_resource_probe_thread;
    if(trace.active&&trace.probe) (void)trace.probe->Record(device,event);
}
void CancelResourceProbe() noexcept {
    auto& trace=g_resource_probe_thread;
    (void)g_resource_call_boundary.End(trace.device);
    if(trace.active&&trace.probe)(void)trace.probe->End(trace.device,false);
    trace.active=false;trace.selected=false;
}
cojvr::backends::d3d9::ResourceProbeHookContinuity CurrentProbeContinuity(IDirect3DDevice9* expected) noexcept {
    std::lock_guard lock(g_stereo.device_mutex);
    // The scope stores values, not a COM lease. Do not dereference an old
    // device after replacement may have released its final retained reference.
    if(!expected||g_stereo.device!=expected)return {};
    return cojvr::backends::d3d9::InspectResourceProbeHookContinuity(expected);
}
void ObserveRenderPass(void*,cojvr::runtime::Eye eye,std::uint64_t frame,
    std::uintptr_t owner,bool begin,bool complete,const char* route_name) noexcept {
    if(!g_resource_probe_enabled.load(std::memory_order_acquire))return;
    const DWORD error=GetLastError();
    auto& trace=g_resource_probe_thread;
    const unsigned route=std::string_view(route_name)=="movie"?2:
        std::string_view(route_name)=="settings"?1:0;
    if(!begin) {
        if(!trace.active){SetLastError(error);return;}
        const auto integrity=CurrentProbeContinuity(trace.device);
        const auto current_generation=g_stereo.device_generation.load(std::memory_order_acquire)+
            g_resource_probe_resets.load(std::memory_order_acquire);
        const bool calls_complete=g_resource_call_boundary.End(trace.device);
        complete=complete&&trace.frame==frame&&trace.owner==owner&&trace.route==route&&
            trace.eye==(eye==cojvr::runtime::Eye::left?0u:1u)&&trace.generation==current_generation&&
            integrity.intact&&integrity.epoch==trace.integrity_epoch&&
            calls_complete;
        if(trace.active&&trace.probe)(void)trace.probe->End(trace.device,complete);
        trace.active=false;
        SetLastError(error);return;
    }
    try {
        if(trace.active)CancelResourceProbe();
        IDirect3DDevice9* device=nullptr;
        {std::lock_guard lock(g_stereo.device_mutex);device=g_stereo.device;}
        const auto device_generation=g_stereo.device_generation.load(std::memory_order_acquire);
        const auto generation=device_generation+
            g_resource_probe_resets.load(std::memory_order_acquire);
        if(eye==cojvr::runtime::Eye::left) {
            const bool due=frame>3&&(!trace.frame||trace.owner!=owner||trace.route!=route||
                trace.generation!=generation||frame>trace.frame+900);
            trace.selected=false;
            if(due&&device&&owner) {
                if(g_resource_probe_pairs.TryAcquire(route)) {
                    trace.selected=true;trace.frame=frame;trace.owner=owner;trace.device=device;
                    trace.generation=generation;trace.route=route;
                }
            }
        }
        if(trace.selected&&trace.frame==frame&&trace.owner==owner&&trace.device==device&&
            trace.generation==generation&&trace.route==route) {
            const auto integrity=CurrentProbeContinuity(device);
            if(!integrity.intact||!g_resource_call_boundary.Begin(device)) {
                trace.selected=false;SetLastError(error);return;
            }
            trace.integrity_epoch=integrity.epoch;
            if(!trace.probe)trace.probe=std::make_unique<ResourceProbe>(&ResourceProbeSink);
            ResourceContext context{};context.device=reinterpret_cast<std::uintptr_t>(device);
            context.device_generation=device_generation;
            context.generation=generation;context.frame=frame;context.owner=owner;
            context.eye=eye==cojvr::runtime::Eye::left?0:1;context.route=route;
            trace.eye=context.eye;
            trace.active=trace.probe->Begin(device,context);
            if(!trace.active)CancelResourceProbe();
        }
    } catch (...) { CancelResourceProbe(); }
    SetLastError(error);
}

std::uintptr_t ModuleRva(const std::uintptr_t address) noexcept {
    if (address == 0) return 0;
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address), &module) ||
        !module) {
        return 0;
    }
    return address - reinterpret_cast<std::uintptr_t>(module);
}

const char* LegacyTextureKindName(
    const cojvr::games::call_of_juarez::LegacyTextureKind kind) noexcept {
    using Kind = cojvr::games::call_of_juarez::LegacyTextureKind;
    switch (kind) {
    case Kind::texture_2d: return "texture2d";
    case Kind::volume_texture: return "volume_texture";
    case Kind::cube_texture: return "cube_texture";
    }
    return "unknown";
}

// Sample only at bounded allocation boundaries. This reports address capacity
// separately from physical memory; it never frees, retries or changes a resource.
void LogAddressSpace(const std::uint64_t sequence, const bool allocation_failed) noexcept {
    try {
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        const std::uint64_t limit = reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress) + 1ULL;
        std::uint64_t free_bytes = 0, largest_free = 0, used_bytes = 0;
        std::uint64_t cursor = reinterpret_cast<std::uintptr_t>(info.lpMinimumApplicationAddress);
        bool complete = true;
        while (cursor < limit) {
            MEMORY_BASIC_INFORMATION region{};
            if (!VirtualQuery(reinterpret_cast<void*>(static_cast<std::uintptr_t>(cursor)), &region, sizeof(region)) ||
                region.RegionSize == 0) { complete = false; break; }
            const auto begin = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(region.BaseAddress));
            const auto end = std::min(limit, begin + region.RegionSize);
            if (end <= cursor) { complete = false; break; }
            const auto bytes = end - cursor;
            if (region.State == MEM_FREE) { free_bytes += bytes; largest_free = std::max(largest_free, bytes); }
            else used_bytes += bytes;
            cursor = end;
        }
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        const bool counters_valid = K32GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)) != FALSE;
        std::ostringstream line;
        line << "native_stereo_address_space: sequence=" << sequence
             << ";thread_id=" << GetCurrentThreadId() << ";allocation_failed=" << allocation_failed
             << ";map_complete=" << complete << ";user_address_limit=" << limit
             << ";used_va_bytes=" << used_bytes << ";free_va_bytes=" << free_bytes
             << ";largest_free_region_bytes=" << largest_free << ";counters_valid=" << counters_valid
             << ";private_commit_bytes=" << counters.PrivateUsage
             << ";peak_private_commit_bytes=" << counters.PeakPagefileUsage
             << ";working_set_bytes=" << counters.WorkingSetSize;
        LogLine(line.str());
    } catch (...) {}
}

void LegacyTextureEvent(
    void*,
    const cojvr::games::call_of_juarez::LegacyTextureCreateEvent& event) noexcept {
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=legacy_texture_create"
             << ";phase="
             << (event.phase ==
                         cojvr::games::call_of_juarez::LegacyTextureCreatePhase::enter
                     ? "enter"
                     : "result")
             << ";sequence=" << event.sequence
             << ";thread_id=" << event.thread_id
             << ";device=0x" << std::hex << event.device
             << ";caller=0x" << event.caller
             << ";caller_rva=0x" << ModuleRva(event.caller)
             << std::dec
             << ";kind=" << LegacyTextureKindName(event.kind)
             << ";width=" << event.width
             << ";height=" << event.height
             << ";depth=" << event.depth
             << ";levels=" << event.levels
             << ";requested_usage=0x" << std::hex << event.requested_usage
             << ";effective_usage=0x" << event.effective_usage
             << std::dec
             << ";format=" << static_cast<unsigned>(event.format)
             << ";requested_pool=" << static_cast<unsigned>(event.requested_pool)
             << ";effective_pool=" << static_cast<unsigned>(event.effective_pool)
             << ";translated=" << (event.translated ? "true" : "false");
        if (event.phase ==
            cojvr::games::call_of_juarez::LegacyTextureCreatePhase::result) {
            line << ";hr=0x" << std::hex << static_cast<unsigned long>(event.result)
                 << std::dec;
        }
        LogLine(line.str());
        if (event.phase == cojvr::games::call_of_juarez::LegacyTextureCreatePhase::result &&
            (FAILED(event.result) || event.sequence == 1 || event.sequence % 64 == 0)) {
            LogAddressSpace(event.sequence, FAILED(event.result));
        }
    } catch (...) {
    }
}

void ResourceDestroyEvent(
    void*, std::uintptr_t resource,
    cojvr::games::call_of_juarez::LegacyTextureKind kind,
    ULONG refcount) noexcept {
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=resource_destroy"
             << ";thread_id=" << GetCurrentThreadId()
             << ";object=0x" << std::hex << resource << std::dec
             << ";interface=IDirect3DBaseTexture9"
             << ";kind=" << LegacyTextureKindName(kind)
             << ";refcount=" << refcount
             << ";hr=0x0";
        LogLine(line.str());
    } catch (...) {
    }
}

const char* KnownInterface(const IID* iid) noexcept {
    if (!iid) return "none";
    if (*iid == IID_IUnknown) return "IUnknown";
    if (*iid == IID_IDirect3D9) return "IDirect3D9";
    if (*iid == IID_IDirect3D9Ex) return "IDirect3D9Ex";
    if (*iid == IID_IDirect3DDevice9) return "IDirect3DDevice9";
    if (*iid == IID_IDirect3DDevice9Ex) return "IDirect3DDevice9Ex";
    if (*iid == IID_IDirect3DSwapChain9) return "IDirect3DSwapChain9";
    if (*iid == IID_IDirect3DSwapChain9Ex) return "IDirect3DSwapChain9Ex";
    return "other";
}

void LogComTrace(
    const char* kind, void* object, const char* event, const IID* iid,
    void* returned, HRESULT result, ULONG refcount) noexcept {
    if (g_stereo.present_telemetry_sequence.load(std::memory_order_relaxed) > 8 &&
        !(std::string_view(event) == "release" && refcount == 0)) return;
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=" << event
             << ";kind=" << kind
             << ";thread_id=" << GetCurrentThreadId()
             << ";object=0x" << std::hex << reinterpret_cast<std::uintptr_t>(object)
             << ";returned=0x" << reinterpret_cast<std::uintptr_t>(returned)
             << ";hr=0x" << static_cast<unsigned long>(result)
             << std::dec << ";interface=" << KnownInterface(iid);
        if (iid && std::string_view(KnownInterface(iid)) == "other") {
            line << ";iid=" << std::hex << std::setfill('0')
                 << std::setw(8) << iid->Data1 << '-'
                 << std::setw(4) << iid->Data2 << '-'
                 << std::setw(4) << iid->Data3 << '-'
                 << std::setw(2) << static_cast<unsigned>(iid->Data4[0])
                 << std::setw(2) << static_cast<unsigned>(iid->Data4[1]) << '-';
            for (int i = 2; i < 8; ++i) {
                line << std::setw(2) << static_cast<unsigned>(iid->Data4[i]);
            }
            line << std::dec;
        }
        if (std::string_view(event) == "add_ref" ||
            std::string_view(event) == "release") {
            line << ";refcount=" << refcount;
        }
        LogLine(line.str());
        if (refcount == 0 && std::string_view(event) == "release") {
            LogLine(std::string("native_stereo_startup_event: event=") + kind +
                    "_destroy;thread_id=" + std::to_string(GetCurrentThreadId()));
        }
    } catch (...) {
    }
}

void FactoryComTrace(void* object, const char* event, const IID* iid,
                     void* returned, HRESULT result, ULONG refcount) noexcept {
    LogComTrace("factory", object, event, iid, returned, result, refcount);
}

void DeviceComTrace(void* object, const char* event, const IID* iid,
                    void* returned, HRESULT result, ULONG refcount) noexcept {
    LogComTrace("device", object, event, iid, returned, result, refcount);
}

void SwapChainComTrace(void* object, const char* event, const IID* iid,
                       void* returned, HRESULT result, ULONG refcount) noexcept {
    LogComTrace("swapchain", object, event, iid, returned, result, refcount);
}

void DeviceMethodTrace(
    IDirect3DDevice9* device, const char* method, bool entering, HRESULT result) noexcept {
    if (g_stereo.present_telemetry_sequence.load(std::memory_order_relaxed) > 8) return;
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=device_method_"
             << (entering ? "enter" : "exit")
             << ";thread_id=" << GetCurrentThreadId()
             << ";object=0x" << std::hex << reinterpret_cast<std::uintptr_t>(device)
             << ";hr=0x" << static_cast<unsigned long>(result)
             << std::dec << ";method=" << method;
        LogLine(line.str());
    } catch (...) {
    }
}

void SwapChainMethodTrace(
    IDirect3DSwapChain9* chain, const char* method, bool entering, HRESULT result) noexcept {
    if (g_stereo.present_telemetry_sequence.load(std::memory_order_relaxed) > 8) return;
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=swapchain_method_"
             << (entering ? "enter" : "exit")
             << ";thread_id=" << GetCurrentThreadId()
             << ";object=0x" << std::hex << reinterpret_cast<std::uintptr_t>(chain)
             << ";hr=0x" << static_cast<unsigned long>(result)
             << std::dec << ";method=" << method;
        LogLine(line.str());
    } catch (...) {
    }
}

void CameraEvent(const char* event, const char* result, const char* detail) noexcept;

void PresenterLog(void*, const std::string_view line) noexcept { LogLine(line); }

void NeutralizeFlatUiPointer(const char* reason) noexcept {
    try {
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        if (g_stereo.flat_ui_pointer_active) {
            std::ostringstream detail;
            detail << "active=false;reason=" << (reason ? reason : "unknown")
                   << ";route=game_cursor_mailbox";
            CameraEvent("flat_ui_pointer", "neutralized", detail.str().c_str());
        }
        g_stereo.flat_ui_pointer_active = false;
        g_stereo.flat_ui_pointer_selection.Observe(cojvr::runtime::UiPointerHand::none, 0, false, false);
        g_stereo.flat_ui_target_applied = false;
        g_stereo.flat_ui_pointer_x = 0;
        g_stereo.flat_ui_pointer_y = 0;
        g_stereo.flat_ui_source_width = g_stereo.flat_ui_source_height = 0;
        ++g_stereo.flat_ui_source_generation;
        g_stereo.flat_ui_pointer_game_route_logged = false;
        g_stereo.flat_ui_pointer_game_error_logged = false;
    } catch (...) {
    }
}

bool FlatUiPointer(
    void*,
    const cojvr::backends::openvr::FlatUiPointerSample& sample) noexcept {
    try {
        const HWND window = reinterpret_cast<HWND>(
            g_stereo.game_window.load(std::memory_order_acquire));
        HWND root = window ? GetAncestor(window, GA_ROOT) : nullptr;
        if (!root) root = window;
        const bool foreground = root && GetForegroundWindow() == root;
        if (!foreground) {
            NeutralizeFlatUiPointer("game_not_foreground");
            return false;
        }
        if (!sample.active) {
            NeutralizeFlatUiPointer("ray_inactive");
            return true;
        }
        if (GetTickCount64() < g_stereo.flat_ui_mouse_override_until_ms.load(std::memory_order_acquire)) {
            NeutralizeFlatUiPointer("physical_mouse_priority");
            return false;
        }
        if (sample.source_width == 0 || sample.source_height == 0 ||
            sample.pixel_x >= sample.source_width || sample.pixel_y >= sample.source_height ||
            sample.claim == 0) {
            NeutralizeFlatUiPointer("invalid_source");
            return false;
        }

        // Queue source-image coordinates only. Desktop absolute movement is
        // reinterpreted by Chrome's mouse stream and feeds back into its cursor.
        // The game thread applies the target through UICursor.SetPos instead.
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        const bool source_changed = g_stereo.flat_ui_pointer_active &&
            (g_stereo.flat_ui_source_width != sample.source_width ||
             g_stereo.flat_ui_source_height != sample.source_height);
        if (g_stereo.flat_ui_source_width != sample.source_width ||
            g_stereo.flat_ui_source_height != sample.source_height) ++g_stereo.flat_ui_source_generation;
        if (source_changed) {
            // Invalidate click identity, not just pending: an old mouse-motion
            // transaction may still be returning from native dispatch unlocked.
            g_stereo.flat_ui_pointer_selection.Observe(cojvr::runtime::UiPointerHand::none,0,false,false);
            g_stereo.flat_ui_target_applied = false;
            CameraEvent("flat_ui_pointer", "source_changed", "pending_click_cancelled=true;hover_reset=true");
        }
        g_stereo.flat_ui_source_width = sample.source_width;
        g_stereo.flat_ui_source_height = sample.source_height;
        if (!g_stereo.flat_ui_pointer_active) {
            std::ostringstream detail;
            detail << "active=true;hand=" << (sample.using_left_hand ? "left" : "right")
                   << ";source=" << sample.source_width << 'x' << sample.source_height
                   << ";route=game_cursor_mailbox;desktop_injection=false";
            CameraEvent("flat_ui_pointer", "active", detail.str().c_str());
        }
        g_stereo.flat_ui_pointer_active = true;
        const auto previous = g_stereo.flat_ui_pointer_selection.snapshot();
        g_stereo.flat_ui_pointer_selection.Observe(sample.using_left_hand
            ? cojvr::runtime::UiPointerHand::left : cojvr::runtime::UiPointerHand::right,
            sample.claim, sample.select_down, sample.select_pressed && !source_changed);
        const auto current = g_stereo.flat_ui_pointer_selection.snapshot();
        // Retain the click's target until the engine has processed its hover.
        // A later pose cannot move that queued click to another menu option.
        if (!current.pending || !previous.pending || current.hand != previous.hand ||
            current.claim != previous.claim || current.click != previous.click) {
            g_stereo.flat_ui_pointer_x = sample.pixel_x;
            g_stereo.flat_ui_pointer_y = sample.pixel_y;
        }
        return true;
    } catch (...) {
        NeutralizeFlatUiPointer("exception");
        return false;
    }
}

cojvr::runtime::UiPointerSelectionSnapshot FlatUiSelection() noexcept {
    try {
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        return g_stereo.flat_ui_pointer_selection.snapshot();
    } catch (...) {
        return {};
    }
}

void CompleteFlatUiSelection(const cojvr::runtime::UiPointerSelectionSnapshot& selection,
                             bool dispatched) noexcept {
    try {
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        g_stereo.flat_ui_pointer_selection.Complete(selection, dispatched);
    } catch (...) {}
}

cojvr::runtime::UiPointerSelectionSnapshot ApplyFlatUiPointerMotion() noexcept {
    try {
        const HWND window = reinterpret_cast<HWND>(g_stereo.game_window.load(std::memory_order_acquire));
        HWND root = window ? GetAncestor(window, GA_ROOT) : nullptr;
        if (!root) root = window;
        if (!root || GetForegroundWindow() != root) {
            NeutralizeFlatUiPointer("game_not_foreground");
            return {};
        }
        cojvr::games::call_of_juarez::CameraProbeVector previous{};
        std::int32_t menu_index = 0;
        std::string observation_error;
        const bool readable = cojvr::games::call_of_juarez::ObserveCameraUiPointerPosition(
            previous, &observation_error, &menu_index);
        if (readable) {
            const bool changed = g_stereo.flat_ui_menu_known && g_stereo.flat_ui_menu_index != menu_index;
            g_stereo.flat_ui_menu_index = menu_index;
            g_stereo.flat_ui_menu_known = true;
            if (changed) {
                NeutralizeFlatUiPointer("menu_index_changed");
                return {};
            }
        }
        const bool mouse_button = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 ||
                                  (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        const bool vr_allowed = g_stereo.flat_ui_mouse_priority.Observe(
            previous.x, previous.y, readable, mouse_button, GetTickCount64());
        g_stereo.flat_ui_mouse_override_until_ms.store(
            g_stereo.flat_ui_mouse_priority.override_until_ms(), std::memory_order_release);
        if (!vr_allowed) {
            if (readable) {
                NeutralizeFlatUiPointer("physical_mouse_priority");
            } else {
                std::lock_guard lock(g_stereo.flat_ui_mutex);
                g_stereo.flat_ui_pointer_selection.Cancel(g_stereo.flat_ui_pointer_selection.snapshot());
                g_stereo.flat_ui_target_applied = false;
                if (g_stereo.flat_ui_pointer_active && !g_stereo.flat_ui_pointer_game_error_logged) {
                    const std::string detail =
                        "route=MainMenuModule.FindUI.GetMousePos;owner=game_cursor_only;"
                        "desktop_injection=false;detail=" + observation_error;
                    CameraEvent("flat_ui_pointer_game_route", "failed", detail.c_str());
                    g_stereo.flat_ui_pointer_game_error_logged = true;
                }
            }
            return {};
        }
        cojvr::runtime::UiPointerSelectionSnapshot selection{};
        std::uint32_t pixel_x = 0, pixel_y = 0;
        std::uint64_t source_generation = 0;
        {
            std::lock_guard lock(g_stereo.flat_ui_mutex);
            if (!g_stereo.flat_ui_pointer_active) return {};
            selection = g_stereo.flat_ui_pointer_selection.snapshot();
            pixel_x = g_stereo.flat_ui_pointer_x;
            pixel_y = g_stereo.flat_ui_pointer_y;
            source_generation = g_stereo.flat_ui_source_generation;
        }
        std::string error;
        bool input_consumed = false;
        bool applied = cojvr::games::call_of_juarez::DispatchCameraUiPointerMotion(
            static_cast<float>(pixel_x), static_cast<float>(pixel_y), &error, &input_consumed);
        cojvr::games::call_of_juarez::CameraProbeVector observed{};
        std::int32_t observed_index = 0;
        if (applied) {
            applied = cojvr::games::call_of_juarez::ObserveCameraUiPointerPosition(observed, &error, &observed_index) &&
                observed_index == menu_index &&
                std::abs(observed.x - static_cast<float>(pixel_x)) <= 1.0F &&
                std::abs(observed.y - static_cast<float>(pixel_y)) <= 1.0F;
            if (!applied && error.empty()) error = "logical cursor did not match the projected target";
            if (applied) g_stereo.flat_ui_mouse_priority.Applied(observed.x, observed.y);
        }
        {
            std::lock_guard lock(g_stereo.flat_ui_mutex);
            const auto current = g_stereo.flat_ui_pointer_selection.snapshot();
            if (!g_stereo.flat_ui_pointer_active || source_generation != g_stereo.flat_ui_source_generation ||
                current.hand != selection.hand ||
                current.claim != selection.claim || current.click != selection.click ||
                current.held != selection.held) return {};
            // A failed mouse delivery must not keep its trigger target latched
            // indefinitely. Cancel the click while preserving automatic aim.
            if (!applied) g_stereo.flat_ui_pointer_selection.Cancel(selection);
            const auto previous_delivery = g_stereo.flat_ui_applied_selection;
            const bool hover_ready = g_stereo.flat_ui_target_applied &&
                previous_delivery.hand == selection.hand && previous_delivery.claim == selection.claim &&
                previous_delivery.click == selection.click &&
                g_stereo.flat_ui_applied_x == pixel_x && g_stereo.flat_ui_applied_y == pixel_y &&
                std::abs(previous.x - static_cast<float>(pixel_x)) <= 1.0F &&
                std::abs(previous.y - static_cast<float>(pixel_y)) <= 1.0F;
            g_stereo.flat_ui_target_applied = applied;
            if (applied) {
                g_stereo.flat_ui_applied_selection = selection;
                g_stereo.flat_ui_applied_x = pixel_x;
                g_stereo.flat_ui_applied_y = pixel_y;
            }
            if (selection.pending && !hover_ready) selection.pending = false;
            const auto sequence = ++g_stereo.flat_ui_pointer_delivery_sequence;
            if (!g_stereo.flat_ui_pointer_game_route_logged ||
                (applied && g_stereo.flat_ui_pointer_game_error_logged) ||
                (!applied && !g_stereo.flat_ui_pointer_game_error_logged) ||
                (applied && sequence % 90 == 0)) {
                std::ostringstream detail;
                detail << "route=MainMenuModule.GetGlobalCursor.UICursor.SetPos+OnMouseMove"
                       << ";owner=game_cursor_only;desktop_injection=false;pixel="
                       << pixel_x << ',' << pixel_y << ";claim=" << selection.claim
                       << ";input_consumer=" << (input_consumed ? "native_sprite_tree" : "startup_sprite_only")
                       << ";menu_index=" << menu_index
                       << ";previous=" << previous.x << ',' << previous.y
                       << ";observed=" << observed.x << ',' << observed.y;
                if (!error.empty()) detail << ";detail=" << error;
                CameraEvent("flat_ui_pointer_game_route", applied ? "applied" : "failed", detail.str().c_str());
            }
            g_stereo.flat_ui_pointer_game_route_logged = true;
            g_stereo.flat_ui_pointer_game_error_logged = !applied;
        }
        return applied ? selection : cojvr::runtime::UiPointerSelectionSnapshot{};
    } catch (...) {
        return {};
    }
}

bool DispatchFlatUiPointerSelection(
    const cojvr::runtime::UiPointerSelectionSnapshot& selection,
    bool* loading, std::string* error, bool* hint_dismissed,
    cojvr::games::call_of_juarez::CoJUiDispatchRoute* route) noexcept {
    try {
        const auto reject = [error](const char* reason) {
            if (error) *error = reason;
            return false;
        };
        const auto reject_with_detail = [error](
            const char* reason, const std::string& detail) {
            if (error) {
                *error = reason;
                if (!detail.empty()) {
                    error->append(": ");
                    error->append(detail);
                }
            }
            return false;
        };
        // Serialize validation and dispatch with presenter cancellation/claim
        // changes. An accepted tap can be released while waiting for hover.
        std::lock_guard lock(g_stereo.flat_ui_mutex);
        const auto current = g_stereo.flat_ui_pointer_selection.snapshot();
        const HWND window = reinterpret_cast<HWND>(g_stereo.game_window.load(std::memory_order_acquire));
        HWND root = window ? GetAncestor(window, GA_ROOT) : nullptr;
        if (!root) root = window;
        if (!selection.pending) return reject("selection_not_pending");
        if (!current.pending) return reject("current_not_pending");
        if (!g_stereo.flat_ui_pointer_active) return reject("pointer_inactive");
        if (!g_stereo.flat_ui_target_applied) return reject("target_not_applied");
        if (current.hand != selection.hand || current.claim != selection.claim ||
            current.click != selection.click) {
            return reject("identity_mismatch");
        }
        if (!root) return reject("game_window_unavailable");
        if (GetForegroundWindow() != root) return reject("game_not_foreground");
        cojvr::games::call_of_juarez::CameraProbeVector position{};
        std::int32_t menu_index = 0;
        std::string readback_error;
        const bool readback_ok = cojvr::games::call_of_juarez::ObserveCameraUiPointerPosition(
            position, &readback_error, &menu_index);
        const char* rejection_reason = nullptr;
        if (!readback_ok) {
            rejection_reason = "readback_failed";
        } else if (!g_stereo.flat_ui_menu_known) {
            rejection_reason = "menu_unknown";
        } else if (menu_index != g_stereo.flat_ui_menu_index) {
            rejection_reason = "menu_index_mismatch";
        } else if (std::abs(position.x - static_cast<float>(g_stereo.flat_ui_applied_x)) > 1.0F ||
                   std::abs(position.y - static_cast<float>(g_stereo.flat_ui_applied_y)) > 1.0F) {
            rejection_reason = "coordinate_mismatch";
        }
        if (rejection_reason) {
            g_stereo.flat_ui_pointer_selection.Observe(cojvr::runtime::UiPointerHand::none, 0, false, false);
            g_stereo.flat_ui_target_applied = false;
            return reject_with_detail(rejection_reason, readback_error);
        }
        std::string dispatch_error;
        const bool dispatched = cojvr::games::call_of_juarez::DispatchCameraUiPointerSelectPress(
            false, loading, &dispatch_error, hint_dismissed, route);
        if (!dispatched) return reject_with_detail("dispatch_failed", dispatch_error);
        if (error) error->clear();
        return true;
    } catch (...) {
        try {
            if (error) *error = "exception";
        } catch (...) {}
        return false;
    }
}

bool ShouldLogStereoTiming(const std::uint64_t frame_sequence) noexcept {
    return frame_sequence > 0 && (frame_sequence <= 8 || (frame_sequence % 90) == 0);
}

std::string ReadJsonStringField(const std::string_view text, const std::string_view field) {
    const std::string needle = "\"" + std::string(field) + "\"";
    const std::size_t field_offset = text.find(needle);
    if (field_offset == std::string_view::npos) return {};
    const std::size_t colon = text.find(':', field_offset + needle.size());
    if (colon == std::string_view::npos) return {};
    const std::size_t quote = text.find('"', colon + 1);
    if (quote == std::string_view::npos) return {};
    const std::size_t end = text.find('"', quote + 1);
    if (end == std::string_view::npos) return {};
    return std::string(text.substr(quote + 1, end - quote - 1));
}

std::string ReadRunManifestField(const std::string_view field) noexcept {
    try {
        const auto path = GameDirectory() / L".cojvr-run.json";
        std::ifstream input(path, std::ios::binary);
        if (!input) return {};
        const std::string text{
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        return ReadJsonStringField(text, field);
    } catch (...) {
        return {};
    }
}

void CameraEvent(const char* event, const char* result, const char* detail) noexcept {
    try {
        std::ostringstream line;
        line << "camera_probe_event: event=" << (event ? event : "unknown")
             << " result=" << (result ? result : "unknown");
        if (detail && *detail) line << " detail=" << detail;
        LogLine(line.str());
    } catch (...) {
    }
}

void LogTransportFailure(
    const std::uint64_t frame_sequence,
    const char* stage,
    const std::string_view error) noexcept {
    if (frame_sequence == 0 ||
        g_stereo.last_error_frame.exchange(frame_sequence, std::memory_order_acq_rel) ==
            frame_sequence) {
        return;
    }
    try {
        std::ostringstream line;
        line << "native_stereo_transport: status=failed stage=" << stage
             << " frame_sequence=" << frame_sequence << " error=" << error;
        LogLine(line.str());
    } catch (...) {
    }
}

IDirect3DDevice9* RetainCurrentDevice() noexcept {
    try {
        std::lock_guard lock(g_stereo.device_mutex);
        if (!g_stereo.device) return nullptr;
        g_stereo.device->AddRef();
        return g_stereo.device;
    } catch (...) {
        return nullptr;
    }
}

bool PublishCapturedFrame(
    NativeStereoState& state,
    cojvr::backends::d3d9::StereoCpuFrame frame,
    const cojvr::backends::d3d9::FramePresentationMode presentation_mode,
    const char* source,
    const SourceCaptureAuthorization authorization = {}) noexcept {
    try {
        if (state.fixed_outputs.enabled) {
            if (!SourceCaptureCurrent(state, authorization) ||
                authorization.sequence != frame.capture_sequence ||
                authorization.device != frame.device_id ||
                authorization.generation != frame.generation) return false;
            // Fixed native output is an equal-extent shared copy only. A CPU
            // compatibility fallback cannot silently admit the private route.
            if (presentation_mode == cojvr::backends::d3d9::FramePresentationMode::native_stereo) {
                bool valid = frame.transport ==
                    cojvr::backends::d3d9::StereoFrameTransport::d3d9ex_shared_texture;
                for (const auto& eye : frame.shared_eyes)
                    valid = valid && eye.width == state.fixed_outputs.native_width &&
                        eye.height == state.fixed_outputs.native_height &&
                        eye.d3d_format == state.fixed_outputs.native_d3d_format;
                if (!valid) {
                    IndependentBankFault(state, "native_shared_layout");
                    return false;
                }
            }
        }
        frame.presentation_mode = presentation_mode;
        frame.transport_sequence =
            state.transport_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
        const std::uint64_t capture_sequence = frame.capture_sequence;
        const bool shared_frame =
            frame.transport ==
            cojvr::backends::d3d9::StereoFrameTransport::d3d9ex_shared_texture;
        const std::uintptr_t device_id = frame.device_id;
        const std::uint64_t generation = frame.generation;
        const std::uint32_t producer_slot = frame.producer_slot;
        if (!state.presenter.Publish(std::move(frame), authorization.output_epoch)) {
            LogTransportFailure(
                capture_sequence, source,
                "presenter mailbox rejected completed CPU frame");
            return false;
        }
        // Producer and consumer diagnostics sample the same frame sequences.
        // Writing a line for every published frame stalls the render hook on I/O.
        if (shared_frame && capture_sequence > 0 &&
            (capture_sequence <= 8 || (capture_sequence % 90) == 0)) {
            std::ostringstream line;
            line << "native_stereo_startup_event: event=shared_frame_published"
                 << ";thread_id=" << GetCurrentThreadId()
                 << ";device=0x" << std::hex << device_id << std::dec
                 << ";generation=" << generation
                 << ";frame_sequence=" << capture_sequence
                 << ";slot=" << producer_slot;
            LogLine(line.str());
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool PaceRenderFrame(NativeStereoState* state) noexcept {
    if (!state || g_finalization_started.load(std::memory_order_acquire)) return false;
    try {
    const float reported_hz = state->presenter.display_frequency_hz();
    const float refresh_hz = state->frame_cadence.Target(
        reported_hz, state->desktop_vsync_disabled.load(std::memory_order_acquire));
    if (refresh_hz != state->last_pacing_hz) {
        state->last_pacing_hz = refresh_hz;
        std::ostringstream line;
        line << "native_stereo_render_pacing: source=hmd_property;target_hz="
             << refresh_hz << ";mode=" << (refresh_hz > 0.0F ? "rate_cap" : "unavailable_fail_open");
        LogLine(line.str());
    }
    if (reported_hz > 0.0F && refresh_hz == 0.0F && !state->late_rate_reported) {
        state->late_rate_reported = true;
        LogLine("native_stereo_render_pacing: status=pending reason=desktop_vsync_reset_required");
    }
    const auto pacing_begin = std::chrono::steady_clock::now();
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        pacing_begin.time_since_epoch()).count();
    const auto deadline = std::chrono::steady_clock::time_point(
        std::chrono::nanoseconds(state->frame_pacer.Deadline(now_ns, refresh_hz)));
    // Intentional CPU render pacing, separate from nonblocking GPU handoff.
    // Refresh changes rebase the cap. Retain the last cadence while the mirror
    // is IMMEDIATE; missing initial timing keeps the game's desktop interval.
    while (std::chrono::steady_clock::now() < deadline) {
        if (g_finalization_started.load(std::memory_order_acquire)) {
            state->frame_pacer.Reset();
            return false;
        }
        std::this_thread::sleep_until(std::min(
            deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds(2)));
    }
    if (ShouldLogStereoTiming(++state->pacing_sequence)) {
        const double wait_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - pacing_begin).count();
        std::ostringstream line;
        line << "native_stereo_render_pacing_timing: sequence=" << state->pacing_sequence
             << ";target_hz=" << refresh_hz << std::fixed << std::setprecision(3)
             << ";pacing_wait_ms=" << wait_ms << ";wait_kind=cpu_render_schedule";
        LogLine(line.str());
    }
    state->frame_cadence.RenderPaced();
    return true;
    } catch (...) {
        state->frame_pacer.Reset();
        return false;
    }
}

std::uint64_t PublishSettingsOwner(NativeStereoState& state,
    const cojvr::games::call_of_juarez::CoJPresentationOwner& owner,
    const bool observed) noexcept {
    // Value-only owner identity. A menu/movie/actor transition cancels an open
    // settings gesture even when the last submitted texture has not changed.
    const auto player=cojvr::games::call_of_juarez::ObserveCameraPlayerGeneration();
    // Startup menus have no campaign module. They still need native UI input
    // and access to local VR settings; campaign mutation remains separately
    // fail-closed. The verified-owner bit fences transition into/out of play.
    const std::uint64_t token=(player << 6U) | 1U |
        (owner.timer_frozen ? 2U : 0U) | (owner.player_alive ? 4U : 0U) |
        (owner.mission_end_visible ? 8U : 0U) | (owner.movie_playing ? 16U : 0U) |
        (observed && owner.valid ? 32U : 0U);
    state.presenter.PublishSettingsContext(token);
    state.presenter.PublishSettingsLanguage(
        cojvr::games::call_of_juarez::ObserveCameraUiLanguage()==
            cojvr::games::call_of_juarez::CoJUiLanguage::spanish ?
            cojvr::runtime::SettingsLanguage::spanish : cojvr::runtime::SettingsLanguage::english);
    return token;
}

std::uint64_t ObserveStereoOwner(NativeStereoState& state,
    const cojvr::games::call_of_juarez::CoJPresentationOwner& owner,
    bool observed, bool settings_captured, bool immersive_movie) noexcept {
    const auto token = PublishSettingsOwner(state, owner, observed);
    const bool allowed = observed && (!owner.blocked() || immersive_movie);
    ObserveSourceOwner(state, (token << 2U) |
        (allowed ? (immersive_movie ? 3U : (settings_captured ? 2U : 1U)) : 0U));
    if (!allowed) {
        state.frame_settings_captured.store(false, std::memory_order_release);
        state.frame_owner_epoch.store(0, std::memory_order_release);
    }
    // Exact player/module observations plus the visual route. Settings and
    // movies have different native visuals even though both suppress gameplay.
    return state.stereo_queue.ObserveOwner(allowed ?
        (token << 2U) | (immersive_movie ? 3U : (settings_captured ? 2U : 1U)) : 0);
}

void ObserveNaturalFrame(void* context,bool complete) noexcept {
    auto& state=*static_cast<NativeStereoState*>(context);
    state.natural_world_seen.store(true,std::memory_order_release);
    state.natural_frame_ready.store(false,std::memory_order_release);
    if(!complete)return;
    cojvr::games::call_of_juarez::CoJPresentationOwner owner{};
    const bool observed=cojvr::games::call_of_juarez::ObserveCameraGamePresentationOwner(owner);
    const bool movie=observed&&owner.immersive_movie_allowed()&&
        cojvr::runtime::CurrentCinematicMode()==cojvr::runtime::CinematicsMode::immersive;
    (void)ObserveStereoOwner(state,owner,observed,cojvr::runtime::SettingsInputCaptured(),movie);
    std::lock_guard lock(state.source_mutex);
    state.natural_owner.store(state.source_owner);
    state.natural_generation.store(state.device_generation.load());
    state.natural_frame_ready.store(observed,std::memory_order_release);
}

void BeforePresentBoundary(IDirect3DDevice9* device, const char* source) noexcept {
    if (!device || !g_stereo.runtime_ready) {
        InvalidateSourceAuthority(g_stereo);
        g_stereo.presenter.PublishGameplayFeedbackContext(false);
        return;
    }
    if (g_stereo.frame_cadence.PresentNeedsPacing()) {
        if (!PaceRenderFrame(&g_stereo)) {
            InvalidateSourceAuthority(g_stereo);
            g_stereo.presenter.PublishGameplayFeedbackContext(false);
            return;
        }
        (void)g_stereo.frame_cadence.PresentNeedsPacing();
    }
    // A failed native/COM rollback still owns its restore transaction. Do not
    // capture partially redirected targets as natural desktop/theater content.
    if (g_stereo.fixed_outputs.enabled &&
        (g_stereo.independent_render.active() || g_stereo.independent_render.restoration_pending())) return;
    try {
        bool timer_frozen = false;
        std::string timer_error;
        const bool timer_valid =
            cojvr::games::call_of_juarez::ObserveCameraGameTimerFrozen(
                timer_frozen, &timer_error);
        cojvr::games::call_of_juarez::CoJPresentationOwner presentation{};
        const bool presentation_valid=
            cojvr::games::call_of_juarez::ObserveCameraGamePresentationOwner(presentation);
        const bool presentation_blocked=presentation.blocked();
        const bool immersive_movie=presentation_valid && presentation.immersive_movie_allowed() &&
            cojvr::runtime::CurrentCinematicMode()==cojvr::runtime::CinematicsMode::immersive;
        // Native menus use current presenter capture, including its release
        // barrier. A previous stereo frame snapshot cannot latch this gate
        // after death/pause prevents further stereo admission.
        const bool settings_captured=cojvr::runtime::SettingsInputCaptured();
        if (settings_captured) {
            NeutralizeFlatUiPointer("vr_settings_capture");
            g_stereo.flat_ui_select_retry = {};
        }
        const std::uint64_t now_ms = GetTickCount64();
        const auto present_owner_epoch = ObserveStereoOwner(
            g_stereo, presentation, presentation_valid, settings_captured, immersive_movie);
        if (g_stereo.flat_ui_select_release_required) g_stereo.stereo_queue.Invalidate();
        if (g_stereo.fixed_outputs.enabled) {
            const auto sequence = g_stereo.source_capture_admission.queued_sequence();
            const auto authorization = FindSourceCapture(g_stereo, sequence);
            if (g_stereo.source_capture_admission.PreserveForPresent(sequence,
                    presentation_valid && (!presentation_blocked || immersive_movie) &&
                        !g_stereo.flat_ui_select_release_required,
                    present_owner_epoch && authorization.owner_epoch == present_owner_epoch &&
                        g_stereo.stereo_queue.CaptureEpoch(sequence) == present_owner_epoch,
                    SourceCaptureCurrent(g_stereo, authorization))) {
                // Queued does not mean published/fresh. Preserve its token for
                // next Begin's collection, and never recapture the eye mirror
                // under the natural-theater label at this Present boundary.
                g_stereo.flat_capture_active.store(false, std::memory_order_release);
                return;
            }
        }
        const bool stereo_fresh = g_stereo.stereo_queue.Fresh(now_ms);
        if (!presentation_valid || presentation_blocked || settings_captured || g_stereo.flat_ui_select_release_required ||
            !stereo_fresh) {
            g_stereo.presenter.PublishGameplayFeedbackContext(false);
        }
        if (presentation_valid && (!presentation_blocked || immersive_movie) && !g_stereo.flat_ui_select_release_required &&
            stereo_fresh) {
            g_stereo.flat_capture_active.store(false, std::memory_order_release);
            return;
        }

        if (!g_stereo.flat_capture_active.exchange(true, std::memory_order_acq_rel)) {
            std::ostringstream entered;
            entered << "native_stereo_flat_theater: status=entered source="
                    << (source ? source : "unknown")
                    << ";reason="
                    << (presentation_blocked && !immersive_movie ? "native_blocking_presentation" : "native_stereo_stale")
                    << ";owner_observed=" << presentation_valid
                    << ";owner_valid=" << presentation.valid
                    << ";player_alive=" << presentation.player_alive
                    << ";mission_end_visible=" << presentation.mission_end_visible
                    << ";movie_playing=" << presentation.movie_playing
                    << ";immersive_movie=" << immersive_movie << ";stereo_fresh=" << stereo_fresh
                    << ";timer_valid=" << timer_valid << ";timer_frozen=" << timer_frozen;
            LogLine(entered.str());
        }


        cojvr::backends::openvr::OpenVrTrackingSample tracking{};
        if (!g_stereo.presenter.LatestTracking(tracking) ||
            !tracking.pose.orientation_valid || !tracking.pose.position_valid || tracking.sequence == 0) {
            g_stereo.stereo_queue.Invalidate();
            g_stereo.presenter.PublishGameplayFeedbackContext(false);
            return;
        }

        const HWND ui_window = reinterpret_cast<HWND>(g_stereo.game_window.load(std::memory_order_acquire));
        HWND ui_root = ui_window ? GetAncestor(ui_window, GA_ROOT) : nullptr;
        if (!ui_root) ui_root = ui_window;
        const bool menu_dispatch_allowed = !settings_captured && !tracking.vr_settings_input_captured &&
            tracking.ui_actions_allowed && ui_root && GetForegroundWindow() == ui_root;
        if (!menu_dispatch_allowed) {
            g_stereo.flat_ui_select_retry = {};
        }
        // Input focus gates menu mutation, not the frame publication that can
        // establish compositor focus during startup.
        if (menu_dispatch_allowed) {
            // Navigation cancels the prior menu's trigger before moving or
            // selecting anything in the destination menu.
            if ((tracking.ui_back_pressed || tracking.pause_pressed) || tracking.ui_accept_pressed)
                NeutralizeFlatUiPointer("ui_navigation");
            if ((tracking.ui_back_pressed || tracking.pause_pressed)) g_stereo.flat_ui_select_retry = {};
            if ((tracking.ui_back_pressed || tracking.pause_pressed)) {
                cojvr::games::call_of_juarez::CoJUiDispatchRoute back_route =
                    cojvr::games::call_of_juarez::CoJUiDispatchRoute::none;
                std::string back_error;
                const bool backed = cojvr::games::call_of_juarez::DispatchCameraUiBackPress(
                    &back_error, &back_route);
                std::ostringstream detail;
                detail << "source=" << (tracking.pause_pressed ? "right_options" : "right_circle") << ";route="
                       << cojvr::games::call_of_juarez::CoJUiBackDispatchRouteName(back_route);
                if (!back_error.empty()) detail << ";detail=" << back_error;
                CameraEvent("flat_ui_back", backed ? "applied" : "failed", detail.str().c_str());
            }
            const auto pointer_selection = ApplyFlatUiPointerMotion();
            const auto pointer_hand = pointer_selection.hand;
            const bool pointer_active = pointer_hand != cojvr::runtime::UiPointerHand::none;
            // Selection may follow only a successfully applied logical cursor
            // target from the same hand/claim/click. No desktop motion is injected.

            if (g_stereo.flat_ui_select_release_required && timer_valid && !timer_frozen &&
                !tracking.ui_select_left && !tracking.ui_select_right && !tracking.ui_accept) {
                g_stereo.flat_ui_select_release_required = false;
                g_stereo.flat_loading_resume_pending = false;
                CameraEvent(
                    "blocking_ui_resume",
                    "observed",
                    "game_timer_valid=true;game_timer_frozen=false;trigger_released=true;route=LawmanModule.TimerStart");
            }

            // Ray edges arrive only after the accepted motion, tagged with its hand
            // and claim. Raw action edges cannot become valid under a later claim.
            const bool pointer_select = pointer_selection.pending;
            g_stereo.flat_ui_select_retry.Observe(!(tracking.ui_back_pressed || tracking.pause_pressed) && tracking.ui_accept_pressed,
                !(tracking.ui_back_pressed || tracking.pause_pressed) && tracking.ui_accept);
            const bool cross_select = g_stereo.flat_ui_select_retry.ShouldDispatch(true, timer_valid && timer_frozen);
            const bool ui_select_pressed = pointer_select || tracking.ui_accept_pressed;
            const bool select_allowed = pointer_active || tracking.ui_accept || (timer_valid && timer_frozen);
            if (pointer_select || cross_select) {
                bool current_ui_is_loading = false;
                bool paused_hint_dismissed = false;
                cojvr::games::call_of_juarez::CoJUiDispatchRoute ui_route =
                    cojvr::games::call_of_juarez::CoJUiDispatchRoute::none;
                std::string ui_error;
                const bool dispatched = select_allowed && (cross_select ?
                    cojvr::games::call_of_juarez::DispatchCameraUiSelectPress(
                        false, &current_ui_is_loading, &ui_error,
                        &paused_hint_dismissed, &ui_route) :
                    DispatchFlatUiPointerSelection(pointer_selection, &current_ui_is_loading,
                        &ui_error, &paused_hint_dismissed, &ui_route));
                g_stereo.flat_ui_select_retry.Complete(dispatched);
                if (pointer_select && !cross_select) CompleteFlatUiSelection(pointer_selection, dispatched);
                if (ui_select_pressed || dispatched) {
                    std::ostringstream detail;
                    detail << "source=";
                    if (cross_select) {
                        detail << "right_cross";
                    } else {
                        detail
                           << (pointer_hand == cojvr::runtime::UiPointerHand::left ? "left_l2" : "right_r2");
                    }
                    detail
                           << ";pointer_active=" << (pointer_active ? "true" : "false")
                           << ";pointer_claim=" << pointer_selection.claim
                           << ";retry=" << (!ui_select_pressed ? "true" : "false")
                           << ";current_ui_is_loading="
                           << (current_ui_is_loading ? "true" : "false")
                           << ";paused_hint_dismissed="
                           << (paused_hint_dismissed ? "true" : "false")
                           << ";route=" <<
                               cojvr::games::call_of_juarez::CoJUiDispatchRouteName(ui_route);
                    if (!ui_error.empty()) detail << ";detail=" << ui_error;
                    CameraEvent(
                        "flat_ui_select",
                        dispatched ? "applied" : "failed",
                        detail.str().c_str());
                }
                if (dispatched && timer_valid && timer_frozen) {
                    g_stereo.flat_loading_resume_pending = true;
                    g_stereo.flat_ui_select_release_required = true;
                    std::ostringstream blocking_detail;
                    blocking_detail << "source="
                                    << (cross_select ? "right_cross" :
                                        (pointer_hand == cojvr::runtime::UiPointerHand::left ? "left_l2" : "right_r2"))
                                    << ";game_timer_valid=true;game_timer_frozen=true"
                                    << ";current_ui_is_loading="
                                    << (current_ui_is_loading ? "true" : "false")
                                    << ";gameplay_suppressed=true"
                                    << ";body_mutation_suppressed=true"
                                    << ";fire_suppressed_until_release=true"
                                    << ";route=" <<
                                        cojvr::games::call_of_juarez::CoJUiDispatchRouteName(ui_route);
                    CameraEvent("blocking_ui_select", "applied", blocking_detail.str().c_str());
                }
            }

        }
        if(g_stereo.fixed_outputs.enabled&&g_stereo.natural_world_seen.load()){
            std::lock_guard lock(g_stereo.source_mutex);
            if(!g_stereo.natural_frame_ready.load()||g_stereo.natural_owner.load()!=g_stereo.source_owner||
               g_stereo.natural_generation.load()!=g_stereo.device_generation.load())return;
        }
        // The startup/menu fallback does not need a full 60/90 Hz CPU readback.
        // Capture every second Present; the presenter repeats the latest frame
        // at compositor cadence between updates.
        const std::uint64_t present_sequence =
            g_stereo.flat_capture_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
        if ((present_sequence & 1U) != 0U) return;

        Microsoft::WRL::ComPtr<IDirect3DSurface9> back_buffer;
        const HRESULT back_buffer_result =
            device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
        if (FAILED(back_buffer_result) || !back_buffer) return;
        const std::uint64_t generation =
            g_stereo.device_generation.load(std::memory_order_acquire);
        if (generation == 0) return;
        // Menu dispatch above may have changed native owner synchronously.
        // Freeze the fresh flat owner and source mode before its immediate copy.
        if (g_stereo.fixed_outputs.enabled) {
            cojvr::games::call_of_juarez::CoJPresentationOwner flat_owner{};
            const bool observed = cojvr::games::call_of_juarez::ObserveCameraGamePresentationOwner(flat_owner);
            const bool movie = observed && flat_owner.immersive_movie_allowed() &&
                cojvr::runtime::CurrentCinematicMode() == cojvr::runtime::CinematicsMode::immersive;
            (void)ObserveStereoOwner(g_stereo, flat_owner, observed,
                cojvr::runtime::SettingsInputCaptured(), movie);
        }
        const auto authorization = AuthorizeSourceCapture(g_stereo, present_sequence,
            g_stereo.frame_owner_epoch.load(std::memory_order_acquire), generation, device,
            cojvr::backends::d3d9::FramePresentationMode::flat_theater);
        cojvr::backends::d3d9::StereoCpuFrame completed{};
        if (!g_stereo.flat_capture.CaptureFlatFrameImmediate(
                device, back_buffer.Get(), present_sequence, generation,
                tracking.pose, tracking.sequence, completed)) {
            LogTransportFailure(
                present_sequence, "flat_capture", g_stereo.flat_capture.last_error());
            if (g_stereo.fixed_outputs.enabled) InvalidateSourceAuthority(g_stereo);
            return;
        }
        if (ShouldLogStereoTiming(present_sequence)) {
            LogLine(
                "native_stereo_flat_theater: status=captured " +
                std::string(g_stereo.flat_capture.capture_description()));
        }
        // Presentation only: no native UI, camera, target or cursor mutation.
        // Positive exact-game MenuMain ownership excludes pause/loading/movie.
        std::uint32_t menu_owner = 0;
        if (!g_stereo.fixed_outputs.enabled && completed.eyes[0].width == 2804 &&
            completed.eyes[0].height == 2860 &&
            cojvr::games::call_of_juarez::ObserveCameraMainMenuActive(&menu_owner)) {
            completed.flat_reference_width = 2560;
            completed.flat_reference_height = 1440;
            completed.flat_reference_owner = menu_owner;
        }
        if (PublishCapturedFrame(
                g_stereo, std::move(completed),
                cojvr::backends::d3d9::FramePresentationMode::flat_theater,
                "flat_publish", authorization)) {
            g_stereo.stereo_queue.FlatPublished();
            if (ShouldLogStereoTiming(present_sequence))
                LogLine(
                    "native_stereo_flat_theater: status=published " +
                    std::string(g_stereo.flat_capture.collect_description()));
        }
    } catch (...) {
        g_stereo.presenter.PublishGameplayFeedbackContext(false);
        LogLine("native_stereo_flat_theater: status=exception");
    }
}

thread_local std::uint32_t g_device_present_depth = 0;
thread_local std::uint64_t g_device_present_sequence = 0;

void BeforeDevicePresent(IDirect3DDevice9* device) noexcept {
    ++g_device_present_depth;
    if (g_device_present_depth == 1) {
        g_device_present_sequence =
            g_stereo.present_telemetry_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (g_device_present_sequence <= 8 || (g_device_present_sequence % 90) == 0) {
            try {
                std::ostringstream line;
                line << "native_stereo_startup_event: event=present_enter"
                     << ";thread_id=" << GetCurrentThreadId()
                     << ";device=0x" << std::hex
                     << reinterpret_cast<std::uintptr_t>(device) << std::dec
                     << ";generation="
                     << g_stereo.device_generation.load(std::memory_order_acquire)
                     << ";frame_sequence=" << g_device_present_sequence;
                LogLine(line.str());
            } catch (...) {
            }
        }
        BeforePresentBoundary(device, "device_present");
        PollFrameLumaProbes();
        if (g_stereo.luma_enabled) {
            Microsoft::WRL::ComPtr<IDirect3DSurface9> backbuffer;
            if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer)))
                QueueFrameLumaProbe(device, backbuffer.Get(), 2, g_device_present_sequence);
        }
    }
}

void AfterDevicePresent(IDirect3DDevice9* device, const HRESULT result) noexcept {
    if (g_device_present_depth == 1 &&
        (g_device_present_sequence <= 8 || (g_device_present_sequence % 90) == 0)) {
        try {
            std::ostringstream line;
            line << "native_stereo_startup_event: event=present_exit"
                 << ";thread_id=" << GetCurrentThreadId()
                 << ";device=0x" << std::hex
                 << reinterpret_cast<std::uintptr_t>(device) << std::dec
                 << ";generation="
                 << g_stereo.device_generation.load(std::memory_order_acquire)
                 << ";frame_sequence=" << g_device_present_sequence
                 << ";hr=0x" << std::hex << static_cast<unsigned long>(result) << std::dec;
            LogLine(line.str());
        } catch (...) {
        }
    }
    if (g_device_present_depth > 0) --g_device_present_depth;
}

void BeforeDeviceReset(
    IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* parameters) noexcept {
    CancelResourceProbe();
    g_stereo.frame_pacer.Reset();
    const float refresh_hz = g_stereo.runtime_ready ? g_stereo.presenter.display_frequency_hz() : 0.0F;
    if (parameters) (void)g_stereo.present_policy.Begin(*parameters, refresh_hz, true);
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=reset_enter"
             << ";thread_id=" << GetCurrentThreadId()
             << ";device=0x" << std::hex << reinterpret_cast<std::uintptr_t>(device)
             << std::dec
             << ";generation="
             << g_stereo.device_generation.load(std::memory_order_acquire);
        if (parameters) {
            line << ";backbuffer=" << parameters->BackBufferWidth << 'x'
                 << parameters->BackBufferHeight
                 << ";format=" << static_cast<unsigned>(parameters->BackBufferFormat)
                 << ";windowed=" << (parameters->Windowed ? "true" : "false")
                 << ";swap_effect=" << static_cast<unsigned>(parameters->SwapEffect);
        }
        LogLine(line.str());
    } catch (...) {
    }
}

void AfterDeviceReset(
    IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* parameters,
    const HRESULT result) noexcept {
    if (parameters) g_stereo.present_policy.Complete(SUCCEEDED(result), *parameters, g_stereo.frame_cadence);
    try {
        std::uint64_t generation =
            g_stereo.device_generation.load(std::memory_order_acquire);
        if (SUCCEEDED(result)) {
            g_stereo.desktop_vsync_disabled.store(parameters &&
                parameters->PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE,
                std::memory_order_release);
            g_stereo.late_rate_reported = false;
            generation = g_stereo.device_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
            g_stereo.flat_capture_active.store(false, std::memory_order_release);
            g_stereo.stereo_queue.Invalidate();
            g_stereo.capture_source_logged = {};
        }
        std::ostringstream line;
        line << "native_stereo_startup_event: event=reset_exit"
             << ";thread_id=" << GetCurrentThreadId()
             << ";device=0x" << std::hex << reinterpret_cast<std::uintptr_t>(device)
             << std::dec
             << ";generation=" << generation
             << ";hr=0x" << std::hex << static_cast<unsigned long>(result) << std::dec;
        LogLine(line.str());
        if (SUCCEEDED(result)) {
            std::ostringstream changed;
            changed << "native_stereo_startup_event: event=resource_generation_changed"
                    << ";thread_id=" << GetCurrentThreadId()
                    << ";device=0x" << std::hex
                    << reinterpret_cast<std::uintptr_t>(device) << std::dec
                    << ";generation=" << generation
                    << ";reason=reset_success";
            LogLine(changed.str());
        }
    } catch (...) {
    }
}

void BeforeSwapChainPresent(IDirect3DDevice9* device) noexcept {
    if (g_device_present_depth == 0) {
        BeforePresentBoundary(device, "swapchain_present");
        PollFrameLumaProbes();
        if (g_stereo.luma_enabled && device) {
            Microsoft::WRL::ComPtr<IDirect3DSurface9> backbuffer;
            if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer)))
                QueueFrameLumaProbe(device, backbuffer.Get(), 2,
                    g_stereo.present_telemetry_sequence.fetch_add(1) + 1);
        }
    }
}

void MaintainDevicePresentHook(const std::stop_token stop_token) noexcept {
    using namespace std::chrono_literals;
    while (!stop_token.stop_requested()) {
        IDirect3DDevice9* device = RetainCurrentDevice();
        if (device) {
            const auto outcome =
                cojvr::backends::d3d9::ReacquireDeviceVtableHookDetailed(device);
            (void)cojvr::games::call_of_juarez::
                ReacquireD3D9ExLegacyTextureCompatibility(device);
            device->Release();
            if (outcome.result ==
                    cojvr::backends::d3d9::HookRegistryResult::Installed &&
                outcome.modified_slots > 0) {
                g_stereo.present_hook_conflict_logged.store(
                    false, std::memory_order_release);
                LogLine(
                    "native_stereo_device_present_hook: status=reacquired slots=" +
                    std::to_string(outcome.modified_slots));
            } else if (outcome.result ==
                           cojvr::backends::d3d9::HookRegistryResult::Conflict &&
                !g_stereo.present_hook_conflict_logged.exchange(
                    true, std::memory_order_acq_rel)) {
                LogLine(
                    "native_stereo_device_present_hook: status=foreign_owner_preserved");
            }
        }
        std::this_thread::sleep_for(100ms);
    }
}

void StartPresentHookWorker() noexcept {
    try {
        std::lock_guard lock(g_stereo.present_hook_worker_mutex);
        if (!g_stereo.present_hook_worker.joinable()) {
            g_stereo.present_hook_worker = std::jthread(MaintainDevicePresentHook);
        }
    } catch (...) {
        LogLine("native_stereo_device_present_hook: status=watchdog_start_failed");
    }
}

void StopPresentHookWorker() noexcept {
    try {
        std::lock_guard lock(g_stereo.present_hook_worker_mutex);
        if (g_stereo.present_hook_worker.joinable()) {
            g_stereo.present_hook_worker.request_stop();
            g_stereo.present_hook_worker.join();
        }
    } catch (...) {
        LogLine("native_stereo_device_present_hook: status=watchdog_stop_failed");
    }
}

void BeforeCreateDevice(
    IDirect3D9* factory,
    const UINT adapter,
    const D3DDEVTYPE device_type,
    HWND focus_window,
    const DWORD behavior_flags,
    D3DPRESENT_PARAMETERS* presentation_parameters) noexcept {
    const float refresh_hz = g_stereo.runtime_ready ? g_stereo.presenter.display_frequency_hz() : 0.0F;
    bool vr_paced = false;
    const DWORD requested_interval = presentation_parameters
        ? presentation_parameters->PresentationInterval : 0;
    if (presentation_parameters) {
        vr_paced = g_stereo.present_policy.Begin(*presentation_parameters, refresh_hz, false);
    }
    try {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=create_device_enter"
             << ";thread_id=" << GetCurrentThreadId()
             << ";factory=0x" << std::hex << reinterpret_cast<std::uintptr_t>(factory)
             << std::dec
             << ";adapter=" << adapter
             << ";device_type=" << static_cast<unsigned>(device_type)
             << ";focus_window=0x" << std::hex
             << reinterpret_cast<std::uintptr_t>(focus_window)
             << ";behavior_flags=0x" << behavior_flags << std::dec;
        if (presentation_parameters) {
            line << ";backbuffer=" << presentation_parameters->BackBufferWidth << 'x'
                 << presentation_parameters->BackBufferHeight
                 << ";format="
                 << static_cast<unsigned>(presentation_parameters->BackBufferFormat)
                 << ";windowed="
                 << (presentation_parameters->Windowed ? "true" : "false")
                 << ";swap_effect="
                 << static_cast<unsigned>(presentation_parameters->SwapEffect)
                 << ";requested_present_interval=" << requested_interval
                 << ";effective_present_interval=" << presentation_parameters->PresentationInterval
                 << ";vr_paced=" << (vr_paced ? "true" : "false");
        }
        LogLine(line.str());
    } catch (...) {
    }
}

void AfterCreateDevice(
    IDirect3D9* factory,
    UINT,
    D3DDEVTYPE,
    HWND focus_window,
    DWORD,
    D3DPRESENT_PARAMETERS* parameters,
    IDirect3DDevice9** returned_device,
    const HRESULT result) noexcept {
    IDirect3DDevice9* observed = returned_device ? *returned_device : nullptr;
    Microsoft::WRL::ComPtr<IDirect3DDevice9Ex> observed_ex;
    const bool returned_ex = observed &&
        SUCCEEDED(observed->QueryInterface(IID_PPV_ARGS(&observed_ex))) && observed_ex;
    try {
        std::ostringstream result_line;
        result_line << "native_stereo_startup_event: event=create_device_ex_result"
                    << ";thread_id=" << GetCurrentThreadId()
                    << ";device=0x" << std::hex
                    << reinterpret_cast<std::uintptr_t>(observed)
                    << ";hr=0x" << static_cast<unsigned long>(result) << std::dec
                    << ";device_api="
                    << (returned_ex ? "d3d9ex" : (observed ? "classic_d3d9" : "none"));
        LogLine(result_line.str());
    } catch (...) {
    }
    if (parameters) g_stereo.present_policy.Complete(SUCCEEDED(result) && observed, *parameters, g_stereo.frame_cadence);
    if (FAILED(result) || !observed) return;
    g_stereo.desktop_vsync_disabled.store(parameters &&
        parameters->PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE,
        std::memory_order_release);
    try {
        const auto log_factory_identity = [factory, observed](const char* event) {
            Microsoft::WRL::ComPtr<IDirect3D9> recovered_factory;
            Microsoft::WRL::ComPtr<IUnknown> expected_identity;
            Microsoft::WRL::ComPtr<IUnknown> recovered_identity;
            const HRESULT recovered_result = observed->GetDirect3D(&recovered_factory);
            const bool matches = factory &&
                SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&expected_identity))) &&
                SUCCEEDED(recovered_result) && recovered_factory &&
                SUCCEEDED(recovered_factory->QueryInterface(IID_PPV_ARGS(&recovered_identity))) &&
                expected_identity.Get() == recovered_identity.Get();
            std::ostringstream line;
            line << "native_stereo_startup_event: event=" << event
                 << ";thread_id=" << GetCurrentThreadId()
                 << ";factory=0x" << std::hex
                 << reinterpret_cast<std::uintptr_t>(factory)
                 << ";recovered=0x"
                 << reinterpret_cast<std::uintptr_t>(recovered_factory.Get())
                 << ";device=0x" << reinterpret_cast<std::uintptr_t>(observed)
                 << ";hr=0x" << static_cast<unsigned long>(recovered_result)
                 << std::dec << ";matches=" << (matches ? "true" : "false");
            LogLine(line.str());
        };
        log_factory_identity("factory_native_identity");
        IDirect3DDevice9* replacement = observed;
        ResetFrameLumaProbes();
        (void)RetireIndependentBank(g_stereo, "device_replacement");
        if(g_resource_probe_enabled.load(std::memory_order_acquire))
            g_resource_probe_resets.fetch_add(1,std::memory_order_acq_rel);
        HWND root = focus_window ? GetAncestor(focus_window, GA_ROOT) : nullptr;
        if (!root) root = focus_window;
        g_stereo.game_window.store(
            reinterpret_cast<std::uintptr_t>(root), std::memory_order_release);
        replacement->AddRef();
        IDirect3DDevice9* previous = nullptr;
        {
            std::lock_guard lock(g_stereo.device_mutex);
            previous = g_stereo.device;
            g_stereo.device = replacement;
        }
        const std::uint64_t generation =
            g_stereo.device_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
        const bool is_d3d9ex = returned_ex;
        g_stereo.flat_capture_active.store(false, std::memory_order_release);
        g_stereo.stereo_queue.Invalidate();
        if (previous) previous->Release();
        const bool legacy_compat = is_d3d9ex &&
            cojvr::games::call_of_juarez::InstallD3D9ExLegacyTextureCompatibility(
                replacement,
                cojvr::games::call_of_juarez::LegacyTextureCompatibilityCallbacks{
                    .event = &LegacyTextureEvent,
                    .resource_destroy = &ResourceDestroyEvent,
                });
        const cojvr::backends::d3d9::DeviceHookCallbacks device_callbacks{
            .before_present = &BeforeDevicePresent,
            .after_present = &AfterDevicePresent,
            .before_reset = &BeforeDeviceReset,
            .after_reset = &AfterDeviceReset,
            .com_trace = &DeviceComTrace,
            .method_trace = &DeviceMethodTrace,
            .get_direct3d_factory = factory,
            .get_direct3d_device = replacement,
            .resource_probe_event = (g_resource_probe_enabled.load() || g_stereo.fixed_outputs.enabled)
                ? &ResourceProbeEvent : nullptr,
            .resource_probe_call = g_resource_probe_enabled.load() ? &ResourceProbeCall : nullptr,
            .allow_reset = (g_stereo.fixed_outputs.enabled || g_stereo.luma_enabled) ? &AllowDeviceReset : nullptr,
            .after_reset_ex = g_stereo.fixed_outputs.enabled ? &AfterDeviceReset : nullptr,
        };
        const auto device_hook =
            cojvr::backends::d3d9::InstallDeviceVtableHookDetailed(
                replacement, device_callbacks);
        log_factory_identity("factory_identity");
        const cojvr::backends::d3d9::SwapChainHookCallbacks swapchain_callbacks{
            .before_present = &BeforeSwapChainPresent,
            .com_trace = &SwapChainComTrace,
            .method_trace = &SwapChainMethodTrace,
        };
        const auto swapchain_hook =
            cojvr::backends::d3d9::InstallSwapChainVtableHookDetailed(
                replacement, swapchain_callbacks);
        {
            std::ostringstream hook_line;
            hook_line << "native_stereo_startup_event: event=device_hook_installed"
                      << ";thread_id=" << GetCurrentThreadId()
                      << ";device=0x" << std::hex
                      << reinterpret_cast<std::uintptr_t>(replacement) << std::dec
                      << ";generation=" << generation
                      << ";result=" << static_cast<unsigned>(device_hook.result)
                      << ";modified_slots=" << device_hook.modified_slots;
            LogLine(hook_line.str());
        }
        {
            std::ostringstream hook_line;
            hook_line << "native_stereo_startup_event: event=swapchain_hook_installed"
                      << ";thread_id=" << GetCurrentThreadId()
                      << ";device=0x" << std::hex
                      << reinterpret_cast<std::uintptr_t>(replacement) << std::dec
                      << ";generation=" << generation
                      << ";result=" << static_cast<unsigned>(swapchain_hook.result)
                      << ";modified_slots=" << swapchain_hook.modified_slots;
            LogLine(hook_line.str());
        }
        {
            std::ostringstream observed_line;
            observed_line << "native_stereo_startup_event: event=device_observed"
                          << ";thread_id=" << GetCurrentThreadId()
                          << ";device=0x" << std::hex
                          << reinterpret_cast<std::uintptr_t>(replacement) << std::dec
                          << ";generation=" << generation
                          << ";device_api="
                          << (is_d3d9ex ? "d3d9ex" : "classic_d3d9_fallback")
                          << ";legacy_texture_compat="
                          << (legacy_compat ? "installed" : "unavailable");
            LogLine(observed_line.str());
        }
        std::ostringstream line;
        line << "native_stereo_device: status=observed device=0x" << std::hex
             << reinterpret_cast<std::uintptr_t>(replacement) << std::dec
             << " generation=" << generation
             << " device_api=" << (is_d3d9ex ? "d3d9ex" : "classic_d3d9_fallback")
             << " device_present_hook="
             << ((device_hook.result == cojvr::backends::d3d9::HookRegistryResult::Installed ||
                     device_hook.result == cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled)
                     ? "installed"
                     : "failed")
             << " swapchain_present_hook="
             << ((swapchain_hook.result == cojvr::backends::d3d9::HookRegistryResult::Installed ||
                     swapchain_hook.result == cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled)
                     ? "installed"
                     : "failed");
        LogLine(line.str());
        StartPresentHookWorker();
    } catch (...) {
    }
}

void InvalidateStereoFrame(void* context) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return;
    InvalidateSourceAuthority(*state);
    state->presenter.PublishGameplayFeedbackContext(false);
}

bool BeginStereoFrame(
    void* context,
    cojvr::games::call_of_juarez::CameraStereoFrameSample& sample) noexcept {
    sample = {};
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return false;
    state->source_capture_admission.BeginFrame();
    if (!state->runtime_ready ||
        g_finalization_started.load(std::memory_order_acquire) || !state->presenter.running() ||
        !PaceRenderFrame(state)) {
        InvalidateSourceAuthority(*state);
        state->presenter.PublishGameplayFeedbackContext(false);
        return false;
    }
    if (state->fixed_outputs.enabled) {
        const auto readiness = state->presenter.GetFixedOutputReadiness();
        cojvr::games::call_of_juarez::CoJIndependentResolutionRecipe recipe{};
        const bool eligible = readiness.state == cojvr::backends::openvr::FixedOutputState::ready &&
            readiness.theater_capture_initialized && !readiness.native_content_failed &&
            state->device_generation.load()>state->reset_suspended_generation.load() &&
            !state->independent_bank_faulted.load(std::memory_order_acquire) &&
            !state->independent_render.active() && !state->independent_render.restoration_pending() &&
            state->capture_readback_enabled.load(std::memory_order_acquire) &&
            cojvr::games::call_of_juarez::BuildCoJIndependentResolutionRecipe(
                cojvr::games::call_of_juarez::ObserveInstalledCoJPostprocessResources(),
                state->fixed_outputs.native_height, recipe);
        const bool previously_blocked = state->independent_admission_blocked.exchange(
            !eligible, std::memory_order_acq_rel);
        if (!eligible) {
            if (!previously_blocked) InvalidateSourceAuthority(*state);
            state->presenter.PublishGameplayFeedbackContext(false);
            return false; // Natural full wrapper supplies a fresh theater source.
        }
    }
    bool timer_frozen = false;
    std::string timer_error;
    const bool timer_valid = cojvr::games::call_of_juarez::ObserveCameraGameTimerFrozen(
        timer_frozen, &timer_error);
    cojvr::games::call_of_juarez::CoJPresentationOwner presentation{};
    const bool presentation_valid=cojvr::games::call_of_juarez::ObserveCameraGamePresentationOwner(
        presentation);
    const bool cinematic_immersive=presentation_valid && presentation.immersive_movie_allowed() &&
        cojvr::runtime::CurrentCinematicMode()==cojvr::runtime::CinematicsMode::immersive;
    const bool settings_captured=cojvr::runtime::SettingsInputCaptured();
    if (!presentation_valid || (presentation.blocked() && !cinematic_immersive)) {
        ObserveStereoOwner(*state, presentation, presentation_valid, settings_captured, cinematic_immersive);
        state->presenter.PublishGameplayFeedbackContext(false);
        state->stereo_queue.Invalidate();
        return false;
    }
    cojvr::backends::openvr::OpenVrTrackingSample tracking{};
    if (!state->presenter.LatestTracking(tracking) ||
        !tracking.pose.orientation_valid || !tracking.pose.position_valid ||
        tracking.sequence == 0) {
        state->stereo_queue.Invalidate();
        state->presenter.PublishGameplayFeedbackContext(false);
        return false;
    }
    if (state->flat_ui_select_release_required) {
        if (tracking.ui_select_left || tracking.ui_select_right || tracking.ui_accept) {
            state->presenter.PublishGameplayFeedbackContext(false);
            state->stereo_queue.Invalidate();
            return false;
        }
        state->flat_ui_select_release_required = false;
        state->flat_loading_resume_pending = false;
        CameraEvent(
            "blocking_ui_resume",
            "observed",
            "game_timer_valid=true;game_timer_frozen=false;trigger_released=true;route=BeginStereoFrame");
    }

    const bool effective_settings = settings_captured || tracking.vr_settings_input_captured;
    state->frame_settings_captured.store(effective_settings, std::memory_order_release);
    const auto frame_epoch = ObserveStereoOwner(*state, presentation, presentation_valid,
        effective_settings, cinematic_immersive);
    state->frame_owner_epoch.store(frame_epoch, std::memory_order_release);
    state->presenter.PublishGameplayFeedbackContext(timer_valid && !timer_frozen &&
        !presentation.blocked() && !effective_settings);
    state->frame_feedback_context_token.store(
        state->presenter.GameplayFeedbackContextToken(), std::memory_order_release);
    // Passive routes retain only the successful queue's original 250 ms lease.
    state->stereo_queue.Admit(frame_epoch, GetTickCount64(), effective_settings, cinematic_immersive);
    cojvr::backends::d3d9::StereoCpuFrame completed{};
    const bool capture_enabled =
        state->capture_readback_enabled.load(std::memory_order_acquire);
    if (capture_enabled && state->capture.TryCollectReady(completed)) {
        const std::uint64_t sequence = completed.capture_sequence;
        const auto authorization = state->fixed_outputs.enabled ?
            FindSourceCapture(*state, sequence) : SourceCaptureAuthorization{};
        if ((!state->fixed_outputs.enabled || authorization.owner_epoch == frame_epoch) &&
            state->stereo_queue.Accept(sequence) && PublishCapturedFrame(
                *state, std::move(completed),
                cojvr::backends::d3d9::FramePresentationMode::native_stereo,
                "publish", authorization) && state->stereo_queue.Published(frame_epoch)) {
            if (state->flat_capture_active.exchange(false, std::memory_order_acq_rel))
                LogLine("native_stereo_flat_theater: status=leaving reason=native_stereo_active");
            if (ShouldLogStereoTiming(sequence)) {
                LogLine(
                    "native_stereo_producer_timing: status=published " +
                    std::string(state->capture.collect_description()));
            }
        }
    }
    sample.hmd_pose.pose = tracking.pose;
    sample.hmd_pose.sequence = tracking.sequence;
    state->luma_pose_sequence.store(tracking.sequence, std::memory_order_release);
    sample.native_gameplay_context_token =
        state->frame_feedback_context_token.load(std::memory_order_acquire);
    sample.left_controller = tracking.left_controller;
    sample.right_controller = tracking.right_controller;
    sample.left_aim = tracking.left_aim;
    sample.right_aim = tracking.right_aim;
    sample.cinematic_immersive = cinematic_immersive;
    sample.settings_input_captured = effective_settings;
    sample.gameplay = (cinematic_immersive || effective_settings) ?
        cojvr::runtime::GameplayInputState{} : tracking.gameplay;
    sample.left_fingers = tracking.left_fingers;
    sample.right_fingers = tracking.right_fingers;
    sample.recenter_requested = tracking.recenter_requested;
    sample.objectives_requested = tracking.objectives_requested;
    sample.ui_select_left = tracking.ui_select_left;
    sample.ui_select_right = tracking.ui_select_right;
    sample.ui_select_left_pressed = tracking.ui_select_left_pressed;
    sample.ui_select_right_pressed = tracking.ui_select_right_pressed;
    sample.ui_accept = tracking.ui_accept;
    sample.ui_back = tracking.ui_back;
    sample.pause_pressed = tracking.pause_pressed;
    sample.ui_accept_pressed = tracking.ui_accept_pressed;
    sample.ui_back_pressed = tracking.ui_back_pressed;
    if (effective_settings || cinematic_immersive) {
        sample.objectives_requested = false;
        sample.ui_select_left = sample.ui_select_right = false;
        sample.ui_select_left_pressed = sample.ui_select_right_pressed = false;
        sample.ui_accept = sample.ui_back = false;
        sample.pause_pressed = sample.ui_accept_pressed = sample.ui_back_pressed = false;
        // Settings retain rigid tracked-hand visuals; cinematics retain only
        // the natural native visuals. Neither route receives gameplay input.
        if (cinematic_immersive) {
            sample.left_fingers = {}; sample.right_fingers = {};
        }
    }
    if (tracking.recenter_requested) {
        LogLine("openvr_input_event: action=recenter result=pressed source=global_action owner=presenter_thread");
    }
    sample.eyes = state->eyes;
    return true;
}

bool BeginIndependentEyeRender(void* context, const cojvr::runtime::Eye eye,
    const std::uint64_t frame_sequence) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state || !state->fixed_outputs.enabled) return false;
    const auto readiness = state->presenter.GetFixedOutputReadiness();
    if (readiness.state != cojvr::backends::openvr::FixedOutputState::ready ||
        !readiness.theater_capture_initialized || readiness.native_content_failed ||
        state->device_generation.load()<=state->reset_suspended_generation.load() ||
        state->independent_bank_faulted.load(std::memory_order_acquire) ||
        state->independent_render.active() || state->independent_render.restoration_pending()) return false;
    Microsoft::WRL::ComPtr<IDirect3DDevice9> device;
    device.Attach(RetainCurrentDevice());
    if (!device) return false;
    const auto generation = state->device_generation.load(std::memory_order_acquire);
    if (!generation) return false;
    const auto owner_epoch = state->frame_owner_epoch.load(std::memory_order_acquire);
    SourceCaptureAuthorization authorization{};
    if (eye == cojvr::runtime::Eye::left) {
        if (!state->stereo_queue.BindCapture(owner_epoch, frame_sequence)) return false;
        authorization = AuthorizeSourceCapture(*state, frame_sequence, owner_epoch, generation,
            device.Get(), cojvr::backends::d3d9::FramePresentationMode::native_stereo);
    } else {
        authorization = FindSourceCapture(*state, frame_sequence);
        if (!owner_epoch || state->stereo_queue.CaptureEpoch(frame_sequence) != owner_epoch ||
            authorization.owner_epoch != owner_epoch || !SourceCaptureCurrent(*state, authorization)) return false;
    }
    if (state->independent_render.payload_bytes() &&
        state->independent_history_epoch != authorization.source_epoch &&
        !state->independent_render.ClearHistories()) {
        IndependentBankFault(*state, "clear_histories");
        return false;
    }
    state->native_render_thread.store(GetCurrentThreadId(), std::memory_order_release);
    state->independent_frame = frame_sequence;
    state->independent_eye = eye;
    state->independent_generation = generation;
    state->independent_source_epoch = authorization.source_epoch;
    // Installed observation is admitted by exact executable/engine SHA identity;
    // the adapter additionally checks the complete native resource family.
    if(!state->capture.PrepareSharedOnly(device.Get(),state->fixed_outputs.native_width,
        state->fixed_outputs.native_height,static_cast<D3DFORMAT>(state->fixed_outputs.native_d3d_format),generation)){
        IndependentBankFault(*state,"prepare_shared_capture");return false;
    }
    if (!state->independent_render.Begin(device.Get(), generation,
            cojvr::games::call_of_juarez::ObserveInstalledCoJPostprocessResources(),
            state->fixed_outputs.native_height, eye)) {
        if (state->independent_render.last_error() == "awaiting natural queue completion") {
            InvalidateSourceAuthority(*state);
            state->presenter.PublishGameplayFeedbackContext(false);
        } else {
            IndependentBankFault(*state, "begin");
        }
        return false;
    }
    state->independent_history_epoch = authorization.source_epoch;
    return true;
}

bool EndIndependentEyeRender(void* context, const cojvr::runtime::Eye eye,
    const std::uint64_t frame_sequence) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return false;
    // A rejected Begin may have made no mutation, or completed its own rollback.
    // There is no owned End transaction in that case and no new fault to latch.
    if (!state->independent_render.active() && !state->independent_render.restoration_pending())
        return true;
    const bool matching = state->independent_frame == frame_sequence && state->independent_eye == eye;
    // Never short-circuit End: capture/owner failure still owes native rollback.
    const bool ended = state->independent_render.End();
    if (!ended || !matching) IndependentBankFault(*state, "end");
    // Eye validity is independent from rollback safety. The optional pending
    // query lets the camera reject this eye yet safely replay the natural view.
    return ended && matching;
}

bool IndependentEyeRestorationPending(void* context) noexcept {
    const auto* state = static_cast<NativeStereoState*>(context);
    return state && state->independent_render.restoration_pending();
}

bool MirrorIndependentEyePair(void* context, const std::uint64_t frame_sequence) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state || state->independent_bank_faulted.load(std::memory_order_acquire) ||
        state->independent_render.active() || state->independent_render.restoration_pending() ||
        state->independent_frame != frame_sequence || state->independent_eye != cojvr::runtime::Eye::right ||
        !state->stereo_queue.CaptureEpoch(frame_sequence) ||
        !SourceCaptureCurrent(*state, FindSourceCapture(*state, frame_sequence))) return false;
    // The game adapter retains the ORIGINAL final desktop surface at Begin.
    // It owns the aspect-preserving mirror after native-field/COM restoration.
    if (!state->independent_render.Mirror()) {
        IndependentBankFault(*state, "mirror");
        // Mirror has its own saved COM state after the eye scope ended. Attempt
        // restoration here too. The caller must retain an End barrier if it
        // remains pending before admitting any natural fallback render.
        (void)EndIndependentEyeRender(context, cojvr::runtime::Eye::right, frame_sequence);
        return false;
    }
    return true;
}

bool CaptureStereoEye(
    void* context,
    const cojvr::runtime::Eye eye,
    const std::uint64_t frame_sequence,
    std::uint64_t* content_hash) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state || !content_hash) return false;
    state->native_render_thread.store(GetCurrentThreadId(), std::memory_order_release);
    *content_hash = 0;
    if (!state->capture_readback_enabled.load(std::memory_order_acquire)) return true;
    if (eye == cojvr::runtime::Eye::left && !state->fixed_outputs.enabled) {
        if (!state->stereo_queue.BindCapture(
                state->frame_owner_epoch.load(std::memory_order_acquire), frame_sequence)) return false;
    } else if (!state->stereo_queue.CaptureEpoch(frame_sequence)) {
        return false;
    }
    IDirect3DDevice9* device = RetainCurrentDevice();
    if (!device) {
        LogTransportFailure(frame_sequence, "capture", "D3D9 device unavailable");
        return false;
    }
    const std::uint64_t generation =
        state->device_generation.load(std::memory_order_acquire);
    if (state->fixed_outputs.enabled) {
        const auto authorization = FindSourceCapture(*state, frame_sequence);
        Microsoft::WRL::ComPtr<IDirect3DSurface9> target;
        D3DSURFACE_DESC description{};
        const bool valid = state->independent_render.active() &&
            state->independent_frame == frame_sequence && state->independent_eye == eye &&
            state->independent_generation == generation &&
            state->independent_source_epoch == authorization.source_epoch &&
            SourceCaptureCurrent(*state, authorization) &&
            SUCCEEDED(device->GetRenderTarget(0, &target)) && target &&
            SUCCEEDED(target->GetDesc(&description)) &&
            description.Width == state->fixed_outputs.native_width &&
            description.Height == state->fixed_outputs.native_height &&
            description.Format == state->fixed_outputs.native_d3d_format &&
            description.MultiSampleType == D3DMULTISAMPLE_NONE;
        if (!valid) {
            device->Release();
            IndependentBankFault(*state, "capture_source");
            return false;
        }
    }
    const std::uintptr_t device_id = reinterpret_cast<std::uintptr_t>(device);
    if (state->luma_enabled) {
        Microsoft::WRL::ComPtr<IDirect3DSurface9> source;
        if (SUCCEEDED(device->GetRenderTarget(0, &source)))
            QueueFrameLumaProbe(device, source.Get(), eye == cojvr::runtime::Eye::left ? 0 : 1, frame_sequence);
    }
    const bool captured = state->capture.CaptureEye(
        device, eye, frame_sequence, generation);
    device->Release();
    if (captured && state->fixed_outputs.enabled && !state->capture.gpu_resident_active()) {
        IndependentBankFault(*state, "capture_requires_native_shared_transport");
        return false;
    }
    if (captured) {
        std::uint64_t previously_logged =
            state->capture_resources_generation_logged.load(std::memory_order_acquire);
        if (generation != 0 && previously_logged != generation &&
            state->capture_resources_generation_logged.compare_exchange_strong(
                previously_logged, generation,
                std::memory_order_acq_rel)) {
            try {
                std::ostringstream line;
                line << "native_stereo_startup_event: event=capture_resources_created"
                     << ";thread_id=" << GetCurrentThreadId()
                     << ";device=0x" << std::hex << device_id << std::dec
                     << ";generation=" << generation
                     << ";frame_sequence=" << frame_sequence
                     << ";slot=all"
                     << ";detail=" << state->capture.capture_description();
                LogLine(line.str());
            } catch (...) {
            }
        }
        const std::size_t eye_index = eye == cojvr::runtime::Eye::left ? 0U : 1U;
        if (!state->capture_source_logged[eye_index]) {
            state->capture_source_logged[eye_index] = true;
            LogLine(
                std::string("native_stereo_capture: status=source eye=") +
                (eye == cojvr::runtime::Eye::left ? "left " : "right ") +
                std::string(state->capture.capture_description()));
        }
        if (ShouldLogStereoTiming(frame_sequence)) {
            std::ostringstream line;
            line << "native_stereo_capture_timing: status=ok frame_sequence="
                 << frame_sequence << " eye="
                 << (eye == cojvr::runtime::Eye::left ? "left " : "right ")
                 << state->capture.capture_description();
            if (state->fixed_outputs.enabled) {
                const auto evidence = state->independent_render.evidence();
                MEMORYSTATUSEX memory{sizeof(memory)};
                PROCESS_MEMORY_COUNTERS_EX process{};
                process.cb=sizeof(process);
                (void)GlobalMemoryStatusEx(&memory);
                (void)GetProcessMemoryInfo(GetCurrentProcess(),
                    reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process),sizeof(process));
                line << ";private_bank_payload_bytes=" << state->independent_render.payload_bytes()
                     << ";available_va_bytes=" << memory.ullAvailVirtual
                     << ";process_private_bytes=" << process.PrivateUsage;
                line << ";geometry_draws=" << evidence.draws
                     << ";full_extent_depth_draws=" << evidence.full_extent_depth_draws
                     << ";geometry_color_width=" << evidence.color_width
                     << ";geometry_color_height=" << evidence.color_height
                     << ";geometry_depth_width=" << evidence.depth_width
                     << ";geometry_depth_height=" << evidence.depth_height
                     << ";geometry_viewport_width=" << evidence.viewport_width
                     << ";geometry_viewport_height=" << evidence.viewport_height;
            }
            LogLine(line.str());
        }
    }
    if (!captured) {
        LogTransportFailure(frame_sequence, "capture", state->capture.last_error());
        if (state->fixed_outputs.enabled) InvalidateSourceAuthority(*state);
    }
    return captured;
}

void PublishReloadFeedbackOwner(void* context, std::uint64_t token) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (state) state->presenter.PublishReloadFeedbackOwner(token);
}

void ObserveReloadDepth(void* context,cojvr::runtime::Eye eye,std::uint64_t frame_sequence) noexcept {
    auto* state=static_cast<NativeStereoState*>(context);
    if(!state)return;
    Microsoft::WRL::ComPtr<IDirect3DDevice9> device;
    device.Attach(RetainCurrentDevice());
    if(!device)return;
    const auto depth=cojvr::backends::d3d9::ObserveD3D9Depth(device.Get());
    try{
        std::ostringstream line;
        line<<"manual_reload_depth: frame_sequence="<<frame_sequence
            <<";eye="<<(eye==cojvr::runtime::Eye::left?"left":"right")
            <<";generation="<<state->device_generation.load(std::memory_order_acquire)
            <<";device=0x"<<std::hex<<reinterpret_cast<std::uintptr_t>(device.Get())
            <<";target=0x"<<depth.target_identity<<";depth=0x"<<depth.depth_identity<<std::dec
            <<";target_valid="<<depth.target_valid<<";depth_valid="<<depth.depth_valid
            <<";state_valid="<<depth.state_valid<<";layout_matches="<<depth.layout_matches
            <<";target_width="<<depth.target.Width<<";target_height="<<depth.target.Height
            <<";target_format="<<depth.target.Format<<";depth_width="<<depth.depth.Width
            <<";target_msaa="<<depth.target.MultiSampleType<<";target_quality="<<depth.target.MultiSampleQuality
            <<";depth_height="<<depth.depth.Height<<";depth_format="<<depth.depth.Format
            <<";depth_msaa="<<depth.depth.MultiSampleType<<";depth_quality="<<depth.depth.MultiSampleQuality
            <<";viewport="<<depth.viewport.X<<','<<depth.viewport.Y<<','<<depth.viewport.Width<<','<<depth.viewport.Height
            <<";min_z="<<depth.viewport.MinZ<<";max_z="<<depth.viewport.MaxZ
            <<";z_enable="<<depth.z_enable<<";z_write="<<depth.z_write<<";z_func="<<depth.z_function
            <<";transforms_valid="<<depth.transforms_valid;
        const auto matrix=[&](const char* name,const D3DMATRIX& value){
            line<<';'<<name<<'=';
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){
                if(r||c)line<<',';
                line<<value.m[r][c];
            }
        };
        matrix("view",depth.view);matrix("projection",depth.projection);
        line<<";phase=full_eye_complete;read_only=true;depth_content=unverified;occlusion_admission=false";
        LogLine(line.str());
    }catch(...){}
}

bool SubmitStereoFrame(
    void* context,
    const std::uint64_t frame_sequence,
    const cojvr::runtime::PoseSample& render_hmd_pose,
    const cojvr::games::call_of_juarez::CameraStereoReticleOverlay& gameplay_reticle,
    const cojvr::runtime::StereoHudTextOverlay& hud_text) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return false;
    if (!state->capture_readback_enabled.load(std::memory_order_acquire)) return true;
    cojvr::backends::d3d9::StereoReticleOverlay capture_reticle{};
    capture_reticle.active = gameplay_reticle.active;
    capture_reticle.no_shoot = gameplay_reticle.no_shoot;
    capture_reticle.frame_sequence = gameplay_reticle.frame_sequence;
    for (std::size_t eye = 0; eye < capture_reticle.eyes.size(); ++eye) {
        capture_reticle.eyes[eye] = {
            .valid = gameplay_reticle.eyes[eye].valid,
            .u = gameplay_reticle.eyes[eye].u,
            .v = gameplay_reticle.eyes[eye].v,
        };
    }
    auto captured_hud_text = hud_text;
    captured_hud_text.feedback_context_token =
        state->frame_feedback_context_token.load(std::memory_order_acquire);
    const auto frame_epoch = state->stereo_queue.CaptureEpoch(frame_sequence);
    if (state->fixed_outputs.enabled) {
        const auto authorization = FindSourceCapture(*state, frame_sequence);
        if (state->independent_bank_faulted.load(std::memory_order_acquire) ||
            state->independent_render.active() || state->independent_render.restoration_pending() ||
            authorization.owner_epoch != frame_epoch || !SourceCaptureCurrent(*state, authorization)) {
            InvalidateSourceAuthority(*state);
            return false;
        }
    }
    cojvr::games::call_of_juarez::CoJPresentationOwner presentation{};
    const bool observed = cojvr::games::call_of_juarez::ObserveCameraGamePresentationOwner(presentation);
    const bool movie = observed && presentation.immersive_movie_allowed() &&
        cojvr::runtime::CurrentCinematicMode() == cojvr::runtime::CinematicsMode::immersive;
    const bool settings = cojvr::runtime::SettingsInputCaptured() ||
        state->frame_settings_captured.load(std::memory_order_acquire);
    if (!frame_epoch || ObserveStereoOwner(*state, presentation, observed, settings, movie) != frame_epoch)
        return false;
    const bool accepted = state->capture.EndFrame(
        frame_sequence, render_hmd_pose.pose, render_hmd_pose.sequence, capture_reticle, captured_hud_text);
    if (accepted && !state->stereo_queue.Queued(
            frame_epoch, frame_sequence, GetTickCount64())) {
        if (state->fixed_outputs.enabled) InvalidateSourceAuthority(*state);
        return false;
    }
    if (accepted && state->fixed_outputs.enabled)
        state->source_capture_admission.Queued(frame_sequence);
    if (!accepted) {
        LogTransportFailure(frame_sequence, "fence", state->capture.last_error());
        if (state->fixed_outputs.enabled) InvalidateSourceAuthority(*state);
    } else if (ShouldLogStereoTiming(frame_sequence)) {
        std::ostringstream line;
        line << "native_stereo_frame_queue: status=ok frame_sequence="
             << frame_sequence << " transport="
             << (state->capture.gpu_resident_active()
                     ? "d3d9ex_shared_texture_ring"
                     : "classic_d3d9_locked_systemmem_fallback");
        LogLine(line.str());
    }
    return accepted;
}

void SetCaptureReadbackEnabled(void* context, const bool enabled) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return;
    const bool previous = state->capture_readback_enabled.exchange(
        enabled, std::memory_order_acq_rel);
    if (previous == enabled) return;
    if (state->fixed_outputs.enabled) InvalidateSourceAuthority(*state);
    LogLine(std::string("native_stereo_diagnostic_capture: status=") +
        (enabled ? "enabled" : "disabled") +
        ";readback=" + (enabled ? "active" : "bypassed") +
        ";presenter=repeat_last_frame");
}

bool QueryDiagnosticCounters(
    void* context,
    cojvr::games::call_of_juarez::CameraStereoDiagnosticCounters& counters) noexcept {
    counters = {};
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return false;
    try {
        const auto capture = state->capture.stats();
        const auto presenter = state->presenter.stats();
        counters.frames_fenced = capture.frames_fenced;
        counters.frames_collected = capture.frames_collected;
        counters.frames_uploaded = presenter.frames_uploaded;
        counters.new_submissions = presenter.new_frame_submissions;
        counters.repeat_submissions = presenter.repeated_frame_submissions;
        return true;
    } catch (...) {
        return false;
    }
}

bool StartStereoRuntime() noexcept {
    g_stereo.frame_feedback_context_token.store(0, std::memory_order_release);
    std::string manifest_path;
    try {
        const auto manifest = GameDirectory() / L"cojvr_openvr_input" / L"actions.json";
        const std::u8string manifest_utf8 = manifest.u8string();
        manifest_path.assign(
            reinterpret_cast<const char*>(manifest_utf8.data()), manifest_utf8.size());
    } catch (...) {
        LogLine("openvr_input: status=unavailable error=manifest_path_exception");
    }
    if (!g_stereo.presenter.Start(
            manifest_path, &PresenterLog, nullptr, &FlatUiPointer, nullptr,
            g_stereo.fixed_outputs)) {
        LogLine(
            "native_stereo_runtime: status=unavailable owner=presenter_thread error=" +
            g_stereo.presenter.last_error());
        return false;
    }
    if (!g_stereo.presenter.EyeViews(g_stereo.eyes)) {
        LogLine("native_stereo_runtime: status=eye_config_failed owner=presenter_thread");
        InvalidateSourceAuthority(g_stereo);
        g_stereo.presenter.Stop();
        return false;
    }
    g_stereo.runtime_ready = true;
    try {
        std::ostringstream line;
        line << "native_stereo_runtime: status=started backend=openvr owner=presenter_thread"
             << " recommended_eye=" << g_stereo.eyes[0].width << 'x'
             << g_stereo.eyes[0].height
             << " left_eye_x=" << g_stereo.eyes[0].eye_to_head.position.x
             << " right_eye_x=" << g_stereo.eyes[1].eye_to_head.position.x
             << " pose_semantics=eye_to_head";
        LogLine(line.str());
    } catch (...) {
    }
    return true;
}

void Finalize() noexcept {
    if (g_finalization_started.exchange(true, std::memory_order_acq_rel)) return;
    InvalidateSourceAuthority(g_stereo);
    g_stereo.presenter.PublishGameplayFeedbackContext(false);
    g_stereo.frame_feedback_context_token.store(0, std::memory_order_release);
    LogLine("native_stereo_pre_exit: stage=begin boundary=CoJ.exe!DestroyGame");
    g_stereo.runtime_ready = false;
    StopPresentHookWorker();
    cojvr::games::call_of_juarez::ShutdownCameraProbe();
    (void)RetireIndependentBank(g_stereo, "pre_destroy_game");
    const bool legacy_texture_restored =
        cojvr::games::call_of_juarez::RestoreD3D9ExLegacyTextureCompatibility();
    LogLine(std::string("native_stereo_legacy_texture_hook: status=") +
        (legacy_texture_restored ? "restored" : "incomplete"));
    const bool device_restored =
        cojvr::backends::d3d9::RestoreAllDeviceVtableHooks();
    LogLine(std::string("native_stereo_device_hook: status=") +
        (device_restored ? "restored" : "incomplete"));
    const bool swapchain_restored =
        cojvr::backends::d3d9::RestoreAllSwapChainVtableHooks();
    LogLine(std::string("native_stereo_swapchain_hook: status=") +
        (swapchain_restored ? "restored" : "incomplete"));
    if (g_stereo.factory) {
        const bool restored =
            cojvr::backends::d3d9::RestoreFactoryVtableHook(g_stereo.factory);
        LogLine(std::string("native_stereo_factory_hook: status=") +
            (restored ? "restored" : "incomplete"));
    }
    LogLine("native_stereo_shutdown: stage=presenter_begin");
    g_stereo.presenter.Stop();
    NeutralizeFlatUiPointer("presenter_stopped");
    g_stereo.flat_ui_select_retry = {};
    const auto presenter_stop_stats = g_stereo.presenter.stats();
    LogLine(std::string("native_stereo_presenter_stop: shutdown_complete=") +
        (presenter_stop_stats.shutdown_complete ? "true" : "false"));
    LogLine("native_stereo_shutdown: stage=presenter_end");
    LogLine("native_stereo_shutdown: stage=capture_begin");
    g_stereo.capture.Shutdown();
    g_stereo.flat_capture.Shutdown();
    if (g_stereo.luma_enabled) {
        std::lock_guard lock(g_stereo.luma_mutex);
        for (std::size_t stream=0;stream<g_stereo.luma_channels.size();++stream) {
            const auto& channel=g_stereo.luma_channels[stream];
            try {
                std::ostringstream line;
                line << "native_stereo_luma_probe: status=summary;stream=" << stream
                     << ";queued=" << channel.probe.queued << ";completed=" << channel.probe.completed
                     << ";dropped=" << channel.probe.dropped << ";failures=" << channel.probe.failures
                     << ";disabled=" << (channel.disabled ? "true" : "false");
                LogLine(line.str());
            } catch (...) {}
        }
    }
    ResetFrameLumaProbes();
    LogLine("native_stereo_shutdown: stage=capture_end");
    if(g_stereo.fixed_outputs.enabled)LogLine(std::string("independent_eye_shutdown: restored=")+
        (g_stereo.independent_render.Shutdown()?"true":"false"));
    try {
        const auto capture_stats = g_stereo.capture.stats();
        const auto presenter_stats = g_stereo.presenter.stats();
        std::ostringstream line;
        line << "native_stereo_transport_summary: frames_fenced="
             << capture_stats.frames_fenced
             << ";frames_collected=" << capture_stats.frames_collected
             << ";capture_ring_drops=" << capture_stats.frames_dropped_no_slot
             << ";capture_query_not_ready=" << capture_stats.query_not_ready
             << ";capture_query_flushes=" << capture_stats.query_flushes
             << ";shared_frames_published=" << capture_stats.shared_frames_published
             << ";cpu_fallback_frames=" << capture_stats.cpu_fallback_frames
             << ";fallback_activations=" << capture_stats.fallback_activations
             << ";consumer_releases=" << capture_stats.consumer_releases
             << ";capture_ring_depth=" << capture_stats.ring_depth
             << ";capture_ring_depth_peak=" << capture_stats.ring_depth_peak
             << ";mailbox_published=" << presenter_stats.mailbox.published
             << ";mailbox_replaced=" << presenter_stats.mailbox.replaced_pending
             << ";frames_uploaded=" << presenter_stats.frames_uploaded
             << ";new_submissions=" << presenter_stats.new_frame_submissions
             << ";repeat_submissions=" << presenter_stats.repeated_frame_submissions
             << ";shared_frames_copied=" << presenter_stats.shared_frames_copied
             << ";shared_resources_opened=" << presenter_stats.shared_resources_opened
             << ";shared_copy_fences_completed="
             << presenter_stats.shared_copy_fences_completed
             << ";shared_pending_copy_fences="
             << presenter_stats.shared_pending_copy_fences
             << ";shared_pending_copy_fences_peak="
             << presenter_stats.shared_pending_copy_fences_peak
             << ";shared_open_failures=" << presenter_stats.shared_open_failures
             << ";shared_copy_failures=" << presenter_stats.shared_copy_failures
             << ";submit_failures=" << presenter_stats.submit_failures;
        LogLine(line.str());
    } catch (...) {
    }
    LogLine("native_stereo_runtime: status=stopped");
    // These extra factory/device references remain deliberately process-lifetime.
    // Capture resources and consumer leases were released above while their owners
    // were alive. Windows reclaims only the retained roots at process exit.
    {
        std::lock_guard lock(g_stereo.device_mutex);
        g_stereo.device = nullptr;
    }
    g_stereo.factory = nullptr;
    LogLine("native_stereo_d3d9_refs: status=process_lifetime_released_by_os");
    const bool exit_hook_restored = cojvr::games::call_of_juarez::RestoreGameShutdownHook();
    LogLine(std::string("native_stereo_pre_exit_hook: status=") +
        (exit_hook_restored ? "restored" : "incomplete"));
    LogLine("native_stereo_pre_exit: stage=end boundary=CoJ.exe!DestroyGame");
    LogLine("run_end: run_id=" + g_run_id);
    g_finalization_completed.store(true, std::memory_order_release);
}

void BeforeGameDestroy(void*) noexcept { Finalize(); }

void ReportIncompletePreExitShutdown() noexcept {
    if (g_finalization_completed.load(std::memory_order_acquire)) return;
    // Do not use the shared logger mutex: a terminated presenter may own it.
    // Report failure without joining threads, COM calls or promoting run_end.
    constexpr char message[] =
        "native_stereo_shutdown: status=incomplete reason=pre_exit_boundary_not_completed\r\n";
    HANDLE log = CreateFileW(g_process_exit_log_path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) return;
    DWORD written{};
    (void)WriteFile(log, message, sizeof(message) - 1, &written, nullptr);
    CloseHandle(log);
}

void ObserveHudBoundary(void* context,
    const cojvr::games::call_of_juarez::CoJHudBoundaryEvent& event,
    const std::uint64_t frame_sequence) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return;
    try {
        std::ostringstream line;
        line << "native_hud_boundary: stage="
             << cojvr::games::call_of_juarez::CoJHudBoundaryStageName(event.stage)
             << ";pass_sequence=" << event.pass_sequence
             << ";frame_sequence=" << frame_sequence
             << ";present_sequence=" << state->present_telemetry_sequence.load()
             << ";thread_id=" << GetCurrentThreadId()
             << ";owner=0x" << std::hex << reinterpret_cast<std::uintptr_t>(event.owner)
             << std::dec << ";option=" << event.option;
        // Query only on the observed native eye-render thread. Never wait for
        // the device owner, redirect a target, or replay native sprite traversal.
        std::unique_lock lock(state->device_mutex, std::try_to_lock);
        if (!lock.owns_lock() || !state->device || GetCurrentThreadId() !=
            state->native_render_thread.load(std::memory_order_acquire)) {
            line << ";target_observation=unavailable";
        } else {
            Microsoft::WRL::ComPtr<IDirect3DSurface9> target;
            D3DSURFACE_DESC desc{};
            D3DVIEWPORT9 viewport{};
            const HRESULT rt_result = state->device->GetRenderTarget(0, &target);
            const HRESULT desc_result = target ? target->GetDesc(&desc) : E_FAIL;
            const HRESULT viewport_result = state->device->GetViewport(&viewport);
            line << ";device=0x" << std::hex << reinterpret_cast<std::uintptr_t>(state->device)
                 << ";rt=0x" << reinterpret_cast<std::uintptr_t>(target.Get())
                 << ";rt_hr=0x" << static_cast<unsigned long>(rt_result)
                 << ";desc_hr=0x" << static_cast<unsigned long>(desc_result)
                 << ";viewport_hr=0x" << static_cast<unsigned long>(viewport_result)
                 << std::dec << ";generation=" << state->device_generation.load()
                 << ";size=" << desc.Width << ',' << desc.Height << ";format=" << desc.Format
                 << ";viewport=" << viewport.X << ',' << viewport.Y << ','
                 << viewport.Width << ',' << viewport.Height;
            const std::array<std::pair<D3DRENDERSTATETYPE, const char*>, 6> states{{
                {D3DRS_ALPHABLENDENABLE, "alpha_blend"}, {D3DRS_SRCBLEND, "src_blend"},
                {D3DRS_DESTBLEND, "dst_blend"}, {D3DRS_COLORWRITEENABLE, "color_write"},
                {D3DRS_SEPARATEALPHABLENDENABLE, "separate_alpha"}, {D3DRS_ZENABLE, "depth"}}};
            for (const auto& [key, name] : states) {
                DWORD value = 0;
                if (SUCCEEDED(state->device->GetRenderState(key, &value)))
                    line << ';' << name << '=' << value;
                else line << ';' << name << "=unavailable";
            }
        }
        if (lock.owns_lock()) lock.unlock();
        LogLine(line.str());
    } catch (...) { LogLine("native_hud_boundary: status=observation_exception"); }
}

void EnsureStarted() noexcept {
    try {
        std::call_once(g_start_once, [] {
            const std::string run_id = ReadRunManifestField("runId");
            const std::string build_manifest_id = ReadRunManifestField("buildManifestId");
            if (!run_id.empty()) g_run_id = run_id;
            std::ostringstream start;
            start << "run_start: run_id=" << g_run_id
                  << " build_manifest_id="
                  << (build_manifest_id.empty() ? "unbound" : build_manifest_id)
                  << " pid=" << GetCurrentProcessId();
            LogLine(start.str());
            (void)cojvr::runtime::ConfigureNativeRenderFaultReporter(
                GameDirectory()/(L"cojvr-render-fault-"+std::to_wstring(GetCurrentProcessId())+L".txt"),
                g_run_id,build_manifest_id);

            lstrcpynW(g_process_exit_log_path, LogPath().c_str(), 32768);
            std::atexit(ReportIncompletePreExitShutdown);
            const auto exit_hook = cojvr::games::call_of_juarez::InstallCurrentGameShutdownHook(
                &BeforeGameDestroy, nullptr);
            LogLine(std::string("native_stereo_pre_exit_hook: status=") +
                cojvr::games::call_of_juarez::GameShutdownHookStatusName(exit_hook));
            const bool exit_boundary_ready = exit_hook ==
                cojvr::games::call_of_juarez::GameShutdownHookStatus::installed;

            // Startup-only opt-in; runtime control edits cannot change the hook set.
            {
                std::ifstream input(GameDirectory()/L"cojvr-camera-control.json",std::ios::binary);
                std::string control(65537,'\0'); input.read(control.data(),static_cast<std::streamsize>(control.size()));
                control.resize(static_cast<std::size_t>(input.gcount()));
                cojvr::games::call_of_juarez::CameraProbeCommand command{};
                if(control.size()<=65536&&cojvr::games::call_of_juarez::ParseCameraProbeCommand(control,command,nullptr)) {
                    g_resource_probe_enabled.store(command.render_resource_probe_enabled);
                    g_stereo.luma_enabled = command.frame_luma_probe_enabled;
                    if (command.independent_eye_height == 1440 || command.independent_eye_height == 1920) {
                        g_stereo.fixed_outputs = {
                            .enabled = true,
                            .native_width = 2560,
                            .native_height = command.independent_eye_height,
                            .desktop_width = 2560,
                            .desktop_height = 1440,
                            .native_d3d_format = 21,
                            .max_output_bytes = 128ULL * 1024 * 1024,
                        };
                    }
                }
                LogLine(std::string("native_stereo_luma_probe_config: startup_enabled=") +
                    (g_stereo.luma_enabled ? "true" : "false") +
                    ";grid=16x16;ring_slots_per_stream=3;query_poll=no_flush;effects_unchanged=true;coverage=native_source_points");
                LogLine(std::string("independent_eye_config: startup_enabled=") +
                    (g_stereo.fixed_outputs.enabled ? "true" : "false") +
                    ";native_width=" + std::to_string(g_stereo.fixed_outputs.native_width) +
                    ";native_height=" + std::to_string(g_stereo.fixed_outputs.native_height) +
                    ";desktop=2560x1440;format=21;max_output_bytes=134217728;runtime_edits=ignored");
                LogLine(std::string("render_resource_probe_config: startup_enabled=")+
                    (g_resource_probe_enabled.load()?"true":"false")+" coverage=binding_state_only max_pairs=6 pairs_per_route=2 max_records_per_eye=4096 cpu_buffer_bytes_per_thread="+
                    std::to_string(sizeof(cojvr::backends::d3d9::D3D9RenderResourceRecord)*ResourceProbe::max_records)+
                    ";native_snapshot_bytes="+std::to_string(sizeof(cojvr::games::call_of_juarez::CoJPostprocessResourceSnapshot)));
            }
            cojvr::games::call_of_juarez::CameraStereoRuntimeCallbacks stereo_callbacks{};
            if (exit_boundary_ready && StartStereoRuntime()) {
                stereo_callbacks.context = &g_stereo;
                stereo_callbacks.begin_frame = &BeginStereoFrame;
                stereo_callbacks.capture_eye = &CaptureStereoEye;
                stereo_callbacks.invalidate_frame = &InvalidateStereoFrame;
                stereo_callbacks.submit_frame = &SubmitStereoFrame;
                stereo_callbacks.set_capture_readback_enabled = &SetCaptureReadbackEnabled;
                stereo_callbacks.diagnostic_counters = &QueryDiagnosticCounters;
                stereo_callbacks.observe_hud_boundary = &ObserveHudBoundary;
                stereo_callbacks.publish_reload_feedback_owner = &PublishReloadFeedbackOwner;
                stereo_callbacks.observe_reload_depth = &ObserveReloadDepth;
                if (g_stereo.fixed_outputs.enabled) {
                    stereo_callbacks.begin_eye_render = &BeginIndependentEyeRender;
                    stereo_callbacks.end_eye_render = &EndIndependentEyeRender;
                    stereo_callbacks.mirror_eye_pair = &MirrorIndependentEyePair;
                    stereo_callbacks.eye_render_restoration_pending = &IndependentEyeRestorationPending;
                    stereo_callbacks.observe_natural_render = &ObserveNaturalFrame;
                }
                if(g_resource_probe_enabled.load())stereo_callbacks.observe_render_pass = &ObserveRenderPass;
            }

            const auto control_path = GameDirectory() / L"cojvr-camera-control.json";
            const auto status = cojvr::games::call_of_juarez::InitializeCameraProbe(
                control_path, &CameraEvent, nullptr, stereo_callbacks);
            LogLine(
                std::string("camera_probe_bootstrap: status=") +
                cojvr::games::call_of_juarez::CameraProbeInstallStatusName(status) +
                " system_d3d9=" +
                (cojvr::backends::d3d9::IsExpectedSystemD3D9Module()
                    ? "expected" : "unexpected"));
        });
    } catch (...) {
        LogLine("native_stereo_bootstrap: status=exception");
    }
}

void ObserveFactory(IDirect3D9* factory) noexcept {
    if (!factory) return;
    try {
        if (!g_stereo.factory) {
            factory->AddRef();
            g_stereo.factory = factory;
        }
        const cojvr::backends::d3d9::FactoryHookCallbacks callbacks{
            .prefer_ex_device = [&] {
                IDirect3D9Ex* ex = nullptr;
                const bool available = SUCCEEDED(factory->QueryInterface(
                    IID_IDirect3D9Ex, reinterpret_cast<void**>(&ex))) && ex;
                if (ex) ex->Release();
                return available;
            }(),
            .before_create_device = &BeforeCreateDevice,
            .after_create_device = &AfterCreateDevice,
            .com_trace = &FactoryComTrace,
        };
        const bool installed =
            cojvr::backends::d3d9::InstallFactoryVtableHook(factory, callbacks);
        LogLine(std::string("native_stereo_factory_hook: status=") +
            (installed ? "installed" : "failed"));
    } catch (...) {
        LogLine("native_stereo_factory_hook: status=exception");
    }
}

} // namespace

extern "C" IDirect3D9* WINAPI Direct3DCreate9(const UINT sdk_version) {
    EnsureStarted();
    const auto create = cojvr::backends::d3d9::SystemDirect3DCreate9();
    IDirect3D9* classic_factory = create ? create(sdk_version) : nullptr;

    const auto create_ex = cojvr::backends::d3d9::SystemDirect3DCreate9Ex();
    IDirect3D9Ex* ex_factory = nullptr;
    const HRESULT ex_result = create_ex
        ? create_ex(sdk_version, &ex_factory)
        : E_NOINTERFACE;
    {
        std::ostringstream line;
        line << "native_stereo_startup_event: event=factory_ex_created"
             << ";thread_id=" << GetCurrentThreadId()
             << ";factory=0x" << std::hex
             << reinterpret_cast<std::uintptr_t>(ex_factory)
             << ";hr=0x" << static_cast<unsigned long>(ex_result) << std::dec;
        LogLine(line.str());
    }
    if (SUCCEEDED(ex_result) && ex_factory) {
        if (classic_factory) classic_factory->Release();
        LogLine(
            "native_stereo_d3d9_factory: status=d3d9ex_primary "
            "fallback=classic_create_device transport=d3d9ex_shared_texture_ring");
        ObserveFactory(ex_factory);
        return ex_factory;
    } else {
        LogLine(
            "native_stereo_d3d9_factory: status=classic_fallback "
            "reason=Direct3DCreate9Ex_unavailable_or_failed");
    }

    ObserveFactory(classic_factory);
    return classic_factory;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
