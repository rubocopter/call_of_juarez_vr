"""Validate the deployed manifest/binding and runtime lookup boundary."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / "assets/openvr/actions.json").read_text(encoding="utf-8"))
binding = json.loads((root / "assets/openvr/bindings/psvr2_sense.json").read_text(encoding="utf-8"))
actions = {action["name"]: action for action in manifest["actions"]}
assert len(actions) == len(manifest["actions"]), "duplicate manifest action"
for side in ("left", "right"):
    haptic = f"/actions/global/out/haptic_{side}"
    assert actions.get(haptic) == {"name": haptic, "type": "vibration", "requirement": "optional"}, \
        f"missing optional {side} haptic output"
    assert {"output": haptic, "path": f"/user/hand/{side}/output/haptic"} in \
        binding["bindings"]["/actions/global"]["haptics"], "haptics must use actual output path"
    name = f"/actions/global/in/{side}_hand_skeleton"
    assert name in actions, f"missing optional {side} finger skeleton action"
    assert actions[name] == {"name": name, "type": "skeleton",
                             "skeleton": f"/skeleton/hand/{side}", "requirement": "optional"}
    skeletons = binding["bindings"]["/actions/global"]["skeleton"]
    assert {"output": name, "path": f"/user/hand/{side}/input/skeleton/{side}"} in skeletons, \
        f"{side} skeleton must bind to the actual Sense skeletal input"
runtime = (root / "src/runtime/openvr_runtime.cpp").read_text(encoding="utf-8")
lookups = set(re.findall(r'"(/actions/(?:global|gameplay)/(?:in|out)/[^\"]+)"', runtime))
assert set(actions) == lookups, f"manifest/runtime mismatch: {set(actions) ^ lookups}"

for locale in manifest["localization"]:
    assert set(actions) <= set(locale), f"missing labels in {locale['language_tag']}"
    assert all("\u00c3" not in value and "\ufffd" not in value
               for value in locale.values()), "damaged localization encoding"

def output_paths(value):
    if isinstance(value, dict):
        for key, child in value.items():
            if key == "output":
                yield child
            else:
                yield from output_paths(child)
    elif isinstance(value, list):
        for child in value:
            yield from output_paths(child)

assert set(output_paths(binding)) <= set(actions), "binding points to undeclared action"
routes = {}
for action_set in binding["bindings"].values():
    for source in action_set["sources"]:
        for output in output_paths(source["inputs"]):
            routes.setdefault(source["path"], set()).add(output)

expected = {
    "/user/hand/right/input/options": {"/actions/global/in/pause"},
    "/user/hand/right/input/circle": {"/actions/global/in/ui_back", "/actions/gameplay/in/alternate_fire"},
    "/user/hand/left/input/l1": {"/actions/global/in/ui_pointer_left", "/actions/gameplay/in/interact"},
    "/user/hand/left/input/triangle": {"/actions/gameplay/in/weapon_radial"},
    "/user/hand/right/input/r1": {"/actions/global/in/ui_pointer_right", "/actions/gameplay/in/kick"},
    "/user/hand/left/input/left_stick": {"/actions/gameplay/in/move", "/actions/gameplay/in/crouch"},
    "/user/hand/right/input/right_stick": {"/actions/gameplay/in/turn", "/actions/gameplay/in/focus"},
    "/user/hand/right/input/cross": {"/actions/gameplay/in/jump", "/actions/global/in/ui_accept"},
    "/user/hand/left/input/square": {"/actions/gameplay/in/reload"},
    "/user/hand/left/input/l2": {"/actions/gameplay/in/fire_left", "/actions/global/in/ui_select_left"},
    "/user/hand/right/input/r2": {"/actions/gameplay/in/fire_right", "/actions/global/in/ui_select_right"},
    "/user/hand/left/input/create": {"/actions/global/in/recenter"},
}
for path, outputs in expected.items():
    assert routes.get(path) == outputs, f"conflicting default route at {path}: {routes.get(path)}"
assert not {f"/actions/gameplay/in/{name}" for name in (
    "quick_load", "quick_save", "logs", "weapon_next", "weapon_previous", "lean_left", "lean_right", "walk", "run"
)} & set(output_paths(binding)), "auxiliary native controls must have no default Sense binding"
assert all(not action_set["chords"] for action_set in binding["bindings"].values()), "no default chords"
print("OpenVR action assets and runtime lookup boundary passed")
