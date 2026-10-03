#include "runtime/gameplay_utility.hpp"
#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using namespace cojvr::runtime;
    GameplayUtilityMapper mapper;
    GameplayInputState raw{};
    raw.active = true;
    raw.fire_right = true;
    if (mapper.Update(raw).fire_right) return 1; // already held at acquisition
    raw.fire_right = false;
    (void)mapper.Update(raw);
    raw.fire_right = true;
    if (!mapper.Update(raw).fire_right) return 2;
    raw.fire_right = false;
    raw.utility_modifier = true;
    raw.reload = true;
    raw.jump = true;
    raw.kick = true;
    raw.run = true;
    raw.crouch = true;
    raw.interact = true;
    raw.weapon_next = true;
    auto output = mapper.Update(raw);
    if (!output.focus || !output.alternate_fire || !output.hands || !output.walk ||
        !output.objectives || !output.discard_weapon || !output.weapon_previous ||
        output.reload || output.jump || output.kick || output.run || output.crouch ||
        output.interact || output.weapon_next || output.fire_left || output.fire_right) return 3;
    raw.fire_right = true;
    output = mapper.Update(raw);
    if (!output.logs || output.fire_right) return 4;
    raw.utility_modifier = false;
    output = mapper.Update(raw);
    if (output.fire_right || output.reload || output.kick || output.interact) return 5;
    raw = {};
    raw.active = true;
    (void)mapper.Update(raw);
    raw.fire_right = true;
    if (!mapper.Update(raw).fire_right) return 6;
    (void)mapper.Update({}); // focus / menu ownership lost
    if (mapper.Update(raw).fire_right) return 7;
    raw.fire_right = false;
    raw.utility_modifier = false;
    (void)mapper.Update(raw);
    raw.utility_modifier = true;
    raw.turn = {0, 1};
    output = mapper.Update(raw);
    if (!output.equipment_select[0] || output.turn.x != 0 || output.turn.y != 0) return 8;
    if (!mapper.Update(raw).equipment_select[0]) return 9;
    raw.turn = {1, 0}; // crossing sectors is not a second selection
    output = mapper.Update(raw);
    if (!output.equipment_select[0] || output.equipment_select[2]) return 10;
    raw.turn = {};
    (void)mapper.Update(raw);
    raw.turn = {1, 0};
    if (!mapper.Update(raw).equipment_select[2]) return 11;
    raw.utility_modifier = false;
    if (mapper.Update(raw).turn.x != 0) return 12; // no delayed snap after choosing
    raw.turn = {};
    (void)mapper.Update(raw);
    raw.turn = {1, 0};
    if (mapper.Update(raw).turn.x != 1) return 13;
    raw.utility_modifier = true;
    raw.turn = {std::numeric_limits<float>::quiet_NaN(), 1};
    output = mapper.Update(raw);
    for (bool selected : output.equipment_select) if (selected) return 14;
    if (!std::isfinite(output.turn.x) || output.quick_load || output.quick_save) return 15;
    // A modifier held across focus restoration cannot start utility actions.
    (void)mapper.Update({});
    raw.turn = {};
    raw.fire_right = true;
    output = mapper.Update(raw);
    if (output.logs || output.utility_modifier) return 16;
    GameplayUtilityMapper transition;
    raw = {}; raw.active = true;
    (void)transition.Update(raw);
    raw.fire_right = true;
    (void)transition.Update(raw);
    raw.utility_modifier = true;
    if (transition.Update(raw).logs) return 17; // don't reinterpret already held fire
    GameplayUtilityMapper focus_mapper;
    raw = {}; raw.active = true;
    (void)focus_mapper.Update(raw);
    raw.utility_modifier = true; raw.reload = true;
    if (!focus_mapper.Update(raw).focus) return 18;
    raw.utility_modifier = false; raw.reload = false;
    if (!focus_mapper.Update(raw).focus) return 19; // can leave utility and aim/fire
    raw.fire_right = true;
    output = focus_mapper.Update(raw);
    if (!output.focus || !output.fire_right) return 20;
    raw.fire_right = false; raw.utility_modifier = true; raw.reload = true;
    if (focus_mapper.Update(raw).focus) return 21; // deliberate second press exits focus
    if (focus_mapper.Update(raw).focus) return 22; // held press cannot toggle repeatedly
    raw.reload = false; (void)focus_mapper.Update(raw);
    raw.reload = true; (void)focus_mapper.Update(raw);
    (void)focus_mapper.Update({});
    raw = {}; raw.active = true;
    if (focus_mapper.Update(raw).focus) return 23; // context loss releases focus
    GameplayUtilityMapper unavailable;
    raw = {}; raw.active = true;
    (void)unavailable.Update(raw);
    raw.utility_modifier = true; raw.fire_right = true;
    (void)unavailable.Update(raw);
    raw.utility_modifier = false; (void)unavailable.Update(raw);
    raw.digital_available &= ~(1U << 1); raw.fire_right = false;
    if (unavailable.Update(raw).fire_right) return 24;
    raw.digital_available |= 1U << 1; raw.fire_right = true;
    if (unavailable.Update(raw).fire_right) return 25; // unavailable is not a release
    raw.fire_right = false; (void)unavailable.Update(raw);
    raw.fire_right = true;
    if (!unavailable.Update(raw).fire_right) return 26;
    raw.turn_available = false; raw.turn = {};
    (void)unavailable.Update(raw);
    raw.turn_available = true; raw.turn = {1,0};
    if (unavailable.Update(raw).turn.x != 0) return 27;
    // The game context, rather than asynchronous presentation mode, resets
    // focus and held actions even when no inactive presenter poll intervenes.
    raw = {}; raw.active = true;
    (void)unavailable.Update(raw);
    raw.utility_modifier = true; raw.reload = true;
    (void)unavailable.Update(raw);
    raw.utility_modifier = false; raw.reload = false;
    if (!unavailable.Update(raw).focus) return 28;
    raw.kick = true; raw.jump = true;
    (void)unavailable.Update(raw, false); // native menu owns these buttons
    output = unavailable.Update(raw, true);
    if (output.focus || output.kick || output.jump) return 29;
    raw.kick = false; raw.jump = false; (void)unavailable.Update(raw);
    raw.utility_modifier = true; raw.reload = true; (void)unavailable.Update(raw);
    raw.utility_modifier = false; raw.reload = false;
    raw.input_context_generation = 1; raw.fire_left = true;
    output = unavailable.Update(raw);
    if (output.focus || output.fire_left) return 30; // missed inactive presenter polls
    // Optional custom bindings must remain unavailable without disabling
    // other actions or masquerading as releases of their own held inputs.
    raw = {}; raw.active = true; (void)unavailable.Update(raw);
    raw.focus = true; raw.equipment_select[2] = true; (void)unavailable.Update(raw);
    raw.digital_available &= ~((1U << 11) | (1U << 23));
    raw.focus = false; raw.equipment_select[2] = false;
    (void)unavailable.Update(raw);
    raw.digital_available = 0xFFFFFFFFU; raw.focus = true; raw.equipment_select[2] = true;
    output = unavailable.Update(raw);
    if (output.focus || output.equipment_select[2]) return 31;
    std::cout << "Gameplay utility ownership and selection tests passed\n";
    return 0;
}
