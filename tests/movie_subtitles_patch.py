"""Execute exact movie subtitle bytecode with bounded host native stubs.

Reads original assets only. No staging, game startup, or original assets embedded.
"""
import hashlib
import os
from pathlib import Path
import re
import sys
import unittest
import zipfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from inspect_java_bytecode import Reader, parse_constant_pool, parse_class

GAME = Path(os.environ.get("COJ_GAME_DIRECTORY",
    "C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez"))
PAK_SHA = "f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12"
CLASS_SHA = "80bdbbe228fa06eb8a3424f1dfc72ee42c42e9eefecdae0beaf41acd235330aa"
CLASS_NAME = "LawmanModule.class"
KEY, ID, MOVIE = "cojvrMovieSubtitleKey", "cojvrMovieSubtitleId", "cojvrMovieSubtitleMovie"
TARGETS = {("OnShowSubtitle", "(Ljava/lang/String;I)V"): 681,
           ("OnHideSubtitle", "(I)V"): 41,
           ("MovieDestroySubtitles", "()V"): 38,
           ("OnStartMovie", "(LMovie;)V"): 45,
           ("OnEndMovie", "(LMovie;)V"): 54}

# An identity transformer supplies the genuine unpatched RED behavior.
if (ROOT / "tools/coj_movie_subtitles.py").exists():
    from coj_movie_subtitles import patch_movie_subtitles
else:
    def patch_movie_subtitles(data):
        return data


def original_class():
    present = False
    for archive in (GAME / "code.pak", GAME / "code.cojvr-backup.pak"):
        if not archive.is_file():
            continue
        present = True
        if hashlib.sha256(archive.read_bytes()).hexdigest() != PAK_SHA:
            continue
        with zipfile.ZipFile(archive) as source:
            data = source.read(CLASS_NAME)
        assert hashlib.sha256(data).hexdigest() == CLASS_SHA
        return data
    if present:
        raise AssertionError("Present archives do not include the exact original code.pak SHA-256")
    raise unittest.SkipTest("Original code.pak unavailable")


def structure(data):
    r = Reader(data); r.read(8); pool = parse_constant_pool(r)
    cp_end = r.stream.tell()
    start = cp_end; r.read(6); r.read(r.u2() * 2)
    header = data[start:r.stream.tell()]
    groups = []
    for _ in range(2):
        members = {}
        for _ in range(r.u2()):
            start = r.stream.tell()
            access = r.u2(); name, desc = pool.utf8(r.u2()), pool.utf8(r.u2())
            attrs = []
            for _ in range(r.u2()):
                index = r.u2(); attrs.append((pool.utf8(index), r.read(r.u4())))
            members[name, desc] = (access, attrs, data[start:r.stream.tell()])
        groups.append(members)
    return pool, cp_end, header, groups[0], groups[1], data[r.stream.tell():]


def code_parts(member):
    payload = dict(member[1])["Code"]
    r = Reader(payload); stack, locals_ = r.u2(), r.u2(); code = r.read(r.u4())
    return stack, locals_, code, payload[r.stream.tell():]


def line_numbers(tail):
    r = Reader(tail)
    assert r.u2() == 0 and r.u2() == 1  # exact original handlers/Code attribute count
    r.u2(); table = Reader(r.read(r.u4()))
    result = [(table.u2(), table.u2()) for _ in range(table.u2())]
    assert r.stream.tell() == len(tail)
    return result


def sprite():
    return {"visible": False}


class VM:
    """Execute original target bodies and emitted suffixes; unknown calls fail.

    Native sprite geometry/drawing, player callbacks and parent movie operations
    are stubs. Original control flow, normalization, IDs and cache writes execute.
    """
    def __init__(self, data, *, child=True, parent=True, player=True, belt=False, enabled=True):
        self.pool, methods = parse_class(data)
        self.methods = {(m.name, m.descriptor): m for m in methods}
        self.events = []
        self.logs = {}
        self.player = {} if player else None
        self.settings = {"bSubtitles": enabled}
        self.extent = {"fY": 16.0}
        self.module = {"cSubtitle": sprite() if parent else None,
                       "cSubtitleText": sprite() if child else None,
                       "cBlack2": {"visible": belt}, "m_bShowSubtitleOnMovieBelt": belt,
                       "m_cMoviePlaying": {"bUseLevelTrack": True}, "bMovieSubtitles": True,
                       "iSubtitleNumber": -1}
        self.fail_call = None
        self.reentrant = {}

    def call(self, owner, name, desc, obj, args):
        if self.fail_call == (owner, name):
            raise RuntimeError("native callback failure")
        callback = self.reentrant.pop((owner, name), None)
        if callback is not None: callback()
        if owner == "java/lang/String" and name == "indexOf":
            return obj.find(chr(args[0]))
        if owner == "java/lang/StringBuffer":
            if name == "<init>": obj["text"] = ""; return
            if name == "append": obj["text"] += args[0]; return obj
            if name == "toString": return obj["text"]
        if owner == "LawmanModule":
            if name == "GetLogsManager": return self.logs
            if name == "GetMainPlayer": return self.player
            if name == "GetScreenWidth": return 1280
            if name == "GetScreenHeight": return 720
            if name == "CreateObject":
                assert args == ["Sprite"]
                self.events.append((owner, name)); return sprite()
            if (name, desc) in self.methods and name in ("MovieDestroySubtitles",):
                self.run(name, desc, args); return
            if name in ("RemoveSprite", "MovieCreateView", "MovieCreateSubtitles",
                        "MovieDestroyView", "RefreshControllerState"):
                self.events.append((owner, name)); return
        if owner == "LawmanGame" and name == "GetSettings": return self.settings
        if owner == "Vector" and name == "GiveTemp":
            return dict(zip(("fX", "fY", "fZ"), args))
        if owner == "Rect" and name == "<init>": return
        if owner == "Fonts" and name == "GetProperFontScale": return 1.0
        if owner == "LogsManager" and name == "AddDialog":
            self.events.append((name, args[0])); return
        if owner == "Being" and name in ("OnShowSubtitle", "OnHideSubtitle", "OnStartMovie", "OnEndMovie"):
            self.events.append((owner, name)); return
        if owner == "Module" and name == "MovieDestroySubtitles":
            self.events.append((owner, name)); return
        if owner == "Sprite":
            if name == "IsVisible": return obj["visible"]
            if name == "GetPosVolatile": return {"fX": 0.0, "fY": 700.0, "fZ": 0.0}
            if name == "AddTexture": self.events.append((owner, name)); return 1
            if name in ("SetTexture", "SetFont"):
                self.events.append((owner, name)); return True
            if name in ("Show", "Hide"): obj["visible"] = name == "Show"
            elif name == "DisplayText": obj["text"] = args[0]
            elif name == "GetTextExtent": args[1]["fY"] = 16.0; return True
            elif name not in ("SetSize", "SetPos", "MoveTop", "SetTextPosFlags", "delete",
                              "SetAlpha", "SetName", "SetNoTexture", "ProcessMouse", "SetTextColor",
                              "AddChild", "SetFontScale"):
                raise AssertionError("Unmodelled sprite call " + name)
            self.events.append((owner, name)); return
        raise AssertionError("Unmodelled call " + owner + "." + name + desc)

    def run(self, name, desc="()V", args=()):
        method = self.methods[name, desc]; code = method.code
        local = [self.module, *args] + [None] * (method.max_locals - 1 - len(args))
        stack, pc = [], 0
        for _ in range(3000):
            assert len(stack) <= method.max_stack
            at, op = pc, code[pc]; pc += 1
            if op == 0x00: pass
            elif 0x2a <= op <= 0x2d: stack.append(local[op - 0x2a])
            elif 0x1a <= op <= 0x1d: stack.append(local[op - 0x1a])
            elif 0x22 <= op <= 0x25: stack.append(local[op - 0x22])
            elif 0x4b <= op <= 0x4e: local[op - 0x4b] = stack.pop()
            elif 0x43 <= op <= 0x46: local[op - 0x43] = stack.pop()
            elif op in (0x15, 0x17, 0x19): stack.append(local[code[pc]]); pc += 1
            elif op in (0x36, 0x38, 0x3a): local[code[pc]] = stack.pop(); pc += 1
            elif op == 1: stack.append(None)
            elif 2 <= op <= 8: stack.append(op - 3)
            elif 0xb <= op <= 0xd: stack.append(float(op - 0xb))
            elif op == 0x59: stack.append(stack[-1])
            elif op == 0x57: stack.pop()
            elif op in (0x62, 0x66, 0x6a, 0x6e):
                b, a = stack.pop(), stack.pop()
                stack.append({0x62: lambda: a+b, 0x66: lambda: a-b,
                              0x6a: lambda: a*b, 0x6e: lambda: a/b}[op]())
            elif op in (0x86, 0x8b): stack.append(float(stack.pop()) if op == 0x86 else int(stack.pop()))
            elif op == 0x95:
                b, a = stack.pop(), stack.pop(); stack.append((a > b) - (a < b))
            elif op in (0x10, 0x11):
                n = 1 if op == 0x10 else 2
                stack.append(int.from_bytes(code[pc:pc+n], "big", signed=True)); pc += n
            elif op in (0x12, 0x13):
                n = 1 if op == 0x12 else 2
                entry = self.pool.get(int.from_bytes(code[pc:pc+n], "big")); pc += n
                stack.append(self.pool.utf8(entry[1]) if entry[0] == "String" else entry[1])
            elif op in (0xb2, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xbb, 0xc0):
                entry = self.pool.get(int.from_bytes(code[pc:pc+2], "big")); pc += 2
                if op == 0xbb:
                    stack.append({}); continue
                if op == 0xc0:
                    assert self.pool.describe(int.from_bytes(code[pc-2:pc], "big")) == "Sprite"
                    assert isinstance(stack[-1], dict); continue
                owner = self.pool.describe(entry[1]); nt = self.pool.get(entry[2])
                key, signature = self.pool.utf8(nt[1]), self.pool.utf8(nt[2])
                if op == 0xb2:
                    if owner == "LawmanModule" and key == "sm_vrTextExtent": stack.append(self.extent)
                    elif owner == "LawmanFonts" and key == "FONT_SUBTITLES": stack.append("subtitle")
                    elif owner == "Module" and key == "fonts": stack.append({})
                    elif owner == "ESpriteText" and key == "MULTILINE": stack.append(4)
                    else: raise AssertionError("Unmodelled static field " + owner + "." + key)
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
                    assert op == 0xb8 or target is not None, "null call"
                    result = self.call(owner, key, signature, target, call_args)
                    if not signature.endswith("V"): stack.append(result)
            elif op in (0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa7, 0xc6, 0xc7):
                offset = int.from_bytes(code[pc:pc+2], "big", signed=True); pc += 2
                if op == 0xa7: take = True
                elif op in (0x9f, 0xa0):
                    b, a = stack.pop(), stack.pop(); take = (a == b) if op == 0x9f else (a != b)
                elif op in (0xc6, 0xc7):
                    value = stack.pop(); take = (value is None) if op == 0xc6 else (value is not None)
                else:
                    value = stack.pop()
                    take = {0x99: value == 0, 0x9a: value != 0,
                            0x9b: value < 0, 0x9c: value >= 0, 0x9d: value > 0, 0x9e: value <= 0}[op]
                if take: pc = at + offset
            elif op == 0xb1:
                assert not stack; return
            else: raise AssertionError(f"Unsupported opcode {op:02x} at {at} in {name}")
        raise AssertionError("Unbounded target bytecode")


class MovieSubtitleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = original_class()
        cls.transformed = patch_movie_subtitles(cls.original)

    def test_show_caches_normalized_key_id_movie_after_native_effects(self):
        for text in ("MOVIE_LINE", "&MOVIE_LINE&", "café_字幕"):
            vm = VM(self.transformed); owner = vm.module["m_cMoviePlaying"]
            vm.run("OnShowSubtitle", "(Ljava/lang/String;I)V", (text, 7))
            normalized = text if "&" in text else "&" + text + "&"
            self.assertEqual(vm.module.get(KEY), normalized)
            self.assertEqual(vm.module.get(ID), 7)
            self.assertIs(vm.module.get(MOVIE), owner)
            self.assertEqual(vm.module["cSubtitleText"]["text"], normalized)
            self.assertTrue(vm.module["cSubtitle"]["visible"])
            self.assertEqual(vm.events.count(("Being", "OnShowSubtitle")), 1)
            self.assertEqual(vm.events[0], ("AddDialog", text))

    def test_native_effects_and_optional_visual_branches_match_original(self):
        for child, parent, player, belt, enabled in ((False, True, True, True, True),
                (False, True, True, False, True),
                (True, True, True, False, False), (False, False, False, False, True)):
            options = dict(child=child, parent=parent, player=player, belt=belt, enabled=enabled)
            before, after = VM(self.original, **options), VM(self.transformed, **options)
            for vm in (before, after): vm.run("OnShowSubtitle", "(Ljava/lang/String;I)V", ("line", 3))
            self.assertEqual(before.events, after.events)
            self.assertEqual({k:v for k,v in after.module.items() if not k.startswith("cojvr")}, before.module)
            self.assertEqual(after.module.get(KEY), "&line&")

    def test_stale_hide_preserves_new_line_and_accepted_hide_clears(self):
        vm = VM(self.transformed)
        for text, identity in (("old", 1), ("new", 2)):
            vm.run("OnShowSubtitle", "(Ljava/lang/String;I)V", (text, identity))
        events = list(vm.events)
        vm.run("OnHideSubtitle", "(I)V", (1,))
        self.assertEqual(vm.events, events)
        self.assertEqual(vm.module.get(KEY), "&new&")
        self.assertEqual(vm.module.get(ID), 2)
        vm.run("OnHideSubtitle", "(I)V", (2,))
        self.assert_cleared(vm)
        self.assertFalse(vm.module["cSubtitle"]["visible"])
        self.assertEqual(vm.events.count(("Being", "OnHideSubtitle")), 1)

    def assert_cleared(self, vm):
        self.assertIsNone(vm.module.get(KEY))
        self.assertEqual(vm.module.get(ID), -1)
        self.assertIsNone(vm.module.get(MOVIE))

    def test_lifecycle_clears_preserve_original_side_effects(self):
        for name in ("MovieDestroySubtitles", "OnStartMovie", "OnEndMovie"):
            for player, child, movie, subtitles in ((False, False, False, False),
                    (False, True, True, True), (True, True, True, True)):
                options = dict(player=player, child=child)
                before, after = VM(self.original, **options), VM(self.transformed, **options)
                for vm in (before, after):
                    if not movie: vm.module["m_cMoviePlaying"] = None
                    vm.module["bMovieSubtitles"] = subtitles
                    vm.module.update({KEY: "&stale&", ID: 4, MOVIE: vm.module["m_cMoviePlaying"]})
                    vm.run(name, "()V" if name == "MovieDestroySubtitles" else "(LMovie;)V", () if name == "MovieDestroySubtitles" else ({},))
                self.assert_cleared(after)
                self.assertEqual(before.events, after.events)
                self.assertEqual(before.module.get("m_cMoviePlaying"), after.module.get("m_cMoviePlaying"))

    def test_reused_id_is_bound_to_new_movie(self):
        vm = VM(self.transformed); old = vm.module["m_cMoviePlaying"]
        vm.run("OnShowSubtitle", "(Ljava/lang/String;I)V", ("old", 1))
        vm.module["m_cMoviePlaying"] = {"bUseLevelTrack": False}
        self.assertIs(vm.module.get(MOVIE), old)
        self.assertIsNot(vm.module.get(MOVIE), vm.module["m_cMoviePlaying"])
        vm.run("OnShowSubtitle", "(Ljava/lang/String;I)V", ("new", 1))
        self.assertIs(vm.module.get(MOVIE), vm.module["m_cMoviePlaying"])

    def test_native_exception_does_not_publish_partial_show(self):
        vm = VM(self.transformed); vm.fail_call = ("Being", "OnShowSubtitle")
        with self.assertRaisesRegex(RuntimeError, "native callback"):
            vm.run("OnShowSubtitle", "(Ljava/lang/String;I)V", ("line", 4))
        self.assertNotIn(KEY, vm.module)
        self.assertNotIn(ID, vm.module)
        self.assertNotIn(MOVIE, vm.module)

    def test_only_target_code_and_relocated_lines_change(self):
        old, new = structure(self.original), structure(self.transformed)
        self.assertEqual(self.original[:8], self.transformed[:8])
        self.assertEqual(self.original[10:old[1]], self.transformed[10:old[1]])
        self.assertEqual(old[0].entries, new[0].entries[:len(old[0].entries)])
        self.assertEqual(old[2], new[2]); self.assertEqual(old[5], new[5])
        for key, member in old[3].items(): self.assertEqual(member, new[3][key])
        self.assertEqual(set(new[3])-set(old[3]), {(KEY,"Ljava/lang/String;"),(ID,"I"),(MOVIE,"LMovie;")})
        for key in set(new[3])-set(old[3]): self.assertEqual(new[3][key][:2], (0x81, []))
        self.assertEqual(set(old[4]), set(new[4]))
        for key, member in old[4].items():
            if key not in TARGETS:
                self.assertEqual(member, new[4][key]); continue
            a, b = code_parts(member), code_parts(new[4][key])
            if key[0] in ("OnShowSubtitle", "OnHideSubtitle"):
                self.assertEqual(a[2][:-1], b[2][:TARGETS[key]])
                self.assertEqual(a[3], b[3])
            else:
                self.assertEqual(a[2], b[2][16:])
                self.assertEqual(a[3][:12], b[3][:12])
                self.assertEqual(len(a[3]), len(b[3]))
                self.assertEqual(line_numbers(b[3]), [(pc+16, line) for pc, line in line_numbers(a[3])])
            self.assertEqual(a[1], b[1])
            self.assertEqual(b[0], max(2, a[0]))
            self.assertEqual(member[0], new[4][key][0])

    def test_reentrant_first_subtitle_survives_start_callbacks(self):
        for callback in (("LawmanModule", "MovieCreateSubtitles"), ("Being", "OnStartMovie")):
            vm = VM(self.transformed)
            vm.module.update({KEY: "&old&", ID: 7, MOVIE: vm.module["m_cMoviePlaying"]})
            def show():
                self.assert_cleared(vm)
                vm.run("OnShowSubtitle", "(Ljava/lang/String;I)V", ("first", 8))
            vm.reentrant[callback] = show
            vm.run("OnStartMovie", "(LMovie;)V", (vm.module["m_cMoviePlaying"],))
            self.assertEqual(vm.module.get(KEY), "&first&")
            self.assertEqual(vm.module.get(ID), 8)
            self.assertIs(vm.module.get(MOVIE), vm.module["m_cMoviePlaying"])
            self.assertEqual(vm.events.count(callback), 1)
            self.assertEqual(vm.events.count(("Being", "OnStartMovie")), 1)

    def test_reentrant_line_is_not_cleared_after_destroy_or_end_callbacks(self):
        for name, callback in (("MovieDestroySubtitles", ("Module", "MovieDestroySubtitles")),
                               ("OnEndMovie", ("Being", "OnEndMovie"))):
            vm = VM(self.transformed)
            vm.module.update({KEY: "&old&", ID: 1, MOVIE: vm.module["m_cMoviePlaying"]})
            def show():
                self.assert_cleared(vm)
                vm.run("OnShowSubtitle", "(Ljava/lang/String;I)V", ("next", 2))
            vm.reentrant[callback] = show
            vm.run(name, "()V" if name == "MovieDestroySubtitles" else "(LMovie;)V",
                   () if name == "MovieDestroySubtitles" else ({},))
            self.assertEqual(vm.module.get(KEY), "&next&")
            self.assertEqual(vm.module.get(ID), 2)
            self.assertEqual(vm.events.count(callback), 1)

    def test_unknown_truncated_modified_and_already_patched_reject(self):
        for data in (b"", self.original[:-1], self.original[:40]+b"X"+self.original[41:], self.transformed):
            with self.assertRaisesRegex(ValueError, "SHA-256"):
                patch_movie_subtitles(data)


if __name__ == "__main__":
    unittest.main(verbosity=2)
