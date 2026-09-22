#include "games/call_of_juarez/java_player_bridge.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace cojvr::games::call_of_juarez {
namespace {

constexpr std::int32_t kJniOk = 0;
constexpr std::int32_t kJniDetached = -2;
constexpr std::int32_t kJniVersion14 = 0x00010004;

// JNI 1.4 function-table indices. The shipped JRE is Java 1.4.2_10, and these
// entries are part of the stable JNI ABI. Keeping the small subset here avoids
// taking a build-time dependency on a system JDK that the game never required.
constexpr std::size_t kFindClass = 6;
constexpr std::size_t kExceptionOccurred = 15;
constexpr std::size_t kExceptionClear = 17;
constexpr std::size_t kNewGlobalRef = 21;
constexpr std::size_t kDeleteGlobalRef = 22;
constexpr std::size_t kDeleteLocalRef = 23;
constexpr std::size_t kIsSameObject = 24;
constexpr std::size_t kNewObjectA = 30;
constexpr std::size_t kGetObjectClass = 31;
constexpr std::size_t kIsInstanceOf = 32;
constexpr std::size_t kGetMethodId = 33;
constexpr std::size_t kCallObjectMethodA = 36;
constexpr std::size_t kCallBooleanMethodA = 39;
constexpr std::size_t kCallIntMethodA = 51;
constexpr std::size_t kCallFloatMethodA = 57;
constexpr std::size_t kCallVoidMethodA = 63;
constexpr std::size_t kGetFieldId = 94;
constexpr std::size_t kGetObjectField = 95;
constexpr std::size_t kGetBooleanField = 96;
constexpr std::size_t kGetIntField = 100;
constexpr std::size_t kGetFloatField = 102;
constexpr std::size_t kSetFloatField = 111;
constexpr std::size_t kGetStaticMethodId = 113;
constexpr std::size_t kCallStaticIntMethodA = 131;
constexpr std::size_t kGetStaticFieldId = 144;
constexpr std::size_t kGetStaticObjectField = 145;
constexpr std::size_t kNewStringUtf = 167;
constexpr std::size_t kGetArrayLength = 171;
constexpr std::size_t kGetObjectArrayElement = 173;

constexpr std::size_t kVmAttachCurrentThreadAsDaemon = 7;
constexpr std::size_t kVmGetEnv = 6;
constexpr std::size_t kVmDetachCurrentThread = 5;

#if defined(_WIN32)
#define COJVR_JNICALL __stdcall
#else
#define COJVR_JNICALL
#endif

// Thread-local flag to track if this thread was attached by the bridge.
static thread_local bool g_jni_thread_attached = false;

union JValue {
    std::uint8_t z;
    std::int8_t b;
    std::uint16_t c;
    std::int16_t s;
    std::int32_t i;
    std::int64_t j;
    float f;
    double d;
    void* l;
};

static_assert(sizeof(JValue) == 8);

template <typename Function>
Function EnvFunction(void* env, const std::size_t index) noexcept {
    if (!env) return nullptr;
    auto*** holder = reinterpret_cast<void***>(env);
    if (!holder || !*holder) return nullptr;
    return reinterpret_cast<Function>((*holder)[index]);
}

template <typename Function>
Function VmFunction(void* vm, const std::size_t index) noexcept {
    if (!vm) return nullptr;
    auto*** holder = reinterpret_cast<void***>(vm);
    if (!holder || !*holder) return nullptr;
    return reinterpret_cast<Function>((*holder)[index]);
}

using GetCreatedJavaVmsFn = std::int32_t(COJVR_JNICALL*)(void**, std::int32_t, std::int32_t*);
using GetEnvFn = std::int32_t(COJVR_JNICALL*)(void*, void**, std::int32_t);
using AttachCurrentThreadFn = std::int32_t(COJVR_JNICALL*)(void*, void**, void*);
using DetachCurrentThreadFn = std::int32_t(COJVR_JNICALL*)(void*);
using FindClassFn = void*(COJVR_JNICALL*)(void*, const char*);
using ExceptionOccurredFn = void*(COJVR_JNICALL*)(void*);
using ExceptionClearFn = void(COJVR_JNICALL*)(void*);
using NewGlobalRefFn = void*(COJVR_JNICALL*)(void*, void*);
using DeleteGlobalRefFn = void(COJVR_JNICALL*)(void*, void*);
using DeleteLocalRefFn = void(COJVR_JNICALL*)(void*, void*);
using IsSameObjectFn = std::uint8_t(COJVR_JNICALL*)(void*, void*, void*);
using NewObjectAFn = void*(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using GetObjectClassFn = void*(COJVR_JNICALL*)(void*, void*);
using IsInstanceOfFn = std::uint8_t(COJVR_JNICALL*)(void*, void*, void*);
using GetMethodIdFn = void*(COJVR_JNICALL*)(void*, void*, const char*, const char*);
using CallObjectMethodAFn = void*(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using CallBooleanMethodAFn = std::uint8_t(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using CallIntMethodAFn = std::int32_t(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using CallFloatMethodAFn = float(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using CallVoidMethodAFn = void(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using GetFieldIdFn = void*(COJVR_JNICALL*)(void*, void*, const char*, const char*);
using GetObjectFieldFn = void*(COJVR_JNICALL*)(void*, void*, void*);
using GetBooleanFieldFn = std::uint8_t(COJVR_JNICALL*)(void*, void*, void*);
using GetIntFieldFn = std::int32_t(COJVR_JNICALL*)(void*, void*, void*);
using GetFloatFieldFn = float(COJVR_JNICALL*)(void*, void*, void*);
using SetFloatFieldFn = void(COJVR_JNICALL*)(void*, void*, void*, float);
using GetStaticMethodIdFn = void*(COJVR_JNICALL*)(void*, void*, const char*, const char*);
using CallStaticIntMethodAFn =
    std::int32_t(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using GetStaticFieldIdFn = void*(COJVR_JNICALL*)(void*, void*, const char*, const char*);
using GetStaticObjectFieldFn = void*(COJVR_JNICALL*)(void*, void*, void*);
using NewStringUtfFn = void*(COJVR_JNICALL*)(void*, const char*);
using GetArrayLengthFn = std::int32_t(COJVR_JNICALL*)(void*, void*);
using GetObjectArrayElementFn = void*(COJVR_JNICALL*)(void*, void*, std::int32_t);

void SetError(std::string* error, const char* value) noexcept {
    if (!error) return;
    try {
        *error = value ? value : "";
    } catch (...) {
    }
}

void DeleteLocal(void* env, void* value) noexcept {
    if (!env || !value) return;
    if (const auto fn = EnvFunction<DeleteLocalRefFn>(env, kDeleteLocalRef)) fn(env, value);
}

} // namespace

const char* CoJMovementSpeedStateName(const int speed_state) noexcept {
    // EBeingSpeedStates uses bit 7 for the moving family and the low nibble
    // for Walk/Trot/Run/Sprint. Keep the raw value in telemetry as well.
    if ((speed_state & 0x80) == 0) return "other";
    switch (speed_state & 0x0F) {
    case 0: return "walk";
    case 1: return "trot";
    case 2: return "run";
    case 3: return "sprint";
    default: return "other";
    }
}

bool CoJOdeWalkStateGrounded(const int ode_walk_state) noexcept {
    // Creature.CanSetJumpState and UpdateMoveState use bit 1 for the native
    // flying interval. Report its inverse as the exact-build grounded state;
    // the raw ODE state remains in every sample for auditability.
    return (ode_walk_state & 0x02) == 0;
}

const char* CoJUiDispatchRouteName(const CoJUiDispatchRoute route) noexcept {
    switch (route) {
    case CoJUiDispatchRoute::paused_hint:
        return "LawmanModule.GetHintManager.DisableCurrentHint";
    case CoJUiDispatchRoute::global_menu:
        return "GameWithMenu.sm_cMenuModule.GetCurrentUI.Enter";
    case CoJUiDispatchRoute::active_game_menu:
        return "LawmanGame.sm_cActiveGameModule.cMenu.GetCurrentUI.Enter";
    case CoJUiDispatchRoute::active_game_module:
        return "LawmanGame.sm_cActiveGameModule.OnInputKey";
    case CoJUiDispatchRoute::loading_ui:
        return "GameUILoading.OnInputKey";
    case CoJUiDispatchRoute::intro_skip:
        return "GameWithMenu.sm_cIntroModule.OnInputKey";
    default: return "none";
    }
}

const char* CoJUiBackDispatchRouteName(const CoJUiDispatchRoute route) noexcept {
    switch (route) {
    case CoJUiDispatchRoute::global_menu:
        return "GameWithMenu.sm_cMenuModule.GetCurrentUI.CallOnInputKeyGlobal(Escape)";
    case CoJUiDispatchRoute::active_game_menu:
        return "LawmanGame.sm_cActiveGameModule.cMenu.GetCurrentUI.CallOnInputKeyGlobal(Escape)";
    case CoJUiDispatchRoute::active_game_module:
        return "LawmanGame.sm_cActiveGameModule.OnInputKey(Escape)";
    default: return "none";
    }
}

CoJUiBackDispatchPolicy BuildCoJUiBackDispatchPolicy(
    const bool current_ui_available) noexcept {
    (void)current_ui_available;
    return {
        .dispatch = true,
        .key_code = 1,
        .press = true,
        .release = true,
    };
}

bool ShouldFallbackCoJUiBack(const CoJCurrentUiResolution resolution) noexcept {
    return resolution == CoJCurrentUiResolution::unavailable;
}

CoJUiPointerDispatchPolicy BuildCoJUiPointerDispatchPolicy(
    const bool menu_available,
    const bool current_ui_available) noexcept {
    return {
        .dispatch_cursor = menu_available,
        .dispatch_process_mouse = menu_available && current_ui_available,
    };
}

std::array<CoJGameplayActionValue, 16> BuildCoJGameplayActionValues(
    const cojvr::runtime::GameplayInputState& state) noexcept {
    const auto positive = [](const float value) noexcept {
        return std::max(value, 0.0F);
    };
    const auto constrain_axis = [](const float value) noexcept {
        constexpr float kDeadZone = 0.04F;
        constexpr float kSaturation = 1.0F;
        const float clamped = std::clamp(value, -kSaturation, kSaturation);
        if (std::fabs(clamped) < kDeadZone) return 0.0F;
        const float denominator = kSaturation - kDeadZone;
        return clamped > 0.0F
            ? (clamped - kDeadZone) / denominator
            : (clamped + kDeadZone) / denominator;
    };
    float move_x = 0.0F;
    float move_y = 0.0F;
    if (state.active && std::isfinite(state.move.x) && std::isfinite(state.move.y)) {
        // Shipped InputAnalog.ApplyConstraints uses one independent 0.04
        // deadzone per axis and a 1.0 saturation, then linearly remaps the
        // surviving range. Reproduce that exact shaping before dispatching the
        // directional action magnitude through CallOnInputGameController.
        move_x = constrain_axis(state.move.x);
        move_y = constrain_axis(state.move.y);
    }
    return {{
        // Horizontal turn is consumed by the exact 45-degree snap path below.
        // Keep both native analog actions neutral so mouse sensitivity cannot
        // turn a snap request back into continuous rotation.
        {2, 0.0F},
        {3, 0.0F},
        {4, positive(move_y)},
        {5, positive(-move_y)},
        {6, positive(move_x)},
        {7, positive(-move_x)},
        {9, state.active && state.fire_left ? 1.0F : 0.0F},
        {10, state.active && state.fire_right ? 1.0F : 0.0F},
        {11, state.active && state.jump ? 1.0F : 0.0F},
        {16, state.active && state.crouch ? 1.0F : 0.0F},
        {18, state.active && state.run ? 1.0F : 0.0F},
        {30, state.active && state.interact ? 1.0F : 0.0F},
        {31, state.active && state.reload ? 1.0F : 0.0F},
        {39, state.active && state.kick ? 1.0F : 0.0F},
        {46, state.active && state.weapon_next ? 1.0F : 0.0F},
        {47, state.active && state.weapon_previous ? 1.0F : 0.0F},
    }};
}

bool UseDirectAnalogCoJLocomotion(const int action) noexcept {
    return action >= 4 && action <= 7;
}

CoJGameplayTargetSelection BuildCoJGameplayTargetSelection(
    const int action,
    const int target_type,
    const int target_count) noexcept {
    return {
        .action = action,
        .index = target_type,
        .valid = action >= 0 && target_type >= 0 && target_type < target_count,
    };
}

bool ShouldYieldCoJArmIkForNativeReload(
    const bool observation_available,
    const bool reloading) noexcept {
    return observation_available && reloading;
}

float CoJSnapTurnState::Update(
    const cojvr::runtime::GameplayInputState& state) noexcept {
    constexpr float kEngageThreshold = 0.70F;
    constexpr float kReleaseThreshold = 0.35F;
    constexpr float kSnapDegrees = 45.0F;

    if (!state.active || !std::isfinite(state.turn.x)) {
        latched = false;
        return 0.0F;
    }
    const float turn_x = std::clamp(state.turn.x, -1.0F, 1.0F);
    if (std::fabs(turn_x) <= kReleaseThreshold) {
        latched = false;
        return 0.0F;
    }
    if (latched || std::fabs(turn_x) < kEngageThreshold) return 0.0F;
    latched = true;
    return turn_x > 0.0F ? kSnapDegrees : -kSnapDegrees;
}

CoJFireOriginMutationPolicy BuildCoJFireOriginMutationPolicy(
    const int action,
    const bool current_pressed,
    const bool digital_transition,
    const bool origin_valid) noexcept {
    constexpr int kRightHand = 0;
    constexpr int kLeftHand = 1;
    const int hand = action == 9 ? kLeftHand : (action == 10 ? kRightHand : -1);
    const bool scoped_override = hand >= 0 && current_pressed && digital_transition && origin_valid;
    return {
        .hand = hand,
        .override_for_translate = scoped_override,
        .restore_after_translate = false,
    };
}

CoJLoadingUiInputResult CoJLoadingUiInputGate::Update(
    const bool timer_state_valid,
    const bool timer_frozen,
    const bool ui_select_pressed,
    const bool fire_left,
    const bool fire_right) noexcept {
    CoJLoadingUiInputResult result{};
    const bool blocking_ui = timer_state_valid && timer_frozen;
    result.dispatch_select = blocking_ui && ui_select_pressed;
    result.suppress_fire = blocking_ui;
    result.suppress_gameplay = blocking_ui;
    if (result.dispatch_select) fire_release_required_ = true;
    if (fire_release_required_) {
        if (!fire_left && !fire_right) {
            fire_release_required_ = false;
        } else {
            result.suppress_fire = true;
        }
    }
    return result;
}

void CoJLoadingUiInputGate::Reset() noexcept { fire_release_required_ = false; }

void CoJUiSelectRetryState::Observe(
    const bool pressed_edge,
    const bool select_held) noexcept {
    held_ = select_held;
    if (pressed_edge) pending_ = true;
    if (!held_) pending_ = false;
}

bool CoJUiSelectRetryState::ShouldDispatch(
    const bool pointer_active,
    const bool timer_frozen) const noexcept {
    return pending_ && held_ && (pointer_active || timer_frozen);
}

void CoJUiSelectRetryState::Complete(const bool dispatched) noexcept {
    if (dispatched) pending_ = false;
}

bool JavaPlayerBridge::EnsureVm(std::string* error) noexcept {
    if (vm_) return true;

    const HMODULE jvm = GetModuleHandleW(L"jvm.dll");
    if (!jvm) {
        SetError(error, "jvm.dll is not loaded");
        return false;
    }
    const auto get_created = reinterpret_cast<GetCreatedJavaVmsFn>(
        GetProcAddress(jvm, "JNI_GetCreatedJavaVMs"));
    if (!get_created) {
        SetError(error, "JNI_GetCreatedJavaVMs is unavailable");
        return false;
    }

    void* vm = nullptr;
    std::int32_t count = 0;
    if (get_created(&vm, 1, &count) != kJniOk || count != 1 || !vm) {
        SetError(error, "the game JVM is not available");
        return false;
    }
    vm_ = vm;
    return true;
}

void JavaPlayerBridge::Reset() noexcept {
    std::string ignored;
    void* env = vm_ ? Environment(&ignored) : nullptr;
    if (env) {
        ClearBeing(env);
        if (vector_class_) {
            if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
                delete_global(env, vector_class_);
            }
        }
        if (session_class_) {
            if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
                delete_global(env, session_class_);
            }
        }
        if (lawman_module_single_class_) {
            if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
                delete_global(env, lawman_module_single_class_);
            }
        }
        if (lawman_game_class_) {
            if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
                delete_global(env, lawman_game_class_);
            }
        }
        if (input_digital_class_) {
            if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
                delete_global(env, input_digital_class_);
            }
        }
        if (input_analog_class_) {
            if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
                delete_global(env, input_analog_class_);
            }
        }
        if (input_settings_class_) {
            if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
                delete_global(env, input_settings_class_);
            }
        }
        if (game_object_class_) {
            if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
                delete_global(env, game_object_class_);
            }
        }
    } else {
        being_ = nullptr;
    }
    session_class_ = nullptr;
    session_local_player_field_ = nullptr;
    session_players_field_ = nullptr;
    vector_size_method_ = nullptr;
    vector_get_method_ = nullptr;
    net_player_being_field_ = nullptr;
    lawman_game_class_ = nullptr;
    lawman_module_single_class_ = nullptr;
    active_game_module_field_ = nullptr;
    is_timer_freezed_method_ = nullptr;
    get_mesh_element_method_ = nullptr;
    get_element_id_method_ = nullptr;
    hide_element_method_ = nullptr;
    unhide_element_method_ = nullptr;
    is_element_hidden_method_ = nullptr;
    get_bone_joint_method_ = nullptr;
    get_bone_direction_method_ = nullptr;
    get_bone_perpendicular_method_ = nullptr;
    get_element_position_method_ = nullptr;
    get_element_left_method_ = nullptr;
    get_element_up_method_ = nullptr;
    set_element_world_basis_method_ = nullptr;
    rotate_element_with_children_method_ = nullptr;
    bone_rotate_method_ = nullptr;
    get_position_vector_method_ = nullptr;
    set_position_method_ = nullptr;
    update_body_rotation_method_ = nullptr;
    current_head_vertical_field_ = nullptr;
    current_head_horizontal_field_ = nullptr;
    current_spine_vertical_field_ = nullptr;
    current_spine_horizontal_field_ = nullptr;
    body_rotation_lookup_attempted_ = false;
    vector_x_field_ = nullptr;
    vector_y_field_ = nullptr;
    vector_z_field_ = nullptr;
    vector_class_ = nullptr;
    vector_constructor_ = nullptr;
    input_controller_field_ = nullptr;
    controller_actions_field_ = nullptr;
    controller_targets_field_ = nullptr;
    controller_lock_apply_method_ = nullptr;
    controller_unlock_apply_method_ = nullptr;
    controller_apply_method_ = nullptr;
    input_settings_class_ = nullptr;
    input_target_type_method_ = nullptr;
    input_digital_class_ = nullptr;
    input_analog_class_ = nullptr;
    digital_device_field_ = nullptr;
    digital_button_field_ = nullptr;
    digital_translate_method_ = nullptr;
    analog_device_field_ = nullptr;
    analog_axis_field_ = nullptr;
    analog_axis_sign_field_ = nullptr;
    analog_translate_method_ = nullptr;
    input_target_item_target_field_ = nullptr;
    input_target_item_next_field_ = nullptr;
    input_target_can_execute_method_ = nullptr;
    game_object_class_ = nullptr;
    game_object_controller_input_method_ = nullptr;
    look_dir_for_hand_field_ = nullptr;
    aim_from_point_field_ = nullptr;
    look_from_point_field_ = nullptr;
    is_weapon_reloading_method_ = nullptr;
    bone_read_lookup_attempted_ = false;
    element_world_read_lookup_attempted_ = false;
    element_world_basis_lookup_attempted_ = false;
    element_rotation_lookup_attempted_ = false;
    bone_rotation_lookup_attempted_ = false;
    element_visibility_lookup_attempted_ = false;
    aim_lookup_attempted_ = false;
    fire_origin_lookup_attempted_ = false;
    weapon_reload_lookup_attempted_ = false;
    single_player_fallback_used_ = false;
    campaign_module_fallback_used_ = false;
    vm_ = nullptr;
    being_generation_ = 0;
    last_gameplay_input_ = {};
    gameplay_input_applied_ = false;
    snap_turn_state_ = {};
    last_snap_turn_degrees_ = 0.0F;
    last_analog_transaction_applied_ = false;
    fire_origins_ = {};
    fire_origin_valid_ = {};
    // Detach current thread if it was attached by this bridge
    DetachCurrentThread();
}

void* JavaPlayerBridge::Environment(std::string* error) noexcept {
    if (!EnsureVm(error)) return nullptr;

    const auto get_env = VmFunction<GetEnvFn>(vm_, kVmGetEnv);
    if (!get_env) {
        SetError(error, "JavaVM GetEnv is unavailable");
        return nullptr;
    }

    void* env = nullptr;
    const std::int32_t result = get_env(vm_, &env, kJniVersion14);
    if (result == kJniOk && env) return env;
    if (result != kJniDetached) {
        SetError(error, "JavaVM GetEnv failed");
        return nullptr;
    }

    const auto attach = VmFunction<AttachCurrentThreadFn>(vm_, kVmAttachCurrentThreadAsDaemon);
    if (!attach || attach(vm_, &env, nullptr) != kJniOk || !env) {
        SetError(error, "AttachCurrentThreadAsDaemon failed");
        return nullptr;
    }
    g_jni_thread_attached = true;
    return env;
}

void JavaPlayerBridge::DetachCurrentThread() noexcept {
    if (!vm_ || !g_jni_thread_attached) return;
    const auto detach = VmFunction<DetachCurrentThreadFn>(vm_, kVmDetachCurrentThread);
    if (detach) {
        detach(vm_);
        g_jni_thread_attached = false;
    }
}

bool JavaPlayerBridge::ClearException(
    void* env, std::string* error, const char* stage) noexcept {
    const auto occurred = EnvFunction<ExceptionOccurredFn>(env, kExceptionOccurred);
    if (!occurred) return false;
    void* exception = occurred(env);
    if (!exception) return false;
    if (const auto clear = EnvFunction<ExceptionClearFn>(env, kExceptionClear)) clear(env);
    DeleteLocal(env, exception);
    SetError(error, stage);
    return true;
}

bool JavaPlayerBridge::EnsureSession(void* env, std::string* error) noexcept {
    if (session_class_ && session_local_player_field_ && session_players_field_ &&
        vector_size_method_ && vector_get_method_) {
        return true;
    }

    const auto find_class = EnvFunction<FindClassFn>(env, kFindClass);
    const auto new_global = EnvFunction<NewGlobalRefFn>(env, kNewGlobalRef);
    const auto get_static_field = EnvFunction<GetStaticFieldIdFn>(env, kGetStaticFieldId);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    if (!find_class || !new_global || !get_static_field || !get_method) {
        SetError(error, "required JNI class lookup functions are unavailable");
        return false;
    }

    void* local_class = find_class(env, "Session");
    if (!local_class || ClearException(env, error, "FindClass(Session) failed")) {
        DeleteLocal(env, local_class);
        return false;
    }
    void* global_class = new_global(env, local_class);
    DeleteLocal(env, local_class);
    if (!global_class || ClearException(env, error, "NewGlobalRef(Session) failed")) return false;

    void* local_player_field =
        get_static_field(env, global_class, "sm_LocalPlayer", "LNetPlayer;");
    void* players_field =
        get_static_field(env, global_class, "sm_Players", "Ljava/util/Vector;");
    if (!local_player_field || !players_field ||
        ClearException(env, error, "Session player-field lookup failed")) {
        if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
            delete_global(env, global_class);
        }
        return false;
    }

    void* vector_class = find_class(env, "java/util/Vector");
    if (!vector_class || ClearException(env, error, "FindClass(java/util/Vector) failed")) {
        DeleteLocal(env, vector_class);
        if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
            delete_global(env, global_class);
        }
        return false;
    }
    void* vector_size = get_method(env, vector_class, "size", "()I");
    void* vector_get = get_method(env, vector_class, "get", "(I)Ljava/lang/Object;");
    const bool vector_lookup_failed =
        ClearException(env, error, "java/util/Vector method lookup failed");
    DeleteLocal(env, vector_class);
    if (vector_lookup_failed || !vector_size || !vector_get) {
        if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
            delete_global(env, global_class);
        }
        return false;
    }

    session_class_ = global_class;
    session_local_player_field_ = local_player_field;
    session_players_field_ = players_field;
    vector_size_method_ = vector_size;
    vector_get_method_ = vector_get;
    return true;
}

bool JavaPlayerBridge::EnsureCampaignAccess(void* env, std::string* error) noexcept {
    if (lawman_game_class_ && lawman_module_single_class_ && active_game_module_field_) {
        return true;
    }

    const auto find_class = EnvFunction<FindClassFn>(env, kFindClass);
    const auto new_global = EnvFunction<NewGlobalRefFn>(env, kNewGlobalRef);
    const auto get_static_field = EnvFunction<GetStaticFieldIdFn>(env, kGetStaticFieldId);
    if (!find_class || !new_global || !get_static_field) {
        SetError(error, "required JNI campaign lookup functions are unavailable");
        return false;
    }

    void* local_game_class = find_class(env, "LawmanGame");
    if (!local_game_class || ClearException(env, error, "FindClass(LawmanGame) failed")) {
        DeleteLocal(env, local_game_class);
        return false;
    }
    void* global_game_class = new_global(env, local_game_class);
    DeleteLocal(env, local_game_class);
    if (!global_game_class || ClearException(env, error, "NewGlobalRef(LawmanGame) failed")) {
        return false;
    }

    void* active_module_field = get_static_field(
        env, global_game_class, "sm_cActiveGameModule", "LLawmanModule;");
    if (!active_module_field ||
        ClearException(env, error, "LawmanGame.sm_cActiveGameModule lookup failed")) {
        if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
            delete_global(env, global_game_class);
        }
        return false;
    }

    void* local_single_class = find_class(env, "LawmanModuleSingle");
    if (!local_single_class ||
        ClearException(env, error, "FindClass(LawmanModuleSingle) failed")) {
        DeleteLocal(env, local_single_class);
        if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
            delete_global(env, global_game_class);
        }
        return false;
    }
    void* global_single_class = new_global(env, local_single_class);
    DeleteLocal(env, local_single_class);
    if (!global_single_class ||
        ClearException(env, error, "NewGlobalRef(LawmanModuleSingle) failed")) {
        if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
            delete_global(env, global_game_class);
        }
        return false;
    }

    lawman_game_class_ = global_game_class;
    lawman_module_single_class_ = global_single_class;
    active_game_module_field_ = active_module_field;
    return true;
}

bool JavaPlayerBridge::EnsureGameplayInputAccess(void* env, std::string* error) noexcept {
    if (input_controller_field_ && controller_actions_field_ && controller_targets_field_ &&
        controller_lock_apply_method_ && controller_unlock_apply_method_ &&
        controller_apply_method_ &&
        input_settings_class_ && input_target_type_method_ &&
        input_digital_class_ && input_analog_class_ && digital_device_field_ &&
        digital_button_field_ && digital_translate_method_ && analog_device_field_ &&
        analog_axis_field_ && analog_axis_sign_field_ && analog_translate_method_ &&
        input_target_item_target_field_ && input_target_item_next_field_ &&
        input_target_can_execute_method_ && game_object_class_ &&
        game_object_controller_input_method_) {
        return true;
    }
    if (!EnsureCampaignAccess(env, error)) return false;

    const auto find_class = EnvFunction<FindClassFn>(env, kFindClass);
    const auto new_global = EnvFunction<NewGlobalRefFn>(env, kNewGlobalRef);
    const auto get_static_method = EnvFunction<GetStaticMethodIdFn>(env, kGetStaticMethodId);
    const auto get_static_field = EnvFunction<GetStaticFieldIdFn>(env, kGetStaticFieldId);
    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_field = EnvFunction<GetFieldIdFn>(env, kGetFieldId);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    if (!find_class || !new_global || !get_static_method || !get_static_field || !get_static_object ||
        !get_class || !get_field || !get_method) {
        SetError(error, "required JNI gameplay-input lookup functions are unavailable");
        return false;
    }

    input_controller_field_ = get_static_field(
        env, lawman_game_class_, "sm_cInputController", "LGameInputController;");
    if (!input_controller_field_ ||
        ClearException(env, error, "LawmanGame.sm_cInputController lookup failed")) {
        input_controller_field_ = nullptr;
        return false;
    }
    void* controller = get_static_object(env, lawman_game_class_, input_controller_field_);
    if (!controller ||
        ClearException(env, error, "LawmanGame.sm_cInputController read failed")) {
        DeleteLocal(env, controller);
        SetError(error, "game input controller is unavailable");
        return false;
    }
    void* controller_class = get_class(env, controller);
    if (!controller_class || ClearException(env, error, "GameInputController class read failed")) {
        DeleteLocal(env, controller_class);
        DeleteLocal(env, controller);
        return false;
    }
    controller_actions_field_ = get_field(
        env, controller_class, "m_Actions", "[LInputAction;");
    controller_targets_field_ = get_field(
        env, controller_class, "m_Targets", "[LEInputTargetItem;");
    controller_lock_apply_method_ = get_method(
        env, controller_class, "LockApplyControllerState", "([LEInputTargetItem;)V");
    controller_unlock_apply_method_ = get_method(
        env, controller_class, "UnlockApplyControllerState", "([LEInputTargetItem;)V");
    controller_apply_method_ = get_method(
        env, controller_class, "ApplyControllerState", "([LEInputTargetItem;)V");
    DeleteLocal(env, controller_class);
    DeleteLocal(env, controller);
    if (!controller_actions_field_ || !controller_targets_field_ ||
        !controller_lock_apply_method_ || !controller_unlock_apply_method_ ||
        !controller_apply_method_ ||
        ClearException(env, error, "GameInputController action/target lookup failed")) {
        controller_actions_field_ = nullptr;
        controller_targets_field_ = nullptr;
        controller_lock_apply_method_ = nullptr;
        controller_unlock_apply_method_ = nullptr;
        controller_apply_method_ = nullptr;
        return false;
    }

    void* settings_local = find_class(env, "InputSettings");
    void* digital_local = find_class(env, "InputDigital");
    void* analog_local = find_class(env, "InputAnalog");
    if (!settings_local || !digital_local || !analog_local ||
        ClearException(env, error, "InputSettings/InputDigital/InputAnalog class lookup failed")) {
        DeleteLocal(env, settings_local);
        DeleteLocal(env, digital_local);
        DeleteLocal(env, analog_local);
        return false;
    }
    input_settings_class_ = new_global(env, settings_local);
    input_digital_class_ = new_global(env, digital_local);
    input_analog_class_ = new_global(env, analog_local);
    DeleteLocal(env, settings_local);
    DeleteLocal(env, digital_local);
    DeleteLocal(env, analog_local);
    if (!input_settings_class_ || !input_digital_class_ || !input_analog_class_ ||
        ClearException(env, error, "gameplay input class global-ref creation failed")) {
        return false;
    }

    input_target_type_method_ = get_static_method(
        env, input_settings_class_, "GetTargetTypeForAction", "(I)I");
    if (!input_target_type_method_ ||
        ClearException(env, error, "InputSettings.GetTargetTypeForAction lookup failed")) {
        input_target_type_method_ = nullptr;
        return false;
    }

    digital_device_field_ = get_field(env, input_digital_class_, "m_iDevice", "I");
    digital_button_field_ = get_field(env, input_digital_class_, "m_iButtonID", "I");
    digital_translate_method_ = get_method(
        env, input_digital_class_, "Translate", "(LEInputTargetItem;IIZ)Z");
    analog_device_field_ = get_field(env, input_analog_class_, "m_iDevice", "I");
    analog_axis_field_ = get_field(env, input_analog_class_, "m_iAxis", "I");
    analog_axis_sign_field_ = get_field(env, input_analog_class_, "m_iAxisSign", "I");
    analog_translate_method_ = get_method(
        env, input_analog_class_, "Translate", "(LEInputTargetItem;IIF)Z");
    if (!digital_device_field_ || !digital_button_field_ || !digital_translate_method_ ||
        !analog_device_field_ || !analog_axis_field_ || !analog_axis_sign_field_ ||
        !analog_translate_method_ ||
        ClearException(env, error, "InputAction translation member lookup failed")) {
        return false;
    }

    void* target_item_local = find_class(env, "EInputTargetItem");
    void* input_target_local = find_class(env, "EInputTarget");
    void* game_object_local = find_class(env, "GameObject");
    if (!target_item_local || !input_target_local || !game_object_local ||
        ClearException(env, error, "gameplay target class lookup failed")) {
        DeleteLocal(env, target_item_local);
        DeleteLocal(env, input_target_local);
        DeleteLocal(env, game_object_local);
        return false;
    }
    input_target_item_target_field_ = get_field(
        env, target_item_local, "m_cTarget", "LEInputTarget;");
    input_target_item_next_field_ = get_field(
        env, target_item_local, "m_cNext", "LEInputTargetItem;");
    input_target_can_execute_method_ = get_method(
        env, input_target_local, "CanExecuteInput", "()Z");
    game_object_controller_input_method_ = get_method(
        env, game_object_local, "CallOnInputGameController", "(IFLjava/lang/Object;)V");
    game_object_class_ = new_global(env, game_object_local);
    DeleteLocal(env, target_item_local);
    DeleteLocal(env, input_target_local);
    DeleteLocal(env, game_object_local);
    if (!input_target_item_target_field_ || !input_target_item_next_field_ ||
        !input_target_can_execute_method_ || !game_object_controller_input_method_ ||
        !game_object_class_ ||
        ClearException(env, error, "gameplay target member lookup failed")) {
        return false;
    }
    return true;
}

bool JavaPlayerBridge::TryResolveCampaignBeing(void* env, std::string* error) noexcept {
    if (!EnsureCampaignAccess(env, error)) return false;

    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    const auto is_instance = EnvFunction<IsInstanceOfFn>(env, kIsInstanceOf);
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto call_object = EnvFunction<CallObjectMethodAFn>(env, kCallObjectMethodA);
    const auto is_same = EnvFunction<IsSameObjectFn>(env, kIsSameObject);
    if (!get_static_object || !is_instance || !get_class || !get_method || !call_object ||
        !is_same) {
        SetError(error, "required JNI campaign player discovery functions are unavailable");
        return false;
    }

    void* module = get_static_object(env, lawman_game_class_, active_game_module_field_);
    if (ClearException(env, error, "LawmanGame.sm_cActiveGameModule read failed") || !module) {
        DeleteLocal(env, module);
        SetError(error, "single-player active game module is unavailable");
        return false;
    }
    if (is_instance(env, module, lawman_module_single_class_) == 0 ||
        ClearException(env, error, "LawmanModuleSingle type check failed")) {
        DeleteLocal(env, module);
        SetError(error, "active game module is not LawmanModuleSingle");
        return false;
    }

    void* module_class = get_class(env, module);
    if (!module_class || ClearException(env, error, "GetObjectClass(active game module) failed")) {
        DeleteLocal(env, module_class);
        DeleteLocal(env, module);
        return false;
    }
    void* get_main_player = get_method(env, module_class, "GetMainPlayer", "()LBeing;");
    const bool method_failed =
        ClearException(env, error, "LawmanModuleSingle.GetMainPlayer lookup failed");
    DeleteLocal(env, module_class);
    if (method_failed || !get_main_player) {
        DeleteLocal(env, module);
        return false;
    }

    void* local_being = call_object(env, module, get_main_player, nullptr);
    DeleteLocal(env, module);
    if (ClearException(env, error, "LawmanModuleSingle.GetMainPlayer call failed") ||
        !local_being) {
        DeleteLocal(env, local_being);
        SetError(error, "single-player main player is unavailable");
        return false;
    }

    const bool same = being_ && is_same(env, being_, local_being) != 0;
    if (!same && !ResolveBeingMethods(env, local_being, error)) {
        DeleteLocal(env, local_being);
        return false;
    }
    DeleteLocal(env, local_being);
    campaign_module_fallback_used_ = true;
    single_player_fallback_used_ = false;
    return being_ != nullptr;
}

bool JavaPlayerBridge::TryGetActiveGameTimerFrozen(
    bool& frozen, std::string* error) noexcept {
    frozen = false;
    void* env = Environment(error);
    if (!env || !EnsureCampaignAccess(env, error)) return false;

    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    const auto is_instance = EnvFunction<IsInstanceOfFn>(env, kIsInstanceOf);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto call_boolean = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    if (!get_static_object || !is_instance || !get_method || !call_boolean) {
        SetError(error, "required JNI game-timer observation functions are unavailable");
        return false;
    }

    if (!is_timer_freezed_method_) {
        is_timer_freezed_method_ =
            get_method(env, lawman_module_single_class_, "IsTimerFreezed", "()Z");
        if (!is_timer_freezed_method_ ||
            ClearException(env, error, "Module.IsTimerFreezed lookup failed")) {
            is_timer_freezed_method_ = nullptr;
            return false;
        }
    }

    void* module = get_static_object(env, lawman_game_class_, active_game_module_field_);
    if (ClearException(env, error, "LawmanGame.sm_cActiveGameModule timer read failed") ||
        !module) {
        DeleteLocal(env, module);
        SetError(error, "active game module is unavailable for timer observation");
        return false;
    }
    if (is_instance(env, module, lawman_module_single_class_) == 0 ||
        ClearException(env, error, "LawmanModuleSingle timer type check failed")) {
        DeleteLocal(env, module);
        SetError(error, "active game module is not LawmanModuleSingle for timer observation");
        return false;
    }

    const std::uint8_t value = call_boolean(env, module, is_timer_freezed_method_, nullptr);
    DeleteLocal(env, module);
    if (ClearException(env, error, "Module.IsTimerFreezed call failed")) return false;
    frozen = value != 0;
    return true;
}

bool JavaPlayerBridge::TryObserveSubtitleState(
    CoJSubtitleRuntimeState& state,
    std::string* error) noexcept {
    state = {};
    void* env = Environment(error);
    if (!env) return false;

    const auto find_class = EnvFunction<FindClassFn>(env, kFindClass);
    const auto get_static_field = EnvFunction<GetStaticFieldIdFn>(env, kGetStaticFieldId);
    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    const auto get_object_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_field = EnvFunction<GetFieldIdFn>(env, kGetFieldId);
    const auto get_boolean = EnvFunction<GetBooleanFieldFn>(env, kGetBooleanField);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto call_boolean = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    if (!find_class || !get_static_field || !get_static_object || !get_object_class ||
        !get_field || !get_boolean || !get_method || !call_boolean) {
        SetError(error, "required JNI subtitle observation functions are unavailable");
        return false;
    }

    void* lawman_game = find_class(env, "LawmanGame");
    if (!lawman_game || ClearException(env, error, "FindClass(LawmanGame) for subtitles failed")) {
        DeleteLocal(env, lawman_game);
        return false;
    }
    void* settings_field = get_static_field(env, lawman_game, "sm_cSettings", "LSettings;");
    if (!settings_field || ClearException(env, error, "LawmanGame.sm_cSettings lookup failed")) {
        DeleteLocal(env, lawman_game);
        return false;
    }
    void* settings = get_static_object(env, lawman_game, settings_field);
    DeleteLocal(env, lawman_game);
    if (!settings || ClearException(env, error, "LawmanGame.sm_cSettings read failed")) {
        DeleteLocal(env, settings);
        return false;
    }
    void* settings_class = get_object_class(env, settings);
    void* subtitles_field = settings_class
        ? get_field(env, settings_class, "bSubtitles", "Z")
        : nullptr;
    if (!settings_class || !subtitles_field ||
        ClearException(env, error, "Settings.bSubtitles lookup failed")) {
        DeleteLocal(env, settings_class);
        DeleteLocal(env, settings);
        return false;
    }
    state.subtitles_enabled = get_boolean(env, settings, subtitles_field) != 0;
    const bool settings_read_failed =
        ClearException(env, error, "Settings.bSubtitles read failed");
    DeleteLocal(env, settings_class);
    DeleteLocal(env, settings);
    if (settings_read_failed) return false;

    void* dialog_class = find_class(env, "Dialog");
    if (!dialog_class || ClearException(env, error, "FindClass(Dialog) failed")) {
        DeleteLocal(env, dialog_class);
        return false;
    }
    void* playing_dialog_field =
        get_static_field(env, dialog_class, "cPlayingDialog", "LDialog;");
    if (!playing_dialog_field ||
        ClearException(env, error, "Dialog.cPlayingDialog lookup failed")) {
        DeleteLocal(env, dialog_class);
        return false;
    }
    void* dialog = get_static_object(env, dialog_class, playing_dialog_field);
    if (ClearException(env, error, "Dialog.cPlayingDialog read failed")) {
        DeleteLocal(env, dialog);
        DeleteLocal(env, dialog_class);
        return false;
    }
    state.dialog_playing = dialog != nullptr;
    if (!dialog) {
        DeleteLocal(env, dialog_class);
        return true;
    }

    void* current_line_field = get_field(env, dialog_class, "m_bCurrentLineVisible", "Z");
    void* subtitles_visible_method = get_method(env, dialog_class, "AreSubtitlesVisible", "()Z");
    if (!current_line_field || !subtitles_visible_method ||
        ClearException(env, error, "Dialog subtitle-state lookup failed")) {
        DeleteLocal(env, dialog);
        DeleteLocal(env, dialog_class);
        return false;
    }
    state.current_line_visible = get_boolean(env, dialog, current_line_field) != 0;
    state.dialog_subtitle_visible =
        call_boolean(env, dialog, subtitles_visible_method, nullptr) != 0;
    const bool dialog_read_failed =
        ClearException(env, error, "Dialog subtitle-state read failed");
    DeleteLocal(env, dialog);
    DeleteLocal(env, dialog_class);
    return !dialog_read_failed;
}

bool JavaPlayerBridge::TryGetWeaponReloading(
    bool& reloading,
    std::string* error) noexcept {
    reloading = false;
    if (!being_ && !Refresh(error)) return false;

    void* env = Environment(error);
    if (!env || !being_) return false;
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto call_boolean = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    if (!get_class || !get_method || !call_boolean) {
        SetError(error, "required JNI weapon-reload observation functions are unavailable");
        return false;
    }

    if (!weapon_reload_lookup_attempted_) {
        weapon_reload_lookup_attempted_ = true;
        void* player_class = get_class(env, being_);
        if (!player_class ||
            ClearException(env, error, "GetObjectClass(player weapon reload) failed")) {
            DeleteLocal(env, player_class);
            return false;
        }
        is_weapon_reloading_method_ =
            get_method(env, player_class, "IsWeaponReloading", "()Z");
        DeleteLocal(env, player_class);
        if (!is_weapon_reloading_method_ ||
            ClearException(env, error, "ArmedPlayerBeing.IsWeaponReloading lookup failed")) {
            is_weapon_reloading_method_ = nullptr;
            return false;
        }
    }
    if (!is_weapon_reloading_method_) {
        SetError(error, "player weapon-reload observation route is unavailable");
        return false;
    }

    const std::uint8_t value =
        call_boolean(env, being_, is_weapon_reloading_method_, nullptr);
    if (ClearException(env, error, "ArmedPlayerBeing.IsWeaponReloading call failed")) {
        return false;
    }
    reloading = value != 0;
    return true;
}

bool JavaPlayerBridge::TryApplyGameplayInput(
    const cojvr::runtime::GameplayInputState& state,
    std::string* error) noexcept {
    last_snap_turn_degrees_ = 0.0F;
    last_analog_transaction_applied_ = false;
    last_fire_origin_transitions_ = {};
    const float snap_turn_degrees = snap_turn_state_.Update(state);
    if (!state.active && !gameplay_input_applied_) return true;

    if (snap_turn_degrees != 0.0F) {
        if (!TryRotateHorizontally(snap_turn_degrees, error)) return false;
        last_snap_turn_degrees_ = snap_turn_degrees;
    }

    void* env = Environment(error);
    if (!env || !EnsureGameplayInputAccess(env, error)) return false;

    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    const auto get_object_field = EnvFunction<GetObjectFieldFn>(env, kGetObjectField);
    const auto get_length = EnvFunction<GetArrayLengthFn>(env, kGetArrayLength);
    const auto get_element = EnvFunction<GetObjectArrayElementFn>(env, kGetObjectArrayElement);
    const auto get_int = EnvFunction<GetIntFieldFn>(env, kGetIntField);
    const auto is_instance = EnvFunction<IsInstanceOfFn>(env, kIsInstanceOf);
    const auto call_static_int = EnvFunction<CallStaticIntMethodAFn>(env, kCallStaticIntMethodA);
    const auto call_boolean = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    const auto call_void = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    if (!get_static_object || !get_object_field || !get_length || !get_element || !get_int ||
        !is_instance || !call_static_int || !call_boolean || !call_void) {
        SetError(error, "required JNI gameplay-input functions are unavailable");
        return false;
    }

    void* controller = get_static_object(env, lawman_game_class_, input_controller_field_);
    if (!controller ||
        ClearException(env, error, "LawmanGame.sm_cInputController gameplay read failed")) {
        DeleteLocal(env, controller);
        SetError(error, "game input controller is unavailable");
        return false;
    }
    void* actions = get_object_field(env, controller, controller_actions_field_);
    void* targets = get_object_field(env, controller, controller_targets_field_);
    if (!actions || !targets ||
        ClearException(env, error, "GameInputController action/target array read failed")) {
        DeleteLocal(env, actions);
        DeleteLocal(env, targets);
        DeleteLocal(env, controller);
        return false;
    }
    const std::int32_t action_count = get_length(env, actions);
    const std::int32_t target_count = get_length(env, targets);
    if (action_count <= 47 || target_count <= 0 ||
        ClearException(env, error, "GameInputController array length read failed")) {
        DeleteLocal(env, actions);
        DeleteLocal(env, targets);
        DeleteLocal(env, controller);
        SetError(error, "game input action/target arrays are incomplete");
        return false;
    }

    const auto values = BuildCoJGameplayActionValues(state);
    const auto previous_values = gameplay_input_applied_
        ? BuildCoJGameplayActionValues(last_gameplay_input_)
        : BuildCoJGameplayActionValues({});
    bool analog_transaction_needed = false;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (UseDirectAnalogCoJLocomotion(values[index].action) &&
            (!gameplay_input_applied_ ||
             std::fabs(values[index].value - previous_values[index].value) > 0.005F)) {
            analog_transaction_needed = true;
            break;
        }
    }

    JValue controller_state_args[1]{};
    controller_state_args[0].l = targets;
    bool analog_transaction_locked = false;
    if (analog_transaction_needed) {
        call_void(env, controller, controller_lock_apply_method_, controller_state_args);
        if (ClearException(
                env, error,
                "GameInputController.LockApplyControllerState VR locomotion failed")) {
            DeleteLocal(env, actions);
            DeleteLocal(env, targets);
            DeleteLocal(env, controller);
            return false;
        }
        analog_transaction_locked = true;
    }

    const auto apply_value = [&](const CoJGameplayActionValue& item,
                                 const CoJGameplayActionValue& previous_item) noexcept {
        void* action = get_element(env, actions, item.action);
        if (!action || ClearException(env, error, "InputAction array access failed")) {
            DeleteLocal(env, action);
            return false;
        }
        const bool digital = is_instance(env, action, input_digital_class_) != 0;
        const bool analog = !digital && is_instance(env, action, input_analog_class_) != 0;
        if (ClearException(env, error, "InputAction type check failed")) {
            DeleteLocal(env, action);
            return false;
        }

        constexpr float kDigitalThreshold = 0.35F;
        const bool current_pressed = item.value > kDigitalThreshold;
        const bool previous_pressed = previous_item.value > kDigitalThreshold;
        const bool needs_digital_update = !gameplay_input_applied_ ||
            current_pressed != previous_pressed;
        const bool needs_analog_update = !gameplay_input_applied_ ||
            std::fabs(item.value - previous_item.value) > 0.005F;
        const int fire_hand = item.action == 9 ? 1 : (item.action == 10 ? 0 : -1);
        const auto fire_origin_policy = BuildCoJFireOriginMutationPolicy(
            item.action,
            current_pressed,
            needs_digital_update,
            fire_hand >= 0 && fire_origin_valid_[static_cast<std::size_t>(fire_hand)]);
        const auto resolve_target_item = [&](void*& target_item) noexcept {
            target_item = nullptr;
            JValue target_type_args[1]{};
            target_type_args[0].i = item.action;
            const std::int32_t target_type = call_static_int(
                env, input_settings_class_, input_target_type_method_, target_type_args);
            if (ClearException(env, error, "InputSettings.GetTargetTypeForAction call failed")) {
                return false;
            }
            const auto selection = BuildCoJGameplayTargetSelection(
                item.action, target_type, target_count);
            if (!selection.valid) {
                SetError(error, "InputSettings returned an invalid gameplay target category");
                return false;
            }
            target_item = get_element(env, targets, selection.index);
            if (ClearException(env, error, "selected gameplay target access failed")) {
                DeleteLocal(env, target_item);
                target_item = nullptr;
                return false;
            }
            return true;
        };
        if (UseDirectAnalogCoJLocomotion(item.action)) {
            if (!needs_analog_update) {
                DeleteLocal(env, action);
                return true;
            }

            void* target_item = nullptr;
            if (!resolve_target_item(target_item)) {
                DeleteLocal(env, action);
                return false;
            }
            bool invoked = false;
            unsigned int depth = 0;
            while (target_item && depth++ < 256U) {
                void* target = get_object_field(
                    env, target_item, input_target_item_target_field_);
                void* next = get_object_field(
                    env, target_item, input_target_item_next_field_);
                if (ClearException(env, error, "VR locomotion target traversal failed")) {
                    DeleteLocal(env, target);
                    DeleteLocal(env, next);
                    DeleteLocal(env, target_item);
                    DeleteLocal(env, action);
                    return false;
                }
                if (target) {
                    const bool executable =
                        call_boolean(env, target, input_target_can_execute_method_, nullptr) != 0;
                    const bool game_object =
                        is_instance(env, target, game_object_class_) != 0;
                    if (ClearException(env, error, "VR locomotion target eligibility failed")) {
                        DeleteLocal(env, target);
                        DeleteLocal(env, next);
                        DeleteLocal(env, target_item);
                        DeleteLocal(env, action);
                        return false;
                    }
                    if (executable && game_object) {
                        JValue direct_args[3]{};
                        direct_args[0].i = item.action;
                        direct_args[1].f = item.value;
                        direct_args[2].l = action;
                        call_void(
                            env, target, game_object_controller_input_method_, direct_args);
                        if (ClearException(
                                env, error,
                                "GameObject.CallOnInputGameController VR locomotion failed")) {
                            DeleteLocal(env, target);
                            DeleteLocal(env, next);
                            DeleteLocal(env, target_item);
                            DeleteLocal(env, action);
                            return false;
                        }
                        invoked = true;
                    }
                }
                DeleteLocal(env, target);
                DeleteLocal(env, target_item);
                target_item = next;
            }
            if (target_item) {
                DeleteLocal(env, target_item);
                DeleteLocal(env, action);
                SetError(error, "VR locomotion target list exceeded safety bound");
                return false;
            }
            DeleteLocal(env, action);
            return invoked;
        }
        if (digital && !needs_digital_update) {
            DeleteLocal(env, action);
            return true;
        }
        if (analog && !needs_analog_update) {
            DeleteLocal(env, action);
            return true;
        }
        if (!digital && !analog) {
            DeleteLocal(env, action);
            if (item.value == 0.0F && previous_item.value == 0.0F) return true;
            SetError(error, "configured CoJ action uses an unsupported POV input binding");
            return false;
        }

        const std::int32_t device = get_int(
            env, action, digital ? digital_device_field_ : analog_device_field_);
        const std::int32_t code = get_int(
            env, action, digital ? digital_button_field_ : analog_axis_field_);
        const std::int32_t axis_sign = analog
            ? get_int(env, action, analog_axis_sign_field_)
            : 1;
        if (ClearException(env, error, "configured CoJ action binding read failed")) {
            DeleteLocal(env, action);
            return false;
        }

        void* target = nullptr;
        if (!resolve_target_item(target)) {
            DeleteLocal(env, action);
            return false;
        }
        if (!target) {
            DeleteLocal(env, action);
            SetError(error, "selected gameplay target category is empty");
            return false;
        }
        if (digital) {
                // Data/InputActions.def is exact for this build: action 9 fires
                // the left-hand weapon and action 10 the right-hand weapon.
                // InputDigital.Translate only selects the requested hand state.
                // The actual WeaponAttack runs from UpdateHandStates on the next
                // game update and GetFireOriginForWeapon then reads the global
                // Being look-from point. Publish the matching controller origin
                // on the press transition and let CoJ's later
                // UpdateLookAndAimPoints pass naturally reclaim the field.
                void* look_from_point = nullptr;
                JavaPlayerPosition native_look_from{};
                bool fire_origin_overridden = false;
                if (fire_origin_policy.override_for_translate) {
                    auto& transition = last_fire_origin_transitions_[
                        static_cast<std::size_t>(fire_origin_policy.hand)];
                    transition.attempted = true;
                    transition.origin = fire_origins_[
                        static_cast<std::size_t>(fire_origin_policy.hand)];
                    if ((!being_ && !Refresh(error)) ||
                        !EnsureFireOriginAccess(env, error)) {
                        DeleteLocal(env, target);
                        DeleteLocal(env, action);
                        return false;
                    }
                    look_from_point = get_object_field(env, being_, look_from_point_field_);
                    if (!look_from_point ||
                        ClearException(env, error, "Being.m_vLookFromPoint read failed") ||
                        !ReadVector(env, look_from_point, native_look_from, error)) {
                        DeleteLocal(env, look_from_point);
                        DeleteLocal(env, target);
                        DeleteLocal(env, action);
                        return false;
                    }
                    if (!WriteVector(
                            env,
                            look_from_point,
                            fire_origins_[static_cast<std::size_t>(fire_origin_policy.hand)],
                            error)) {
                        std::string restore_error;
                        (void)WriteVector(env, look_from_point, native_look_from, &restore_error);
                        DeleteLocal(env, look_from_point);
                        DeleteLocal(env, target);
                        DeleteLocal(env, action);
                        return false;
                    }
                    fire_origin_overridden = true;
                    transition.write_succeeded = true;
                }
                const JValue args[3]{
                    {.l = target},
                    {.i = device},
                    {.i = code},
                };
                JValue digital_args[4]{};
                digital_args[0] = args[0];
                digital_args[1] = args[1];
                digital_args[2] = args[2];
                digital_args[3].z = current_pressed ? 1U : 0U;
                (void)call_boolean(
                    env, action, digital_translate_method_, digital_args);
                const bool translate_failed =
                    ClearException(env, error, "InputAction.Translate failed");
                if (fire_origin_overridden) {
                    auto& transition = last_fire_origin_transitions_[
                        static_cast<std::size_t>(fire_origin_policy.hand)];
                    transition.translate_succeeded = !translate_failed;
                    transition.retained_for_attack = !translate_failed;
                }
                bool rollback_ok = true;
                if (fire_origin_overridden && translate_failed) {
                    std::string restore_error;
                    rollback_ok = WriteVector(
                        env, look_from_point, native_look_from, &restore_error);
                    if (!rollback_ok) {
                        SetError(
                            error,
                            restore_error.empty()
                                ? "Being.m_vLookFromPoint rollback failed"
                                : restore_error.c_str());
                    }
                    last_fire_origin_transitions_[
                        static_cast<std::size_t>(fire_origin_policy.hand)]
                        .retained_for_attack = false;
                }
                DeleteLocal(env, look_from_point);
                if (translate_failed || !rollback_ok) {
                    DeleteLocal(env, target);
                    DeleteLocal(env, action);
                    return false;
                }
        } else {
            JValue analog_args[4]{};
            analog_args[0].l = target;
            analog_args[1].i = device;
            analog_args[2].i = code;
            analog_args[3].f = item.value * (axis_sign < 0 ? -1.0F : 1.0F);
            (void)call_boolean(
                env, action, analog_translate_method_, analog_args);
        }
        DeleteLocal(env, target);
        if (!digital && ClearException(env, error, "InputAction.Translate failed")) {
            DeleteLocal(env, action);
            return false;
        }
        DeleteLocal(env, action);
        return true;
    };

    bool ok = true;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index].action != previous_values[index].action ||
            !apply_value(values[index], previous_values[index])) {
            ok = false;
            break;
        }
    }

    if (analog_transaction_locked) {
        std::string transaction_error;
        call_void(env, controller, controller_unlock_apply_method_, controller_state_args);
        const bool unlock_failed = ClearException(
            env, &transaction_error,
            "GameInputController.UnlockApplyControllerState VR locomotion failed");
        bool apply_failed = false;
        if (!unlock_failed) {
            call_void(env, controller, controller_apply_method_, controller_state_args);
            apply_failed = ClearException(
                env, &transaction_error,
                "GameInputController.ApplyControllerState VR locomotion failed");
        }
        if (unlock_failed || apply_failed) {
            if (ok) SetError(error, transaction_error.c_str());
            ok = false;
        } else if (ok) {
            last_analog_transaction_applied_ = true;
        }
    }

    DeleteLocal(env, actions);
    DeleteLocal(env, targets);
    DeleteLocal(env, controller);
    if (!ok) return false;
    last_gameplay_input_ = state.active ? state : cojvr::runtime::GameplayInputState{};
    gameplay_input_applied_ = state.active;
    return true;
}

bool JavaPlayerBridge::TryRotateHorizontally(
    const float degrees,
    std::string* error) noexcept {
    if (!std::isfinite(degrees)) {
        SetError(error, "player horizontal rotation is not finite");
        return false;
    }
    if (std::fabs(degrees) <= 1.0e-5F) return true;
    if ((!being_ || !rotate_horizontally_method_) && !Refresh(error)) return false;

    void* env = Environment(error);
    if (!env || !being_ || !rotate_horizontally_method_) {
        SetError(error, "player horizontal-rotation route is unavailable");
        return false;
    }
    const auto call_void = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    if (!call_void) {
        SetError(error, "JNI CallVoidMethodA is unavailable for player horizontal rotation");
        return false;
    }
    JValue args[1]{};
    args[0].f = degrees;
    call_void(env, being_, rotate_horizontally_method_, args);
    return !ClearException(env, error, "PlayerBeing.RotateHorizontally VR rotation failed");
}

CoJCurrentUiResolution JavaPlayerBridge::TryResolveCurrentGameUi(
    void* env,
    void*& ui,
    CoJUiDispatchRoute& route,
    std::string* error) noexcept {
    ui = nullptr;
    route = CoJUiDispatchRoute::none;
    if (!env) {
        SetError(error, "JNI environment is unavailable for game UI lookup");
        return CoJCurrentUiResolution::error;
    }
    const auto find_class = EnvFunction<FindClassFn>(env, kFindClass);
    const auto get_static_field = EnvFunction<GetStaticFieldIdFn>(env, kGetStaticFieldId);
    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto call_object = EnvFunction<CallObjectMethodAFn>(env, kCallObjectMethodA);
    const auto get_field = EnvFunction<GetFieldIdFn>(env, kGetFieldId);
    const auto get_object = EnvFunction<GetObjectFieldFn>(env, kGetObjectField);
    if (!find_class || !get_static_field || !get_static_object || !get_class ||
        !get_method || !call_object || !get_field || !get_object) {
        SetError(error, "required JNI current-game-UI lookup functions are unavailable");
        return CoJCurrentUiResolution::error;
    }

    const auto resolve_menu_ui = [&](void* menu,
                                     const CoJUiDispatchRoute candidate_route) noexcept {
        if (!menu) return CoJCurrentUiResolution::unavailable;
        void* menu_class = get_class(env, menu);
        if (!menu_class || ClearException(env, error, "MainMenuModule class lookup failed")) {
            DeleteLocal(env, menu_class);
            return CoJCurrentUiResolution::error;
        }
        void* get_current_ui = get_method(
            env, menu_class, "GetCurrentUI", "()LGameUserInterface;");
        DeleteLocal(env, menu_class);
        if (!get_current_ui || ClearException(
                env, error, "MainMenuModule.GetCurrentUI lookup failed")) {
            return CoJCurrentUiResolution::error;
        }
        ui = call_object(env, menu, get_current_ui, nullptr);
        if (ClearException(env, error, "MainMenuModule.GetCurrentUI call failed")) {
            DeleteLocal(env, ui);
            ui = nullptr;
            return CoJCurrentUiResolution::error;
        }
        if (!ui) return CoJCurrentUiResolution::unavailable;
        route = candidate_route;
        return CoJCurrentUiResolution::resolved;
    };

    void* game_with_menu_class = find_class(env, "GameWithMenu");
    if (!game_with_menu_class ||
        ClearException(env, error, "FindClass(GameWithMenu) failed")) {
        DeleteLocal(env, game_with_menu_class);
        return CoJCurrentUiResolution::error;
    }
    void* menu_field = get_static_field(
        env, game_with_menu_class, "sm_cMenuModule", "LMainMenuModule;");
    if (!menu_field ||
        ClearException(env, error, "GameWithMenu.sm_cMenuModule lookup failed")) {
        DeleteLocal(env, game_with_menu_class);
        return CoJCurrentUiResolution::error;
    }
    void* menu = get_static_object(env, game_with_menu_class, menu_field);
    DeleteLocal(env, game_with_menu_class);
    if (ClearException(env, error, "GameWithMenu.sm_cMenuModule read failed")) {
        DeleteLocal(env, menu);
        return CoJCurrentUiResolution::error;
    }
    const CoJCurrentUiResolution global_result =
        resolve_menu_ui(menu, CoJUiDispatchRoute::global_menu);
    DeleteLocal(env, menu);
    if (global_result == CoJCurrentUiResolution::resolved) {
        SetError(error, "");
        return CoJCurrentUiResolution::resolved;
    }
    if (global_result == CoJCurrentUiResolution::error) {
        return CoJCurrentUiResolution::error;
    }

    // Escape/loading screens may be owned by the active campaign module while
    // the global menu pointer is null.
    if (!EnsureCampaignAccess(env, error)) return CoJCurrentUiResolution::error;
    void* module = get_static_object(env, lawman_game_class_, active_game_module_field_);
    if (ClearException(env, error, "LawmanGame.sm_cActiveGameModule menu read failed")) {
        DeleteLocal(env, module);
        return CoJCurrentUiResolution::error;
    }
    if (module) {
        void* module_class = get_class(env, module);
        if (!module_class || ClearException(
                env, error, "active LawmanModule class lookup failed")) {
            DeleteLocal(env, module_class);
            DeleteLocal(env, module);
            return CoJCurrentUiResolution::error;
        }
        void* active_menu_field = get_field(
            env, module_class, "cMenu", "LMainMenuModule;");
        DeleteLocal(env, module_class);
        if (!active_menu_field || ClearException(env, error, "LawmanModule.cMenu lookup failed")) {
            DeleteLocal(env, module);
            return CoJCurrentUiResolution::error;
        }
        menu = get_object(env, module, active_menu_field);
        DeleteLocal(env, module);
        if (ClearException(env, error, "LawmanModule.cMenu read failed")) {
            DeleteLocal(env, menu);
            return CoJCurrentUiResolution::error;
        }
        const CoJCurrentUiResolution active_result = resolve_menu_ui(
            menu, CoJUiDispatchRoute::active_game_menu);
        DeleteLocal(env, menu);
        if (active_result == CoJCurrentUiResolution::resolved) {
            SetError(error, "");
            return CoJCurrentUiResolution::resolved;
        }
        if (active_result == CoJCurrentUiResolution::error) {
            return CoJCurrentUiResolution::error;
        }
    } else {
        DeleteLocal(env, module);
    }

    SetError(error, "current global/active game UI is unavailable");
    return CoJCurrentUiResolution::unavailable;
}

bool JavaPlayerBridge::TryResolveMenuForUiRoute(
    void* env,
    const CoJUiDispatchRoute route,
    void*& menu,
    std::string* error) noexcept {
    menu = nullptr;
    if (!env) {
        SetError(error, "JNI environment is unavailable for menu lookup");
        return false;
    }
    const auto find_class = EnvFunction<FindClassFn>(env, kFindClass);
    const auto get_static_field = EnvFunction<GetStaticFieldIdFn>(env, kGetStaticFieldId);
    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_field = EnvFunction<GetFieldIdFn>(env, kGetFieldId);
    const auto get_object = EnvFunction<GetObjectFieldFn>(env, kGetObjectField);
    if (!find_class || !get_static_field || !get_static_object || !get_class ||
        !get_field || !get_object) {
        SetError(error, "required JNI menu lookup functions are unavailable");
        return false;
    }

    if (route == CoJUiDispatchRoute::global_menu) {
        void* game_with_menu_class = find_class(env, "GameWithMenu");
        if (!game_with_menu_class ||
            ClearException(env, error, "FindClass(GameWithMenu) failed for menu route")) {
            DeleteLocal(env, game_with_menu_class);
            return false;
        }
        void* menu_field = get_static_field(
            env, game_with_menu_class, "sm_cMenuModule", "LMainMenuModule;");
        if (!menu_field || ClearException(
                env, error, "GameWithMenu.sm_cMenuModule lookup failed for menu route")) {
            DeleteLocal(env, game_with_menu_class);
            return false;
        }
        menu = get_static_object(env, game_with_menu_class, menu_field);
        DeleteLocal(env, game_with_menu_class);
        if (ClearException(env, error, "GameWithMenu.sm_cMenuModule read failed for menu route") ||
            !menu) {
            DeleteLocal(env, menu);
            menu = nullptr;
            SetError(error, "global MainMenuModule is unavailable");
            return false;
        }
        return true;
    }

    if (route == CoJUiDispatchRoute::active_game_menu) {
        if (!EnsureCampaignAccess(env, error)) return false;
        void* module = get_static_object(env, lawman_game_class_, active_game_module_field_);
        if (ClearException(env, error, "LawmanGame.sm_cActiveGameModule menu route read failed") ||
            !module) {
            DeleteLocal(env, module);
            SetError(error, "active LawmanModule is unavailable for menu route");
            return false;
        }
        void* module_class = get_class(env, module);
        if (!module_class || ClearException(
                env, error, "active LawmanModule class lookup failed for menu route")) {
            DeleteLocal(env, module_class);
            DeleteLocal(env, module);
            return false;
        }
        void* menu_field = get_field(env, module_class, "cMenu", "LMainMenuModule;");
        DeleteLocal(env, module_class);
        if (!menu_field || ClearException(env, error, "LawmanModule.cMenu lookup failed for menu route")) {
            DeleteLocal(env, module);
            return false;
        }
        menu = get_object(env, module, menu_field);
        DeleteLocal(env, module);
        if (ClearException(env, error, "LawmanModule.cMenu read failed for menu route") || !menu) {
            DeleteLocal(env, menu);
            menu = nullptr;
            SetError(error, "active MainMenuModule is unavailable");
            return false;
        }
        return true;
    }

    SetError(error, "UI route has no MainMenuModule owner");
    return false;
}

bool JavaPlayerBridge::TryDispatchIntroSkip(
    void* env,
    std::string* error) noexcept {
    const auto find_class = EnvFunction<FindClassFn>(env, kFindClass);
    const auto get_static_field = EnvFunction<GetStaticFieldIdFn>(env, kGetStaticFieldId);
    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto call_void = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    if (!find_class || !get_static_field || !get_static_object || !get_class ||
        !get_method || !call_void) {
        SetError(error, "required JNI intro-skip functions are unavailable");
        return false;
    }

    void* game_with_menu_class = find_class(env, "GameWithMenu");
    if (!game_with_menu_class || ClearException(env, error, "FindClass(GameWithMenu) failed")) {
        DeleteLocal(env, game_with_menu_class);
        return false;
    }
    void* intro_field = get_static_field(
        env, game_with_menu_class, "sm_cIntroModule", "LIntroModule;");
    if (!intro_field || ClearException(
            env, error, "GameWithMenu.sm_cIntroModule lookup failed")) {
        DeleteLocal(env, game_with_menu_class);
        return false;
    }
    void* intro = get_static_object(env, game_with_menu_class, intro_field);
    DeleteLocal(env, game_with_menu_class);
    if (ClearException(env, error, "GameWithMenu.sm_cIntroModule read failed") || !intro) {
        DeleteLocal(env, intro);
        SetError(error, "intro module is unavailable");
        return false;
    }
    void* intro_class = get_class(env, intro);
    if (!intro_class || ClearException(env, error, "IntroModule class lookup failed")) {
        DeleteLocal(env, intro_class);
        DeleteLocal(env, intro);
        return false;
    }
    void* on_input_key = get_method(env, intro_class, "OnInputKey", "(IZC)V");
    DeleteLocal(env, intro_class);
    if (!on_input_key || ClearException(env, error, "IntroModule.OnInputKey lookup failed")) {
        DeleteLocal(env, intro);
        return false;
    }
    JValue args[3]{};
    args[0].i = 1;
    args[1].z = 0;
    args[2].c = 0;
    call_void(env, intro, on_input_key, args);
    DeleteLocal(env, intro);
    if (ClearException(env, error, "IntroModule.OnInputKey call failed")) return false;
    SetError(error, "");
    return true;
}

bool JavaPlayerBridge::TryDismissPausedHint(
    void* env,
    bool& dismissed,
    std::string* error) noexcept {
    dismissed = false;
    if (!env || !EnsureCampaignAccess(env, error)) return false;

    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto call_object = EnvFunction<CallObjectMethodAFn>(env, kCallObjectMethodA);
    const auto call_boolean = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    const auto call_void = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    if (!get_static_object || !get_class || !get_method || !call_object ||
        !call_boolean || !call_void) {
        SetError(error, "required JNI paused-hint functions are unavailable");
        return false;
    }

    void* module = get_static_object(env, lawman_game_class_, active_game_module_field_);
    if (ClearException(env, error, "LawmanGame.sm_cActiveGameModule hint read failed")) {
        DeleteLocal(env, module);
        return false;
    }
    if (!module) return true;

    void* module_class = get_class(env, module);
    if (!module_class || ClearException(env, error, "active LawmanModule class lookup failed")) {
        DeleteLocal(env, module_class);
        DeleteLocal(env, module);
        return false;
    }
    void* get_hint_manager =
        get_method(env, module_class, "GetHintManager", "()LHintManager;");
    DeleteLocal(env, module_class);
    if (!get_hint_manager ||
        ClearException(env, error, "LawmanModule.GetHintManager lookup failed")) {
        DeleteLocal(env, module);
        return false;
    }
    void* manager = call_object(env, module, get_hint_manager, nullptr);
    DeleteLocal(env, module);
    if (ClearException(env, error, "LawmanModule.GetHintManager call failed")) {
        DeleteLocal(env, manager);
        return false;
    }
    if (!manager) return true;

    void* manager_class = get_class(env, manager);
    if (!manager_class || ClearException(env, error, "HintManager class lookup failed")) {
        DeleteLocal(env, manager_class);
        DeleteLocal(env, manager);
        return false;
    }
    void* get_active_hint = get_method(env, manager_class, "GetActiveHint", "()LHint;");
    void* disable_current_hint = get_method(env, manager_class, "DisableCurrentHint", "()V");
    DeleteLocal(env, manager_class);
    if (!get_active_hint || !disable_current_hint ||
        ClearException(env, error, "HintManager active-hint methods lookup failed")) {
        DeleteLocal(env, manager);
        return false;
    }

    void* hint = call_object(env, manager, get_active_hint, nullptr);
    if (ClearException(env, error, "HintManager.GetActiveHint call failed")) {
        DeleteLocal(env, hint);
        DeleteLocal(env, manager);
        return false;
    }
    if (!hint) {
        DeleteLocal(env, manager);
        return true;
    }

    void* hint_class = get_class(env, hint);
    if (!hint_class || ClearException(env, error, "Hint class lookup failed")) {
        DeleteLocal(env, hint_class);
        DeleteLocal(env, hint);
        DeleteLocal(env, manager);
        return false;
    }
    void* is_pause_game = get_method(env, hint_class, "IsPauseGame", "()Z");
    DeleteLocal(env, hint_class);
    if (!is_pause_game || ClearException(env, error, "Hint.IsPauseGame lookup failed")) {
        DeleteLocal(env, hint);
        DeleteLocal(env, manager);
        return false;
    }
    const bool pauses_game = call_boolean(env, hint, is_pause_game, nullptr) != 0;
    DeleteLocal(env, hint);
    if (ClearException(env, error, "Hint.IsPauseGame call failed")) {
        DeleteLocal(env, manager);
        return false;
    }
    if (!pauses_game) {
        DeleteLocal(env, manager);
        return true;
    }

    call_void(env, manager, disable_current_hint, nullptr);
    DeleteLocal(env, manager);
    if (ClearException(env, error, "HintManager.DisableCurrentHint call failed")) return false;
    dismissed = true;
    return true;
}

bool JavaPlayerBridge::TryProcessUiPointer(
    const float pixel_x,
    const float pixel_y,
    std::string* error) noexcept {
    if (!std::isfinite(pixel_x) || !std::isfinite(pixel_y) || pixel_x < 0.0F || pixel_y < 0.0F) {
        SetError(error, "game UI pointer coordinates are invalid");
        return false;
    }
    void* env = Environment(error);
    if (!env) return false;

    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto call_object = EnvFunction<CallObjectMethodAFn>(env, kCallObjectMethodA);
    const auto call_void = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    const auto new_object = EnvFunction<NewObjectAFn>(env, kNewObjectA);
    if (!get_class || !get_method || !call_object || !call_void || !new_object) {
        SetError(error, "required JNI game-UI mouse functions are unavailable");
        return false;
    }

    // The main-menu cursor can exist while GetCurrentUI() is null. Resolve its
    // owning menu directly so pointer motion is not gated on an unrelated UI
    // object. This is the state observed in the 20260920 headset run.
    void* menu = nullptr;
    CoJUiDispatchRoute menu_route = CoJUiDispatchRoute::global_menu;
    std::string menu_error;
    if (!TryResolveMenuForUiRoute(env, menu_route, menu, &menu_error)) {
        menu_route = CoJUiDispatchRoute::active_game_menu;
        if (!TryResolveMenuForUiRoute(env, menu_route, menu, &menu_error)) {
            SetError(error, menu_error.c_str());
            return false;
        }
    }
    const auto pointer_policy = BuildCoJUiPointerDispatchPolicy(menu != nullptr, false);
    if (!pointer_policy.dispatch_cursor) {
        DeleteLocal(env, menu);
        SetError(error, "MainMenuModule is unavailable for cursor dispatch");
        return false;
    }
    void* menu_class = get_class(env, menu);
    if (!menu_class || ClearException(env, error, "MainMenuModule class lookup failed for cursor")) {
        DeleteLocal(env, menu_class);
        DeleteLocal(env, menu);
        return false;
    }
    void* get_cursor = get_method(env, menu_class, "GetGlobalCursor", "()LUICursorGame;");
    DeleteLocal(env, menu_class);
    if (!get_cursor || ClearException(env, error, "MainMenuModule.GetGlobalCursor lookup failed")) {
        DeleteLocal(env, menu);
        return false;
    }
    void* cursor = call_object(env, menu, get_cursor, nullptr);
    DeleteLocal(env, menu);
    if (!cursor || ClearException(env, error, "MainMenuModule.GetGlobalCursor call failed")) {
        DeleteLocal(env, cursor);
        SetError(error, "global UICursorGame is unavailable");
        return false;
    }
    void* cursor_class = get_class(env, cursor);
    if (!cursor_class || ClearException(env, error, "UICursorGame class lookup failed")) {
        DeleteLocal(env, cursor_class);
        DeleteLocal(env, cursor);
        return false;
    }
    void* set_cursor = get_method(env, cursor_class, "SetPos", "(LVector;)V");
    void* move_cursor = get_method(env, cursor_class, "OnMouseMove", "(FFI)V");
    DeleteLocal(env, cursor_class);
    if (!set_cursor || !move_cursor ||
        ClearException(env, error, "UICursor.SetPos/OnMouseMove lookup failed")) {
        DeleteLocal(env, cursor);
        return false;
    }
    if (!EnsureVectorAccess(env, error)) {
        DeleteLocal(env, cursor);
        return false;
    }
    void* logical_position = new_object(env, vector_class_, vector_constructor_, nullptr);
    if (!logical_position ||
        ClearException(env, error, "Vector construction for UI cursor failed")) {
        DeleteLocal(env, logical_position);
        DeleteLocal(env, cursor);
        return false;
    }
    if (!WriteVector(env, logical_position, {pixel_x, pixel_y, 0.0F}, error)) {
        DeleteLocal(env, logical_position);
        DeleteLocal(env, cursor);
        return false;
    }
    JValue set_args[1]{};
    set_args[0].l = logical_position;
    call_void(env, cursor, set_cursor, set_args);
    DeleteLocal(env, logical_position);
    if (ClearException(env, error, "UICursor.SetPos call failed")) {
        DeleteLocal(env, cursor);
        return false;
    }
    JValue move_args[3]{};
    move_args[0].f = pixel_x;
    move_args[1].f = pixel_y;
    move_args[2].i = 0;
    call_void(env, cursor, move_cursor, move_args);
    DeleteLocal(env, cursor);
    if (ClearException(env, error, "UICursorGame.OnMouseMove call failed")) {
        return false;
    }

    // Mouse processing augments the cursor when a concrete UI exists, but the
    // cursor update above remains successful in the menu state where it does not.
    void* ui = nullptr;
    CoJUiDispatchRoute ui_route = CoJUiDispatchRoute::none;
    std::string ui_error;
    const CoJCurrentUiResolution ui_resolution =
        TryResolveCurrentGameUi(env, ui, ui_route, &ui_error);
    const bool current_ui_available =
        ui_resolution == CoJCurrentUiResolution::resolved && ui != nullptr;
    const auto final_policy = BuildCoJUiPointerDispatchPolicy(true, current_ui_available);
    if (!final_policy.dispatch_process_mouse) {
        DeleteLocal(env, ui);
        SetError(error, "");
        return true;
    }
    void* ui_class = get_class(env, ui);
    if (!ui_class || ClearException(env, error, "GameUserInterface class lookup failed")) {
        DeleteLocal(env, ui_class);
        DeleteLocal(env, ui);
        return false;
    }
    void* process_mouse = get_method(env, ui_class, "SetProcessMouse", "()V");
    DeleteLocal(env, ui_class);
    if (!process_mouse || ClearException(env, error, "GameUserInterface.SetProcessMouse lookup failed")) {
        DeleteLocal(env, ui);
        return false;
    }
    call_void(env, ui, process_mouse, nullptr);
    DeleteLocal(env, ui);
    if (ClearException(env, error, "GameUserInterface.SetProcessMouse call failed")) return false;
    SetError(error, "");
    return true;
}

bool JavaPlayerBridge::TryDispatchUiBackPress(
    std::string* error,
    CoJUiDispatchRoute* route) noexcept {
    if (route) *route = CoJUiDispatchRoute::none;
    void* env = Environment(error);
    if (!env) return false;
    void* ui = nullptr;
    CoJUiDispatchRoute ui_route = CoJUiDispatchRoute::none;
    std::string ui_error;
    const CoJCurrentUiResolution ui_resolution =
        TryResolveCurrentGameUi(env, ui, ui_route, &ui_error);
    if (ui_resolution == CoJCurrentUiResolution::error) {
        DeleteLocal(env, ui);
        SetError(error, ui_error.c_str());
        return false;
    }
    const bool current_ui_available =
        ui_resolution == CoJCurrentUiResolution::resolved && ui != nullptr;
    const auto policy = BuildCoJUiBackDispatchPolicy(current_ui_available && ui != nullptr);
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto call_void = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    if (!get_class || !get_method || !call_void || !get_static_object) {
        DeleteLocal(env, ui);
        SetError(error, "required JNI game-UI back functions are unavailable");
        return false;
    }

    if (ShouldFallbackCoJUiBack(ui_resolution)) {
        DeleteLocal(env, ui);
        if (!EnsureCampaignAccess(env, error)) return false;
        void* module = get_static_object(env, lawman_game_class_, active_game_module_field_);
        if (!module ||
            ClearException(env, error, "LawmanGame.sm_cActiveGameModule Escape read failed")) {
            DeleteLocal(env, module);
            if (error && !ui_error.empty()) {
                try { *error += "; ui=" + ui_error; } catch (...) {}
            }
            return false;
        }
        void* module_class = get_class(env, module);
        if (!module_class ||
            ClearException(env, error, "active LawmanModule class lookup failed for Escape")) {
            DeleteLocal(env, module_class);
            DeleteLocal(env, module);
            return false;
        }
        void* on_input_key = get_method(env, module_class, "OnInputKey", "(IZC)V");
        DeleteLocal(env, module_class);
        if (!on_input_key ||
            ClearException(env, error, "LawmanModule.OnInputKey lookup failed for Escape")) {
            DeleteLocal(env, module);
            return false;
        }
        JValue args[3]{};
        args[0].i = policy.key_code;
        args[1].z = policy.press ? 1 : 0;
        args[2].c = 0;
        call_void(env, module, on_input_key, args);
        if (ClearException(env, error, "LawmanModule Escape press failed")) {
            DeleteLocal(env, module);
            return false;
        }
        args[1].z = policy.release ? 0 : 1;
        call_void(env, module, on_input_key, args);
        DeleteLocal(env, module);
        if (ClearException(env, error, "LawmanModule Escape release failed")) return false;
        if (route) *route = CoJUiDispatchRoute::active_game_module;
        SetError(error, "");
        return true;
    }

    void* ui_class = get_class(env, ui);
    if (!ui_class || ClearException(env, error, "GameUserInterface class lookup failed for back")) {
        DeleteLocal(env, ui_class);
        DeleteLocal(env, ui);
        return false;
    }
    void* call_input = get_method(env, ui_class, "CallOnInputKeyGlobal", "(IZI)V");
    DeleteLocal(env, ui_class);
    if (!call_input || ClearException(env, error, "GameUserInterface.CallOnInputKeyGlobal lookup failed")) {
        DeleteLocal(env, ui);
        return false;
    }

    JValue args[3]{};
    args[0].i = policy.key_code;
    args[1].z = policy.press ? 1 : 0;
    args[2].i = 0;
    call_void(env, ui, call_input, args);
    if (ClearException(env, error, "GameUserInterface Escape press failed")) {
        DeleteLocal(env, ui);
        return false;
    }
    args[1].z = policy.release ? 0 : 1;
    call_void(env, ui, call_input, args);
    DeleteLocal(env, ui);
    if (ClearException(env, error, "GameUserInterface Escape release failed")) return false;
    if (route) *route = ui_route;
    SetError(error, "");
    return true;
}

bool JavaPlayerBridge::TryDispatchUiSelectPress(
    const bool require_loading_ui,
    bool* current_ui_is_loading,
    std::string* error,
    bool* paused_hint_dismissed,
    CoJUiDispatchRoute* route) noexcept {
    if (current_ui_is_loading) *current_ui_is_loading = false;
    if (paused_hint_dismissed) *paused_hint_dismissed = false;
    if (route) *route = CoJUiDispatchRoute::none;
    void* env = Environment(error);
    if (!env) return false;

    bool dismissed_hint = false;
    std::string hint_error;
    if (!TryDismissPausedHint(env, dismissed_hint, &hint_error)) {
        // IntroModule exists before the campaign module is guaranteed to be
        // available. Do not let a pre-game hint-manager lookup prevent the
        // exact startup video/logo skip route.
        if (!require_loading_ui && TryDispatchIntroSkip(env, error)) {
            if (route) *route = CoJUiDispatchRoute::intro_skip;
            return true;
        }
        SetError(error, hint_error.c_str());
        return false;
    }
    if (dismissed_hint) {
        if (paused_hint_dismissed) *paused_hint_dismissed = true;
        if (route) *route = CoJUiDispatchRoute::paused_hint;
        return true;
    }

    void* ui = nullptr;
    CoJUiDispatchRoute ui_route = CoJUiDispatchRoute::none;
    const CoJCurrentUiResolution ui_resolution =
        TryResolveCurrentGameUi(env, ui, ui_route, error);
    if (ui_resolution != CoJCurrentUiResolution::resolved) {
        if (!require_loading_ui && TryDispatchIntroSkip(env, error)) {
            if (route) *route = CoJUiDispatchRoute::intro_skip;
            return true;
        }
        return false;
    }

    const auto find_class = EnvFunction<FindClassFn>(env, kFindClass);
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto call_void = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    const auto is_instance = EnvFunction<IsInstanceOfFn>(env, kIsInstanceOf);
    if (!find_class || !get_class || !get_method || !call_void || !is_instance) {
        DeleteLocal(env, ui);
        SetError(error, "required JNI game-UI input functions are unavailable");
        return false;
    }

    void* loading_class = find_class(env, "GameUILoading");
    if (!loading_class || ClearException(env, error, "FindClass(GameUILoading) failed")) {
        DeleteLocal(env, loading_class);
        DeleteLocal(env, ui);
        return false;
    }
    const bool is_loading = is_instance(env, ui, loading_class) != 0;
    DeleteLocal(env, loading_class);
    if (ClearException(env, error, "GameUILoading type check failed")) {
        DeleteLocal(env, ui);
        return false;
    }
    if (current_ui_is_loading) *current_ui_is_loading = is_loading;
    if (require_loading_ui && !is_loading) {
        DeleteLocal(env, ui);
        SetError(error, "current game UI is not GameUILoading");
        return false;
    }

    void* ui_class = get_class(env, ui);
    if (!ui_class || ClearException(env, error, "GameUserInterface class lookup failed")) {
        DeleteLocal(env, ui_class);
        DeleteLocal(env, ui);
        return false;
    }
    if (is_loading) {
        void* on_input_key = get_method(env, ui_class, "OnInputKey", "(IZC)V");
        DeleteLocal(env, ui_class);
        if (!on_input_key || ClearException(env, error, "GameUILoading.OnInputKey lookup failed")) {
            DeleteLocal(env, ui);
            return false;
        }
        JValue loading_args[3]{};
        loading_args[0].i = 1;
        loading_args[1].z = 0;
        loading_args[2].c = 0;
        call_void(env, ui, on_input_key, loading_args);
        DeleteLocal(env, ui);
        if (ClearException(env, error, "GameUILoading.OnInputKey call failed")) return false;
        if (route) *route = CoJUiDispatchRoute::loading_ui;
        SetError(error, "");
        return true;
    }
    void* press = get_method(env, ui_class, "CallEnterKeyPressed", "()V");
    void* release = get_method(env, ui_class, "CallEnterKeyReleased", "()V");
    DeleteLocal(env, ui_class);
    if (!press || !release ||
        ClearException(env, error, "GameUserInterface Enter helpers lookup failed")) {
        DeleteLocal(env, ui);
        return false;
    }

    call_void(env, ui, press, nullptr);
    if (ClearException(env, error, "GameUserInterface.CallEnterKeyPressed failed")) {
        DeleteLocal(env, ui);
        return false;
    }
    call_void(env, ui, release, nullptr);
    DeleteLocal(env, ui);
    if (ClearException(env, error, "GameUserInterface.CallEnterKeyReleased failed")) {
        return false;
    }
    if (route) *route = ui_route;
    return true;
}

bool JavaPlayerBridge::TrySetPerHandAimDirection(
    const int hand,
    const JavaPlayerPosition& direction,
    std::string* error) noexcept {
    if (hand < 0 || hand > 1 || !std::isfinite(direction.x) ||
        !std::isfinite(direction.y) || !std::isfinite(direction.z)) {
        SetError(error, "per-hand aim direction input is invalid");
        return false;
    }
    const float length_squared = direction.x * direction.x + direction.y * direction.y +
        direction.z * direction.z;
    if (!std::isfinite(length_squared) || length_squared <= 1.0e-8F) {
        SetError(error, "per-hand aim direction is degenerate");
        return false;
    }
    if (!being_ && !Refresh(error)) return false;
    void* env = Environment(error);
    if (!env || !being_ || !EnsureAimAccess(env, error)) return false;
    const auto get_object = EnvFunction<GetObjectFieldFn>(env, kGetObjectField);
    const auto get_length = EnvFunction<GetArrayLengthFn>(env, kGetArrayLength);
    const auto get_element = EnvFunction<GetObjectArrayElementFn>(env, kGetObjectArrayElement);
    if (!get_object || !get_length || !get_element) {
        SetError(error, "required JNI per-hand aim functions are unavailable");
        return false;
    }
    void* directions = get_object(env, being_, look_dir_for_hand_field_);
    if (!directions || ClearException(env, error, "per-hand aim array read failed")) {
        DeleteLocal(env, directions);
        return false;
    }
    const std::int32_t count = get_length(env, directions);
    if (ClearException(env, error, "per-hand aim array length read failed") || count <= hand) {
        DeleteLocal(env, directions);
        SetError(error, "per-hand aim array is incomplete");
        return false;
    }
    void* vector = get_element(env, directions, hand);
    if (!vector || ClearException(env, error, "per-hand aim vector read failed")) {
        DeleteLocal(env, vector);
        DeleteLocal(env, directions);
        return false;
    }
    const float inverse_length = 1.0F / std::sqrt(length_squared);
    const JavaPlayerPosition normalized{
        direction.x * inverse_length,
        direction.y * inverse_length,
        direction.z * inverse_length,
    };
    const bool written = WriteVector(env, vector, normalized, error);
    DeleteLocal(env, vector);
    DeleteLocal(env, directions);
    return written;
}

bool JavaPlayerBridge::TrySetPerHandAimOrigin(
    const int hand,
    const JavaPlayerPosition& origin,
    std::string* error) noexcept {
    if (hand < 0 || hand > 1 || !std::isfinite(origin.x) ||
        !std::isfinite(origin.y) || !std::isfinite(origin.z)) {
        SetError(error, "per-hand aim origin input is invalid");
        return false;
    }
    if (!being_ && !Refresh(error)) return false;
    void* env = Environment(error);
    if (!env || !being_ || !EnsureAimAccess(env, error)) return false;
    const auto get_object = EnvFunction<GetObjectFieldFn>(env, kGetObjectField);
    const auto get_length = EnvFunction<GetArrayLengthFn>(env, kGetArrayLength);
    const auto get_element = EnvFunction<GetObjectArrayElementFn>(env, kGetObjectArrayElement);
    if (!get_object || !get_length || !get_element) {
        SetError(error, "required JNI per-hand aim-origin functions are unavailable");
        return false;
    }
    void* origins = get_object(env, being_, aim_from_point_field_);
    if (!origins || ClearException(env, error, "per-hand aim-origin array read failed")) {
        DeleteLocal(env, origins);
        return false;
    }
    const std::int32_t count = get_length(env, origins);
    if (ClearException(env, error, "per-hand aim-origin array length read failed") || count <= hand) {
        DeleteLocal(env, origins);
        SetError(error, "per-hand aim-origin array is incomplete");
        return false;
    }
    void* vector = get_element(env, origins, hand);
    if (!vector || ClearException(env, error, "per-hand aim-origin vector read failed")) {
        DeleteLocal(env, vector);
        DeleteLocal(env, origins);
        return false;
    }
    const bool written = WriteVector(env, vector, origin, error);
    DeleteLocal(env, vector);
    DeleteLocal(env, origins);
    return written;
}

void JavaPlayerBridge::SetPerHandFireOrigin(
    const int hand,
    const JavaPlayerPosition& origin,
    const bool valid) noexcept {
    if (hand < 0 || hand >= static_cast<int>(fire_origins_.size())) return;
    const bool finite = std::isfinite(origin.x) && std::isfinite(origin.y) &&
        std::isfinite(origin.z);
    fire_origins_[static_cast<std::size_t>(hand)] = finite ? origin : JavaPlayerPosition{};
    fire_origin_valid_[static_cast<std::size_t>(hand)] = valid && finite;
}

void JavaPlayerBridge::ClearBeing(void* env) noexcept {
    if (being_ && env) {
        if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
            delete_global(env, being_);
        }
    }
    being_ = nullptr;
    get_mesh_element_method_ = nullptr;
    get_element_id_method_ = nullptr;
    hide_element_method_ = nullptr;
    unhide_element_method_ = nullptr;
    is_element_hidden_method_ = nullptr;
    get_bone_joint_method_ = nullptr;
    get_bone_direction_method_ = nullptr;
    get_bone_perpendicular_method_ = nullptr;
    get_element_position_method_ = nullptr;
    get_element_left_method_ = nullptr;
    get_element_up_method_ = nullptr;
    set_element_world_basis_method_ = nullptr;
    rotate_element_with_children_method_ = nullptr;
    bone_rotate_method_ = nullptr;
    get_position_vector_method_ = nullptr;
    set_position_method_ = nullptr;
    rotate_horizontally_method_ = nullptr;
    get_time_method_ = nullptr;
    get_time_delta_method_ = nullptr;
    get_forward_speed_method_ = nullptr;
    get_side_speed_method_ = nullptr;
    get_current_vertical_speed_method_ = nullptr;
    get_wanted_local_speed_method_ = nullptr;
    get_run_method_ = nullptr;
    can_run_method_ = nullptr;
    get_speed_state_method_ = nullptr;
    can_jump_method_ = nullptr;
    is_jumping_method_ = nullptr;
    get_jump_height_method_ = nullptr;
    get_stair_height_method_ = nullptr;
    get_ode_walk_state_method_ = nullptr;
    update_body_rotation_method_ = nullptr;
    current_head_vertical_field_ = nullptr;
    current_head_horizontal_field_ = nullptr;
    current_spine_vertical_field_ = nullptr;
    current_spine_horizontal_field_ = nullptr;
    body_rotation_lookup_attempted_ = false;
    movement_observation_lookup_attempted_ = false;
    bone_read_lookup_attempted_ = false;
    element_world_read_lookup_attempted_ = false;
    element_world_basis_lookup_attempted_ = false;
    element_rotation_lookup_attempted_ = false;
    bone_rotation_lookup_attempted_ = false;
    element_visibility_lookup_attempted_ = false;
    aim_lookup_attempted_ = false;
    look_dir_for_hand_field_ = nullptr;
    aim_from_point_field_ = nullptr;
    fire_origin_lookup_attempted_ = false;
    look_from_point_field_ = nullptr;
    weapon_reload_lookup_attempted_ = false;
    is_weapon_reloading_method_ = nullptr;
    fire_origins_ = {};
    fire_origin_valid_ = {};
}

bool JavaPlayerBridge::EnsureMovementObservationAccess(
    void* env, std::string* error) noexcept {
    if (movement_observation_lookup_attempted_) {
        if (get_time_method_ && get_time_delta_method_ && get_forward_speed_method_ &&
            get_side_speed_method_ && get_current_vertical_speed_method_ &&
            get_wanted_local_speed_method_ && get_run_method_ && can_run_method_ &&
            get_speed_state_method_ && can_jump_method_ && is_jumping_method_ &&
            get_jump_height_method_ && get_stair_height_method_ &&
            get_ode_walk_state_method_) {
            return EnsureVectorAccess(env, error);
        }
        SetError(error, "player movement observation path is unavailable");
        return false;
    }
    movement_observation_lookup_attempted_ = true;
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    if (!get_class || !get_method || !being_) {
        SetError(error, "required JNI movement lookup functions are unavailable");
        return false;
    }
    void* player_class = get_class(env, being_);
    if (!player_class || ClearException(env, error, "GetObjectClass(movement) failed")) {
        DeleteLocal(env, player_class);
        return false;
    }
    get_time_method_ = get_method(env, player_class, "GetTime", "()F");
    get_time_delta_method_ = get_method(env, player_class, "GetTimeDelta", "()F");
    get_forward_speed_method_ = get_method(env, player_class, "GetForwardSpeed", "()F");
    get_side_speed_method_ = get_method(env, player_class, "GetSideSpeed", "()F");
    get_current_vertical_speed_method_ =
        get_method(env, player_class, "GetCurrentVertSpeed", "()F");
    get_wanted_local_speed_method_ =
        get_method(env, player_class, "ODEWalk_GetWantedLocalSpeed", "(LVector;)V");
    get_run_method_ = get_method(env, player_class, "GetRun", "()Z");
    can_run_method_ = get_method(env, player_class, "CanRun", "()Z");
    get_speed_state_method_ = get_method(env, player_class, "GetSpeedState", "()I");
    can_jump_method_ = get_method(env, player_class, "CanJump", "()Z");
    is_jumping_method_ = get_method(env, player_class, "IsJumping", "()Z");
    // PropGetJumpHeight is side-effect free. PlayerBeing.GetJumpHeight clears
    // the one-shot big-jump flag and must never be called by diagnostics.
    get_jump_height_method_ = get_method(env, player_class, "PropGetJumpHeight", "()F");
    get_stair_height_method_ =
        get_method(env, player_class, "ODEWalk_GetStairHeight", "()F");
    get_ode_walk_state_method_ = get_method(env, player_class, "ODEWalk_GetState", "()I");
    const bool failed = ClearException(env, error, "movement method lookup failed");
    DeleteLocal(env, player_class);
    if (failed || !get_time_method_ || !get_time_delta_method_ ||
        !get_forward_speed_method_ || !get_side_speed_method_ ||
        !get_current_vertical_speed_method_ || !get_wanted_local_speed_method_ ||
        !get_run_method_ || !can_run_method_ || !get_speed_state_method_ ||
        !can_jump_method_ || !is_jumping_method_ || !get_jump_height_method_ ||
        !get_stair_height_method_ || !get_ode_walk_state_method_) {
        return false;
    }
    return EnsureVectorAccess(env, error);
}

bool JavaPlayerBridge::ResolveBeingMethods(
    void* env, void* local_being, std::string* error) noexcept {
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto new_global = EnvFunction<NewGlobalRefFn>(env, kNewGlobalRef);
    if (!get_class || !get_method || !new_global) {
        SetError(error, "required JNI player method lookup functions are unavailable");
        return false;
    }

    void* player_class = get_class(env, local_being);
    if (!player_class || ClearException(env, error, "GetObjectClass(player being) failed")) {
        DeleteLocal(env, player_class);
        return false;
    }

    void* get_mesh = get_method(env, player_class, "GetMeshElemFromBoneID", "(B)I");
    void* get_position = get_method(env, player_class, "GetPositionVectorVolatile", "()LVector;");
    void* set_position = get_method(env, player_class, "SetPosition", "(FFF)V");
    void* rotate_horizontally = get_method(env, player_class, "RotateHorizontally", "(F)V");
    const bool lookup_failed = ClearException(env, error, "player native-method lookup failed");
    DeleteLocal(env, player_class);
    if (lookup_failed || !get_mesh || !get_position || !set_position || !rotate_horizontally) {
        SetError(error, "player native-method lookup was incomplete");
        return false;
    }

    void* global_being = new_global(env, local_being);
    if (!global_being || ClearException(env, error, "NewGlobalRef(player being) failed")) return false;

    ClearBeing(env);
    being_ = global_being;
    ++being_generation_;
    if (being_generation_ == 0) being_generation_ = 1;
    get_mesh_element_method_ = get_mesh;
    get_position_vector_method_ = get_position;
    set_position_method_ = set_position;
    rotate_horizontally_method_ = rotate_horizontally;
    return true;
}

bool JavaPlayerBridge::EnsureElementVisibilityAccess(
    void* env, std::string* error) noexcept {
    if (element_visibility_lookup_attempted_) {
        if (get_element_id_method_ && hide_element_method_ && unhide_element_method_ &&
            is_element_hidden_method_) {
            return true;
        }
        SetError(error, "player mesh-element visibility path is unavailable");
        return false;
    }
    element_visibility_lookup_attempted_ = true;
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    if (!get_class || !get_method || !being_) {
        SetError(error, "required JNI mesh-element visibility lookup functions are unavailable");
        return false;
    }
    void* player_class = get_class(env, being_);
    if (!player_class || ClearException(env, error, "GetObjectClass(element visibility) failed")) {
        DeleteLocal(env, player_class);
        return false;
    }
    get_element_id_method_ =
        get_method(env, player_class, "GetElementID", "(Ljava/lang/String;)I");
    hide_element_method_ = get_method(env, player_class, "HideElement", "(I)Z");
    unhide_element_method_ = get_method(env, player_class, "UnhideElement", "(I)Z");
    is_element_hidden_method_ = get_method(env, player_class, "IsElementHidden", "(I)Z");
    const bool failed = ClearException(env, error, "mesh-element visibility method lookup failed");
    DeleteLocal(env, player_class);
    if (failed || !get_element_id_method_ || !hide_element_method_ ||
        !unhide_element_method_ || !is_element_hidden_method_) {
        get_element_id_method_ = nullptr;
        hide_element_method_ = nullptr;
        unhide_element_method_ = nullptr;
        is_element_hidden_method_ = nullptr;
        return false;
    }
    return true;
}

bool JavaPlayerBridge::EnsureAimAccess(void* env, std::string* error) noexcept {
    if (aim_lookup_attempted_) {
        if (look_dir_for_hand_field_ && aim_from_point_field_) return true;
        SetError(error, "player per-hand aim path is unavailable");
        return false;
    }
    aim_lookup_attempted_ = true;
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_field = EnvFunction<GetFieldIdFn>(env, kGetFieldId);
    if (!get_class || !get_field || !being_) {
        SetError(error, "required JNI per-hand aim lookup functions are unavailable");
        return false;
    }
    void* player_class = get_class(env, being_);
    if (!player_class || ClearException(env, error, "GetObjectClass(per-hand aim) failed")) {
        DeleteLocal(env, player_class);
        return false;
    }
    look_dir_for_hand_field_ =
        get_field(env, player_class, "m_avLookDirDevForHand", "[LVector;");
    aim_from_point_field_ =
        get_field(env, player_class, "m_avAimFromPoint", "[LVector;");
    const bool failed = ClearException(env, error, "per-hand aim field lookup failed");
    DeleteLocal(env, player_class);
    if (failed || !look_dir_for_hand_field_ || !aim_from_point_field_) {
        look_dir_for_hand_field_ = nullptr;
        aim_from_point_field_ = nullptr;
        return false;
    }
    return true;
}

bool JavaPlayerBridge::EnsureFireOriginAccess(void* env, std::string* error) noexcept {
    if (fire_origin_lookup_attempted_) {
        if (look_from_point_field_) return true;
        SetError(error, "player fire-origin path is unavailable");
        return false;
    }
    fire_origin_lookup_attempted_ = true;
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_field = EnvFunction<GetFieldIdFn>(env, kGetFieldId);
    if (!get_class || !get_field || !being_) {
        SetError(error, "required JNI fire-origin lookup functions are unavailable");
        return false;
    }
    void* player_class = get_class(env, being_);
    if (!player_class || ClearException(env, error, "GetObjectClass(fire origin) failed")) {
        DeleteLocal(env, player_class);
        return false;
    }
    // m_vLookFromPoint is declared on Being and inherited by ArmedPlayerBeing.
    // JNI GetFieldID resolves inherited instance fields for the runtime class.
    look_from_point_field_ =
        get_field(env, player_class, "m_vLookFromPoint", "LVector;");
    const bool failed = ClearException(env, error, "Being.m_vLookFromPoint lookup failed");
    DeleteLocal(env, player_class);
    if (failed || !look_from_point_field_) {
        look_from_point_field_ = nullptr;
        return false;
    }
    return true;
}

bool JavaPlayerBridge::EnsureVectorAccess(void* env, std::string* error) noexcept {
    if (vector_class_ && vector_constructor_ && vector_x_field_ && vector_y_field_ &&
        vector_z_field_) {
        return true;
    }
    const auto find_class = EnvFunction<FindClassFn>(env, kFindClass);
    const auto new_global = EnvFunction<NewGlobalRefFn>(env, kNewGlobalRef);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto get_field = EnvFunction<GetFieldIdFn>(env, kGetFieldId);
    if (!find_class || !new_global || !get_method || !get_field) {
        SetError(error, "required JNI Vector lookup functions are unavailable");
        return false;
    }

    void* local_class = find_class(env, "Vector");
    if (!local_class || ClearException(env, error, "FindClass(Vector) failed")) {
        DeleteLocal(env, local_class);
        return false;
    }
    void* global_class = new_global(env, local_class);
    DeleteLocal(env, local_class);
    if (!global_class || ClearException(env, error, "NewGlobalRef(Vector) failed")) return false;

    void* constructor = get_method(env, global_class, "<init>", "()V");
    void* x_field = get_field(env, global_class, "fX", "F");
    void* y_field = get_field(env, global_class, "fY", "F");
    void* z_field = get_field(env, global_class, "fZ", "F");
    const bool failed = ClearException(env, error, "Vector member lookup failed");
    if (failed || !constructor || !x_field || !y_field || !z_field) {
        if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
            delete_global(env, global_class);
        }
        return false;
    }
    if (vector_class_) {
        if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
            delete_global(env, vector_class_);
        }
    }
    vector_class_ = global_class;
    vector_constructor_ = constructor;
    vector_x_field_ = x_field;
    vector_y_field_ = y_field;
    vector_z_field_ = z_field;
    return true;
}

bool JavaPlayerBridge::EnsureBoneReadAccess(void* env, std::string* error) noexcept {
    if (bone_read_lookup_attempted_) {
        if (get_bone_joint_method_ && get_bone_direction_method_ &&
            get_bone_perpendicular_method_) {
            return EnsureVectorAccess(env, error);
        }
        SetError(error, "player bone-read path is unavailable");
        return false;
    }
    bone_read_lookup_attempted_ = true;
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    if (!get_class || !get_method) {
        SetError(error, "required JNI bone-read lookup functions are unavailable");
        return false;
    }
    void* player_class = get_class(env, being_);
    if (!player_class || ClearException(env, error, "GetObjectClass(bone reader) failed")) {
        DeleteLocal(env, player_class);
        return false;
    }
    get_bone_joint_method_ = get_method(env, player_class, "GetBoneJointPos", "(BLVector;)Z");
    get_bone_direction_method_ = get_method(env, player_class, "GetBoneDirVector", "(BLVector;)Z");
    get_bone_perpendicular_method_ =
        get_method(env, player_class, "GetBonePerpVector", "(BLVector;)Z");
    const bool failed = ClearException(env, error, "player bone-read method lookup failed");
    DeleteLocal(env, player_class);
    if (failed || !get_bone_joint_method_ || !get_bone_direction_method_ ||
        !get_bone_perpendicular_method_) {
        get_bone_joint_method_ = nullptr;
        get_bone_direction_method_ = nullptr;
        get_bone_perpendicular_method_ = nullptr;
        return false;
    }
    return EnsureVectorAccess(env, error);
}

bool JavaPlayerBridge::EnsureElementWorldReadAccess(void* env, std::string* error) noexcept {
    if (element_world_read_lookup_attempted_) {
        if (get_element_position_method_ && get_element_left_method_ && get_element_up_method_) {
            return EnsureVectorAccess(env, error);
        }
        SetError(error, "player element world-read path is unavailable");
        return false;
    }
    element_world_read_lookup_attempted_ = true;
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    if (!get_class || !get_method) {
        SetError(error, "required JNI element world-read lookup functions are unavailable");
        return false;
    }
    void* player_class = get_class(env, being_);
    if (!player_class || ClearException(env, error, "GetObjectClass(element reader) failed")) {
        DeleteLocal(env, player_class);
        return false;
    }
    get_element_position_method_ =
        get_method(env, player_class, "GetElementPos", "(ILVector;)Z");
    get_element_left_method_ =
        get_method(env, player_class, "GetElementLeftVector", "(ILVector;)Z");
    get_element_up_method_ =
        get_method(env, player_class, "GetElementUpVector", "(ILVector;)Z");
    const bool failed =
        ClearException(env, error, "player element world-read method lookup failed");
    DeleteLocal(env, player_class);
    if (failed || !get_element_position_method_ || !get_element_left_method_ ||
        !get_element_up_method_) {
        get_element_position_method_ = nullptr;
        get_element_left_method_ = nullptr;
        get_element_up_method_ = nullptr;
        return false;
    }
    return EnsureVectorAccess(env, error);
}

bool JavaPlayerBridge::EnsureElementWorldBasisAccess(void* env, std::string* error) noexcept {
    if (element_world_basis_lookup_attempted_) {
        if (set_element_world_basis_method_) return EnsureVectorAccess(env, error);
        SetError(error, "player element world-basis path is unavailable");
        return false;
    }
    element_world_basis_lookup_attempted_ = true;
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    if (!get_class || !get_method) {
        SetError(error, "required JNI element world-basis lookup functions are unavailable");
        return false;
    }
    void* player_class = get_class(env, being_);
    if (!player_class || ClearException(env, error, "GetObjectClass(element writer) failed")) {
        DeleteLocal(env, player_class);
        return false;
    }
    set_element_world_basis_method_ = get_method(
        env, player_class, "FromUpForwardPosElementWorld",
        "(ILVector;LVector;LVector;)V");
    const bool failed =
        ClearException(env, error, "player element world-basis method lookup failed");
    DeleteLocal(env, player_class);
    if (failed || !set_element_world_basis_method_) {
        set_element_world_basis_method_ = nullptr;
        return false;
    }
    return EnsureVectorAccess(env, error);
}

bool JavaPlayerBridge::EnsureElementRotationAccess(void* env, std::string* error) noexcept {
    if (element_rotation_lookup_attempted_) {
        if (rotate_element_with_children_method_) return EnsureVectorAccess(env, error);
        SetError(error, "player element hierarchy-rotation path is unavailable");
        return false;
    }
    element_rotation_lookup_attempted_ = true;
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    if (!get_class || !get_method) {
        SetError(error, "required JNI element rotation lookup functions are unavailable");
        return false;
    }
    void* player_class = get_class(env, being_);
    if (!player_class || ClearException(env, error, "GetObjectClass(element rotation) failed")) {
        DeleteLocal(env, player_class);
        return false;
    }
    rotate_element_with_children_method_ = get_method(
        env, player_class, "RotateElementWithChildren", "(ILVector;F)V");
    const bool failed = ClearException(env, error, "RotateElementWithChildren method lookup failed");
    DeleteLocal(env, player_class);
    if (failed || !rotate_element_with_children_method_) {
        rotate_element_with_children_method_ = nullptr;
        return false;
    }
    return EnsureVectorAccess(env, error);
}

bool JavaPlayerBridge::EnsureBoneRotationAccess(void* env, std::string* error) noexcept {
    if (bone_rotation_lookup_attempted_) {
        if (bone_rotate_method_) return EnsureVectorAccess(env, error);
        SetError(error, "player BoneRotate path is unavailable");
        return false;
    }
    bone_rotation_lookup_attempted_ = true;
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    if (!get_class || !get_method) {
        SetError(error, "required JNI BoneRotate lookup functions are unavailable");
        return false;
    }
    void* player_class = get_class(env, being_);
    if (!player_class || ClearException(env, error, "GetObjectClass(BoneRotate) failed")) {
        DeleteLocal(env, player_class);
        return false;
    }
    bone_rotate_method_ =
        get_method(env, player_class, "BoneRotate", "(BLVector;FZ)V");
    const bool failed = ClearException(env, error, "BoneRotate method lookup failed");
    DeleteLocal(env, player_class);
    if (failed || !bone_rotate_method_) {
        bone_rotate_method_ = nullptr;
        return false;
    }
    return EnsureVectorAccess(env, error);
}

bool JavaPlayerBridge::ReadVector(
    void* env, void* vector, JavaPlayerPosition& value, std::string* error) noexcept {
    value = {};
    if (!vector || !EnsureVectorAccess(env, error)) return false;
    const auto get_float = EnvFunction<GetFloatFieldFn>(env, kGetFloatField);
    if (!get_float) {
        SetError(error, "JNI GetFloatField is unavailable");
        return false;
    }
    value.x = get_float(env, vector, vector_x_field_);
    value.y = get_float(env, vector, vector_y_field_);
    value.z = get_float(env, vector, vector_z_field_);
    return !ClearException(env, error, "Vector read failed") &&
        std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool JavaPlayerBridge::WriteVector(
    void* env, void* vector, const JavaPlayerPosition& value, std::string* error) noexcept {
    if (!vector || !EnsureVectorAccess(env, error) ||
        !std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
        SetError(error, "Vector write value is invalid");
        return false;
    }
    const auto set_float = EnvFunction<SetFloatFieldFn>(env, kSetFloatField);
    if (!set_float) {
        SetError(error, "JNI SetFloatField is unavailable");
        return false;
    }
    set_float(env, vector, vector_x_field_, value.x);
    set_float(env, vector, vector_y_field_, value.y);
    set_float(env, vector, vector_z_field_, value.z);
    return !ClearException(env, error, "Vector write failed");
}

bool JavaPlayerBridge::EnsureBodyRotationAccess(void* env, std::string* error) noexcept {
    if (body_rotation_lookup_attempted_) {
        const bool available = update_body_rotation_method_ && current_head_vertical_field_ &&
            current_head_horizontal_field_ && current_spine_vertical_field_ &&
            current_spine_horizontal_field_;
        if (!available) SetError(error, "ArmedPlayerBeing body-rotation path is unavailable");
        return available;
    }
    body_rotation_lookup_attempted_ = true;

    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    const auto get_field = EnvFunction<GetFieldIdFn>(env, kGetFieldId);
    if (!get_class || !get_method || !get_field) {
        SetError(error, "required JNI body-rotation lookup functions are unavailable");
        return false;
    }

    void* player_class = get_class(env, being_);
    if (!player_class || ClearException(env, error, "GetObjectClass(body player) failed")) {
        DeleteLocal(env, player_class);
        return false;
    }

    update_body_rotation_method_ =
        get_method(env, player_class, "UpdateBodyRotation", "(FZZ)V");
    current_head_vertical_field_ =
        get_field(env, player_class, "m_fCurrentHeadVertRotAngle", "F");
    current_head_horizontal_field_ =
        get_field(env, player_class, "m_fCurrentHeadHorzRotAngle", "F");
    current_spine_vertical_field_ =
        get_field(env, player_class, "m_fCurrentSpineVertRotAngle", "F");
    current_spine_horizontal_field_ =
        get_field(env, player_class, "m_fCurrentSpineHorzRotAngle", "F");
    const bool failed = ClearException(env, error, "ArmedPlayerBeing body-rotation lookup failed");
    DeleteLocal(env, player_class);
    if (failed || !update_body_rotation_method_ || !current_head_vertical_field_ ||
        !current_head_horizontal_field_ || !current_spine_vertical_field_ ||
        !current_spine_horizontal_field_) {
        update_body_rotation_method_ = nullptr;
        current_head_vertical_field_ = nullptr;
        current_head_horizontal_field_ = nullptr;
        current_spine_vertical_field_ = nullptr;
        current_spine_horizontal_field_ = nullptr;
        SetError(error, "ArmedPlayerBeing body-rotation lookup was incomplete");
        return false;
    }
    return true;
}

bool JavaPlayerBridge::Refresh(std::string* error) noexcept {
    void* env = Environment(error);
    if (!env || !EnsureSession(env, error)) return false;

    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_field_id = EnvFunction<GetFieldIdFn>(env, kGetFieldId);
    const auto get_object_field = EnvFunction<GetObjectFieldFn>(env, kGetObjectField);
    const auto is_same = EnvFunction<IsSameObjectFn>(env, kIsSameObject);
    const auto call_int = EnvFunction<CallIntMethodAFn>(env, kCallIntMethodA);
    const auto call_object = EnvFunction<CallObjectMethodAFn>(env, kCallObjectMethodA);
    if (!get_static_object || !get_class || !get_field_id || !get_object_field || !is_same ||
        !call_int || !call_object) {
        SetError(error, "required JNI player discovery functions are unavailable");
        return false;
    }

    void* local_player = get_static_object(env, session_class_, session_local_player_field_);
    if (ClearException(env, error, "Session.sm_LocalPlayer read failed")) {
        DeleteLocal(env, local_player);
        ClearBeing(env);
        return false;
    }
    single_player_fallback_used_ = false;
    campaign_module_fallback_used_ = false;
    if (!local_player) {
        void* players = get_static_object(env, session_class_, session_players_field_);
        if (ClearException(env, error, "Session.sm_Players read failed") || !players) {
            DeleteLocal(env, players);
            ClearBeing(env);
            SetError(error, "local player is unavailable and the session player list is unavailable");
            return false;
        }

        const std::int32_t player_count = call_int(env, players, vector_size_method_, nullptr);
        if (ClearException(env, error, "Session.sm_Players size read failed")) {
            DeleteLocal(env, players);
            ClearBeing(env);
            return false;
        }
        if (player_count == 0) {
            DeleteLocal(env, players);
            if (TryResolveCampaignBeing(env, error)) return true;
            ClearBeing(env);
            return false;
        }
        if (player_count != 1) {
            DeleteLocal(env, players);
            ClearBeing(env);
            SetError(error, "local player is unavailable and the session player list is ambiguous");
            return false;
        }

        JValue args[1]{};
        args[0].i = 0;
        local_player = call_object(env, players, vector_get_method_, args);
        DeleteLocal(env, players);
        if (ClearException(env, error, "Session.sm_Players[0] read failed") || !local_player) {
            DeleteLocal(env, local_player);
            ClearBeing(env);
            SetError(error, "single-player session entry is unavailable");
            return false;
        }
        single_player_fallback_used_ = true;
    }

    if (!net_player_being_field_) {
        void* player_class = get_class(env, local_player);
        if (!player_class || ClearException(env, error, "GetObjectClass(local player) failed")) {
            DeleteLocal(env, player_class);
            DeleteLocal(env, local_player);
            return false;
        }
        net_player_being_field_ = get_field_id(env, player_class, "m_Being", "LBeing;");
        const bool failed = ClearException(env, error, "NetPlayer.m_Being lookup failed");
        DeleteLocal(env, player_class);
        if (failed || !net_player_being_field_) {
            DeleteLocal(env, local_player);
            net_player_being_field_ = nullptr;
            return false;
        }
    }

    void* local_being = get_object_field(env, local_player, net_player_being_field_);
    DeleteLocal(env, local_player);
    if (ClearException(env, error, "NetPlayer.m_Being read failed") || !local_being) {
        DeleteLocal(env, local_being);
        ClearBeing(env);
        SetError(error, "local player being is unavailable");
        return false;
    }

    const bool same = being_ && is_same(env, being_, local_being) != 0;
    if (!same && !ResolveBeingMethods(env, local_being, error)) {
        DeleteLocal(env, local_being);
        return false;
    }
    DeleteLocal(env, local_being);
    return being_ != nullptr;
}

bool JavaPlayerBridge::TryGetMeshElement(
    const std::int8_t bone, int& element, std::string* error) noexcept {
    element = -1;
    void* env = Environment(error);
    if (!env || !being_ || !get_mesh_element_method_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    const auto call = EnvFunction<CallIntMethodAFn>(env, kCallIntMethodA);
    if (!call) {
        SetError(error, "JNI CallIntMethodA is unavailable");
        return false;
    }
    JValue args[1]{};
    args[0].b = bone;
    const std::int32_t result = call(env, being_, get_mesh_element_method_, args);
    if (ClearException(env, error, "GetMeshElemFromBoneID call failed") || result < 0) {
        SetError(error, "bone does not map to a mesh element");
        return false;
    }
    element = result;
    return true;
}

bool JavaPlayerBridge::TryGetMeshElementByName(
    const char* name, int& element, std::string* error) noexcept {
    element = -1;
    if (!name || !*name) {
        SetError(error, "mesh element name is empty");
        return false;
    }
    void* env = Environment(error);
    if (!env || !being_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    if (!EnsureElementVisibilityAccess(env, error)) return false;
    const auto new_string = EnvFunction<NewStringUtfFn>(env, kNewStringUtf);
    const auto call_int = EnvFunction<CallIntMethodAFn>(env, kCallIntMethodA);
    if (!new_string || !call_int) {
        SetError(error, "required JNI mesh-element name lookup functions are unavailable");
        return false;
    }
    void* java_name = new_string(env, name);
    if (!java_name || ClearException(env, error, "NewStringUTF(element name) failed")) {
        DeleteLocal(env, java_name);
        return false;
    }
    JValue args[1]{};
    args[0].l = java_name;
    const std::int32_t result = call_int(env, being_, get_element_id_method_, args);
    const bool failed = ClearException(env, error, "GetElementID call failed");
    DeleteLocal(env, java_name);
    if (failed || result < 0) {
        if (!failed) SetError(error, "named mesh element is unavailable");
        return false;
    }
    element = result;
    return true;
}

bool JavaPlayerBridge::TryGetMeshElementHidden(
    const int element, bool& hidden, std::string* error) noexcept {
    hidden = false;
    if (element < 0) {
        SetError(error, "mesh element id is invalid");
        return false;
    }
    void* env = Environment(error);
    if (!env || !being_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    if (!EnsureElementVisibilityAccess(env, error)) return false;
    const auto call_boolean = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    if (!call_boolean) {
        SetError(error, "JNI CallBooleanMethodA is unavailable");
        return false;
    }
    JValue args[1]{};
    args[0].i = element;
    hidden = call_boolean(env, being_, is_element_hidden_method_, args) != 0;
    return !ClearException(env, error, "IsElementHidden call failed");
}

bool JavaPlayerBridge::TrySetMeshElementHidden(
    const int element, const bool hidden, std::string* error) noexcept {
    if (element < 0) {
        SetError(error, "mesh element id is invalid");
        return false;
    }
    void* env = Environment(error);
    if (!env || !being_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    if (!EnsureElementVisibilityAccess(env, error)) return false;
    const auto call_boolean = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    if (!call_boolean) {
        SetError(error, "JNI CallBooleanMethodA is unavailable");
        return false;
    }
    JValue args[1]{};
    args[0].i = element;
    const bool changed = call_boolean(
        env,
        being_,
        hidden ? hide_element_method_ : unhide_element_method_,
        args) != 0;
    if (ClearException(env, error, hidden ? "HideElement call failed" : "UnhideElement call failed")) {
        return false;
    }
    if (!changed) {
        bool current = false;
        if (!TryGetMeshElementHidden(element, current, error) || current != hidden) {
            SetError(error, hidden ? "HideElement did not hide the element" : "UnhideElement did not show the element");
            return false;
        }
    }
    return true;
}

bool JavaPlayerBridge::TryGetBoneJointPosition(
    const std::int8_t bone, JavaPlayerPosition& position, std::string* error) noexcept {
    position = {};
    void* env = Environment(error);
    if (!env || !being_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    if (!EnsureBoneReadAccess(env, error)) return false;

    const auto new_object = EnvFunction<NewObjectAFn>(env, kNewObjectA);
    const auto call_boolean = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    if (!new_object || !call_boolean) {
        SetError(error, "required JNI bone-read functions are unavailable");
        return false;
    }

    void* vector = new_object(env, vector_class_, vector_constructor_, nullptr);
    if (!vector || ClearException(env, error, "Vector construction for bone joint failed")) {
        DeleteLocal(env, vector);
        return false;
    }

    JValue args[2]{};
    args[0].b = bone;
    args[1].l = vector;
    const bool available = call_boolean(env, being_, get_bone_joint_method_, args) != 0;
    if (ClearException(env, error, "GetBoneJointPos call failed") || !available) {
        DeleteLocal(env, vector);
        if (!available) SetError(error, "bone joint position is unavailable");
        return false;
    }

    const bool read = ReadVector(env, vector, position, error);
    DeleteLocal(env, vector);
    return read;
}

bool JavaPlayerBridge::TryGetBoneDirection(
    const std::int8_t bone, JavaPlayerPosition& direction, std::string* error) noexcept {
    direction = {};
    void* env = Environment(error);
    if (!env || !being_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    if (!EnsureBoneReadAccess(env, error)) return false;

    const auto new_object = EnvFunction<NewObjectAFn>(env, kNewObjectA);
    const auto call_boolean = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    if (!new_object || !call_boolean) {
        SetError(error, "required JNI bone-read functions are unavailable");
        return false;
    }

    void* vector = new_object(env, vector_class_, vector_constructor_, nullptr);
    if (!vector || ClearException(env, error, "Vector construction for bone direction failed")) {
        DeleteLocal(env, vector);
        return false;
    }

    JValue args[2]{};
    args[0].b = bone;
    args[1].l = vector;
    const bool available = call_boolean(env, being_, get_bone_direction_method_, args) != 0;
    if (ClearException(env, error, "GetBoneDirVector call failed") || !available) {
        DeleteLocal(env, vector);
        if (!available) SetError(error, "bone direction is unavailable");
        return false;
    }

    const bool read = ReadVector(env, vector, direction, error);
    DeleteLocal(env, vector);
    return read;
}

bool JavaPlayerBridge::TryGetBonePerpendicular(
    const std::int8_t bone, JavaPlayerPosition& direction, std::string* error) noexcept {
    direction = {};
    void* env = Environment(error);
    if (!env || !being_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    if (!EnsureBoneReadAccess(env, error)) return false;

    const auto new_object = EnvFunction<NewObjectAFn>(env, kNewObjectA);
    const auto call_boolean = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    if (!new_object || !call_boolean) {
        SetError(error, "required JNI bone-read functions are unavailable");
        return false;
    }
    void* vector = new_object(env, vector_class_, vector_constructor_, nullptr);
    if (!vector || ClearException(env, error, "Vector construction for bone perpendicular failed")) {
        DeleteLocal(env, vector);
        return false;
    }
    JValue args[2]{};
    args[0].b = bone;
    args[1].l = vector;
    const bool available = call_boolean(env, being_, get_bone_perpendicular_method_, args) != 0;
    if (ClearException(env, error, "GetBonePerpVector call failed") || !available) {
        DeleteLocal(env, vector);
        if (!available) SetError(error, "bone perpendicular is unavailable");
        return false;
    }
    const bool read = ReadVector(env, vector, direction, error);
    DeleteLocal(env, vector);
    return read;
}

bool JavaPlayerBridge::TryGetElementWorldBasis(
    const int element,
    JavaPlayerPosition& position,
    JavaPlayerPosition& up,
    JavaPlayerPosition& forward,
    std::string* error) noexcept {
    position = {};
    up = {};
    forward = {};
    if (element < 0) {
        SetError(error, "mesh element is invalid");
        return false;
    }
    void* env = Environment(error);
    if (!env || !being_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    if (!EnsureElementWorldReadAccess(env, error)) return false;

    const auto new_object = EnvFunction<NewObjectAFn>(env, kNewObjectA);
    const auto call_boolean = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    if (!new_object || !call_boolean) {
        SetError(error, "required JNI element world-read functions are unavailable");
        return false;
    }
    void* vector = new_object(env, vector_class_, vector_constructor_, nullptr);
    if (!vector || ClearException(env, error, "Vector construction for element frame failed")) {
        DeleteLocal(env, vector);
        return false;
    }

    const auto read_element_vector = [&](
                                         void* method,
                                         const char* call_error,
                                         const char* unavailable_error,
                                         JavaPlayerPosition& value) noexcept {
        JValue args[2]{};
        args[0].i = element;
        args[1].l = vector;
        const bool available = call_boolean(env, being_, method, args) != 0;
        if (ClearException(env, error, call_error) || !available) {
            if (!available) SetError(error, unavailable_error);
            return false;
        }
        return ReadVector(env, vector, value, error);
    };

    JavaPlayerPosition element_x_axis{};
    const bool read =
        read_element_vector(
            get_element_position_method_, "GetElementPos call failed",
            "element world position is unavailable", position) &&
        read_element_vector(
            get_element_left_method_, "GetElementLeftVector call failed",
            "element world X axis is unavailable", element_x_axis) &&
        read_element_vector(
            get_element_up_method_, "GetElementUpVector call failed",
            "element world up axis is unavailable", up);
    DeleteLocal(env, vector);
    if (!read) return false;

    // Exact ChromeEngine3 disassembly shows GetElementLeftVector transforms local
    // +X by the element world matrix. FromUpForwardPosElementWorld stores that same
    // +X row as up x forward, so rebuild forward from the paired X/up getters instead
    // of using GetElementForwardVector (whose shipped native handler writes the
    // vector but incorrectly returns false).
    const auto length = [](const JavaPlayerPosition value) noexcept {
        return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    };
    const auto scale = [](const JavaPlayerPosition value, const float factor) noexcept {
        return JavaPlayerPosition{value.x * factor, value.y * factor, value.z * factor};
    };
    const auto dot = [](const JavaPlayerPosition left, const JavaPlayerPosition right) noexcept {
        return left.x * right.x + left.y * right.y + left.z * right.z;
    };
    const auto cross = [](const JavaPlayerPosition left, const JavaPlayerPosition right) noexcept {
        return JavaPlayerPosition{
            left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x,
        };
    };

    const float up_length = length(up);
    if (!std::isfinite(up_length) || up_length <= 1.0e-5F) {
        SetError(error, "element world up axis is degenerate");
        return false;
    }
    up = scale(up, 1.0F / up_length);
    element_x_axis = {
        element_x_axis.x - up.x * dot(element_x_axis, up),
        element_x_axis.y - up.y * dot(element_x_axis, up),
        element_x_axis.z - up.z * dot(element_x_axis, up),
    };
    const float x_length = length(element_x_axis);
    if (!std::isfinite(x_length) || x_length <= 1.0e-5F) {
        SetError(error, "element world X axis is degenerate");
        return false;
    }
    element_x_axis = scale(element_x_axis, 1.0F / x_length);
    forward = cross(element_x_axis, up);
    const float forward_length = length(forward);
    if (!std::isfinite(forward_length) || forward_length <= 1.0e-5F) {
        SetError(error, "element world forward axis is degenerate");
        return false;
    }
    forward = scale(forward, 1.0F / forward_length);
    return true;
}

bool JavaPlayerBridge::TrySetElementWorldBasis(
    const int element,
    const JavaPlayerPosition& up,
    const JavaPlayerPosition& forward,
    const JavaPlayerPosition& position,
    std::string* error) noexcept {
    if (element < 0) {
        SetError(error, "mesh element is invalid");
        return false;
    }
    void* env = Environment(error);
    if (!env || !being_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    if (!EnsureElementWorldBasisAccess(env, error)) return false;
    const auto new_object = EnvFunction<NewObjectAFn>(env, kNewObjectA);
    const auto call_void = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    if (!new_object || !call_void) {
        SetError(error, "required JNI element world-basis functions are unavailable");
        return false;
    }

    void* up_vector = new_object(env, vector_class_, vector_constructor_, nullptr);
    void* forward_vector = new_object(env, vector_class_, vector_constructor_, nullptr);
    void* position_vector = new_object(env, vector_class_, vector_constructor_, nullptr);
    if (!up_vector || !forward_vector || !position_vector ||
        ClearException(env, error, "Vector construction for element world basis failed")) {
        DeleteLocal(env, up_vector);
        DeleteLocal(env, forward_vector);
        DeleteLocal(env, position_vector);
        return false;
    }
    if (!WriteVector(env, up_vector, up, error) ||
        !WriteVector(env, forward_vector, forward, error) ||
        !WriteVector(env, position_vector, position, error)) {
        DeleteLocal(env, up_vector);
        DeleteLocal(env, forward_vector);
        DeleteLocal(env, position_vector);
        return false;
    }

    JValue args[4]{};
    args[0].i = element;
    args[1].l = up_vector;
    args[2].l = forward_vector;
    args[3].l = position_vector;
    call_void(env, being_, set_element_world_basis_method_, args);
    const bool failed = ClearException(env, error, "FromUpForwardPosElementWorld call failed");
    DeleteLocal(env, up_vector);
    DeleteLocal(env, forward_vector);
    DeleteLocal(env, position_vector);
    return !failed;
}

bool JavaPlayerBridge::TryRotateElementWithChildren(
    const int element,
    const JavaPlayerPosition& axis,
    const float angle_degrees,
    std::string* error) noexcept {
    if (element < 0 || !std::isfinite(axis.x) || !std::isfinite(axis.y) ||
        !std::isfinite(axis.z) || !std::isfinite(angle_degrees)) {
        SetError(error, "element rotation input is invalid");
        return false;
    }
    const float axis_length =
        std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (axis_length <= 1.0e-5F || std::fabs(angle_degrees) > 180.0001F) {
        SetError(error, "element rotation axis/angle is degenerate");
        return false;
    }

    void* env = Environment(error);
    if (!env || !being_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    if (!EnsureElementRotationAccess(env, error)) return false;
    const auto new_object = EnvFunction<NewObjectAFn>(env, kNewObjectA);
    const auto call_void = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    if (!new_object || !call_void) {
        SetError(error, "required JNI RotateElementWithChildren functions are unavailable");
        return false;
    }

    void* axis_vector = new_object(env, vector_class_, vector_constructor_, nullptr);
    if (!axis_vector ||
        ClearException(env, error, "Vector construction for RotateElementWithChildren failed")) {
        DeleteLocal(env, axis_vector);
        return false;
    }
    const JavaPlayerPosition normalized_axis{
        axis.x / axis_length,
        axis.y / axis_length,
        axis.z / axis_length,
    };
    if (!WriteVector(env, axis_vector, normalized_axis, error)) {
        DeleteLocal(env, axis_vector);
        return false;
    }

    JValue args[3]{};
    args[0].i = element;
    args[1].l = axis_vector;
    args[2].f = angle_degrees;
    call_void(env, being_, rotate_element_with_children_method_, args);
    const bool failed = ClearException(env, error, "RotateElementWithChildren call failed");
    DeleteLocal(env, axis_vector);
    return !failed;
}

bool JavaPlayerBridge::TryRotateBone(
    const std::int8_t bone,
    const JavaPlayerPosition& axis,
    const float angle_degrees,
    const bool update_hierarchy,
    std::string* error) noexcept {
    if (bone < 0 || !std::isfinite(axis.x) || !std::isfinite(axis.y) ||
        !std::isfinite(axis.z) || !std::isfinite(angle_degrees)) {
        SetError(error, "bone rotation input is invalid");
        return false;
    }
    const float axis_length =
        std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (axis_length <= 1.0e-5F || std::fabs(angle_degrees) > 180.0001F) {
        SetError(error, "bone rotation axis/angle is degenerate");
        return false;
    }

    void* env = Environment(error);
    if (!env || !being_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    if (!EnsureBoneRotationAccess(env, error)) return false;
    const auto new_object = EnvFunction<NewObjectAFn>(env, kNewObjectA);
    const auto call_void = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    if (!new_object || !call_void) {
        SetError(error, "required JNI BoneRotate functions are unavailable");
        return false;
    }

    void* axis_vector = new_object(env, vector_class_, vector_constructor_, nullptr);
    if (!axis_vector || ClearException(env, error, "Vector construction for BoneRotate failed")) {
        DeleteLocal(env, axis_vector);
        return false;
    }
    const JavaPlayerPosition normalized_axis{
        axis.x / axis_length,
        axis.y / axis_length,
        axis.z / axis_length,
    };
    if (!WriteVector(env, axis_vector, normalized_axis, error)) {
        DeleteLocal(env, axis_vector);
        return false;
    }

    JValue args[4]{};
    args[0].b = bone;
    args[1].l = axis_vector;
    args[2].f = angle_degrees;
    args[3].z = update_hierarchy ? 1 : 0;
    call_void(env, being_, bone_rotate_method_, args);
    const bool failed = ClearException(env, error, "BoneRotate call failed");
    DeleteLocal(env, axis_vector);
    return !failed;
}

bool JavaPlayerBridge::TryGetPosition(
    JavaPlayerPosition& position, std::string* error) noexcept {
    position = {};
    void* env = Environment(error);
    if (!env || !being_ || !get_position_vector_method_) {
        SetError(error, "player being is not resolved");
        return false;
    }

    const auto call = EnvFunction<CallObjectMethodAFn>(env, kCallObjectMethodA);
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_field_id = EnvFunction<GetFieldIdFn>(env, kGetFieldId);
    const auto get_float = EnvFunction<GetFloatFieldFn>(env, kGetFloatField);
    if (!call || !get_class || !get_field_id || !get_float) {
        SetError(error, "required JNI position functions are unavailable");
        return false;
    }

    void* vector = call(env, being_, get_position_vector_method_, nullptr);
    if (!vector || ClearException(env, error, "GetPositionVectorVolatile call failed")) {
        DeleteLocal(env, vector);
        return false;
    }

    if (!vector_x_field_ || !vector_y_field_ || !vector_z_field_) {
        void* vector_class = get_class(env, vector);
        if (!vector_class || ClearException(env, error, "GetObjectClass(Vector) failed")) {
            DeleteLocal(env, vector_class);
            DeleteLocal(env, vector);
            return false;
        }
        vector_x_field_ = get_field_id(env, vector_class, "fX", "F");
        vector_y_field_ = get_field_id(env, vector_class, "fY", "F");
        vector_z_field_ = get_field_id(env, vector_class, "fZ", "F");
        const bool failed = ClearException(env, error, "Vector fX/fY/fZ lookup failed");
        DeleteLocal(env, vector_class);
        if (failed || !vector_x_field_ || !vector_y_field_ || !vector_z_field_) {
            DeleteLocal(env, vector);
            return false;
        }
    }

    position.x = get_float(env, vector, vector_x_field_);
    position.y = get_float(env, vector, vector_y_field_);
    position.z = get_float(env, vector, vector_z_field_);
    const bool failed = ClearException(env, error, "Vector position read failed");
    DeleteLocal(env, vector);
    return !failed;
}

bool JavaPlayerBridge::TryObserveMovement(
    CoJMovementObservation& observation,
    std::string* error) noexcept {
    observation = {};
    if (!TryGetPosition(observation.position, error)) return false;

    void* env = Environment(error);
    if (!env || !being_ || !EnsureMovementObservationAccess(env, error)) return false;
    const auto call_float = EnvFunction<CallFloatMethodAFn>(env, kCallFloatMethodA);
    const auto call_bool = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    const auto call_int = EnvFunction<CallIntMethodAFn>(env, kCallIntMethodA);
    const auto call_void = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    const auto new_object = EnvFunction<NewObjectAFn>(env, kNewObjectA);
    if (!call_float || !call_bool || !call_int || !call_void || !new_object) {
        SetError(error, "required JNI movement observation functions are unavailable");
        return false;
    }

    void* wanted = new_object(env, vector_class_, vector_constructor_, nullptr);
    if (!wanted || ClearException(env, error, "Vector allocation for movement failed")) {
        DeleteLocal(env, wanted);
        return false;
    }
    JValue wanted_arg{};
    wanted_arg.l = wanted;
    call_void(env, being_, get_wanted_local_speed_method_, &wanted_arg);
    if (ClearException(env, error, "ODEWalk_GetWantedLocalSpeed call failed") ||
        !ReadVector(env, wanted, observation.wanted_local_speed, error)) {
        DeleteLocal(env, wanted);
        return false;
    }
    DeleteLocal(env, wanted);

    observation.game_time = call_float(env, being_, get_time_method_, nullptr);
    observation.game_time_delta = call_float(env, being_, get_time_delta_method_, nullptr);
    observation.forward_speed = call_float(env, being_, get_forward_speed_method_, nullptr);
    observation.side_speed = call_float(env, being_, get_side_speed_method_, nullptr);
    observation.current_vertical_speed =
        call_float(env, being_, get_current_vertical_speed_method_, nullptr);
    const float property_jump_height =
        call_float(env, being_, get_jump_height_method_, nullptr);
    observation.stair_height = call_float(env, being_, get_stair_height_method_, nullptr);
    observation.jump_height = property_jump_height - observation.stair_height;
    observation.speed_state = call_int(env, being_, get_speed_state_method_, nullptr);
    observation.ode_walk_state = call_int(env, being_, get_ode_walk_state_method_, nullptr);
    observation.run = call_bool(env, being_, get_run_method_, nullptr) != 0;
    observation.can_run = call_bool(env, being_, can_run_method_, nullptr) != 0;
    observation.can_jump = call_bool(env, being_, can_jump_method_, nullptr) != 0;
    observation.jumping = call_bool(env, being_, is_jumping_method_, nullptr) != 0;
    if (ClearException(env, error, "movement observation call failed")) return false;

    const float values[]{
        observation.position.x,
        observation.position.y,
        observation.position.z,
        observation.wanted_local_speed.x,
        observation.wanted_local_speed.y,
        observation.wanted_local_speed.z,
        observation.game_time,
        observation.game_time_delta,
        observation.forward_speed,
        observation.side_speed,
        observation.current_vertical_speed,
        observation.jump_height,
        observation.stair_height,
    };
    if (!std::all_of(std::begin(values), std::end(values), [](const float value) {
            return std::isfinite(value);
        })) {
        SetError(error, "movement observation contained non-finite values");
        return false;
    }
    if (error) error->clear();
    return true;
}

bool JavaPlayerBridge::TryGetMovementClock(
    float& game_time,
    float& game_time_delta,
    std::string* error) noexcept {
    game_time = 0.0F;
    game_time_delta = 0.0F;
    void* env = Environment(error);
    if (!env || !being_ || !EnsureMovementObservationAccess(env, error)) return false;
    const auto call_float = EnvFunction<CallFloatMethodAFn>(env, kCallFloatMethodA);
    if (!call_float) {
        SetError(error, "JNI CallFloatMethodA is unavailable");
        return false;
    }
    game_time = call_float(env, being_, get_time_method_, nullptr);
    game_time_delta = call_float(env, being_, get_time_delta_method_, nullptr);
    if (ClearException(env, error, "movement clock call failed") ||
        !std::isfinite(game_time) || !std::isfinite(game_time_delta)) {
        SetError(error, "movement clock contained non-finite values");
        return false;
    }
    if (error) error->clear();
    return true;
}

bool JavaPlayerBridge::TrySetPosition(
    const JavaPlayerPosition& position, std::string* error) noexcept {
    void* env = Environment(error);
    if (!env || !being_ || !set_position_method_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    const auto call = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    if (!call) {
        SetError(error, "JNI CallVoidMethodA is unavailable");
        return false;
    }
    JValue args[3]{};
    args[0].f = position.x;
    args[1].f = position.y;
    args[2].f = position.z;
    call(env, being_, set_position_method_, args);
    return !ClearException(env, error, "SetPosition(FFF) call failed");
}

bool JavaPlayerBridge::TryApplyUpperBodyTracking(
    const float head_horizontal_offset_degrees,
    const float spine_horizontal_offset_degrees,
    const float head_vertical_offset_degrees,
    std::string* error) noexcept {
    if (!std::isfinite(head_horizontal_offset_degrees) ||
        !std::isfinite(spine_horizontal_offset_degrees) ||
        !std::isfinite(head_vertical_offset_degrees)) {
        SetError(error, "upper-body tracking offsets are not finite");
        return false;
    }

    void* env = Environment(error);
    if (!env || !being_) {
        SetError(error, "player being is not resolved");
        return false;
    }
    if (!EnsureBodyRotationAccess(env, error)) return false;

    const auto get_float = EnvFunction<GetFloatFieldFn>(env, kGetFloatField);
    const auto set_float = EnvFunction<SetFloatFieldFn>(env, kSetFloatField);
    const auto call_void = EnvFunction<CallVoidMethodAFn>(env, kCallVoidMethodA);
    if (!get_float || !set_float || !call_void) {
        SetError(error, "required JNI body-rotation functions are unavailable");
        return false;
    }

    const float original_head_vertical =
        get_float(env, being_, current_head_vertical_field_);
    const float original_head_horizontal =
        get_float(env, being_, current_head_horizontal_field_);
    const float original_spine_vertical =
        get_float(env, being_, current_spine_vertical_field_);
    const float original_spine_horizontal =
        get_float(env, being_, current_spine_horizontal_field_);
    if (ClearException(env, error, "ArmedPlayerBeing rotation-state read failed") ||
        !std::isfinite(original_head_vertical) || !std::isfinite(original_head_horizontal) ||
        !std::isfinite(original_spine_vertical) || !std::isfinite(original_spine_horizontal)) {
        SetError(error, "ArmedPlayerBeing rotation state is invalid");
        return false;
    }

    set_float(
        env, being_, current_head_vertical_field_,
        original_head_vertical + head_vertical_offset_degrees);
    set_float(
        env, being_, current_head_horizontal_field_,
        original_head_horizontal + head_horizontal_offset_degrees);
    set_float(
        env, being_, current_spine_horizontal_field_,
        original_spine_horizontal + spine_horizontal_offset_degrees);
    if (ClearException(env, error, "ArmedPlayerBeing VR rotation-state write failed")) {
        set_float(env, being_, current_head_vertical_field_, original_head_vertical);
        set_float(env, being_, current_head_horizontal_field_, original_head_horizontal);
        set_float(env, being_, current_spine_vertical_field_, original_spine_vertical);
        set_float(env, being_, current_spine_horizontal_field_, original_spine_horizontal);
        (void)ClearException(env, nullptr, "ArmedPlayerBeing rotation-state rollback failed");
        return false;
    }

    // UpdateBodyRotation(dt, forceRemoteMode, recomputeAngles). Passing the
    // final flag as false is the key contract recovered from shipped bytecode:
    // it rebuilds the animated element chain from the current fields without
    // recomputing them from mouse/weapon look. The fields are restored
    // immediately afterwards, so the VR overlay does not become game state.
    JValue args[3]{};
    args[0].f = 0.0F;
    args[1].z = 0;
    args[2].z = 0;
    call_void(env, being_, update_body_rotation_method_, args);
    const bool update_failed =
        ClearException(env, error, "ArmedPlayerBeing.UpdateBodyRotation VR overlay failed");

    set_float(env, being_, current_head_vertical_field_, original_head_vertical);
    set_float(env, being_, current_head_horizontal_field_, original_head_horizontal);
    set_float(env, being_, current_spine_vertical_field_, original_spine_vertical);
    set_float(env, being_, current_spine_horizontal_field_, original_spine_horizontal);
    const bool restore_failed =
        ClearException(env, error, "ArmedPlayerBeing rotation-state restore failed");
    return !update_failed && !restore_failed;
}

} // namespace cojvr::games::call_of_juarez
