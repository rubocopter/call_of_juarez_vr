#include "runtime/ui_pointer_ownership.hpp"
#include <iostream>

int main() {
    using namespace cojvr::runtime;
    UiPointerOwnership pointer;
    // Automatic ray: no shoulder button is needed to point at the menu.
    for (int frame = 0; frame < 100; ++frame)
        if (pointer.Update(true, true, false, false, true, true) != UiPointerHand::right) {
            std::cerr << "automatic tracked ray still requires a shoulder button\n";
            return 1;
        }
    const auto right_claim = pointer.claim();
    // A fresh left trigger chooses its ray; release keeps that hand stable.
    if (pointer.Update(true, true, true, false, true, true) != UiPointerHand::left) return 2;
    if (pointer.claim() == right_claim ||
        pointer.Update(true, true, false, false, true, true) != UiPointerHand::left) return 3;
    if (UiPointerSelect(UiPointerHand::left, false, true) ||
        !UiPointerSelect(UiPointerHand::left, true, false)) return 4;
    if (pointer.Update(true, true, false, true, true, true) != UiPointerHand::right) return 5;
    if (pointer.Update(true, true, false, false, false, true) != UiPointerHand::right) return 6;
    if (UiPointerSelect(UiPointerHand::right, true, false) ||
        !UiPointerSelect(UiPointerHand::right, false, true) ||
        UiPointerSelect(UiPointerHand::none, true, true)) return 7;
    if (pointer.Update(false, true, false, false, true, true) != UiPointerHand::none) return 8;
    if (pointer.Update(true, true, false, false, true, true) != UiPointerHand::right) return 9;
    if (pointer.Update(true, true, false, false, true, false) != UiPointerHand::left) return 10;
    if (pointer.Update(true, false, false, false, true, true) != UiPointerHand::none) return 11;
    if (pointer.Update(true, true, false, false, false, false) != UiPointerHand::none) return 12;
    if (pointer.Update(true, true, false, false, true, true, false, false) != UiPointerHand::none) return 13;
    if (pointer.Update(true, true, false, false, true, true) != UiPointerHand::right) return 14;
    UiPointerSelection selection;
    selection.Observe(UiPointerHand::left, 1, false, false);
    // A right trigger edge never queues against a left-owned ray.
    selection.Observe(UiPointerHand::left, 1,
        UiPointerSelect(UiPointerHand::left, false, true),
        UiPointerSelect(UiPointerHand::left, false, true));
    selection.Observe(UiPointerHand::right, 2, true, false);
    if (selection.snapshot().pending) return 15;
    selection.Observe(UiPointerHand::right, 2, true, true);
    if (!selection.snapshot().pending) return 16;
    selection.Observe(UiPointerHand::none, 0, false, false);
    selection.Observe(UiPointerHand::right, 3, true, false);
    if (selection.snapshot().pending) return 17;
    selection.Observe(UiPointerHand::right, 3, true, true);
    auto old_claim = selection.snapshot();
    old_claim.claim = 2;
    selection.Complete(old_claim, true);
    if (!selection.snapshot().pending) return 18;
    selection.Complete(selection.snapshot(), false);
    if (!selection.snapshot().pending) return 19;
    selection.Complete(selection.snapshot(), true);
    if (selection.snapshot().pending) return 20;
    selection.Observe(UiPointerHand::right, 3, true, true);
    const auto first_click = selection.snapshot();
    selection.Observe(UiPointerHand::right, 3, false, false);
    if (!selection.snapshot().pending || selection.snapshot().held) return 25;
    auto not_ready = selection.snapshot();
    not_ready.pending = false;
    selection.Complete(not_ready, true);
    if (!selection.snapshot().pending) return 26;
    selection.Observe(UiPointerHand::right, 3, true, true);
    selection.Complete(first_click, true);
    if (!selection.snapshot().pending) return 21;
    UiPointerGameplayGate weapon_gate;
    weapon_gate.Observe(true, true, true, true, true);
    weapon_gate.Observe(false, true, true, true, true);
    if (!weapon_gate.left_blocked() || !weapon_gate.right_blocked()) return 22;
    weapon_gate.Observe(false, false, false, false, false);
    if (!weapon_gate.left_blocked() || !weapon_gate.right_blocked()) return 23;
    weapon_gate.Observe(false, true, false, true, false);
    if (weapon_gate.left_blocked() || weapon_gate.right_blocked()) return 24;
    std::cout << "automatic hand ownership, selection, loss and gameplay gate passed\n";
}
