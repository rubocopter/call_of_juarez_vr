"""Interpret reload bytecode with modeled scheduling; separately verify Java 1.4."""
import importlib.util
import inspect
import copy
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.dont_write_bytecode=True
sys.path[:0]=[str(Path(__file__).parent),str(Path(__file__).resolve().parents[1]/'tools')]
from indexed_manual_reload import ARCHIVE, IndexedVM, original_archive, PROFILES
from inspect_java_bytecode import parse_class
from reload_wait_probe import VM, JVMThrown


class NativeHandSlots:
    def __init__(self,row):self.row=row
    def __getitem__(self,index):
        return self.row[('m_cActualState','active','m_cDesiredState')[index]]['m_cWeapon']
    def __setitem__(self,index,value):
        self.row[('m_cActualState','active','m_cDesiredState')[index]]['m_cWeapon']=value


class NativeActiveSlots:
    def __init__(self,rows):self.rows=rows
    def __getitem__(self,index):return self.rows[index]['active']['m_cWeapon']


class SessionVM(IndexedVM):
    def __init__(self, builder, kind='WeaponPistolLemant', hand=0):
        super().__init__(lambda _:builder(original_archive(),kind)[0],kind,hand)
        _,machine=builder(original_archive(),kind)
        cp,methods=parse_class(machine)
        self.classes['StateMashine']=cp,{(m.name,m.descriptor):m for m in methods}
        self.states={n:{'class':'PlayerState','id':n,'m_iStateID':n} for n in (1,20,21,22)}
        self.machine.update(m_sCurrentState=self.states[1],m_sDestState=self.states[21],
                            m_fCurrentTime=0.,start=0.,hand=hand)
        self.other=dict(self.machine);self.other['hand']=1-hand
        rows=[{'m_cStateMashine':self.machine if i==hand else self.other,
               'm_cActualState':{'m_cWeapon':self.weapon if i==hand else None},
               'active':{'m_cWeapon':self.weapon if i==hand else None},
               'm_cDesiredState':{'m_cWeapon':self.weapon if i==hand else None,'m_iState':1}}
              for i in (0,1)]
        self.player['m_aHandStateMashine']=rows
        self.player['ActiveWeapon']=NativeActiveSlots(rows)
        self.slots=[NativeHandSlots(row) for row in rows]
        self.desired_state=1
        self.transitions=[]
        self.events=self.automatic_transfers=0
        self.close_rotations=0
        self.on_event=self.on_max=self.on_close=None

    def native(self,owner,name,args):
        try:return self._native(owner,name,args)
        finally:
            self.player['m_aHandStateMashine'][self.hand]['m_cDesiredState']['m_iState']=self.desired_state

    def _native(self,owner,name,args):
        if name=='GetHandStateMashine':return self.machine if args[0]==self.hand else self.other
        if name=='GetHandStateMashineState':return (self.machine if args[0]==self.hand else self.other)['m_sCurrentState']['id']
        if name=='SetDesiredWeaponState':self.desired_state=args[1];return None
        if name=='GetOtherHand':return 1-args[0]
        if name=='GetWeaponReloadState':return 21
        if name=='GetWeaponMoveState':return 1
        if name=='GetWeaponMaxAmmoToReload':
            value=sum(min(cap-self.weapon['m_iAmmo'+str(i)+'Count'],
                self.player['nAmmo'+('Rifle','Shotgun','Pistol')[self.types[i]]])
                for i,cap in enumerate(self.capacities))
            if self.on_max:self.on_max()
            return value
        if name=='WeaponReload':self.automatic_transfers+=1;raise AssertionError('Ascending native transfer must stay vetoed')
        if name=='OnGameEvent':
            self.events+=1
            if self.on_event:self.on_event()
            return None
        if name=='ReloadRotate':self.close_rotations+=1;return None
        if name=='HasBowInHands':return False
        if name=='CanAutomaticAttack':return False
        if name in ('FindStateByID','GetCurrentState','GetDestState','GetPrevState','SetPrevState',
                    'SetCurrentState','IsStoped','CanChangeState','GetNextState','Clear','Set'):
            result=VM.native(self,owner,name,args)
            if name=='Set' and owner is self.states[22] and self.on_close:
                self.on_close()
            return result
        if name=='GetCurrentStateID':return owner['m_sCurrentState']['id']
        return super().native(owner,name,args)

    def arm(self):
        self.player['cojvrManualReloadWeapon']=self.weapon
        self.run('ArmedPlayerBeing','ReloadWeapon','(I)V',self.player,[self.hand])
        if self.desired_state==21:self.machine['m_sCurrentState']=self.states[20]

    def update(self,time):
        self.machine['m_fCurrentTime']=time
        return self.run('StateMashine','Update','()Z',self.machine)

    def wait(self):self.arm();self.update(.31);self.update(.62)
    def insert(self,index=1):
        return self.run('ArmedPlayerBeing','cojvrLabInsertWaitingIndexedUnit','(I)Z',self.player,[index])


class IndexedSessionTests(unittest.TestCase):
    def make(self,kind='WeaponPistolLemant',hand=0):
        self.assertIsNotNone(importlib.util.find_spec('coj_indexed_reload_session'),
                             'Missing supervised indexed session builder')
        from coj_indexed_reload_session import build_indexed_session_lab
        return SessionVM(build_indexed_session_lab,kind,hand)

    def test_explicit_selected_unit_requires_actual_supervised_wait(self):
        for kind in PROFILES:
            for hand in (0,1):
                with self.subTest(kind=kind,hand=hand):
                    vm=self.make(kind,hand)
                    self.assertFalse(vm.insert())
                    vm.arm();self.assertFalse(vm.insert())
                    vm.update(.31);vm.update(.62)
                    self.assertEqual(vm.player['cojvrReloadProbeStatus'],2)
                    self.assertTrue(vm.insert(1))
                    self.assertEqual(vm.vector()[0],(0,1))
                    self.assertEqual(vm.events,1)
                    self.assertEqual(vm.automatic_transfers,0)

    def test_cancel_and_watchdog_close_without_automatic_ammunition(self):
        for opening in (False,True):
            vm=self.make('WeaponRifleRemington')
            vm.arm()
            if not opening:vm.update(.31);vm.update(.62)
            before=vm.vector()
            vm.run('ArmedPlayerBeing','cojvrCancelReloadProbe','()V',vm.player)
            self.assertEqual(vm.machine['m_sCurrentState']['id'],22)
            self.assertFalse(vm.insert())
            vm.update(1.1)
            self.assertEqual(vm.vector(),before)
            self.assertEqual(vm.automatic_transfers,0)
        vm=self.make();vm.wait();vm.update(2.2)
        self.assertEqual(vm.player['cojvrReloadProbeStatus'],3)
        self.assertFalse(vm.insert())
        self.assertEqual(vm.vector(),((0,0),(10,10,10)))

    def test_last_shared_round_or_full_weapon_closes_natively(self):
        for reserve_last in (False,True):
            vm=self.make('WeaponRifleRemington');vm.wait()
            if reserve_last:vm.player['nAmmoShotgun']=1
            else:vm.weapon['m_iAmmo0Count']=1
            self.assertTrue(vm.insert(1))
            self.assertEqual(vm.machine['m_sCurrentState']['id'],22)
            self.assertEqual(vm.player['cojvrReloadProbeStatus'],3)
            self.assertEqual(vm.events,1)
            self.assertFalse(vm.insert(0))
            self.assertEqual(vm.automatic_transfers,0)

    def test_disjoint_selected_units_stay_typed_and_require_retained_ticket(self):
        vm=self.make();vm.wait()
        self.assertTrue(vm.insert(1));self.assertTrue(vm.insert(0))
        self.assertEqual(vm.vector(),((1,1),(9,10,9)))
        self.assertEqual(vm.events,2)
        vm.machine['cojvrReloadProbeOwner']={'foreign':True}
        before=vm.vector();self.assertFalse(vm.insert(1));self.assertEqual(vm.vector(),before)

    def test_cancellation_during_parameter_read_prevents_selected_write(self):
        vm=self.make();vm.wait()
        def cancel(when,name,owner,args):
            if when=='after' and name=='GetAmmoCapacity':
                vm.hook=None
                vm.run('ArmedPlayerBeing','cojvrCancelReloadProbe','()V',vm.player)
        vm.hook=cancel
        self.assertFalse(vm.insert())
        self.assertEqual(vm.vector(),((0,0),(10,10,10)))
        self.assertEqual(vm.skin_calls,0)

    def test_fault_after_loaded_write_quarantines_and_clears_session_latches(self):
        vm=self.make();vm.wait();exception={'class':'Throwable','same':object()}
        def fail(when,name,owner,args):
            if when=='after' and name=='UpdateAmmoSkin':raise JVMThrown(exception)
        vm.hook=fail
        with self.assertRaises(JVMThrown) as caught:vm.insert()
        self.assertIs(caught.exception.value,exception)
        self.assertEqual(vm.vector(),((0,1),(10,10,10)))
        self.assertTrue(vm.player['cojvrIndexedFaulted'])
        self.assertFalse(vm.player['cojvrIndexedBusy'])
        self.assertFalse(vm.player['cojvrIndexedSessionBusy'])
        self.assertFalse(vm.player['cojvrIndexedSessionDispatch'])
        vm.hook=None;before=vm.vector()
        self.assertFalse(vm.insert())
        self.assertFalse(vm.run('ArmedPlayerBeing','cojvrKeepManualReloadAlive','()Z',vm.player))
        self.assertEqual(vm.vector(),before)
        vm.update(2.2)
        self.assertEqual(vm.machine['m_sCurrentState']['id'],22)

    def test_reentry_and_direct_unscoped_unit_call_cannot_duplicate(self):
        vm=self.make();vm.wait()
        # Even a body21 wait is insufficient to call the inner unit directly.
        self.assertFalse(IndexedVM.insert(vm,1))
        replies=[]
        def reenter(when,name,owner,args):
            if when=='before' and name=='GetAmmoCapacity':
                vm.hook=None;replies.append(vm.insert())
        vm.hook=reenter
        self.assertTrue(vm.insert());self.assertEqual(replies,[False])
        self.assertEqual(vm.vector(),((0,1),(10,10,9)))

    def test_unknown_inputs_reject_before_composition(self):
        from coj_indexed_reload_session import build_indexed_session_lab
        for data,kind in ((original_archive()+b'changed','WeaponPistolLemant'),
                          (original_archive(),'WeaponRifleWinchesterScope')):
            with self.assertRaises(ValueError):build_indexed_session_lab(data,kind)

    def test_event_and_close_callbacks_cannot_reenter_or_report_changed_owner_success(self):
        for change in ('reentry','loaded','reserve','ticket','exception'):
            with self.subTest(change=change):
                vm=self.make();vm.wait();replies=[];exception={'class':'Throwable'}
                def event():
                    if change=='reentry':replies.append(vm.insert())
                    if change=='loaded':vm.weapon['m_iAmmo0Count']=1
                    if change=='reserve':vm.player['nAmmoRifle']-=1
                    if change=='ticket':vm.player['cojvrReloadProbeMachine']=vm.other
                    if change=='exception':raise JVMThrown(exception)
                vm.on_event=event
                if change=='exception':
                    with self.assertRaises(JVMThrown) as caught:vm.insert()
                    self.assertIs(caught.exception.value,exception)
                else:self.assertEqual(bool(vm.insert()),change=='reentry')
                self.assertEqual(vm.events,1)
                self.assertFalse(vm.player['cojvrIndexedSessionBusy'])
                self.assertFalse(vm.player['cojvrIndexedSessionDispatch'])
                if change=='reentry':self.assertEqual(replies,[False])
                else:
                    self.assertTrue(vm.player['cojvrIndexedFaulted'])
                    vm.on_event=None;before=vm.vector();self.assertFalse(vm.insert())
                    self.assertEqual(vm.vector(),before)
        vm=self.make('WeaponRifleRemington');vm.wait();vm.player['nAmmoShotgun']=1
        vm.on_max=lambda:vm.player.update(cojvrReloadProbeMachine=vm.other)
        self.assertFalse(vm.insert())
        self.assertEqual(vm.machine['m_sCurrentState']['id'],21)
        self.assertEqual(vm.other['m_sCurrentState']['id'],1)
        self.assertTrue(vm.player['cojvrIndexedFaulted'])

    def test_expired_clock_and_foreign_probe_never_accept(self):
        for change in ('deadline','nan','reverse','machine','owner','cancelled','status'):
            with self.subTest(change=change):
                vm=self.make();vm.wait()
                if change=='deadline':vm.machine['m_fCurrentTime']=3.
                if change=='nan':vm.machine['m_fCurrentTime']=float('nan')
                if change=='reverse':vm.machine['m_fCurrentTime']=0.
                if change=='machine':vm.player['cojvrReloadProbeMachine']=vm.other
                if change=='owner':vm.machine['cojvrReloadProbeOwner']={}
                if change=='cancelled':vm.player['cojvrReloadProbeCancelled']=True
                if change=='status':vm.player['cojvrReloadProbeStatus']=1
                before=vm.vector();self.assertFalse(vm.insert());self.assertEqual(vm.vector(),before)

    def test_event_is_not_published_for_empty_selected_pool_or_invalid_index(self):
        vm=self.make();vm.wait();vm.player['nAmmoPistol']=0
        self.assertFalse(vm.insert(1));self.assertEqual(vm.events,0)
        self.assertEqual(vm.vector()[0],(0,0))
        self.assertTrue(vm.insert(0));self.assertEqual(vm.events,1)
        for index in (-1,2,3):
            before=vm.vector();self.assertFalse(vm.insert(index));self.assertEqual(vm.vector(),before)

    def test_exact_profile_admission_and_occupied_support_never_fall_through_to_native_fill(self):
        for change in ('class','whole_clip','support'):
            vm=self.make('WeaponRifleRemington')
            if change=='class':vm.weapon['class']='DerivedRemington'
            if change=='whole_clip':
                native=vm.native
                vm.native=lambda owner,name,args:False if name=='IsReloadWholeClipAtOnce' else native(owner,name,args)
            if change=='support':vm.slots[1-vm.hand]=[{'class':'foreign'}]*3
            vm.wait()
            before=vm.vector();self.assertFalse(vm.insert());self.assertEqual(vm.vector(),before)
            self.assertEqual(vm.automatic_transfers,0)

    def test_composed_constant_pool_and_inner_dispatch_are_validated(self):
        from coj_indexed_reload_session import build_indexed_session_lab
        from inspect_java_bytecode import instruction_offsets
        for kind in PROFILES:
            player,machine=build_indexed_session_lab(original_archive(),kind)
            for data in (player,machine):
                cp,methods=parse_class(data)
                for i,entry in enumerate(cp.entries):
                    if i and entry is not None:cp.describe(i)
                for m in methods:
                    if m.code is None:continue
                    boundaries=instruction_offsets(m.code)
                    for start,end,handler,catch in m.exception_table:
                        self.assertIn(start,boundaries);self.assertIn(handler,boundaries)
                        self.assertTrue(end==len(m.code) or end in boundaries)
                        if catch:self.assertEqual(cp.get(catch)[0],'Class')
            names={(m.name,m.descriptor) for m in parse_class(player)[1]}
            self.assertNotIn(('cojvrInsertManualRound','()Z'),names,
                             'Indexed session must not expose the ascending insertion helper')

    def test_shipped_jvm_verifies_every_indexed_session_derivative(self):
        """The interpreter does not model Java 1.4 bytecode verification."""
        from clip_manual_reload import clip_verifier_class
        from coj_indexed_reload_session import build_indexed_session_lab
        java = ARCHIVE.parent / 'jre/bin/java.exe'
        if not java.is_file():
            self.skipTest('Shipped Java 1.4 runtime required')
        original = original_archive()
        with tempfile.TemporaryDirectory(prefix='cojvr-indexed-verify-') as temp:
            root = Path(temp)
            (root / 'VerifyManualClip.class').write_bytes(clip_verifier_class())

            def verify(path):
                return subprocess.run([str(java), '-Xverify:all', '-cp',
                                       str(root) + ';' + str(path), 'VerifyManualClip'],
                                      capture_output=True, text=True, timeout=30)

            for weapon,cycle_animation in ((w,c) for w in PROFILES for c in (False,True)):
                player, machine = build_indexed_session_lab(original, weapon,cycle_animation=cycle_animation)
                target = root / (weapon + ('-cycle' if cycle_animation else '') + '.pak')
                with zipfile.ZipFile(io.BytesIO(original)) as source, zipfile.ZipFile(target, 'w') as out:
                    for info in source.infolist():
                        out.writestr(info, {'ArmedPlayerBeing.class': player,
                                           'StateMashine.class': machine}.get(
                                               info.filename, source.read(info.filename)))
                result = verify(target)
                self.assertEqual(result.returncode, 0, weapon + ': ' + result.stdout + result.stderr)
                if weapon == next(iter(PROFILES)):
                    _, methods = parse_class(player)
                    code = next(m.code for m in methods if (m.name, m.descriptor) ==
                                ('cojvrLabInsertWaitingIndexedUnit', '(I)Z'))
                    self.assertEqual(player.count(code), 1)
                    damaged = player.replace(code, b'\xfe' + code[1:], 1)
                    bad = root / 'invalid-indexed.pak'
                    with zipfile.ZipFile(target) as good, zipfile.ZipFile(bad, 'w') as out:
                        for info in good.infolist():
                            out.writestr(info, damaged if info.filename == 'ArmedPlayerBeing.class'
                                         else good.read(info.filename))
                    rejected = verify(bad)
                    self.assertNotEqual(rejected.returncode, 0)
                    self.assertIn('VerifyError', rejected.stdout + rejected.stderr)

    def test_every_observed_callback_fault_rethrows_and_retires_outer_scope(self):
        for closing in (False,True):
            template=self.make('WeaponRifleRemington' if closing else 'WeaponPistolLemant')
            template.wait()
            if closing:template.player['nAmmoShotgun']=1
            def clone():
                vm=copy.copy(template)
                state=copy.deepcopy({key:getattr(template,key) for key in
                    ('player','weapon','machine','other','states','slots','counters','types','capacities')})
                for key,value in state.items():setattr(vm,key,value)
                vm.calls=[];vm.writes=[];vm.frames=[];vm.transitions=[]
                return vm
            baseline=clone();observed=[];native=baseline.native
            def observe(owner,name,args):
                observed.append(('before',name));result=native(owner,name,args)
                observed.append(('after',name));return result
            baseline.native=observe;self.assertTrue(baseline.insert())
            self.assertGreater(len(observed),500)
            for position in range(len(observed)):
                vm=clone();native=vm.native;seen=[0];exception={'class':'Throwable','at':position}
                def fault():
                    current=seen[0];seen[0]+=1
                    if current==position:raise JVMThrown(exception)
                def inject(owner,name,args):
                    fault();result=native(owner,name,args);fault();return result
                vm.native=inject
                with self.subTest(closing=closing,position=position,boundary=observed[position]):
                    with self.assertRaises(JVMThrown) as caught:vm.insert()
                    self.assertIs(caught.exception.value,exception)
                    self.assertFalse(vm.player['cojvrIndexedSessionBusy'])
                    self.assertFalse(vm.player['cojvrIndexedSessionDispatch'])
                    self.assertFalse(vm.player.get('cojvrIndexedBusy',False))
                    self.assertTrue(vm.player['cojvrIndexedFaulted'])
                    before=vm.vector();vm.native=native
                    self.assertFalse(vm.insert());self.assertEqual(vm.vector(),before)

    def test_last_validator_callback_cannot_invalidate_wait_before_actions(self):
        for ordinal,expected in ((4,((0,0),(10,10,10),0)),
                                 (6,((0,1),(10,10,10),0)),
                                 (8,((0,1),(10,10,9),0))):
            for change in ('veto_owner','deadline'):
                vm=self.make();vm.wait();seen=[0]
                def invalidate(when,name,owner,args):
                    if when=='after' and name=='GetAmmoTypeCount' and ('ArmedPlayerBeing','cojvrManualReloadValid') in vm.frames:
                        seen[0]+=1
                        if seen[0]==ordinal:
                            if change=='veto_owner':vm.machine['cojvrReloadProbeOwner']={'foreign':True}
                            else:vm.machine['m_fCurrentTime']=vm.player['cojvrReloadProbeUntil']
                vm.hook=invalidate
                self.assertFalse(vm.insert())
                self.assertEqual((*vm.vector(),vm.events),expected)

    def test_close_callback_must_not_redirect_native_state_setter(self):
        vm=self.make('WeaponRifleRemington');vm.wait();vm.player['nAmmoShotgun']=1
        vm.other['m_sCurrentState']=vm.states[20]
        native=vm.native;swapped=[False]
        def swap(owner,name,args):
            result=native(owner,name,args)
            if name=='GetWeaponMoveState' and not swapped[0]:
                swapped[0]=True;vm.player['cojvrReloadProbeMachine']=vm.other
            return result
        vm.native=swap
        self.assertFalse(vm.insert())
        self.assertEqual(vm.machine['m_sCurrentState']['id'],21)
        self.assertEqual(vm.other['m_sCurrentState']['id'],20)
        self.assertTrue(vm.player['cojvrIndexedFaulted'])

    def test_post_close_callbacks_preserve_selected_unit_and_retained_owners(self):
        for change in ('loaded','reserve','counter','counter_max','owner','ticket','state',
                       'hand_machine','reentry','exception'):
            with self.subTest(change=change):
                vm=self.make('WeaponRifleRemington');vm.wait()
                vm.player['nAmmoShotgun']=1
                replies=[];exception={'class':'Throwable','close':change}
                def close():
                    if change=='loaded':vm.weapon['m_iAmmo1Count']=0
                    if change=='reserve':vm.player['nAmmoShotgun']=9
                    if change=='counter':vm.counters[1]=dict(vm.counters[1])
                    if change=='counter_max':vm.counters[1]['nMaxAmount']-=1
                    if change=='owner':vm.weapon['cOwner']={'class':'foreign'}
                    if change=='ticket':vm.player['cojvrReloadProbeWeapon']={'class':'foreign'}
                    if change=='state':vm.machine['m_sCurrentState']=vm.states[1]
                    if change=='hand_machine':
                        vm.player['m_aHandStateMashine'][vm.hand]['m_cStateMashine']=vm.other
                    if change=='reentry':replies.append(vm.insert())
                    if change=='exception':raise JVMThrown(exception)
                vm.on_close=close
                if change=='exception':
                    with self.assertRaises(JVMThrown) as caught:vm.insert()
                    self.assertIs(caught.exception.value,exception)
                else:self.assertEqual(bool(vm.insert()),change=='reentry')
                self.assertEqual(vm.events,1)
                self.assertEqual(vm.automatic_transfers,0)
                self.assertFalse(vm.player['cojvrIndexedSessionBusy'])
                self.assertFalse(vm.player['cojvrIndexedSessionDispatch'])
                if change=='reentry':
                    self.assertEqual(replies,[False])
                    self.assertFalse(vm.player.get('cojvrIndexedFaulted',False))
                else:
                    self.assertTrue(vm.player['cojvrIndexedFaulted'])
                    vm.on_close=None;before=vm.vector()
                    self.assertFalse(vm.insert())
                    self.assertEqual(vm.vector(),before)
                    self.assertFalse(vm.run('ArmedPlayerBeing','cojvrKeepManualReloadAlive',
                                            '()Z',vm.player))

    def test_exhausted_wait_closes_even_when_no_unit_can_be_inserted(self):
        for full in (False,True):
            vm=self.make('WeaponRifleRemington');vm.wait()
            if full:vm.weapon.update(m_iAmmo0Count=1,m_iAmmo1Count=1)
            else:vm.player['nAmmoShotgun']=0
            before=vm.vector();self.assertFalse(vm.insert())
            self.assertEqual(vm.vector(),before);self.assertEqual(vm.events,0)
            self.assertEqual(vm.machine['m_sCurrentState']['id'],22)
            self.assertFalse(vm.run('ArmedPlayerBeing','cojvrKeepManualReloadAlive','()Z',vm.player))


class IndexedBodyCycleVM(SessionVM):
    """Original Started/Reload/skin setters; native scheduler/audio are stand-ins."""
    def __init__(self, kind='WeaponPistolLemant', hand=0, cycle=True, *, builder=None):
        from coj_indexed_reload_session import build_indexed_session_lab
        if builder is None:
            def builder(archive, weapon):
                return build_indexed_session_lab(archive, weapon, cycle_animation=cycle)
        super().__init__(builder, kind, hand)
        self.body_starts=0;self.zero_reloads=[];self.sounds=[];self.important=[]
        self.on_body=self.on_clear=None
        self.ammo_params=[{'class':'WpnAmmo','m_cSBReload':{'index':i}} for i in (0,1)]

    def run(self, cls, name, desc, owner, args=()):
        if name=='OnHandStateStarted_Reload':
            # This original two-index loop uses iinc, supported by the older
            # wait interpreter. All emitted helpers retain IndexedVM's engine.
            return VM.run(self,cls,name,desc,owner,args)
        return super().run(cls,name,desc,owner,args)

    def _native(self, owner, name, args):
        if name=='IncHandImportatntState':self.important.append(tuple(args));return None
        if name=='GetAmmoParams':return self.ammo_params[args[0]]
        if name=='ExecSound':self.sounds.append(args[0]);return 0
        if name in ('Reload','ReloadInside','CanReload'):
            if name=='Reload':
                self.zero_reloads.append(tuple(args))
                assert args[1]==0, 'Body replay must never transfer an inventory unit'
            desc='(I)Z' if name=='CanReload' else '(II)I'
            return self.run('Weapon',name,desc,owner,args)
        if name=='Clear' and owner is self.states[21] and self.on_clear:self.on_clear()
        result=super()._native(owner,name,args)
        if name=='Set' and owner is self.states[21]:
            self.body_starts+=1
            self.run('ArmedPlayerBeing','OnHandStateStarted_Reload','(IIZ)V',self.player,[self.hand,21,False])
            if self.on_body:self.on_body()
        return result


class IndexedBodyCycleTests(unittest.TestCase):
    def make(self,kind='WeaponPistolLemant',hand=0,cycle=True):
        from coj_indexed_reload_session import build_indexed_session_lab
        self.assertIn('cycle_animation',inspect.signature(build_indexed_session_lab).parameters,
                      'Missing isolated indexed body21 replay opt-in')
        vm=IndexedBodyCycleVM(kind,hand,cycle);vm.wait();return vm

    def test_selected_unit_replays_real_started_and_waits_before_next_unit(self):
        for kind in PROFILES:
            for hand in (0,1):
                with self.subTest(kind=kind,hand=hand):
                    vm=self.make(kind,hand);starts=vm.body_starts
                    before_max=[counter['nMaxAmount'] for counter in vm.counters]
                    self.assertTrue(vm.insert(1))
                    pool=PROFILES[kind][1][1]
                    self.assertEqual(vm.vector(),((0,1),tuple(9 if p==pool else 10 for p in range(3))))
                    self.assertEqual(vm.body_starts,starts+1)
                    self.assertEqual(vm.zero_reloads[-2:],[(0,0),(1,0)])
                    self.assertEqual([counter['nMaxAmount'] for counter in vm.counters],before_max)
                    self.assertEqual((vm.events,vm.automatic_transfers,vm.desired_state),(1,0,21))
                    self.assertEqual(vm.player['cojvrReloadProbeStatus'],1)
                    before=vm.vector();self.assertFalse(vm.insert(0));self.assertEqual(vm.vector(),before)
                    vm.update(.8);self.assertEqual(vm.player['cojvrReloadProbeStatus'],1)
                    vm.update(.96);self.assertEqual(vm.player['cojvrReloadProbeStatus'],2)
                    self.assertTrue(vm.insert(0))
                    if kind=='WeaponPistolLemant':
                        self.assertEqual(vm.player['cojvrReloadProbeStatus'],1)
                    else:
                        self.assertEqual(vm.machine['m_sCurrentState']['id'],22)
                        self.assertEqual(vm.body_starts,starts+1)

    def test_lemant_eight_pistol_units_and_rifle_close_without_duplicates(self):
        vm=self.make();vm.player['nAmmoPistol']=8;vm.player['nAmmoRifle']=1
        for count in range(1,9):
            self.assertTrue(vm.insert(1))
            self.assertEqual(vm.vector(),((0,count),(1,10,8-count)))
            self.assertEqual(vm.events,count)
            self.assertFalse(vm.insert(1))
            vm.update(vm.machine['m_fCurrentTime']+.31)
        self.assertEqual(vm.player['cojvrReloadProbeStatus'],2)
        starts=vm.body_starts
        self.assertTrue(vm.insert(0))
        self.assertEqual(vm.vector(),((1,8),(0,10,0)))
        self.assertEqual((vm.events,vm.body_starts,vm.machine['m_sCurrentState']['id']),(9,starts,22))
        vm.update(vm.machine['m_fCurrentTime']+.31)
        self.assertEqual(vm.close_rotations,1)
        self.assertEqual(vm.automatic_transfers,0)

    def test_last_reserve_and_watchdog_close_do_not_restart_or_transfer(self):
        for kind in PROFILES:
            vm=self.make(kind);pool=PROFILES[kind][1][1]
            # Exhaust every pool the weapon uses, except the selected last unit.
            for p in set(vm.types):vm.player['nAmmo'+('Rifle','Shotgun','Pistol')[p]]=0
            vm.player['nAmmo'+('Rifle','Shotgun','Pistol')[pool]]=1
            starts=vm.body_starts;self.assertTrue(vm.insert(1))
            self.assertEqual(vm.body_starts,starts)
            self.assertEqual(vm.machine['m_sCurrentState']['id'],22)
            before=vm.vector();vm.update(1.);self.assertEqual(vm.vector(),before)
        vm=self.make();self.assertTrue(vm.insert(1));before=vm.vector()
        vm.update(.96);vm.update(2.6);vm.update(2.95)
        self.assertEqual(vm.vector(),before);self.assertEqual(vm.events,1)
        self.assertEqual(vm.close_rotations,1)

    def test_self_clear_guard_is_scoped_and_naive_replay_still_invalidates(self):
        vm=self.make();self.assertTrue(vm.insert(1))
        self.assertFalse(vm.player.get('cojvrIndexedCycleSelfClear',False))
        before=vm.vector()
        vm.run('StateMashine','SetCurrentState','(LBaseState;)V',vm.machine,[vm.states[21]])
        self.assertEqual(vm.desired_state,1);self.assertEqual(vm.vector(),before)
        vm=self.make(cycle=False);self.assertTrue(vm.insert(1))
        vm.run('StateMashine','SetCurrentState','(LBaseState;)V',vm.machine,[vm.states[21]])
        self.assertEqual(vm.desired_state,1)

    def test_reentry_from_started_event_and_clear_cannot_insert_again(self):
        for point in ('on_body','on_event','on_clear'):
            vm=self.make();answers=[]
            setattr(vm,point,lambda:answers.append(vm.insert(0)))
            self.assertTrue(vm.insert(1));self.assertTrue(answers)
            self.assertFalse(any(answers));self.assertEqual(vm.events,1)
            self.assertEqual(vm.vector(),((0,1),(10,10,9)))

    def test_probe_loss_or_throw_in_replay_keeps_committed_unit_and_no_late_refill(self):
        for point in ('on_body','on_clear','on_event'):
            for throws in (False,True):
                vm=self.make();exception={'class':'Throwable','unique':object()}
                def invalidate():
                    setattr(vm,point,None)
                    vm.run('ArmedPlayerBeing','cojvrClearReloadProbe','(I)V',vm.player,[vm.hand])
                    if throws:raise JVMThrown(exception)
                setattr(vm,point,invalidate)
                if throws:
                    with self.assertRaises(JVMThrown) as caught:vm.insert(1)
                    self.assertIs(caught.exception.value,exception)
                else:self.assertFalse(vm.insert(1))
                self.assertEqual(vm.vector(),((0,1),(10,10,9)))
                self.assertFalse(vm.player['cojvrIndexedSessionBusy'])
                self.assertFalse(vm.player.get('cojvrIndexedCycleSelfClear',False))
                self.assertFalse(vm.insert(0))
                before=vm.vector();vm.update(1.);vm.update(1.4)
                self.assertEqual(vm.vector(),before);self.assertEqual(vm.automatic_transfers,0)

    def test_body_callbacks_cannot_change_vector_counter_maximum_or_owner(self):
        for change in ('loaded','other_pool','maximum','counter','owner','desired'):
            vm=self.make()
            def mutate():
                if change=='loaded':vm.weapon['m_iAmmo0Count']=1
                if change=='other_pool':vm.player['nAmmoRifle']=9
                if change=='maximum':vm.counters[0]['nMaxAmount']+=1
                if change=='counter':vm.counters[0]=dict(vm.counters[0])
                if change=='owner':vm.weapon['cOwner']={'foreign':True}
                if change=='desired':vm.desired_state=1
            vm.on_body=mutate
            self.assertFalse(vm.insert(1));self.assertTrue(vm.player['cojvrIndexedFaulted'])
            vm.on_body=None;before=vm.vector();self.assertFalse(vm.insert(0));self.assertEqual(vm.vector(),before)
            self.assertFalse(vm.player['cojvrIndexedSessionBusy'])
            self.assertFalse(vm.player.get('cojvrIndexedCycleSelfClear',False))

    def test_synchronous_guard_rejects_other_hand_state_and_retired_body(self):
        vm=self.make();answers=[]
        def check():
            for hand,state in ((vm.hand,21),(1-vm.hand,21),(vm.hand,149),(vm.hand,22)):
                answers.append(bool(vm.run('ArmedPlayerBeing','cojvrIndexedCycleSuppressCompletion',
                                          '(II)Z',vm.player,[hand,state])))
            vm.machine['m_sCurrentState']=vm.states[22]
            answers.append(bool(vm.run('ArmedPlayerBeing','cojvrIndexedCycleSuppressCompletion',
                                      '(II)Z',vm.player,[vm.hand,21])))
            vm.machine['m_sCurrentState']=vm.states[21]
            row=vm.player['m_aHandStateMashine'][vm.hand]
            row['m_cStateMashine']=vm.other
            answers.append(bool(vm.run('ArmedPlayerBeing','cojvrIndexedCycleSuppressCompletion',
                                      '(II)Z',vm.player,[vm.hand,21])))
            row['m_cStateMashine']=vm.machine
        vm.on_clear=check
        self.assertTrue(vm.insert(1));self.assertEqual(answers,[True,False,False,False,False,False])
        self.assertFalse(vm.run('ArmedPlayerBeing','cojvrIndexedCycleSuppressCompletion',
                               '(II)Z',vm.player,[vm.hand,21]))

    def test_full_selected_slot_does_not_gain_body_replay_credit(self):
        for kind in PROFILES:
            vm=self.make(kind);vm.weapon['m_iAmmo1Count']=vm.capacities[1]
            before=vm.vector();starts=vm.body_starts
            self.assertFalse(vm.insert(1));self.assertEqual(vm.vector(),before)
            self.assertEqual((vm.body_starts,vm.events),(starts,0))
            self.assertEqual(vm.player['cojvrReloadProbeStatus'],2)

    def test_exception_with_intact_probe_cannot_rearm_or_leak_scope(self):
        for point in ('on_body','on_clear','on_event'):
            vm=self.make();exception={'class':'Throwable','point':point}
            def fail():raise JVMThrown(exception)
            setattr(vm,point,fail)
            with self.assertRaises(JVMThrown) as caught:vm.insert(1)
            self.assertIs(caught.exception.value,exception)
            setattr(vm,point,None)
            self.assertFalse(vm.player['cojvrIndexedSessionBusy'])
            self.assertFalse(vm.player['cojvrIndexedCycleSelfClear'])
            self.assertFalse(vm.player['cojvrIndexedSessionDispatch'])
            self.assertTrue(vm.player['cojvrIndexedFaulted'])
            self.assertFalse(vm.run('ArmedPlayerBeing','cojvrKeepManualReloadAlive','()Z',vm.player))
            before=vm.vector();vm.update(3.);vm.update(3.4)
            self.assertEqual(vm.vector(),before);self.assertEqual(vm.automatic_transfers,0)

    def test_cycle_optin_rejects_noncanonical_runtime_endpoint(self):
        from coj_indexed_reload_session import _append_indexed_session
        with self.assertRaisesRegex(ValueError,'canonical runtime'):
            _append_indexed_session(b'not even parsed',runtime=True,cycle_animation=True)

    def test_probe_loss_after_accepted_cycle_keeps_late_completion_zero_then_retires(self):
        for kind in PROFILES:
            vm=self.make(kind);self.assertTrue(vm.insert(1));before=vm.vector()
            vm.run('ArmedPlayerBeing','cojvrClearReloadProbe','(I)V',vm.player,[vm.hand])
            native=vm.native
            def natural(owner,name,args):
                if name=='WeaponReload':return VM.run(vm,'PawnArmed',name,'(LWeapon;)Z',owner,args)
                return native(owner,name,args)
            vm.native=natural
            vm.update(.96);vm.update(1.3)
            self.assertEqual(vm.vector(),before);self.assertEqual(vm.events,1)
            self.assertIsNone(vm.player['cojvrIndexedDebtMachine'])
            vm.player['cojvrManualReloadWeapon']=None
            vm.run('ArmedPlayerBeing','ReloadWeapon','(I)V',vm.player,[vm.hand])
            vm.machine.update(m_sCurrentState=vm.states[20],start=1.3)
            vm.update(1.7);vm.update(2.1)
            self.assertGreater(sum(vm.vector()[0]),sum(before[0]))
            self.assertEqual(vm.events,2)

    def test_partial_setter_and_changed_body_data_throw_keep_original_debt(self):
        for point in ('partial_loaded','body_reserve','body_maximum'):
            vm=self.make();exception={'class':'Throwable','point':point}
            retained=vm.machine
            def fail(when,name,owner,args):
                if point=='partial_loaded' and when=='after' and name=='UpdateAmmoSkin':
                    vm.hook=None
                    raise JVMThrown(exception)
            def body():
                if point=='body_reserve':vm.player['nAmmoRifle']=9
                if point=='body_maximum':vm.counters[0]['nMaxAmount']+=1
                raise JVMThrown(exception)
            if point=='partial_loaded':vm.hook=fail
            else:vm.on_body=body
            with self.assertRaises(JVMThrown) as caught:vm.insert(1)
            self.assertIs(caught.exception.value,exception)
            self.assertIs(vm.player['cojvrIndexedDebtMachine'],retained)
            self.assertIs(vm.player['cojvrIndexedDebtBody'],vm.states[21])
            self.assertEqual(vm.weapon['m_iAmmo1Count'],1)
            self.assertEqual(vm.player['nAmmoPistol'],10 if point=='partial_loaded' else 9)
            self.assertEqual(vm.events,0 if point=='partial_loaded' else 1)
            self.assertTrue(vm.player['cojvrIndexedFaulted'])
            self.assertFalse(vm.player['cojvrIndexedBusy'])
            self.assertFalse(vm.player['cojvrIndexedSessionBusy'])
            self.assertFalse(vm.player['cojvrIndexedCycleSelfClear'])
            vm.on_body=None;vm.hook=None
            before=vm.vector();self.assertFalse(vm.insert(0));self.assertEqual(vm.vector(),before)
            vm.run('ArmedPlayerBeing','cojvrClearReloadProbe','(I)V',vm.player,[vm.hand])
            native=vm.native
            def natural(owner,name,args):
                if name=='WeaponReload':return VM.run(vm,'PawnArmed',name,'(LWeapon;)Z',owner,args)
                return native(owner,name,args)
            vm.native=natural;vm.update(1.);vm.update(1.4)
            self.assertEqual(vm.vector(),before)


if __name__=='__main__':unittest.main()
