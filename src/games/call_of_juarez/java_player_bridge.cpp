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
constexpr std::size_t kCallVoidMethodA = 63;
constexpr std::size_t kGetFieldId = 94;
constexpr std::size_t kGetObjectField = 95;
constexpr std::size_t kGetIntField = 100;
constexpr std::size_t kGetFloatField = 102;
constexpr std::size_t kSetFloatField = 111;
constexpr std::size_t kGetStaticFieldId = 144;
constexpr std::size_t kGetStaticObjectField = 145;
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
using CallVoidMethodAFn = void(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using GetFieldIdFn = void*(COJVR_JNICALL*)(void*, void*, const char*, const char*);
using GetObjectFieldFn = void*(COJVR_JNICALL*)(void*, void*, void*);
using GetIntFieldFn = std::int32_t(COJVR_JNICALL*)(void*, void*, void*);
using GetFloatFieldFn = float(COJVR_JNICALL*)(void*, void*, void*);
using SetFloatFieldFn = void(COJVR_JNICALL*)(void*, void*, void*, float);
using GetStaticFieldIdFn = void*(COJVR_JNICALL*)(void*, void*, const char*, const char*);
using GetStaticObjectFieldFn = void*(COJVR_JNICALL*)(void*, void*, void*);
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

std::array<CoJGameplayActionValue, 16> BuildCoJGameplayActionValues(
    const cojvr::runtime::GameplayInputState& state) noexcept {
    const auto normalize_axis = [](const float value) noexcept {
        if (!std::isfinite(value) || std::fabs(value) < 0.15F) return 0.0F;
        return std::clamp(value, -1.0F, 1.0F);
    };
    const auto positive = [](const float value) noexcept {
        return std::max(value, 0.0F);
    };
    const float move_x = state.active ? normalize_axis(state.move.x) : 0.0F;
    const float move_y = state.active ? normalize_axis(state.move.y) : 0.0F;
    const float turn_x = state.active ? normalize_axis(state.turn.x) : 0.0F;
    return {{
        {2, positive(-turn_x)},
        {3, positive(turn_x)},
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
    input_digital_class_ = nullptr;
    input_analog_class_ = nullptr;
    digital_device_field_ = nullptr;
    digital_button_field_ = nullptr;
    digital_translate_method_ = nullptr;
    analog_device_field_ = nullptr;
    analog_axis_field_ = nullptr;
    analog_axis_sign_field_ = nullptr;
    analog_translate_method_ = nullptr;
    bone_read_lookup_attempted_ = false;
    element_world_read_lookup_attempted_ = false;
    element_world_basis_lookup_attempted_ = false;
    element_rotation_lookup_attempted_ = false;
    bone_rotation_lookup_attempted_ = false;
    single_player_fallback_used_ = false;
    campaign_module_fallback_used_ = false;
    vm_ = nullptr;
    being_generation_ = 0;
    last_gameplay_input_ = {};
    gameplay_input_applied_ = false;
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
        input_digital_class_ && input_analog_class_ && digital_device_field_ &&
        digital_button_field_ && digital_translate_method_ && analog_device_field_ &&
        analog_axis_field_ && analog_axis_sign_field_ && analog_translate_method_) {
        return true;
    }
    if (!EnsureCampaignAccess(env, error)) return false;

    const auto find_class = EnvFunction<FindClassFn>(env, kFindClass);
    const auto new_global = EnvFunction<NewGlobalRefFn>(env, kNewGlobalRef);
    const auto get_static_field = EnvFunction<GetStaticFieldIdFn>(env, kGetStaticFieldId);
    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    const auto get_class = EnvFunction<GetObjectClassFn>(env, kGetObjectClass);
    const auto get_field = EnvFunction<GetFieldIdFn>(env, kGetFieldId);
    const auto get_method = EnvFunction<GetMethodIdFn>(env, kGetMethodId);
    if (!find_class || !new_global || !get_static_field || !get_static_object ||
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
    DeleteLocal(env, controller_class);
    DeleteLocal(env, controller);
    if (!controller_actions_field_ || !controller_targets_field_ ||
        ClearException(env, error, "GameInputController action/target lookup failed")) {
        controller_actions_field_ = nullptr;
        controller_targets_field_ = nullptr;
        return false;
    }

    void* digital_local = find_class(env, "InputDigital");
    void* analog_local = find_class(env, "InputAnalog");
    if (!digital_local || !analog_local ||
        ClearException(env, error, "InputDigital/InputAnalog class lookup failed")) {
        DeleteLocal(env, digital_local);
        DeleteLocal(env, analog_local);
        return false;
    }
    input_digital_class_ = new_global(env, digital_local);
    input_analog_class_ = new_global(env, analog_local);
    DeleteLocal(env, digital_local);
    DeleteLocal(env, analog_local);
    if (!input_digital_class_ || !input_analog_class_ ||
        ClearException(env, error, "gameplay input class global-ref creation failed")) {
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

bool JavaPlayerBridge::TryApplyGameplayInput(
    const cojvr::runtime::GameplayInputState& state,
    std::string* error) noexcept {
    if (!state.active && !gameplay_input_applied_) return true;

    void* env = Environment(error);
    if (!env || !EnsureGameplayInputAccess(env, error)) return false;

    const auto get_static_object = EnvFunction<GetStaticObjectFieldFn>(env, kGetStaticObjectField);
    const auto get_object_field = EnvFunction<GetObjectFieldFn>(env, kGetObjectField);
    const auto get_length = EnvFunction<GetArrayLengthFn>(env, kGetArrayLength);
    const auto get_element = EnvFunction<GetObjectArrayElementFn>(env, kGetObjectArrayElement);
    const auto get_int = EnvFunction<GetIntFieldFn>(env, kGetIntField);
    const auto is_instance = EnvFunction<IsInstanceOfFn>(env, kIsInstanceOf);
    const auto call_boolean = EnvFunction<CallBooleanMethodAFn>(env, kCallBooleanMethodA);
    if (!get_static_object || !get_object_field || !get_length || !get_element || !get_int ||
        !is_instance || !call_boolean) {
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
        if ((digital && !needs_digital_update) || (analog && !needs_analog_update)) {
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

        bool invoked = false;
        for (std::int32_t index = 0; index < target_count; ++index) {
            void* target = get_element(env, targets, index);
            if (ClearException(env, error, "InputTarget array access failed")) {
                DeleteLocal(env, target);
                DeleteLocal(env, action);
                return false;
            }
            if (!target) continue;
            if (digital) {
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
            if (ClearException(env, error, "InputAction.Translate failed")) {
                DeleteLocal(env, action);
                return false;
            }
            invoked = true;
        }
        DeleteLocal(env, action);
        return invoked;
    };

    bool ok = true;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index].action != previous_values[index].action ||
            !apply_value(values[index], previous_values[index])) {
            ok = false;
            break;
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

void JavaPlayerBridge::ClearBeing(void* env) noexcept {
    if (being_ && env) {
        if (const auto delete_global = EnvFunction<DeleteGlobalRefFn>(env, kDeleteGlobalRef)) {
            delete_global(env, being_);
        }
    }
    being_ = nullptr;
    get_mesh_element_method_ = nullptr;
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
    bone_read_lookup_attempted_ = false;
    element_world_read_lookup_attempted_ = false;
    element_world_basis_lookup_attempted_ = false;
    element_rotation_lookup_attempted_ = false;
    bone_rotation_lookup_attempted_ = false;
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
    const bool lookup_failed = ClearException(env, error, "player native-method lookup failed");
    DeleteLocal(env, player_class);
    if (lookup_failed || !get_mesh || !get_position || !set_position) {
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
