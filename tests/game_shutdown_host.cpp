#include <Windows.h>
#include <cstring>
int main(int argc, char** argv) {
    if (argc != 4) return 2;
    const auto module = LoadLibraryA(argv[1]);
    if (!module) return 3;
    const auto start = reinterpret_cast<BOOL (__cdecl*)(const char*, BOOL)>(GetProcAddress(module, "Start"));
    const auto destroy = reinterpret_cast<void (__cdecl*)()>(GetProcAddress(module, "Destroy"));
    if (!start || !destroy || !start(argv[2], std::strcmp(argv[3], "hooked") == 0)) return 4;
    destroy();
    return 0; // Real ExitProcess/DLL atexit ordering after the imported boundary.
}
