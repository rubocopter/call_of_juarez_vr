#pragma once
#include "runtime/reload_gesture.hpp"
#include <array>
#include <cstdint>

namespace cojvr::games::call_of_juarez {
// Observation only. No insertion permission, ammunition cache or session state.
struct CoJReloadTraceSnapshot {
    bool valid = false;
    int armed_hand = -1;
    std::uint64_t weapon_id = 0;
    std::array<std::array<int, 4>, 2> states{}; // current/destination/desired/actual
    std::array<int, 2> animation_id{};
    std::array<float, 2> current_time{}, start_time{}, play_time{}, duration{};
    std::array<float, 2> min_advance{}, max_advance{};
    int ammo_readback = 0; // GetAmmoCount(0), NOT proof of a transfer or storage.
    int pistol_reserve = 0; // PawnInventory.nAmmoPistol, pure counter backing field.
    float drum_phase = 0;
    bool reloading = false;
    bool probe_valid = false;
    int probe_status = 0; // 1=armed, 2=waiting, 3=closing, 4=cleared; diagnostic only.
    float probe_until = 0;
};
// Exact-build local game-thread caller only. All references are local and freed.
bool ReadCoJReloadTrace(void* env, void* player, CoJReloadTraceSnapshot& out,
    bool include_wait_probe = false) noexcept;

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
    const CoJReloadTraceSnapshot& before,const CoJReloadTraceSnapshot& after) noexcept {
    return completed&&accepted&&before.valid&&after.valid&&before.probe_valid&&after.probe_valid&&
        before.weapon_id!=0&&before.weapon_id==after.weapon_id&&before.armed_hand>=0&&before.armed_hand<2&&
        before.armed_hand==after.armed_hand&&before.probe_status==2&&
        (after.probe_status==2||after.probe_status==3)&&before.ammo_readback>=0&&after.ammo_readback>=0&&
        before.pistol_reserve>0&&after.pistol_reserve>=0&&
        static_cast<std::int64_t>(after.ammo_readback)-before.ammo_readback==1&&
        static_cast<std::int64_t>(after.pistol_reserve)-before.pistol_reserve==-1;
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
