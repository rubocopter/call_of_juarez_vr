#pragma once
#include "backends/openvr/neural_protocol.hpp"
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace cojvr::backends::openvr::neural {
enum class State {stopped,starting,ready,pending,recovering,failed};
// Isolated CPU-proxy laboratory client; not linked into game presentation.
// Single caller. Pending shared input is immutable until completion or child exit.
class Session final {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    Session();~Session();
    Session(const Session&)=delete;Session& operator=(const Session&)=delete;
    bool Start(const std::filesystem::path& helper,const std::filesystem::path& runtime,const std::wstring& mode);
    State Poll();
    bool Process(const Header& frame,std::span<const std::uint8_t> pair,std::uint32_t timeout_ms);
    const std::vector<std::uint8_t>& Output() const;
    const std::string& Error() const;
    void Stop();
};
}
