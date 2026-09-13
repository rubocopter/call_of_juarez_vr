#include "runtime/host_identity.hpp"

#include "runtime/build_identity.hpp"
#include "runtime/game_id.hpp"

#include <windows.h>

#include <array>

namespace cojvr::runtime {

std::optional<HostIdentity> InspectHost(const std::filesystem::path& executable) noexcept {
    const auto digest = Sha256File(executable);
    if (!digest) return std::nullopt;

    HostIdentity identity;
    identity.executable_path = executable;
    identity.filename_game = ClassifyExecutable(executable.filename().wstring());
    identity.sha256 = *digest;
    identity.known_build = FindKnownBuild(identity.sha256);
    return identity;
}

std::optional<HostIdentity> InspectCurrentHost() noexcept {
    try {
        std::array<wchar_t, 32768> buffer{};
        const DWORD count = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (count == 0 || count >= buffer.size()) return std::nullopt;
        return InspectHost(std::filesystem::path(std::wstring_view(buffer.data(), count)));
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace cojvr::runtime
