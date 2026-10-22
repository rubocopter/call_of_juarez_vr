#pragma once
#include "runtime/hud_text.hpp"
#include <cstdint>
#include <filesystem>
#include <mutex>

namespace cojvr::runtime {
enum class CinematicsMode : std::uint8_t { theater, immersive };
enum class TurnMode : std::uint8_t { smooth, snap };
struct VrPreferences {
    CinematicsMode cinematics = CinematicsMode::theater;
    TurnMode turning = TurnMode::smooth;
    bool holster_guides = false; // Visual comfort only; never changes selection eligibility.
};
enum class SettingsLanguage : std::uint8_t { english, spanish };
// Game-thread reads never touch presenter-owned menu or filesystem objects.
[[nodiscard]] CinematicsMode CurrentCinematicMode() noexcept;
[[nodiscard]] TurnMode CurrentTurnMode() noexcept;
[[nodiscard]] bool CurrentHolsterGuidesEnabled() noexcept;
[[nodiscard]] bool SettingsInputCaptured() noexcept;
void PublishCinematicMode(CinematicsMode mode) noexcept;
void PublishSettingsInputCapture(bool captured) noexcept;

enum class SettingsLoadStatus { loaded, missing, invalid, io_error };
struct SettingsLoadResult {
    CinematicsMode mode = CinematicsMode::theater;
    SettingsLoadStatus status = SettingsLoadStatus::missing;
    std::uint32_t error_code = 0;
    TurnMode turning = TurnMode::smooth;
    bool holster_guides = false;
};
struct SettingsSaveResult { bool ok = false; std::uint32_t error_code = 0; };
[[nodiscard]] std::filesystem::path DefaultVrSettingsPath() noexcept;
// Strict v3 writes; v1/v2 reads supply guides=false. Cinema-only saves preserve
// the other persisted values and refuse malformed/unreadable files.
[[nodiscard]] SettingsLoadResult LoadVrSettings(const std::filesystem::path& file) noexcept;
[[nodiscard]] SettingsSaveResult SaveVrSettings(const std::filesystem::path& file, CinematicsMode mode) noexcept;
[[nodiscard]] SettingsSaveResult SaveVrSettings(const std::filesystem::path& file, VrPreferences values) noexcept;

struct VrPreferenceSnapshot {
    VrPreferences values{};
    std::uint64_t revision = 0;
    bool save_failed = false;
    std::uint32_t save_error = 0;
    SettingsLoadStatus load_status = SettingsLoadStatus::missing;
    std::uint32_t load_error = 0;
};
struct VrPreferenceApplyResult {
    bool accepted = false;
    bool saved = false;
    bool conflict = false;
    std::uint32_t error_code = 0;
    VrPreferenceSnapshot snapshot{};
};
// Editors share this owner. Read returns coherent values and revision; direct
// game-thread getters above only load atomics and never acquire this I/O lock.
class VrPreferenceStore final {
public:
    // First call owns the path for this session, including missing/invalid I/O.
    // Initialization reads/publishes but never rewrites the configuration.
    [[nodiscard]] VrPreferenceSnapshot Initialize(const std::filesystem::path& file) noexcept;
    [[nodiscard]] VrPreferenceSnapshot Read() const noexcept;
    // Every accepted request advances revision, even an unchanged-value retry.
    // Save failure retains the published session choice. load_status always
    // describes initialization; load_error retains that read's Windows error.
    // Invalid/unreadable initial files are protected for the whole session;
    // edits apply in memory only until the file is repaired/removed and restart.
    // save_failed/save_error describe the last persistence attempt.
    [[nodiscard]] VrPreferenceApplyResult Apply(VrPreferences values, std::uint64_t expected_revision) noexcept;
    [[nodiscard]] VrPreferenceApplyResult ApplyCinematics(CinematicsMode mode, std::uint64_t expected_revision) noexcept;
private:
    [[nodiscard]] VrPreferenceApplyResult ApplyLocked(VrPreferences values, std::uint64_t expected_revision) noexcept;
    mutable std::mutex mutex_;
    std::filesystem::path file_;
    VrPreferenceSnapshot snapshot_{};
    bool initialized_ = false;
};
[[nodiscard]] VrPreferenceStore& SharedVrPreferences() noexcept;

struct UiReleaseInput {
    bool available = false; // focus, tracking, polling and game context
    // Bit order follows the held fields below. Missing bindings are distinct
    // from observed releases, and do not freeze an otherwise neutral baseline.
    static constexpr std::uint16_t options_bit=1U,accept_bit=2U,back_bit=4U;
    std::uint16_t global_available = 0;
    std::uint64_t context_token = 0, monotonic_ms = 0;
    bool options = false, accept = false, back = false;
    bool select_left = false, select_right = false;
    bool auxiliary = false, shoulder_left = false, shoulder_right = false;
    GameplayInputState gameplay{}; // raw, before wheel/native policy
};
struct UiReleaseResult { bool input_captured=true,native_pause=false; };
// Presenter-owned availability fence only. Options is a fresh press; holding
// it never opens an editor or changes preferences. Native Options owns editing.
class VrInputReleaseGate final {
public:
    [[nodiscard]] UiReleaseResult Update(const UiReleaseInput& input) noexcept;
    void Invalidate() noexcept;
    [[nodiscard]] bool input_captured() const noexcept { return capture_; }
private:
    bool capture_=true,barrier_=true,options_armed_=false,options_down_=false,have_time_=false;
    std::uint64_t context_=0,last_ms_=0;
    std::uint32_t digital_release_mask_=0;
    std::uint16_t global_release_mask_=0,previous_global_available_=0;
    bool radial_release_required_=false,move_release_required_=false,turn_release_required_=false;
};
}
