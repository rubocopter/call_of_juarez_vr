"""Execute the exact original bow Attack scheduling contract, without a JVM.

Only ArmedPlayerBeing.Attack(IZ)V and Attack(II)Z execute. Surrounding native
permissions, weapon selection and state scheduling are bounded stubs. No hand
state machine, animation, Weapon.Attack, arrow, ammo or physical success is faked.
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
from inspect_java_bytecode import parse_class

PAK_SHA256 = "f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12"
CLASS_SHA256 = "039b0c12682b99b6560d4597737d794acd4a2b4bb71060c1dc70a8b79ffc4478"
METHODS = {
    ("Attack", "(IZ)V"): (301, "da983bb12c8186d093859a4182bc595f176a5ced2de55cf28f5015889db57a46"),
    ("Attack", "(II)Z"): (34, "b81435980e1fc061a67c54a5f80896a75abadef42eae82bea4413e8c315cb510"),
}


def original_class():
    game = Path(os.environ.get("COJ_GAME_DIRECTORY",
        "C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez"))
    found = False
    for path in (game / "code.pak", game / "code.cojvr-backup.pak"):
        if not path.is_file():
            continue
        found = True
        archive = path.read_bytes()
        if hashlib.sha256(archive).hexdigest() != PAK_SHA256:
            continue
        with zipfile.ZipFile(path) as pak:
            data = pak.read("ArmedPlayerBeing.class")
        if hashlib.sha256(data).hexdigest() != CLASS_SHA256:
            raise AssertionError("Original ArmedPlayerBeing class SHA-256 mismatch")
        return data
    if found:
        raise AssertionError("Present archives do not include the exact original code.pak")
    raise unittest.SkipTest("Exact original code.pak unavailable")


class VM:
    """Small fail-closed JVM instruction interpreter; no method-body substitution."""
    def __init__(self, data):
        self.pool, methods = parse_class(data)
        self.methods = {(m.name, m.descriptor): m for m in methods if (m.name, m.descriptor) in METHODS}
        for key, (length, digest) in METHODS.items():
            method = self.methods[key]
            if len(method.code) != length or hashlib.sha256(method.code).hexdigest() != digest:
                raise AssertionError(f"Original method changed: {key}")
        self.player = {"m_abAttackHandState": [True, False]}
        self.bow = object()
        self.state = 129
        self.permitted = True
        self.events = []
        self.executed = []
        # A stubbed native GetWeaponShotState result, not an executed state machine.
        self.native_shot_state = 158

    def reference(self, index):
        entry = self.pool.get(index)
        if entry[0] not in ("Methodref", "InterfaceMethodref", "Fieldref"):
            raise AssertionError(f"Unsupported constant-pool reference {entry}")
        pair = self.pool.get(entry[2])
        return self.pool.describe(entry[1]), self.pool.utf8(pair[1]), self.pool.utf8(pair[2])

    def native(self, owner, name, descriptor, target, args):
        key = owner, name, descriptor
        self.events.append((key, tuple(args)))
        if key == ("InventoryBeing", "Attack", "(IZ)V"):
            return None  # Inherited cannon behavior is outside the bow contract.
        if key == ("EBeingOperateStates", "IsAlt", "(I)Z"):
            return bool(args[0] & 128)
        if target is not self.player:
            raise AssertionError(f"Unexpected native receiver: {key}")
        if owner == "ArmedPlayerBeing":
            if (name, descriptor) == ("CanAttack", "(II)Z"):
                return self.permitted
            if (name, descriptor) == ("IsCarrying", "()Z"):
                return False
            if (name, descriptor) in (("HasWeaponInHands", "()Z"), ("HasBowInHands", "()Z")):
                return True
            if (name, descriptor) == ("TurnOnAlternateFire", "()V"):
                return None  # Intent only; no fabricated alternate-state transition.
            if (name, descriptor) == ("GetBowHand", "()I"):
                return 1
            if (name, descriptor) == ("GetHandStateMashineState", "(I)I"):
                if args != [1]:
                    raise AssertionError("Wrong native bow owner hand")
                return self.state
            if (name, descriptor) == ("HasWhipInHands", "()Z"):
                return False
            if (name, descriptor) == ("GetDesiredWeapon", "(I)LWeapon;"):
                return self.bow if args == [1] else None
            if (name, descriptor) == ("GetWeaponShotState", "(LWeapon;I)I"):
                if args != [self.bow, 1]:
                    raise AssertionError("Wrong bow/native input channel at shot-state boundary")
                return self.native_shot_state
            if (name, descriptor) in (("SetDesiredWeaponState", "(II)V"), ("UpdateHandStateNow", "()V")):
                return None  # Record scheduling; never execute/fake state callbacks.
        raise AssertionError(f"Unknown native call: {key}")

    def run(self, name, descriptor, args=(), depth=0):
        if depth > 4:
            raise AssertionError("Bytecode call-depth bound exceeded")
        method = self.methods[name, descriptor]
        code = method.code
        self.executed.append((name, descriptor))
        loc = [None] * method.max_locals
        loc[:1 + len(args)] = [self.player] + list(args)
        stack = []
        pc = 0
        for _ in range(1000):
            if not 0 <= pc < len(code) or len(stack) > method.max_stack:
                raise AssertionError("Invalid bytecode PC/stack bound")
            at, op = pc, code[pc]
            pc += 1
            if op == 0x01:
                stack.append(None)
            elif 0x02 <= op <= 0x08:
                stack.append(op - 3)
            elif 0x1A <= op <= 0x1D:
                stack.append(loc[op - 0x1A])
            elif 0x2A <= op <= 0x2D:
                stack.append(loc[op - 0x2A])
            elif op in (0x15, 0x19):
                stack.append(loc[code[pc]])
                pc += 1
            elif op in (0x36, 0x3A):
                loc[code[pc]] = stack.pop()
                pc += 1
            elif 0x3B <= op <= 0x3E:
                loc[op - 0x3B] = stack.pop()
            elif 0x4B <= op <= 0x4E:
                loc[op - 0x4B] = stack.pop()
            elif op == 0x33:  # baload: original m_abAttackHandState read.
                index, array = stack.pop(), stack.pop()
                if not 0 <= index < len(array):
                    raise AssertionError("Native array index outside bound")
                stack.append(int(array[index]))
            elif op == 0x57:
                stack.pop()
            elif op == 0x60:
                b, a = stack.pop(), stack.pop()
                stack.append(a + b)
            elif op == 0xB4:
                index = int.from_bytes(code[pc:pc + 2], "big")
                pc += 2
                _, field, _ = self.reference(index)
                stack.append(stack.pop()[field])
            elif op in (0xB6, 0xB7, 0xB8):
                index = int.from_bytes(code[pc:pc + 2], "big")
                pc += 2
                owner, called, desc = self.reference(index)
                arguments, result = desc[1:].split(")", 1)
                types = re.findall(r"\[*L[^;]+;|\[*[ZBCSIJFD]", arguments)
                if "".join(types) != arguments or any(t in ("J", "D") for t in types):
                    raise AssertionError(f"Unsupported invocation descriptor: {desc}")
                values = [stack.pop() for _ in types][::-1]
                target = None if op == 0xB8 else stack.pop()
                if owner == "ArmedPlayerBeing" and (called, desc) in self.methods:
                    if op != 0xB6 or target is not self.player:
                        raise AssertionError("Unexpected original-method dispatch")
                    value = self.run(called, desc, values, depth + 1)
                else:
                    value = self.native(owner, called, desc, target, values)
                if result != "V":
                    stack.append(value)
            elif 0x99 <= op <= 0xA7:
                delta = int.from_bytes(code[pc:pc + 2], "big", signed=True)
                pc += 2
                if op == 0xA7:
                    take = True
                elif op <= 0x9E:
                    value = stack.pop()
                    take = (value == 0, value != 0, value < 0,
                            value >= 0, value > 0, value <= 0)[op - 0x99]
                else:
                    b, a = stack.pop(), stack.pop()
                    take = (a == b, a != b, a < b, a >= b, a > b, a <= b,
                            a is b, a is not b)[op - 0x9F]
                if take:
                    pc = at + delta
            elif op in (0xAC, 0xB1):
                result = stack.pop() if op == 0xAC else None
                if stack:
                    raise AssertionError("Nonempty operand stack at return")
                return result
            else:
                raise AssertionError(f"Unknown opcode 0x{op:02x} at {at} in {name}{descriptor}")
        raise AssertionError("Bytecode instruction bound exceeded")

    def count(self, name):
        return sum(key[1] == name for key, _ in self.events)

    def requests(self):
        return [args for key, args in self.events if key[1] == "SetDesiredWeaponState"]


class BowNativeDispatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = original_class()

    def vm(self):
        return VM(self.data)

    def test_right_press_requests_alternate_only(self):
        vm = self.vm()
        vm.run("Attack", "(IZ)V", [0, True])
        self.assertEqual(vm.count("TurnOnAlternateFire"), 1)
        self.assertEqual(vm.requests(), [])
        self.assertEqual(vm.count("CanAttack"), 0)

    def test_releases_do_not_request_attack_or_draw(self):
        for hand in (0, 1):
            with self.subTest(hand=hand):
                vm = self.vm()
                vm.run("Attack", "(IZ)V", [hand, False])
                self.assertEqual(vm.count("Attack"), 1)  # Inherited call only.
                self.assertEqual(vm.count("TurnOnAlternateFire"), 0)
                self.assertEqual(vm.requests(), [])

    def test_internal_left_press_schedules_once_through_original_overload(self):
        vm = self.vm()
        vm.run("Attack", "(IZ)V", [1, True])
        self.assertEqual(vm.requests(), [(1, vm.native_shot_state)])
        self.assertEqual(vm.count("CanAttack"), 1)
        self.assertEqual(vm.count("UpdateHandStateNow"), 1)
        self.assertEqual(vm.executed, [("Attack", "(IZ)V"), ("Attack", "(II)Z")])

    def test_native_permission_denial_never_schedules_either_hand(self):
        vm = self.vm()
        vm.permitted = False
        vm.run("Attack", "(IZ)V", [1, True])
        self.assertEqual(vm.requests(), [])
        self.assertEqual(vm.count("CanAttack"), 2)  # Original opposite-hand retry.
        self.assertEqual(vm.count("GetWeaponShotState"), 0)

    def test_unheld_right_rejects_internal_left_before_permission_check(self):
        vm = self.vm()
        vm.player["m_abAttackHandState"][0] = False
        vm.run("Attack", "(IZ)V", [1, True])
        self.assertEqual(vm.requests(), [])
        self.assertEqual(vm.count("CanAttack"), 0)

    def test_nonalternate_states_reject_internal_left(self):
        for state in (0, 1, 3, 31):
            with self.subTest(state=state):
                vm = self.vm()
                vm.state = state
                vm.run("Attack", "(IZ)V", [1, True])
                self.assertEqual(vm.requests(), [])
                self.assertEqual(vm.count("CanAttack"), 0)

    def test_original_alt_bit_alone_also_accepts_alt_start(self):
        vm = self.vm()
        vm.state = 131
        vm.run("Attack", "(IZ)V", [1, True])
        self.assertEqual(vm.requests(), [(1, vm.native_shot_state)])
        # The physical adapter's stricter 129-only gate is independently tested
        # in bow_draw_native.cpp; it is not an original Attack(IZ) requirement.

    def test_protected_overload_honors_native_permission(self):
        for allowed in (False, True):
            with self.subTest(allowed=allowed):
                vm = self.vm()
                vm.permitted = allowed
                result = vm.run("Attack", "(II)Z", [1, 1])
                self.assertEqual(bool(result), allowed)
                self.assertEqual(vm.requests(), [(1, vm.native_shot_state)] if allowed else [])

    def test_one_internal_pulse_then_releases_requests_exactly_once(self):
        vm = self.vm()
        vm.state = 1
        vm.run("Attack", "(IZ)V", [0, True])
        self.assertEqual(vm.requests(), [])
        # Explicit stub observation of native readiness; no simulated animation.
        vm.state = 129
        vm.run("Attack", "(IZ)V", [1, True])
        vm.run("Attack", "(IZ)V", [1, False])
        vm.player["m_abAttackHandState"][0] = False
        vm.run("Attack", "(IZ)V", [0, False])
        self.assertEqual(vm.requests(), [(1, vm.native_shot_state)])
        self.assertEqual(vm.count("TurnOnAlternateFire"), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
