#include "runtime/log.hpp"

#include <fstream>
#include <mutex>

namespace cojvr::runtime {

namespace {
std::mutex g_log_mutex;
}

void AppendLogLine(const std::filesystem::path& path, std::string_view line) noexcept {
    try {
        std::lock_guard lock(g_log_mutex);
        std::ofstream output(path, std::ios::app | std::ios::binary);
        if (!output) return;
        output << line << "\r\n";
    } catch (...) {
    }
}

} // namespace cojvr::runtime
