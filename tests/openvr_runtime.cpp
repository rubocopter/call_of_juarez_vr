#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <cmath>
#include <iostream>
#include <utility>
// Like the JNI dispatcher fixtures, compile the real implementation to exercise
// its transaction without initializing OpenVR or constructing a mock SDK ABI.
#define private public
#include "runtime/openvr_runtime.hpp"
#undef private
#include "../src/runtime/openvr_runtime.cpp"

namespace {

bool Near(const float a, const float b) {
    return std::fabs(a - b) <= 0.0001F;
}

} // namespace

int main() {
    using cojvr::runtime::OpenVrDigitalActionEdge;
    using cojvr::runtime::OpenVrGlobalActions;
    using cojvr::runtime::OpenVrProjectionRawToEyeFov;

    // OpenVR GetProjectionRaw convention observed on the PSVR2 runtime:
    // left/right retain their signs while top is negative and bottom positive.
    const auto fov = OpenVrProjectionRawToEyeFov(-1.84F, 0.95F, -1.33F, 1.33F);
    if (!(fov.angle_left < 0.0F && fov.angle_right > 0.0F &&
          fov.angle_up > 0.0F && fov.angle_down < 0.0F &&
          Near(fov.angle_up, -fov.angle_down))) {
        std::cerr << "OpenVR raw projection did not convert to the neutral EyeFov sign contract\n";
        return 1;
    }

    OpenVrDigitalActionEdge edge;
    if (edge.Update(true, false) || !edge.Update(true, true) ||
        edge.Update(true, true) || edge.Update(true, false) ||
        !edge.Update(true, true)) {
        std::cerr << "OpenVR digital action press-edge semantics are incorrect\n";
        return 1;
    }
    (void)edge.Update(false, false);
    if (edge.Update(true, true) || edge.Update(true, true)) {
        std::cerr << "unavailable action fabricated a press from a held recovery\n";
        return 1;
    }
    (void)edge.Update(false, false); // An unavailable false is not a release.
    if (edge.Update(true, true) || edge.Update(true, false) || !edge.Update(true, true)) {
        std::cerr << "global action must require an available release to recover\n";
        return 1;
    }
    edge.Reset();
    if (edge.Update(true, true) || edge.Update(true, false) || !edge.Update(true, true)) {
        std::cerr << "OpenVR action reset must disarm held controls until release\n";
        return 1;
    }
    OpenVrDigitalActionEdge startup;
    if (startup.Update(true,true) || startup.Update(true,false) || !startup.Update(true,true)) {
        std::cerr << "startup-held global action escaped its release barrier\n";
        return 1;
    }

    // Fault after some global actions have already been read. Every old edge,
    // including unread actions, must be cancelled and partial outputs discarded.
    cojvr::runtime::OpenVrRuntime::Impl input_owner;
    const std::array<OpenVrDigitalActionEdge*,8> edges{
        &input_owner.recenter_edge,&input_owner.ui_select_left_edge,
        &input_owner.ui_select_right_edge,&input_owner.ui_accept_edge,
        &input_owner.ui_back_edge,&input_owner.pause_edge,
        &input_owner.ui_pointer_edges[0],&input_owner.ui_pointer_edges[1]};
    for(const bool exception_failure:{false,true}){
        for(auto* action:edges)(void)action->Update(true,false);
        OpenVrGlobalActions partial{};
        cojvr::runtime::GameplayInputState partial_gameplay{};
        cojvr::runtime::OpenVrHandPoses partial_poses{};
        try{
            cojvr::runtime::OpenVrRuntime::Impl::InputPollTransaction poll{
                input_owner,partial,&partial_gameplay,&partial_poses};
            partial.ui_select_left=partial.ui_select_left_pressed=partial.ui_accept=true;
            partial_gameplay.active=partial_gameplay.fire_right=true;
            partial_poses.left_grip_active=partial_poses.left_grip.position_valid=true;
            (void)edges[1]->Update(true,true);
            if(exception_failure)throw 1; // Exercise guard unwinding directly, without an SDK call.
        }catch(int){}
        if(partial.ui_select_left||partial.ui_select_left_pressed||partial.ui_accept||
            partial_gameplay.active||partial_gameplay.fire_right||partial_poses.left_grip_active||
            partial_poses.left_grip.position_valid){
            std::cerr<<"failed input transaction retained partial output\n";return 1;
        }
        for(auto* action:edges){
            if(action->Update(true,true)||action->Update(true,false)||!action->Update(true,true)){
                std::cerr<<"partial poll failure retained an armed global action\n";return 1;
            }
        }
    }
    for(auto* action:edges)(void)action->Update(true,false);
    OpenVrGlobalActions committed{};
    {
        cojvr::runtime::OpenVrRuntime::Impl::InputPollTransaction poll{input_owner,committed};
        committed.ui_accept=committed.ui_accept_pressed=edges[3]->Update(true,true);
        poll.committed=true;
    }
    if(!committed.ui_accept_pressed||edges[3]->Update(true,true)||!edges[4]->Update(true,true)){
        std::cerr<<"successful input transaction altered output or independent action history\n";return 1;
    }

    // UI-select is shared by flat menus and the exact-game loading gate.  The
    // runtime contract must carry both held state and a single press edge so a
    // trigger held across flat_theater -> native_stereo cannot be counted as a
    // second activation.
    OpenVrGlobalActions ui_actions{};
    ui_actions.ui_select_left = true;
    ui_actions.ui_select_left_pressed = true;
    ui_actions.ui_select_right = false;
    ui_actions.ui_select_right_pressed = false;
    ui_actions.ui_accept = true;
    ui_actions.ui_accept_pressed = true;
    ui_actions.ui_back = false;
    ui_actions.ui_back_pressed = false;
    if (!ui_actions.ui_select_left || !ui_actions.ui_select_left_pressed ||
        ui_actions.ui_select_right || ui_actions.ui_select_right_pressed ||
        !ui_actions.ui_accept || !ui_actions.ui_accept_pressed ||
        ui_actions.ui_back || ui_actions.ui_back_pressed) {
        std::cerr << "OpenVR global UI-select state lost press-edge semantics\n";
        return 1;
    }

    // Move construction/assignment must leave the source safely destructible
    // and queryable even when no live OpenVR session exists.
    cojvr::runtime::OpenVrRuntime first;
    cojvr::runtime::OpenVrRuntime second(std::move(first));
    if (first.initialized() || second.initialized()) {
        std::cerr << "OpenVR move construction invented initialized state\n";
        return 1;
    }
    cojvr::runtime::Pose moved_pose{};
    std::array<cojvr::runtime::EyeView, 2> moved_eyes{};
    if (first.ReadHmdPose(moved_pose) || first.ReadEyeConfiguration(moved_eyes)) {
        std::cerr << "moved-from OpenVR runtime accepted live-state reads\n";
        return 1;
    }
    cojvr::runtime::OpenVrRuntime third;
    third = std::move(second);
    if (second.initialized() || third.initialized() ||
        second.global_actions_initialized() || third.global_actions_initialized()) {
        std::cerr << "OpenVR move assignment retained invalid ownership state\n";
        return 1;
    }

    std::cout << "OpenVR projection/input contract tests passed\n";
    return 0;
}
