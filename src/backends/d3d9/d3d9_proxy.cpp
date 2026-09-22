#include "backends/d3d9/classic_readback_bridge.hpp"
#include "backends/d3d9/diagnostic_context.hpp"
#include "backends/d3d9/device_vtable_hook.hpp"
#include "backends/d3d9/d3d9ex_forwarder.hpp"
#include "backends/d3d9/factory_vtable_hook.hpp"
#include "backends/d3d9/swapchain_vtable_hook.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
#include "backends/d3d9/openvr_flat_bridge.hpp"
#endif

#include "runtime/game_id.hpp"
#include "runtime/host_identity.hpp"
#include "runtime/log.hpp"
#include "runtime/structured_telemetry.hpp"


#include <atomic>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_set>

namespace {

HMODULE g_proxy_module = nullptr;
std::once_flag g_identity_log_once;
std::once_flag g_telemetry_once;
std::once_flag g_present_log_once;
std::once_flag g_readback_capture_once;
std::atomic_bool g_readback_requested{false};
cojvr::backends::d3d9::D3D9DiagnosticContextRegistry g_diagnostic_contexts;
cojvr::runtime::StructuredRunTelemetry* g_telemetry = nullptr;
HANDLE g_observer_stop_event = nullptr;
HANDLE g_observer_thread = nullptr;
std::atomic_bool g_telemetry_finalized{false};
std::mutex g_current_context_mutex;
cojvr::runtime::TelemetryContext g_current_context{};
#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
std::once_flag g_openvr_flat_success_log_once;
std::once_flag g_openvr_flat_failure_log_once;
std::once_flag g_begin_scene_log_once;
std::once_flag g_end_scene_log_once;
std::atomic_uint64_t g_present_callbacks{0};
std::atomic_uint64_t g_present_callback_returns{0};
std::atomic_uint64_t g_begin_scene_callbacks{0};
std::atomic_uint64_t g_end_scene_callbacks{0};
std::atomic_uint64_t g_end_scene_callback_returns{0};
std::atomic_int g_openvr_runtime_state{0};
std::atomic_bool g_flat_bridge_failure_reported{false};
#endif

thread_local cojvr::runtime::TelemetryToken g_present_token{};
thread_local cojvr::runtime::TelemetryToken g_begin_scene_token{};
thread_local cojvr::runtime::TelemetryToken g_end_scene_token{};
thread_local cojvr::runtime::TelemetryToken g_reset_token{};
thread_local cojvr::runtime::TelemetryToken g_swapchain_present_token{};
thread_local cojvr::runtime::TelemetryContext g_callback_context{};
#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
thread_local cojvr::runtime::TelemetryToken g_capture_token{};
thread_local cojvr::runtime::TelemetryToken g_upload_token{};
thread_local cojvr::runtime::TelemetryToken g_wait_poses_token{};
thread_local cojvr::runtime::TelemetryToken g_submit_left_token{};
thread_local cojvr::runtime::TelemetryToken g_submit_right_token{};
#endif

bool D3D9ExBridgeRequested() noexcept {
    wchar_t value[16]{};
    const DWORD length = GetEnvironmentVariableW(
        L"COJVR_D3D9_EX_BRIDGE", value, static_cast<DWORD>(std::size(value)));
    if (length > 0 && length < std::size(value)) {
        const std::wstring_view text(value, length);
        if (text == L"1" || text == L"true" || text == L"TRUE" || text == L"on" || text == L"ON") {
            return true;
        }
    }

    try {
        wchar_t module_path[MAX_PATH]{};
        const DWORD module_length = GetModuleFileNameW(g_proxy_module, module_path, MAX_PATH);
        if (module_length == 0 || module_length >= MAX_PATH) return false;
        const std::filesystem::path path(std::wstring_view(module_path, module_length));
        return std::filesystem::exists(path.parent_path() / L".cojvr-d3d9-ex-bridge");
    } catch (...) {
        return false;
    }
}

bool ClassicReadbackRequested() noexcept {
#if defined(COJVR_D3D9_READBACK_ALWAYS_ON)
    return true;
#else
    return false;
#endif
}

#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
cojvr::backends::d3d9::OpenVrFlatBridge& OpenVrFlatBridgeInstance() {
    // Process-lifetime diagnostic object. Avoid OpenVR/COM teardown from DllMain.
    static auto* bridge = new cojvr::backends::d3d9::OpenVrFlatBridge();
    return *bridge;
}
#endif

std::filesystem::path LogPath() noexcept {
    try {
        wchar_t buffer[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(g_proxy_module, buffer, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return L"cojvr.log";
        std::filesystem::path path(std::wstring_view(buffer, length));
        return path.parent_path() / L"cojvr.log";
    } catch (...) {
        return L"cojvr.log";
    }
}

void LogLine(std::string_view line) noexcept {
    cojvr::runtime::AppendLogLine(LogPath(), line);
}

std::string ReadRunManifestField(std::string_view field) noexcept {
    try {
        wchar_t buffer[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(g_proxy_module, buffer, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return {};
        const std::filesystem::path module_path(std::wstring_view(buffer, length));
        std::ifstream input(module_path.parent_path() / L".cojvr-run.json", std::ios::binary);
        if (!input) return {};
        const std::string contents{
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        const std::string key = "\"" + std::string(field) + "\"";
        const std::size_t key_position = contents.find(key);
        if (key_position == std::string::npos) return {};
        const std::size_t colon = contents.find(':', key_position + key.size());
        if (colon == std::string::npos) return {};
        const std::size_t quote = contents.find('"', colon + 1);
        if (quote == std::string::npos) return {};
        const std::size_t end_quote = contents.find('"', quote + 1);
        if (end_quote == std::string::npos) return {};
        return contents.substr(quote + 1, end_quote - quote - 1);
    } catch (...) {
        return {};
    }
}

std::string ModulePathForAddress(void* address) noexcept {
    if (!address) return "null";
    try {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(address), &module) || !module) {
            return "unknown";
        }
        wchar_t module_path[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(module, module_path, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return "unknown";
        return std::filesystem::path(std::wstring_view(module_path, length)).string();
    } catch (...) {
        return "unknown";
    }
}

std::string ModuleNameForAddress(void* address) noexcept {
    const auto path = ModulePathForAddress(address);
    if (path == "null" || path == "unknown") return path;
    try {
        return std::filesystem::path(path).filename().string();
    } catch (...) {
        return "unknown";
    }
}

cojvr::runtime::TelemetryContext CurrentTelemetryContext() noexcept {
    try {
        std::lock_guard lock(g_current_context_mutex);
        return g_current_context;
    } catch (...) {
        return {};
    }
}

cojvr::runtime::TelemetryContext ToTelemetryContext(
    const cojvr::backends::d3d9::DeviceDiagnosticContext& context) noexcept {
    return cojvr::runtime::TelemetryContext{
        context.factory_id, context.device_id, context.swapchain_id, context.generation};
}

void SetCurrentTelemetryContext(const cojvr::runtime::TelemetryContext& context) noexcept {
    try {
        std::lock_guard lock(g_current_context_mutex);
        g_current_context = context;
    } catch (...) {
    }
}

cojvr::runtime::TelemetryContext ContextFor(IDirect3DDevice9* device) noexcept {
    const auto context = g_diagnostic_contexts.FindDevice(device);
    return context ? ToTelemetryContext(*context) : CurrentTelemetryContext();
}

const char* HookResultName(const cojvr::backends::d3d9::HookRegistryResult result) noexcept {
    using Result = cojvr::backends::d3d9::HookRegistryResult;
    switch (result) {
    case Result::Installed: return "installed";
    case Result::AlreadyInstalled: return "already_installed";
    case Result::ProtectionFailure: return "protection_failure";
    case Result::Conflict: return "conflict";
    case Result::RollbackIncomplete: return "rollback_incomplete";
    case Result::InvalidArgument: return "invalid_argument";
    }
    return "unknown";
}

std::string PointerText(const void* value) {
    std::ostringstream out;
    out << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(value);
    return out.str();
}

cojvr::backends::d3d9::HookDiagnostics AllHookDiagnostics() {
    auto diagnostics = cojvr::backends::d3d9::InspectAllFactoryVtableHooks();
    auto device = cojvr::backends::d3d9::InspectAllDeviceVtableHooks();
    auto swapchain = cojvr::backends::d3d9::InspectAllSwapChainVtableHooks();
    diagnostics.insert(diagnostics.end(), device.begin(), device.end());
    diagnostics.insert(diagnostics.end(), swapchain.begin(), swapchain.end());
    return diagnostics;
}

void EmitHookOutcome(
    const cojvr::backends::d3d9::HookRegistryOutcome& outcome,
    const cojvr::runtime::TelemetryContext& context,
    void** affected_vtable) noexcept {
    if (!g_telemetry) return;
    try {
        const bool successful =
            outcome.result == cojvr::backends::d3d9::HookRegistryResult::Installed ||
            outcome.result == cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled;
        const char* event = successful ? "hook_installed" :
            (outcome.result == cojvr::backends::d3d9::HookRegistryResult::Conflict
                ? "hook_conflict" : "hook_install_failed");
        bool emitted = false;
        for (const auto& slot : AllHookDiagnostics()) {
            if (affected_vtable && slot.vtable != affected_vtable) continue;
            std::ostringstream detail;
            detail << "interface=" << slot.interface_name
                << ";slot=" << slot.slot_name
                << ";index=" << slot.index
                << ";vtable=" << PointerText(slot.vtable)
                << ";install_result=" << HookResultName(outcome.result)
                << ";original_target=" << PointerText(slot.original)
                << ";replacement_target=" << PointerText(slot.replacement)
                << ";current_target=" << PointerText(slot.current)
                << ";original_module=" << ModuleNameForAddress(slot.original)
                << ";replacement_module=" << ModuleNameForAddress(slot.replacement)
                << ";current_module=" << ModuleNameForAddress(slot.current)
                << ";original_path=" << ModulePathForAddress(slot.original)
                << ";replacement_path=" << ModulePathForAddress(slot.replacement)
                << ";current_path=" << ModulePathForAddress(slot.current)
                << ";owned=" << (slot.owned ? "true" : "false");
            const std::string detail_text = detail.str();
            g_telemetry->Emit({
                .event = event,
                .context = context,
                .runtime_result = HookResultName(outcome.result),
                .detail = detail_text,
            });
            emitted = true;
        }
        if (!emitted) {
            std::ostringstream detail;
            detail << "vtable=" << PointerText(affected_vtable)
                << ";install_result=" << HookResultName(outcome.result)
                << ";modified_slots=" << outcome.modified_slots
                << ";ownership_record_retained="
                << (outcome.ownership_record_retained ? "true" : "false");
            const std::string detail_text = detail.str();
            g_telemetry->Emit({
                .event = event,
                .context = context,
                .runtime_result = HookResultName(outcome.result),
                .detail = detail_text,
            });
        }
    } catch (...) {
    }
}

DWORD WINAPI ObserveRunContinuity(LPVOID) noexcept {
    std::unordered_set<std::string> reported_losses;
    while (g_observer_stop_event &&
           WaitForSingleObject(g_observer_stop_event, 1000) == WAIT_TIMEOUT) {
        try {
            const auto devices = g_diagnostic_contexts.RegisteredDevices();
            const auto factories = g_diagnostic_contexts.RegisteredFactories();
            std::size_t total_slots = 0;
            std::size_t owned_slots = 0;
            for (const auto& slot : AllHookDiagnostics()) {
                ++total_slots;
                if (slot.owned) {
                    ++owned_slots;
                    continue;
                }
                const std::string key = PointerText(slot.vtable) + ":" +
                    std::to_string(slot.index) + ":" + PointerText(slot.current);
                if (!reported_losses.insert(key).second || !g_telemetry) continue;
                std::ostringstream out;
                out << "interface=" << slot.interface_name
                    << ";slot=" << slot.slot_name
                    << ";index=" << slot.index
                    << ";vtable=" << PointerText(slot.vtable)
                    << ";original_target=" << PointerText(slot.original)
                    << ";expected_target=" << PointerText(slot.replacement)
                    << ";current_target=" << PointerText(slot.current)
                    << ";original_module=" << ModuleNameForAddress(slot.original)
                    << ";expected_module=" << ModuleNameForAddress(slot.replacement)
                    << ";current_module=" << ModuleNameForAddress(slot.current)
                    << ";original_path=" << ModulePathForAddress(slot.original)
                    << ";expected_path=" << ModulePathForAddress(slot.replacement)
                    << ";current_path=" << ModulePathForAddress(slot.current);
                const std::string detail_text = out.str();
                const auto emit_loss = [&](const cojvr::runtime::TelemetryContext& context) {
                    g_telemetry->Emit({
                        .event = "hook_integrity_lost",
                        .context = context,
                        .runtime_result = "overwritten",
                        .detail = detail_text,
                    });
                };
                bool context_emitted = false;
                for (const auto& device : devices) {
                    if (device.device_vtable == slot.vtable ||
                        device.swapchain_vtable == slot.vtable) {
                        emit_loss(ToTelemetryContext(device));
                        context_emitted = true;
                    }
                }
                for (const auto& factory : factories) {
                    if (factory.vtable == slot.vtable) {
                        emit_loss({factory.factory_id, 0, 0, 0});
                        context_emitted = true;
                    }
                }
                if (!context_emitted) emit_loss(CurrentTelemetryContext());
                LogLine("d3d9 hook continuity: overwritten " + detail_text);
            }
            if (g_telemetry) {
                std::ostringstream detail;
                detail << "hook_slots=" << total_slots
                    << ";owned_hook_slots=" << owned_slots
                    << ";hooks_owned=" <<
                        (total_slots != 0 && total_slots == owned_slots ? "true" : "false");
                if (devices.empty()) {
                    g_telemetry->PeriodicSummary(CurrentTelemetryContext(), detail.str());
                } else {
                    for (const auto& device : devices) {
                        g_telemetry->PeriodicSummary(
                            ToTelemetryContext(device), detail.str());
                    }
                }
            }
        } catch (...) {
        }
    }
    return 0;
}

void FinalizeTelemetry() noexcept {
    if (g_telemetry_finalized.exchange(true, std::memory_order_acq_rel)) return;
    if (g_observer_stop_event) SetEvent(g_observer_stop_event);
    if (g_observer_thread) {
        WaitForSingleObject(g_observer_thread, 2000);
        CloseHandle(g_observer_thread);
        g_observer_thread = nullptr;
    }
    if (g_telemetry) g_telemetry->RunEnd(CurrentTelemetryContext(), "complete");
    if (g_observer_stop_event) {
        CloseHandle(g_observer_stop_event);
        g_observer_stop_event = nullptr;
    }
}

void EnsureTelemetry(std::string run_id, std::string build_manifest_id) noexcept {
    try {
        std::call_once(g_telemetry_once, [&] {
            if (run_id.empty()) run_id = "unbound";
            if (build_manifest_id.empty()) build_manifest_id = "unbound";
            g_telemetry = new cojvr::runtime::StructuredRunTelemetry(
                LogPath(), std::move(run_id), std::move(build_manifest_id));
            g_observer_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (g_observer_stop_event) {
                g_observer_thread = CreateThread(
                    nullptr, 0, ObserveRunContinuity, nullptr, 0, nullptr);
            }
            std::atexit(FinalizeTelemetry);
        });
    } catch (...) {
    }
}

void LogHostIdentityOnce() noexcept {
    try {
        std::call_once(g_identity_log_once, [] {
            const std::string run_id = ReadRunManifestField("runId");
            const std::string build_manifest_id = ReadRunManifestField("buildManifestId");
            EnsureTelemetry(run_id, build_manifest_id);
            std::ostringstream run_start;
            run_start << "run_start: run_id=" << (run_id.empty() ? "unbound" : run_id)
                      << " build_manifest_id="
                      << (build_manifest_id.empty() ? "unbound" : build_manifest_id)
                      << " pid=" << GetCurrentProcessId();
            LogLine(run_start.str());
            if (g_telemetry) {
                g_telemetry->Emit({
                    .event = "run_start",
                    .runtime_result = run_id.empty() || build_manifest_id.empty()
                        ? "unbound" : "bound",
                });
                const std::string deployment_detail =
                    "diagnostic_mode=" + ReadRunManifestField("diagnosticMode");
                g_telemetry->Emit({
                    .event = "build/deployment_identified",
                    .runtime_result = run_id.empty() || build_manifest_id.empty()
                        ? "unbound" : "verified_by_run_manifest",
                    .detail = deployment_detail,
                });
            }

            const auto identity = cojvr::runtime::InspectCurrentHost();
            if (!identity) {
                LogLine("d3d9 bootstrap: host identity unavailable");
                return;
            }

            std::ostringstream out;
            out << "d3d9 bootstrap: host="
                << cojvr::runtime::GameIdName(identity->filename_game)
                << " sha256=" << identity->sha256
                << " exact_build=" << (identity->IsKnownExactBuild() ? "known" : "unknown");
            const std::string host_detail = out.str();
            LogLine(host_detail);
            if (g_telemetry) {
                g_telemetry->Emit({
                    .event = "host_identified",
                    .runtime_result = identity->IsKnownExactBuild() ? "known" : "unknown",
                    .detail = host_detail,
                });
            }
        });
    } catch (...) {
    }
}

void BeforePresent(IDirect3DDevice9* device) noexcept {
    try {
        g_callback_context = ContextFor(device);
        if (g_telemetry) {
            g_present_token = g_telemetry->CallbackEnter("Present", g_callback_context);
        }
        std::call_once(g_present_log_once, [] {
            LogLine("d3d9 Present: frame boundary observed");
        });

#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
        const std::uint64_t callbacks =
            g_present_callbacks.fetch_add(1, std::memory_order_relaxed) + 1;
        if ((callbacks >= 2 && callbacks <= 10) || callbacks == 60 || callbacks == 300 ||
            (callbacks > 300 && callbacks % 1800 == 0)) {
            LogLine("d3d9 Present: callback_count=" + std::to_string(callbacks));
        }
#endif

        if (g_readback_requested.load(std::memory_order_relaxed)) {
            std::call_once(g_readback_capture_once, [device] {
                std::string diagnostic;
                const bool success =
                    cojvr::backends::d3d9::CaptureBackBufferToD3D11(device, diagnostic);
                LogLine(std::string("d3d9 classic readback -> D3D11: ") +
                    (success ? "success " : "failed ") + diagnostic);
            });
        }


    } catch (...) {
    }
}

#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
bool IsDiagnosticMilestone(const std::uint64_t count) noexcept {
    return (count >= 2 && count <= 10) || count == 60 || count == 300 ||
        (count > 300 && count % 1800 == 0);
}

void LogOpenVrFlatPhase(
    const cojvr::backends::d3d9::OpenVrFlatBridgePhaseEvent& event) noexcept {
    try {
        using Phase = cojvr::backends::d3d9::OpenVrFlatBridgePhase;
        switch (event.phase) {
        case Phase::RuntimeInitialized:
            if (g_openvr_runtime_state.exchange(1, std::memory_order_acq_rel) != 1 && g_telemetry) {
                g_telemetry->RuntimeStateChanged(
                    g_callback_context, "OpenVR", "initialized");
            }
            break;
        case Phase::RuntimeInitializationFailed:
            if (g_openvr_runtime_state.exchange(-1, std::memory_order_acq_rel) != -1 && g_telemetry) {
                g_telemetry->RuntimeStateChanged(
                    g_callback_context, "OpenVR",
                    "initialization_failed:" + std::to_string(event.runtime_result));
            }
            break;
        case Phase::BeforeReadback:
            if (g_telemetry) g_capture_token =
                g_telemetry->StageBegin("capture", g_callback_context);
            break;
        case Phase::AfterReadback:
            if (g_telemetry) g_telemetry->StageEnd(
                "capture", g_callback_context, g_capture_token, event.hresult);
            break;
        case Phase::FramePublished:
            if (g_telemetry) {
                static_cast<void>(g_telemetry->FramePublished(
                    g_callback_context, g_capture_token.sequence, event.content_hash));
            }
            break;
        case Phase::BeforeUpload:
            if (g_telemetry) g_upload_token =
                g_telemetry->StageBegin("upload", g_callback_context);
            break;
        case Phase::AfterUpload:
            if (g_telemetry) g_telemetry->StageEnd(
                "upload", g_callback_context, g_upload_token, event.hresult);
            break;
        case Phase::BeforeWaitForHmdPose:
            if (g_telemetry) g_wait_poses_token =
                g_telemetry->StageBegin("wait_poses", g_callback_context);
            break;
        case Phase::AfterWaitForHmdPose:
            if (g_telemetry) g_telemetry->StageEnd(
                "wait_poses", g_callback_context, g_wait_poses_token, event.hresult,
                std::to_string(event.runtime_result));
            break;
        case Phase::BeforeSubmitLeft:
            if (g_telemetry) g_submit_left_token =
                g_telemetry->SubmitBegin("left", g_callback_context);
            break;
        case Phase::AfterSubmitLeft:
            if (g_telemetry) g_telemetry->SubmitEnd(
                "left", g_callback_context, g_submit_left_token,
                std::to_string(event.runtime_result));
            break;
        case Phase::BeforeSubmitRight:
            if (g_telemetry) g_submit_right_token =
                g_telemetry->SubmitBegin("right", g_callback_context);
            break;
        case Phase::AfterSubmitRight:
            if (g_telemetry) g_telemetry->SubmitEnd(
                "right", g_callback_context, g_submit_right_token,
                std::to_string(event.runtime_result));
            break;
        }

        const std::uint64_t callback = g_end_scene_callbacks.load(std::memory_order_relaxed);
        if (!IsDiagnosticMilestone(callback)) return;

        const char* name = "unknown";
        switch (event.phase) {
        case Phase::RuntimeInitialized: name = "runtime_initialized"; break;
        case Phase::RuntimeInitializationFailed: name = "runtime_initialization_failed"; break;
        case Phase::BeforeReadback: name = "before_readback"; break;
        case Phase::AfterReadback: name = "after_readback"; break;
        case Phase::FramePublished: name = "frame_published"; break;
        case Phase::BeforeUpload: name = "before_upload"; break;
        case Phase::AfterUpload: name = "after_upload"; break;
        case Phase::BeforeWaitForHmdPose: name = "before_wait_for_hmd_pose"; break;
        case Phase::AfterWaitForHmdPose: name = "after_wait_for_hmd_pose"; break;
        case Phase::BeforeSubmitLeft: name = "before_submit_left"; break;
        case Phase::AfterSubmitLeft: name = "after_submit_left"; break;
        case Phase::BeforeSubmitRight: name = "before_submit_right"; break;
        case Phase::AfterSubmitRight: name = "after_submit_right"; break;
        }
        LogLine(
            "d3d9 OpenVR flat bridge: callback=" + std::to_string(callback) +
            " phase=" + name);
    } catch (...) {
    }
}

#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
void AfterPresent(IDirect3DDevice9*, HRESULT result) noexcept {
    try {
        if (g_telemetry) {
            g_telemetry->CallbackExit("Present", g_callback_context, g_present_token, result);
        }
        const std::uint64_t returns =
            g_present_callback_returns.fetch_add(1, std::memory_order_relaxed) + 1;
        if ((returns >= 2 && returns <= 10) || returns == 60 || returns == 300 ||
            (returns > 300 && returns % 1800 == 0)) {
            LogLine("d3d9 Present: callback_return_count=" + std::to_string(returns));
        }
    } catch (...) {
    }
}
#endif

void BeforeBeginScene(IDirect3DDevice9* device) noexcept {
    try {
        g_callback_context = ContextFor(device);
        if (g_telemetry) {
            g_begin_scene_token = g_telemetry->CallbackEnter("BeginScene", g_callback_context);
        }
    } catch (...) {
    }
}

void AfterBeginScene(IDirect3DDevice9*, HRESULT result) noexcept {
    try {
        if (SUCCEEDED(result)) {
            const std::uint64_t callbacks =
                g_begin_scene_callbacks.fetch_add(1, std::memory_order_relaxed) + 1;
            std::call_once(g_begin_scene_log_once, [] {
                LogLine("d3d9 BeginScene: frame boundary observed");
            });
            if (IsDiagnosticMilestone(callbacks)) {
                LogLine("d3d9 BeginScene: callback_count=" + std::to_string(callbacks));
            }
        }
        if (g_telemetry) {
            g_telemetry->CallbackExit(
                "BeginScene", g_callback_context, g_begin_scene_token, result);
        }
    } catch (...) {
    }
}

void BeforeEndScene(IDirect3DDevice9* device) noexcept {
    try {
        g_callback_context = ContextFor(device);
        if (g_telemetry) {
            g_end_scene_token = g_telemetry->CallbackEnter("EndScene", g_callback_context);
        }
    } catch (...) {
    }
}

void AfterEndScene(IDirect3DDevice9* device, HRESULT result) noexcept {
    try {
        if (SUCCEEDED(result)) {
            const std::uint64_t callbacks =
                g_end_scene_callbacks.fetch_add(1, std::memory_order_relaxed) + 1;
            std::call_once(g_end_scene_log_once, [] {
                LogLine("d3d9 EndScene: frame boundary observed");
            });
            if (IsDiagnosticMilestone(callbacks)) {
                LogLine("d3d9 EndScene: callback_count=" + std::to_string(callbacks));
            }

            auto& bridge = OpenVrFlatBridgeInstance();
            if (bridge.CaptureAndSubmit(device, &LogOpenVrFlatPhase)) {
                g_flat_bridge_failure_reported.store(false, std::memory_order_release);
                const std::uint64_t submitted = bridge.submitted_frames();
                std::call_once(g_openvr_flat_success_log_once, [&bridge] {
                    LogLine(std::string("d3d9 OpenVR flat bridge: first stereo submission success ") +
                        std::string(bridge.description()));
                });
                if (IsDiagnosticMilestone(submitted)) {
                    LogLine("d3d9 OpenVR flat bridge: submitted_frames=" + std::to_string(submitted));
                }
            } else {
                std::call_once(g_openvr_flat_failure_log_once, [&bridge] {
                    LogLine(std::string("d3d9 OpenVR flat bridge: failure ") +
                        std::string(bridge.last_error()));
                });
                if (g_telemetry &&
                    !g_flat_bridge_failure_reported.exchange(true, std::memory_order_acq_rel)) {
                    g_telemetry->RuntimeStateChanged(
                        g_callback_context, "flat_bridge", bridge.last_error());
                }
            }
        }
    } catch (...) {
    }

    try {
        const std::uint64_t returns =
            g_end_scene_callback_returns.fetch_add(1, std::memory_order_relaxed) + 1;
        if (IsDiagnosticMilestone(returns)) {
            LogLine("d3d9 EndScene: callback_return_count=" + std::to_string(returns));
        }
        if (g_telemetry) {
            g_telemetry->CallbackExit("EndScene", g_callback_context, g_end_scene_token, result);
        }
    } catch (...) {
    }
}
#endif

void BeforeSwapChainPresent(IDirect3DDevice9* device) noexcept {
    try {
        g_callback_context = ContextFor(device);
        if (g_telemetry) {
            g_swapchain_present_token =
                g_telemetry->CallbackEnter("SwapChainPresent", g_callback_context);
        }
    } catch (...) {
    }
}

void AfterSwapChainPresent(IDirect3DDevice9*, HRESULT result) noexcept {
    try {
        if (g_telemetry) {
            g_telemetry->CallbackExit(
                "SwapChainPresent", g_callback_context, g_swapchain_present_token, result);
        }
    } catch (...) {
    }
}

void BeforeReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS*) noexcept {
    try {
        g_callback_context = ContextFor(device);
        if (g_telemetry) {
            g_reset_token = g_telemetry->CallbackEnter("Reset", g_callback_context);
        }
    } catch (...) {
    }
#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
    try {
        OpenVrFlatBridgeInstance().BeforeD3D9Reset();
    } catch (...) {
    }
#endif
}

void AfterReset(
    IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters, HRESULT result) noexcept {
    try {
        std::ostringstream out;
        out << "d3d9 Reset: hr=0x" << std::hex << static_cast<unsigned long>(result);
        if (parameters) {
            out << std::dec << " backbuffer=" << parameters->BackBufferWidth
                << 'x' << parameters->BackBufferHeight;
        }
        LogLine(out.str());
        if (g_telemetry) {
            g_telemetry->CallbackExit("Reset", g_callback_context, g_reset_token, result);
        }
        if (SUCCEEDED(result)) {
            const auto advanced = g_diagnostic_contexts.AdvanceGeneration(device);
            if (advanced) {
                const auto context = ToTelemetryContext(*advanced);
                SetCurrentTelemetryContext(context);
                if (g_telemetry) {
                    std::ostringstream detail;
                    detail << "reason=Reset"
                        << ";device_vtable=" << PointerText(advanced->device_vtable)
                        << ";swapchain_vtable=" << PointerText(advanced->swapchain_vtable);
                    const std::string detail_text = detail.str();
                    g_telemetry->Emit({
                        .event = "generation_changed",
                        .context = context,
                        .has_hresult = true,
                        .hresult = result,
                        .runtime_result = "success",
                        .detail = detail_text,
                    });
                }
                const auto swapchain_outcome =
                    cojvr::backends::d3d9::InstallSwapChainVtableHookDetailed(
                        device,
                        cojvr::backends::d3d9::SwapChainHookCallbacks{
                            .before_present = BeforeSwapChainPresent,
                            .after_present = AfterSwapChainPresent,
                        });
                EmitHookOutcome(swapchain_outcome, context, advanced->swapchain_vtable);
            }
        }
    } catch (...) {
    }
}

void ObserveCreatedDevice(
    IDirect3D9* factory,
    UINT adapter,
    D3DDEVTYPE device_type,
    HWND,
    DWORD behavior_flags,
    D3DPRESENT_PARAMETERS* presentation_parameters,
    IDirect3DDevice9** returned_device,
    HRESULT result) noexcept {
    if (SUCCEEDED(result) && returned_device && *returned_device) {
        const auto diagnostic = g_diagnostic_contexts.RegisterDevice(factory, *returned_device);
        const auto context = ToTelemetryContext(diagnostic);
        SetCurrentTelemetryContext(context);
        if (g_telemetry) {
            std::ostringstream detail;
            detail << "creation_thread=" << diagnostic.creation_thread_id
                << ";factory_identity_matches="
                << (diagnostic.factory_identity_matches ? "true" : "false")
                << ";device_identity=" << PointerText(diagnostic.device_identity)
                << ";device_vtable=" << PointerText(diagnostic.device_vtable)
                << ";swapchain_identity=" << PointerText(diagnostic.swapchain_identity)
                << ";swapchain_vtable=" << PointerText(diagnostic.swapchain_vtable);
            const std::string detail_text = detail.str();
            g_telemetry->Emit({
                .event = "device_created",
                .context = context,
                .has_hresult = true,
                .hresult = result,
                .runtime_result = diagnostic.factory_identity_matches
                    ? "native_identity_match" : "native_identity_mismatch",
                .detail = detail_text,
            });
        }
        const cojvr::backends::d3d9::DeviceHookCallbacks callbacks{
            .before_present = BeforePresent,
#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
            .after_present = AfterPresent,
            .before_begin_scene = BeforeBeginScene,
            .after_begin_scene = AfterBeginScene,
            .before_end_scene = BeforeEndScene,
            .after_end_scene = AfterEndScene,
#else
            .after_present = nullptr,
            .before_begin_scene = nullptr,
            .after_begin_scene = nullptr,
            .before_end_scene = nullptr,
            .after_end_scene = nullptr,
#endif
            .before_reset = BeforeReset,
            .after_reset = AfterReset,
        };
        const auto device_hook_outcome =
            cojvr::backends::d3d9::InstallDeviceVtableHookDetailed(
                *returned_device, callbacks);
        const bool hooks_installed =
            device_hook_outcome.result == cojvr::backends::d3d9::HookRegistryResult::Installed ||
            device_hook_outcome.result ==
                cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled;
        EmitHookOutcome(device_hook_outcome, context, diagnostic.device_vtable);
        LogLine(hooks_installed
                    ? "d3d9 device hooks: Present/Reset active"
                    : "d3d9 device hooks: installation failed");
#if defined(COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON)
        if (hooks_installed) {
            LogLine("d3d9 device hook: EndScene active");
        }
#endif
        const auto swapchain_hook_outcome =
            cojvr::backends::d3d9::InstallSwapChainVtableHookDetailed(
                *returned_device,
                cojvr::backends::d3d9::SwapChainHookCallbacks{
                    .before_present = BeforeSwapChainPresent,
                    .after_present = AfterSwapChainPresent,
                });
        EmitHookOutcome(swapchain_hook_outcome, context, diagnostic.swapchain_vtable);
    }

    try {
        std::ostringstream out;
        out << "d3d9 CreateDevice: adapter=" << adapter
            << " type=" << static_cast<unsigned>(device_type)
            << " flags=0x" << std::hex << behavior_flags
            << " hr=0x" << static_cast<unsigned long>(result);
        if (presentation_parameters) {
            out << std::dec
                << " windowed=" << (presentation_parameters->Windowed ? 1 : 0)
                << " backbuffer=" << presentation_parameters->BackBufferWidth
                << 'x' << presentation_parameters->BackBufferHeight;
        }
LogLine(out.str());
    } catch (...) {
    }
}

} // namespace

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT sdk_version) {
    LogHostIdentityOnce();
    g_readback_requested.store(ClassicReadbackRequested(), std::memory_order_relaxed);

    if (D3D9ExBridgeRequested()) {
        const auto create_ex = cojvr::backends::d3d9::SystemDirect3DCreate9Ex();
        if (!create_ex) {
            LogLine("d3d9 D3D9Ex bridge: system entry point unavailable");
            return nullptr;
        }

        IDirect3D9Ex* real_d3d_ex = nullptr;
        const HRESULT result = create_ex(sdk_version, &real_d3d_ex);
        if (FAILED(result) || real_d3d_ex == nullptr) {
            LogLine("d3d9 D3D9Ex bridge: system creation failed");
            return nullptr;
        }

        const auto factory_context = g_diagnostic_contexts.RegisterCreatedFactory(real_d3d_ex);
        const cojvr::runtime::TelemetryContext telemetry_context{
            factory_context.factory_id, 0, 0, 0};

        try {
            auto* forwarder = new cojvr::backends::d3d9::Direct3D9ExForwarder(real_d3d_ex);
            real_d3d_ex->Release();
            LogLine("d3d9 D3D9Ex bridge: forwarding wrapper active (IDirect3D9 + IDirect3D9Ex)");

            // Install factory hook on the forwarder to observe CreateDevice calls
            const cojvr::backends::d3d9::FactoryHookCallbacks ex_callbacks{
                .after_create_device = &ObserveCreatedDevice,
            };
            const auto ex_factory_hook_outcome =
                cojvr::backends::d3d9::InstallFactoryVtableHookDetailed(forwarder, ex_callbacks);
            EmitHookOutcome(ex_factory_hook_outcome, telemetry_context, nullptr);

            return forwarder;
        } catch (...) {
            LogLine("d3d9 D3D9Ex bridge: wrapper allocation failed");
            return static_cast<IDirect3D9*>(real_d3d_ex);
        }
    }

    const auto create = cojvr::backends::d3d9::SystemDirect3DCreate9();
    if (!create) {
        LogLine("d3d9 Direct3DCreate9: system entry point unavailable");
        return nullptr;
    }

    IDirect3D9* real_d3d = create(sdk_version);
    if (!real_d3d) {
        LogLine("d3d9 Direct3DCreate9: system call returned null");
        return nullptr;
    }

    const auto factory_context = g_diagnostic_contexts.RegisterCreatedFactory(real_d3d);
    const cojvr::runtime::TelemetryContext telemetry_context{
        factory_context.factory_id, 0, 0, 0};
    if (g_telemetry) {
        std::ostringstream detail;
        detail << "creation_thread=" << factory_context.creation_thread_id
            << ";factory_identity=" << PointerText(factory_context.identity)
            << ";factory_vtable=" << PointerText(factory_context.vtable);
        const std::string detail_text = detail.str();
        g_telemetry->Emit({
            .event = "factory_created",
            .context = telemetry_context,
            .runtime_result = "native",
            .detail = detail_text,
        });
    }

    const cojvr::backends::d3d9::FactoryHookCallbacks callbacks{
        .after_create_device = ObserveCreatedDevice,
    };
    const auto factory_hook_outcome =
        cojvr::backends::d3d9::InstallFactoryVtableHookDetailed(real_d3d, callbacks);
    EmitHookOutcome(factory_hook_outcome, telemetry_context, factory_context.vtable);
    if (factory_hook_outcome.result != cojvr::backends::d3d9::HookRegistryResult::Installed &&
        factory_hook_outcome.result !=
            cojvr::backends::d3d9::HookRegistryResult::AlreadyInstalled) {
        LogLine("d3d9 Direct3DCreate9: native factory observation failed");
        real_d3d->Release();
        return nullptr;
    }
    LogLine("d3d9 Direct3DCreate9: native factory observation active");
    return real_d3d;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_proxy_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
