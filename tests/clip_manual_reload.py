"""Interpreted Schofield clip transaction; native animation/skins are doubles."""
from pathlib import Path
import hashlib
import io
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
sys.path.insert(0, str(Path(__file__).parent))

import coj_clip_reload as clips
from manual_reload_native import ManualVM
from reload_wait_probe import original, JVMThrown
from inspect_java_bytecode import parse_class, disassemble
from patch_coj_weapon_consumers import patch_manual_reload_classes, Code, u2, u4
from indexed_manual_reload import original_archive
from runtime_manual_families import FamilyVM
from reload_wait_probe import VM


def clip_verifier_class():
    """Tiny Java 1.4 main: load/verify ArmedPlayerBeing without game startup."""
    pool = []
    def add(raw):
        pool.append(raw)
        return len(pool)
    def utf(value):
        raw = value.encode('utf-8')
        return add(b'\x01' + u2(len(raw)) + raw)
    def cls(value): return add(b'\x07' + u2(utf(value)))
    def method(owner, name, descriptor):
        signature = add(b'\x0c' + u2(utf(name)) + u2(utf(descriptor)))
        return add(b'\x0a' + u2(cls(owner)) + u2(signature))
    this, parent = cls('VerifyManualClip'), cls('java/lang/Object')
    main, signature, code_name = utf('main'), utf('([Ljava/lang/String;)V'), utf('Code')
    target = add(b'\x08' + u2(utf('ArmedPlayerBeing')))
    loader = method('java/lang/ClassLoader', 'getSystemClassLoader', '()Ljava/lang/ClassLoader;')
    for_name = method('java/lang/Class', 'forName',
                      '(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;')
    methods = method('java/lang/Class', 'getDeclaredMethods', '()[Ljava/lang/reflect/Method;')
    code = Code()
    code.ref(0x13, target); code.emit(0x03)
    code.ref(0xb8, loader); code.ref(0xb8, for_name)
    code.ref(0xb6, methods); code.emit(0x57, 0xb1)
    body = u2(4) + u2(1) + u4(len(code.finish())) + code.finish() + u2(0) + u2(0)
    return (bytes.fromhex('cafebabe0000002e') + u2(len(pool) + 1) + b''.join(pool) +
            u2(0x21) + u2(this) + u2(parent) + u2(0) + u2(0) + u2(1) +
            u2(9) + u2(main) + u2(signature) + u2(1) + u2(code_name) +
            u4(len(body)) + body + u2(0))


class ClipVM(ManualVM):
    def __init__(self, weapon='WeaponPistolSchofield'):
        super().__init__()
        player, machine = clips.patch_offline_clip_classes(original('ArmedPlayerBeing'),
                                                          original('StateMashine'), weapon)
        for cls, value in (('ArmedPlayerBeing', player), ('StateMashine', machine)):
            pool, methods = parse_class(value)
            self.classes[cls] = pool, {(m.name, m.descriptor): m for m in methods}
        self.weapon.update({'class': weapon, 'whole_clip': True})
        self.weapon['cOwner'] = self.player
        self.counter = {'amount': self.reserve}
        self.calls = 0
        self.transferred = True
        self.on_event = None
        self.on_close = None

    def native(self, owner, name, args):
        if name == 'GetAmmoCount': return self.ammo
        if name == 'GetAmmoType': return 2
        if name == 'GetAmmoCapacity': return 6
        if name == 'FindAmmoCounter': return self.counter if args[0] == 2 else None
        if name == 'GetAmount': return owner['amount']
        if name == 'WeaponReload':
            self.calls += 1
            if not self.transferred: return True  # simulate false-positive native success
            count = min(6 - self.ammo, self.counter['amount'])
            if count < 1: return False
            self.ammo += count
            self.counter['amount'] -= count
            return True
        if name == 'GetWeaponMaxAmmoToReload':
            return min(6 - self.ammo, self.counter['amount'])
        if name == 'OnGameEvent':
            result = super().native(owner, name, args)
            if self.on_event: self.on_event()
            return result
        if name == 'Set':
            result = super().native(owner, name, args)
            if owner['id'] == 22 and self.on_close: self.on_close()
            return result
        return super().native(owner, name, args)


class OfflineClipTests(unittest.TestCase):
    def test_whole_clip_inserts_exact_remaining_capacity_once(self):
        for weapon in clips.CLIP_WEAPONS:
            for amount in (1, 2, 4, 9):
                with self.subTest(weapon=weapon, amount=amount):
                    vm = ClipVM(weapon); vm.wait(); vm.counter['amount'] = amount
                    self.assertTrue(vm.call('cojvrInsertManualClip'))
                    transfer = min(amount, 4)
                    self.assertEqual((vm.ammo, vm.counter['amount'], vm.events, vm.calls),
                                     (2 + transfer, amount - transfer, 1, 1))
                    self.assertEqual(vm.machine['m_sCurrentState']['id'], 22)
                    self.assertFalse(vm.call('cojvrInsertManualClip'))
                    vm.update(1.)
                    self.assertEqual(vm.events, 1)

    def test_fail_closed_on_invalid_owner_state_and_transfer(self):
        for change in ('subclass', 'single_round', 'other_hand', 'dead',
                       'changed_desired', 'no_reserve', 'full', 'false_positive'):
            with self.subTest(change=change):
                vm = ClipVM(); vm.wait()
                if change == 'subclass': vm.weapon['class'] = 'DerivedSchofield'
                if change == 'single_round': vm.weapon['whole_clip'] = False
                if change == 'other_hand': vm.other_weapon = {'class': 'foreign'}
                if change == 'dead': vm.player['dead'] = True
                if change == 'changed_desired': vm.player['desired'] = {'class': 'foreign'}
                if change == 'no_reserve': vm.counter['amount'] = 0
                if change == 'full': vm.ammo = 6
                if change == 'false_positive': vm.transferred = False
                self.assertFalse(vm.call('cojvrInsertManualClip'))
                self.assertEqual(vm.events, 0)
                self.assertEqual(vm.ammo, 6 if change == 'full' else 2)
                self.assertEqual(vm.calls, 1 if change == 'false_positive' else 0)
                if change == 'false_positive':
                    vm.transferred = True
                    self.assertFalse(vm.call('cojvrInsertManualClip'))
                    self.assertEqual(vm.calls, 1)

    def test_production_clip_composition_and_exact_archive_pinning(self):
        p, m = original('ArmedPlayerBeing'), original('StateMashine')
        first, second = patch_manual_reload_classes(p, m)
        signatures=[(method.name,method.descriptor) for method in parse_class(first)[1]]
        self.assertEqual(len(signatures),len(set(signatures)))
        self.assertIn(('cojvrInsertManualClip','()Z'),signatures)
        # Production now deliberately composes clip and unit families. Exercise
        # that derivative rather than pinning the obsolete revolver-only bytes.
        from runtime_manual_families import FamilyVM
        for weapon in clips.CLIP_WEAPONS:
            vm=FamilyVM(weapon);vm.wait();vm.player['nAmmoPistol']=3
            self.assertTrue(vm.boolean('cojvrInsertManualRound'))
            self.assertEqual((vm.weapon['m_iAmmo0Count'],vm.player['nAmmoPistol'],
                              vm.events,vm.clip_calls),(5,0,1,1))
            self.assertEqual(vm.machine['m_sCurrentState']['id'],22)
        for weapon in ('WeaponPistolSchofield_C','WeaponPistolSchofield_A_Old','DerivedSchofield'):
            vm=FamilyVM(clips.CLIP_WEAPONS[0]);vm.weapon['class']=weapon;vm.wait()
            self.assertFalse(vm.boolean('cojvrInsertManualRound'))
            self.assertFalse(vm.insert_clip())
            self.assertEqual((vm.weapon['m_iAmmo0Count'],vm.player['nAmmoPistol'],
                              vm.events,vm.clip_calls),(2,10,0,0))
        for weapon in ('WeaponPistolWalker', 'WeaponRifleWinchester', 'WeaponPistolSchofield_C'):
            with self.assertRaises(ValueError): clips.patch_offline_clip_classes(p, m, weapon)
        for bad_p, bad_m in ((p + b'X', m), (p, m + b'X')):
            with self.assertRaises(ValueError):
                clips.patch_offline_clip_classes(bad_p, bad_m, clips.CLIP_WEAPONS[0])
            with self.assertRaises(ValueError):patch_manual_reload_classes(bad_p,bad_m)
        with self.assertRaises(ValueError):patch_manual_reload_classes(first,second)

        from reload_wait_probe import ARCHIVE
        source = next((path for path in (ARCHIVE, ARCHIVE.with_name('code.cojvr-backup.pak'))
                       if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest()
                       == clips.PAK_SHA256), None)
        if source is not None:
            original_archive = source.read_bytes()
            self.assertEqual(clips.build_offline_clip_lab(original_archive, clips.CLIP_WEAPONS[0]),
                             clips.patch_offline_clip_classes(p, m, clips.CLIP_WEAPONS[0]))
            with self.assertRaisesRegex(ValueError, 'SHA-256'):
                clips.build_offline_clip_lab(original_archive + b'X', clips.CLIP_WEAPONS[0])

    def test_event_callback_changes_quarantine_future_insertion(self):
        vm = ClipVM(); vm.wait()
        vm.on_event = lambda: vm.counter.update(amount=99)
        self.assertFalse(vm.call('cojvrInsertManualClip'))
        self.assertEqual((vm.ammo, vm.events, vm.calls), (6, 1, 1))
        self.assertTrue(vm.player['cojvrClipFaulted'])
        self.assertFalse(vm.call('cojvrKeepManualReloadAlive'))
        vm.on_event = None
        self.assertFalse(vm.call('cojvrInsertManualClip'))
        self.assertEqual(vm.calls, 1)

    def test_busy_or_faulted_clip_cannot_keep_native_reload_waiting(self):
        vm = ClipVM(); vm.wait()
        self.assertTrue(vm.call('cojvrKeepManualReloadAlive'))
        vm.player['cojvrClipBusy'] = True
        self.assertFalse(vm.call('cojvrKeepManualReloadAlive'))
        vm.player['cojvrClipBusy'] = False
        vm.player['cojvrClipFaulted'] = True
        self.assertFalse(vm.call('cojvrKeepManualReloadAlive'))

    def test_native_weapon_owner_change_rejects_clip_before_and_after_event(self):
        vm = ClipVM(); vm.wait()
        vm.weapon['cOwner'] = {'class': 'foreign'}
        self.assertFalse(vm.call('cojvrInsertManualClip'))
        self.assertEqual((vm.ammo, vm.calls, vm.events), (2, 0, 0))

        vm = ClipVM(); vm.wait()
        vm.on_event = lambda: vm.weapon.update(cOwner={'class': 'foreign'})
        self.assertFalse(vm.call('cojvrInsertManualClip'))
        self.assertEqual((vm.ammo, vm.calls, vm.events), (6, 1, 1))
        self.assertTrue(vm.player['cojvrClipFaulted'])
        self.assertFalse(vm.call('cojvrKeepManualReloadAlive'))

    def test_close_callback_must_preserve_exact_clip_accounting_and_owner(self):
        for change in ('loaded', 'reserve', 'owner', 'ticket', 'state', 'reentry'):
            with self.subTest(change=change):
                vm = ClipVM(); vm.wait(); replies = []
                def close():
                    if change == 'loaded': vm.ammo -= 1
                    if change == 'reserve': vm.counter['amount'] += 1
                    if change == 'owner': vm.weapon['cOwner'] = {'class': 'foreign'}
                    if change == 'ticket': vm.player['cojvrReloadProbeMachine'] = vm.other
                    if change == 'state': vm.machine['m_sCurrentState'] = vm.states[1]
                    if change == 'reentry': replies.append(vm.call('cojvrInsertManualClip'))
                vm.on_close = close
                self.assertEqual(vm.call('cojvrInsertManualClip'), change == 'reentry')
                self.assertEqual(vm.calls, 1)
                self.assertEqual(vm.events, 1)
                self.assertFalse(vm.player['cojvrClipBusy'])
                if change == 'reentry':
                    self.assertEqual(replies, [False])
                    self.assertFalse(vm.player.get('cojvrClipFaulted', False))
                else:
                    self.assertTrue(vm.player['cojvrClipFaulted'])
                    self.assertFalse(vm.call('cojvrKeepManualReloadAlive'))
                    self.assertFalse(vm.call('cojvrInsertManualClip'))

    def test_reentrant_clip_and_native_exception_quarantine(self):
        vm = ClipVM(); vm.wait(); reentrant = []
        vm.on_event = lambda: reentrant.append(vm.call('cojvrInsertManualClip'))
        self.assertTrue(vm.call('cojvrInsertManualClip'))
        self.assertEqual((reentrant, vm.calls, vm.events), ([False], 1, 1))

        vm = ClipVM(); vm.wait()
        def raises_after_transfer():
            raise JVMThrown({'class': 'Throwable', 'point': 'event'})
        vm.on_event = raises_after_transfer
        with self.assertRaises(JVMThrown):
            vm.call('cojvrInsertManualClip')
        self.assertFalse(vm.player['cojvrClipBusy'])
        self.assertTrue(vm.player['cojvrClipFaulted'])
        self.assertFalse(vm.call('cojvrInsertManualClip'))
        self.assertEqual((vm.calls, vm.events), (1, 1))

        vm = ClipVM(); vm.wait()
        exception = {'class': 'Throwable', 'point': 'native_close'}
        def fail_close(): raise JVMThrown(exception)
        vm.on_close = fail_close
        with self.assertRaises(JVMThrown) as caught:
            vm.call('cojvrInsertManualClip')
        self.assertIs(caught.exception.value, exception)
        self.assertTrue(vm.player['cojvrClipFaulted'])
        self.assertFalse(vm.player['cojvrClipBusy'])
        self.assertFalse(vm.call('cojvrInsertManualClip'))
        self.assertEqual((vm.calls, vm.events), (1, 1))

    def test_shipped_jvm_verifies_offline_clip_bytecode(self):
        from reload_wait_probe import ARCHIVE
        java = ARCHIVE.parent / 'jre/bin/java.exe'
        source = next((p for p in (ARCHIVE, ARCHIVE.with_name('code.cojvr-backup.pak'))
                       if p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest()
                       == clips.PAK_SHA256), None)
        if source is None or not java.is_file():
            self.skipTest('Exact original PAK and shipped Java 1.4 runtime required')
        raw = source.read_bytes()
        with tempfile.TemporaryDirectory(prefix='cojvr-clip-verify-') as temp:
            root = Path(temp)
            (root / 'VerifyManualClip.class').write_bytes(clip_verifier_class())
            def verify(path):
                return subprocess.run([str(java), '-Xverify:all', '-cp',
                                       str(root) + ';' + str(path), 'VerifyManualClip'],
                                      capture_output=True, text=True, timeout=30)
            from coj_schofield_unload import build_offline_schofield_unload
            variants = {weapon: clips.build_offline_clip_lab(raw, weapon) for weapon in clips.CLIP_WEAPONS}
            variants['SchofieldMotion'] = build_offline_schofield_unload(raw)
            for variant, (player, machine) in variants.items():
                target = root / (variant + '.pak')
                with zipfile.ZipFile(source) as original_archive, zipfile.ZipFile(target, 'w') as out:
                    replacement = {'ArmedPlayerBeing.class': player, 'StateMashine.class': machine}
                    for info in original_archive.infolist():
                        out.writestr(info, replacement.get(info.filename,
                                                          original_archive.read(info.filename)))
                result = verify(target)
                self.assertEqual(result.returncode, 0, variant + ': ' + result.stdout + result.stderr)
                signatures = (('cojvrOpenSchofield', '(I)Z'), ('cojvrUnloadSchofield', '()I'),
                              ('cojvrInsertSchofieldBundle', '()Z')) if variant == 'SchofieldMotion' else (
                                  (('cojvrInsertManualClip', '()Z'),) if variant == clips.CLIP_WEAPONS[0] else ())
                for signature in signatures:
                    _, methods = parse_class(player)
                    helper_code = next(m.code for m in methods
                                       if (m.name, m.descriptor) == signature)
                    self.assertEqual(player.count(helper_code), 1)
                    invalid_player = player.replace(helper_code, b'\xfe' + helper_code[1:], 1)
                    invalid_archive = root / 'bad-clip.pak'
                    with zipfile.ZipFile(target) as good, zipfile.ZipFile(invalid_archive, 'w') as bad:
                        for info in good.infolist():
                            bad.writestr(info, invalid_player if info.filename == 'ArmedPlayerBeing.class'
                                         else good.read(info.filename))
                    rejected = verify(invalid_archive)
                    self.assertNotEqual(rejected.returncode, 0, 'Invalid clip helper bypassed JVM verification')
                    self.assertIn('VerifyError', rejected.stdout + rejected.stderr)

    def test_clip_uses_one_native_transfer_and_no_direct_setters(self):
        vm = ClipVM()
        pool, methods = vm.classes['ArmedPlayerBeing']
        self.assertNotIn(('cojvrInsertManualRound', '()Z'), methods)
        code = '\n'.join(disassemble(methods[('cojvrInsertManualClip', '()Z')].code, pool))
        self.assertEqual(code.count('WeaponReload(LWeapon;)Z'), 1)
        self.assertNotIn('SetAmmoCount', code)
        self.assertNotIn('SetAmount', code)


class SchofieldVM(FamilyVM):
    def __init__(self, loaded=2, reserve=10, *, hand=0, kind='WeaponPistolSchofield', waiting=True):
        super().__init__(kind, hand)
        with zipfile.ZipFile(io.BytesIO(original_archive())) as archive:
            pool, methods = parse_class(archive.read('PawnInventory.class'))
        self.classes['PawnInventory'] = pool, {(m.name, m.descriptor): m for m in methods}
        self.unload_calls = 0
        self.requests = []
        self.on_request = None
        self.weapon['m_iAmmo0Count'] = loaded
        self.player['nAmmoPistol'] = reserve
        if waiting:
            self.wait()
        else:
            armed = self.player['m_aHandStateMashine'][hand]
            support = self.player['m_aHandStateMashine'][1-hand]
            armed['m_cActualState']['m_iState'] = 1
            support['m_cDesiredState']['m_iState'] = 0
            self.machine['m_sDestState'] = self.states[1]
            empty = {'class': 'PlayerState', 'id': 0, 'm_iStateID': 0}
            self.other.update(m_sCurrentState=empty, m_sDestState=empty)
            self.player['cojvrManualReloadWeapon'] = self.weapon

    def _native(self, owner, name, args):
        if name == 'ReloadWeapon':
            self.requests.append((self.special_class, tuple(args)))
            self.run('ArmedPlayerBeing', name, '(I)V', owner, args)
            if self.on_request:
                self.on_request()
            return None
        if name == 'SetDesiredWeaponState':
            hand, state = args
            # The shipped setter classifies reload states 20–22 as two-handed
            # even when the equipped Schofield and the idle pose use one hand.
            desired = self.player['m_aHandStateMashine'][hand]['m_cDesiredState']
            desired['m_bTwoHanded'] = state in (20, 21, 22, 25, 26, 27)
        if name == 'AddAmmo':
            self.unload_calls += 1
            return VM.run(self, 'PawnInventory', name, '(LWeapon;Z)Z', owner, args)
        if name == 'min': return min(args)
        if name == 'PlaySound2D': return 0
        return super()._native(owner, name, args)

    def unload(self):
        return self.run('ArmedPlayerBeing', 'cojvrUnloadSchofield', '()I', self.player) == 1

    def open(self, hand=None):
        return self.run('ArmedPlayerBeing', 'cojvrOpenSchofield', '(I)Z',
                        self.player, [self.hand if hand is None else hand])

    def insert_bundle(self):
        return self.boolean('cojvrInsertSchofieldBundle')


class SchofieldMotionTests(unittest.TestCase):
    def test_partial_bundle_leaves_the_native_drum_open(self):
        vm = SchofieldVM(loaded=3, reserve=2)
        self.assertTrue(vm.insert_bundle())
        self.assertEqual((vm.vector()[0][0], vm.vector()[1][2], vm.clip_calls), (5, 0, 1))
        self.assertEqual(vm.player['cojvrReloadProbeStatus'], 2)

    def test_constructor_only_variants_share_the_motion_transaction(self):
        for kind in clips.CLIP_WEAPONS[1:]:
            for hand in (0, 1):
                with self.subTest(kind=kind, hand=hand):
                    vm = SchofieldVM(loaded=5, reserve=94, kind=kind, hand=hand, waiting=False)
                    self.assertTrue(vm.open())
                    vm.machine.update(m_sCurrentState=vm.states[20], m_sDestState=vm.states[21])
                    vm.update(.31); vm.update(.62)
                    self.assertTrue(vm.unload())
                    self.assertEqual(vm.vector(), ((0, 0), (10, 10, 99)))
                    self.assertTrue(vm.insert_bundle())
                    self.assertEqual(vm.vector(), ((6, 0), (10, 10, 93)))
                    self.assertEqual(vm.player['cojvrReloadProbeStatus'], 2)
                    vm.run('ArmedPlayerBeing', 'cojvrCancelReloadProbe', '()V', vm.player)
                    vm.update(1.0); vm.update(1.4)
                    self.assertEqual(vm.vector(), ((6, 0), (10, 10, 93)))

    def test_opens_full_or_zero_reserve_and_keeps_bundle_waiting_until_native_close(self):
        for hand in (0, 1):
            for loaded, reserve in ((5, 77), (1, 75), (6, 3), (2, 0), (6, 0), (0, 0)):
                with self.subTest(hand=hand, loaded=loaded, reserve=reserve):
                    vm = SchofieldVM(loaded, reserve, hand=hand, waiting=False)
                    before = vm.vector()
                    self.assertTrue(vm.open())
                    self.assertEqual(vm.requests, [('ArmedPlayerBeing', (hand,))])
                    self.assertEqual((vm.vector(), vm.events, vm.clip_calls), (before, 0, 0))
                    self.assertEqual((vm.player['cojvrReloadProbeStatus'], vm.desired_state), (1, 21))
                    self.assertTrue(vm.player['m_aHandStateMashine'][hand]['m_cDesiredState']['m_bTwoHanded'])
                    self.assertIs(vm.player['cojvrReloadProbeWeapon'], vm.weapon)
                    self.assertIsNone(vm.player['cojvrManualReloadWeapon'])
                    self.assertFalse(vm.open())
                    vm.machine.update(m_sCurrentState=vm.states[20], m_sDestState=vm.states[21])
                    vm.update(.31); vm.update(.62)
                    self.assertEqual((vm.vector(), vm.player['cojvrReloadProbeStatus']), (before, 2))
                    self.assertEqual(vm.unload(), bool(loaded))
                    self.assertEqual(vm.vector()[0][0], 0)
                    self.assertEqual(vm.vector()[1][2], loaded + reserve)
                    self.assertEqual(vm.insert_bundle(), bool(loaded + reserve))
                    self.assertEqual((vm.player['cojvrReloadProbeStatus'],
                                      vm.machine['m_sCurrentState']['id'], vm.desired_state), (2, 21, 21))
                    self.assertTrue(vm.boolean('cojvrKeepManualReloadAlive'))
                    self.assertFalse(vm.insert_bundle())
                    after = vm.vector()
                    vm.run('ArmedPlayerBeing', 'cojvrCancelReloadProbe', '()V', vm.player)
                    vm.update(1.0); vm.update(1.4)
                    self.assertEqual((vm.vector(), vm.clip_calls, vm.events),
                                     (after, int(bool(loaded + reserve)), int(bool(loaded + reserve))))

    def test_open_requires_exact_publication_and_fresh_local_sole_idle_hand(self):
        for change in ('null', 'wrong_weapon', 'variant', 'wrong_hand', 'bad_hand', 'busy',
                       'occupied', 'carrying', 'dead', 'network', 'foreign_owner', 'not_owner',
                       'current', 'destiny', 'actual_state', 'desired_state', 'old_ticket'):
            with self.subTest(change=change):
                vm = SchofieldVM(kind='WeaponPistolSchofield_A_Old' if change == 'variant'
                                 else 'WeaponPistolSchofield', waiting=False)
                if change == 'null': vm.player['cojvrManualReloadWeapon'] = None
                if change == 'wrong_weapon': vm.player['cojvrManualReloadWeapon'] = dict(vm.weapon)
                if change == 'busy': vm.player['cojvrClipBusy'] = True
                if change == 'occupied': vm.slots[1][1] = vm.weapon
                if change == 'carrying': vm.carrying = True
                if change == 'dead': vm.alive = False
                if change == 'network': vm.net_game = True
                if change == 'foreign_owner': vm.weapon['cOwner'] = {}
                if change == 'not_owner': vm.net_owner = False
                if change == 'current': vm.machine['m_sCurrentState'] = vm.states[20]
                if change == 'destiny': vm.machine['m_sDestState'] = vm.states[21]
                if change == 'actual_state': vm.player['m_aHandStateMashine'][0]['m_cActualState']['m_iState'] = 20
                if change == 'desired_state': vm.desired_state = 21
                if change == 'old_ticket': vm.player['cojvrReloadProbeWeapon'] = vm.weapon
                before = vm.vector()
                self.assertFalse(vm.open({'wrong_hand': 1, 'bad_hand': -1}.get(change)))
                self.assertEqual((vm.requests, vm.vector()), ([], before))

    def test_open_reports_success_only_for_exact_status_one_and_unchanged_counts(self):
        for obj, field, value in (('player', 'cojvrReloadProbeStatus', 2),
                                  ('player', 'cojvrReloadProbeHand', 1),
                                  ('player', 'nAmmoPistol', 1),
                                  ('weapon', 'm_iAmmo0Count', 3)):
            with self.subTest(field=field):
                vm = SchofieldVM(reserve=0, waiting=False)
                reentered = []
                def change_receipt():
                    reentered.append(vm.open())
                    getattr(vm, obj)[field] = value
                vm.on_request = change_receipt
                self.assertFalse(vm.open())
                self.assertEqual(reentered, [False])
                self.assertEqual(len(vm.requests), 1)
                self.assertTrue(vm.player['cojvrClipFaulted'])
                self.assertFalse(vm.player['cojvrClipBusy'])

    def test_open_accepts_only_the_armed_desired_reload_flag(self):
        for support, slot, two_handed in ((False, 'm_cDesiredState', False),
                                           (False, 'm_cActualState', True),
                                           (True, 'm_cDesiredState', True)):
            with self.subTest(support=support, slot=slot):
                vm = SchofieldVM(waiting=False)
                hand = vm.player['m_aHandStateMashine'][int(support)]
                vm.on_request = lambda: hand[slot].update(m_bTwoHanded=two_handed)
                before = vm.vector()
                self.assertFalse(vm.open())
                self.assertEqual(vm.vector(), before)

    def test_bundle_rejects_unknown_models_and_lost_ticket_cannot_add_on_native_close(self):
        for kind in ('WeaponPistolSchofield_A_Old', 'WeaponPistolSchofield_B_Old',
                     'WeaponPistolPeacemaker'):
            vm = SchofieldVM(kind=kind)
            self.assertFalse(vm.insert_bundle())
            self.assertEqual((vm.clip_calls, vm.events), (0, 0))

        vm = SchofieldVM()
        vm.player['nAmmoPistol'] = 2
        self.assertTrue(vm.insert_bundle())
        before = vm.vector()
        vm.player['nAmmoPistol'] += 3  # reserve arrives after the gesture
        vm.run('ArmedPlayerBeing', 'cojvrClearReloadProbe', '(I)V', vm.player, [vm.hand])
        vm.update(1.0); vm.update(1.4)
        self.assertIn(22, vm.transitions)
        self.assertEqual((vm.vector()[0], vm.vector()[1][2], vm.clip_calls, vm.events),
                         (before[0], before[1][2]+3, 1, 1))


class OfflineUnloadTests(unittest.TestCase):
    def test_native_unload_result_distinguishes_capacity_guard_and_commit(self):
        for hand in (0, 1):
            for reserve, result in ((93, 1), (94, 2), (97, 2), (99, 2)):
                with self.subTest(hand=hand, reserve=reserve):
                    vm = SchofieldVM(loaded=6, reserve=reserve, hand=hand)
                    self.assertEqual(vm.run('ArmedPlayerBeing', 'cojvrUnloadSchofield', '()I',
                                            vm.player), result)
                    expected = ((0, 0), (10, 10, 99)) if result == 1 else ((6, 0), (10, 10, reserve))
                    self.assertEqual(vm.vector(), expected)
                    self.assertEqual(vm.unload_calls, int(result == 1))
                    self.assertTrue(vm.boolean('cojvrKeepManualReloadAlive'))

    def test_original_native_unload_and_reload_conserve_ammo(self):
        for loaded in range(1, 7):
            with self.subTest(loaded=loaded):
                before = 0 if loaded < 6 else 93
                vm = SchofieldVM(loaded, before)
                self.assertEqual(vm.vector(), ((loaded, 0), (10, 10, before)))
                self.assertTrue(vm.unload())
                self.assertEqual(vm.vector(), ((0, 0), (10, 10, before+loaded)))
                self.assertEqual(vm.machine['m_sCurrentState']['id'], 21)
                self.assertFalse(vm.unload())
                self.assertEqual(vm.unload_calls, 1)
                self.assertTrue(vm.insert_clip())
                self.assertEqual(sum(vm.vector()[0])+vm.vector()[1][2], before+loaded)

    def test_offline_verification_uses_the_pinned_canonical_motion_bytecode(self):
        from coj_schofield_unload import build_offline_schofield_unload
        raw = original_archive()
        with self.assertRaisesRegex(ValueError, 'SHA-256'):
            build_offline_schofield_unload(raw + b'X')
        with zipfile.ZipFile(io.BytesIO(raw)) as archive:
            player, machine = patch_manual_reload_classes(archive.read('ArmedPlayerBeing.class'),
                                                           archive.read('StateMashine.class'))
        self.assertEqual(build_offline_schofield_unload(raw), (player, machine))
        self.assertNotIn('cojvrLabUnloadSchofield', {method.name for method in parse_class(player)[1]})

    def test_rejects_invalid_preconditions_without_writes(self):
        for change in ('short_headroom', 'full_reserve', 'bad_count', 'counter', 'wrong_model', 'not_waiting'):
            with self.subTest(change=change):
                vm = SchofieldVM()
                if change == 'short_headroom': vm.player['nAmmoPistol'] = 98
                if change == 'full_reserve': vm.player['nAmmoPistol'] = 99
                if change == 'bad_count': vm.weapon['m_iAmmo0Count'] = 7
                if change == 'counter': vm.counters[2]['cOwner'] = {}
                if change == 'wrong_model': vm.weapon['class'] = 'WeaponPistolSchofield_A_Old'
                if change == 'not_waiting': vm.machine['m_sCurrentState'] = vm.states[20]
                before = vm.vector()
                self.assertFalse(vm.unload())
                self.assertEqual(vm.vector(), before)
                self.assertEqual(vm.unload_calls, 0)

    def test_reentry_and_interrupted_native_write(self):
        vm = SchofieldVM()
        replies = []
        def reenter(when, name, owner, args):
            if when == 'before' and name == 'SetAmount':
                replies.extend((vm.unload(), vm.insert_clip(),
                                vm.boolean('cojvrKeepManualReloadAlive')))
        vm.hook = reenter
        self.assertTrue(vm.unload())
        self.assertEqual(replies, [False, False, False])

        vm = SchofieldVM()
        exception = {'class': 'Throwable', 'point': 'before_reserve_write'}
        def interrupt(when, name, owner, args):
            if when == 'before' and name == 'SetAmount': raise JVMThrown(exception)
        vm.hook = interrupt
        with self.assertRaises(JVMThrown) as raised:
            vm.unload()
        self.assertIs(raised.exception.value, exception)
        self.assertEqual(vm.vector(), ((0, 0), (10, 10, 10)))
        self.assertTrue(vm.player['cojvrClipFaulted'])
        self.assertFalse(vm.unload())
        self.assertFalse(vm.insert_clip())
        self.assertFalse(vm.boolean('cojvrKeepManualReloadAlive'))
        self.assertEqual(vm.unload_calls, 1)

    def test_completed_native_call_with_wrong_readback_remains_quarantined(self):
        vm = SchofieldVM()
        def change_reserve(when, name, owner, args):
            if when == 'after' and name == 'SetAmount':
                vm.player['nAmmoPistol'] -= 1
        vm.hook = change_reserve
        self.assertFalse(vm.unload())
        self.assertTrue(vm.player['cojvrClipFaulted'])
        self.assertEqual(vm.vector(), ((0, 0), (10, 10, 11)))
        self.assertFalse(vm.unload())
        self.assertEqual(vm.unload_calls, 1)

    def test_loss_of_wait_ticket_does_not_release_the_original_transfer(self):
        for boundary in ('GetAmmoTypeCount', 'SetAmount'):
            with self.subTest(boundary=boundary):
                vm = SchofieldVM()
                exception = {'class': 'Throwable'}
                def interrupt(when, name, owner, args):
                    if when == 'before' and name == boundary:
                        vm.hook = None
                        vm.run('ArmedPlayerBeing', 'cojvrClearReloadProbe', '(I)V', vm.player, [vm.hand])
                        raise JVMThrown(exception)
                vm.hook = interrupt
                with self.assertRaises(JVMThrown) as raised:
                    vm.unload()
                self.assertIs(raised.exception.value, exception)
                before = vm.vector()
                vm.update(1.0)
                self.assertEqual(vm.vector(), before)
                self.assertEqual(vm.events, 0)
                self.assertEqual(vm.desired_state, 1)
                vm.update(1.4)
                vm.player['cojvrManualReloadWeapon'] = None
                vm.run('ArmedPlayerBeing', 'ReloadWeapon', '(I)V', vm.player, [vm.hand])
                vm.machine.update(m_sCurrentState=vm.states[20], start=1.4)
                vm.update(1.8); vm.update(2.2)
                self.assertGreater(vm.vector()[0][0], before[0][0])
                self.assertEqual(vm.events, 1)


if __name__ == '__main__':
    unittest.main()
