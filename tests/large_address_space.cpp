#include <windows.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    static_assert(sizeof(void*) == 4);
    BOOL wow64 = FALSE;
    if (!IsWow64Process(GetCurrentProcess(), &wow64) || !wow64) return 77;
    const bool extended = argc == 2 && std::string_view(argv[1]) == "extended";
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const auto ceiling = reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress);
    if ((ceiling > 0x80000000U) != extended) {
        std::cerr << "unexpected x86 user address-space ceiling: " << std::hex << ceiling << '\n';
        return 1;
    }
    void* allocation = nullptr;
    // Reserve one page-sized region at a free high address, not gigabytes of RAM.
    for (std::uint64_t address = 0x90000000ULL; address < 0xf0000000ULL; address += 0x1000000ULL) {
        allocation = VirtualAlloc(reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)),
            65536, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (allocation) break;
    }
    if (static_cast<bool>(allocation) != extended) {
        std::cerr << "high-address allocation did not match PE capacity\n";
        if (allocation) VirtualFree(allocation, 0, MEM_RELEASE);
        return 1;
    }
    if (allocation) {
        std::memset(allocation, 0x5a, 65536);
        if (static_cast<unsigned char*>(allocation)[65535] != 0x5a) return 1;
        MEMORY_BASIC_INFORMATION region{};
        if (!VirtualQuery(allocation, &region, sizeof(region)) || region.State != MEM_COMMIT) return 1;
        if (!VirtualFree(allocation, 0, MEM_RELEASE)) return 1;
    }
    std::cout << "x86 address-space capacity=" << (extended ? "4GB" : "2GB")
              << "; high-address reserve/commit/write/free=" << (extended ? "passed" : "rejected") << '\n';
    return 0;
}
