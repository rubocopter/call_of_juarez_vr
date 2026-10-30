#include "runtime/graphics_settings.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>

namespace cojvr::runtime {
namespace {
constexpr std::size_t maximum_bytes = 8192;
// Render reads never acquire a store's editor mutex. This mutex covers only a
// fixed-size snapshot copy, including the revision paired with its values.
std::mutex publication_mutex;
GraphicsPreferenceSnapshot current_preferences;
std::atomic<std::uint64_t> save_sequence{0};
std::atomic<std::int64_t> mounted_deadline{0};
std::int64_t MonotonicMs() noexcept {return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();}
void Publish(const GraphicsPreferenceSnapshot& snapshot) noexcept {
    std::lock_guard lock(publication_mutex);
    current_preferences = snapshot;
}
GraphicsSettingsLoadResult Failure(GraphicsSettingsStatus status, std::uint32_t error) noexcept {
    return {GraphicsSettings{}, status, error};
}
GraphicsSettingsLoadResult Invalid() noexcept {
    return Failure(GraphicsSettingsStatus::invalid, ERROR_INVALID_DATA);
}
std::string_view Trim(std::string_view text) noexcept {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    return text;
}
bool ParseBool(std::string_view text, bool& value) noexcept {
    if (text != "true" && text != "false") return false;
    value = text == "true";
    return true;
}
bool ParseFloat(std::string_view text, float& value) noexcept {
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && std::isfinite(value);
}
bool InRange(float value, float minimum, float maximum) noexcept {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
struct File {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
struct Temporary {
    std::filesystem::path path;
    bool owned = false;
    ~Temporary() { if (owned) DeleteFileW(path.c_str()); }
};
void AppendFloat(std::string& text, float value) {
    std::array<char, 64> buffer{};
    // max_digits10 preserves all finite float bits (including tiny and negative
    // zero values) through the existing locale-independent strict parser.
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
        std::chars_format::general, std::numeric_limits<float>::max_digits10);
    text.append(buffer.data(), result.ptr);
    text += '\n';
}
std::uint32_t ProtectedFileError(const GraphicsSettingsLoadResult& loaded) noexcept {
    if (loaded.status == GraphicsSettingsStatus::invalid)
        return loaded.error_code ? loaded.error_code : ERROR_INVALID_DATA;
    if (loaded.status == GraphicsSettingsStatus::io_error)
        return loaded.error_code ? loaded.error_code : ERROR_READ_FAULT;
    return 0;
}
}

bool ValidGraphicsSettings(const GraphicsSettings& values) noexcept {
    return static_cast<unsigned>(values.reconstruction)<=2 && values.render_scale<=5 && values.horse_comfort<=2 &&
        InRange(values.sharpness, 0.0F, 1.0F) && InRange(values.contrast, 0.5F, 1.5F) &&
        InRange(values.saturation, 0.0F, 2.0F) && InRange(values.gamma, 0.5F, 2.0F);
}
bool GraphicsEffectsActive(const GraphicsSettings& values) noexcept {
    return values.enabled && ValidGraphicsSettings(values) &&
        (values.sharpness != 0.0F || values.contrast != 1.0F || values.saturation != 1.0F ||
         values.gamma != 1.0F || values.edge_smoothing);
}

GraphicsSettingsLoadResult ParseGraphicsSettings(std::string_view text) noexcept {
    if (text.size() > maximum_bytes || text.find('\0') != std::string_view::npos) return Invalid();
    constexpr std::array<std::string_view, 11> keys{
        "version", "enabled", "sharpness", "contrast", "saturation", "gamma", "edge_smoothing", "reconstruction", "render_scale", "horse_comfort", "neural"};
    unsigned seen = 0;
    unsigned version = 0;
    GraphicsSettings values;
    while (!text.empty()) {
        const auto newline = text.find('\n');
        auto line = text.substr(0, newline);
        if (newline == std::string_view::npos) text = {};
        else text.remove_prefix(newline + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        line = Trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;
        const auto equals = line.find('=');
        if (equals == std::string_view::npos) return Invalid();
        const auto key = Trim(line.substr(0, equals));
        const auto value = Trim(line.substr(equals + 1));
        std::size_t index = 0;
        while (index < keys.size() && keys[index] != key) ++index;
        if (index == keys.size() || (seen & (1U << index))) return Invalid();
        seen |= 1U << index;
        bool valid = false;
        switch (index) {
        case 0: valid = value == "1" || value == "2" || value == "3" || value == "4" || value == "5";
            if(valid)version=static_cast<unsigned>(value[0]-'0');break;
        case 1: valid = ParseBool(value, values.enabled); break;
        case 2: valid = ParseFloat(value, values.sharpness); break;
        case 3: valid = ParseFloat(value, values.contrast); break;
        case 4: valid = ParseFloat(value, values.saturation); break;
        case 5: valid = ParseFloat(value, values.gamma); break;
        case 6: valid = ParseBool(value, values.edge_smoothing); break;
        case 7:
            valid=value=="native"||value=="dlaa"||value=="dlss_quality";
            values.reconstruction=value=="dlaa"?ReconstructionMode::dlaa:
                value=="dlss_quality"?ReconstructionMode::dlss_quality:ReconstructionMode::native;
            break;
        case 8: valid=value.size()==1&&value[0]>='0'&&value[0]<='5';
            if(valid)values.render_scale=static_cast<std::uint8_t>(value[0]-'0');break;
        case 9: valid=value.size()==1&&value[0]>='0'&&value[0]<='2';
            if(valid)values.horse_comfort=static_cast<std::uint8_t>(value[0]-'0');break;
        case 10: valid=ParseBool(value,values.neural);break;
        }
        if (!valid) return Invalid();
    }
    if (seen != (version==1?127U:version==2?255U:version==5?2047U:1023U) || !ValidGraphicsSettings(values)) return Invalid();
    // Interpret legacy indices only after all keys, including version, are read.
    // Loading never persists this migration; an accepted menu save writes v5.
    if(version==3) {
        if(values.render_scale>4)return Invalid();
        if(values.render_scale==4)values.render_scale=0;
    }
    return {values, GraphicsSettingsStatus::loaded, 0};
}

std::filesystem::path DefaultGraphicsSettingsPath() noexcept {
    try {
        std::wstring buffer(32768, L'\0');
        const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!length || length >= buffer.size()) return {};
        buffer.resize(length);
        const std::filesystem::path local(buffer);
        if (!local.is_absolute()) return {};
        return local / L"CoJVR" / L"CallOfJuarez" / L"graphics.ini";
    } catch (...) { return {}; }
}

GraphicsSettingsLoadResult LoadGraphicsSettings(const std::filesystem::path& path) noexcept {
    try {
        if (path.empty() || path.native().find(L'\0') != std::wstring::npos)
            return Failure(GraphicsSettingsStatus::io_error, ERROR_INVALID_NAME);
        File file{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (file.handle == INVALID_HANDLE_VALUE) {
            const auto error = GetLastError();
            return Failure(error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
                GraphicsSettingsStatus::missing : GraphicsSettingsStatus::io_error, error);
        }
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file.handle, &size)) return Failure(GraphicsSettingsStatus::io_error, GetLastError());
        if (size.QuadPart > static_cast<LONGLONG>(maximum_bytes))
            return Failure(GraphicsSettingsStatus::invalid, ERROR_FILE_TOO_LARGE);
        if (size.QuadPart < 0) return Failure(GraphicsSettingsStatus::io_error, ERROR_READ_FAULT);
        // One extra byte detects growth without unbounded allocation or a size/read race.
        std::array<char, maximum_bytes + 1> buffer{};
        DWORD read = 0;
        if (!ReadFile(file.handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr))
            return Failure(GraphicsSettingsStatus::io_error, GetLastError());
        if (read > maximum_bytes) return Failure(GraphicsSettingsStatus::invalid, ERROR_FILE_TOO_LARGE);
        if (static_cast<LONGLONG>(read) != size.QuadPart)
            return Failure(GraphicsSettingsStatus::io_error, ERROR_READ_FAULT);
        return ParseGraphicsSettings(std::string_view(buffer.data(), read));
    } catch (...) { return Failure(GraphicsSettingsStatus::io_error, ERROR_NOT_ENOUGH_MEMORY); }
}

GraphicsSettingsSaveResult SaveGraphicsSettings(const std::filesystem::path& path, GraphicsSettings values) noexcept {
    if (!ValidGraphicsSettings(values)) return {false, ERROR_INVALID_PARAMETER};
    try {
        // Besides startup protection in the store, direct saves preserve files
        // that have become unrecognized or unreadable since initialization.
        if (const auto error = ProtectedFileError(LoadGraphicsSettings(path))) return {false, error};
        std::string text = "version=5\nenabled=";
        text += values.enabled ? "true\n" : "false\n";
        text += "sharpness="; AppendFloat(text, values.sharpness);
        text += "contrast="; AppendFloat(text, values.contrast);
        text += "saturation="; AppendFloat(text, values.saturation);
        text += "gamma="; AppendFloat(text, values.gamma);
        text += "edge_smoothing=";
        text += values.edge_smoothing ? "true\n" : "false\n";
        text += "reconstruction=";
        text += values.reconstruction==ReconstructionMode::dlaa?"dlaa\n":
            values.reconstruction==ReconstructionMode::dlss_quality?"dlss_quality\n":"native\n";
        text += "render_scale=";text+=static_cast<char>('0'+values.render_scale);text+='\n';
        text += "horse_comfort=";text+=static_cast<char>('0'+values.horse_comfort);text+='\n';
        text += "neural=";text+=values.neural?"true\n":"false\n";

        std::error_code error;
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), error);
        if (error) return {false, static_cast<std::uint32_t>(error.value())};
        Temporary temporary;
        File file;
        for (unsigned attempt = 0; attempt < 64; ++attempt) {
            temporary.path = path;
            temporary.path += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(save_sequence.fetch_add(1));
            file.handle = CreateFileW(temporary.path.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file.handle != INVALID_HANDLE_VALUE) { temporary.owned = true; break; }
            const auto create_error = GetLastError();
            if (create_error != ERROR_FILE_EXISTS && create_error != ERROR_ALREADY_EXISTS) return {false, create_error};
        }
        if (!temporary.owned) return {false, ERROR_FILE_EXISTS};
        DWORD written = 0;
        if (!WriteFile(file.handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr))
            return {false, GetLastError()};
        if (written != text.size()) return {false, ERROR_WRITE_FAULT};
        if (!FlushFileBuffers(file.handle)) return {false, GetLastError()};
        const BOOL closed = CloseHandle(file.handle);
        const auto close_error = closed ? ERROR_SUCCESS : GetLastError();
        file.handle = INVALID_HANDLE_VALUE;
        if (!closed) return {false, close_error};
        // Narrow the check/replace window: an external malformed or unreadable
        // replacement encountered during the write is protected as well.
        if (const auto protected_error = ProtectedFileError(LoadGraphicsSettings(path))) return {false, protected_error};
        if (!MoveFileExW(temporary.path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return {false, GetLastError()};
        temporary.owned = false;
        return {true, 0};
    } catch (...) { return {false, ERROR_NOT_ENOUGH_MEMORY}; }
}

GraphicsPreferenceSnapshot GraphicsPreferenceStore::Initialize(const std::filesystem::path& path) noexcept {
    std::lock_guard lock(mutex_);
    if (initialized_) return snapshot_;
    try {
        file_ = path;
        const auto loaded = LoadGraphicsSettings(file_);
        snapshot_.values = loaded.values;
        snapshot_.load_status = loaded.status;
        snapshot_.load_error = loaded.error_code;
    } catch (...) {
        file_.clear();
        snapshot_.values = {};
        snapshot_.load_status = GraphicsSettingsStatus::io_error;
        snapshot_.load_error = ERROR_NOT_ENOUGH_MEMORY;
    }
    initialized_ = true;
    snapshot_.revision = 1;
    Publish(snapshot_);
    return snapshot_;
}

GraphicsPreferenceSnapshot GraphicsPreferenceStore::Read() const noexcept {
    std::lock_guard lock(mutex_);
    return snapshot_;
}

GraphicsPreferenceApplyResult GraphicsPreferenceStore::Apply(GraphicsSettings values, std::uint64_t expected_revision) noexcept {
    // Serializing through persistence prevents an older editor's replacement
    // from finishing after a newer accepted edit. Publication never spans I/O.
    std::lock_guard lock(mutex_);
    GraphicsPreferenceApplyResult result;
    if (!ValidGraphicsSettings(values)) result.error_code = ERROR_INVALID_PARAMETER;
    else if (!initialized_) result.error_code = ERROR_NOT_READY;
    else if (expected_revision != snapshot_.revision) {
        result.conflict = true;
        result.error_code = ERROR_REVISION_MISMATCH;
    } else if (snapshot_.revision == std::numeric_limits<std::uint64_t>::max()) {
        result.error_code = ERROR_ARITHMETIC_OVERFLOW;
    } else {
        result.accepted = true;
        snapshot_.values = values;
        ++snapshot_.revision;
        Publish(snapshot_);
        // Startup errors protect the file for this entire session, including
        // after external repair/removal and repeated Initialize calls.
        const GraphicsSettingsLoadResult initial{{}, snapshot_.load_status, snapshot_.load_error};
        const auto protection = ProtectedFileError(initial);
        const auto saved = protection ? GraphicsSettingsSaveResult{false, protection} : SaveGraphicsSettings(file_, values);
        result.saved = saved.ok;
        result.error_code = saved.error_code;
        snapshot_.save_failed = !saved.ok;
        snapshot_.save_error = saved.error_code;
        Publish(snapshot_);
    }
    result.snapshot = snapshot_;
    return result;
}

GraphicsPreferenceStore& SharedGraphicsPreferences() noexcept {
    static GraphicsPreferenceStore store;
    return store;
}

GraphicsPreferenceSnapshot CurrentGraphicsPreferences() noexcept {
    std::lock_guard lock(publication_mutex);
    return current_preferences;
}
void PublishMountedMotion(bool moving) noexcept {mounted_deadline.store(moving?MonotonicMs()+150:0);}
std::uint32_t CurrentMountedMotionLeaseMs() noexcept {
    const auto now=MonotonicMs();const auto deadline=mounted_deadline.load();
    return deadline>now&&deadline-now<=150?static_cast<std::uint32_t>(deadline-now):0;
}
bool CurrentMountedMotion() noexcept {return CurrentMountedMotionLeaseMs()!=0;}
}
