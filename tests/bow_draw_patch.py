"""Execute the original/transformed bow cancellation bytecode, without a JVM.

Real CheckBulletTime, TurnOffAlternateFire, PrepareForNormalFire and move-state
selection execute. Game ownership and BulletTime getters are bounded fixtures;
no animation, clock, projectile or headset execution is claimed.
"""
import hashlib
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from inspect_java_bytecode import parse_class
from inspect_java_class import parse_class as declarations
from patch_coj_weapon_consumers import Code, patch_class, patch_archive, u2, u4, PAK_SHA256
from bow_native_dispatch import original_class

FIELD = "cojvrPhysicalBowDrawWeapon"
CHECK_SHA = "6e97b3218cb31c904931261bc2ae9a50ab5baf80f8813d01bb7cec29898e5abf"
EXECUTED = {"CheckBulletTime", "TurnOffAlternateFire", "PrepareForNormalFire",
            "SetDesiredWeaponMoveState", "GetWeaponMoveState"}


def verifier_class():
    """Reflect on the transformed player with initialization explicitly disabled."""
    pool = []
    def add(raw):
        pool.append(raw); return len(pool)
    def utf(text):
        raw = text.encode(); return add(b'\x01' + u2(len(raw)) + raw)
    def cls(name): return add(b'\x07' + u2(utf(name)))
    def method(owner, name, desc):
        nt = add(b'\x0c' + u2(utf(name)) + u2(utf(desc)))
        return add(b'\x0a' + u2(cls(owner)) + u2(nt))
    this, parent = cls('VerifyBowDrawPatch'), cls('java/lang/Object')
    main, desc, attr = utf('main'), utf('([Ljava/lang/String;)V'), utf('Code')
    player = add(b'\x08' + u2(utf('ArmedPlayerBeing')))
    loader = method('java/lang/ClassLoader', 'getSystemClassLoader', '()Ljava/lang/ClassLoader;')
    lookup = method('java/lang/Class', 'forName', '(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;')
    methods = method('java/lang/Class', 'getDeclaredMethods', '()[Ljava/lang/reflect/Method;')
    fields = method('java/lang/Class', 'getDeclaredFields', '()[Ljava/lang/reflect/Field;')
    c = Code(); c.ref(0x13, player); c.emit(0x03); c.ref(0xb8, loader)
    c.ref(0xb8, lookup); c.emit(0x59); c.ref(0xb6, methods); c.emit(0x57)
    c.ref(0xb6, fields); c.emit(0x57, 0xb1)
    code = c.finish()
    body = u2(3) + u2(1) + u4(len(code)) + code + u2(0) + u2(0)
    return (bytes.fromhex('cafebabe0000002e') + u2(len(pool)+1) + b''.join(pool) +
            u2(0x21) + u2(this) + u2(parent) + u2(0) + u2(0) + u2(1) +
            u2(9) + u2(main) + u2(desc) + u2(1) + u2(attr) + u4(len(body)) + body + u2(0))


class BowCancelVM:
    def __init__(self, data):
        self.pool, methods = parse_class(data)
        self.methods = {(m.name, m.descriptor): m for m in methods}
        self.bow = {"class": "WeaponBow"}
        self.player = {FIELD: self.bow, "m_abAttackHandState": [True, False],
                       "m_bAlternateFireOn": True, "m_bNetAttackForced": False,
                       "m_iPrimaryHand": 0, "m_Module": object()}
        self.actual = [None, self.bow]
        self.desired = [None, self.bow]
        self.bow_hand = 1
        self.state = 129
        self.net = False
        self.running = True
        self.stopping = True
        self.can_shoot = True
        self.has_bow = True
        self.events = []
        self.reads = 0
        self.replace_on_read = 0

    def reference(self, index):
        entry = self.pool.get(index)
        pair = self.pool.get(entry[2])
        return self.pool.describe(entry[1]), self.pool.utf8(pair[1]), self.pool.utf8(pair[2])

    def invoke(self, owner, name, desc, target, args):
        self.events.append((name, tuple(args)))
        if owner == "ArmedPlayerBeing" and (name in EXECUTED or name.startswith("cojvr")):
            if target is not self.player:
                raise AssertionError("Wrong player receiver")
            return self.run(name, desc, args)
        if name == "IsAlt": return bool(args[0] & 128)
        if name == "IsNetGame": return self.net
        if name == "HasBowInHands": return self.has_bow
        if name == "GetBowHand": return self.bow_hand
        if name == "GetHandStateMashineState": return self.state
        if name == "IsBulletTime": return self.running
        if name == "GetBulletTime": return self
        if name == "IsStopping": return self.stopping
        if name == "CanShootInBulletTimeBow": return self.can_shoot
        if name in ("BreakBulletTime", "HideWeapon", "SetDesiredWeaponState"): return None
        if name in ("HasBulletTimeWeapon", "CanShootInBulletTime"): return True
        if name in ("GetActualWeaponNotEmpty", "GetActualWeapon", "GetDesiredWeapon"):
            self.reads += 1
            if self.reads == self.replace_on_read:
                self.actual[1] = self.desired[1] = {"class": "WeaponBow"}
            return (self.desired if name == "GetDesiredWeapon" else self.actual)[args[0]]
        if name == "GetOtherHand": return 1 - args[0]
        if name == "GetDesiredWeaponState": return 1 if self.desired[args[0]] else 0
        if name == "CanAlternateUse": return True
        if name == "getClass": return ("class", target["class"])
        if name == "getName": return target[1]
        if name == "equals": return target == args[0]
        raise AssertionError(f"Unexpected native dependency: {owner}.{name}{desc}")

    def run(self, name="CheckBulletTime", desc="()V", args=()):
        method = self.methods[name, desc]
        code = method.code
        local = [None] * method.max_locals
        local[:1 + len(args)] = [self.player] + list(args)
        stack = []
        pc = 0
        for _ in range(2000):
            if not 0 <= pc < len(code) or len(stack) > method.max_stack:
                raise AssertionError("Invalid bytecode PC/stack bound")
            at, op = pc, code[pc]
            pc += 1
            if op == 0x01: stack.append(None)
            elif 0x02 <= op <= 0x08: stack.append(op - 3)
            elif op == 0x11:
                stack.append(int.from_bytes(code[pc:pc+2], "big", signed=True)); pc += 2
            elif op == 0x13:
                entry = self.pool.get(int.from_bytes(code[pc:pc+2], "big")); pc += 2
                if entry[0] != "String": raise AssertionError("Expected string constant")
                stack.append(self.pool.utf8(entry[1]))
            elif 0x1a <= op <= 0x1d: stack.append(local[op - 0x1a])
            elif 0x2a <= op <= 0x2d: stack.append(local[op - 0x2a])
            elif 0x3b <= op <= 0x3e: local[op - 0x3b] = stack.pop()
            elif 0x4b <= op <= 0x4e: local[op - 0x4b] = stack.pop()
            elif op == 0x33:
                index, array = stack.pop(), stack.pop(); stack.append(int(array[index]))
            elif op in (0xb4, 0xb5):
                _, field, _ = self.reference(int.from_bytes(code[pc:pc+2], "big")); pc += 2
                if op == 0xb4: stack.append(stack.pop()[field])
                else:
                    value = stack.pop(); stack.pop()[field] = value
            elif op in (0xb6, 0xb7, 0xb8):
                owner, called, signature = self.reference(int.from_bytes(code[pc:pc+2], "big")); pc += 2
                arguments, result = signature[1:].split(")", 1)
                types = re.findall(r"\[*L[^;]+;|\[*[ZBCSIJFD]", arguments)
                if "".join(types) != arguments: raise AssertionError(signature)
                values = [stack.pop() for _ in types][::-1]
                target = None if op == 0xb8 else stack.pop()
                value = self.invoke(owner, called, signature, target, values)
                if result != "V": stack.append(value)
            elif 0x99 <= op <= 0xa7 or op in (0xc6, 0xc7):
                delta = int.from_bytes(code[pc:pc+2], "big", signed=True); pc += 2
                if op == 0xa7: take = True
                elif op in (0xc6, 0xc7): take = (stack.pop() is None) == (op == 0xc6)
                elif op <= 0x9e:
                    value = stack.pop()
                    take = (value == 0, value != 0, value < 0, value >= 0, value > 0, value <= 0)[op - 0x99]
                else:
                    b, a = stack.pop(), stack.pop()
                    take = (a == b, a != b, a < b, a >= b, a > b, a <= b)[op - 0x9f] if op < 0xa5 else ((a is b) == (op == 0xa5))
                if take: pc = at + delta
            elif op in (0xac, 0xb0, 0xb1):
                value = None if op == 0xb1 else stack.pop()
                if stack: raise AssertionError("Nonempty return stack")
                return value
            else: raise AssertionError(f"Unknown opcode {op:02x} at {at}")
        raise AssertionError("Instruction bound exceeded")


class BowDrawPatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = original_class()
        cls.patched = patch_class(cls.original)

    def vm(self, original=False):
        return BowCancelVM(self.original if original else self.patched)

    def test_original_reproduces_held_bow_expiry_and_patched_preserves_alt(self):
        old = self.vm(original=True); old.run()
        self.assertFalse(old.player["m_bAlternateFireOn"])
        self.assertTrue(old.player["m_abAttackHandState"][0])
        self.assertIn(("SetDesiredWeaponState", (1, 1)), old.events)
        for running in (False, True):
            with self.subTest(running=running):
                vm = self.vm(); vm.running = running; vm.run()
                self.assertTrue(vm.player["m_bAlternateFireOn"], "held qualified physical bow lost ALT on native expiry")
                self.assertFalse(any(n == "SetDesiredWeaponState" for n, _ in vm.events))

    def test_physical_reference_cannot_bypass_original_right_release(self):
        vm = self.vm(); vm.player["m_abAttackHandState"][0] = False; vm.run()
        self.assertFalse(vm.player["m_bAlternateFireOn"])
        self.assertIn(("TurnOffAlternateFire", ()), vm.events)

    def test_missing_reference_preserves_original_behavior_and_effect_order(self):
        for running, stopping, can_shoot, state in [(False, False, True, 129), (True, True, True, 129),
                (True, False, False, 129), (True, False, True, 129), (False, False, True, 131)]:
            with self.subTest(values=(running, stopping, can_shoot, state)):
                old, new = self.vm(True), self.vm()
                for vm in (old, new):
                    vm.player[FIELD] = None
                    vm.running, vm.stopping, vm.can_shoot, vm.state = running, stopping, can_shoot, state
                    vm.run()
                self.assertEqual(old.player["m_bAlternateFireOn"], new.player["m_bAlternateFireOn"])
                # The helper itself is new; every original observable native effect remains ordered.
                self.assertEqual(old.events, [(n, a) for n, a in new.events if not n.startswith("cojvr")])

    def test_stale_nonbow_wrong_hand_and_network_refs_cannot_preserve_alt(self):
        for fault in ("actual", "desired", "both", "class", "subclass", "hand", "forced", "replace_during_read"):
            with self.subTest(fault=fault):
                vm = self.vm()
                if fault in ("actual", "both"): vm.actual[1] = {"class": "WeaponBow"}
                if fault in ("desired", "both"): vm.desired[1] = {"class": "WeaponBow"}
                if fault == "class": vm.bow["class"] = "WeaponRifle"
                if fault == "subclass": vm.bow["class"] = "CustomWeaponBow"
                if fault == "hand": vm.bow_hand = 0
                if fault == "forced": vm.player["m_bNetAttackForced"] = True
                if fault == "replace_during_read": vm.replace_on_read = 3
                vm.run()
                self.assertFalse(vm.player["m_bAlternateFireOn"], fault)
        vm = self.vm(); vm.net = True; vm.run()
        self.assertEqual(vm.events, [("IsNetGame", ())])

    def test_guard_does_not_skip_bullet_time_break_or_alt_start_exemption(self):
        vm = self.vm(); vm.stopping = False; vm.can_shoot = False; vm.run()
        self.assertIn(("BreakBulletTime", ()), vm.events)
        self.assertTrue(vm.player["m_bAlternateFireOn"])
        vm = self.vm(); vm.state = 131; vm.running = False; vm.run()
        self.assertTrue(vm.player["m_bAlternateFireOn"])
        self.assertFalse(any(n.startswith("cojvr") for n, _ in vm.events))

    def test_exact_original_boundary_and_transient_field(self):
        _, methods = parse_class(self.original)
        check = next(m for m in methods if m.name == "CheckBulletTime")
        self.assertEqual(hashlib.sha256(check.code).hexdigest(), CHECK_SHA)
        self.assertEqual(len(check.code), 173)
        self.assertEqual(check.code[114:118], bytes.fromhex("2a b6 03 cf"))
        fields = [f for f in declarations(self.patched).fields if f.name == FIELD]
        self.assertEqual(len(fields), 1, "transient physical bow owner field is missing")
        field = fields[0]
        self.assertEqual(field.descriptor, "LWeapon;")
        self.assertEqual(field.access, 0x0081)
        _, patched = parse_class(self.patched)
        changed = next(m for m in patched if m.name == "CheckBulletTime")
        self.assertEqual(changed.code[:115], check.code[:115])
        self.assertEqual(changed.code[118:], check.code[118:])
        self.assertEqual(len(changed.code), len(check.code))

    def test_shipped_verifier_and_archive_preserve_native_concentration_lifecycle(self):
        import os
        from coj_reload_wait_probe import ClassEdit
        game = Path(os.environ.get('COJ_GAME_DIRECTORY',
            'C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez'))
        java = game / 'jre/bin/java.exe'
        if not java.is_file(): self.skipTest('Shipped Java 1.4 JVM unavailable')
        archive = next(p for p in (game / 'code.pak', game / 'code.cojvr-backup.pak')
                       if p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest() == PAK_SHA256)
        evidence = ROOT / 'work/bow-draw-patch-jvm'
        evidence.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='verify-', dir=evidence) as directory:
            root = Path(directory)
            candidate = root / 'code.pak'
            patch_archive(archive, candidate, manual_reload=True)
            with zipfile.ZipFile(candidate) as patched, zipfile.ZipFile(archive) as original:
                for member in ('BulletTime.class', 'PlayerBeingBilly.class', 'PlayerBowStateMashine.class'):
                    self.assertEqual(patched.read(member), original.read(member), member)
                transformed = patched.read('ArmedPlayerBeing.class')
            (root / 'VerifyBowDrawPatch.class').write_bytes(verifier_class())
            good, bad = root / 'good', root / 'bad'
            good.mkdir(); bad.mkdir()
            (good / 'ArmedPlayerBeing.class').write_bytes(transformed)
            edit = ClassEdit(transformed)
            edit.changes['cojvrStopUnownedBowAlternate', '()V'] = lambda code: b'\xfe' + code[1:]
            (bad / 'ArmedPlayerBeing.class').write_bytes(edit.finish())
            for label, folder in (('good', good), ('bad', bad)):
                result = subprocess.run([str(java), '-Xverify:all', '-cp',
                    str(folder) + ';' + str(root) + ';' + str(candidate), 'VerifyBowDrawPatch'],
                    capture_output=True, text=True, timeout=30)
                (evidence / ('jvm-' + label + '.log')).write_text(
                    f'exit_code={result.returncode}\n' + result.stdout + result.stderr, encoding='utf-8')
                if label == 'good':
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                else:
                    self.assertNotEqual(result.returncode, 0, 'Corrupt helper bypassed verification')
                    self.assertIn('VerifyError', result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
