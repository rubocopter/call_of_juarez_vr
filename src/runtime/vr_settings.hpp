#pragma once
#include "runtime/hud_text.hpp"
#include <filesystem>

namespace cojvr::runtime {
enum class CinematicsMode : std::uint8_t { theater, immersive };
enum class SettingsLanguage : std::uint8_t { english, spanish };
// Game-thread reads never touch presenter-owned menu or filesystem objects.
[[nodiscard]] CinematicsMode CurrentCinematicMode() noexcept;
[[nodiscard]] bool SettingsInputCaptured() noexcept;
void PublishCinematicMode(CinematicsMode mode) noexcept;
void PublishSettingsInputCapture(bool captured) noexcept;

enum class SettingsLoadStatus { loaded, missing, invalid, io_error };
struct SettingsLoadResult {
    CinematicsMode mode = CinematicsMode::theater;
    SettingsLoadStatus status = SettingsLoadStatus::missing;
    std::uint32_t error_code = 0;
};
struct SettingsSaveResult { bool ok = false; std::uint32_t error_code = 0; };
[[nodiscard]] std::filesystem::path DefaultVrSettingsPath() noexcept;
[[nodiscard]] SettingsLoadResult LoadVrSettings(const std::filesystem::path& file) noexcept;
[[nodiscard]] SettingsSaveResult SaveVrSettings(const std::filesystem::path& file, CinematicsMode mode) noexcept;

struct SettingsMenuInput {
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
struct SettingsMenuResult {
    bool active = false, input_captured = false, native_pause = false;
    bool mode_changed = false;
    CinematicsMode requested_mode = CinematicsMode::theater;
};
// Presenter owner only. All other threads read the atomic API above.
class VrSettingsMenu final {
public:
    [[nodiscard]] SettingsMenuResult Update(const SettingsMenuInput& input) noexcept;
    void Invalidate() noexcept;
    void SetMode(CinematicsMode mode) noexcept {
        mode_ = mode == CinematicsMode::immersive ? mode : CinematicsMode::theater;
    }
    [[nodiscard]] CinematicsMode mode() const noexcept { return mode_; }
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] bool input_captured() const noexcept { return capture_; }
    [[nodiscard]] bool awaiting_release() const noexcept { return barrier_; }
private:
    CinematicsMode mode_ = CinematicsMode::theater;
    bool active_ = false, capture_ = true, barrier_ = true;
    bool options_armed_ = false, options_down_ = false;
    bool accept_down_ = false, back_down_ = false, stick_armed_ = false;
    bool have_time_ = false;
    std::uint64_t context_ = 0, last_ms_ = 0, started_ms_ = 0;
    std::uint32_t digital_release_mask_ = 0;
    std::uint16_t global_release_mask_ = 0,previous_global_available_ = 0;
    bool radial_release_required_=false,move_release_required_=false,turn_release_required_=false;
};
[[nodiscard]] HudText BuildVrSettingsText(CinematicsMode mode, SettingsLanguage language,
    bool save_failed, bool awaiting_release, std::uint32_t save_error = 0) noexcept;
}
