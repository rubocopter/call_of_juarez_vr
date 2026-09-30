#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace cojvr::games::call_of_juarez {

using DestroyGameFunction = void (__cdecl*)();
using BeforeGameDestroy = void (*)(void*) noexcept;

enum class GameShutdownHookStatus {
    installed,
    unsupported_build,
    invalid_image,
    target_mismatch,
    conflict,
    protection_failure,
};

// Internal exact-build adapter seam shared with the mapped-image host fixture.
// Production obtains both hashes from disk and the export from the loaded engine.
GameShutdownHookStatus InstallGameShutdownHookForImage(
    std::span<std::byte> image, std::string_view executable_sha256,
    std::string_view engine_sha256, DestroyGameFunction original,
    BeforeGameDestroy before_destroy, void* context) noexcept;

GameShutdownHookStatus InstallCurrentGameShutdownHook(
    BeforeGameDestroy before_destroy, void* context) noexcept;
bool RestoreGameShutdownHook() noexcept;
const char* GameShutdownHookStatusName(GameShutdownHookStatus status) noexcept;

} // namespace cojvr::games::call_of_juarez
