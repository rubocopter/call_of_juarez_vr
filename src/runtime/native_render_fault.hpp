#pragma once
#include <windows.h>
#include <filesystem>
#include <string_view>
#include <cstdint>
namespace cojvr::runtime {
// Configure once before exact-game hooks are installed. No process-wide filter,
// exception handling/recovery or native state mutation is introduced.
[[nodiscard]] bool ConfigureNativeRenderFaultReporter(const std::filesystem::path& path,
    std::string_view run_id,std::string_view build_manifest_id) noexcept;
// Diagnostic SEH filter for an exception escaping the render hook. Always
// EXCEPTION_CONTINUE_SEARCH: the game's original handler remains authoritative.
LONG RecordNativeRenderFault(EXCEPTION_POINTERS* pointers,std::uint64_t frame) noexcept;
}
