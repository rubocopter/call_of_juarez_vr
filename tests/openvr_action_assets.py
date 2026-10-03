"""Validate the deployed manifest/binding and runtime lookup boundary."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / "assets/openvr/actions.json").read_text(encoding="utf-8"))
binding = json.loads((root / "assets/openvr/bindings/psvr2_sense.json").read_text(encoding="utf-8"))
actions = {action["name"]: action for action in manifest["actions"]}
assert len(actions) == len(manifest["actions"]), "duplicate manifest action"
runtime = (root / "src/runtime/openvr_runtime.cpp").read_text(encoding="utf-8")
lookups = set(re.findall(r'"(/actions/(?:global|gameplay)/in/[^\"]+)"', runtime))
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
    "/user/hand/right/input/circle": {"/actions/global/in/ui_back", "/actions/gameplay/in/kick"},
    "/user/hand/left/input/l1": {"/actions/global/in/ui_pointer_left", "/actions/gameplay/in/interact"},
    "/user/hand/left/input/triangle": {"/actions/gameplay/in/utility_modifier"},
}
for path, outputs in expected.items():
    assert routes.get(path) == outputs, f"conflicting default route at {path}: {routes.get(path)}"
assert not {"/actions/gameplay/in/quick_load", "/actions/gameplay/in/quick_save"} & set(output_paths(binding)), \
    "save/load must remain deliberate custom bindings or native pause-menu actions"
print("OpenVR action assets and runtime lookup boundary passed")
