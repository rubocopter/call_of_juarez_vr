#pragma once

#include <cstdint>
#include <string>

namespace cojvr::games::call_of_juarez {

struct JavaPlayerPosition {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

// Thin adapter over the Java 1.4 VM already owned by ChromeEngine3. It never
// creates or loads a second VM. All lookups fail closed when the current game
// state does not expose an unambiguous player being. Multiplayer discovery
// uses Session/NetPlayer; the exact single-player campaign fallback uses
// LawmanGame.sm_cActiveGameModule -> LawmanModuleSingle.GetMainPlayer().
class JavaPlayerBridge final {
public:
    void Reset() noexcept;
    [[nodiscard]] bool Refresh(std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryGetPosition(
        JavaPlayerPosition& position, std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TrySetPosition(
        const JavaPlayerPosition& position, std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryGetMeshElement(
        std::int8_t bone, int& element, std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryGetBoneJointPosition(
        std::int8_t bone, JavaPlayerPosition& position, std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryGetBoneDirection(
        std::int8_t bone, JavaPlayerPosition& direction, std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryGetBonePerpendicular(
        std::int8_t bone, JavaPlayerPosition& direction, std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryGetElementWorldBasis(
        int element,
        JavaPlayerPosition& position,
        JavaPlayerPosition& up,
        JavaPlayerPosition& forward,
        std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TrySetElementWorldBasis(
        int element,
        const JavaPlayerPosition& up,
        const JavaPlayerPosition& forward,
        const JavaPlayerPosition& position,
        std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryRotateElementWithChildren(
        int element,
        const JavaPlayerPosition& axis,
        float angle_degrees,
        std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryRotateBone(
        std::int8_t bone,
        const JavaPlayerPosition& axis,
        float angle_degrees,
        bool update_hierarchy,
        std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryGetActiveGameTimerFrozen(
        bool& frozen, std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryApplyUpperBodyTracking(
        float head_horizontal_offset_degrees,
        float spine_horizontal_offset_degrees,
        float head_vertical_offset_degrees,
        std::string* error = nullptr) noexcept;

    // Detach the current thread from the JVM if it was attached by this bridge.
    // Should be called when a thread that used JNI operations is about to exit.
    void DetachCurrentThread() noexcept;

    [[nodiscard]] bool player_available() const noexcept { return being_ != nullptr; }
    [[nodiscard]] std::uint64_t being_generation() const noexcept { return being_generation_; }
    [[nodiscard]] bool single_player_fallback_used() const noexcept {
        return single_player_fallback_used_;
    }
    [[nodiscard]] bool campaign_module_fallback_used() const noexcept {
        return campaign_module_fallback_used_;
    }

private:
    [[nodiscard]] void* Environment(std::string* error) noexcept;
    [[nodiscard]] bool EnsureVm(std::string* error) noexcept;
    [[nodiscard]] bool EnsureSession(void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureCampaignAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool TryResolveCampaignBeing(void* env, std::string* error) noexcept;
    [[nodiscard]] bool ResolveBeingMethods(
        void* env, void* local_being, std::string* error) noexcept;
    [[nodiscard]] bool EnsureBodyRotationAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureBoneReadAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureElementWorldReadAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureElementWorldBasisAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureElementRotationAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureBoneRotationAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureVectorAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool ReadVector(
        void* env, void* vector, JavaPlayerPosition& value, std::string* error) noexcept;
    [[nodiscard]] bool WriteVector(
        void* env, void* vector, const JavaPlayerPosition& value, std::string* error) noexcept;
    [[nodiscard]] bool ClearException(void* env, std::string* error, const char* stage) noexcept;
    void ClearBeing(void* env) noexcept;

    void* vm_ = nullptr;
    void* session_class_ = nullptr;
    void* session_local_player_field_ = nullptr;
    void* session_players_field_ = nullptr;
    void* vector_size_method_ = nullptr;
    void* vector_get_method_ = nullptr;
    void* net_player_being_field_ = nullptr;
    void* lawman_game_class_ = nullptr;
    void* lawman_module_single_class_ = nullptr;
    void* active_game_module_field_ = nullptr;
    void* is_timer_freezed_method_ = nullptr;
    void* being_ = nullptr;
    void* get_mesh_element_method_ = nullptr;
    void* get_bone_joint_method_ = nullptr;
    void* get_bone_direction_method_ = nullptr;
    void* get_bone_perpendicular_method_ = nullptr;
    void* get_element_position_method_ = nullptr;
    void* get_element_left_method_ = nullptr;
    void* get_element_up_method_ = nullptr;
    void* set_element_world_basis_method_ = nullptr;
    void* rotate_element_with_children_method_ = nullptr;
    void* bone_rotate_method_ = nullptr;
    void* get_position_vector_method_ = nullptr;
    void* set_position_method_ = nullptr;
    void* update_body_rotation_method_ = nullptr;
    void* current_head_vertical_field_ = nullptr;
    void* current_head_horizontal_field_ = nullptr;
    void* current_spine_vertical_field_ = nullptr;
    void* current_spine_horizontal_field_ = nullptr;
    void* vector_x_field_ = nullptr;
    void* vector_y_field_ = nullptr;
    void* vector_z_field_ = nullptr;
    void* vector_class_ = nullptr;
    void* vector_constructor_ = nullptr;
    bool bone_read_lookup_attempted_ = false;
    bool element_world_read_lookup_attempted_ = false;
    bool element_world_basis_lookup_attempted_ = false;
    bool element_rotation_lookup_attempted_ = false;
    bool bone_rotation_lookup_attempted_ = false;
    bool body_rotation_lookup_attempted_ = false;
    bool single_player_fallback_used_ = false;
    bool campaign_module_fallback_used_ = false;
    std::uint64_t being_generation_ = 0;
};

} // namespace cojvr::games::call_of_juarez
