#pragma once

#include "runtime/game_id.hpp"

#include <span>
#include <string_view>

namespace cojvr::runtime {

struct KnownBuild {
    GameId game = GameId::unknown;
    RendererBackend renderer = RendererBackend::unknown;
    std::string_view executable_name{};
    std::string_view sha256{};
};

std::span<const KnownBuild> KnownBuilds() noexcept;
const KnownBuild* FindKnownBuild(std::string_view sha256) noexcept;

} // namespace cojvr::runtime
