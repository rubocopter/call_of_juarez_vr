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
unsigned native_selects = 0;
float cursor_x = 0, cursor_y = 0;
bool native_available = true;
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
bool FixtureUiSelect(bool, bool*, std::string*, bool*, CoJUiDispatchRoute*) noexcept {
    ++fixture::native_selects;
    return true;
}
bool FixturePointerMotion(float x, float y, std::string*) noexcept {
    if (!fixture::native_available) return false;
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
        fixture::native_selects != 1) return 21;
    CompleteFlatUiSelection(delivered, true);
    if (FlatUiSelection().pending) return 7;
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
        !FlatUiSelection().pending) {
        std::cerr << "mismatched logical readback accepted a trigger selection\n";
        return 19;
    }
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
        fixture::native_selects != 1) return 22;
    sample.claim = 4;
    FlatUiPointer(nullptr, sample);
    ApplyFlatUiPointerMotion();
    delivered = ApplyFlatUiPointerMotion();
    if (!delivered.pending) return 23;
    NeutralizeFlatUiPointer("ui_navigation");
    if (DispatchFlatUiPointerSelection(delivered, nullptr, nullptr, nullptr, nullptr) ||
        fixture::native_selects != 1) return 24;
    std::cout << "PASS - logical delivery, applied selection and physical mouse priority\n";
}
