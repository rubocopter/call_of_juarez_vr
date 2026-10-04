#pragma once

#include "runtime/vr_types.hpp"
#include "runtime/hud_text.hpp"
#include "games/call_of_juarez/body_adapter.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cojvr::games::call_of_juarez {

struct JavaPlayerPosition {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct CoJWeaponAttackOrigins {
    JavaPlayerPosition ballistic_origin{};
    JavaPlayerPosition visual_origin{};
    bool ballistic_valid = false;
    bool visual_valid = false;
};

struct CoJWeaponShotDiagnostics {
    std::array<int, 5> values{}; // attack entries, hits, FX entries, FX status, suppressed FX
    std::array<int, 2> fx_handles{}; // last committed comb/smoke; not retained emitter pointers
    std::uint32_t owner_id = 0; // fresh active Weapon.GetThisID, game-thread observation only
    bool valid = false;
};

// Exact-build native sprite-tree mouse delivery. The Java object ID is the
// shipped GameObject.GetThisID handle, valid only during the game-thread call.
using CoJNativeUiMouseDispatch = bool(*)(
    std::uint32_t object_id, float pixel_x, float pixel_y, std::string* error) noexcept;
using CoJNativeUiMouseButtonDispatch = bool(*)(
    std::uint32_t object_id, std::string* error) noexcept;

struct CoJJniDiagnosticState {
    bool vm_cached = false;
    bool environment_observed = false;
    bool thread_attached_by_bridge = false;
    std::uint64_t being_generation = 0;
};

struct CoJMovementObservation {
    JavaPlayerPosition position{};
    JavaPlayerPosition wanted_local_speed{};
    float game_time = 0.0F;
    float game_time_delta = 0.0F;
    float forward_speed = 0.0F;
    float side_speed = 0.0F;
    float current_vertical_speed = 0.0F;
    float jump_height = 0.0F;
    float stair_height = 0.0F;
    int speed_state = 0;
    int ode_walk_state = 0;
    bool run = false;
    bool can_run = false;
    bool can_jump = false;
    bool jumping = false;
};

[[nodiscard]] const char* CoJMovementSpeedStateName(int speed_state) noexcept;
[[nodiscard]] bool CoJOdeWalkStateGrounded(int ode_walk_state) noexcept;

struct CoJGameplayActionValue {
    int action = -1;
    float value = 0.0F;
};

struct CoJGameplayTargetSelection {
    int action = -1;
    int index = -1;
    bool valid = false;
};

struct CoJSubtitleRuntimeState {
    bool subtitles_enabled = false;
    bool dialog_playing = false;
    bool current_line_visible = false;
    bool dialog_subtitle_visible = false;
};

enum class CoJUiDispatchRoute {
    none,
    paused_hint,
    global_menu,
    active_game_menu,
    active_game_module,
    loading_ui,
    intro_skip,
};

enum class CoJCurrentUiResolution {
    resolved,
    unavailable,
    error,
};

[[nodiscard]] const char* CoJUiDispatchRouteName(CoJUiDispatchRoute route) noexcept;
[[nodiscard]] const char* CoJUiBackDispatchRouteName(CoJUiDispatchRoute route) noexcept;

struct CoJUiBackDispatchPolicy {
    bool dispatch = false;
    int key_code = 1;
    bool press = true;
    bool release = true;
};

[[nodiscard]] CoJUiBackDispatchPolicy BuildCoJUiBackDispatchPolicy(
    bool current_ui_available) noexcept;
[[nodiscard]] bool ShouldDispatchCoJNativeUiNavigation(
    bool pause_pressed, bool back_pressed,
    bool game_timer_valid, bool game_timer_frozen) noexcept;
[[nodiscard]] bool ShouldFallbackCoJUiBack(
    CoJCurrentUiResolution resolution) noexcept;

struct CoJUiPointerDispatchPolicy {
    bool dispatch_cursor = false;
    bool dispatch_process_mouse = false;
};

[[nodiscard]] CoJUiPointerDispatchPolicy BuildCoJUiPointerDispatchPolicy(
    bool menu_available,
    bool current_ui_available) noexcept;

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
[[nodiscard]] std::array<CoJGameplayActionValue, 33> BuildCoJGameplayActionValues(
    const cojvr::runtime::GameplayInputState& state) noexcept;
[[nodiscard]] bool UseDirectAnalogCoJLocomotion(int action) noexcept;
enum class CoJInputDispatchPhase { all, non_fire, fire };
[[nodiscard]] bool ShouldDispatchCoJGameplayAction(CoJInputDispatchPhase phase, int action) noexcept;
[[nodiscard]] cojvr::runtime::GameplayInputState MergeCoJGameplayInputPhase(
    const cojvr::runtime::GameplayInputState& previous,
    const cojvr::runtime::GameplayInputState& current, CoJInputDispatchPhase phase) noexcept;
[[nodiscard]] CoJGameplayTargetSelection BuildCoJGameplayTargetSelection(
    int action,
    int target_type,
    int target_count) noexcept;
[[nodiscard]] bool ShouldYieldCoJArmIkForNativeReload(
    bool observation_available,
    bool reloading) noexcept;

// Thin adapter over the Java 1.4 VM already owned by ChromeEngine3. It never
// creates or loads a second VM. All lookups fail closed when the current game
// state does not expose an unambiguous player being. Multiplayer discovery
// uses Session/NetPlayer; the exact single-player campaign fallback uses
// LawmanGame.sm_cActiveGameModule -> LawmanModuleSingle.GetMainPlayer().
class JavaPlayerBridge final {
public:
    [[nodiscard]] bool TryApplyTrackedWeapon(
        int hand, cojvr::runtime::Vec3 wrist, cojvr::runtime::Vec3 grip,
        cojvr::runtime::Vec3 direction, cojvr::runtime::Vec3 up,
        JavaPlayerPosition& muzzle, std::string* error = nullptr,
        const ElementWorldBasisTarget* natural_hand = nullptr,
        ElementWorldBasisTarget* tracked_hand = nullptr) noexcept;
    [[nodiscard]] bool TryApplyIndependentHand(int hand,
        const ElementWorldBasisTarget& natural_hand, const ElementWorldBasisTarget& tracked_hand,
        std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryReadHandAttachment(int hand, ElementWorldBasisTarget& frame,
        std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryPrepareTrackedWeapon(int hand, cojvr::runtime::Vec3 socket,
        cojvr::runtime::Vec3 grip, cojvr::runtime::Vec3 direction, cojvr::runtime::Vec3 up,
        const ElementWorldBasisTarget& natural_hand, ElementWorldBasisTarget& tracked_hand,
        bool& unarmed_hand,
        std::string* error = nullptr) noexcept;
    [[nodiscard]] bool ApplyPreparedTrackedWeapon(int hand, JavaPlayerPosition& muzzle,
        std::string* error = nullptr) noexcept;
    [[nodiscard]] bool VerifyTrackedWeapon(int hand, bool& active,
        std::string* error = nullptr) noexcept;
    [[nodiscard]] bool RestoreTrackedHands(std::string* error = nullptr) noexcept;
    [[nodiscard]] bool VerifyTrackedHand(int hand, bool& active, std::string* error = nullptr) noexcept;
    void OnVisualBodyOffsetRestored(cojvr::runtime::Vec3 removed_offset) noexcept;
    [[nodiscard]] bool RestoreTrackedWeapons(std::string* error = nullptr, int only_hand = -1) noexcept;
    [[nodiscard]] bool TryPublishWeaponRay(
        int hand, const JavaPlayerPosition& origin, const JavaPlayerPosition& direction,
        bool valid, std::string* error = nullptr) noexcept;
    void Reset() noexcept;
    [[nodiscard]] bool Refresh(std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryGetPosition(
        JavaPlayerPosition& position, std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryObserveMovement(
        CoJMovementObservation& observation,
        std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryGetMovementClock(
        float& game_time,
        float& game_time_delta,
        std::string* error = nullptr) noexcept;
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
    [[nodiscard]] bool TryObserveSubtitleState(
        CoJSubtitleRuntimeState& state, std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryGetWeaponReloading(
        bool& reloading, std::string* error = nullptr) noexcept;
    [[nodiscard]] bool TryObserveWeaponShotDiagnostics(
        int hand, CoJWeaponShotDiagnostics& state, std::string* error = nullptr) noexcept;
    // Read the active weapon's actual mesh barrel, independent of controller
    // fields and m_avAimFromPoint. Missing weapons/barrels fail closed.
    [[nodiscard]] bool TryGetWeaponBarrel(
        int hand, JavaPlayerPosition& origin, JavaPlayerPosition& direction,
        std::string* error = nullptr,
        CoJWeaponAttackOrigins* attack_origins = nullptr,
        ElementWorldBasisTarget* barrel_frame = nullptr) noexcept;
    [[nodiscard]] bool TryApplyUpperBodyTracking(
        float head_horizontal_offset_degrees,
        float spine_horizontal_offset_degrees,
        float head_vertical_offset_degrees,
        std::string* error = nullptr) noexcept;

    [[nodiscard]] bool TryObserveHudText(runtime::HudTextSnapshot& text) noexcept;
    // Exact BeingTriggered.CheckTriggers cache. Only its local getter helpers
    // use this gaze ray; native look, ballistics, range and trigger rules remain
    // owned by the game. Context loss clears both nullable instance fields.
    [[nodiscard]] bool TryPublishInteractionRay(const JavaPlayerPosition& origin,
        const JavaPlayerPosition& direction, bool valid,
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
        std::string* error = nullptr,
        CoJInputDispatchPhase phase = CoJInputDispatchPhase::all) noexcept;
    // Route an intentional VR UI-select press through the currently visible
    // CoJ UI. GameUILoading owns an exact OnInputKey route for its exclusive
    // "press a key" gate; ordinary menus keep their Enter helper route.
    [[nodiscard]] bool TryDispatchUiSelectPress(
        bool require_loading_ui,
        bool* current_ui_is_loading = nullptr,
        std::string* error = nullptr,
        bool* paused_hint_dismissed = nullptr,
        CoJUiDispatchRoute* route = nullptr) noexcept;
    // Route a ray-trigger press through the same native mouse context used by
    // hover. Startup/loading and paused hints retain their dedicated shipped
    // input paths; ordinary menu roots dispatch native left mouse instead of
    // the keyboard/global Enter helper.
    [[nodiscard]] bool TryDispatchUiPointerSelectPress(
        bool require_loading_ui,
        CoJNativeUiMouseButtonDispatch dispatch_button,
        bool* current_ui_is_loading = nullptr,
        std::string* error = nullptr,
        bool* paused_hint_dismissed = nullptr,
        CoJUiDispatchRoute* route = nullptr) noexcept;
    [[nodiscard]] bool TryDispatchUiBackPress(
        std::string* error = nullptr,
        CoJUiDispatchRoute* route = nullptr) noexcept;
    // Deliver through the current UI's exact native sprite-tree mouse dispatcher
    // before moving its cursor sprite. A current UI requires that dispatcher;
    // cursor-only startup states remain distinguishable via input_consumed.
    // This is invoked from the game Present thread.
    [[nodiscard]] bool TryProcessUiPointer(
        float pixel_x,
        float pixel_y,
        std::string* error = nullptr,
        CoJNativeUiMouseDispatch dispatch_mouse = nullptr,
        bool* input_consumed = nullptr) noexcept;
    // Observe the current UI's native GetMousePos (X/Z mapped to X/Y), falling
    // back to cursor sprite position only when no current UI exists. Call on
    // the game thread; failure clears all output.
    [[nodiscard]] bool TryReadUiPointer(
        JavaPlayerPosition& position,
        std::string* error = nullptr,
        std::int32_t* ui_index = nullptr) noexcept;
    // Override the exact per-hand look direction consumed by
    // GetFireDirForWeapon. The game's next UpdateLookAndAimDirs pass naturally
    // replaces this value, so loss of VR input fails back to native aiming.
    [[nodiscard]] bool TrySetPerHandAimDirection(
        int hand,
        const JavaPlayerPosition& direction,
        std::string* error = nullptr) noexcept;
    // Update the exact per-hand visualization origin consumed by
    // GetFireOriginVisualizationForWeapon -> GetFireOriginVisualizationForHand.
    // Verified weapon rays use the separate nullable shot-consumer cache.
    [[nodiscard]] bool TrySetPerHandAimOrigin(
        int hand,
        const JavaPlayerPosition& origin,
        std::string* error = nullptr) noexcept;
    [[nodiscard]] float last_snap_turn_degrees() const noexcept {
        return last_snap_turn_degrees_;
    }
    [[nodiscard]] bool last_analog_transaction_applied() const noexcept {
        return last_analog_transaction_applied_;
    }
    // Detach the current thread from the JVM if it was attached by this bridge.
    // Should be called when a thread that used JNI operations is about to exit.
    void DetachCurrentThread() noexcept;

    // Cached state only: failure telemetry must not make new JNI calls.
    [[nodiscard]] CoJJniDiagnosticState jni_diagnostic_state() const noexcept;
    void SetDetailedExceptionDiagnostics(bool enabled) noexcept {
        detailed_exception_diagnostics_ = enabled;
    }

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
    [[nodiscard]] bool EnsureGameplayInputAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureMovementObservationAccess(
        void* env, std::string* error) noexcept;
    [[nodiscard]] bool EnsureVectorAccess(void* env, std::string* error) noexcept;
    [[nodiscard]] CoJCurrentUiResolution TryResolveCurrentGameUi(
        void* env,
        void*& ui,
        CoJUiDispatchRoute& route,
        std::string* error,
        bool load_if_missing = true) noexcept;
    [[nodiscard]] bool TryResolveMenuForUiRoute(
        void* env,
        CoJUiDispatchRoute route,
        void*& menu,
        std::string* error,
        CoJCurrentUiResolution* resolution = nullptr) noexcept;
    // On success the caller owns the returned JNI local reference.
    [[nodiscard]] bool TryResolveUiCursor(
        void* env, void*& cursor, std::string* error,
        bool fail_on_menu_error = false,
        std::int32_t* ui_index = nullptr) noexcept;
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

    [[nodiscard]] bool ReadObjectElementFrame(
        void* object, int element, JavaPlayerPosition& position,
        JavaPlayerPosition& up, JavaPlayerPosition& forward, std::string* error) noexcept;
    [[nodiscard]] bool WriteObjectElementFrame(
        void* object, int element, const JavaPlayerPosition& up,
        const JavaPlayerPosition& forward, const JavaPlayerPosition& position,
        std::string* error) noexcept;
    struct WeaponOverlay {
        void* weapon = nullptr;
        std::vector<ElementWorldBasisTarget> natural;
        std::vector<ElementWorldBasisTarget> targets;
        TrackedWeaponFramePlan plan{};
        JavaPlayerPosition actor_at_capture{};
        int barrel_element = -1;
        bool active = false;
        bool applied = false;
    };
    bool CaptureTrackedWeapon(int hand, cojvr::runtime::Vec3 wrist, cojvr::runtime::Vec3 grip,
        cojvr::runtime::Vec3 direction, cojvr::runtime::Vec3 up, std::string* error,
        const ElementWorldBasisTarget* natural_hand, ElementWorldBasisTarget* tracked_hand,
        bool* unarmed_hand = nullptr) noexcept;
    std::array<WeaponOverlay, 2> weapon_overlays_{};
    std::array<void*, 8> weapon_ray_fields_{};
    bool weapon_overlay_faulted_ = false;
    struct HandOverlay {
        void* object = nullptr;
        CoJHandOverlay frames{};
    };
    struct HandIoContext { JavaPlayerBridge* bridge; void* object; std::string* error; };
    static bool ReadHandFrame(void*, int, ElementWorldBasisTarget&) noexcept;
    static bool WriteHandFrame(void*, int, const ElementWorldBasisTarget&) noexcept;
    std::array<HandOverlay, 2> hand_overlays_{};
    std::array<std::array<int,20>,2> independent_hand_elements_{};
    std::uint64_t independent_hand_generation_ = 0;
    void* independent_arms_object_ = nullptr;
    int independent_arms_element_ = -1;
    bool independent_arms_were_hidden_ = false;
    bool independent_hand_faulted_ = false;

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
    std::array<void*,2> interaction_ray_fields_{};
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
    void* get_time_method_ = nullptr;
    void* get_time_delta_method_ = nullptr;
    void* get_forward_speed_method_ = nullptr;
    void* get_side_speed_method_ = nullptr;
    void* get_current_vertical_speed_method_ = nullptr;
    void* get_wanted_local_speed_method_ = nullptr;
    void* get_run_method_ = nullptr;
    void* can_run_method_ = nullptr;
    void* get_speed_state_method_ = nullptr;
    void* can_jump_method_ = nullptr;
    void* is_jumping_method_ = nullptr;
    void* get_jump_height_method_ = nullptr;
    void* get_stair_height_method_ = nullptr;
    void* get_ode_walk_state_method_ = nullptr;
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
    void* controller_lock_apply_method_ = nullptr;
    void* controller_unlock_apply_method_ = nullptr;
    void* controller_apply_method_ = nullptr;
    void* input_settings_class_ = nullptr;
    void* input_target_type_method_ = nullptr;
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
    void* is_weapon_reloading_method_ = nullptr;
    bool bone_read_lookup_attempted_ = false;
    bool element_world_read_lookup_attempted_ = false;
    bool element_world_basis_lookup_attempted_ = false;
    bool element_rotation_lookup_attempted_ = false;
    bool bone_rotation_lookup_attempted_ = false;
    bool element_visibility_lookup_attempted_ = false;
    bool aim_lookup_attempted_ = false;
    bool weapon_reload_lookup_attempted_ = false;
    bool body_rotation_lookup_attempted_ = false;
    bool movement_observation_lookup_attempted_ = false;
    bool single_player_fallback_used_ = false;
    bool campaign_module_fallback_used_ = false;
    bool jni_environment_observed_ = false;
    bool detailed_exception_diagnostics_ = false;
    std::uint64_t being_generation_ = 0;
    runtime::HudTextSnapshot hud_text_cache_{};
    std::uint64_t hud_text_player_generation_ = 0;
    std::uint64_t hud_text_sample_tick_ = 0;
    bool hud_text_sample_valid_ = false;
    cojvr::runtime::GameplayInputState last_gameplay_input_{};
    bool gameplay_input_applied_ = false;
    CoJSnapTurnState snap_turn_state_{};
    float last_snap_turn_degrees_ = 0.0F;
    bool last_analog_transaction_applied_ = false;
};

} // namespace cojvr::games::call_of_juarez
