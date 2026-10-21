"""Bounded execution of exact transformed CoJ menu bytes; never stages or launches.

Optional --game-directory or COJ_GAME_DIRECTORY locates read-only original PAKs.
Missing originals skip explicitly; a present, mismatched explicit input fails.
Native widget storage/drawing and game settings persistence are model boundaries.
"""
import hashlib
import os
from pathlib import Path
import re
import struct
import sys
import unittest
import zipfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from inspect_java_bytecode import Reader, parse_constant_pool, parse_class, disassemble
from inspect_java_class import parse_class as declarations

CODE_SHA = "f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12"
DATA_SHA = "5eb6ec4856e53226dfabad929522706a21f02d428923a8c9c38804cc2cf11a94"
CLASS_SHA = "2fe3aa28003aa2c3f9539071eefa040e4faedbdd6868fbd8c179ab0230ba6dd8"
SCRIPT_SHA = "995388edb69798167e318189598e451319d31c351b230455d737679608772b1d"
SCRIPT_NAME = "Data/Menu/scr/OptionsGame.scr"
PATCH_PATH = ROOT / "tools/coj_native_settings.py"
# Identity is the real unpatched baseline for the initial RED behavioral run.
if PATCH_PATH.exists():
    import coj_native_settings as patch
else:
    class patch:
        patch_menu_class = staticmethod(lambda data: data)
        patch_menu_script = staticmethod(lambda data: data)

EXPLICIT_GAME = bool(os.environ.get("COJ_GAME_DIRECTORY"))
GAME = Path(os.environ.get("COJ_GAME_DIRECTORY",
    "C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez"))
if "--game-directory" in sys.argv:
    i = sys.argv.index("--game-directory")
    GAME = Path(sys.argv[i + 1])
    EXPLICIT_GAME = True
    del sys.argv[i:i + 2]


def file_sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def originals():
    candidates = (GAME,) if EXPLICIT_GAME else (GAME, ROOT / "work/release-prealpha2/originals")
    for directory in candidates:
        code, data = directory / "code.pak", directory / "Data0.pak"
        if not code.is_file() or not data.is_file():
            continue
        if file_sha(code) != CODE_SHA or file_sha(data) != DATA_SHA:
            if EXPLICIT_GAME:
                raise AssertionError("Explicit code.pak/Data0.pak present but not exact original SHA-256")
            continue
        with zipfile.ZipFile(code) as z:
            menu = z.read("MenuOptionsGame.class")
            option = z.read("UIOptionsControlSelect$OptionInfo.class")
        with zipfile.ZipFile(data) as z:
            script = z.read(SCRIPT_NAME)
        assert hashlib.sha256(menu).hexdigest() == CLASS_SHA
        assert hashlib.sha256(script).hexdigest() == SCRIPT_SHA
        return menu, script, option
    raise unittest.SkipTest("Exact original code.pak/Data0.pak unavailable; native settings behavior not tested")


def widget(kind="UIOptionsControlSelect"):
    return {"kind": kind, "options": [], "selected": None, "text": ""}


class VM:
    """Execute emitted wrappers/helpers and original menu bodies (10k step cap).

    Unsupported opcodes/calls fail. Only the unrelated speech directory scan,
    engine widget operations, parent UI and native settings I/O are stubbed.
    OptionInfo.intValue executes the original shipped implementation.
    """
    def __init__(self, menu, option, missing=(), paused=False):
        self.classes = {}
        for name, data in (("MenuOptionsGame", menu),
                           ("UIOptionsControlSelect$OptionInfo", option)):
            pool, methods = parse_class(data)
            self.classes[name] = pool, {(m.name, m.descriptor): m for m in methods}
        self.widgets = {name: widget() for name in (
            "ID_AUTO_RELOAD", "ID_FULL_AKIMBO", "ID_SELECT_SPEECH", "ID_SUBTITLES",
            "ID_VR_CINEMATICS", "ID_VR_TURNING", "ID_VR_HOLSTER_GUIDES")}
        self.widgets["ID_VR_STATUS"] = widget("UIStatic")
        for name in missing:
            self.widgets.pop(name)
        self.menu = {"kind": "MenuOptionsGame"}
        self.settings = {"bAutoReloadGuns": 0, "bFullAkimbo": 0,
                         "bSubtitles": 1, "sSpeechID": "en"}
        self.events = []
        self.paused = paused
        self.depth = 0
        self.run("<init>")

    def call(self, owner, name, desc, obj, args):
        if owner == "java/lang/String" and name == "equals": return obj == args[0]
        if owner == "GameUserInterface" and name == "OnUICommand": return
        if (owner == "MenuOptionsGame" and name != "ScanForSpeechVersions"
                and (name, desc) in self.classes[owner][1]):
            return self.run(name, desc, args, obj, owner)
        if owner == "UIOptionsControlSelect$OptionInfo" and name == "intValue":
            return self.run(name, desc, args, obj, owner)
        if name == "<init>" and owner == "GameUserInterface": return
        if name == "<init>" and owner == "java/lang/Integer":
            obj["value"] = args[0]; return
        if owner == "java/lang/Integer" and name == "intValue": return obj["value"]
        if owner == "MenuOptionsGame" and name == "ScanForSpeechVersions":
            self.events.append("speech-scan"); return
        if owner == "GameUserInterface" and name in ("OnInit", "Show"):
            self.events.append(name); return
        if owner == "LawmanGame" and name == "GetSettings": return self.settings
        if owner == "LawmanGame" and name == "GetActiveGameModule":
            return {} if self.paused else None
        if owner == "MainMenuModule" and name == "ShowUI":
            self.events.append(("back", args[0])); return
        if owner == "Settings":
            if name == "SaveSettings": self.events.append("save"); return
            if name == "SetSpeechID": obj["sSpeechID"] = args[0]; return
            if name == "GetDefaultSpeechID": return "en"
            if name == "GetDefaultSubtitles": return 1
        if name == "GetElem" and owner in ("MenuOptionsGame", "UIWindow"):
            return self.widgets.get(args[0])
        if owner == "UIOptionsControlSelect" and name == "InitOnOffOption":
            w = self.widgets[args[1]]
            w["options"] = [self.option(0), self.option(1)]
            return w
        if owner in ("UIOptionsControlSelect", "UIOptionsControl"):
            assert obj is not None, "null dropdown dereference"
            if name == "SetLabel": obj["label"] = args[0]; return
            if name == "SetToolTip": obj["tooltip"] = args[0]; return
            if name == "SetCycleOptions": obj["cycle"] = args[0]; return
            if name == "Clear": obj["options"] = []; obj["selected"] = None; return
            if name == "AddOption":
                opt = {"kind": "UIOptionsControlSelect$OptionInfo", "m_cData": args[1], "text": args[0]}
                obj["options"].append(opt)
                if obj["selected"] is None: obj["selected"] = opt
                return opt
            if name == "GetSelected": return obj["selected"]
            if name == "SetSelectionByIntegerData":
                for opt in obj["options"]:
                    if opt["m_cData"]["value"] == args[0]: obj["selected"] = opt; break
                return
            if name == "SetSelectionByStringData":
                obj["selected"] = {"string": args[0]}; return
        if owner == "UIOptionsControlSelect$OptionInfo":
            if name == "boolValue": return obj["m_cData"]["value"] != 0
            if name == "stringValue": return obj["string"]
        if owner == "UIStatic" and name == "SetText":
            assert obj is not None, "null status dereference"
            obj["text"] = args[0]; return
        raise AssertionError("Unmodelled call: " + owner + "." + name + desc)

    @staticmethod
    def option(value):
        return {"kind": "UIOptionsControlSelect$OptionInfo",
                "m_cData": {"kind": "java/lang/Integer", "value": value}}

    def run(self, name, desc="()V", args=(), obj=None, cls="MenuOptionsGame"):
        self.depth += 1
        assert self.depth < 30, "recursive bytecode bound"
        try:
            return self.execute(cls, name, desc, self.menu if obj is None else obj, args)
        finally:
            self.depth -= 1

    def execute(self, cls, name, desc, obj, args):
        pool, methods = self.classes[cls]
        assert (name, desc) in methods, "Missing bytecode method: " + name + desc
        method = methods[name, desc]
        code = method.code
        local = [obj, *args] + [None] * (method.max_locals - 1 - len(args))
        stack, pc = [], 0
        for _ in range(10000):
            assert len(stack) <= method.max_stack, "declared max_stack exceeded"
            at, op = pc, code[pc]
            pc += 1
            if 0x2a <= op <= 0x2d: stack.append(local[op - 0x2a])
            elif 0x1a <= op <= 0x1d: stack.append(local[op - 0x1a])
            elif 0x4b <= op <= 0x4e: local[op - 0x4b] = stack.pop()
            elif 0x3b <= op <= 0x3e: local[op - 0x3b] = stack.pop()
            elif op in (0x15, 0x19): stack.append(local[code[pc]]); pc += 1
            elif op in (0x36, 0x3a): local[code[pc]] = stack.pop(); pc += 1
            elif op == 1: stack.append(None)
            elif 2 <= op <= 8: stack.append(op - 3)
            elif op == 0x59: stack.append(stack[-1])
            elif op == 0x57: stack.pop()
            elif op == 0x60:
                b, a = stack.pop(), stack.pop()
                stack.append(((a + b + 2**31) % 2**32) - 2**31)
            elif op in (0x10, 0x11):
                n = 1 if op == 0x10 else 2
                stack.append(int.from_bytes(code[pc:pc+n], "big", signed=True)); pc += n
            elif op in (0x12, 0x13):
                n = 1 if op == 0x12 else 2
                entry = pool.get(int.from_bytes(code[pc:pc+n], "big")); pc += n
                stack.append(pool.utf8(entry[1]) if entry[0] == "String" else entry[1])
            elif op in (0xb2, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xbb, 0xc0, 0xc1):
                entry = pool.get(int.from_bytes(code[pc:pc+2], "big")); pc += 2
                if op in (0xbb, 0xc0, 0xc1):
                    kind = pool.utf8(entry[1])
                    if op == 0xbb: stack.append({"kind": kind})
                    elif op == 0xc1:
                        value = stack.pop(); stack.append(isinstance(value, dict) and value.get("kind") == kind)
                    else:
                        value = stack.pop()
                        assert value is None or value["kind"] == kind, "bad checkcast"
                        stack.append(value)
                    continue
                owner = pool.describe(entry[1]); nt = pool.get(entry[2])
                key, signature = pool.utf8(nt[1]), pool.utf8(nt[2])
                if op == 0xb2:
                    assert owner == "LawmanGame" and key == "sm_cMenuModule"
                    stack.append({"kind": "MainMenuModule"})
                elif op == 0xb4:
                    target = stack.pop(); assert target is not None, "null getfield"
                    stack.append(target.get(key, None if signature.startswith("L") else 0))
                elif op == 0xb5:
                    value = stack.pop(); target = stack.pop(); assert target is not None
                    target[key] = value
                else:
                    params = re.findall(r"\[*L[^;]+;|\[*[ZBCSIJFD]", signature[1:signature.index(")")])
                    call_args = [stack.pop() for _ in params][::-1]
                    target = None if op == 0xb8 else stack.pop()
                    assert op == 0xb8 or target is not None, "null invocation"
                    result = self.call(owner, key, signature, target, call_args)
                    if not signature.endswith("V"): stack.append(result)
            elif op == 0xaa:
                pc = (pc + 3) & ~3
                default, low, high = struct.unpack_from(">iii", code, pc); pc += 12
                key = stack.pop()
                offset = struct.unpack_from(">i", code, pc + 4 * (key - low))[0] if low <= key <= high else default
                pc = at + offset
            elif op in (*range(0x99, 0xa7), 0xa7, 0xc6, 0xc7):
                offset = int.from_bytes(code[pc:pc+2], "big", signed=True); pc += 2
                if op == 0xa7: take = True
                elif op in (0xc6, 0xc7):
                    value = stack.pop(); take = (value is None) if op == 0xc6 else (value is not None)
                elif op < 0x9f:
                    value = stack.pop()
                    take = {0x99: value == 0, 0x9a: value != 0, 0x9b: value < 0,
                            0x9c: value >= 0, 0x9d: value > 0, 0x9e: value <= 0}[op]
                else:
                    b, a = stack.pop(), stack.pop()
                    if op in (0xa5, 0xa6): take = (a is b) if op == 0xa5 else (a is not b)
                    else: take = {0x9f: a == b, 0xa0: a != b, 0xa1: a < b,
                                  0xa2: a >= b, 0xa3: a > b, 0xa4: a <= b}[op]
                if take: pc = at + offset
            elif op in (0xac, 0xb0):
                result = stack.pop(); assert not stack, "dirty return stack"; return result
            elif op == 0xb1:
                assert not stack, "dirty void return stack"; return
            else: raise AssertionError(f"Unsupported opcode {op:02x} in {name} at {at}")
        raise AssertionError("Unbounded bytecode execution")


def members(data):
    r = Reader(data); r.read(8); pool = parse_constant_pool(r)
    r.read(6); r.read(r.u2() * 2)
    groups = []
    for _ in range(2):
        result = {}
        for _ in range(r.u2()):
            start = r.stream.tell()
            r.u2(); name, desc = pool.utf8(r.u2()), pool.utf8(r.u2())
            for _ in range(r.u2()): r.u2(); r.read(r.u4())
            result[name, desc] = data[start:r.stream.tell()]
        groups.append(result)
    return groups, data[r.stream.tell():]


class NativeSettingsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original, cls.script, cls.option = originals()
        cls.transformed = patch.patch_menu_class(cls.original)

    def vm(self, **kwargs):
        vm = VM(self.transformed, self.option, **kwargs)
        vm.run("OnInit"); vm.run("Show")
        return vm

    def sync(self, vm, cinema=1, turn=1, language=0, guides=0):
        vm.run("cojvrSyncSettings", "(IIII)V", (cinema, turn, guides, language))

    def test_initialized_ok_keeps_native_page_visible(self):
        vm = self.vm()
        # Supply a valid initialized native editor without requiring new helpers:
        # the unpatched OnOK must fail by saving/backing immediately.
        vm.menu["cojvrSettingsReady"] = 1
        for key in ("ID_VR_CINEMATICS", "ID_VR_TURNING", "ID_VR_HOLSTER_GUIDES"):
            vm.widgets[key]["options"] = [vm.option(0), vm.option(1)]
            vm.widgets[key]["selected"] = vm.widgets[key]["options"][1]
        vm.run("OnOK")
        self.assertNotIn("save", vm.events)
        self.assertEqual(vm.menu.get("cojvrSettingsRequest", 0), 1)

    def test_ok_waits_for_bridge_before_native_save(self):
        vm = self.vm()
        self.sync(vm)
        vm.run("OnOK")
        self.assertEqual(vm.menu["cojvrSettingsRequest"], 1)
        self.assertNotIn("save", vm.events)
        self.assertFalse(any(isinstance(e, tuple) for e in vm.events))
        vm.menu["cojvrSettingsRequest"] = 2  # parent consumes before effects
        vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (1, ""))
        self.assertEqual(vm.menu["cojvrSettingsRequest"], 0)
        self.assertEqual(vm.events[-2:], ["save", ("back", 1)])

    def test_accepted_completion_requires_consumed_token_two(self):
        vm = self.vm(); self.sync(vm)
        vm.run("OnOK")
        self.assertEqual(vm.menu["cojvrSettingsRequest"], 1)
        vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (1, ""))
        self.assertNotIn("save", vm.events)
        self.assertEqual(vm.menu["cojvrSettingsRequest"], 1)
        vm.menu["cojvrSettingsRequest"] = 2
        vm.run("OnOK")  # repeated accept cannot downgrade consumed token
        self.assertEqual(vm.menu["cojvrSettingsRequest"], 2)
        vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (1, ""))
        self.assertEqual(vm.menu["cojvrSettingsRequest"], 0)
        self.assertEqual(vm.events[-2:], ["save", ("back", 1)])
        vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (1, ""))
        self.assertEqual(vm.events.count("save"), 1)

    def test_consumed_completion_requires_ready(self):
        vm = self.vm(); self.sync(vm); vm.run("OnOK")
        vm.menu["cojvrSettingsRequest"] = 2
        vm.menu["cojvrSettingsReady"] = 0
        vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (1, ""))
        self.assertNotIn("save", vm.events)

    def test_explicit_conflict_refresh_reloads_authoritative_values(self):
        vm = self.vm(); self.sync(vm, 1, 1, 0); vm.run("OnOK")
        vm.menu["cojvrSettingsRequest"] = 2
        vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (0, "Preferences changed"))
        vm.menu["cojvrSettingsReady"] = 0  # deliberate parent refresh
        self.sync(vm, 0, 0, 1)
        self.assertEqual(vm.menu["cojvrSettingsRequest"], 0)
        self.assertEqual(vm.menu["cojvrSettingsReady"], 1)
        self.assertEqual(vm.run("cojvrCinematicsValue", "()I"), 0)
        self.assertEqual(vm.run("cojvrTurningValue", "()I"), 0)
        self.assertEqual(vm.widgets["ID_VR_STATUS"]["text"], "")
        self.assertEqual(vm.widgets["ID_VR_TURNING"]["label"], "Giro RV")
        self.assertEqual(vm.menu["cojvrSettingsOpenSerial"], 1)
        self.assertNotIn("save", vm.events)

    def test_public_instance_bridge_contract(self):
        info = declarations(self.transformed)
        self.assertEqual((info.major, info.minor), (declarations(self.original).major, declarations(self.original).minor))
        fields = {(f.name, f.descriptor): f.access for f in info.fields}
        for key in (("cojvrSettingsReady", "Z"), ("cojvrSettingsRequest", "I"), ("cojvrSettingsOpenSerial", "I")):
            self.assertEqual(fields[key], 0x0041)  # public volatile, never static
        methods = {(m.name, m.descriptor): m.access for m in info.methods}
        for key in (("cojvrSyncSettings", "(IIII)V"), ("cojvrCinematicsValue", "()I"),
                    ("cojvrTurningValue", "()I"), ("cojvrHolsterGuidesValue", "()I"), ("cojvrCompleteSettings", "(ZLjava/lang/String;)V")):
            self.assertEqual(methods[key], 1)

    def test_unrelated_members_and_attributes_are_byte_identical(self):
        (old_fields, old_methods), old_tail = members(self.original)
        (new_fields, new_methods), new_tail = members(self.transformed)
        for key, value in old_fields.items(): self.assertEqual(new_fields[key], value)
        changed = {"OnInit", "Show", "OnDefault", "OnOK", "Back"}
        for key, value in old_methods.items():
            if key[0] not in changed: self.assertEqual(new_methods[key], value, key)
        self.assertEqual(old_tail, new_tail)
        old_pool, old = parse_class(self.original)
        new_pool, new = parse_class(self.transformed)
        self.assertEqual(new_pool.entries[:len(old_pool.entries)], old_pool.entries)
        indexed = {(m.name, m.descriptor): m for m in new}
        for m in old:
            if m.name in changed:
                retained = indexed["cojvrOriginal" + m.name, m.descriptor]
                self.assertEqual(retained.code, m.code)
                self.assertEqual(retained.max_locals, m.max_locals)
        self.assertGreater(len(self.transformed), len(self.original))

    def test_show_revokes_request_increments_serial_preserves_native_values(self):
        vm = self.vm()
        self.assertEqual(vm.menu["cojvrSettingsOpenSerial"], 1)
        self.assertEqual(vm.menu["cojvrSettingsReady"], 0)
        self.sync(vm, 1, 0)
        vm.run("OnOK")
        vm.run("Show")
        self.assertEqual(vm.menu["cojvrSettingsOpenSerial"], 2)
        self.assertEqual(vm.menu["cojvrSettingsRequest"], 0)
        self.assertEqual(vm.menu["cojvrSettingsReady"], 0)
        self.assertEqual(vm.widgets["ID_AUTO_RELOAD"]["selected"]["m_cData"]["value"], 0)
        self.assertEqual(vm.widgets["ID_SUBTITLES"]["selected"]["m_cData"]["value"], 1)
        vm.menu["cojvrSettingsOpenSerial"] = 2**31 - 1
        vm.run("Show")
        self.assertEqual(vm.menu["cojvrSettingsOpenSerial"], -(2**31))

    def test_all_translations_values_and_once_per_show(self):
        for cinema in (0, 1):
            for turn in (0, 1):
                for language in (0, 1):
                    with self.subTest(cinema=cinema, turn=turn, language=language):
                        vm = self.vm()
                        self.sync(vm, cinema, turn, language)
                        self.assertEqual(vm.menu["cojvrSettingsReady"], 1)
                        self.assertEqual(vm.run("cojvrCinematicsValue", "()I"), cinema)
                        self.assertEqual(vm.run("cojvrTurningValue", "()I"), turn)
                        cine, turning = vm.widgets["ID_VR_CINEMATICS"], vm.widgets["ID_VR_TURNING"]
                        self.assertEqual(cine["label"], "Cinemáticas RV" if language else "VR cinematics")
                        self.assertEqual(turning["label"], "Giro RV" if language else "VR turning")
                        self.assertEqual([o["text"] for o in cine["options"]],
                                         ["Teatro", "Inmersivo"] if language else ["Theater", "Immersive"])
                        self.assertEqual([o["text"] for o in turning["options"]],
                                         ["Suave", "Por pasos"] if language else ["Smooth", "Snap"])
                        self.assertTrue(cine["cycle"]); self.assertTrue(turning["cycle"])
                        self.assertTrue(cine["tooltip"]); self.assertTrue(turning["tooltip"])
                        cine["selected"] = cine["options"][1 - cinema]
                        self.sync(vm, cinema, turn, 1 - language)
                        self.assertEqual(vm.run("cojvrCinematicsValue", "()I"), 1 - cinema)
                        self.assertEqual(len(cine["options"]), 2)
                        vm.run("Show"); self.sync(vm, cinema, turn, 1 - language)
                        self.assertEqual(vm.run("cojvrCinematicsValue", "()I"), cinema)
                        self.assertEqual(len(cine["options"]), 2)

    def test_holster_guides_localization_edits_reopen_and_cancel(self):
        for language in (0, 1):
            for guides in (0, 1):
                vm = self.vm(); self.sync(vm, language=language, guides=guides)
                row = vm.widgets["ID_VR_HOLSTER_GUIDES"]
                self.assertEqual(vm.run("cojvrHolsterGuidesValue", "()I"), guides)
                self.assertEqual(row["label"], "Comodidad RV: guías de armas" if language else "VR Comfort: weapon guides")
                self.assertEqual([o["text"] for o in row["options"]], ["No", "Sí"] if language else ["Off", "On"])
                self.assertTrue(row["cycle"]); self.assertTrue(row["tooltip"])
                row["selected"] = row["options"][1-guides]
                self.sync(vm, language=language, guides=guides)
                self.assertEqual(vm.run("cojvrHolsterGuidesValue", "()I"), 1-guides)
                vm.run("OnOK"); vm.run("OnCancel")
                self.assertNotIn("save", vm.events)
                vm.run("Show"); self.sync(vm, language=language, guides=guides)
                self.assertEqual(vm.run("cojvrHolsterGuidesValue", "()I"), guides)

    def test_rejected_save_stays_visible_and_retry_commits_native_values(self):
        for paused in (False, True):
            with self.subTest(paused=paused):
                vm = self.vm(paused=paused); self.sync(vm)
                vm.run("OnOK")
                vm.menu["cojvrSettingsRequest"] = 2
                vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (0, "Could not save / No se pudo guardar"))
                self.assertEqual(vm.menu["cojvrSettingsRequest"], 0)
                self.assertEqual(vm.menu["cojvrSettingsReady"], 1)
                self.assertEqual(vm.widgets["ID_VR_STATUS"]["text"], "Could not save / No se pudo guardar")
                self.assertNotIn("save", vm.events)
                self.assertEqual(vm.run("cojvrCinematicsValue", "()I"), 1)
                vm.widgets["ID_AUTO_RELOAD"]["selected"] = vm.widgets["ID_AUTO_RELOAD"]["options"][1]
                vm.run("OnOK")
                vm.menu["cojvrSettingsRequest"] = 2
                vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (1, ""))
                self.assertEqual(vm.settings["bAutoReloadGuns"], True)
                self.assertEqual(vm.events[-2:], ["save", ("back", 6 if paused else 1)])
                self.assertEqual(vm.menu["cojvrSettingsReady"], 0)
                vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (1, ""))
                self.assertEqual(vm.events.count("save"), 1)

    def test_cancel_escape_and_back_never_commit_pending_proposal(self):
        for method in ("OnCancel", "OnKeyEscape", "Back"):
            for paused in (False, True):
                with self.subTest(method=method, paused=paused):
                    vm = self.vm(paused=paused); self.sync(vm)
                    vm.run("OnOK")
                    vm.menu["cojvrSettingsRequest"] = 2
                    vm.run(method)
                    self.assertEqual(vm.menu["cojvrSettingsRequest"], 0)
                    self.assertEqual(vm.menu["cojvrSettingsReady"], 0)
                    vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (1, ""))
                    self.assertNotIn("save", vm.events)
                    self.assertEqual(vm.events[-1], ("back", 6 if paused else 1))

    def test_defaults_are_local_theater_smooth_and_clear_status_request(self):
        vm = self.vm(); self.sync(vm, guides=1)
        vm.run("OnOK")
        vm.widgets["ID_VR_STATUS"]["text"] = "old error"
        vm.run("OnDefault")
        self.assertEqual(vm.menu["cojvrSettingsRequest"], 0)
        self.assertEqual(vm.run("cojvrCinematicsValue", "()I"), 0)
        self.assertEqual(vm.run("cojvrTurningValue", "()I"), 0)
        self.assertEqual(vm.widgets["ID_VR_STATUS"]["text"], "")
        self.assertEqual(vm.run("cojvrHolsterGuidesValue", "()I"), 0)
        self.assertEqual(vm.widgets["ID_AUTO_RELOAD"]["selected"]["m_cData"]["value"], 1)
        self.assertNotIn("save", vm.events)

    def test_missing_or_wrong_typed_dropdowns_fall_back_without_crash(self):
        for id_ in ("ID_VR_CINEMATICS", "ID_VR_TURNING", "ID_VR_HOLSTER_GUIDES"):
            for wrong_type in (False, True):
                with self.subTest(id=id_, wrong_type=wrong_type):
                    vm = VM(self.transformed, self.option, missing=(id_,))
                    if wrong_type: vm.widgets[id_] = widget("UIStatic")
                    vm.run("OnInit"); vm.run("Show"); self.sync(vm)
                    self.assertEqual(vm.menu["cojvrSettingsReady"], 0)
                    self.assertEqual(vm.run("cojvrCinematicsValue" if id_.endswith("CINEMATICS") else "cojvrHolsterGuidesValue" if id_.endswith("GUIDES") else "cojvrTurningValue", "()I"), -1)
                    vm.run("OnDefault"); vm.run("OnOK")
                    self.assertEqual(vm.events[-2:], ["save", ("back", 1)])
                    self.assertEqual(vm.menu["cojvrSettingsRequest"], 0)

    def test_missing_status_and_null_status_are_safe(self):
        for missing in ((), ("ID_VR_STATUS",)):
            with self.subTest(missing=missing):
                vm = self.vm(missing=missing); self.sync(vm)
                vm.run("OnOK")
                vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (0, None))
                self.assertNotIn("save", vm.events)
                if not missing: self.assertEqual(vm.widgets["ID_VR_STATUS"]["text"], "")
                vm.run("OnOK")
                vm.menu["cojvrSettingsRequest"] = 2
                vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (1, ""))
                self.assertEqual(vm.events.count("save"), 1)

    def test_invalid_or_missing_selections_return_minus_one_and_native_fallback(self):
        for id_, getter in (("ID_VR_CINEMATICS", "cojvrCinematicsValue"), ("ID_VR_TURNING", "cojvrTurningValue"), ("ID_VR_HOLSTER_GUIDES", "cojvrHolsterGuidesValue")):
            for value in (None, -1, 2, 99, "not an integer"):
                with self.subTest(id=id_, value=value):
                    vm = self.vm(); self.sync(vm)
                    opt = vm.option(value)
                    if isinstance(value, str): opt["m_cData"] = value
                    vm.widgets[id_]["selected"] = None if value is None else opt
                    self.assertEqual(vm.run(getter, "()I"), -1)
                    vm.run("OnOK")
                    self.assertEqual(vm.events[-2:], ["save", ("back", 1)])
                    self.assertEqual(vm.menu["cojvrSettingsRequest"], 0)

    def test_invalid_sync_inputs_cannot_publish_ready(self):
        for args in ((-1, 0, 0), (2, 0, 0), (0, -1, 0), (0, 2, 0), (0, 0, -1), (0, 0, 2), (0, 0, 0, -1), (0, 0, 0, 2)):
            with self.subTest(args=args):
                vm = self.vm(); self.sync(vm, *args)
                self.assertEqual(vm.menu["cojvrSettingsReady"], 0)
                self.assertEqual(vm.menu["cojvrSettingsRequest"], 0)
                self.assertEqual(vm.run("cojvrCinematicsValue", "()I"), -1)

    def test_unsynchronized_ok_retains_native_save_back(self):
        vm = self.vm(); vm.run("OnOK")
        self.assertEqual(vm.events[-2:], ["save", ("back", 1)])
        self.assertEqual(vm.menu["cojvrSettingsRequest"], 0)

    def test_original_native_command_dispatch_reaches_wrappers(self):
        vm = self.vm(); self.sync(vm)
        vm.run("OnUICommand", "(LUIWindow;I)V", ({"m_sId": "ID_OK"}, 0))
        self.assertEqual(vm.menu["cojvrSettingsRequest"], 1)
        self.assertNotIn("save", vm.events)
        vm.run("OnUICommand", "(LUIWindow;I)V", ({"m_sId": "ID_DEFAULT"}, 0))
        self.assertEqual(vm.menu["cojvrSettingsRequest"], 0)
        self.assertEqual(vm.run("cojvrCinematicsValue", "()I"), 0)
        vm.run("OnUICommand", "(LUIWindow;I)V", ({"m_sId": "ID_CANCEL"}, 0))
        self.assertNotIn("save", vm.events)
        self.assertEqual(vm.events[-1], ("back", 1))

    def test_new_bytecode_branches_target_instruction_boundaries(self):
        pool, methods = parse_class(self.transformed)
        changed = {"OnInit", "Show", "OnDefault", "OnOK", "Back"}
        for m in methods:
            if not (m.name.startswith("cojvr") or m.name in changed): continue
            with self.subTest(method=m.name):
                positions = {int(line.split(":", 1)[0]) for line in disassemble(m.code, pool)}
                self.assertTrue(positions)
                for at in positions:
                    op = m.code[at]
                    if 0x99 <= op <= 0xa7 or op in (0xc6, 0xc7):
                        target = at + int.from_bytes(m.code[at+1:at+3], "big", signed=True)
                        self.assertIn(target, positions)

    def test_wrong_typed_status_and_all_new_resources_absent_are_safe(self):
        for absent in (False, True):
            with self.subTest(absent=absent):
                vm = VM(self.transformed, self.option,
                        missing=("ID_VR_STATUS", "ID_VR_CINEMATICS", "ID_VR_TURNING", "ID_VR_HOLSTER_GUIDES") if absent else ())
                if not absent: vm.widgets["ID_VR_STATUS"] = widget("UIWindow")
                vm.run("OnInit"); vm.run("Show"); self.sync(vm)
                vm.run("cojvrCompleteSettings", "(ZLjava/lang/String;)V", (0, "Save failed"))
                vm.run("OnDefault"); vm.run("OnOK")
                if absent:
                    self.assertEqual(vm.events[-2:], ["save", ("back", 1)])
                else:
                    self.assertNotIn("save", vm.events)
                    self.assertEqual(vm.menu["cojvrSettingsRequest"], 1)

    def test_unknown_and_double_patch_rejected(self):
        for function, original in ((patch.patch_menu_class, self.original), (patch.patch_menu_script, self.script)):
            with self.subTest(function=function.__name__):
                for invalid in (b"", original[:-1], original + b"x", function(original)):
                    with self.assertRaisesRegex(ValueError, "SHA-256"):
                        function(invalid)
                self.assertEqual(function(original), function(original))

    def test_resource_augmentation_retains_every_original_byte_except_frame_height(self):
        out = patch.patch_menu_script(self.script)
        for id_, y in ((b"ID_VR_CINEMATICS", b"0.30681818"), (b"ID_VR_TURNING", b"0.37954545"), (b"ID_VR_HOLSTER_GUIDES", b"0.45227273")):
            self.assertEqual(out.count(b'"' + id_ + b'"'), 1)
            a, b = patch._block(out, b"MenuButton", id_)
            row = out[a:b].replace(id_, b"ID_SUBTITLES").replace(y, b"0.23409091")
            c, d = patch._block(self.script, b"MenuButton", b"ID_SUBTITLES")
            self.assertEqual(row, self.script[c:d])
        a, b = patch._block(out, b"Static", b"ID_VR_STATUS")
        status = out[a:b]
        for prop in (b"CanFocus(0)", b"Mouse(0)", b"Keyboard(0)", b"Joy(0)", b'Font("MenuSmall")', b"FontScale(0.5)"):
            self.assertIn(prop, status)
        self.assertNotIn(b"ForceTextLowerCase", status)
        insertion, _ = patch._block(self.script, b"Window", b"ID_LIST_FRAME")
        frame, _ = patch._block(out, b"Window", b"ID_LIST_FRAME")
        restored = out[:insertion] + out[frame:]
        restored = restored.replace(b"LogicSize(0.5125,0.49833333)", b"LogicSize(0.5125,0.29166666)")
        self.assertEqual(restored, self.script)
        self.assertEqual(out.count(b"{"), out.count(b"}"))
        # Measured reference frame .13833334 + y*.73333335; absolute row height.
        row_bottom = .13833334 + .45227273 * .73333335 + .05
        status_top = .13833334 + .525 * .73333335
        status_bottom = status_top + .06
        self.assertLess(row_bottom, status_top)
        self.assertLess(status_bottom, .10166667 + .49833333)
        self.assertLess(.10166667 + .49833333, .87166667)


if __name__ == "__main__":
    unittest.main(verbosity=2)
