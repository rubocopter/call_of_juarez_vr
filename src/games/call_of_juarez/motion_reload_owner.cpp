#include "games/call_of_juarez/motion_reload_owner.hpp"
#include <array>
#include <cstddef>
#if defined(_WIN32)
#define COJ_RELOAD_JNICALL __stdcall
#else
#define COJ_RELOAD_JNICALL
#endif
namespace cojvr::games::call_of_juarez {
namespace {
union Value { std::int64_t alignment; void* object; std::int32_t integer; };
static_assert(sizeof(Value) == 8);
using Find = void*(COJ_RELOAD_JNICALL*)(void*, const char*);
using Exception = void*(COJ_RELOAD_JNICALL*)(void*);
using Clear = void(COJ_RELOAD_JNICALL*)(void*);
using Delete = void(COJ_RELOAD_JNICALL*)(void*, void*);
using Same = std::uint8_t(COJ_RELOAD_JNICALL*)(void*, void*, void*);
using Class = void*(COJ_RELOAD_JNICALL*)(void*, void*);
using Method = void*(COJ_RELOAD_JNICALL*)(void*, void*, const char*, const char*);
using Object = void*(COJ_RELOAD_JNICALL*)(void*, void*, void*, const Value*);
using Boolean = std::uint8_t(COJ_RELOAD_JNICALL*)(void*, void*, void*, const Value*);
using Integer = std::int32_t(COJ_RELOAD_JNICALL*)(void*, void*, void*, const Value*);
template<class T> T Fn(void* env, std::size_t index) noexcept {
    return reinterpret_cast<T>((*static_cast<void***>(env))[index]);
}
struct Locals {
    void* env;
    std::array<void*, 32> refs{};
    std::size_t count = 0;
    void* Keep(void* ref) noexcept { if (ref) refs[count++] = ref; return ref; }
    ~Locals() { while (count) Fn<Delete>(env, 23)(env, refs[--count]); }
};
bool Clean(void* env) noexcept {
    void* exception = Fn<Exception>(env, 15)(env);
    if (!exception) return true;
    Fn<Clear>(env, 17)(env);
    Fn<Delete>(env, 23)(env, exception);
    return false;
}
struct Hand {
    std::array<void*, 3> weapons{};
    std::array<int, 4> states{};
    bool occupied = false;
    bool state_machine_two_hand = false;
};
struct Getter { const char* name; const char* signature; };
constexpr Getter getters[]{
    {"GetActualWeaponNotEmpty", "(I)LWeapon;"},
    {"GetActiveWeapon", "(I)LWeapon;"},
    {"GetDesiredWeapon", "(I)LWeapon;"},
    {"GetHandStateMashineState", "(I)I"},
    {"GetHandStateMashineDestinyState", "(I)I"},
    {"GetDesiredWeaponState", "(I)I"},
    {"GetActualWeaponState", "(I)I"},
    {"IsActualWeaponOperatedTwoHand", "(I)Z"},
    {"IsDesiredWeaponOperatedTwoHand", "(I)Z"},
    {"IsHandStateMashineOperatedTwoHand", "(I)Z"},
    {"HasSomethingInHand", "(I)Z"},
    {"GetAttackState", "(I)Z"},
    {"IsNotAlive", "()Z"},
    {"IsWeaponReloading", "()Z"}
};
constexpr const char* known_pistol_classes[]{
    "WeaponPistolFrontier1878_Regular",
    "WeaponPistolSchofield_A",
    "WeaponPistolSchofield_B",
    "WeaponPistolCogswell",
    "WeaponPistolDerringer",
    "WeaponPistolDoubleAmmo",
    "WeaponPistolFrontier",
    "WeaponPistolFrontier1878_Regular_Old",
    "WeaponPistolLemant",
    "WeaponPistolPeacemaker",
    "WeaponPistolSchofield",
    "WeaponPistolSchofield_A_Old",
    "WeaponPistolSchofield_B_Old",
    "WeaponPistolVolcanic",
    "WeaponPistolWalker",
    "WeaponPistol",
};
}
const char* CoJMotionReloadRejectReasonName(CoJMotionReloadRejectReason reason) noexcept {
    switch (reason) {
    case CoJMotionReloadRejectReason::none: return "none";
    case CoJMotionReloadRejectReason::invalid_context: return "invalid_context";
    case CoJMotionReloadRejectReason::jni_unavailable: return "jni_unavailable";
    case CoJMotionReloadRejectReason::jni_failure: return "jni_failure";
    case CoJMotionReloadRejectReason::dead_or_reloading: return "dead_or_reloading";
    case CoJMotionReloadRejectReason::two_handed: return "two_handed";
    case CoJMotionReloadRejectReason::no_armed_weapon: return "no_armed_weapon";
    case CoJMotionReloadRejectReason::support_occupied: return "support_occupied";
    case CoJMotionReloadRejectReason::support_weapon: return "support_weapon";
    case CoJMotionReloadRejectReason::support_state: return "support_state";
    case CoJMotionReloadRejectReason::armed_state: return "armed_state";
    case CoJMotionReloadRejectReason::weapon_identity: return "weapon_identity";
    case CoJMotionReloadRejectReason::unsupported_weapon: return "unsupported_weapon";
    case CoJMotionReloadRejectReason::invalid_weapon_id: return "invalid_weapon_id";
    case CoJMotionReloadRejectReason::owner_changed: return "owner_changed";
    }
    return "unknown";
}

bool ReadCoJMotionReloadWeaponClassName(void* env, void* player, int hand,
    const char*& out_name) noexcept {
    out_name = nullptr;
    if (!env || !player || (hand != 0 && hand != 1) || !*static_cast<void***>(env))
        return false;
    constexpr int required[]{6, 15, 17, 23, 24, 31, 33, 36};
    for (int index : required) if (!(*static_cast<void***>(env))[index]) return false;
    if (!Clean(env)) return false;

    Locals locals{env};
    void* player_class = locals.Keep(Fn<Class>(env, 31)(env, player));
    if (!Clean(env) || !player_class) return false;
    void* get_weapon = Fn<Method>(env, 33)(env, player_class,
        "GetActualWeaponNotEmpty", "(I)LWeapon;");
    if (!Clean(env) || !get_weapon) return false;
    Value arg{.integer = hand};
    void* weapon = locals.Keep(Fn<Object>(env, 36)(env, player, get_weapon, &arg));
    if (!Clean(env) || !weapon) return false;
    void* weapon_class = locals.Keep(Fn<Class>(env, 31)(env, weapon));
    if (!Clean(env) || !weapon_class) return false;

    // Best-effort diagnostic inventory from the exact shipped class audit.
    // Missing optional classes are ignored after clearing FindClass exceptions.
    for (const char* name : known_pistol_classes) {
        void* candidate = Fn<Find>(env, 6)(env, name);
        const bool lookup_clean = Clean(env);
        if (!candidate || !lookup_clean) {
            if (candidate) Fn<Delete>(env, 23)(env, candidate);
            continue;
        }
        const bool equal = Fn<Same>(env, 24)(env, weapon_class, candidate) != 0;
        const bool compare_clean = Clean(env);
        Fn<Delete>(env, 23)(env, candidate);
        if (compare_clean && equal) {
            out_name = name;
            return true;
        }
    }
    return false;
}

static bool ReadCoJReloadOwner(void* env, void* player, CoJMotionReloadOwner& out,
    CoJMotionReloadDiagnostic* diagnostic, const bool native_recovery) noexcept {
    out = {};
    if (diagnostic) *diagnostic = {};
    const auto reject = [&](CoJMotionReloadRejectReason reason) noexcept {
        if (diagnostic) diagnostic->reason = reason;
        return false;
    };
    if (!env || !player || !*static_cast<void***>(env))
        return reject(CoJMotionReloadRejectReason::invalid_context);
    constexpr int required[]{6, 15, 17, 23, 24, 31, 33, 36, 39, 51};
    for (int index : required) if (!(*static_cast<void***>(env))[index])
        return reject(CoJMotionReloadRejectReason::jni_unavailable);
    if (!Clean(env)) return reject(CoJMotionReloadRejectReason::jni_failure);
    Locals locals{env};
    void* player_class = locals.Keep(Fn<Class>(env, 31)(env, player));
    if (!Clean(env) || !player_class) return reject(CoJMotionReloadRejectReason::jni_failure);
    std::array<void*, std::size(getters)> methods{};
    for (std::size_t i = 0; i < methods.size(); ++i) {
        methods[i] = Fn<Method>(env, 33)(env, player_class, getters[i].name, getters[i].signature);
        if (!Clean(env) || !methods[i]) return reject(CoJMotionReloadRejectReason::jni_failure);
    }
    void* is_carrying = nullptr;
    if (native_recovery) {
        // Shipped HumanBeing.IsCarrying is only m_cCarriedObject != null.
        // HasSomethingInHand also includes opposite-hand two-hand animation;
        // it cannot independently prove that a carried object is absent.
        is_carrying = Fn<Method>(env, 33)(env, player_class, "IsCarrying", "()Z");
        if (!Clean(env) || !is_carrying)
            return reject(CoJMotionReloadRejectReason::jni_failure);
    }
    // Exact concrete classes: no unknown subclasses or parameter getters.
    std::array<void*, 4> classes{};
    constexpr const char* names[]{"WeaponPistolFrontier1878_Regular",
        "WeaponPistolSchofield_A", "WeaponPistolSchofield_B",
        "WeaponPistolPeacemaker"};
    for (std::size_t i = 0; i < classes.size(); ++i) {
        if (native_recovery && (i == 1 || i == 2)) continue;
        classes[i] = locals.Keep(Fn<Find>(env, 6)(env, names[i]));
        if (!Clean(env) || !classes[i]) return reject(CoJMotionReloadRejectReason::jni_failure);
    }
    auto same = [&](void* a, void* b) noexcept {
        const bool equal = Fn<Same>(env, 24)(env, a, b) != 0;
        return Clean(env) && equal;
    };
    auto read = [&](std::array<Hand, 2>& hands) noexcept {
        const bool dead = Fn<Boolean>(env, 39)(env, player, methods[12], nullptr) != 0;
        if (!Clean(env)) return reject(CoJMotionReloadRejectReason::jni_failure);
        if (diagnostic) diagnostic->player_dead = dead;
        if (dead) return reject(CoJMotionReloadRejectReason::dead_or_reloading);
        const bool reloading = Fn<Boolean>(env, 39)(env, player, methods[13], nullptr) != 0;
        if (!Clean(env)) return reject(CoJMotionReloadRejectReason::jni_failure);
        if (diagnostic) diagnostic->weapon_reloading = reloading;
        if (reloading && !native_recovery)
            return reject(CoJMotionReloadRejectReason::dead_or_reloading);
        if (native_recovery) {
            const bool carrying = Fn<Boolean>(env, 39)(env, player, is_carrying, nullptr) != 0;
            if (!Clean(env)) return reject(CoJMotionReloadRejectReason::jni_failure);
            if (carrying) return reject(CoJMotionReloadRejectReason::support_occupied);
        }
        for (int h = 0; h < 2; ++h) {
            Value arg{.integer = h};
            for (std::size_t i = 0; i < 3; ++i) {
                hands[h].weapons[i] = locals.Keep(Fn<Object>(env, 36)(env, player, methods[i], &arg));
                if (!Clean(env)) return reject(CoJMotionReloadRejectReason::jni_failure);
            }
            for (std::size_t i = 0; i < 4; ++i) {
                hands[h].states[i] = Fn<Integer>(env, 51)(env, player, methods[i + 3], &arg);
                if (!Clean(env)) return reject(CoJMotionReloadRejectReason::jni_failure);
            }
            for (std::size_t i = 7; i < 10; ++i) {
                const bool two = Fn<Boolean>(env, 39)(env, player, methods[i], &arg) != 0;
                if (!Clean(env)) return reject(CoJMotionReloadRejectReason::jni_failure);
                if (i == 9) hands[h].state_machine_two_hand = two;
                // Recovery reads every safety predicate before considering the
                // animation exception. The boundary below still requires an
                // exact single pistol, empty support and native reload states.
                if (two && !native_recovery) return reject(CoJMotionReloadRejectReason::two_handed);
            }
            hands[h].occupied = Fn<Boolean>(env, 39)(env, player, methods[10], &arg) != 0;
            if (!Clean(env)) return reject(CoJMotionReloadRejectReason::jni_failure);
            const bool attack = Fn<Boolean>(env, 39)(env, player, methods[11], &arg) != 0;
            if (!Clean(env)) return reject(CoJMotionReloadRejectReason::jni_failure);
            if (diagnostic) diagnostic->attacking[h] = attack;
            if (attack) return reject(CoJMotionReloadRejectReason::dead_or_reloading);
        }
        if (diagnostic) {
            for (int h = 0; h < 2; ++h) {
                diagnostic->states[h] = hands[h].states;
                diagnostic->occupied[h] = hands[h].occupied;
                for (std::size_t i = 0; i < 3; ++i)
                    diagnostic->weapon_present[h][i] = hands[h].weapons[i] != nullptr;
            }
        }
        return true;
    };
    auto eligible = [&](const std::array<Hand, 2>& hands, int armed) noexcept {
        if (diagnostic) diagnostic->armed_hand = armed;
        const auto& gun = hands[armed];
        const auto& support = hands[1 - armed];
        if (!gun.weapons[0]) return reject(CoJMotionReloadRejectReason::no_armed_weapon);
        // Only the armed state-machine flag explains the shipped opposite-hand
        // occupation fallback. Recovery already proved no carried object in
        // this snapshot; the remaining empty-support, reload and exact-identity
        // checks must all pass before publishing any recovery owner.
        if (support.occupied && !(native_recovery && gun.states[0] != 0 &&
                gun.state_machine_two_hand))
            return reject(CoJMotionReloadRejectReason::support_occupied);
        for (void* weapon : support.weapons) if (weapon)
            return reject(CoJMotionReloadRejectReason::support_weapon);
        for (int state : support.states) if (state != 0)
            return reject(CoJMotionReloadRejectReason::support_state);
        bool has_reload_state = false;
        for (int state : gun.states) {
            const bool reload_state = state == 20 || state == 21 || state == 22;
            if (state != 1 && (!native_recovery || !reload_state))
                return reject(CoJMotionReloadRejectReason::armed_state);
            has_reload_state = has_reload_state || reload_state;
        }
        // All-idle or attack/alternate recovery is not this animation boundary.
        // In particular, state32 must never be mistaken for native reload-end.
        if (native_recovery && !has_reload_state)
            return reject(CoJMotionReloadRejectReason::armed_state);
        if (!same(gun.weapons[0], gun.weapons[1]) || !same(gun.weapons[0], gun.weapons[2]))
            return reject(CoJMotionReloadRejectReason::weapon_identity);
        return true;
    };
    std::array<Hand, 2> first{}, second{};
    if (!read(first)) return false;
    const int armed = first[0].weapons[0] ? 0 : 1;
    if (!eligible(first, armed)) return false;
    void* weapon_class = locals.Keep(Fn<Class>(env, 31)(env, first[armed].weapons[0]));
    if (!Clean(env) || !weapon_class) return reject(CoJMotionReloadRejectReason::jni_failure);
    bool whitelisted = false;
    bool single_round_supported = false;
    for (std::size_t class_index = 0; class_index < classes.size(); ++class_index) {
        void* cls = classes[class_index];
        if (!cls) continue;
        const bool equal = Fn<Same>(env, 24)(env, weapon_class, cls) != 0;
        if (!Clean(env)) return reject(CoJMotionReloadRejectReason::jni_failure);
        whitelisted = whitelisted || equal;
        single_round_supported = single_round_supported || (equal && (class_index == 0 || class_index == 3));
    }
    if (!whitelisted) return reject(CoJMotionReloadRejectReason::unsupported_weapon);
    void* id_method = Fn<Method>(env, 33)(env, weapon_class, "GetThisID", "()I");
    if (!Clean(env) || !id_method) return reject(CoJMotionReloadRejectReason::jni_failure);
    const std::int32_t id = Fn<Integer>(env, 51)(env, first[armed].weapons[0], id_method, nullptr);
    if (!Clean(env)) return reject(CoJMotionReloadRejectReason::jni_failure);
    if (id <= 0) return reject(CoJMotionReloadRejectReason::invalid_weapon_id);
    // Recheck on the same game owner before publication. This rejects observed
    // replacement, without claiming cross-thread atomicity or advancing state.
    if (!read(second) || !eligible(second, armed)) return false;
    if (!same(first[armed].weapons[0], second[armed].weapons[0]))
        return reject(CoJMotionReloadRejectReason::owner_changed);
    if (native_recovery) {
        const auto second_id = Fn<Integer>(env, 51)(env, second[armed].weapons[0], id_method, nullptr);
        if (!Clean(env)) return reject(CoJMotionReloadRejectReason::jni_failure);
        if (second_id != id) return reject(CoJMotionReloadRejectReason::owner_changed);
    }
    out = {armed, static_cast<std::uint32_t>(id), true, single_round_supported};
    if (diagnostic) diagnostic->reason = CoJMotionReloadRejectReason::none;
    return true;
}
bool ReadCoJMotionReloadOwner(void* env, void* player, CoJMotionReloadOwner& out,
    CoJMotionReloadDiagnostic* diagnostic) noexcept {
    return ReadCoJReloadOwner(env, player, out, diagnostic, false);
}
bool ReadCoJNativeReloadRecoveryOwner(void* env, void* player, CoJMotionReloadOwner& out,
    CoJMotionReloadDiagnostic* diagnostic) noexcept {
    return ReadCoJReloadOwner(env, player, out, diagnostic, true);
}
}
