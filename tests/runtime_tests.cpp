#include "runtime/build_catalog.hpp"
#include "runtime/build_identity.hpp"
#include "runtime/game_id.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    using namespace cojvr::runtime;

    if (ClassifyExecutable(L"CoJ.exe") != GameId::call_of_juarez_dx9) return Fail("CoJ.exe classification failed");
    if (ClassifyExecutable(L"coj_dx10.EXE") != GameId::call_of_juarez_dx10) return Fail("DX10 classification failed");
    if (ClassifyExecutable(L"CoJBiBGame_x86.exe") != GameId::bound_in_blood) return Fail("Bound in Blood classification failed");
    if (ClassifyExecutable(L"CoJGunslinger.exe") != GameId::gunslinger) return Fail("Gunslinger classification failed");
    if (ClassifyExecutable(L"unrelated.exe") != GameId::unknown) return Fail("unknown classification failed");

    const auto builds = KnownBuilds();
    if (builds.size() != 4) return Fail("known build count mismatch");
    for (const auto& build : builds) {
        if (build.sha256.size() != 64) return Fail("invalid SHA-256 length in catalog");
        if (FindKnownBuild(build.sha256) != &build) return Fail("catalog lookup failed");
    }

    const std::filesystem::path temp = std::filesystem::temp_directory_path() / L"cojvr_sha256_test.txt";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out << "abc";
    }
    const auto digest = Sha256File(temp);
    std::error_code ec;
    std::filesystem::remove(temp, ec);
    if (!digest) return Fail("SHA-256 calculation failed");
    if (*digest != "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD") {
        return Fail("SHA-256 result mismatch");
    }

    std::cout << "runtime tests passed\n";
    return 0;
}
