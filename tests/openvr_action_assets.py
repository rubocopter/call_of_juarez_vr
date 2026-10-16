"""Validate the deployed manifest/binding and runtime lookup boundary."""
from collections import Counter
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
route_counts = Counter()
for action_set in binding["bindings"].values():
    for source in action_set["sources"]:
        for output in output_paths(source["inputs"]):
            routes.setdefault(source["path"], set()).add(output)
            route_counts[(source["path"], output)] += 1

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
assert set(routes) == set(expected), f"unexpected Sense input paths: {set(routes) - set(expected)}"
assert all(count == 1 for count in route_counts.values()), \
    f"duplicated Sense action routes: {[route for route, count in route_counts.items() if count != 1]}"
assert not {f"/actions/gameplay/in/{name}" for name in (
    "quick_load", "quick_save", "logs", "weapon_next", "weapon_previous", "lean_left", "lean_right", "walk", "run"
)} & set(output_paths(binding)), "auxiliary native controls must have no default Sense binding"
assert all(not action_set["chords"] for action_set in binding["bindings"].values()), "no default chords"
print("OpenVR action assets and runtime lookup boundary passed")

# Additional layouts are explicit experimental assets; Sense routes above stay strict.
expected_controllers = {"playstation_vr2_sense", "oculus_touch", "knuckles",
                        "vive_controller", "holographic_controller"}
defaults = manifest["default_bindings"]
assert {d["controller_type"] for d in defaults} == expected_controllers
assert len(defaults) == len(expected_controllers)
assert len({d["binding_url"].casefold() for d in defaults}) == len(defaults)
core = {f"/actions/gameplay/in/{n}" for n in (
    "move", "turn", "fire_left", "fire_right", "jump", "reload", "weapon_radial",
    "interact", "crouch", "focus", "kick", "alternate_fire")}
core |= {f"/actions/global/in/{n}" for n in (
    "recenter", "pause", "ui_accept", "ui_back", "ui_select_left", "ui_select_right",
    "ui_pointer_left", "ui_pointer_right")}
# Paths/capabilities from Valve-distributed SteamVR drivers (touch_profile,
# index_controller_profile, vive_controller_profile) and Valve's tutorial WMR
# binding + resources/config/vrcompositor_bindings_holographic.json. Dpad format:
# ValveSoftware/steamvr_unity_plugin Assets/SteamVR/Input/ExampleJSON/
# bindings_holographic_controller.json. No reserved system buttons.
controls = {
    "oculus_touch": {"joystick", "trigger", "grip", "x", "y", "a", "b"},
    "knuckles": {"thumbstick", "trigger", "grip", "a", "b"},
    "vive_controller": {"trackpad", "trigger", "grip", "application_menu"},
    "holographic_controller": {"joystick", "trackpad", "trigger", "grip", "application_menu"},
}
for default in defaults:
    url = default["binding_url"]
    assert re.fullmatch(r"bindings/[a-z0-9_]+\.json", url), url
    config = json.loads((root / "assets/openvr" / url).read_text(encoding="utf-8"))
    kind = default["controller_type"]
    assert config["controller_type"] == kind
    if kind == "playstation_vr2_sense":
        continue
    assert "experimental" in config["description"].lower()
    assert core <= set(output_paths(config)), (kind, core - set(output_paths(config)))
    assert set(output_paths(config)) <= set(actions)
    for side in ("left", "right"):
        global_set = config["bindings"]["/actions/global"]
        assert {"output": f"/actions/global/out/haptic_{side}",
                "path": f"/user/hand/{side}/output/haptic"} in global_set["haptics"]
        for semantic in ("grip", "aim"):
            poses = [x for x in global_set["poses"] if x["output"] ==
                     f"/actions/global/in/{side}_hand_{semantic}_pose"]
            assert len(poses) == 1
            assert poses[0]["path"] == f"/user/hand/{side}/pose/" + (
                "raw" if kind == "holographic_controller" and semantic == "grip" else
                "handgrip" if semantic == "grip" else "tip")
    for set_name, action_set in config["bindings"].items():
        assert not action_set["chords"], "defaults avoid chord/action overlap"
        seen = Counter()
        for source in action_set["sources"]:
            match = re.fullmatch(r"/user/hand/(left|right)/input/(\w+)", source["path"])
            assert match and match[2] in controls[kind], (kind, source["path"])
            if kind == "oculus_touch" and match[2] in {"a", "b", "x", "y"}:
                assert match[1] == ("left" if match[2] in {"x", "y"} else "right")
            if source["mode"] == "dpad":
                assert source["parameters"]["sub_mode"] == "click"
                assert match[2] in {"joystick", "thumbstick", "trackpad"}
            for component, target in source["inputs"].items():
                assert target["output"].startswith(set_name + "/")
                seen[(source["path"], source["mode"], component, target["output"])] += 1
        assert all(n == 1 for n in seen.values()), (kind, seen)
for locale in manifest["localization"]:
    assert "Create" not in locale["/actions/global/in/recenter"]
print("Experimental controller profiles passed (physical validation remains pending)")
