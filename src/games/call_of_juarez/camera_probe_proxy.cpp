#include "games/call_of_juarez/camera_probe.hpp"

#include "backends/d3d9/system_d3d9.hpp"
#include "runtime/log.hpp"
#if defined(COJVR_CAMERA_HMD_OPENVR)
#include "runtime/openvr_runtime.hpp"
#endif

#include <windows.h>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace {

std::once_flag g_start_once;
std::filesystem::path g_game_directory{};
std::string g_run_id{"unbound"};

#if defined(COJVR_CAMERA_HMD_OPENVR)
class OpenVrCameraPoseSource final : public cojvr::runtime::PoseSource {
public:
    [[nodiscard]] bool Start() noexcept {
        try {
            Stop();
            {
                std::lock_guard lock(mutex_);
                latest_ = {};
                startup_complete_ = false;
                running_ = false;
                last_error_.clear();
                last_result_code_ = 0;
            }
            worker_ = std::jthread([this](const std::stop_token stop_token) {
                cojvr::runtime::OpenVrRuntime runtime;
                const bool initialized = runtime.Initialize("Call of Juarez VR Camera");
                {
                    std::lock_guard lock(mutex_);
                    startup_complete_ = true;
                    running_ = initialized;
                    if (!initialized) {
                        last_error_ = std::string(runtime.last_error());
                        last_result_code_ = runtime.last_result_code();
                    }
                }
                startup_condition_.notify_all();
                if (!initialized) return;

                while (!stop_token.stop_requested()) {
                    cojvr::runtime::Pose pose{};
                    if (runtime.WaitForHmdPose(pose)) {
                        std::lock_guard lock(mutex_);
                        latest_.pose = pose;
                        ++latest_.sequence;
                        last_error_.clear();
                        last_result_code_ = 0;
                        continue;
                    }
                    {
                        std::lock_guard lock(mutex_);
                        latest_.pose.orientation_valid = false;
                        latest_.pose.position_valid = false;
                        last_error_ = std::string(runtime.last_error());
                        last_result_code_ = runtime.last_result_code();
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                runtime.Shutdown();
                std::lock_guard lock(mutex_);
                running_ = false;
            });

            std::unique_lock lock(mutex_);
            startup_condition_.wait(lock, [this] { return startup_complete_; });
            return running_;
        } catch (...) {
            std::lock_guard lock(mutex_);
            startup_complete_ = true;
            running_ = false;
            last_error_ = "OpenVR pose-source startup raised an exception";
            last_result_code_ = -1;
            return false;
        }
    }

    void Stop() noexcept {
        if (!worker_.joinable()) return;
        worker_.request_stop();
        worker_.join();
        std::lock_guard lock(mutex_);
        running_ = false;
        latest_ = {};
    }

    [[nodiscard]] bool TryGetLatestPose(cojvr::runtime::PoseSample& sample) noexcept override {
        sample = {};
        std::lock_guard lock(mutex_);
        if (!running_ || latest_.sequence == 0 || !latest_.pose.orientation_valid) return false;
        sample = latest_;
        return true;
    }

    [[nodiscard]] std::string last_error() const {
        std::lock_guard lock(mutex_);
        return last_error_;
    }

    [[nodiscard]] std::int32_t last_result_code() const noexcept {
        std::lock_guard lock(mutex_);
        return last_result_code_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable startup_condition_;
    std::jthread worker_;
    cojvr::runtime::PoseSample latest_{};
    bool startup_complete_ = false;
    bool running_ = false;
    std::string last_error_;
    std::int32_t last_result_code_ = 0;
};

OpenVrCameraPoseSource g_hmd_pose_source;
#endif

std::filesystem::path GameDirectory() noexcept {
    try {
        if (!g_game_directory.empty()) return g_game_directory;
        wchar_t buffer[32768]{};
        const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
        if (length == 0 || length >= std::size(buffer)) return {};
        g_game_directory = std::filesystem::path(std::wstring_view(buffer, length)).parent_path();
        return g_game_directory;
    } catch (...) {
        return {};
    }
}

std::filesystem::path LogPath() noexcept {
    const auto directory = GameDirectory();
    return directory.empty() ? std::filesystem::path{} : directory / L"cojvr.log";
}

void LogLine(std::string_view line) noexcept {
    const auto path = LogPath();
    if (!path.empty()) cojvr::runtime::AppendLogLine(path, line);
}

std::string ReadJsonStringField(std::string_view text, std::string_view field) {
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

std::string ReadRunManifestField(std::string_view field) noexcept {
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

void Finalize() noexcept {
    cojvr::games::call_of_juarez::ShutdownCameraProbe();
#if defined(COJVR_CAMERA_HMD_OPENVR)
    g_hmd_pose_source.Stop();
    LogLine("camera_hmd_pose_source: status=stopped");
#endif
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

            const auto control_path = GameDirectory() / L"cojvr-camera-control.json";
#if defined(COJVR_CAMERA_HMD_OPENVR)
            cojvr::runtime::PoseSource* pose_source = nullptr;
            if (g_hmd_pose_source.Start()) {
                pose_source = &g_hmd_pose_source;
                LogLine("camera_hmd_pose_source: status=started backend=openvr tracking_space=standing");
            } else {
                std::ostringstream xr_error;
                xr_error << "camera_hmd_pose_source: status=unavailable backend=openvr result_code="
                         << g_hmd_pose_source.last_result_code()
                         << " error=" << g_hmd_pose_source.last_error();
                LogLine(xr_error.str());
            }
#else
            cojvr::runtime::PoseSource* pose_source = nullptr;
#endif
            const auto status = cojvr::games::call_of_juarez::InitializeCameraProbe(
                control_path, &CameraEvent, pose_source);
            LogLine(
                std::string("camera_probe_bootstrap: status=") +
                cojvr::games::call_of_juarez::CameraProbeInstallStatusName(status) +
                " system_d3d9=" +
                (cojvr::backends::d3d9::IsExpectedSystemD3D9Module() ? "expected" : "unexpected"));
            std::atexit(Finalize);
        });
    } catch (...) {
        LogLine("camera_probe_bootstrap: status=exception");
    }
}

} // namespace

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT sdk_version) {
    EnsureStarted();
    const auto create = cojvr::backends::d3d9::SystemDirect3DCreate9();
    return create ? create(sdk_version) : nullptr;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
