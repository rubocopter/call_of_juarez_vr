#include "games/call_of_juarez/java_player_bridge.hpp"

#include <windows.h>

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
constexpr std::size_t kGetMethodId = 33;
constexpr std::size_t kCallObjectMethodA = 36;
constexpr std::size_t kCallBooleanMethodA = 39;
constexpr std::size_t kCallIntMethodA = 51;
constexpr std::size_t kCallVoidMethodA = 63;
constexpr std::size_t kGetFieldId = 94;
constexpr std::size_t kGetObjectField = 95;
constexpr std::size_t kGetFloatField = 102;
constexpr std::size_t kSetFloatField = 111;
constexpr std::size_t kGetStaticFieldId = 144;
constexpr std::size_t kGetStaticObjectField = 145;

constexpr std::size_t kVmAttachCurrentThreadAsDaemon = 7;
constexpr std::size_t kVmGetEnv = 6;

#if defined(_WIN32)
#define COJVR_JNICALL __stdcall
#else
#define COJVR_JNICALL
#endif

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
using FindClassFn = void*(COJVR_JNICALL*)(void*, const char*);
using ExceptionOccurredFn = void*(COJVR_JNICALL*)(void*);
using ExceptionClearFn = void(COJVR_JNICALL*)(void*);
using NewGlobalRefFn = void*(COJVR_JNICALL*)(void*, void*);
using DeleteGlobalRefFn = void(COJVR_JNICALL*)(void*, void*);
using DeleteLocalRefFn = void(COJVR_JNICALL*)(void*, void*);
using IsSameObjectFn = std::uint8_t(COJVR_JNICALL*)(void*, void*, void*);
using NewObjectAFn = void*(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using GetObjectClassFn = void*(COJVR_JNICALL*)(void*, void*);
using GetMethodIdFn = void*(COJVR_JNICALL*)(void*, void*, const char*, const char*);
using CallObjectMethodAFn = void*(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using CallBooleanMethodAFn = std::uint8_t(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using CallIntMethodAFn = std::int32_t(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using CallVoidMethodAFn = void(COJVR_JNICALL*)(void*, void*, void*, const JValue*);
using GetFieldIdFn = void*(COJVR_JNICALL*)(void*, void*, const char*, const char*);
using GetObjectFieldFn = void*(COJVR_JNICALL*)(void*, void*, void*);
using GetFloatFieldFn = float(COJVR_JNICALL*)(void*, void*, void*);
using SetFloatFieldFn = void(COJVR_JNICALL*)(void*, void*, void*, float);
using GetStaticFieldIdFn = void*(COJVR_JNICALL*)(void*, void*, const char*, const char*);
using GetStaticObjectFieldFn = void*(COJVR_JNICALL*)(void*, void*, void*);

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
    } else {
        being_ = nullptr;
    }
    session_class_ = nullptr;
    session_local_player_field_ = nullptr;
    session_players_field_ = nullptr;
    vector_size_method_ = nullptr;
    vector_get_method_ = nullptr;
    net_player_being_field_ = nullptr;
    get_mesh_element_method_ = nullptr;
    get_bone_joint_method_ = nullptr;
    get_bone_direction_method_ = nullptr;
    get_bone_perpendicular_method_ = nullptr;
    set_element_world_basis_method_ = nullptr;
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
    bone_read_lookup_attempted_ = false;
    element_world_basis_lookup_attempted_ = false;
    single_player_fallback_used_ = false;
    vm_ = nullptr;
    being_generation_ = 0;
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
    return env;
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
    set_element_world_basis_method_ = nullptr;
    get_position_vector_method_ = nullptr;
    set_position_method_ = nullptr;
    update_body_rotation_method_ = nullptr;
    current_head_vertical_field_ = nullptr;
    current_head_horizontal_field_ = nullptr;
    current_spine_vertical_field_ = nullptr;
    current_spine_horizontal_field_ = nullptr;
    body_rotation_lookup_attempted_ = false;
    bone_read_lookup_attempted_ = false;
    element_world_basis_lookup_attempted_ = false;
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
    if (!local_player) {
        void* players = get_static_object(env, session_class_, session_players_field_);
        if (ClearException(env, error, "Session.sm_Players read failed") || !players) {
            DeleteLocal(env, players);
            ClearBeing(env);
            SetError(error, "local player is unavailable and the session player list is unavailable");
            return false;
        }

        const std::int32_t player_count = call_int(env, players, vector_size_method_, nullptr);
        if (ClearException(env, error, "Session.sm_Players size read failed") || player_count != 1) {
            DeleteLocal(env, players);
            ClearBeing(env);
            SetError(error, player_count == 0
                ? "local player is unavailable and the session player list is empty"
                : "local player is unavailable and the session player list is ambiguous");
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
