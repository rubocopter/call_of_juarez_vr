#pragma once
#include <array>
namespace cojvr::games::call_of_juarez {
// Exact concrete classes in the pinned CoJ archive. Never admit subclasses by
// prefix or assume every pistol has the same reload/ammunition contract.
enum class CoJManualReloadKind { unit,indexed,clip };
enum class CoJReloadReserveKind { pistol,rifle,shotgun };
inline const char* CoJReloadReserveName(CoJReloadReserveKind kind) noexcept {
    return kind==CoJReloadReserveKind::pistol?"pistol":kind==CoJReloadReserveKind::rifle?"rifle":
        kind==CoJReloadReserveKind::shotgun?"shotgun":"unknown";
}
// Vector positions and the native WpnAmmo type numbers are different contracts.
inline constexpr int CoJReloadReserveSlot(CoJReloadReserveKind kind) noexcept {
    switch(kind){case CoJReloadReserveKind::pistol:return 0;case CoJReloadReserveKind::rifle:return 1;
        case CoJReloadReserveKind::shotgun:return 2;}return -1;
}
inline constexpr int CoJReloadNativeAmmoType(CoJReloadReserveKind kind) noexcept {
    switch(kind){case CoJReloadReserveKind::pistol:return 2;case CoJReloadReserveKind::rifle:return 0;
        case CoJReloadReserveKind::shotgun:return 1;}return -1;
}
struct CoJRevolverProfile {
    const char* class_name;bool single_round;
    CoJReloadReserveKind reserve=CoJReloadReserveKind::pistol;
    bool native_body_cycle=false;
    bool right_owned_only=false;
    CoJManualReloadKind kind=CoJManualReloadKind::unit;
    bool manual_supported=false; // Additional manual-only families; never legacy unit pacing.
    std::array<int,2> capacities{6,0};
    int index_count=1;
    CoJReloadReserveKind secondary_reserve=CoJReloadReserveKind::pistol;
};
// Historical name; exact manual profiles now also include indexed/clip families.
inline constexpr std::array<CoJRevolverProfile,15> kCoJRevolvers{{
    {"WeaponPistolFrontier1878_Regular",true},
    {"WeaponPistolSchofield_A",false,CoJReloadReserveKind::pistol,false,false,CoJManualReloadKind::clip,true},
    {"WeaponPistolSchofield_B",false,CoJReloadReserveKind::pistol,false,false,CoJManualReloadKind::clip,true},
    {"WeaponPistolPeacemaker",true},
    {"WeaponPistolFrontier",true},{"WeaponPistolWalker",true},{"WeaponPistolCogswell",true,CoJReloadReserveKind::rifle},
    {"WeaponPistolSchofield",false,CoJReloadReserveKind::pistol,false,false,CoJManualReloadKind::clip,true},
    {"WeaponRifleWinchester",false,CoJReloadReserveKind::rifle,true,true,CoJManualReloadKind::unit,true,{12,0}},
    {"WeaponRifleWinchesterScope",false,CoJReloadReserveKind::rifle,true,true,CoJManualReloadKind::unit,true,{12,0}},
    // WpnAmmoPistolVolcanic -> WpnAmmoPistol: native type2/pistol reserve.
    {"WeaponPistolVolcanic",false,CoJReloadReserveKind::pistol,true,false,CoJManualReloadKind::unit,true,{8,0}},
    {"WeaponRifleRemington",false,CoJReloadReserveKind::shotgun,false,true,CoJManualReloadKind::indexed,true,{1,1},2,CoJReloadReserveKind::shotgun},
    {"WeaponRifleRemingtonSawOff",false,CoJReloadReserveKind::shotgun,false,false,CoJManualReloadKind::indexed,true,{1,1},2,CoJReloadReserveKind::shotgun},
    {"WeaponPistolDerringer",false,CoJReloadReserveKind::pistol,false,false,CoJManualReloadKind::indexed,true,{1,1},2,CoJReloadReserveKind::pistol},
    {"WeaponPistolLemant",false,CoJReloadReserveKind::rifle,false,false,CoJManualReloadKind::indexed,true,{1,8},2,CoJReloadReserveKind::pistol}
}};
}
