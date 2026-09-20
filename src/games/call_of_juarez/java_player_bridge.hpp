#pragma once

#include "runtime/vr_types.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace cojvr::games::call_of_juarez {

struct JavaPlayerPosition {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct CoJGameplayActionValue {
    int action = -1;
    float value = 0.0F;
};

enum class CoJUiDispatchRoute {
    none,
    paused_hint,
    global_menu,
    active_game_menu,
    intro_skip,
};

[[nodiscard]] const char* CoJUiDispatchRouteName(CoJUiDispatchRoute route) noexcept;

struct CoJSnapTurnState {
    bool latched = false;

    // Return one exact game-yaw step when the right stick crosses the engage
    // threshold. The stick must return near centre before another step can fire.
    [[nodiscard]] float Update(
        const cojvr::runtime::GameplayInputState& state) noexcept;
};

struct CoJLoadingUiInputResult {
    bool dispatch_select = false;
    bool suppress_fire = false;
    bool suppress_gameplay = false;
};

// Exact-game safety policy for GameUILoading::WaitForUserInput.  A Sense
// UI-select press may satisfy the frozen loading gate. While the game timer is
// frozen all gameplay input/body mutation is neutralized; the physical trigger
// remains neutralized until release so the same press cannot become a gameplay
// shot when TimerStart resumes the simulation.
class CoJLoadingUiInputGate final {
public:
    [[nodiscard]] CoJLoadingUiInputResult Update(
        bool timer_state_valid,
        bool timer_frozen,
        bool ui_select_pressed,
        bool fire_left,
        bool fire_right) noexcept;
    void Reset() noexcept;

private:
    bool fire_release_required_ = false;
};

// MainMenuModule can be transiently unavailable during startup/menu changes.
// Keep one physical select edge alive only while the trigger remains held so
// the exact Java UI route can be retried without generating extra selections.
class CoJUiSelectRetryState final {
public:
    void Observe(bool pressed_edge, bool select_held) noexcept;
    [[nodiscard]] bool ShouldDispatch(
        bool pointer_active,
        bool timer_frozen) const noexcept;
    void Complete(bool dispatched) noexcept;

private:
    bool pending_ = false;
    bool held_ = false;
};

// Exact Call of Juarez action IDs from Data/InputActions.def/InputSettings.
// Stick axes are decomposed into the game's directional action model while
// button semantics stay independent from the physical XR controller profile.
[[nodiscard]] std::array<CoJGameplayActionValue, 16> BuildCoJGameplayActionValues(
    const cojvr::runtime::GameplayInputState& state) noexcept;
[[nodiscard]] bool UseDirectAnalogCoJLocomotion(int action) noexcept;

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
    [[nodiscard]] bool TryGetMeshElementByName(
        const char* name, int& element, std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryGetMeshElementHidden(
        int element, bool& hidden, std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TrySetMeshElementHidden(
        int element, bool hidden, std::string* error = nullptr) noexcept;
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
    // Rotate the exact PlayerBeing root through the same shipped route already
    // used by headset-validated snap turn. VR body yaw commits this only after
    // the stereo eye overlays have been restored for the current frame.
    [[nodiscard]] bool TryRotateHorizontally(
        float degrees,
        std::string* error = nullptr) noexcept;
    // Feed semantic VR gameplay intent through the game's own configured
    // InputAction objects. This never synthesizes process-global keyboard or
    // mouse input; digital transitions and analog values are translated on
    // the game JVM thread using the active CoJ bindings.
    [[nodiscard]] bool TryApplyGameplayInput(
        const cojvr::runtime::GameplayInputState& state,
        std::string* error = nullptr) noexcept;
    // Route an intentional VR UI-select press through the currently visible
    // CoJ GameUserInterface.  The shipped helper emits scan-code 28 (Enter)
    // through GameObject.CallOnInputKeyGlobal, which also reaches the
    // GameUILoading exclusive key request and its TimerStart continuation.
    [[nodiscard]] bool TryDispatchUiSelectPress(
        bool require_loading_ui,
        bool* current_ui_is_loading = nullptr,
        std::string* error = nullptr,
        bool* paused_hint_dismissed = nullptr,
        CoJUiDispatchRoute* route = nullptr) noexcept;
    // Re-arm the shipped GameUserInterface mouse-processing path after the
    // flat-theater ray moves the real Win32 cursor. This is invoked from the
    // game Present thread, never from the OpenVR presenter worker.
    [[nodiscard]] bool TryProcessUiPointer(std::string* error = nullptr) noexcept;
    // Override the exact per-hand look direction consumed by
    // GetFireDirForWeapon. The game's next UpdateLookAndAimDirs pass naturally
    // replaces this value, so loss of VR input fails back to native aiming.
    [[nodiscard]] bool TrySetPerHandAimDirection(
        int hand,
        const JavaPlayerPosition& direction,
        std::string* error = nullptr) noexcept;
    // Update the exact per-hand visualization origin consumed by
    // GetFireOriginVisualizationForWeapon -> GetFireOriginVisualizationForHand.
    // This leaves the ordinary ballistic Being.m_vLookFromPoint ownership to
    // the narrowly scoped fire-input override below.
    [[nodiscard]] bool TrySetPerHandAimOrigin(
        int hand,
        const JavaPlayerPosition& origin,
        std::string* error = nullptr) noexcept;
    // Cache the controller-derived world origin for one inventory hand. When
    // that hand's digital fire action is translated, TryApplyGameplayInput
    // substitutes Being.m_vLookFromPoint only for the synchronous input call
    // and restores the native value immediately afterwards.
    void SetPerHandFireOrigin(
        int hand,
        const JavaPlayerPosition& origin,
        bool valid) noexcept;
    [[nodiscard]] float last_snap_turn_degrees() const noexcept {
        return last_snap_turn_degrees_;
    }

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
    [[nodiscard]] bool EnsureElementVisibilityAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureAimAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureFireOriginAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureGameplayInputAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureVectorAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool TryResolveCurrentGameUi(
        void* env,
        void*& ui,
        CoJUiDispatchRoute& route,
        std::string* error) noexcept;
    [[nodiscard]] bool TryDispatchIntroSkip(
        void* env,
        std::string* error) noexcept;
    [[nodiscard]] bool TryDismissPausedHint(
        void* env, bool& dismissed, std::string* error) noexcept;
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
    void* get_element_id_method_ = nullptr;
    void* hide_element_method_ = nullptr;
    void* unhide_element_method_ = nullptr;
    void* is_element_hidden_method_ = nullptr;
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
    void* rotate_horizontally_method_ = nullptr;
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
    void* input_controller_field_ = nullptr;
    void* controller_actions_field_ = nullptr;
    void* controller_targets_field_ = nullptr;
    void* input_digital_class_ = nullptr;
    void* input_analog_class_ = nullptr;
    void* digital_device_field_ = nullptr;
    void* digital_button_field_ = nullptr;
    void* digital_translate_method_ = nullptr;
    void* analog_device_field_ = nullptr;
    void* analog_axis_field_ = nullptr;
    void* analog_axis_sign_field_ = nullptr;
    void* analog_translate_method_ = nullptr;
    void* input_target_item_target_field_ = nullptr;
    void* input_target_item_next_field_ = nullptr;
    void* input_target_can_execute_method_ = nullptr;
    void* game_object_class_ = nullptr;
    void* game_object_controller_input_method_ = nullptr;
    void* look_dir_for_hand_field_ = nullptr;
    void* aim_from_point_field_ = nullptr;
    void* look_from_point_field_ = nullptr;
    bool bone_read_lookup_attempted_ = false;
    bool element_world_read_lookup_attempted_ = false;
    bool element_world_basis_lookup_attempted_ = false;
    bool element_rotation_lookup_attempted_ = false;
    bool bone_rotation_lookup_attempted_ = false;
    bool element_visibility_lookup_attempted_ = false;
    bool aim_lookup_attempted_ = false;
    bool fire_origin_lookup_attempted_ = false;
    bool body_rotation_lookup_attempted_ = false;
    bool single_player_fallback_used_ = false;
    bool campaign_module_fallback_used_ = false;
    std::uint64_t being_generation_ = 0;
    cojvr::runtime::GameplayInputState last_gameplay_input_{};
    bool gameplay_input_applied_ = false;
    CoJSnapTurnState snap_turn_state_{};
    float last_snap_turn_degrees_ = 0.0F;
    std::array<JavaPlayerPosition, 2> fire_origins_{};
    std::array<bool, 2> fire_origin_valid_{};
};

} // namespace cojvr::games::call_of_juarez
