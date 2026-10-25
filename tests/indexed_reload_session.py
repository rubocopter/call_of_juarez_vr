"""Compose emitted wait/session/indexed storage bytecode; never launch a JVM."""
import importlib.util
import copy
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode=True
sys.path[:0]=[str(Path(__file__).parent),str(Path(__file__).resolve().parents[1]/'tools')]
from indexed_manual_reload import IndexedVM, original_archive, PROFILES
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
        self.on_event=self.on_max=None

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
            return VM.native(self,owner,name,args)
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

    def test_exhausted_wait_closes_even_when_no_unit_can_be_inserted(self):
        for full in (False,True):
            vm=self.make('WeaponRifleRemington');vm.wait()
            if full:vm.weapon.update(m_iAmmo0Count=1,m_iAmmo1Count=1)
            else:vm.player['nAmmoShotgun']=0
            before=vm.vector();self.assertFalse(vm.insert())
            self.assertEqual(vm.vector(),before);self.assertEqual(vm.events,0)
            self.assertEqual(vm.machine['m_sCurrentState']['id'],22)
            self.assertFalse(vm.run('ArmedPlayerBeing','cojvrKeepManualReloadAlive','()Z',vm.player))


if __name__=='__main__':unittest.main()
