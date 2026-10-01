#include "runtime/ui_pointer_ownership.hpp"
#include <iostream>

int main() {
    using namespace cojvr::runtime;
    UiPointerOwnership pointer;
    // Valid moving rays alone must never seize the physical mouse.
    for (int frame = 0; frame < 100; ++frame)
        if (pointer.Update(true, true, false, false, true, true) != UiPointerHand::none) return 1;
    if (pointer.Update(true, true, true, false, true, true) != UiPointerHand::left) return 2;
    if (pointer.Update(true, true, true, true, true, true) != UiPointerHand::left) return 3;
    if (UiPointerSelect(UiPointerHand::left, false, true) ||
        !UiPointerSelect(UiPointerHand::left, true, false)) return 4;
    // Releasing the owner must not transfer control to the other held button.
    if (pointer.Update(true, true, false, true, true, true) != UiPointerHand::none) return 5;
    pointer.Update(true, true, false, false, true, true);
    if (pointer.Update(true, true, false, true, true, true) != UiPointerHand::right) return 6;
    if (UiPointerSelect(UiPointerHand::right, true, false) ||
        !UiPointerSelect(UiPointerHand::right, false, true) ||
        UiPointerSelect(UiPointerHand::none, true, true)) return 7;
    // Focus/menu loss releases control; a held button cannot rearm it.
    if (pointer.Update(false, true, false, true, true, true) != UiPointerHand::none ||
        pointer.Update(true, true, false, true, true, true) != UiPointerHand::none) return 8;
    pointer.Update(true, true, false, false, true, true);
    if (pointer.Update(true, true, false, true, true, true) != UiPointerHand::right) return 9;
    if (pointer.Update(true, true, false, true, true, false) != UiPointerHand::none ||
        pointer.Update(true, true, false, true, true, true) != UiPointerHand::none) return 10;
    // Unavailable input must require an observed release before a new claim.
    pointer.Update(true, false, false, false, true, true);
    if (pointer.Update(true, true, true, true, true, true) != UiPointerHand::none) return 11;
    pointer.Update(true, true, false, false, true, true);
    if (pointer.Update(true, true, true, true, true, true) != UiPointerHand::right) return 12;
    pointer.Update(true, true, false, false, true, true);
    pointer.Update(true, true, false, true, true, true);
    if (pointer.Update(true, true, false, false, true, true, true, false) != UiPointerHand::none ||
        pointer.Update(true, true, false, true, true, true) != UiPointerHand::none) return 13;
    pointer.Update(true, true, false, false, true, true);
    if (pointer.Update(true, true, false, true, true, true) != UiPointerHand::right) return 14;
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
    std::cout << "passive rays, explicit hand ownership, selection and loss/rearm passed\n";
}
