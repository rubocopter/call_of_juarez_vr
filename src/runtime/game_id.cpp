#include "runtime/game_id.hpp"

#include <cwctype>
#include <string>

namespace cojvr::runtime {

GameId ClassifyExecutable(std::wstring_view name) noexcept {
    try {
        std::wstring lowered(name);
        for (wchar_t& ch : lowered) {
            ch = static_cast<wchar_t>(std::towlower(ch));
        }
        if (lowered == L"coj.exe") return GameId::call_of_juarez_dx9;
        if (lowered == L"coj_dx10.exe") return GameId::call_of_juarez_dx10;
        if (lowered == L"cojbibgame_x86.exe") return GameId::bound_in_blood;
        if (lowered == L"cojgunslinger.exe") return GameId::gunslinger;
    } catch (...) {
    }
    return GameId::unknown;
}

std::string_view GameIdName(GameId game) noexcept {
    switch (game) {
    case GameId::call_of_juarez_dx9: return "Call of Juarez (Direct3D 9)";
    case GameId::call_of_juarez_dx10: return "Call of Juarez (Direct3D 10)";
    case GameId::bound_in_blood: return "Call of Juarez: Bound in Blood";
    case GameId::gunslinger: return "Call of Juarez: Gunslinger";
    case GameId::unknown: break;
    }
    return "Unknown";
}

std::string_view RendererName(RendererBackend renderer) noexcept {
    switch (renderer) {
    case RendererBackend::d3d9: return "Direct3D 9";
    case RendererBackend::d3d10: return "Direct3D 10";
    case RendererBackend::unknown: break;
    }
    return "Unknown";
}

} // namespace cojvr::runtime
