// Exercise the actual proxy callback without moving the host's desktop cursor
// or starting a game/VR runtime. The Windows input seam records side effects.
#include <windows.h>
#include <cstdint>
#include <iostream>
#include "games/call_of_juarez/camera_probe.hpp"

namespace fixture {
HWND window = reinterpret_cast<HWND>(0x1234);
bool foreground = true;
unsigned cursor_warps = 0;
unsigned injected_moves = 0;
unsigned posted_moves = 0;
unsigned native_moves = 0;
unsigned keyboard_selects = 0;
unsigned pointer_selects = 0;
float cursor_x = 0, cursor_y = 0;
bool native_available = true;
bool select_available = true;
float native_readback_offset = 0;
std::int32_t ui_index = 0;
bool mouse_button = false;
ULONGLONG now = 100;
HWND Ancestor(HWND value, UINT) { return value; }
HWND Foreground() { return foreground ? window : nullptr; }
BOOL ClientRect(HWND, RECT* rect) { *rect = {0, 0, 1920, 1080}; return TRUE; }
BOOL ToScreen(HWND, POINT*) { return TRUE; }
BOOL CursorPos(int, int) { ++cursor_warps; return TRUE; }
UINT Input(UINT count, LPINPUT, int) { injected_moves += count; return count; }
BOOL Message(HWND, UINT message, WPARAM, LPARAM) {
    if (message == WM_MOUSEMOVE) ++posted_moves;
    return TRUE;
}
SHORT KeyState(int key) { return mouse_button && key == VK_LBUTTON ? SHORT(0x8000) : 0; }
ULONGLONG Ticks() { return now; }
}

namespace cojvr::games::call_of_juarez {
bool FixtureUiSelect(bool, bool*, std::string* error, bool*, CoJUiDispatchRoute*) noexcept {
    ++fixture::keyboard_selects;
    if (!fixture::select_available && error) *error = "fixture native dispatch failed";
    return fixture::select_available;
}
bool FixtureUiPointerSelect(bool, bool*, std::string* error, bool*, CoJUiDispatchRoute*) noexcept {
    ++fixture::pointer_selects;
    if (!fixture::select_available && error) *error = "fixture native dispatch failed";
    return fixture::select_available;
}
bool FixturePointerMotion(float x, float y, std::string*, bool* input_consumed = nullptr) noexcept {
    if (!fixture::native_available) return false;
    if (input_consumed) *input_consumed = true;
    ++fixture::native_moves;
    fixture::cursor_x = x;
    fixture::cursor_y = y;
    return true;
}
bool FixturePointerPosition(CameraProbeVector& position, std::string*, std::int32_t* ui_index = nullptr) noexcept {
    position = {fixture::cursor_x + fixture::native_readback_offset, fixture::cursor_y, 0};
    if (ui_index) *ui_index = fixture::ui_index;
    return fixture::native_available;
}
}

#define GetAncestor fixture::Ancestor
#define GetForegroundWindow fixture::Foreground
#define GetClientRect fixture::ClientRect
#define ClientToScreen fixture::ToScreen
#define SetCursorPos fixture::CursorPos
#define SendInput fixture::Input
#define PostMessageW fixture::Message
#define GetAsyncKeyState fixture::KeyState
#define GetTickCount64 fixture::Ticks
#define DispatchCameraUiPointerMotion FixturePointerMotion
#define ObserveCameraUiPointerPosition FixturePointerPosition
#define DispatchCameraUiSelectPress FixtureUiSelect
#define DispatchCameraUiPointerSelectPress FixtureUiPointerSelect
#include "../src/games/call_of_juarez/native_stereo_proxy.cpp"
#undef GetAncestor
#undef GetForegroundWindow
#undef GetClientRect
#undef ClientToScreen
#undef SetCursorPos
#undef SendInput
#undef PostMessageW
#undef GetAsyncKeyState
#undef GetTickCount64
#undef DispatchCameraUiPointerMotion
#undef ObserveCameraUiPointerPosition
#undef DispatchCameraUiSelectPress
#undef DispatchCameraUiPointerSelectPress

int main() {
    using cojvr::backends::openvr::FlatUiPointerSample;
    g_stereo.game_window.store(reinterpret_cast<std::uintptr_t>(fixture::window));
    FlatUiPointerSample sample{};
    sample.active = true;
    sample.claim = 1;
    sample.source_width = 1920;
    sample.source_height = 1080;
    sample.pixel_x = 1399;
    sample.pixel_y = 701;
    sample.u = 1399.0F / 1920.0F;
    sample.v = 701.0F / 1080.0F;
    for (unsigned frame = 0; frame < 200; ++frame) {
        if (!FlatUiPointer(nullptr, sample)) {
            std::cerr << "valid held ray was rejected\n";
            return 1;
        }
    }
    if (fixture::cursor_warps || fixture::injected_moves || fixture::posted_moves) {
        std::cerr << "held ray re-enters relative mouse input: warps="
                  << fixture::cursor_warps << " injected=" << fixture::injected_moves
                  << " messages=" << fixture::posted_moves << '\n';
        return 2;
    }
    if (fixture::native_moves != 0) return 4; // Presenter must not touch the JVM.
    sample.select_down = sample.select_pressed = true;
    if (!FlatUiPointer(nullptr, sample)) return 5;
    auto delivered = ApplyFlatUiPointerMotion();
    if (delivered.pending || fixture::native_moves != 1 ||
        fixture::cursor_x != sample.pixel_x || fixture::cursor_y != sample.pixel_y) return 6;
    // Hover processing happens in the game update between Presents. A click
    // must not dispatch in the same Present that first publishes its target.
    const auto click_x = sample.pixel_x;
    sample.select_pressed = false;
    sample.select_down = false; // An accepted brief tap survives ordinary release.
    sample.pixel_x += 100;
    FlatUiPointer(nullptr, sample); // Small aim movement while the press is queued.
    delivered = ApplyFlatUiPointerMotion();
    if (!delivered.pending || fixture::cursor_x != click_x) {
        std::cerr << "ray click did not retain its hover target for a game update\n";
        return 15;
    }
    if (!DispatchFlatUiPointerSelection(delivered, nullptr, nullptr, nullptr, nullptr) ||
        fixture::pointer_selects != 1 || fixture::keyboard_selects != 0) {
        std::cerr << "ray selection bypassed native pointer button path: pointer="
                  << fixture::pointer_selects << " keyboard=" << fixture::keyboard_selects << '\n';
        return 21;
    }
    CompleteFlatUiSelection(delivered, true);
    if (FlatUiSelection().pending) return 7;
    std::string rejection_reason;
    if (DispatchFlatUiPointerSelection(
            delivered, nullptr, &rejection_reason, nullptr, nullptr) ||
        rejection_reason != "current_not_pending") {
        std::cerr << "selection rejection was not observable: " << rejection_reason << '\n';
        return 28;
    }
    sample.select_pressed = false;
    FlatUiPointer(nullptr, sample);
    fixture::native_available = false;
    if (ApplyFlatUiPointerMotion().hand != cojvr::runtime::UiPointerHand::none ||
        fixture::native_moves != 2) return 8;
    fixture::native_available = true;
    if (ApplyFlatUiPointerMotion().hand == cojvr::runtime::UiPointerHand::none) return 9;
    // A real mouse move must retain its logical position despite an automatic ray.
    fixture::cursor_x = 300;
    fixture::cursor_y = 200;
    const auto moves_before_mouse = fixture::native_moves;
    if (ApplyFlatUiPointerMotion().hand != cojvr::runtime::UiPointerHand::none ||
        fixture::native_moves != moves_before_mouse || fixture::cursor_x != 300) {
        std::cerr << "automatic ray overwrote physical mouse movement\n";
        return 10;
    }
    fixture::now += 1000;
    if (FlatUiPointer(nullptr, sample)) return 11;
    // Movement while the laser is hidden extends the mouse's priority.
    fixture::cursor_x = 350;
    ApplyFlatUiPointerMotion();
    fixture::now += 1499;
    if (FlatUiPointer(nullptr, sample)) return 12;
    ++fixture::now;
    if (!FlatUiPointer(nullptr, sample) ||
        ApplyFlatUiPointerMotion().hand == cojvr::runtime::UiPointerHand::none) return 13;
    fixture::mouse_button = true;
    const auto before_drag = fixture::native_moves;
    ApplyFlatUiPointerMotion();
    if (fixture::native_moves != before_drag) return 14;
    fixture::mouse_button = false;
    fixture::now += 1500;
    sample.active = false;
    if (!FlatUiPointer(nullptr, sample) || FlatUiSelection().hand != cojvr::runtime::UiPointerHand::none)
        return 3;
    sample.active = true;
    sample.claim = 2;
    fixture::foreground = false;
    const auto before_focus_loss = fixture::native_moves;
    if (FlatUiPointer(nullptr, sample) ||
        ApplyFlatUiPointerMotion().hand != cojvr::runtime::UiPointerHand::none ||
        fixture::native_moves != before_focus_loss) return 16;
    fixture::foreground = true;
    sample.source_width = 0;
    if (FlatUiPointer(nullptr, sample)) return 17;
    sample.source_width = 1920;
    sample.select_down = true;
    sample.select_pressed = true;
    if (!FlatUiPointer(nullptr, sample)) return 18;
    fixture::native_readback_offset = 10;
    g_stereo.flat_ui_mouse_priority = {}; // Start a fresh readable owner.
    if (ApplyFlatUiPointerMotion().hand != cojvr::runtime::UiPointerHand::none ||
        FlatUiSelection().pending) {
        std::cerr << "failed input delivery retained a click and pinned the ray target\n";
        return 19;
    }
    fixture::native_readback_offset = 0;
    sample.select_down = sample.select_pressed = false;
    sample.pixel_x += 25;
    g_stereo.flat_ui_mouse_priority = {};
    FlatUiPointer(nullptr, sample);
    ApplyFlatUiPointerMotion();
    if (fixture::cursor_x != sample.pixel_x || FlatUiSelection().pending) return 25;
    sample.select_down = sample.select_pressed = true;
    sample.claim = 3;
    fixture::native_readback_offset = 0;
    g_stereo.flat_ui_mouse_priority = {};
    FlatUiPointer(nullptr, sample);
    ApplyFlatUiPointerMotion();
    ++fixture::ui_index; // Same cursor coordinates in a different submenu.
    if (ApplyFlatUiPointerMotion().hand != cojvr::runtime::UiPointerHand::none ||
        FlatUiSelection().pending) {
        std::cerr << "queued trigger crossed a menu-index transition\n";
        return 20;
    }
    if (DispatchFlatUiPointerSelection(delivered, nullptr, nullptr, nullptr, nullptr) ||
        fixture::pointer_selects != 1 || fixture::keyboard_selects != 0) return 22;
    sample.claim = 4;
    FlatUiPointer(nullptr, sample);
    ApplyFlatUiPointerMotion();
    delivered = ApplyFlatUiPointerMotion();
    if (!delivered.pending) return 23;
    NeutralizeFlatUiPointer("ui_navigation");
    if (DispatchFlatUiPointerSelection(delivered, nullptr, nullptr, nullptr, nullptr) ||
        fixture::pointer_selects != 1 || fixture::keyboard_selects != 0) return 24;
    sample.claim = 5;
    FlatUiPointer(nullptr, sample);
    ApplyFlatUiPointerMotion();
    fixture::native_available = false; // Fail the initial read, before dispatch.
    ApplyFlatUiPointerMotion();
    if (FlatUiSelection().pending || g_stereo.flat_ui_target_applied) return 26;
    fixture::native_available = true;
    sample.select_down = sample.select_pressed = false;
    sample.pixel_x += 50;
    FlatUiPointer(nullptr, sample);
    ApplyFlatUiPointerMotion();
    delivered = ApplyFlatUiPointerMotion();
    if (delivered.pending || fixture::cursor_x != sample.pixel_x ||
        fixture::pointer_selects != 1 || fixture::keyboard_selects != 0) return 27;
    // Reproduce a queued click in the gameplay pause menu. A native dispatch
    // failure must be distinguishable from rejecting its stale/input target.
    const auto queue_pause_click = [&] {
        fixture::foreground = fixture::native_available = true;
        fixture::native_readback_offset = 0;
        fixture::ui_index = g_stereo.flat_ui_menu_index = 9;
        g_stereo.flat_ui_menu_known = true;
        g_stereo.flat_ui_mouse_priority = {};
        NeutralizeFlatUiPointer("fixture_new_click");
        sample.select_pressed = sample.select_down = true;
        ++sample.claim;
        FlatUiPointer(nullptr, sample);
        ApplyFlatUiPointerMotion();
        return ApplyFlatUiPointerMotion();
    };
    const auto expect_rejection = [&](const auto& click, const char* expected,
                                      const bool dispatch_attempted = false) {
        const auto before = fixture::pointer_selects;
        rejection_reason = "stale diagnostic";
        const bool dispatched = DispatchFlatUiPointerSelection(
            click, nullptr, &rejection_reason, nullptr, nullptr);
        if (dispatched || rejection_reason != expected ||
            fixture::pointer_selects != before + (dispatch_attempted ? 1 : 0) ||
            fixture::keyboard_selects != 0) {
            std::cerr << "pause click rejection: expected=" << expected
                      << " observed=" << rejection_reason << '\n';
            return false;
        }
        return true;
    };
    delivered = queue_pause_click();
    fixture::foreground = false; // Focus changed after the target was delivered.
    if (!expect_rejection(delivered, "game_not_foreground")) return 29;
    delivered = queue_pause_click();
    fixture::native_available = false;
    if (!expect_rejection(delivered, "readback_failed") ||
        FlatUiSelection().pending || g_stereo.flat_ui_target_applied) return 30;
    delivered = queue_pause_click();
    ++fixture::ui_index; // Same coordinates, but a newly opened UI.
    if (!expect_rejection(delivered, "menu_index_mismatch") ||
        FlatUiSelection().pending || g_stereo.flat_ui_target_applied) return 31;
    delivered = queue_pause_click();
    fixture::native_readback_offset = 2;
    if (!expect_rejection(delivered, "coordinate_mismatch") ||
        FlatUiSelection().pending || g_stereo.flat_ui_target_applied) return 32;
    delivered = queue_pause_click();
    auto stale_click = delivered;
    --stale_click.click;
    if (!expect_rejection(stale_click, "identity_mismatch") ||
        !FlatUiSelection().pending) return 33;
    fixture::select_available = false;
    if (!expect_rejection(delivered, "dispatch_failed: fixture native dispatch failed", true) ||
        !FlatUiSelection().pending) return 34;
    fixture::select_available = true;
    if (!DispatchFlatUiPointerSelection(delivered, nullptr, &rejection_reason, nullptr, nullptr) ||
        !rejection_reason.empty()) return 35;
    CompleteFlatUiSelection(delivered, true);
    if (FlatUiSelection().pending) return 36;
    std::cout << "PASS - logical delivery, applied selection and physical mouse priority\n";
}
