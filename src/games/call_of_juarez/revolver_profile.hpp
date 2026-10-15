#pragma once
#include <array>
namespace cojvr::games::call_of_juarez {
// Exact concrete classes in the pinned CoJ archive. Never admit subclasses by
// prefix or assume every pistol has the same reload/ammunition contract.
enum class CoJReloadReserveKind { pistol,rifle };
inline const char* CoJReloadReserveName(CoJReloadReserveKind kind) noexcept {
    return kind==CoJReloadReserveKind::pistol?"pistol":kind==CoJReloadReserveKind::rifle?"rifle":"unknown";
}
struct CoJRevolverProfile {
    const char* class_name;bool single_round;
    CoJReloadReserveKind reserve=CoJReloadReserveKind::pistol;
};
inline constexpr std::array<CoJRevolverProfile,8> kCoJRevolvers{{
    {"WeaponPistolFrontier1878_Regular",true},
    {"WeaponPistolSchofield_A",false},{"WeaponPistolSchofield_B",false},
    {"WeaponPistolPeacemaker",true},
    {"WeaponPistolFrontier",true},{"WeaponPistolWalker",true},{"WeaponPistolCogswell",true,CoJReloadReserveKind::rifle},
    {"WeaponPistolSchofield",false}
}};
}
