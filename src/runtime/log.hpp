#pragma once

#include <filesystem>
#include <string_view>

namespace cojvr::runtime {

void AppendLogLine(const std::filesystem::path& path, std::string_view line) noexcept;

} // namespace cojvr::runtime
