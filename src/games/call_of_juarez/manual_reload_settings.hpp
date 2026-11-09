#pragma once

#include "runtime/vr_settings.hpp"
#include <algorithm>
#include <string_view>

namespace cojvr::games::call_of_juarez {
enum class CoJReloadMode { native, legacy_motion, manual };

[[nodiscard]] constexpr CoJReloadMode ResolveCoJReloadMode(bool declared, bool verified,
    runtime::ManualReloadPreference preference) noexcept {
    if(preference==runtime::ManualReloadPreference::off)return CoJReloadMode::native;
    if (!declared && !verified) return CoJReloadMode::legacy_motion;
    if (declared && verified && (preference == runtime::ManualReloadPreference::install_default ||
        preference == runtime::ManualReloadPreference::on)) return CoJReloadMode::manual;
    return CoJReloadMode::native;
}

// The existing stage writer records a derivative of the pinned original
// archive. Treat the recorded digest as an expectation to check on disk.
[[nodiscard]] inline std::string_view CoJManualReloadStageArchiveHash(
    std::string_view stage, std::string_view executable_sha256) noexcept {
    constexpr std::string_view original="F9DB47C166E03F23E37CBCDFD5344E4AD4C5C9134F35E8F6DCDF66DB7E71CE12";
    constexpr std::string_view whitespace=" \r\n\t";
    const auto scalar=[&](std::string_view key) noexcept -> std::string_view {
        const auto key_at=stage.find(key);
        if(key_at==stage.npos||stage.find(key,key_at+key.size())!=stage.npos)return {};
        const auto colon=stage.find_first_not_of(whitespace,key_at+key.size());
        if(colon==stage.npos||stage[colon]!=':')return {};
        const auto begin=stage.find_first_not_of(whitespace,colon+1);
        if(begin==stage.npos)return {};
        const auto end=stage.find_first_of(",}",begin);
        if(end==stage.npos)return {};
        const auto last=stage.find_last_not_of(whitespace,end-1);
        if(last==stage.npos||last<begin)return {};
        return stage.substr(begin,last-begin+1);
    };
    const auto digest=[&](std::string_view key) noexcept -> std::string_view {
        const auto value=scalar(key);
        if(value.size()!=66||value.front()!='"'||value.back()!='"')return {};
        const auto hex=value.substr(1,64);
        return std::all_of(hex.begin(),hex.end(),[](char c) {
            return (c>='0'&&c<='9')||(c>='A'&&c<='F');
        })?hex:std::string_view{};
    };
    if(executable_sha256.empty()||scalar("\"manualReloadInstalled\"")!="true"||
        scalar("\"codeArchiveManaged\"")!="true"||digest("\"originalCodeSha256\"")!=original||
        digest("\"stagedExecutableSha256\"")!=executable_sha256)return {};
    const auto staged=digest("\"stagedCodeSha256\"");
    return staged==original?std::string_view{}:staged;
}
}
