#pragma once
#include "runtime/hud_text.hpp"

namespace cojvr::games::call_of_juarez {
// Caller supplies the exact-build bridge's owner-thread JNI environment and
// current player. Reads native visibility/localized strings; never updates UI.
[[nodiscard]] bool ReadCoJHudText(void* environment, void* player,
    runtime::HudTextSnapshot& snapshot) noexcept;
}
