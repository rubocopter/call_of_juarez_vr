#pragma once

#include <string_view>

namespace cojvr::runtime {

enum class GameId {
    unknown,
    call_of_juarez_dx9,
    call_of_juarez_dx10,
    bound_in_blood,
    gunslinger,
};

enum class RendererBackend {
    unknown,
    d3d9,
    d3d10,
};

GameId ClassifyExecutable(std::wstring_view name) noexcept;
std::string_view GameIdName(GameId game) noexcept;
std::string_view RendererName(RendererBackend renderer) noexcept;

} // namespace cojvr::runtime
