"""Execute unchanged shipped duel/input bytecode; stub only external engine seams.

No archive is modified and no game process is started. This small interpreter
rejects unknown opcodes/calls; it is a bounded control-flow fixture, not a JVM
or evidence of game/headset execution.
"""
import hashlib
import math
from pathlib import Path
import re
import struct
import sys
import subprocess
import unittest
import zipfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from inspect_java_bytecode import parse_class

SHA = 'f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12'
ROOT = Path(__file__).resolve().parents[1]
MAPPING_EXECUTABLE = None
if '--mapping-executable' in sys.argv:
    flag = sys.argv.index('--mapping-executable')
    MAPPING_EXECUTABLE = sys.argv[flag + 1]
    del sys.argv[flag:flag + 2]


def f32(x):
    return struct.unpack('>f', struct.pack('>f', x))[0]


def archive():
    candidates = [ROOT / 'work/release-prealpha2/originals/code.pak',
                  Path('C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez/code.pak')]
    for path in candidates:
        if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest() == SHA:
            return zipfile.ZipFile(path)
    raise unittest.SkipTest('Exact original code.pak unavailable')


class NativeFixture:
    def __init__(self):
        self.classes = {}
        with archive() as pak:
            for name in ('GameInputController', 'CreatureController', 'BeingController',
                         'ArmedPlayerBeing', 'Duel', 'LawmanModuleSingle', 'Creature', 'InputSettings'):
                pool, methods = parse_class(pak.read(name + '.class'))
                self.classes[name] = (pool, {(m.name, m.descriptor): m for m in methods})
        pool, methods = self.classes['ArmedPlayerBeing']
        # Decode the pinned class initializer's literal assignment rather than
        # replacing the draw threshold with a test-chosen value.
        index = next(i for i in range(1, len(pool.entries)) if pool.entries[i] is not None and
                     pool.describe(i) == 'ArmedPlayerBeing.s_fDuelMouseMinVertVelF')
        init = methods['<clinit>', '()V'].code
        at = init.index(b'\xb3' + index.to_bytes(2, 'big'))
        assert init[at-3] == 0x13
        literal = pool.get(int.from_bytes(init[at-2:at], 'big'))
        assert literal[0] == 'Float'
        self.statics = {'s_fDuelMouseMinVertVel': f32(literal[1]),
                        'sm_vrDuelLookAtPoint': {'fX': 0., 'fY': 0., 'fZ': 0.}}
        self.duel = {'kind': 'Duel', 'm_iState': 2}
        self.module = {'kind': 'LawmanModuleSingle', 'm_cDuel': self.duel}
        self.player = {'kind': 'ArmedPlayerBeing', 'm_Module': self.module,
                       'm_fDuelMouseCurrHorzRot': 0., 'm_fDuelMouseCurrVertRot': 0.,
                       'm_iDuelTestMode': 1, 'm_vDuelLookAtPoint': {'fX': 0., 'fY': 0., 'fZ': 0.}}
        self.target = {'kind': 'CreatureController', 'm_cControlledCreature': self.player}
        item = {'m_cTarget': self.target, 'm_cNext': None}
        self.controller = {'kind': 'GameInputController'}
        self.settings = {'kind': 'InputSettings'}
        self.allowed = True
        self.occupied = False
        self.slomo = 1.
        self.engine_sensitivity = 1.
        self.draws = []
        self.updates = 0
        self.actions = []
        self.run('InputSettings', '<clinit>', '()V', [], None)
        self.run('InputSettings', '<init>', '()V', [], self.settings)
        self.run('InputSettings', 'SetMouseSensitivityOption', '(I)V',
                 [self.settings['nMouseSensitivityOption']], self.settings)
        self.run('GameInputController', '<init>', '()V', [], self.controller)
        self.controller['m_Targets'] = [item, item, item]

    def invoke(self, owner, name, desc, args, obj, special=False):
        # External engine seams and leaf vector storage only. All mouse-axis,
        # controller conversion, accumulation and draw decisions execute bytes.
        if name in ('CanProcessInputs', 'CanExecuteInput', 'CanApplyState'):
            return self.allowed
        if name == '<init>' and owner in ('GameObject', 'java/lang/Object'):
            return None
        if owner == 'java/lang/Math' and name == 'abs':
            return abs(args[0])
        if owner == 'InventoryBeing' and name in ('RotateVertically', 'RotateHorizontally'):
            return None  # ordinary, non-duel native look is outside this fixture
        if name == 'SetMouseSensitivity':
            self.engine_sensitivity = args[1]
            return None
        if name == 'GetMouseSensitivityY':
            return self.engine_sensitivity
        if name == 'GetBulletTimeCurrentSlomo':
            return self.slomo
        if name in ('HasSomethingInHands', 'HasSomethingInHand'):
            return self.occupied
        if name == 'PutWeaponInHands':
            self.draws.append(args[0])
            return True
        if name == 'UpdateHandStateNow':
            self.updates += 1
            return None
        if name == 'Set' and owner == 'Vector':
            obj.update(args[0])
            return None
        if name == 'CallOnInputGameController':
            self.actions.append(tuple(args))
            return self.run('BeingController', 'OnInputGameController', desc, args, obj)
        if obj and not special:
            dynamic = obj.get('kind')
            if dynamic in self.classes and (name, desc) in self.classes[dynamic][1]:
                owner = dynamic
        if name in ('ApplyRotateVertically', 'ApplyRotateHorizontally'):
            owner = 'CreatureController'
        if name == 'GetRealDT':
            owner = 'Creature'
        if name == 'GetRealSlomo':
            owner = 'Creature'
        return self.run(owner, name, desc, args, obj)

    def run(self, cls, name, desc, args, obj):
        pool, methods = self.classes[cls]
        method = methods[name, desc]
        code = method.code
        local = ([] if method.access & 8 else [obj]) + list(args) + [None] * method.max_locals
        stack = []
        pc = 0
        for _ in range(10000):
            at = pc
            op = code[pc]
            pc += 1
            if 0x2a <= op <= 0x2d: stack.append(local[op - 0x2a])
            elif 0x1a <= op <= 0x1d: stack.append(local[op - 0x1a])
            elif 0x22 <= op <= 0x25: stack.append(local[op - 0x22])
            elif 0x3b <= op <= 0x3e: local[op - 0x3b] = stack.pop()
            elif 0x43 <= op <= 0x46: local[op - 0x43] = stack.pop()
            elif 0x4b <= op <= 0x4e: local[op - 0x4b] = stack.pop()
            elif op in (0x15, 0x17, 0x19): stack.append(local[code[pc]]); pc += 1
            elif op in (0x36, 0x38, 0x3a): local[code[pc]] = stack.pop(); pc += 1
            elif op == 1: stack.append(None)
            elif op == 2: stack.append(-1)
            elif 3 <= op <= 8: stack.append(op - 3)
            elif 0xb <= op <= 0xd: stack.append(float(op - 0xb))
            elif op in (0x10, 0x11):
                size = 1 if op == 0x10 else 2
                stack.append(int.from_bytes(code[pc:pc+size], 'big', signed=True)); pc += size
            elif op in (0x12, 0x13):
                size = 1 if op == 0x12 else 2
                index = int.from_bytes(code[pc:pc+size], 'big'); pc += size
                entry = pool.get(index)
                stack.append(entry[1] if entry[0] != 'String' else pool.utf8(entry[1]))
            elif op == 0x59: stack.append(stack[-1])
            elif op == 0x57: stack.pop()
            elif op == 0x32: i = stack.pop(); stack.append(stack.pop()[i])
            elif op in (0x4f, 0x54): v = stack.pop(); i = stack.pop(); stack.pop()[i] = v
            elif op in (0xbc, 0xbd):
                pc += 1 if op == 0xbc else 2
                stack.append([0 if op == 0xbc else None] * stack.pop())
            elif op in (0x60, 0x62, 0x64, 0x66, 0x6a, 0x6c, 0x6e):
                b, a = stack.pop(), stack.pop()
                result = a+b if op in (0x60, 0x62) else a-b if op in (0x64, 0x66) else a*b if op == 0x6a else a/b
                stack.append(int(result) if op in (0x60, 0x64, 0x6c) else f32(result))
            elif op == 0x76: stack.append(-stack.pop())
            elif op == 0x86: stack.append(f32(stack.pop()))
            elif op == 0x84: local[code[pc]] += int.from_bytes(code[pc+1:pc+2], 'big', signed=True); pc += 2
            elif op in (0x95, 0x96):
                b, a = stack.pop(), stack.pop()
                stack.append((-1 if op == 0x95 else 1) if math.isnan(a) or math.isnan(b) else (a > b) - (a < b))
            elif op == 0xaa:
                pc = (pc + 3) & ~3
                default, low, high = struct.unpack_from('>iii', code, pc); pc += 12
                key = stack.pop()
                pc = at + (struct.unpack_from('>i', code, pc + 4*(key-low))[0] if low <= key <= high else default)
            elif op in (0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xc0, 0xc1):
                index = int.from_bytes(code[pc:pc+2], 'big'); pc += 2
                if op == 0xb9: pc += 2
                entry = pool.get(index)
                if op == 0xc0: continue
                if op == 0xc1: stack.append(stack.pop() is not None); continue
                nt = pool.get(entry[2]); key = pool.utf8(nt[1]); signature = pool.utf8(nt[2])
                if op == 0xb2: stack.append(self.statics[key]); continue
                if op == 0xb3: self.statics[key] = stack.pop(); continue
                if op == 0xb4: stack.append(stack.pop()[key]); continue
                if op == 0xb5: value = stack.pop(); stack.pop()[key] = value; continue
                types = re.findall(r'\[*L[^;]+;|\[*[IZFC]', signature[1:signature.index(')')])
                argv = [stack.pop() for _ in types][::-1]
                target = None if op == 0xb8 else stack.pop()
                result = self.invoke(pool.describe(entry[1]), key, signature, argv, target, op == 0xb7)
                if signature[-1] != 'V': stack.append(result)
            elif op in (*range(0x99, 0xa5), 0xa7, 0xc6, 0xc7):
                delta = int.from_bytes(code[pc:pc+2], 'big', signed=True); pc += 2
                if op == 0xa7: take = True
                elif op in (0xc6, 0xc7): take = (stack.pop() is None) == (op == 0xc6)
                elif op >= 0x9f:
                    b, a = stack.pop(), stack.pop()
                    take = (a == b, a != b, a < b, a >= b, a > b, a <= b)[op-0x9f]
                else:
                    a = stack.pop(); take = (a == 0, a != 0, a < 0, a >= 0, a > 0, a <= 0)[op-0x99]
                if take: pc = at + delta
            elif op in (0xac, 0xae, 0xb0): return stack.pop()
            elif op == 0xb1: return None
            else: raise AssertionError(f'Unsupported {cls}.{name} opcode {op:02x} at {at}')
            if len(stack) > method.max_stack: raise AssertionError('Declared max_stack exceeded')
        raise AssertionError('Bytecode instruction limit')

    def mouse(self, axis, value):
        self.run('GameInputController', 'OnInputMouseAxis', '(IF)V', [axis, value], self.controller)

    def update(self, dt):
        self.run('ArmedPlayerBeing', 'UpdateDuel', '(F)V', [f32(dt)], self.player)


class NativeDuelTests(unittest.TestCase):
    @unittest.skipUnless(MAPPING_EXECUTABLE, 'C++ mapping executable not supplied')
    def test_production_cpp_mouse_output_draws_through_unchanged_native_bytecode(self):
        for hz in (90, 120, 144):
            for setting in (0, 5, 10):
                for invert in (False, True):
                    for slomo in (1., .25):
                        vm = NativeFixture(); vm.slomo = slomo
                        vm.run('InputSettings', 'SetMouseSensitivityOption', '(I)V', [setting], vm.settings)
                        vm.statics['bInvertMouse'] = invert
                        game_dt = f32(slomo/hz)
                        real_dt = vm.run('Creature', 'GetRealDT', '(F)F', [game_dt], vm.player)
                        ratio = vm.run('InputSettings', 'GetMouseSensitivity', '()F', [], None)
                        # The production policy emits the sample; this fixture
                        # does not duplicate the conversion formula in Python.
                        parameters = (real_dt, ratio, vm.engine_sensitivity,
                                      vm.statics['s_fDuelMouseMinVertVel'],
                                      vm.controller['fPadSensY'], vm.controller['fPadSpeedYTresh'])
                        output = subprocess.check_output([MAPPING_EXECUTABLE, '--draw-sample',
                                                          *map(str, parameters)], text=True)
                        raw = f32(float(output))
                        vm.mouse(1, raw); vm.update(game_dt)
                        self.assertEqual((vm.draws, vm.updates), ([1], 1),
                                         (hz, setting, invert, slomo, raw))

    def test_allocated_native_duel_starts_inactive(self):
        vm = NativeFixture()
        vm.run('Duel', '<init>', '(LLawmanModule;)V', [vm.module], vm.duel)
        self.assertEqual(vm.duel['m_iState'], -1)
        self.assertTrue(vm.run('Duel', 'IsFinished', '()Z', [], vm.duel))
        self.assertFalse(vm.run('Duel', 'IsInited', '()Z', [], vm.duel))

    def test_mouse_axis_signs_and_inversion_reach_the_original_controller(self):
        for invert in (False, True):
            for axis, value, action in ((0, -10., 2), (0, 10., 3), (1, -10., 1), (1, 10., 0)):
                vm = NativeFixture(); vm.statics['bInvertMouse'] = invert
                vm.mouse(axis, value)
                wanted_action = (1-action) if invert and axis == 1 else action
                wanted_value = -value if invert and axis == 1 else value
                self.assertEqual(vm.actions, [(wanted_action, wanted_value, None)])

    def test_original_pad_default_does_not_draw_at_high_native_update_rates(self):
        for hz in (90, 120, 144):
            for invert in (False, True):
                vm = NativeFixture(); vm.statics['bInvertMouse'] = invert
                vm.run('GameInputController', 'OnInputJoyAxis', '(IIF)V', [0, 5, 1.], vm.controller)
                self.assertEqual(vm.controller['fPadSpeedRY'], -10.)
                vm.run('GameInputController', 'UpdatePadSpeeds', '()V', [], vm.controller)
                rotation = vm.player['m_fDuelMouseCurrVertRot']
                vm.update(1/hz)
                self.assertEqual(vm.draws, [], 'A native default pad sample was incorrectly accepted as quickdraw')
                self.assertEqual(vm.player['m_fDuelMouseCurrVertRot'], 0.)
                self.assertAlmostEqual(abs(rotation)/hz/vm.engine_sensitivity, 2.75/hz, places=6)

    def test_original_mouse_vertical_draw_and_native_gates(self):
        for state, occupied, permitted, wanted in ((-1, False, True, False), (0, False, True, False),
                (1, False, True, False), (2, False, True, True), (3, False, True, True),
                (2, True, True, False), (2, False, False, False)):
            for invert in (False, True):
                vm = NativeFixture(); vm.duel['m_iState'] = state; vm.occupied = occupied; vm.allowed = permitted
                vm.statics['bInvertMouse'] = invert
                if occupied: vm.player['m_iDuelTestMode'] = 99  # isolate the draw gate from occupied aiming
                vm.mouse(1, -100.)
                vm.update(1/144)
                self.assertEqual(bool(vm.draws), wanted)
                if wanted: self.assertEqual((vm.draws, vm.updates), ([1], 1))
                self.assertEqual(vm.player['m_fDuelMouseCurrVertRot'], 0.)

    def test_horizontal_or_upward_input_does_not_draw(self):
        for axis, value in ((0, -100.), (0, 100.), (1, 100.)):
            vm = NativeFixture(); vm.mouse(axis, value); vm.update(1/90)
            self.assertEqual(vm.draws, [])

    def test_draw_threshold_is_strict_and_native_update_consumes_input(self):
        for value, draws in ((-.1, []), (-.10001, [1])):
            vm = NativeFixture()
            vm.run('ArmedPlayerBeing', 'DuelRotateVertically', '(F)V', [f32(value)], vm.player)
            vm.update(1.)
            self.assertEqual(vm.draws, draws)
            vm.update(1.)
            self.assertEqual(vm.draws, draws, 'Native update replayed the consumed mouse rotation')


if __name__ == '__main__':
    unittest.main()
