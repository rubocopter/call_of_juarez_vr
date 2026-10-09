"""Execute exact patched transition/callback bytecode; no game or VR claims."""
import hashlib
import sys
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import patch_coj_weapon_consumers as patch
from inspect_java_bytecode import parse_class, disassemble

ARCHIVE = Path('C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez/code.pak')


def original(name):
    for path in (ARCHIVE, ARCHIVE.with_name('code.cojvr-backup.pak')):
        if path.exists():
            with zipfile.ZipFile(path) as z:
                if hashlib.sha256(z.read('ArmedPlayerBeing.class')).hexdigest() == patch.CLASS_SHA256:
                    return z.read(name + '.class')
    raise unittest.SkipTest('Exact original game archive unavailable')


class VM:
    """Small JVM interpreter; native weapon transfer is an observable boundary.

    Scheduling stands in for native animation. Update, SetCurrentState, the
    reload callback and every new helper execute their emitted bytecode.
    """
    def __init__(self, probe=True):
        self.classes = {}
        if probe and hasattr(patch, 'patch_reload_probe_classes'):
            armed, machine = patch.patch_reload_probe_classes(original('ArmedPlayerBeing'), original('StateMashine'))
        else:
            armed, machine = patch.patch_class(original('ArmedPlayerBeing')), original('StateMashine')
        for name, data in [('ArmedPlayerBeing', armed), ('StateMashine', machine)]:
            pool, methods = parse_class(data)
            self.classes[name] = (pool, {(m.name, m.descriptor): m for m in methods})
        self.weapon = {'class': 'WeaponPistolPeacemaker'}
        self.states = {n: {'id': n} for n in (1, 20, 21, 22)}
        self.machine = {'class': 'StateMashine', 'm_sCurrentState': self.states[1],
                        'm_sDestState': self.states[21], 'm_fCurrentTime': 0., 'start': 0., 'hand': 0}
        self.other = dict(self.machine)
        self.other['hand'] = 1
        self.other_weapon = None
        self.player = {'class': 'ArmedPlayerBeing', 'cojvrSingleRoundWeapon': self.weapon,
                       'cojvrSingleRoundHand': 0, 'cojvrSingleRoundCompleted': 0,
                       'cojvrManualReloadWeapon': None, 'actual': self.weapon,
                       'desired': self.weapon, 'desired_state': 1, 'dead': False,
                       'm_bNetAttackForced': 0, 'net_owner': True}
        self.ammo, self.reserve, self.events = 2, 10, 0
        self.net_game = False
        self.transitions = []

    def native(self, owner, name, args):
        if name == 'GetHandStateMashine': return self.machine if args[0] == 0 else self.other
        if name in ('GetActualWeapon', 'GetActualWeaponNotEmpty', 'GetActiveWeapon'):
            return self.player['actual'] if args[0] == 0 else self.other_weapon
        if name == 'GetDesiredWeapon': return self.player['desired'] if args[0] == 0 else self.other_weapon
        if name == 'GetDesiredWeaponState': return self.player['desired_state']
        if name == 'GetHandStateMashineState': return self.machine['m_sCurrentState']['id']
        if name == 'GetWeaponMoveState': return 1
        if name == 'GetWeaponReloadState': return 21
        if name == 'SetDesiredWeaponState': self.player['desired_state'] = args[1]; return None
        if name == 'GetNetIsOwner': return self.player['net_owner']
        if name == 'IsNotAlive': return self.player['dead']
        if name == 'IsNetGame': return self.net_game
        if name == 'IsReloadWholeClipAtOnce': return False
        if name == 'getClass': return owner['class']
        if name == 'getName': return owner
        if name == 'equals': return owner == args[0]
        if name == 'GetOtherHand': return 1 - args[0]
        if name == 'HasBowInHands': return False
        if name == 'ReloadRotate': return None
        if name == 'CanAutomaticAttack': return False
        if name == 'GetCurrentStateID': return owner['m_sCurrentState']['id']
        if name == 'FindStateByID': return self.states.get(args[0])
        if name == 'GetCurrentState': return owner['m_sCurrentState']
        if name == 'GetDestState': return owner['m_sDestState']
        if name == 'GetPrevState': return owner.get('m_sPrevState')
        if name == 'SetPrevState': owner['m_sPrevState'] = args[0]; return None
        if name == 'SetCurrentState':
            return self.run('StateMashine', name, '(LBaseState;)V', owner, args)
        if name == 'IsStoped': return owner['m_sCurrentState'] is None
        if name == 'CanChangeState': return True
        if name == 'GetNextState':
            machine = args[0]
            if machine['m_fCurrentTime'] < machine['start'] + .3: return None
            n = owner['id']
            return self.states.get({20: 21, 21: 22, 22: 1}.get(n))
        if name == 'Clear':
            if owner['id'] == 21:
                self.run('ArmedPlayerBeing', 'OnHandStateFinished_Reload', '(IIZ)V', self.player, [args[0]['hand'], 21, False])
            elif owner['id'] == 22:
                self.run('ArmedPlayerBeing', 'OnHandStateFinished_ReloadEnd', '(IIZ)V', self.player, [args[0]['hand'], 22, False])
            return None
        if name == 'Set':
            machine = args[0]; machine['start'] = machine['m_fCurrentTime']
            self.transitions.append(owner['id']); return None
        if name == 'WeaponReload':
            if self.ammo >= 6 or self.reserve <= 0: return False
            self.ammo += 1; self.reserve -= 1; return True
        if name == 'GetWeaponMaxAmmoToReload': return min(6 - self.ammo, self.reserve)
        if name == 'OnGameEvent': self.events += 1; return None
        raise AssertionError('Unmodelled native boundary: ' + name)

    def run(self, cls, name, desc, owner, args=()):
        pool, methods = self.classes[cls]
        method = methods.get((name, desc))
        if method is None or method.code is None: return self.native(owner, name, args)
        code = method.code
        locals_ = [owner, *args] + [None] * method.max_locals
        stack, pc, steps = [], 0, 0
        while pc < len(code):
            steps += 1
            if steps > 20000: raise AssertionError('Unbounded bytecode execution')
            at, op = pc, code[pc]; pc += 1
            if op == 0x00: pass
            elif op == 0x01: stack.append(None)
            elif 0x02 <= op <= 0x08: stack.append(op - 3)
            elif 0x0b <= op <= 0x0d: stack.append(float(op - 0x0b))
            elif op in (0x10, 0x11):
                n = 1 if op == 0x10 else 2
                stack.append(int.from_bytes(code[pc:pc+n], 'big', signed=True)); pc += n
            elif op == 0x13:
                idx = int.from_bytes(code[pc:pc+2], 'big'); pc += 2
                entry = pool.get(idx)
                stack.append(pool.utf8(entry[1]) if entry[0] == 'String' else entry[1])
            elif op in (0x15, 0x17, 0x19): stack.append(locals_[code[pc]]); pc += 1
            elif 0x1a <= op <= 0x1d: stack.append(locals_[op - 0x1a])
            elif 0x2a <= op <= 0x2d: stack.append(locals_[op - 0x2a])
            elif op in (0x36, 0x38, 0x3a): locals_[code[pc]] = stack.pop(); pc += 1
            elif 0x3b <= op <= 0x3e: locals_[op - 0x3b] = stack.pop()
            elif 0x4b <= op <= 0x4e: locals_[op - 0x4b] = stack.pop()
            elif op == 0x59: stack.append(stack[-1])
            elif op == 0x62: stack.append(stack.pop() + stack.pop())
            elif op == 0x66:
                b,a=stack.pop(),stack.pop(); stack.append(a-b)
            elif op in (0x95, 0x96):
                b, a = stack.pop(), stack.pop()
                stack.append((-1 if op == 0x95 else 1) if a != a or b != b else (a > b) - (a < b))
            elif op in (0xb2, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8):
                idx = int.from_bytes(code[pc:pc+2], 'big'); pc += 2
                entry = pool.get(idx); class_name = pool.describe(entry[1])
                nt = pool.get(entry[2]); key, signature = pool.utf8(nt[1]), pool.utf8(nt[2])
                if op == 0xb2: stack.append({} if key == 'cSingleton' else 1)
                elif op == 0xb4: stack.append(stack.pop().get(key, None if signature.startswith('L') else 0))
                elif op == 0xb5:
                    val = stack.pop(); stack.pop()[key] = val
                else:
                    # Descriptors used here have only one-slot primitives/objects.
                    import re
                    types = re.findall(r'L[^;]+;|[IZF]', signature[1:signature.index(')')])
                    argv = [stack.pop() for _ in types][::-1]
                    target = None if op == 0xb8 else stack.pop()
                    interpreted = key.startswith('cojvr') or (class_name, key) in {
                        ('StateMashine', 'Update'), ('StateMashine', 'SetCurrentState'),
                        ('ArmedPlayerBeing', 'OnHandStateFinished_Reload'),
                        ('ArmedPlayerBeing', 'OnHandStateFinished_ReloadEnd')}
                    if interpreted and class_name in self.classes and (key, signature) in self.classes[class_name][1]:
                        result = self.run(class_name, key, signature, target, argv)
                    else: result = self.native(target, key, argv)
                    if signature[-1] != 'V': stack.append(result)
            elif op in (0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa5, 0xa6, 0xa7, 0xc6, 0xc7):
                delta = int.from_bytes(code[pc:pc+2], 'big', signed=True); pc += 2
                if op == 0xa7: take = True
                elif op in (0xa5, 0xa6):
                    b, a = stack.pop(), stack.pop(); take = (a is b) if op == 0xa5 else (a is not b)
                elif op in (0x9f, 0xa0):
                    b, a = stack.pop(), stack.pop(); take = (a == b) if op == 0x9f else (a != b)
                else:
                    a = stack.pop()
                    if op == 0xc6: take = a is None
                    elif op == 0xc7: take = a is not None
                    else: take = {0x99: a == 0, 0x9a: a != 0, 0x9b: a < 0, 0x9c: a >= 0, 0x9d: a > 0, 0x9e: a <= 0}[op]
                if take: pc = at + delta
            elif op in (0xac, 0xb0): return stack.pop()
            elif op == 0xb1: return None
            else: raise AssertionError(f'Unimplemented opcode {op:02x} at {cls}.{name}:{at}')
        raise AssertionError('Missing return')

    def arm(self):
        self.player['cojvrManualReloadWeapon'] = self.weapon
        self.run('ArmedPlayerBeing', 'ReloadWeapon', '(I)V', self.player, [0])
        # Native scheduling of the accepted request starts the begin segment;
        # its motor/skin is outside this bytecode interpreter.
        if self.player['desired_state'] == 21:
            self.machine['m_sCurrentState'] = self.states[20]

    def update(self, t, machine=None):
        m = machine or self.machine
        m['m_fCurrentTime'] = t
        return self.run('StateMashine', 'Update', '()Z', m)


class ReloadWaitProbeTests(unittest.TestCase):
    def test_finished_body_waits_before_clear_and_timeout_closes_without_ammo(self):
        vm = VM(); vm.arm()
        vm.update(.31); self.assertEqual(vm.machine['m_sCurrentState']['id'], 21)
        vm.update(.62)
        self.assertEqual(vm.machine['m_sCurrentState']['id'], 21, 'Missing native wait before Clear')
        self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))
        vm.update(1.5); self.assertEqual(vm.machine['m_sCurrentState']['id'], 21)
        vm.update(2.2); self.assertEqual(vm.machine['m_sCurrentState']['id'], 22)
        self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))
        vm.update(2.6); self.assertEqual(vm.machine['m_sCurrentState']['id'], 1)
        self.assertIsNone(vm.player['cojvrReloadProbeWeapon'])
        self.assertIsNone(vm.machine['cojvrReloadProbeOwner'])
        vm.arm(); vm.update(2.91); vm.update(3.22)
        self.assertEqual(vm.machine['m_sCurrentState']['id'], 21)
        self.assertEqual(vm.player['cojvrReloadProbeStatus'], 2)
        self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))

    def test_explicit_cancel_during_opening_and_wait_retains_native_clear_set(self):
        for waiting in (False, True):
            with self.subTest(waiting=waiting):
                vm = VM(); vm.arm()
                if waiting: vm.update(.31); vm.update(.62)
                vm.run('ArmedPlayerBeing', 'cojvrCancelReloadProbe', '()V', vm.player)
                self.assertEqual(vm.machine['m_sCurrentState']['id'], 22)
                self.assertEqual(vm.player['desired_state'], 1)
                self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))
                self.assertIn(22, vm.transitions)
                vm.run('ArmedPlayerBeing', 'cojvrCancelReloadProbe', '()V', vm.player)
                self.assertEqual(vm.transitions.count(22), 1)

    def test_owner_loss_and_death_network_interrupt_without_transfer(self):
        for key, val in [('dead', True), ('net_owner', False), ('m_bNetAttackForced', 1),
                         ('desired_state', 1), ('actual', {'class': 'replacement'}),
                         ('desired', {'class': 'replacement'})]:
            with self.subTest(key=key):
                vm = VM(); vm.arm(); vm.update(.31)
                vm.player[key] = val
                vm.update(.62)
                self.assertEqual(vm.machine['m_sCurrentState']['id'], 22)
                self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))

    def test_unrelated_machine_and_ordinary_reload_remain_native(self):
        vm = VM(); vm.arm()
        vm.other_weapon = {'class': 'unrelated'}
        vm.other['m_sCurrentState'] = vm.states[20]
        vm.update(.31, vm.other); vm.update(.62, vm.other)
        self.assertEqual(vm.other['m_sCurrentState']['id'], 22)
        self.assertEqual((vm.ammo, vm.reserve, vm.events), (3, 9, 1))
        native = VM(False); native.arm(); native.update(.31); native.update(.62)
        self.assertEqual((native.ammo, native.reserve, native.events), (3, 9, 1))

    def test_full_gun_zero_reserve_and_repeated_cleanup_do_not_transfer(self):
        for ammo, reserve in [(6, 10), (2, 0), (2, 10)]:
            with self.subTest(ammo=ammo, reserve=reserve):
                vm = VM(); vm.ammo, vm.reserve = ammo, reserve
                vm.arm(); vm.update(.31); vm.update(.62)
                for _ in range(3):
                    vm.run('ArmedPlayerBeing', 'OnHandStateFinished_Reload', '(IIZ)V', vm.player, [0, 21, False])
                self.assertEqual((vm.ammo, vm.reserve, vm.events), (ammo, reserve, 0))

    def test_repeated_request_while_waiting_cannot_restart_deadline_or_queue_ammo(self):
        vm = VM(); vm.arm(); vm.update(.31); vm.update(.62)
        until = vm.player['cojvrReloadProbeUntil']
        for _ in range(3):
            vm.player['cojvrManualReloadWeapon'] = vm.weapon
            vm.run('ArmedPlayerBeing', 'ReloadWeapon', '(I)V', vm.player, [0])
        self.assertEqual(vm.machine['m_sCurrentState']['id'], 21)
        self.assertEqual(vm.player['cojvrReloadProbeUntil'], until)
        vm.update(2.2); vm.update(2.6)
        self.assertEqual(vm.machine['m_sCurrentState']['id'], 1)
        self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))

    def test_ordinary_request_without_ticket_keeps_native_transfer_in_probe_archive(self):
        vm = VM(); vm.player['cojvrSingleRoundWeapon'] = None
        vm.run('ArmedPlayerBeing', 'ReloadWeapon', '(I)V', vm.player, [0])
        self.assertEqual(vm.player['desired_state'], 21)
        self.assertIsNone(vm.player.get('cojvrReloadProbeWeapon'))
        vm.machine['m_sCurrentState'] = vm.states[20]
        vm.update(.31); vm.update(.62)
        self.assertEqual((vm.ammo, vm.reserve, vm.events), (3, 9, 1))

    def test_clear_releases_only_owned_veto_and_permits_next_request(self):
        vm = VM(); vm.arm(); vm.update(.31); vm.update(.62)
        vm.run('ArmedPlayerBeing', 'cojvrClearReloadProbe', '(I)V', vm.player, [1])
        self.assertIs(vm.machine['cojvrReloadProbeOwner'], vm.player)
        vm.run('ArmedPlayerBeing', 'cojvrCancelReloadProbe', '()V', vm.player)
        vm.run('ArmedPlayerBeing', 'cojvrClearReloadProbe', '(I)V', vm.player, [0])
        self.assertIsNone(vm.machine['cojvrReloadProbeOwner'])
        self.assertIsNone(vm.player['cojvrReloadProbeWeapon'])
        foreign = {}; vm.machine['cojvrReloadProbeOwner'] = foreign
        vm.run('ArmedPlayerBeing', 'cojvrClearReloadProbe', '(I)V', vm.player, [0])
        self.assertIs(vm.machine['cojvrReloadProbeOwner'], foreign)

    def test_unknown_binary_rejected_before_archive_output(self):
        with self.assertRaisesRegex(ValueError, 'StateMashine.class SHA-256'):
            patch.patch_reload_probe_classes(original('ArmedPlayerBeing'), b'unknown')
        with self.assertRaisesRegex(ValueError, 'ArmedPlayerBeing.class SHA-256'):
            patch.patch_reload_probe_classes(b'unknown', original('StateMashine'))

    def test_archive_opt_in_preserves_every_unrelated_payload(self):
        import tempfile
        source = ARCHIVE
        if not source.exists(): raise unittest.SkipTest('Shipped archive unavailable')
        if hashlib.sha256(source.read_bytes()).hexdigest() != patch.PAK_SHA256:
            source = source.with_name('code.cojvr-backup.pak')
        with tempfile.TemporaryDirectory(prefix='cojvr-reload-probe-test-') as temp:
            ordinary, probe = Path(temp) / 'ordinary.pak', Path(temp) / 'probe.pak'
            patch.patch_archive(source, ordinary)
            patch.patch_archive(source, probe, reload_wait_probe=True)
            with zipfile.ZipFile(source) as native, zipfile.ZipFile(ordinary) as old, zipfile.ZipFile(probe) as new:
                self.assertEqual(old.read('StateMashine.class'), native.read('StateMashine.class'))
                self.assertNotEqual(new.read('StateMashine.class'), native.read('StateMashine.class'))
                for name in old.namelist():
                    if name not in ('StateMashine.class', 'ArmedPlayerBeing.class'):
                        self.assertEqual(new.read(name), old.read(name), name)
                self.assertIsNone(new.testzip())
            with self.assertRaisesRegex(ValueError, 'new separate file'):
                patch.patch_archive(source, probe, reload_wait_probe=True)

    def test_clock_rollback_or_nonfinite_clock_closes_without_transfer(self):
        for t in (.1, float('nan'), float('inf')):
            with self.subTest(clock=t):
                vm = VM(); vm.arm(); vm.update(.31); vm.update(.62)
                # Exercise the approved pre-Clear guard directly; the scheduling
                # stand-in otherwise rejects an early time sample itself.
                vm.machine['m_fCurrentTime'] = t
                allowed = vm.run('ArmedPlayerBeing', 'cojvrAllowReloadProbeTransition',
                                 '(LStateMashine;)Z', vm.player, [vm.machine])
                self.assertTrue(allowed)
                vm.run('ArmedPlayerBeing', 'OnHandStateFinished_Reload', '(IIZ)V', vm.player, [0, 21, False])
                self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))

    def test_missing_closing_state_or_foreign_veto_prevents_probe_admission(self):
        for unavailable in ('state', 'veto'):
            with self.subTest(unavailable=unavailable):
                vm = VM()
                if unavailable == 'state': del vm.states[22]
                else: vm.machine['cojvrReloadProbeOwner'] = object()
                vm.arm()
                self.assertIsNone(vm.player.get('cojvrReloadProbeWeapon'))
                self.assertEqual(vm.machine['m_sCurrentState']['id'], 1)
                self.assertEqual(vm.player['desired_state'], 1)
                self.assertIsNone(vm.player['cojvrSingleRoundWeapon'])
                self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))

    def test_probe_branch_targets_and_native_save_order(self):
        armed, machine = patch.patch_reload_probe_classes(original('ArmedPlayerBeing'), original('StateMashine'))
        for data in (armed, machine):
            pool, methods = parse_class(data)
            for m in methods:
                if m.code is None: continue
                lines = list(disassemble(m.code, pool))
                positions = {int(s.split(':')[0]) for s in lines}
                for at in positions:
                    op = m.code[at]
                    if 0x99 <= op <= 0xa8 or op in (0xc6, 0xc7):
                        target = at + int.from_bytes(m.code[at+1:at+3], 'big', signed=True)
                        self.assertIn(target, positions, m.name)
        pool, methods = parse_class(armed)
        for name in ('SGSaveChunk', 'SGLoadChunk'):
            m = next(m for m in methods if m.name == name)
            lines = list(disassemble(m.code, pool))
            self.assertIn('cojvrCancelReloadProbe', lines[1])
            self.assertIn('InventoryBeing.' + name, '\n'.join(lines))
        helpers = '\n'.join(line for m in methods if m.name.startswith('cojvr') and 'Probe' in m.name
                            for line in disassemble(m.code, pool))
        self.assertNotIn('WeaponReload(', helpers)
        self.assertNotIn('nAmmoPistol', helpers)

    def test_every_probe_symbol_resolves_against_shipped_hierarchy(self):
        from inspect_java_bytecode import Reader, parse_constant_pool
        armed, machine = patch.patch_reload_probe_classes(original('ArmedPlayerBeing'), original('StateMashine'))
        patched = {'ArmedPlayerBeing': armed, 'StateMashine': machine}
        cache = {}
        def declared(cls):
            if cls in cache: return cache[cls]
            r = Reader(patched[cls] if cls in patched else original(cls)); r.read(8)
            pool = parse_constant_pool(r); r.u2(); r.u2(); parent_id = r.u2()
            parent = pool.describe(parent_id) if parent_id else None
            r.read(r.u2() * 2); members = set()
            for _ in range(2): # fields, then methods
                for _ in range(r.u2()):
                    r.u2(); name, desc = pool.utf8(r.u2()), pool.utf8(r.u2()); members.add((name, desc))
                    for _ in range(r.u2()): r.u2(); r.read(r.u4())
            cache[cls] = parent, members
            return parent, members
        def resolves(cls, name, desc):
            while cls and not cls.startswith('java/'):
                cls, members = declared(cls)
                if (name, desc) in members: return True
            return False
        for cls, data in patched.items():
            pool, _ = parse_class(data)
            base, _ = parse_class(patch.patch_class(original(cls)) if cls == 'ArmedPlayerBeing' else original(cls))
            old = {base.describe(i) for i, e in enumerate(base.entries) if e is not None and i}
            for i, entry in enumerate(pool.entries):
                if not i or entry is None or entry[0] not in ('Methodref', 'Fieldref') or pool.describe(i) in old: continue
                owner = pool.describe(entry[1]); nt = pool.get(entry[2])
                name, desc = pool.utf8(nt[1]), pool.utf8(nt[2])
                self.assertTrue(resolves(owner, name, desc), pool.describe(i))

    def test_machine_replacement_finishes_old_body_before_detaching_probe(self):
        vm = VM(); vm.arm(); vm.update(.31); vm.update(.62)
        old = vm.machine
        vm.machine = dict(vm.other); vm.machine['hand'] = 0
        vm.player['actual'] = vm.player['desired'] = {'class': 'replacement'}
        vm.run('ArmedPlayerBeing', 'cojvrCheckReloadProbeOwner', '(I)V', vm.player, [0])
        self.assertEqual(old['m_sCurrentState']['id'], 22)
        self.assertIsNone(old['cojvrReloadProbeOwner'])
        self.assertIsNone(vm.player['cojvrReloadProbeWeapon'])
        self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))


if __name__ == '__main__': unittest.main()
