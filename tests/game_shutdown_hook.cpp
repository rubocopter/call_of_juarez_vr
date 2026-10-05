#include "games/call_of_juarez/game_shutdown_hook.hpp"
#include <Windows.h>
#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
using namespace cojvr::games::call_of_juarez;
constexpr auto exe_hash = "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE";
constexpr auto engine_hash = "DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8";
std::atomic<bool> owner_cleaned{false};
bool original_saw_cleanup = false;
unsigned original_calls = 0, callback_calls = 0;
HANDLE stop_event{}, ready_event{};
std::thread worker;
void __cdecl OriginalDestroy() {
    ++original_calls;
    original_saw_cleanup = owner_cleaned.load();
}
void __cdecl ForeignDestroy() {}
void StopOwner(void*) noexcept {
    ++callback_calls;
    SetEvent(stop_event);
    worker.join();
    (void)RestoreGameShutdownHook(); // The real finalizer restores from inside its callback.
}
int Fail(const char* text) { std::cerr << text << '\n'; return 1; }
}

int main() {
    using namespace cojvr::games::call_of_juarez;
    constexpr std::size_t image_size = 0x10000;
    auto* data = static_cast<std::byte*>(VirtualAlloc(nullptr, image_size,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!data) return Fail("fixture allocation failed");
    std::span<std::byte> image(data, image_size);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(data);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(data + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER32);
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage = image_size;
    auto* slot = reinterpret_cast<DestroyGameFunction*>(data + 0x9034);
    *slot = &OriginalDestroy;
    data[0x2105] = std::byte{0xff};
    data[0x2106] = std::byte{0x15};
    const auto slot_address = reinterpret_cast<std::uintptr_t>(slot);
    static_assert(sizeof(slot_address) == 4);
    std::memcpy(data + 0x2107, &slot_address, 4);
    auto install = [&](std::string_view exe, std::string_view engine) {
        return InstallGameShutdownHookForImage(image, exe, engine,
            &OriginalDestroy, &StopOwner, nullptr);
    };
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
    if (install(exe_hash, engine_hash) != GameShutdownHookStatus::invalid_image ||
        *slot != &OriginalDestroy) return Fail("wrong API bitness was accepted");
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    if (install("unknown", engine_hash) != GameShutdownHookStatus::unsupported_build ||
        install(exe_hash, "unknown") != GameShutdownHookStatus::unsupported_build ||
        *slot != &OriginalDestroy) return Fail("unknown hash mutated an import");
    data[0x2105] = std::byte{0x90};
    if (install(exe_hash, engine_hash) != GameShutdownHookStatus::invalid_image ||
        *slot != &OriginalDestroy) return Fail("changed call site was accepted");
    data[0x2105] = std::byte{0xff};
    if (InstallGameShutdownHookForImage(image.first(0x2200), exe_hash, engine_hash,
            &OriginalDestroy, &StopOwner, nullptr) != GameShutdownHookStatus::invalid_image)
        return Fail("truncated image was accepted");
    *slot = &ForeignDestroy;
    if (install(exe_hash, engine_hash) != GameShutdownHookStatus::target_mismatch ||
        *slot != &ForeignDestroy) return Fail("foreign import was overwritten");
    *slot = &OriginalDestroy;
    DWORD old_protection{};
    if (!VirtualProtect(data + 0x9000, 0x1000, PAGE_READONLY, &old_protection))
        return Fail("fixture protection failed");
    if (install(exe_hash, engine_hash) != GameShutdownHookStatus::installed ||
        *slot == &OriginalDestroy) return Fail("exact-build pre-exit hook not installed");
    MEMORY_BASIC_INFORMATION memory{};
    VirtualQuery(slot, &memory, sizeof(memory));
    if (memory.Protect != PAGE_READONLY) return Fail("IAT protection not restored");
    if (install(exe_hash, engine_hash) != GameShutdownHookStatus::installed)
        return Fail("repeat installation was not idempotent");
    stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ready_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stop_event || !ready_event) return Fail("owner event creation failed");
    worker = std::thread([] {
        SetEvent(ready_event);
        WaitForSingleObject(stop_event, INFINITE);
        owner_cleaned.store(true);
    });
    if (WaitForSingleObject(ready_event, 5000) != WAIT_OBJECT_0)
        return Fail("owner did not become ready");
    auto cached_hook = *slot;
    cached_hook();
    cached_hook(); // A cached caller must still forward without repeating finalization.
    if (!original_saw_cleanup || original_calls != 2 || callback_calls != 1 ||
        *slot != &OriginalDestroy) return Fail("owner shutdown did not precede original destroy exactly once");
    if (!RestoreGameShutdownHook()) return Fail("repeat restore was not idempotent");
    VirtualQuery(slot, &memory, sizeof(memory));
    if (memory.Protect != PAGE_READONLY) return Fail("restore lost IAT protection");
    if (install("C8B8BB82FCB3D6599C5F77B1BB9CB3444CBB3A360461DAD43AD808B49AD28DC9", engine_hash) != GameShutdownHookStatus::installed)
        return Fail("exact LAA-derived executable shutdown installation failed");
    VirtualProtect(data + 0x9000, 0x1000, PAGE_READWRITE, &old_protection);
    *slot = &ForeignDestroy;
    VirtualProtect(data + 0x9000, 0x1000, PAGE_READONLY, &old_protection);
    if (RestoreGameShutdownHook() || *slot != &ForeignDestroy)
        return Fail("restore overwrote a foreign hook or reported ownership success");
    CloseHandle(stop_event);
    CloseHandle(ready_event);
    VirtualFree(data, 0, MEM_RELEASE);
    std::cout << "exact-build pre-exit owner shutdown/forwarding/restoration passed\n";
    return 0;
}
