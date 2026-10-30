#pragma once
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string_view>

namespace cojvr::runtime {
enum class ReconstructionMode : std::uint8_t { native, dlaa, dlss_quality };
struct GraphicsSettings {
    bool enabled = false;
    float sharpness = 0.0F;
    float contrast = 1.0F;
    float saturation = 1.0F;
    float gamma = 1.0F;
    bool edge_smoothing = false;
    // Requested preference, not evidence of an admitted temporal source.
    ReconstructionMode reconstruction = ReconstructionMode::native;
    // Next preparation/startup only: 0 Auto, 1 75%, 2 100%, 3 125%, 4 150%, 5 200%.
    // Percentages scale pixel count relative to the current HMD recommendation.
    std::uint8_t render_scale = 0;
    std::uint8_t horse_comfort = 0; // Off / mild / strong peripheral dimming.
    // Independent requested Neural appearance processing; never actual status.
    bool neural = false;
};
[[nodiscard]] bool ValidGraphicsSettings(const GraphicsSettings& values) noexcept;
// Enabled neutral settings bypass processing; invalid settings always bypass it.
[[nodiscard]] bool GraphicsEffectsActive(const GraphicsSettings& values) noexcept;
enum class GraphicsSettingsStatus { loaded, missing, invalid, io_error };
struct GraphicsSettingsLoadResult {
    GraphicsSettings values{};
    GraphicsSettingsStatus status = GraphicsSettingsStatus::missing;
    std::uint32_t error_code = 0;
};
[[nodiscard]] std::filesystem::path DefaultGraphicsSettingsPath() noexcept;
// Read-only, independent of cinematic settings. Failure returns neutral values.
[[nodiscard]] GraphicsSettingsLoadResult LoadGraphicsSettings(const std::filesystem::path& file) noexcept;
// At most 8192 bytes. V1 seven keys; v2 reconstruction; v3/v4 scale/horse comfort;
// v5 adds the independent Neural request. V1-v4 migrate with Neural off.
// V3 Manual index 4 migrates to Auto; v4 adds pixel scales 150%/200% at 4/5.
// Decimal/scientific floats, lowercase booleans, space/tab padding and LF/CRLF;
// # and ; introduce whole-line comments only. Invalid data has a nonzero error.
[[nodiscard]] GraphicsSettingsLoadResult ParseGraphicsSettings(std::string_view text) noexcept;

struct GraphicsSettingsSaveResult {
    bool ok = false;
    std::uint32_t error_code = 0;
};
// Atomic version-5 replacement; old input migrates, malformed files remain protected.
[[nodiscard]] GraphicsSettingsSaveResult SaveGraphicsSettings(
    const std::filesystem::path& file, GraphicsSettings values) noexcept;

struct GraphicsPreferenceSnapshot {
    GraphicsSettings values{};
    std::uint64_t revision = 0;
    bool save_failed = false;
    std::uint32_t save_error = 0;
    GraphicsSettingsStatus load_status = GraphicsSettingsStatus::missing;
    std::uint32_t load_error = 0;
};
struct GraphicsPreferenceApplyResult {
    bool accepted = false;
    bool saved = false;
    bool conflict = false;
    std::uint32_t error_code = 0;
    GraphicsPreferenceSnapshot snapshot{};
};
class GraphicsPreferenceStore final {
public:
    // First initialization owns the path and load diagnostics for the session.
    // Reads/publishes only; repeated calls do not reload or replace that path.
    [[nodiscard]] GraphicsPreferenceSnapshot Initialize(const std::filesystem::path& file) noexcept;
    [[nodiscard]] GraphicsPreferenceSnapshot Read() const noexcept;
    // Accepted edits (including retries) advance revision and publish before I/O.
    // Save errors retain session values; invalid/unreadable startup files remain
    // protected until restart. Editors serialize through persistence completion.
    [[nodiscard]] GraphicsPreferenceApplyResult Apply(
        GraphicsSettings values, std::uint64_t expected_revision) noexcept;
private:
    mutable std::mutex mutex_;
    std::filesystem::path file_;
    GraphicsPreferenceSnapshot snapshot_{};
    bool initialized_ = false;
};
[[nodiscard]] GraphicsPreferenceStore& SharedGraphicsPreferences() noexcept;
// Coherent values/revision, using only a short publication lock, never editor or
// filesystem locks. Does not initialize the store or load preferences implicitly.
[[nodiscard]] GraphicsPreferenceSnapshot CurrentGraphicsPreferences() noexcept;
// Positive adapter-owned mounted movement only, with a short lease. Not a
// movement/speed writer, not inferred from tracking or another game's state.
void PublishMountedMotion(bool moving) noexcept;
// The same lease observation drives diagnostics and processing; zero means
// neutral, revoked or expired. No native speed/actual horse motion is inferred.
[[nodiscard]] std::uint32_t CurrentMountedMotionLeaseMs() noexcept;
[[nodiscard]] bool CurrentMountedMotion() noexcept;
}
