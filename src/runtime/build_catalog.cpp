#include "runtime/build_catalog.hpp"

#include <array>
#include <cctype>

namespace cojvr::runtime {
namespace {

constexpr std::array<KnownBuild, 7> kBuilds{{
    {GameId::call_of_juarez_dx9, RendererBackend::d3d9, "CoJ.exe",
     "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE"},
    // Exact original with only IMAGE_FILE_LARGE_ADDRESS_AWARE set. Staging
    // derives this image from the original and restores it transactionally.
    {GameId::call_of_juarez_dx9, RendererBackend::d3d9, "CoJ.exe",
     "C8B8BB82FCB3D6599C5F77B1BB9CB3444CBB3A360461DAD43AD808B49AD28DC9"},
    // GOG v1.1.1.0 inspected on 2026-10-27; experimental exact-build compatibility.
    {GameId::call_of_juarez_dx9, RendererBackend::d3d9, "CoJ.exe",
     "8BAB3D109BD4C08BF356D30DE1B746CDC3A89DEBF4C953763B1305FE0C8285BD"},
    {GameId::call_of_juarez_dx9, RendererBackend::d3d9, "CoJ.exe",
     "E9D06B14EB70BBC57135C1C63000E5EC7E16C277B8115184CABF29F733729C19"},
    {GameId::call_of_juarez_dx10, RendererBackend::d3d10, "CoJ_DX10.exe",
     "23EDE8E8B3BA0E9E662E83DA2B70D3F80BCADAC5BEE9554578C3AAA7AC109390"},
    {GameId::bound_in_blood, RendererBackend::d3d9, "CoJBiBGame_x86.exe",
     "5EDD55804D69AE8B48ABB2DAB7C412892EB64A86BA15BE3C1EC2C724FDA2A838"},
    {GameId::gunslinger, RendererBackend::d3d9, "CoJGunslinger.exe",
     "CA1C4766900FEB867372E0E2E87EB5ADB92CA26EE537598A034ED7BE1313D93C"},
}};

bool HexEqual(std::string_view lhs, std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        const auto a = static_cast<unsigned char>(lhs[i]);
        const auto b = static_cast<unsigned char>(rhs[i]);
        if (std::toupper(a) != std::toupper(b)) return false;
    }
    return true;
}

} // namespace

std::span<const KnownBuild> KnownBuilds() noexcept {
    return kBuilds;
}

const KnownBuild* FindKnownBuild(std::string_view sha256) noexcept {
    for (const KnownBuild& build : kBuilds) {
        if (HexEqual(build.sha256, sha256)) return &build;
    }
    return nullptr;
}

} // namespace cojvr::runtime
