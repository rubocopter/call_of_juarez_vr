#include "games/call_of_juarez/camera_probe.hpp"

#include "backends/d3d9/factory_vtable_hook.hpp"
#include "backends/d3d9/openvr_stereo_readback.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "runtime/log.hpp"
#include "runtime/openvr_runtime.hpp"

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace {

std::once_flag g_start_once;
std::filesystem::path g_game_directory{};
std::string g_run_id{"unbound"};

struct NativeStereoState {
    cojvr::runtime::OpenVrRuntime runtime;
    cojvr::backends::d3d9::OpenVrStereoReadback readback;
    std::array<cojvr::runtime::EyeView, 2> eyes{};
    std::mutex device_mutex;
    IDirect3D9* factory = nullptr;
    IDirect3DDevice9* device = nullptr;
    std::atomic_uint64_t pose_sequence{0};
    std::atomic_uint64_t last_error_frame{0};
    std::array<bool, 2> capture_source_logged{};
    bool runtime_ready = false;
    bool input_ready = false;
    bool input_poll_error_logged = false;
};

NativeStereoState g_stereo;

std::filesystem::path GameDirectory() noexcept {
    try {
        if (!g_game_directory.empty()) return g_game_directory;
        wchar_t buffer[32768]{};
        const DWORD length = GetModuleFileNameW(
            nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
        if (length == 0 || length >= std::size(buffer)) return {};
        g_game_directory = std::filesystem::path(
            std::wstring_view(buffer, length)).parent_path();
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

void AfterCreateDevice(
    IDirect3D9*,
    UINT,
    D3DDEVTYPE,
    HWND,
    DWORD,
    D3DPRESENT_PARAMETERS*,
    IDirect3DDevice9** returned_device,
    const HRESULT result) noexcept {
    if (FAILED(result) || !returned_device || !*returned_device) return;
    try {
        IDirect3DDevice9* replacement = *returned_device;
        replacement->AddRef();
        IDirect3DDevice9* previous = nullptr;
        {
            std::lock_guard lock(g_stereo.device_mutex);
            previous = g_stereo.device;
            g_stereo.device = replacement;
        }
        if (previous) previous->Release();
        std::ostringstream line;
        line << "native_stereo_device: status=observed device=0x" << std::hex
             << reinterpret_cast<std::uintptr_t>(replacement);
        LogLine(line.str());
    } catch (...) {
    }
}

bool BeginStereoFrame(
    void* context,
    cojvr::games::call_of_juarez::CameraStereoFrameSample& sample) noexcept {
    sample = {};
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state || !state->runtime_ready) return false;
    cojvr::runtime::Pose pose{};
    if (!state->runtime.WaitForHmdPose(pose) || !pose.orientation_valid) return false;
    if (state->input_ready) {
        cojvr::runtime::OpenVrGlobalActions actions{};
        if (state->runtime.PollGlobalActions(actions)) {
            state->input_poll_error_logged = false;
            sample.recenter_requested = actions.recenter_requested;
            if (actions.recenter_requested) {
                LogLine("openvr_input_event: action=recenter result=pressed source=global_action");
            }
        } else if (!state->input_poll_error_logged) {
            state->input_poll_error_logged = true;
            LogLine(
                "openvr_input: status=poll_failed error=" +
                std::string(state->runtime.last_error()));
        }
    }
    sample.hmd_pose.pose = pose;
    sample.hmd_pose.sequence =
        state->pose_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
    sample.eyes = state->eyes;
    return true;
}

bool CaptureStereoEye(
    void* context,
    const cojvr::runtime::Eye eye,
    const std::uint64_t frame_sequence,
    std::uint64_t* content_hash) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state || !content_hash) return false;
    *content_hash = 0;
    IDirect3DDevice9* device = RetainCurrentDevice();
    if (!device) {
        LogTransportFailure(frame_sequence, "capture", "D3D9 device unavailable");
        return false;
    }
    const bool captured = state->readback.CaptureEye(
        device, eye, frame_sequence, *content_hash);
    device->Release();
    if (captured) {
        const std::size_t eye_index = eye == cojvr::runtime::Eye::left ? 0U : 1U;
        if (!state->capture_source_logged[eye_index]) {
            state->capture_source_logged[eye_index] = true;
            LogLine(
                std::string("native_stereo_capture: status=source eye=") +
                (eye == cojvr::runtime::Eye::left ? "left " : "right ") +
                std::string(state->readback.description()));
        }
        if (ShouldLogStereoTiming(frame_sequence)) {
            std::ostringstream line;
            line << "native_stereo_capture_timing: status=ok frame_sequence="
                 << frame_sequence << " eye="
                 << (eye == cojvr::runtime::Eye::left ? "left " : "right ")
                 << state->readback.description();
            LogLine(line.str());
        }
    }
    if (!captured) {
        LogTransportFailure(frame_sequence, "capture", state->readback.last_error());
    }
    return captured;
}

bool SubmitStereoFrame(void* context, const std::uint64_t frame_sequence) noexcept {
    auto* state = static_cast<NativeStereoState*>(context);
    if (!state) return false;
    const bool submitted = state->readback.Submit(state->runtime, frame_sequence);
    if (!submitted) {
        LogTransportFailure(frame_sequence, "submit", state->readback.last_error());
    } else if (ShouldLogStereoTiming(frame_sequence)) {
        std::ostringstream line;
        line << "native_stereo_submit_timing: status=ok frame_sequence="
             << frame_sequence << ' ' << state->readback.description();
        LogLine(line.str());
    }
    return submitted;
}

bool StartStereoRuntime() noexcept {
    if (!g_stereo.runtime.Initialize("Call of Juarez VR native stereo")) {
        std::ostringstream line;
        line << "native_stereo_runtime: status=unavailable result_code="
             << g_stereo.runtime.last_result_code()
             << " error=" << g_stereo.runtime.last_error();
        LogLine(line.str());
        return false;
    }
    if (!g_stereo.runtime.ReadEyeConfiguration(g_stereo.eyes)) {
        LogLine(
            "native_stereo_runtime: status=eye_config_failed error=" +
            std::string(g_stereo.runtime.last_error()));
        g_stereo.runtime.Shutdown();
        return false;
    }
    try {
        const auto manifest = GameDirectory() / L"cojvr_openvr_input" / L"actions.json";
        const std::u8string manifest_utf8 = manifest.u8string();
        const std::string manifest_path(
            reinterpret_cast<const char*>(manifest_utf8.data()), manifest_utf8.size());
        g_stereo.input_ready = g_stereo.runtime.InitializeGlobalActions(manifest_path);
        if (g_stereo.input_ready) {
            LogLine(
                "openvr_input: status=started action_set=/actions/global "
                "recenter=/actions/global/in/recenter binding=psvr2_sense_create");
        } else {
            LogLine(
                "openvr_input: status=unavailable error=" +
                std::string(g_stereo.runtime.last_error()));
        }
    } catch (...) {
        g_stereo.input_ready = false;
        LogLine("openvr_input: status=unavailable error=manifest_path_exception");
    }
    if (!g_stereo.readback.Initialize(g_stereo.runtime)) {
        LogLine(
            "native_stereo_runtime: status=transport_failed error=" +
            std::string(g_stereo.readback.last_error()));
        g_stereo.runtime.Shutdown();
        return false;
    }
    g_stereo.runtime_ready = true;
    try {
        std::ostringstream line;
        line << "native_stereo_runtime: status=started backend=openvr"
             << " recommended_eye=" << g_stereo.eyes[0].width << 'x'
             << g_stereo.eyes[0].height
             << " left_eye_x=" << g_stereo.eyes[0].pose.position.x
             << " right_eye_x=" << g_stereo.eyes[1].pose.position.x
             << " pose_semantics=eye_to_head";
        LogLine(line.str());
    } catch (...) {
    }
    return true;
}

void Finalize() noexcept {
    cojvr::games::call_of_juarez::ShutdownCameraProbe();
    if (g_stereo.factory) {
        const bool restored =
            cojvr::backends::d3d9::RestoreFactoryVtableHook(g_stereo.factory);
        LogLine(std::string("native_stereo_factory_hook: status=") +
            (restored ? "restored" : "incomplete"));
    }
    LogLine("native_stereo_shutdown: stage=readback_begin");
    g_stereo.readback.Shutdown();
    LogLine("native_stereo_shutdown: stage=readback_end");
    LogLine("native_stereo_shutdown: stage=runtime_begin");
    g_stereo.runtime.Shutdown();
    LogLine("native_stereo_shutdown: stage=runtime_end");
    g_stereo.runtime_ready = false;
    g_stereo.input_ready = false;
    g_stereo.input_poll_error_logged = false;
    LogLine("native_stereo_runtime: status=stopped");
    // These COM references are deliberately process-lifetime. The live game has shown
    // that releasing the retained D3D9 device during CRT teardown can terminate finalization
    // after hook restoration but before OpenVR shutdown/run_end. Windows reclaims the
    // remaining process references immediately after atexit completes.
    {
        std::lock_guard lock(g_stereo.device_mutex);
        g_stereo.device = nullptr;
    }
    g_stereo.factory = nullptr;
    LogLine("native_stereo_d3d9_refs: status=process_lifetime_released_by_os");
    LogLine("run_end: run_id=" + g_run_id);
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

            cojvr::games::call_of_juarez::CameraStereoRuntimeCallbacks stereo_callbacks{};
            if (StartStereoRuntime()) {
                stereo_callbacks.context = &g_stereo;
                stereo_callbacks.begin_frame = &BeginStereoFrame;
                stereo_callbacks.capture_eye = &CaptureStereoEye;
                stereo_callbacks.submit_frame = &SubmitStereoFrame;
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
            std::atexit(Finalize);
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
            .after_create_device = &AfterCreateDevice,
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
    IDirect3D9* factory = create ? create(sdk_version) : nullptr;
    ObserveFactory(factory);
    return factory;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
