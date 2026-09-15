#pragma once

#include "runtime/build_catalog.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace cojvr::runtime {

struct HostIdentity {
    std::filesystem::path executable_path{};
    GameId filename_game = GameId::unknown;
    std::string sha256{};
    const KnownBuild* known_build = nullptr;

    bool IsKnownExactBuild() const noexcept {
        return known_build != nullptr && known_build->game == filename_game;
    }
};

std::optional<HostIdentity> InspectHost(const std::filesystem::path& executable) noexcept;
std::optional<HostIdentity> InspectCurrentHost() noexcept;

} // namespace cojvr::runtime
