#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace cojvr::games::call_of_juarez {

// Only copied values leave the JNI owner thread. Zero ID means a successfully
// observed null cache when valid is true; -1 is the native no-element sentinel.
struct CoJInteractionSelection {
    bool valid = false;
    std::int32_t active_id = 0;
    std::int32_t executing_id = 0;
    std::int32_t active_element = -1;
    bool range_valid = false;
    float native_range_cm = 0.0F;
    bool disabled_valid = false;
    bool detection_disabled = false;
};

// Caller supplies an admitted exact-build BeingTriggered player on its JNI
// thread and retains player/context ownership around this call. Original
// BeingTriggered.GetActiveTrigger()/GetExecutingTrigger() are five-byte cached
// field getters. NEVER substitute Actor's overloaded GetActiveTrigger: it traces
// and CanActivate mutates nActiveElement. GetThisID() is the passive native ID
// cache already used for weapon identity. This reader grants no action permission
// and performs no native selection, update, mutation, retry or global retention.
inline bool ReadCoJInteractionSelection(void* env, void* being,
    CoJInteractionSelection& out) noexcept {
    out = {};
    if (!env || !being) return false;
    const auto table = *static_cast<void***>(env);
    if (!table) return false;

#if defined(_WIN32)
#define COJVR_SELECTION_JNICALL __stdcall
#else
#define COJVR_SELECTION_JNICALL
#endif
    using ClassFn = void*(COJVR_SELECTION_JNICALL*)(void*, void*);
    using MemberFn = void*(COJVR_SELECTION_JNICALL*)(void*, void*, const char*, const char*);
    using ObjectFn = void*(COJVR_SELECTION_JNICALL*)(void*, void*, void*, const void*);
    using IntCallFn = std::int32_t(COJVR_SELECTION_JNICALL*)(void*, void*, void*, const void*);
    using IntFieldFn = std::int32_t(COJVR_SELECTION_JNICALL*)(void*, void*, void*);
    using FloatFieldFn = float(COJVR_SELECTION_JNICALL*)(void*, void*, void*);
    using BooleanFieldFn = std::uint8_t(COJVR_SELECTION_JNICALL*)(void*, void*, void*);
    using SameFn = std::uint8_t(COJVR_SELECTION_JNICALL*)(void*, void*, void*);
    using ExceptionFn = void*(COJVR_SELECTION_JNICALL*)(void*);
    using ClearFn = void(COJVR_SELECTION_JNICALL*)(void*);
    using DeleteFn = void(COJVR_SELECTION_JNICALL*)(void*, void*);
#undef COJVR_SELECTION_JNICALL

    // Stable shipped JNI 1.4 ABI; no build dependency on a system JDK.
    const auto get_class = reinterpret_cast<ClassFn>(table[31]);
    const auto get_method = reinterpret_cast<MemberFn>(table[33]);
    const auto call_object = reinterpret_cast<ObjectFn>(table[36]);
    const auto call_int = reinterpret_cast<IntCallFn>(table[51]);
    const auto get_field = reinterpret_cast<MemberFn>(table[94]);
    const auto get_int = reinterpret_cast<IntFieldFn>(table[100]);
    const auto get_float = reinterpret_cast<FloatFieldFn>(table[102]);
    const auto get_boolean = reinterpret_cast<BooleanFieldFn>(table[96]);
    const auto same = reinterpret_cast<SameFn>(table[24]);
    const auto exception = reinterpret_cast<ExceptionFn>(table[15]);
    const auto clear = reinterpret_cast<ClearFn>(table[17]);
    const auto delete_local = reinterpret_cast<DeleteFn>(table[23]);
    if (!get_class || !get_method || !call_object || !call_int || !get_field ||
        !get_int || !same || !exception || !clear || !delete_local) return false;

    const auto failed = [&]() noexcept {
        void* thrown = exception(env);
        if (!thrown) return false;
        clear(env);
        delete_local(env, thrown);
        return true;
    };
    if (failed()) return false;

    struct Locals {
        void* env;
        DeleteFn release;
        std::array<void*, 7> refs{}; // player class, two targets/classes, two rechecks
        std::size_t count = 0;
        void* Keep(void* ref) noexcept {
            if (ref) refs[count++] = ref;
            return ref;
        }
        ~Locals() noexcept {
            while (count) release(env, refs[--count]);
        }
    } locals{env, delete_local};

    void* cls = locals.Keep(get_class(env, being));
    if (failed() || !cls) return false;
    void* active_method = get_method(env, cls, "GetActiveTrigger", "()LTriggerObject;");
    if (failed() || !active_method) return false;
    void* executing_method = get_method(env, cls, "GetExecutingTrigger", "()LTriggerObject;");
    if (failed() || !executing_method) return false;
    void* active = locals.Keep(call_object(env, being, active_method, nullptr));
    if (failed()) return false;
    void* executing = locals.Keep(call_object(env, being, executing_method, nullptr));
    if (failed()) return false;

    CoJInteractionSelection observed{};
    void* element_field = nullptr;
    const auto read_target = [&](void* target, std::int32_t& id,
        bool read_element) noexcept {
        if (!target) return true;
        void* target_cls = locals.Keep(get_class(env, target));
        if (failed() || !target_cls) return false;
        void* id_method = get_method(env, target_cls, "GetThisID", "()I");
        if (failed() || !id_method) return false;
        id = call_int(env, target, id_method, nullptr);
        // JNI int IDs are finite by representation. Non-null native owners must
        // have a positive ID; retain signed 32-bit values without float conversion.
        if (failed() || id <= 0) return false;
        if (read_element) {
            element_field = get_field(env, target_cls, "nActiveElement", "I");
            if (failed() || !element_field) return false;
            observed.active_element = get_int(env, target, element_field);
            if (failed() || observed.active_element < -1) return false;
        }
        return true;
    };
    if (!read_target(active, observed.active_id, true) ||
        !read_target(executing, observed.executing_id, false)) return false;

    // Optional caches fail independently. No inference/tuning of native range
    // and no eligibility call to compensate for missing metadata.
    if (get_float) {
        void* field = get_field(env, cls, "m_fTriggerVisibilityRange", "F");
        const bool lookup_failed = failed();
        if (!lookup_failed && field) {
            const float range = get_float(env, being, field);
            if (!failed() && std::isfinite(range) && range >= 0.0F) {
                observed.native_range_cm = range;
                observed.range_valid = true;
            }
        }
    }
    if (get_boolean) {
        void* field = get_field(env, cls, "m_bDisableTriggerDetection", "Z");
        const bool lookup_failed = failed();
        if (!lookup_failed && field) {
            const auto disabled = get_boolean(env, being, field);
            if (!failed() && disabled <= 1) {
                observed.detection_disabled = disabled != 0;
                observed.disabled_valid = true;
            }
        }
    }

    // Revalidate both cached identities, including null, through JNI semantics.
    // Handle addresses can differ for the same object. A change discards the
    // entire snapshot; never retry or force a selection refresh.
    void* active_now = locals.Keep(call_object(env, being, active_method, nullptr));
    if (failed()) return false;
    void* executing_now = locals.Keep(call_object(env, being, executing_method, nullptr));
    if (failed()) return false;
    const bool active_same = same(env, active, active_now) != 0;
    if (failed() || !active_same) return false;
    const bool executing_same = same(env, executing, executing_now) != 0;
    if (failed() || !executing_same) return false;
    if (active) {
        const auto element_now = get_int(env, active, element_field);
        if (failed() || element_now != observed.active_element) return false;
    }
    observed.valid = true;
    out = observed;
    return true;
}

} // namespace cojvr::games::call_of_juarez
