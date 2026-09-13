#include "runtime/build_catalog.hpp"

#include <array>
#include <cctype>

namespace cojvr::runtime {
namespace {

constexpr std::array<KnownBuild, 4> kBuilds{{
    {GameId::call_of_juarez_dx9, RendererBackend::d3d9, "CoJ.exe",
     "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE"},
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
