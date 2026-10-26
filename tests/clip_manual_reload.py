"""Interpreted Schofield clip transaction; native animation/skins are doubles."""
from pathlib import Path
import hashlib
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
            for weapon in clips.CLIP_WEAPONS:
                player, machine = clips.build_offline_clip_lab(raw, weapon)
                target = root / (weapon + '.pak')
                with zipfile.ZipFile(source) as original_archive, zipfile.ZipFile(target, 'w') as out:
                    replacement = {'ArmedPlayerBeing.class': player, 'StateMashine.class': machine}
                    for info in original_archive.infolist():
                        out.writestr(info, replacement.get(info.filename,
                                                          original_archive.read(info.filename)))
                result = verify(target)
                self.assertEqual(result.returncode, 0, weapon + ': ' + result.stdout + result.stderr)
                if weapon == clips.CLIP_WEAPONS[0]:
                    _, methods = parse_class(player)
                    helper_code = next(m.code for m in methods
                                       if (m.name, m.descriptor) == ('cojvrInsertManualClip', '()Z'))
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


if __name__ == '__main__':
    unittest.main()
