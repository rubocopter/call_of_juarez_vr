// Query-only background observation; never qualifies CoJ's per-application size.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <openvr.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>

namespace {
constexpr std::size_t kMaxString = 1024;
using Buffer = std::array<char, kMaxString + 1>;

// Reject overlong, unterminated and malformed UTF-8 instead of publishing it.
std::string CopyString(const char* value) {
    if (!value) return {};
    std::size_t length = 0;
    while (length <= kMaxString && value[length]) ++length;
    if (length > kMaxString) return {};
    if (length && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value,
                                     static_cast<int>(length), nullptr, 0)) return {};
    return std::string(value, length);
}

// Emit ASCII JSON, including escaped UTF-16 surrogate pairs for UTF-8 input.
std::string Quote(const std::string& value) {
    std::array<wchar_t, kMaxString> wide{};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), wide.data(), static_cast<int>(wide.size()));
    std::string result = "\"";
    for (int index = 0; index < count; ++index) {
        const unsigned code = static_cast<unsigned>(wide[index]);
        if (code == '"' || code == '\\') {
            result += '\\';
            result += static_cast<char>(code);
        } else if (code < 0x20 || code >= 0x7f) {
            char escaped[7]{};
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", code);
            result += escaped;
        } else {
            result += static_cast<char>(code);
        }
    }
    return result + '"';
}

struct Observation {
    std::string preparation_id, runtime_version, serial, model, application_key, init_error_symbol;
    const char* error = "";
    std::uint32_t width = 0, height = 0, scene_pid = 0;
    std::uint32_t application_pid = 0;
    std::int32_t adapter = -1;
    std::int32_t init_error = 0;
    bool observed = false;
};

int Emit(const Observation& observation) {
    SYSTEMTIME now{};
    GetSystemTime(&now);
    char timestamp[32]{};
    std::snprintf(timestamp, sizeof(timestamp), "%04u-%02u-%02uT%02u:%02u:%02uZ",
        static_cast<unsigned>(now.wYear), static_cast<unsigned>(now.wMonth),
        static_cast<unsigned>(now.wDay), static_cast<unsigned>(now.wHour),
        static_cast<unsigned>(now.wMinute), static_cast<unsigned>(now.wSecond));
    std::cout << "{\"schemaVersion\":1,\"recordType\":\"cojvr-resolution-observation\""
        << ",\"status\":" << Quote(observation.observed ? "observed" : "unavailable")
        << ",\"observedUtc\":" << Quote(timestamp)
        << ",\"preparationId\":" << Quote(observation.preparation_id)
        << ",\"recommendedWidth\":" << observation.width
        << ",\"recommendedHeight\":" << observation.height
        << ",\"runtimeVersion\":" << Quote(observation.runtime_version)
        << ",\"hmdSerial\":" << Quote(observation.serial)
        << ",\"hmdModel\":" << Quote(observation.model)
        << ",\"adapterIndex\":" << observation.adapter
        << ",\"applicationKey\":" << Quote(observation.application_key)
        << ",\"applicationKeyStatus\":"
        << Quote(observation.application_key.empty() ? "unavailable" : "observed")
        << ",\"applicationKeyProcessId\":" << observation.application_pid
        << ",\"currentSceneProcessId\":" << observation.scene_pid
        << ",\"identityQualified\":false,\"vrInitError\":" << observation.init_error
        << ",\"vrInitErrorSymbol\":" << Quote(observation.init_error_symbol)
        << ",\"error\":" << Quote(observation.error) << "}\n";
    return observation.observed ? 0 : 2;
}

bool ValidPreparationId(const std::string_view value) {
    if (value.empty() || value.size() > 80) return false;
    for (const char character : value) {
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') ||
              character == '.' || character == '_' || character == '-')) return false;
    }
    return true;
}

struct Shutdown {
    ~Shutdown() { vr::VR_Shutdown(); }
    Shutdown() = default;
    Shutdown(const Shutdown&) = delete;
    Shutdown& operator=(const Shutdown&) = delete;
};

std::string Property(vr::IVRSystem& system, const vr::ETrackedDeviceProperty property) {
    Buffer buffer{};
    vr::ETrackedPropertyError error = vr::TrackedProp_Success;
    const auto size = system.GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
        property, buffer.data(), static_cast<std::uint32_t>(buffer.size()), &error);
    if (error != vr::TrackedProp_Success || size == 0 || size > buffer.size() ||
        buffer[size - 1] != '\0') return {};
    return CopyString(buffer.data());
}

void Query(Observation& observation) {
    if (!vr::VR_IsRuntimeInstalled()) {
        observation.error = "runtime_not_installed";
        return;
    }
    if (!vr::VR_IsHmdPresent()) {
        observation.error = "hmd_not_present";
        return;
    }
    vr::EVRInitError error = vr::VRInitError_None;
    vr::IVRSystem* system = vr::VR_Init(&error, vr::VRApplication_Background);
    if (error != vr::VRInitError_None) {
        observation.init_error = static_cast<std::int32_t>(error);
        observation.init_error_symbol = CopyString(vr::VR_GetVRInitErrorAsSymbol(error));
        observation.error = "background_init_failed";
        return;
    }
    const Shutdown shutdown; // Every successful initialization is balanced, even on exceptions.
    if (!system) {
        observation.error = "system_unavailable";
        return;
    }
    system->GetRecommendedRenderTargetSize(&observation.width, &observation.height);
    observation.runtime_version = CopyString(system->GetRuntimeVersion());
    observation.serial = Property(*system, vr::Prop_SerialNumber_String);
    observation.model = Property(*system, vr::Prop_ModelNumber_String);
    system->GetDXGIOutputInfo(&observation.adapter);
    observation.application_pid = GetCurrentProcessId();
    if (auto* applications = vr::VRApplications()) {
        observation.scene_pid = applications->GetCurrentSceneProcessId();
        Buffer key{};
        const auto key_error = applications->GetApplicationKeyByProcessId(
            observation.application_pid, key.data(), static_cast<std::uint32_t>(key.size()));
        if (key_error == vr::VRApplicationError_None) {
            observation.application_key = CopyString(key.data());
        }
    }
    // Background-app recommendations do not establish the active scene's settings.
    observation.observed = observation.width != 0 && observation.height != 0;
    if (!observation.observed) observation.error = "invalid_recommended_dimensions";
}
} // namespace

int main(const int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: cojvr_openvr_resolution_query --preparation-id [A-Za-z0-9._-]{1,80}\n";
        return 0;
    }
    Observation observation;
    if (argc != 3 || std::string_view(argv[1]) != "--preparation-id" ||
        !ValidPreparationId(argv[2])) {
        observation.error = "invalid_arguments";
        return Emit(observation); // No OpenVR calls on any invalid CLI.
    }
    observation.preparation_id = argv[2];
    try {
        Query(observation);
    } catch (...) {
        observation.observed = false;
        observation.error = "query_failed";
    }
    return Emit(observation);
}
