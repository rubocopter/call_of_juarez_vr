#pragma once
#include <array>
#include <cstdint>

namespace cojvr::games::call_of_juarez {
enum class CoJMotionReloadRejectReason : std::uint8_t {
    none,
    invalid_context,
    jni_unavailable,
    jni_failure,
    dead_or_reloading,
    two_handed,
    no_armed_weapon,
    support_occupied,
    support_weapon,
    support_state,
    armed_state,
    weapon_identity,
    unsupported_weapon,
    invalid_weapon_id,
    owner_changed,
};

struct CoJMotionReloadDiagnostic {
    CoJMotionReloadRejectReason reason = CoJMotionReloadRejectReason::none;
    int armed_hand = -1;
    bool player_dead = false;
    bool weapon_reloading = false;
    std::array<bool, 2> attacking{};
    std::array<std::array<int, 4>, 2> states{};
    std::array<std::array<bool, 3>, 2> weapon_present{};
    std::array<bool, 2> occupied{};
};

struct CoJMotionReloadOwner {
    int armed_hand = -1;
    std::uint64_t weapon_id = 0;
    bool valid = false;
    bool single_round_supported = false; // Exact Frontier/Peacemaker; Schofield stays hybrid.
};
// Exact-build, local-player game-owner caller only. Observation never requests
// reload or reads weapon parameters; action 31 retains ammunition authority.
bool ReadCoJMotionReloadOwner(void* env, void* player,
    CoJMotionReloadOwner& out, CoJMotionReloadDiagnostic* diagnostic = nullptr) noexcept;
// Passive ongoing animation observation, not eligibility for a new request.
// Exact Frontier/Peacemaker, coherent actual/active/desired identity, empty
// support and armed states drawn only from 1/20/21/22 with a reload state still
// present. Native two-hand animation flags may be ignored only in that boundary.
// A positive support HasSomethingInHand requires the armed state-machine
// two-hand fallback plus passive IsCarrying=false in both observations; support
// actual/active/desired weapons remain null and all support states remain0.
// Initial admission retains the original occupied-hand rejection unchanged.
// Caller retains exact-build/local-player generation and gameplay-context gates;
// the adapter must match the result to its accepted continuation owner.
bool ReadCoJNativeReloadRecoveryOwner(void* env, void* player,
    CoJMotionReloadOwner& out, CoJMotionReloadDiagnostic* diagnostic = nullptr) noexcept;
// Diagnostic-only exact-class lookup for a currently observed hand weapon.
// This never changes eligibility or requests a reload; callers should keep it
// off the per-frame path and use it only when emitting a rejection sample.
bool ReadCoJMotionReloadWeaponClassName(void* env, void* player, int hand,
    const char*& out_name) noexcept;
[[nodiscard]] const char* CoJMotionReloadRejectReasonName(
    CoJMotionReloadRejectReason reason) noexcept;
}
