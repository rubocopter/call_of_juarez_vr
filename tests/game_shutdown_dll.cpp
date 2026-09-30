#include "games/call_of_juarez/game_shutdown_hook.hpp"
#include <Windows.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {
using namespace cojvr::games::call_of_juarez;
struct Owner {
    char path[32768]{};
    HANDLE ready{}, stop{};
    std::thread worker;
    std::atomic_bool cleaned{false};
    DestroyGameFunction* import{};
};
Owner& owner = *new Owner;
void Log(const char* text) noexcept {
    HANDLE file = CreateFileA(owner.path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written{};
    WriteFile(file, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
    CloseHandle(file);
}
void Stop() noexcept {
    SetEvent(owner.stop);
    if (owner.worker.joinable()) owner.worker.join();
    Log(owner.cleaned.load() ? "owner_cleaned=true\r\n" : "owner_cleaned=false\r\n");
}
void AtExit() { Log("dll_atexit\r\n"); Stop(); }
void BeforeDestroy(void*) noexcept {
    Log("before_destroy\r\n");
    Stop();
    Log(RestoreGameShutdownHook() ? "hook_restored\r\n" : "hook_restore_failed\r\n");
}
void __cdecl OriginalDestroy() { Log("original_destroy\r\n"); }
}

extern "C" __declspec(dllexport) BOOL __cdecl Start(const char* path, BOOL hooked) {
    lstrcpynA(owner.path, path, 32768);
    owner.ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    owner.stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!owner.ready || !owner.stop) return FALSE;
    owner.worker = std::thread([] {
        Log("worker_ready\r\n");
        SetEvent(owner.ready);
        WaitForSingleObject(owner.stop, INFINITE);
        Log("worker_cleanup\r\n");
        owner.cleaned.store(true);
    });
    if (WaitForSingleObject(owner.ready, 5000) != WAIT_OBJECT_0 || std::atexit(AtExit) != 0)
        return FALSE;
    auto* image = static_cast<std::byte*>(VirtualAlloc(nullptr, 0x10000,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!image) return FALSE;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(image + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER32);
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage = 0x10000;
    owner.import = reinterpret_cast<DestroyGameFunction*>(image + 0x9034);
    *owner.import = &OriginalDestroy;
    image[0x2105] = std::byte{0xff};
    image[0x2106] = std::byte{0x15};
    const auto operand = reinterpret_cast<std::uintptr_t>(owner.import);
    std::memcpy(image + 0x2107, &operand, 4);
    if (!hooked) return TRUE;
    return InstallGameShutdownHookForImage({image, 0x10000},
        "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE",
        "DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8",
        &OriginalDestroy, &BeforeDestroy, nullptr) == GameShutdownHookStatus::installed;
}
extern "C" __declspec(dllexport) void __cdecl Destroy() { (*owner.import)(); }
BOOL WINAPI DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
