#pragma once
#include "runtime/hud_text.hpp"
#include "games/call_of_juarez/ui_language.hpp"

namespace cojvr::games::call_of_juarez {
// Caller supplies the exact-build bridge's owner-thread JNI environment and
// current player. Reads native visibility/localized strings; never updates UI.
[[nodiscard]] bool ReadCoJHudText(void* environment, void* player,
    runtime::HudTextSnapshot& snapshot,CoJUiLanguage language=CoJUiLanguage::english) noexcept;
// Separate movie owner: no actor-dialogue fallback or retained JNI references.
[[nodiscard]] bool ReadCoJMovieSubtitle(void* environment, void* module,
    runtime::HudText& subtitle) noexcept;
}
