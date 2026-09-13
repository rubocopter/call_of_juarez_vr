#include "runtime/log.hpp"

#include <fstream>

namespace cojvr::runtime {

void AppendLogLine(const std::filesystem::path& path, std::string_view line) noexcept {
    try {
        std::ofstream output(path, std::ios::app | std::ios::binary);
        if (!output) return;
        output << line << "\r\n";
    } catch (...) {
    }
}

} // namespace cojvr::runtime
