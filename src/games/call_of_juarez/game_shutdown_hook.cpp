#include "games/call_of_juarez/game_shutdown_hook.hpp"
#include "backends/d3d9/vtable_patch.hpp"
#include "runtime/build_identity.hpp"
#include "runtime/host_identity.hpp"
#include <Windows.h>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>

namespace cojvr::games::call_of_juarez {
static_assert(sizeof(void*) == 4, "The inspected CoJ shutdown boundary is x86 only");
namespace {
constexpr std::size_t kDestroyGameImportRva = 0x9034;
constexpr std::size_t kDestroyGameCallRva = 0x2105;
constexpr std::uintptr_t kDestroyGameExportRva = 0x2d00;
constexpr std::string_view kExecutableHash =
    "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE";
constexpr std::string_view kEngineHash =
    "DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8";
using backends::d3d9::VtablePatch;
using backends::d3d9::VtablePatchResult;
VtablePatch g_import_patch;
DestroyGameFunction g_original{};
BeforeGameDestroy g_before_destroy{};
void* g_context{};
std::atomic_bool g_invoked{false};

bool EqualHash(std::string_view value, std::string_view expected) noexcept {
    if (value.size() != expected.size()) return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(value[i])) != expected[i]) return false;
    }
    return true;
}

void __cdecl HookDestroyGame() {
    // The callback may restore this import. Keep the original target independently
    // of patch ownership and never hold a hook lock while joining the runtime.
    const auto original = g_original;
    if (!g_invoked.exchange(true, std::memory_order_acq_rel)) {
        g_before_destroy(g_context);
    }
    original();
}
}

GameShutdownHookStatus InstallGameShutdownHookForImage(
    std::span<std::byte> image, std::string_view executable_sha256,
    std::string_view engine_sha256, DestroyGameFunction original,
    BeforeGameDestroy before_destroy, void* context) noexcept {
    if (!EqualHash(executable_sha256, kExecutableHash) ||
        !EqualHash(engine_sha256, kEngineHash)) return GameShutdownHookStatus::unsupported_build;
    if (!original || !before_destroy || image.size() < kDestroyGameImportRva + sizeof(void*))
        return GameShutdownHookStatus::invalid_image;
    IMAGE_DOS_HEADER dos{};
    std::memcpy(&dos, image.data(), sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 ||
        static_cast<std::size_t>(dos.e_lfanew) > image.size() - sizeof(IMAGE_NT_HEADERS32))
        return GameShutdownHookStatus::invalid_image;
    IMAGE_NT_HEADERS32 nt{};
    std::memcpy(&nt, image.data() + dos.e_lfanew, sizeof(nt));
    if (nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt.FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER32) ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt.OptionalHeader.SizeOfImage != image.size()) return GameShutdownHookStatus::invalid_image;
    auto** slot = reinterpret_cast<void**>(image.data() + kDestroyGameImportRva);
    std::uint32_t call_operand{};
    std::memcpy(&call_operand, image.data() + kDestroyGameCallRva + 2, sizeof(call_operand));
    if (image[kDestroyGameCallRva] != std::byte{0xff} ||
        image[kDestroyGameCallRva + 1] != std::byte{0x15} ||
        call_operand != reinterpret_cast<std::uintptr_t>(slot)) return GameShutdownHookStatus::invalid_image;
    if (g_import_patch.owns_entry()) {
        return g_import_patch.entry() == slot && *slot == reinterpret_cast<void*>(&HookDestroyGame) &&
                g_original == original && g_before_destroy == before_destroy && g_context == context
            ? GameShutdownHookStatus::installed : GameShutdownHookStatus::conflict;
    }
    if (*slot != reinterpret_cast<void*>(original)) return GameShutdownHookStatus::target_mismatch;
    if (g_import_patch.protection_restore_pending()) return GameShutdownHookStatus::protection_failure;
    if (!backends::d3d9::PinModuleForAddress(reinterpret_cast<void*>(&HookDestroyGame)))
        return GameShutdownHookStatus::protection_failure;
    g_original = original;
    g_before_destroy = before_destroy;
    g_context = context;
    g_invoked.store(false, std::memory_order_release);
    g_import_patch = VtablePatch{};
    const auto result = g_import_patch.Install(slot,
        reinterpret_cast<void*>(original), reinterpret_cast<void*>(&HookDestroyGame));
    if (result.result == VtablePatchResult::Applied && result.protection_restored)
        return GameShutdownHookStatus::installed;
    // Roll back a partial install; preserve any foreign replacement.
    (void)g_import_patch.Restore();
    return result.result == VtablePatchResult::Conflict
        ? GameShutdownHookStatus::conflict : GameShutdownHookStatus::protection_failure;
}

GameShutdownHookStatus InstallCurrentGameShutdownHook(
    BeforeGameDestroy before_destroy, void* context) noexcept {
    try {
        const auto host = runtime::InspectCurrentHost();
        if (!host || !EqualHash(host->sha256, kExecutableHash))
            return GameShutdownHookStatus::unsupported_build;
        const auto engine = GetModuleHandleW(L"ChromeEngine3.dll");
        if (!engine) return GameShutdownHookStatus::target_mismatch;
        wchar_t path[32768]{};
        const DWORD count = GetModuleFileNameW(engine, path, 32768);
        if (count == 0 || count >= 32768) return GameShutdownHookStatus::unsupported_build;
        const auto engine_hash = runtime::Sha256File(std::filesystem::path(path));
        if (!engine_hash || !EqualHash(*engine_hash, kEngineHash))
            return GameShutdownHookStatus::unsupported_build;
        const auto original = reinterpret_cast<DestroyGameFunction>(GetProcAddress(engine, "DestroyGame"));
        if (reinterpret_cast<std::uintptr_t>(original) !=
            reinterpret_cast<std::uintptr_t>(engine) + kDestroyGameExportRva)
            return GameShutdownHookStatus::target_mismatch;
        auto* data = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(data);
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(data + dos->e_lfanew);
        return InstallGameShutdownHookForImage({data, nt->OptionalHeader.SizeOfImage},
            host->sha256, *engine_hash, original, before_destroy, context);
    } catch (...) { return GameShutdownHookStatus::unsupported_build; }
}

bool RestoreGameShutdownHook() noexcept {
    const auto result = g_import_patch.Restore();
    return result.result == VtablePatchResult::Applied || result.result == VtablePatchResult::NoModification;
}

const char* GameShutdownHookStatusName(GameShutdownHookStatus status) noexcept {
    switch (status) {
    case GameShutdownHookStatus::installed: return "installed";
    case GameShutdownHookStatus::unsupported_build: return "unsupported_build";
    case GameShutdownHookStatus::invalid_image: return "invalid_image";
    case GameShutdownHookStatus::target_mismatch: return "target_mismatch";
    case GameShutdownHookStatus::conflict: return "conflict";
    case GameShutdownHookStatus::protection_failure: return "protection_failure";
    }
    return "unknown";
}
} // namespace cojvr::games::call_of_juarez
