#pragma once
#include "games/call_of_juarez/revolver_profile.hpp"
#include "runtime/reload_gesture.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace cojvr::games::call_of_juarez {
// Observation only. No insertion permission, ammunition cache or session state.
struct CoJReloadTraceSnapshot {
    bool valid = false;
    int armed_hand = -1;
    std::uint64_t weapon_id = 0;
    const char* weapon_class=nullptr; // Immutable exact profile literal; no JNI reference.
    std::array<std::array<int, 4>, 2> states{}; // current/destination/desired/actual
    std::array<int, 2> animation_id{};
    std::array<float, 2> current_time{}, start_time{}, play_time{}, duration{};
    std::array<float, 2> min_advance{}, max_advance{};
    int ammo_readback = 0; // Selected native backing slot; alone never proves transfer.
    int reserve_readback = 0; // Exact profile's PawnInventory backing counter.
    CoJReloadReserveKind reserve_kind=CoJReloadReserveKind::pistol;
    bool native_body_cycle=false; // Exact profile; insertion may restart native body21.
    CoJManualReloadKind kind=CoJManualReloadKind::unit;
    int selected_index=0;
    std::array<int,2> loaded{};
    std::array<int,3> reserves{}; // pistol, rifle, shotgun; shared pool appears once.
    std::array<int,2> capacities{}; // Pinned exact archive capacities, no parameter factory.
    int index_count=1;
    std::array<CoJReloadReserveKind,2> reserve_kinds{
        CoJReloadReserveKind::pistol,CoJReloadReserveKind::pistol};
    std::array<int,2> reserve_indices{}; // Index -> reserves slot; not native ammo type.
    float drum_phase = 0;
    bool reloading = false;
    bool probe_valid = false;
    int probe_status = 0; // 1=armed, 2=waiting, 3=closing, 4=cleared; diagnostic only.
    float probe_until = 0;
};
// Exact-build local game-thread caller only. All references are local and freed.
// An optional caller-held local/global jobject binds acquisition to that exact
// weapon before reading its class, identity, counters or animation parameters.
bool ReadCoJReloadTrace(void* env, void* player, CoJReloadTraceSnapshot& out,
    bool include_wait_probe = false, int selected_index = -1,
    void* expected_weapon = nullptr) noexcept;

// Game-thread identity policy. Only the opaque epoch leaves the game adapter;
// ammunition and animation progress do not change ownership.
class CoJReloadFeedbackOwnerEpoch final {
public:
    std::uint64_t Update(std::uint64_t player, std::uint64_t context,
        const CoJReloadTraceSnapshot& native) noexcept {
        if (player == 0 || context == 0 || !native.valid || !native.probe_valid ||
            native.weapon_id == 0 || native.armed_hand < 0 || native.armed_hand > 1) {
            active_ = false;
            return 0;
        }
        if (!active_ || player != player_ || context != context_ ||
            native.weapon_id != weapon_ || native.armed_hand != hand_) {
            if (++epoch_ == 0) ++epoch_;
        }
        active_ = true;
        player_ = player; context_ = context;
        weapon_ = native.weapon_id; hand_ = native.armed_hand;
        return epoch_;
    }
private:
    bool active_ = false;
    std::uint64_t epoch_ = 0, player_ = 0, context_ = 0, weapon_ = 0;
    int hand_ = -1;
};

// Feedback qualification only: caller supplies synchronous same-player reads
// surrounding its one native call. Never grants ammunition or insertion intent.
inline bool CoJUnitManualInsertionObserved(bool completed,bool accepted,
    const CoJReloadTraceSnapshot& before,const CoJReloadTraceSnapshot& after,
    int selected_index=-1) noexcept {
    const bool owner=completed&&accepted&&before.valid&&after.valid&&before.probe_valid&&after.probe_valid&&
        before.weapon_id!=0&&before.weapon_id==after.weapon_id&&before.armed_hand>=0&&before.armed_hand<2&&
        before.armed_hand==after.armed_hand&&before.probe_status==2&&before.kind==after.kind;
    if(!owner||selected_index < -1 ||selected_index>1)return false;
    if(before.kind==CoJManualReloadKind::unit)return selected_index<=0&&
        (after.probe_status==2||after.probe_status==3||
            (after.probe_status==1&&before.native_body_cycle&&after.native_body_cycle))&&
        before.native_body_cycle==after.native_body_cycle&&before.ammo_readback>=0&&after.ammo_readback>=0&&
        before.reserve_readback>0&&after.reserve_readback>=0&&before.reserve_kind==after.reserve_kind&&
        (before.reserve_kind==CoJReloadReserveKind::pistol||before.reserve_kind==CoJReloadReserveKind::rifle)&&
        static_cast<std::int64_t>(after.ammo_readback)-before.ammo_readback==1&&
        static_cast<std::int64_t>(after.reserve_readback)-before.reserve_readback==-1;
    if(before.kind!=CoJManualReloadKind::indexed&&before.kind!=CoJManualReloadKind::clip)return false;
    const bool indexed=before.kind==CoJManualReloadKind::indexed;
    const int index=indexed?(selected_index>=0?selected_index:before.selected_index):0;
    if((!indexed&&selected_index>0)||index<0||index>=before.index_count||
        before.index_count!=(indexed?2:1)||before.index_count!=after.index_count||
        before.capacities!=after.capacities||before.reserve_indices!=after.reserve_indices||
        before.reserve_kinds!=after.reserve_kinds||before.native_body_cycle!=after.native_body_cycle||
        (indexed?!(after.probe_status==2||after.probe_status==3||
            (after.probe_status==1&&before.native_body_cycle&&after.native_body_cycle)):
            (after.probe_status!=3||before.native_body_cycle||after.native_body_cycle)))return false;
    for(int slot=0;slot<2;++slot){
        if(slot>=before.index_count){if(before.loaded[slot]||after.loaded[slot]||before.capacities[slot])return false;continue;}
        if(before.capacities[slot]<=0||before.loaded[slot]<0||after.loaded[slot]<0||
            before.loaded[slot]>before.capacities[slot]||after.loaded[slot]>after.capacities[slot]||
            before.reserve_indices[slot]!=CoJReloadReserveSlot(before.reserve_kinds[slot]))return false;
    }
    const int pool=before.reserve_indices[index];
    if(pool<0||pool>2)return false;
    const auto available=static_cast<std::int64_t>(before.reserves[pool]);
    const auto missing=static_cast<std::int64_t>(before.capacities[index])-before.loaded[index];
    const auto amount=indexed?std::int64_t{1}:std::min(missing,available);
    if(available<=0||missing<=0||amount<=0||available<amount)return false;
    for(int slot=0;slot<2;++slot)
        if(static_cast<std::int64_t>(after.loaded[slot])-before.loaded[slot]!=(slot==index?amount:0))return false;
    for(int p=0;p<3;++p)
        if(before.reserves[p]<0||after.reserves[p]<0||
            static_cast<std::int64_t>(after.reserves[p])-before.reserves[p]!=(p==pool?-amount:0))return false;
    return true;
}

struct CoJReloadTraceSample {
    CoJReloadTraceSnapshot native{};
    std::uint64_t player = 0, context = 0, sequence = 0;
    runtime::MotionReloadGestureStage gesture = runtime::MotionReloadGestureStage::awaiting_release;
    bool allowed = false, cartridge = false, in_zone = false, request = false;
    bool manual_session = false, vr_presentation_requested = false;
    // A literal supplied by the caller: policy/context evidence, not guessed state.
    const char* reason = "none";
};
struct CoJReloadTraceEvent {
    bool emit = false, comparable = false;
    CoJReloadTraceSample before{}, after{};
};
class CoJReloadTraceEvents final {
public:
    CoJReloadTraceEvent Update(bool enabled, const CoJReloadTraceSample& sample) noexcept;
    void Reset() noexcept { have_ = false; previous_ = {}; }
private:
    bool have_ = false;
    CoJReloadTraceSample previous_{};
};
}
