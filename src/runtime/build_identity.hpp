#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace cojvr::runtime {

std::optional<std::string> Sha256File(const std::filesystem::path& path) noexcept;

} // namespace cojvr::runtime
