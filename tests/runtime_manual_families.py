"""Combined production bytecode and pinned storage; native scheduling is modeled."""
import functools
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.dont_write_bytecode = True
sys.path[:0] = [str(Path(__file__).parent), str(Path(__file__).resolve().parents[1] / 'tools')]
from indexed_reload_session import SessionVM
from indexed_manual_reload import original_archive, ARCHIVE
from inspect_java_bytecode import parse_class, instruction_offsets, Reader, parse_constant_pool
from reload_wait_probe import JVMThrown, VM, original
from patch_coj_weapon_consumers import patch_manual_reload_classes


@functools.lru_cache(None)
def production_classes():
    return patch_manual_reload_classes(original('ArmedPlayerBeing'), original('StateMashine'))


def production_builder(archive, kind):
    return production_classes()


class FamilyVM(SessionVM):
    def __init__(self, kind='WeaponPistolLemant', hand=0):
        # SessionVM supplies the real Java hand/ticket backing fields. Replace
        # only the constructor fixture for one-index classes after construction.
        super().__init__(production_builder, kind if kind in (
            'WeaponPistolLemant', 'WeaponPistolDerringer',
            'WeaponRifleRemington', 'WeaponRifleRemingtonSawOff') else 'WeaponPistolLemant', hand)
        self.weapon['class'] = kind
        self.clip = kind in ('WeaponPistolSchofield', 'WeaponPistolSchofield_A', 'WeaponPistolSchofield_B')
        if self.clip:
            self.capacities, self.types = [6], [2]
            self.weapon['m_iAmmo0Count'] = 2
        self.clip_calls = 0

    def _native(self, owner, name, args):
        if name == 'IsReloadWholeClipAtOnce' and self.clip:
            return True
        if name == 'WeaponReload' and self.clip:
            self.clip_calls += 1
            # Original whole-clip code reaches iinc at its ascending-loop tail.
            # The existing wait/cycle interpreter supports that opcode; the
            # indexed-only interpreter intentionally never needed that loop.
            return VM.run(self,'PawnArmed','WeaponReload','(LWeapon;)Z',owner,args)
        return super()._native(owner, name, args)

    def insert(self, index=1):
        return self.run('ArmedPlayerBeing', 'cojvrInsertManualIndexedRound', '(I)Z', self.player, [index])

    def boolean(self, name):
        return self.run('ArmedPlayerBeing', name, '()Z', self.player)

    def insert_clip(self):
        return self.boolean('cojvrInsertManualClip')


class RuntimeManualFamiliesTests(unittest.TestCase):
    def test_first_heartbeat_probe_loss_throw_fences_original_completion_and_recovers(self):
        cases=[(kind,hand,False) for kind in ('WeaponPistolLemant','WeaponRifleRemington','WeaponPistolSchofield')
               for hand in (0,1)]+[('WeaponPistolLemant',0,True)]
        for kind,hand,replacement in cases:
            with self.subTest(kind=kind,hand=hand,replacement=replacement):
                vm=FamilyVM(kind,hand);vm.wait();retained=vm.weapon
                exception={'class':'Throwable','unique':object()}
                def fail(when,name,owner,args):
                    if when=='after' and name=='GetAmmoTypeCount':
                        vm.hook=None
                        if replacement:
                            vm.weapon=dict(retained,m_iAmmo0Count=0,m_iAmmo1Count=0)
                            for index in range(3):vm.slots[hand][index]=vm.weapon
                            retained['cOwner']={'class':'foreign'}
                        vm.run('ArmedPlayerBeing','cojvrClearReloadProbe','(I)V',vm.player,[hand])
                        raise JVMThrown(exception)
                vm.hook=fail
                with self.assertRaises(JVMThrown) as caught:vm.boolean('cojvrKeepManualReloadAlive')
                self.assertIs(caught.exception.value,exception)
                before=vm.vector();self.assertEqual(vm.events,0)
                native=vm.native
                def natural(owner,name,args):
                    if name=='WeaponReload':return VM.run(vm,'PawnArmed',name,'(LWeapon;)Z',owner,args)
                    return native(owner,name,args)
                vm.native=natural;vm.update(1.)
                self.assertEqual(vm.vector(),before);self.assertEqual(vm.events,0)
                self.assertEqual(vm.desired_state,1)
                vm.update(1.4);vm.player['cojvrManualReloadWeapon']=None
                vm.run('ArmedPlayerBeing','ReloadWeapon','(I)V',vm.player,[hand])
                vm.machine.update(m_sCurrentState=vm.states[20],start=1.4)
                vm.update(1.8);vm.update(2.2)
                self.assertGreater(sum(vm.vector()[0]),sum(before[0]))
                self.assertEqual(vm.events,1)

    def test_clip_initial_validator_probe_loss_throw_keeps_late_completion_zero(self):
        for kind in ('WeaponPistolSchofield','WeaponPistolSchofield_A','WeaponPistolSchofield_B'):
            with self.subTest(kind=kind):
                vm = FamilyVM(kind); vm.wait()
                exception = {'class':'Throwable','unique':object()}
                def fail(when,name,owner,args):
                    if when == 'after' and name == 'GetAmmoTypeCount':
                        vm.hook = None
                        vm.run('ArmedPlayerBeing','cojvrClearReloadProbe','(I)V',vm.player,[0])
                        raise JVMThrown(exception)
                vm.hook = fail
                before = vm.vector(), vm.events
                with self.assertRaises(JVMThrown) as caught: vm.insert_clip()
                self.assertIs(caught.exception.value,exception)
                vm.update(1.)
                self.assertEqual((vm.vector(),vm.events),before)
                self.assertEqual(vm.desired_state,1)
                vm.update(1.4)
                vm.player['cojvrManualReloadWeapon']=None
                vm.run('ArmedPlayerBeing','ReloadWeapon','(I)V',vm.player,[vm.hand])
                vm.machine.update(m_sCurrentState=vm.states[20],start=1.4)
                vm.update(1.8);vm.update(2.2)
                self.assertEqual((vm.weapon['m_iAmmo0Count'],vm.player['nAmmoPistol'],vm.events),(6,6,1))

    def test_indexed_probe_loss_then_partial_throw_never_runs_late_ascending_transfer(self):
        for kind in ('WeaponPistolLemant', 'WeaponRifleRemington'):
            for hand in (0, 1):
                with self.subTest(kind=kind, hand=hand):
                    vm = FamilyVM(kind, hand); vm.wait()
                    exception = {'class': 'Throwable', 'unique': object()}
                    def fail(when, name, owner, args):
                        if when == 'after' and name == 'UpdateAmmoSkin':
                            vm.hook = None
                            vm.run('ArmedPlayerBeing', 'cojvrClearReloadProbe', '(I)V', vm.player, [hand])
                            raise JVMThrown(exception)
                    vm.hook = fail
                    with self.assertRaises(JVMThrown) as caught: vm.insert(1)
                    self.assertIs(caught.exception.value, exception)
                    before = vm.vector()
                    self.assertEqual(before, ((0, 1), (10, 10, 10)))
                    self.assertFalse(vm.insert(1))
                    self.assertFalse(vm.player['cojvrIndexedBusy'])
                    self.assertFalse(vm.player['cojvrIndexedSessionBusy'])
                    self.assertFalse(vm.player['cojvrIndexedSessionDispatch'])
                    # Run the original ascending authority if a late callback
                    # escapes the veto, instead of the fixture's fail-fast stub.
                    native = vm.native
                    def natural(owner, name, args):
                        if name == 'WeaponReload':
                            return VM.run(vm, 'PawnArmed', name, '(LWeapon;)Z', owner, args)
                        return native(owner, name, args)
                    vm.native = natural
                    vm.update(1.0)
                    self.assertEqual(vm.vector(), before)
                    self.assertEqual(vm.events, 0)
                    self.assertEqual(vm.desired_state, 1)
                    vm.update(1.4)
                    vm.player['cojvrManualReloadWeapon'] = None
                    vm.run('ArmedPlayerBeing', 'ReloadWeapon', '(I)V', vm.player, [hand])
                    vm.machine['m_sCurrentState'] = vm.states[20]; vm.machine['start'] = 1.4
                    vm.update(1.8); vm.update(2.2)
                    reserves = (9, 10, 10) if kind == 'WeaponPistolLemant' else (10, 9, 10)
                    self.assertEqual(vm.vector(), ((1, 1), reserves))
                    self.assertEqual(vm.events, 1)

    def test_indexed_fault_veto_fences_hand_and_machine(self):
        for change in ('hand', 'machine'):
            with self.subTest(change=change):
                vm = FamilyVM(); vm.wait(); exception = {'class': 'Throwable'}
                def fail(when, name, owner, args):
                    if when == 'after' and name == 'UpdateAmmoSkin':
                        vm.hook = None
                        vm.run('ArmedPlayerBeing','cojvrClearReloadProbe','(I)V',vm.player,[0])
                        raise JVMThrown(exception)
                vm.hook = fail
                with self.assertRaises(JVMThrown): vm.insert()
                hand = 0
                if change == 'hand':
                    hand = 1
                    for index in range(3): vm.slots[1][index] = vm.weapon
                    vm.other['m_sCurrentState'] = vm.states[21]
                if change == 'machine':
                    vm.machine = dict(vm.machine)
                    vm.player['m_aHandStateMashine'][0]['m_cStateMashine'] = vm.machine
                native = vm.native
                def natural(owner, name, args):
                    if name == 'WeaponReload': return VM.run(vm,'PawnArmed',name,'(LWeapon;)Z',owner,args)
                    return native(owner,name,args)
                vm.native = natural
                vm.run('ArmedPlayerBeing','OnHandStateFinished_Reload','(IIZ)V',vm.player,[hand,21,False])
                self.assertEqual(vm.weapon['m_iAmmo0Count'],1)
                self.assertEqual(vm.player['nAmmoRifle'],9)
                self.assertEqual(vm.events,1)

    def test_replaced_weapon_probe_loss_throw_cannot_redirect_old_body_transfer(self):
        for kind in ('WeaponPistolLemant', 'WeaponRifleRemington'):
            with self.subTest(kind=kind):
                vm = FamilyVM(kind); vm.wait(); retained = vm.weapon
                replacement = dict(retained, m_iAmmo0Count=0, m_iAmmo1Count=0)
                exception = {'class':'Throwable','unique':object()}
                def fail(when,name,owner,args):
                    if when == 'after' and name == 'UpdateAmmoSkin':
                        vm.hook = None
                        retained['cOwner'] = {'class':'foreign'}
                        vm.weapon = replacement
                        for index in range(3): vm.slots[0][index] = replacement
                        vm.run('ArmedPlayerBeing','cojvrClearReloadProbe','(I)V',vm.player,[0])
                        raise JVMThrown(exception)
                vm.hook = fail
                with self.assertRaises(JVMThrown) as caught: vm.insert()
                self.assertIs(caught.exception.value,exception)
                native = vm.native
                def natural(owner,name,args):
                    if name == 'WeaponReload': return VM.run(vm,'PawnArmed',name,'(LWeapon;)Z',owner,args)
                    return native(owner,name,args)
                vm.native = natural; vm.update(1.0)
                self.assertEqual((retained['m_iAmmo0Count'],retained['m_iAmmo1Count']),(0,1))
                self.assertEqual(vm.vector(),((0,0),(10,10,10)))
                self.assertEqual(vm.events,0)
                self.assertEqual(vm.desired_state,1)
                vm.update(1.4)
                vm.player['cojvrManualReloadWeapon'] = None
                vm.run('ArmedPlayerBeing','ReloadWeapon','(I)V',vm.player,[0])
                vm.machine['m_sCurrentState'] = vm.states[20]; vm.machine['start'] = 1.4
                vm.update(1.8); vm.update(2.2)
                if kind == 'WeaponPistolLemant':
                    self.assertEqual(vm.vector(),((1,0),(9,10,10)))
                else:self.assertEqual(vm.vector(),((1,1),(10,8,10)))
                self.assertEqual(vm.events,1)

    def test_indexed_remaining_probe_loss_throw_keeps_committed_vector(self):
        vm = FamilyVM(); vm.wait(); exception = {'class':'Throwable','unique':object()}
        def fail():
            vm.run('ArmedPlayerBeing','cojvrClearReloadProbe','(I)V',vm.player,[0])
            raise JVMThrown(exception)
        vm.on_max = fail
        with self.assertRaises(JVMThrown) as caught: vm.insert()
        self.assertIs(caught.exception.value,exception)
        self.assertEqual(vm.vector(),((0,1),(10,10,9)))
        vm.on_max = None; native = vm.native
        def natural(owner,name,args):
            if name == 'WeaponReload': return VM.run(vm,'PawnArmed',name,'(LWeapon;)Z',owner,args)
            return native(owner,name,args)
        vm.native = natural; vm.update(1.0)
        self.assertEqual(vm.vector(),((0,1),(10,10,9)))
        self.assertEqual(vm.events,1)
        self.assertEqual(vm.desired_state,1)
        self.assertFalse(vm.insert())

    def test_indexed_initial_validator_probe_loss_throw_keeps_late_completion_zero(self):
        vm = FamilyVM(); vm.wait(); exception = {'class':'Throwable','unique':object()}
        def fail(when,name,owner,args):
            if when == 'after' and name == 'GetAmmoTypeCount':
                vm.hook = None
                vm.run('ArmedPlayerBeing','cojvrClearReloadProbe','(I)V',vm.player,[0])
                raise JVMThrown(exception)
        vm.hook = fail
        with self.assertRaises(JVMThrown) as caught: vm.insert()
        self.assertIs(caught.exception.value,exception)
        self.assertFalse(vm.player['cojvrIndexedSessionBusy'])
        self.assertFalse(vm.player['cojvrIndexedSessionDispatch'])
        self.assertTrue(vm.player['cojvrIndexedFaulted'])
        native = vm.native
        def natural(owner,name,args):
            if name == 'WeaponReload': return VM.run(vm,'PawnArmed',name,'(LWeapon;)Z',owner,args)
            return native(owner,name,args)
        vm.native = natural; vm.update(1.0)
        self.assertEqual(vm.vector(),((0,0),(10,10,10)))
        self.assertEqual(vm.events,0)
        self.assertEqual(vm.desired_state,1)

    def test_appendix_api_and_combined_unique_members(self):
        self.assertIsNotNone(importlib.util.find_spec('coj_manual_families'), 'Missing production appendix')
        import coj_manual_families as families
        self.assertTrue(callable(families.append_runtime_families))
        player, machine = production_classes()
        methods = parse_class(player)[1]
        signatures = [(m.name, m.descriptor) for m in methods]
        self.assertEqual(len(signatures), len(set(signatures)))
        reader=Reader(player);reader.read(8);pool=parse_constant_pool(reader)
        reader.read(6);reader.read(reader.u2()*2)
        fields=[]
        for _ in range(reader.u2()):
            reader.u2();fields.append((pool.utf8(reader.u2()),pool.utf8(reader.u2())))
            for _ in range(reader.u2()):reader.u2();reader.read(reader.u4())
        self.assertEqual(len(fields),len(set(fields)))
        for signature in (('cojvrInsertManualRound', '()Z'),
                          ('cojvrInsertManualIndexedRound', '(I)Z'),
                          ('cojvrInsertManualClip', '()Z')):
            self.assertEqual(signatures.count(signature), 1, signature)
        for data in (player, machine):
            for method in parse_class(data)[1]:
                if method.code is None:
                    continue
                boundaries = instruction_offsets(method.code)
                for start, end, handler, catch in method.exception_table:
                    self.assertIn(start, boundaries)
                    self.assertIn(handler, boundaries)
                    self.assertTrue(end == len(method.code) or end in boundaries)

    def test_selected_second_index_under_eligible_first_is_one_typed_unit(self):
        for kind, reserves in (
            ('WeaponRifleRemington', (10, 9, 10)),
            ('WeaponRifleRemingtonSawOff', (10, 9, 10)),
            ('WeaponPistolDerringer', (10, 10, 9)),
            ('WeaponPistolLemant', (10, 10, 9))):
            for hand in (0, 1):
                with self.subTest(kind=kind, hand=hand):
                    vm = FamilyVM(kind, hand); vm.wait()
                    self.assertTrue(vm.insert(1))
                    self.assertEqual(vm.vector(), ((0, 1), reserves))
                    self.assertEqual((vm.events, vm.automatic_transfers), (1, 0))

    def test_clip_requested_min_delta_always_closes(self):
        for kind in ('WeaponPistolSchofield', 'WeaponPistolSchofield_A', 'WeaponPistolSchofield_B'):
            for reserve, loaded_after, reserve_after in ((1, 3, 0), (3, 5, 0), (10, 6, 6)):
                with self.subTest(kind=kind, reserve=reserve):
                    vm = FamilyVM(kind); vm.wait(); vm.player['nAmmoPistol'] = reserve
                    self.assertTrue(vm.insert_clip())
                    self.assertEqual((vm.weapon['m_iAmmo0Count'], vm.player['nAmmoPistol']),
                                     (loaded_after, reserve_after))
                    self.assertEqual((vm.clip_calls, vm.events), (1, 1))
                    self.assertEqual(vm.machine['m_sCurrentState']['id'], 22)
                    self.assertFalse(vm.insert_clip())

    def test_remaining_mutation_or_close_retarget_quarantines_indexed(self):
        for point in ('remaining', 'close'):
            vm = FamilyVM('WeaponRifleRemington'); vm.wait(); vm.player['nAmmoShotgun'] = 1
            if point == 'remaining':
                vm.on_max = lambda: vm.player.update(nAmmoPistol=9)
            else:
                vm.on_close = lambda: vm.player.update(cojvrReloadProbeMachine=vm.other)
            self.assertFalse(vm.insert())
            self.assertTrue(vm.player['cojvrIndexedFaulted'])
            before = vm.vector(); self.assertFalse(vm.insert()); self.assertEqual(vm.vector(), before)
            self.assertFalse(vm.boolean('cojvrKeepManualReloadAlive'))

    def test_clip_close_never_redirects_replaced_probe(self):
        vm = FamilyVM('WeaponPistolSchofield'); vm.wait()
        native = vm.native
        def replace(owner, name, args):
            result = native(owner, name, args)
            if name == 'GetWeaponMoveState':
                vm.player['cojvrReloadProbeMachine'] = vm.other
            return result
        vm.native = replace
        self.assertFalse(vm.insert_clip())
        self.assertEqual(vm.other['m_sCurrentState']['id'], 1)
        self.assertTrue(vm.player['cojvrClipFaulted'])

    def test_clip_post_close_owner_and_raw_accounting_changes_quarantine(self):
        for change in ('loaded','reserve','counter','counter_max','hand_machine','support','ticket'):
            with self.subTest(change=change):
                vm=FamilyVM('WeaponPistolSchofield');vm.wait()
                def close():
                    if change=='loaded':vm.weapon['m_iAmmo0Count']=2
                    if change=='reserve':vm.player['nAmmoPistol']=8
                    if change=='counter':vm.counters[2]=dict(vm.counters[2])
                    if change=='counter_max':vm.counters[2]['nMaxAmount']=100
                    if change=='hand_machine':vm.player['m_aHandStateMashine'][0]['m_cStateMashine']=vm.other
                    if change=='support':vm.slots[1][0]={'class':'foreign'}
                    if change=='ticket':vm.player['cojvrReloadProbeWeapon']={'class':'foreign'}
                vm.on_close=close
                self.assertFalse(vm.insert_clip())
                self.assertTrue(vm.player['cojvrClipFaulted'])
                self.assertFalse(vm.player['cojvrClipBusy'])

    def test_wrong_direct_endpoint_does_not_read_nonexistent_ammo_slot(self):
        vm=FamilyVM('WeaponPistolSchofield');vm.wait()
        self.assertFalse(vm.insert(1))
        self.assertEqual((vm.clip_calls,vm.events),(0,0))
        vm=FamilyVM();vm.wait()
        self.assertFalse(vm.insert_clip())
        self.assertEqual(vm.vector(),((0,0),(10,10,10)))

    def test_cancel_and_close_do_not_allow_native_automatic_refill(self):
        vm=FamilyVM('WeaponRifleRemington');vm.wait()
        before=vm.vector()
        vm.run('ArmedPlayerBeing','cojvrCancelReloadProbe','()V',vm.player)
        self.assertFalse(vm.insert())
        vm.update(1.)
        self.assertEqual((vm.vector(),vm.events,vm.automatic_transfers),(before,0,0))

    def test_direct_family_entries_reject_other_active_or_faulted_family_without_callbacks(self):
        fields = ('cojvrManualCycleActive', 'cojvrManualCycleFaulted', 'cojvrIndexedBusy',
                  'cojvrIndexedFaulted', 'cojvrIndexedSessionBusy', 'cojvrClipBusy', 'cojvrClipFaulted',
                  'cojvrManualFamiliesActive','cojvrManualFamiliesFaulted')
        for field in fields:
            vm = FamilyVM(); vm.wait(); vm.player[field] = True
            before = vm.vector()
            native = vm.native
            def forbidden(owner, name, args):
                raise AssertionError('Guard called a native boundary: ' + name)
            vm.native = forbidden
            for name, desc, args in (('cojvrInsertManualIndexedRound', '(I)Z', [1]),
                                     ('cojvrInsertManualClip', '()Z', []),
                                     ('cojvrInsertManualNativeCycleRound', '()Z', []),
                                     ('cojvrInsertManualRevolverRound','()Z',[]),
                                     ('cojvrInsertManualRound','()Z',[])):
                with self.subTest(field=field, name=name):
                    self.assertFalse(vm.run('ArmedPlayerBeing', name, desc, vm.player, args))
            vm.native = native
            self.assertEqual(vm.vector(), before)

    def test_cross_family_reentry_during_indexed_callback_does_not_transfer(self):
        vm = FamilyVM(); vm.wait(); replies = []
        def reenter(when, name, owner, args):
            if when == 'before' and name == 'GetAmmoCapacity':
                vm.hook = None
                replies.extend((vm.insert_clip(), vm.boolean('cojvrInsertManualNativeCycleRound'), vm.insert(),
                                vm.boolean('cojvrInsertManualRevolverRound'),
                                vm.boolean('cojvrInsertManualRevolverRoundImpl')))
        vm.hook = reenter
        self.assertTrue(vm.insert())
        self.assertEqual(replies, [False]*5)
        self.assertEqual(vm.vector(), ((0, 1), (10, 10, 9)))

    def test_cross_family_reentry_during_clip_event_does_not_transfer(self):
        vm=FamilyVM('WeaponPistolSchofield');vm.wait();replies=[]
        def reenter():
            replies.extend((vm.insert(),vm.insert_clip(),
                            vm.boolean('cojvrInsertManualNativeCycleRound'),
                            vm.boolean('cojvrInsertManualRevolverRound'),
                            vm.boolean('cojvrInsertManualRevolverRoundImpl')))
        vm.on_event=reenter
        self.assertTrue(vm.insert_clip())
        self.assertEqual(replies,[False]*5)
        self.assertEqual((vm.weapon['m_iAmmo0Count'],vm.player['nAmmoPistol'],vm.events,vm.clip_calls),(6,6,1,1))

    def test_cross_family_reentry_during_cycle_native_callback_does_not_transfer(self):
        import runtime_manual_reload as cycle_tests
        vm=cycle_tests.RuntimeCycleVM();vm.wait();replies=[]
        def reenter():
            for name,descriptor,args in (('cojvrInsertManualIndexedRound','(I)Z',[1]),
                                         ('cojvrInsertManualClip','()Z',[]),
                                         ('cojvrInsertManualRevolverRound','()Z',[]),
                                         ('cojvrInsertManualRevolverRoundImpl','()Z',[])):
                replies.append(vm.run('ArmedPlayerBeing',name,descriptor,vm.player,args))
        vm.on_capacity=reenter
        self.assertTrue(vm.call('cojvrInsertManualRound'))
        self.assertTrue(replies)
        self.assertFalse(any(replies))
        self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(3,9,1))

    def test_clip_cancel_during_parameter_read_prevents_transfer(self):
        vm=FamilyVM('WeaponPistolSchofield');vm.wait()
        def cancel(when,name,owner,args):
            if when=='after' and name=='GetAmmoCapacity':
                vm.hook=None
                vm.run('ArmedPlayerBeing','cojvrCancelReloadProbe','()V',vm.player)
        vm.hook=cancel
        self.assertFalse(vm.insert_clip())
        self.assertEqual((vm.weapon['m_iAmmo0Count'],vm.player['nAmmoPistol'],vm.events,vm.clip_calls),(2,10,0,0))

    def test_clip_invalid_counter_owner_or_over_max_reserve_never_calls_authority(self):
        for change in ('owner','over_max'):
            vm=FamilyVM('WeaponPistolSchofield');vm.wait()
            if change=='owner':vm.counters[2]['cOwner']={'class':'foreign'}
            else:vm.player['nAmmoPistol']=100
            self.assertFalse(vm.insert_clip())
            self.assertEqual((vm.weapon['m_iAmmo0Count'],vm.clip_calls,vm.events),(2,0,0))

    def test_clip_close_exception_keeps_throwable_and_quarantines_cross_family(self):
        vm=FamilyVM('WeaponPistolSchofield');vm.wait();exception={'class':'Throwable','unique':object()}
        def fail():raise JVMThrown(exception)
        vm.on_close=fail
        with self.assertRaises(JVMThrown) as caught:vm.insert_clip()
        self.assertIs(caught.exception.value,exception)
        self.assertTrue(vm.player['cojvrClipFaulted'])
        self.assertFalse(vm.player['cojvrClipBusy'])
        self.assertFalse(vm.insert())
        self.assertFalse(vm.boolean('cojvrInsertManualNativeCycleRound'))

    def test_appendix_rejects_duplicate_or_missing_cycle_composition(self):
        from coj_manual_families import append_runtime_families
        for player in (original('ArmedPlayerBeing'),production_classes()[0]):
            with self.assertRaises(ValueError):append_runtime_families(player)

    def test_unscoped_inner_indexed_transfer_cannot_bypass_family_entry(self):
        vm=FamilyVM();vm.wait()
        def inner():
            return vm.run('ArmedPlayerBeing','cojvrLabInsertIndexedUnit','(LWeapon;II)Z',
                          vm.player,[vm.weapon,vm.hand,1])
        self.assertFalse(inner())
        for field in ('cojvrClipBusy','cojvrClipFaulted','cojvrManualCycleActive','cojvrManualCycleFaulted'):
            vm.player[field]=True
            vm.player['cojvrIndexedSessionDispatch']=True
            def forbidden(owner,name,args):raise AssertionError('Direct helper crossed callback: '+name)
            vm.native=forbidden
            self.assertFalse(inner())
            vm.player[field]=False
        self.assertEqual(vm.vector(),((0,0),(10,10,10)))

    def test_partial_writer_exception_preserves_throwable_and_faults_all_entries(self):
        vm = FamilyVM(); vm.wait(); exception = {'class': 'Throwable', 'unique': object()}
        def fail(when, name, owner, args):
            if when == 'after' and name == 'UpdateAmmoSkin':
                raise JVMThrown(exception)
        vm.hook = fail
        with self.assertRaises(JVMThrown) as caught:
            vm.insert()
        self.assertIs(caught.exception.value, exception)
        vm.hook = None
        self.assertFalse(vm.player['cojvrIndexedSessionBusy'])
        self.assertFalse(vm.player['cojvrIndexedSessionDispatch'])
        self.assertEqual(vm.vector(), ((0, 1), (10, 10, 10)))
        self.assertFalse(vm.insert_clip())
        self.assertFalse(vm.boolean('cojvrInsertManualNativeCycleRound'))

    def test_shipped_jvm_verifies_combined_production_derivative(self):
        from clip_manual_reload import clip_verifier_class
        java = ARCHIVE.parent / 'jre/bin/java.exe'
        if not java.is_file():
            self.skipTest('Shipped Java 1.4 runtime unavailable')
        player, machine = production_classes()
        with tempfile.TemporaryDirectory(prefix='cojvr-families-verify-') as temp:
            root = Path(temp)
            (root / 'VerifyManualClip.class').write_bytes(clip_verifier_class())
            target = root / 'families.pak'
            with zipfile.ZipFile(io.BytesIO(original_archive())) as source, zipfile.ZipFile(target, 'w') as out:
                for info in source.infolist():
                    out.writestr(info, {'ArmedPlayerBeing.class': player, 'StateMashine.class': machine}.get(
                        info.filename, source.read(info.filename)))
            result = subprocess.run([str(java), '-Xverify:all', '-cp', str(root) + ';' + str(target),
                                     'VerifyManualClip'], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            # A corrupt appended body must actually be loaded, rather than
            # silently verifying the original class from another class path.
            code=next(m.code for m in parse_class(player)[1]
                      if (m.name,m.descriptor)==('cojvrInsertManualIndexedRound','(I)Z'))
            self.assertEqual(player.count(code),1)
            damaged=player.replace(code,b'\xfe'+code[1:],1)
            invalid=root/'invalid.pak'
            with zipfile.ZipFile(target) as source,zipfile.ZipFile(invalid,'w') as out:
                for info in source.infolist():
                    out.writestr(info,damaged if info.filename=='ArmedPlayerBeing.class'
                                 else source.read(info.filename))
            rejected=subprocess.run([str(java),'-Xverify:all','-cp',str(root)+';'+str(invalid),
                                     'VerifyManualClip'],capture_output=True,text=True,timeout=30)
            self.assertNotEqual(rejected.returncode,0)
            self.assertIn('VerifyError',rejected.stdout+rejected.stderr)


if __name__ == '__main__':
    unittest.main()
