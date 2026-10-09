#include "games/call_of_juarez/motion_reload_owner.hpp"
#include "games/call_of_juarez/reload_trace.hpp"
#include <array>
#include <cstddef>
#include <cmath>
#include <cstring>
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
bool ReadCoJReloadTrace(void* env, void* player, CoJReloadTraceSnapshot& out,
    const bool include_wait_probe) noexcept {
    out = {};
    if (!env || !player || !*static_cast<void***>(env)) return false;
    constexpr int required[]{6,15,17,23,24,31,33,36,39,51,57,94,95,100,102};
    for (int i : required) if (!(*static_cast<void***>(env))[i]) return false;
    if (!Clean(env)) return false;
    using CallFloat = float(COJ_RELOAD_JNICALL*)(void*,void*,void*,const Value*);
    using GetField = void*(COJ_RELOAD_JNICALL*)(void*,void*,void*);
    using GetInt = std::int32_t(COJ_RELOAD_JNICALL*)(void*,void*,void*);
    using GetFloat = float(COJ_RELOAD_JNICALL*)(void*,void*,void*);
    Locals locals{env};
    bool ok = true;
    const auto type = [&](void* object) {
        void* result = locals.Keep(Fn<Class>(env,31)(env,object));
        ok = Clean(env) && result; return result;
    };
    const auto method = [&](void* cls, const char* name, const char* descriptor) {
        if (!ok) return static_cast<void*>(nullptr);
        void* result = Fn<Method>(env,33)(env,cls,name,descriptor);
        ok = Clean(env) && result; return result;
    };
    const auto same = [&](void* a, void* b) {
        if (!ok) return false;
        const bool equal = Fn<Same>(env,24)(env,a,b) != 0;
        ok = Clean(env); return ok && equal;
    };
    const auto object = [&](void* owner, void* id, const Value* arg) {
        if (!ok) return static_cast<void*>(nullptr);
        void* result = locals.Keep(Fn<Object>(env,36)(env,owner,id,arg));
        ok = Clean(env); return result;
    };
    const auto integer = [&](void* owner, void* id, const Value* arg) {
        if (!ok) return 0;
        const int result = Fn<Integer>(env,51)(env,owner,id,arg);
        ok = Clean(env); return result;
    };
    const auto field = [&](void* cls, const char* name, const char* descriptor) {
        if (!ok) return static_cast<void*>(nullptr);
        void* result = Fn<Method>(env,94)(env,cls,name,descriptor);
        ok = Clean(env) && result; return result;
    };
    const auto real_field = [&](void* owner, void* cls, const char* name) {
        void* id = field(cls,name,"F");
        if (!ok) return 0.F;
        const float result = Fn<GetFloat>(env,102)(env,owner,id);
        ok = Clean(env) && std::isfinite(result); return result;
    };
    void* player_type = type(player);
    if (!ok) return false;
    std::array<void*,3> weapon_methods{};
    for (int i=0;i<3;++i) weapon_methods[i]=method(player_type,getters[i].name,getters[i].signature);
    std::array<std::array<void*,3>,2> weapons{};
    for (int h=0;h<2;++h) {
        Value arg{.integer=h};
        for (int i=0;i<3;++i) weapons[h][i]=object(player,weapon_methods[i],&arg);
    }
    if (!ok || (weapons[0][0]!=nullptr)==(weapons[1][0]!=nullptr)) return false;
    const int armed = weapons[0][0] ? 0 : 1;
    void* weapon = weapons[armed][0];
    for (int i=0;i<3;++i)
        if (weapons[1-armed][i] || !same(weapon,weapons[armed][i])) return false;
    void* weapon_type = type(weapon);
    if (!ok) return false;
    bool recognized = false;
    for (const char* name : {"WeaponPistolPeacemaker","WeaponPistolFrontier1878_Regular"}) {
        void* cls = locals.Keep(Fn<Find>(env,6)(env,name));
        if (!Clean(env) || !cls) return false;
        const bool matches = same(weapon_type,cls);
        if (!ok) return false;
        recognized |= matches;
    }
    if (!recognized) return false;
    void* owner_id = field(weapon_type,"cOwner","LPawnInventory;");
    if (!ok) return false;
    void* owner = locals.Keep(Fn<GetField>(env,95)(env,weapon,owner_id));
    if (!Clean(env) || !same(owner,player)) return false;
    void* id_method = method(weapon_type,"GetThisID","()I");
    const int identity = integer(weapon,id_method,nullptr);
    if (!ok || identity<=0) return false;
    CoJReloadTraceSnapshot value{};
    value.armed_hand=armed; value.weapon_id=static_cast<std::uint32_t>(identity);
    std::array<void*,4> state_methods{};
    for (int i=0;i<4;++i) state_methods[i]=method(player_type,getters[3+i].name,"(I)I");
    void* machine_method = method(player_type,"GetHandStateMashine","(I)LPlayerStateMashine;");
    for (int h=0;h<2;++h) {
        Value arg{.integer=h};
        for (int i=0;i<4;++i) value.states[h][i]=integer(player,state_methods[i],&arg);
        void* machine = object(player,machine_method,&arg);
        if (!ok || !machine) return false;
        void* cls = type(machine);
        void* animation = field(cls,"m_iAnimID0","I");
        if (!ok) return false;
        value.animation_id[h]=Fn<GetInt>(env,100)(env,machine,animation);
        if (!Clean(env)) return false;
        value.current_time[h]=real_field(machine,cls,"m_fCurrentTime");
        value.start_time[h]=real_field(machine,cls,"m_fTime0");
        value.play_time[h]=real_field(machine,cls,"m_fPlayTime");
        value.duration[h]=real_field(machine,cls,"m_fDuration");
        for (int i=0;i<2;++i) {
            void* advance = method(cls,i==0?"GetMinAnimAdvance":"GetMaxAnimAdvance","()F");
            if (!ok) return false;
            const float result = Fn<CallFloat>(env,57)(env,machine,advance,nullptr);
            if (!Clean(env) || !std::isfinite(result) || result<0.F || result>1.F) return false;
            (i==0?value.min_advance[h]:value.max_advance[h])=result;
        }
    }
    void* ammo_method = method(weapon_type,"GetAmmoCount","(I)I");
    Value slot{.integer=0};
    value.ammo_readback=integer(weapon,ammo_method,&slot);
    void* reserve = field(player_type,"nAmmoPistol","I");
    if (!ok) return false;
    value.pistol_reserve=Fn<GetInt>(env,100)(env,player,reserve);
    if (!Clean(env) || value.ammo_readback<0 || value.pistol_reserve<0) return false;
    value.drum_phase=real_field(weapon,weapon_type,"m_fRotate");
    void* reload = method(player_type,"IsWeaponReloading","()Z");
    if (!ok) return false;
    value.reloading=Fn<Boolean>(env,39)(env,player,reload,nullptr)!=0;
    if (!Clean(env)) return false;
    if (include_wait_probe) {
        void* status = field(player_type,"cojvrReloadProbeStatus","I");
        if (!ok) return false;
        value.probe_status=Fn<GetInt>(env,100)(env,player,status);
        if (!Clean(env) || value.probe_status<0 || value.probe_status>4) return false;
        value.probe_until=real_field(player,player_type,"cojvrReloadProbeUntil");
        if (!ok) return false;
        value.probe_valid=true;
    }
    // Observation must never publish values against a replacement weapon.
    Value hand{.integer=armed};
    for (void* getter : weapon_methods)
        if (!same(weapon,object(player,getter,&hand))) return false;
    if (!ok || integer(weapon,id_method,nullptr)!=identity || !ok) return false;
    value.valid=true; out=value; return true;
}
CoJReloadTraceEvent CoJReloadTraceEvents::Update(bool enabled, const CoJReloadTraceSample& sample) noexcept {
    if (!enabled) { Reset(); return {}; }
    if (have_ && sample.sequence<previous_.sequence) {
        Reset();
        return {};
    }
    // Native dispatch can acquire blocking UI within this same captured frame.
    // Permit that terminal loss once; duplicate polls cannot replay requests.
    if (have_ && sample.sequence==previous_.sequence &&
        !(previous_.allowed && !sample.allowed)) return {};
    const bool comparable = have_ && previous_.allowed && sample.allowed &&
        previous_.native.valid && sample.native.valid && previous_.player==sample.player &&
        previous_.context==sample.context && previous_.native.weapon_id==sample.native.weapon_id &&
        previous_.native.armed_hand==sample.native.armed_hand;
    const bool changed = !have_ || previous_.allowed!=sample.allowed ||
        previous_.manual_session!=sample.manual_session ||
        previous_.vr_presentation_requested!=sample.vr_presentation_requested ||
        previous_.player!=sample.player || previous_.context!=sample.context ||
        previous_.gesture!=sample.gesture || previous_.cartridge!=sample.cartridge ||
        previous_.in_zone!=sample.in_zone || previous_.native.valid!=sample.native.valid ||
        std::strcmp(previous_.reason?previous_.reason:"unknown",sample.reason?sample.reason:"unknown")!=0 ||
        (sample.native.valid && (previous_.native.weapon_id!=sample.native.weapon_id ||
        previous_.native.armed_hand!=sample.native.armed_hand || previous_.native.states!=sample.native.states ||
        previous_.native.animation_id!=sample.native.animation_id || previous_.native.reloading!=sample.native.reloading ||
        previous_.native.ammo_readback!=sample.native.ammo_readback ||
        previous_.native.probe_valid!=sample.native.probe_valid ||
        previous_.native.probe_status!=sample.native.probe_status ||
        previous_.native.pistol_reserve!=sample.native.pistol_reserve));
    const bool request_edge=sample.request && (!have_ || !previous_.request);
    CoJReloadTraceEvent event{changed || request_edge,comparable,previous_,sample};
    previous_=sample; have_=true; return event;
}
}
