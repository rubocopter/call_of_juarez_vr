"""In-memory indexed wait/session composition; no installer or runtime admission.

The ordinary native request, pre-Clear veto and close remain authoritative.
Selected unit storage is reachable only from this derivative's retained wait.
The body cycle restarts the original callbacks after one verified selected
unit. The standalone lab keeps explicit opt-in; the canonical production
appendix enables it with exact-class admission and shared transaction guards.
Native animation playback is not validated.
"""
import hashlib
import io
import zipfile

from coj_indexed_reload import ORIGINAL_ARCHIVE_SHA256, PROFILES, _append_indexed_helpers
from coj_reload_wait_probe import ClassEdit
from inspect_java_bytecode import parse_class, Reader, parse_constant_pool
from patch_coj_weapon_consumers import Code, _patch_class, insert_target_tag, u2


def _profile(weapon):
    for profile in PROFILES:
        if profile[0]==weapon:return profile
    raise ValueError('No exact indexed session profile')


def _patch_indexed_player(player,weapon):
    return _patch_class(player,(weapon,),_whole_clip_expected=_profile(weapon)[3])


def _emit_retained_wait(e,c,weapon,hand,machine,failure,*,desired=True,move_local=None):
    """Callback-free proof of exact Java ticket/hand/state fields and clock."""
    own='ArmedPlayerBeing'
    def field(owner,name,desc):return e.ref(owner,name,desc,9)
    def local(slot,integer=False):c.emit(0x15 if integer else 0x19,slot)
    for name,desc,slot in (('Weapon','LWeapon;',weapon),('Hand','I',hand),('Machine','LPlayerStateMashine;',machine)):
        c.emit(0x2a);c.ref(0xb4,field(own,'cojvrReloadProbe'+name,desc));local(slot,name=='Hand')
        c.branch(0xa0 if name=='Hand' else 0xa6,failure)
    for name,desc,value in (('Status','I',2),('Cancelled','Z',0)):
        c.emit(0x2a);c.ref(0xb4,field(own,'cojvrReloadProbe'+name,desc));c.emit(value+3);c.branch(0xa0,failure)
    local(weapon);c.ref(0xb4,field('Weapon','cOwner','LPawnInventory;'));c.emit(0x2a);c.branch(0xa6,failure)
    local(machine);c.ref(0xb4,field('StateMashine','cojvrReloadProbeOwner','LArmedPlayerBeing;'))
    c.emit(0x2a);c.branch(0xa6,failure)
    # Float subtraction rejects NaN and both infinities, as the existing veto.
    clock=field('PlayerStateMashine','m_fCurrentTime','F')
    local(machine);c.ref(0xb4,clock);local(machine);c.ref(0xb4,clock)
    c.emit(0x66,0x0b,0x95);c.branch(0x9a,failure)
    for bound,comparison in (('Since',0x9b),('Until',0x9c)):
        local(machine);c.ref(0xb4,clock);c.emit(0x2a)
        c.ref(0xb4,field(own,'cojvrReloadProbe'+bound,'F'));c.emit(0x95 if bound=='Since' else 0x96)
        c.branch(comparison,failure)
    current=field('StateMashine','m_sCurrentState','LBaseState;')
    local(machine);c.ref(0xb4,current);c.branch(0xc6,failure)
    local(machine);c.ref(0xb4,current);c.ref(0xc0,e.cls('PlayerState'))
    c.ref(0xb4,field('PlayerState','m_iStateID','I'));c.emit(0x10,21);c.branch(0xa0,failure)
    # These are the exact field-only bodies of the pinned original getters.
    hand_machine=field(own,'m_aHandStateMashine','[LArmedPlayerBeing$HandStateMashine;')
    def slot(other=False):
        c.emit(0x2a);c.ref(0xb4,hand_machine)
        if other:c.emit(0x04)
        local(hand,True)
        if other:c.emit(0x64)
        c.emit(0x32)
    slot();c.ref(0xb4,field('ArmedPlayerBeing$HandStateMashine','m_cStateMashine','LPlayerStateMashine;'))
    local(machine);c.branch(0xa6,failure)
    for state in ('m_cActualState','m_cDesiredState'):
        slot();c.ref(0xb4,field('ArmedPlayerBeing$HandStateMashine',state,'LArmedPlayerBeing$HandState;'))
        c.ref(0xb4,field('ArmedPlayerBeing$HandState','m_cWeapon','LWeapon;'));local(weapon);c.branch(0xa6,failure)
    slot();c.ref(0xb4,field('ArmedPlayerBeing$HandStateMashine','m_cDesiredState','LArmedPlayerBeing$HandState;'))
    if desired or move_local is not None:
        c.ref(0xb4,field('ArmedPlayerBeing$HandState','m_iState','I'))
        if desired:c.emit(0x10,21)
        else:local(move_local,True)
        c.branch(0xa0,failure)
    else:c.emit(0x57)
    for state in ('m_cActualState','m_cDesiredState'):
        slot(True);c.ref(0xb4,field('ArmedPlayerBeing$HandStateMashine',state,'LArmedPlayerBeing$HandState;'))
        c.ref(0xb4,field('ArmedPlayerBeing$HandState','m_cWeapon','LWeapon;'));c.branch(0xc7,failure)
    for other in (False,True):
        c.emit(0x2a);c.ref(0xb4,field(own,'ActiveWeapon','[LWeapon;'))
        if other:c.emit(0x04)
        local(hand,True)
        if other:c.emit(0x64)
        c.emit(0x32)
        if other:c.branch(0xc7,failure)
        else:local(weapon);c.branch(0xa6,failure)


def _emit_retained_closed(e,c,weapon,hand,machine,failure):
    """Field-only post-close ownership proof; no mutable-probe retargeting."""
    own='ArmedPlayerBeing'
    def field(owner,name,desc):return e.ref(owner,name,desc,9)
    def local(slot,integer=False):c.emit(0x15 if integer else 0x19,slot)
    for name,desc,slot in (('Weapon','LWeapon;',weapon),('Hand','I',hand),
                           ('Machine','LPlayerStateMashine;',machine)):
        c.emit(0x2a);c.ref(0xb4,field(own,'cojvrReloadProbe'+name,desc));local(slot,name=='Hand')
        c.branch(0xa0 if name=='Hand' else 0xa6,failure)
    c.emit(0x2a);c.ref(0xb4,field(own,'cojvrReloadProbeStatus','I'));c.emit(0x06);c.branch(0xa0,failure)
    c.emit(0x2a);c.ref(0xb4,field(own,'cojvrReloadProbeCancelled','Z'));c.branch(0x99,failure)
    local(weapon);c.ref(0xb4,field('Weapon','cOwner','LPawnInventory;'));c.emit(0x2a);c.branch(0xa6,failure)
    local(machine);c.ref(0xb4,field('StateMashine','cojvrReloadProbeOwner','LArmedPlayerBeing;'))
    c.emit(0x2a);c.branch(0xa6,failure)
    current=field('StateMashine','m_sCurrentState','LBaseState;')
    local(machine);c.ref(0xb4,current);c.branch(0xc6,failure)
    local(machine);c.ref(0xb4,current);c.ref(0xc0,e.cls('PlayerState'))
    c.ref(0xb4,field('PlayerState','m_iStateID','I'));c.emit(0x10,22);c.branch(0xa0,failure)
    def slot(other=False):
        c.emit(0x2a);c.ref(0xb4,field(own,'m_aHandStateMashine','[LArmedPlayerBeing$HandStateMashine;'))
        if other:c.emit(0x04)
        local(hand,True)
        if other:c.emit(0x64)
        c.emit(0x32)
    slot();c.ref(0xb4,field('ArmedPlayerBeing$HandStateMashine','m_cStateMashine','LPlayerStateMashine;'))
    local(machine);c.branch(0xa6,failure)
    for other in (False,True):
        for state in ('m_cActualState','m_cDesiredState'):
            slot(other);c.ref(0xb4,field('ArmedPlayerBeing$HandStateMashine',state,'LArmedPlayerBeing$HandState;'))
            c.ref(0xb4,field('ArmedPlayerBeing$HandState','m_cWeapon','LWeapon;'))
            if other:c.branch(0xc7,failure)
            else:local(weapon);c.branch(0xa6,failure)
        c.emit(0x2a);c.ref(0xb4,field(own,'ActiveWeapon','[LWeapon;'))
        if other:c.emit(0x04)
        local(hand,True)
        if other:c.emit(0x64)
        c.emit(0x32)
        if other:c.branch(0xc7,failure)
        else:local(weapon);c.branch(0xa6,failure)


def build_indexed_session_lab(original_archive:bytes,weapon:str,*,cycle_animation=False)->tuple[bytes,bytes]:
    _profile(weapon)
    if hashlib.sha256(original_archive).hexdigest()!=ORIGINAL_ARCHIVE_SHA256:
        raise ValueError('Unknown original code.pak SHA-256; no indexed session')
    from coj_manual_reload import patch_manual_classes
    with zipfile.ZipFile(io.BytesIO(original_archive)) as archive:
        player,machine=patch_manual_classes(archive.read('ArmedPlayerBeing.class'),
            archive.read('StateMashine.class'),_offline_indexed=weapon)
    return _append_indexed_session(player,cycle_animation=cycle_animation),machine


class _IndexedBodyCycle:
    """Synchronous Clear fence; independent of mutable probe fields.

    The indexed debt lease separately blocks late inventory authority after a
    failed callback. Only this exact retained body/hand's synchronous Clear
    skips the whole finish handler, preserving desired21 during native restart.
    """
    def __init__(self,e):
        self.e=e
        self.fields={name:e.field('cojvrIndexedCycle'+name,desc) for name,desc in (
            ('SelfClear','Z'),('SawClear','Z'),('Machine','LPlayerStateMashine;'),
            ('Body','LBaseState;'),('Hand','I'),('Weapon','LWeapon;'),
            ('Hands','[LArmedPlayerBeing$HandStateMashine;'))}
        self.fence=e.ref('ArmedPlayerBeing','cojvrIndexedCycleFenceChangedBody','()V')
        c=Code();c.emit(0x2a);c.ref(0xb6,self.fence)
        self.read(c,'SelfClear');c.branch(0x99,'reject')
        c.emit(0x1b);self.read(c,'Hand');c.branch(0xa0,'reject')
        c.emit(0x1c,0x10,21);c.branch(0xa0,'reject')
        for name in ('Machine','Body'):self.read(c,name);c.branch(0xc6,'reject')
        self.read(c,'Machine')
        c.ref(0xb4,e.ref('StateMashine','m_sCurrentState','LBaseState;',9))
        self.read(c,'Body');c.branch(0xa6,'reject')
        c.emit(0x2a)
        c.ref(0xb4,e.ref('ArmedPlayerBeing','m_aHandStateMashine','[LArmedPlayerBeing$HandStateMashine;',9))
        self.read(c,'Hand');c.emit(0x32)
        c.ref(0xb4,e.ref('ArmedPlayerBeing$HandStateMashine','m_cStateMashine','LPlayerStateMashine;',9))
        self.read(c,'Machine');c.branch(0xa6,'reject')
        self.boolean(c,'SawClear',True);c.emit(0x04,0xac)
        c.label('reject');c.emit(0x03,0xac)
        guard=e.ref('ArmedPlayerBeing','cojvrIndexedCycleSuppressCompletion','(II)Z')
        e.helper('cojvrIndexedCycleSuppressCompletion','(II)Z',c.finish(),3)
        def completion(code):
            tag=Code();tag.emit(0x2a,0x1b,0x1c);tag.ref(0xb6,guard)
            tag.branch(0x99,'native');tag.emit(0xb1);tag.label('native')
            prefix=tag.finish();return prefix+b'\0'*(-len(prefix)%4)+code
        e.changes['OnHandStateFinished_Reload','(IIZ)V']=completion

        # Preserve both leases: the old body may still complete, while a
        # callback-installed current body must not acquire fresh ammo authority.
        # Reuse the unchanged exact-body suppression/retirement machinery.
        from coj_reload_cycle import _retained_completion_veto
        arm_cancel,_=_retained_completion_veto(e,'cojvrIndexedCycleCancelDebt')
        c=Code()
        for name,slot in (('Weapon',1),('Machine',2),('Body',3)):
            self.read(c,name);c.emit(0x3a,slot,0x19,slot);c.branch(0xc6,'done')
        self.read(c,'Hand');c.emit(0x36,4,0x15,4);c.branch(0x9b,'done')
        c.emit(0x15,4,0x04);c.branch(0xa3,'done')
        c.emit(0x2b);c.ref(0xb4,e.ref('Weapon','cOwner','LPawnInventory;',9))
        c.emit(0x2a);c.branch(0xa6,'done')
        # Probe clear legitimately removes its owner. Foreign ownership does
        # not authorize capture. No virtual lookup or unchecked cast runs in catch.
        c.emit(0x2c);c.ref(0xb4,e.ref('StateMashine','cojvrReloadProbeOwner','LArmedPlayerBeing;',9))
        c.emit(0x3a,5,0x19,5);c.branch(0xc6,'owner')
        c.emit(0x19,5,0x2a);c.branch(0xa6,'done');c.label('owner')
        hands=e.ref('ArmedPlayerBeing','m_aHandStateMashine','[LArmedPlayerBeing$HandStateMashine;',9)
        self.read(c,'Hands');c.branch(0xc6,'done')
        c.emit(0x2a);c.ref(0xb4,hands);self.read(c,'Hands');c.branch(0xa6,'done')
        # The retained native two-slot array was indexed by the entry proof
        # before any callbacks. Java arrays cannot shrink; replacements reject
        # before aaload, including null or short foreign arrays.
        self.read(c,'Hands');c.emit(0x15,4,0x32,0x3a,5,0x19,5);c.branch(0xc6,'done')
        c.emit(0x19,5);c.ref(0xb4,e.ref('ArmedPlayerBeing$HandStateMashine','m_cStateMashine','LPlayerStateMashine;',9))
        c.emit(0x2c);c.branch(0xa6,'done')
        for state in ('m_cActualState','m_cDesiredState'):
            c.emit(0x19,5);c.ref(0xb4,e.ref('ArmedPlayerBeing$HandStateMashine',state,'LArmedPlayerBeing$HandState;',9))
            c.emit(0x3a,6,0x19,6);c.branch(0xc6,'done')
            c.emit(0x19,6);c.ref(0xb4,e.ref('ArmedPlayerBeing$HandState','m_cWeapon','LWeapon;',9))
            c.emit(0x2b);c.branch(0xa6,'done')
        # Hand actual/desired plus native machine identity are sufficient here;
        # this failure lease only withholds ammo, never mutates native state.
        current=e.ref('StateMashine','m_sCurrentState','LBaseState;',9)
        c.emit(0x2c);c.ref(0xb4,current);c.emit(0x3a,3,0x2d);c.branch(0xc6,'done')
        c.emit(0x2d);self.read(c,'Body');c.branch(0xa5,'done')
        c.emit(0x2d);c.ref(0xc1,e.cls('PlayerState'));c.branch(0x99,'done')
        c.emit(0x2d);c.ref(0xc0,e.cls('PlayerState'))
        c.ref(0xb4,e.ref('PlayerState','m_iStateID','I',9))
        c.emit(0x10,21);c.branch(0xa0,'done')
        # Capture the exact object, not every future body21. Suppress() also
        # requires the callback's hand/state21 and the retained native machine.
        arm_cancel(c,1,4,2)
        c.emit(0x2a,0x04);c.ref(0xb5,e.ref('ArmedPlayerBeing','cojvrIndexedFaulted','Z',9))
        c.label('done');c.emit(0xb1)
        e.helper('cojvrIndexedCycleFenceChangedBody','()V',c.finish(),7)

        c=Code()
        for name in ('Machine','Body'):self.read(c,name);c.branch(0xc6,'reject')
        self.read(c,'Machine');c.ref(0xb4,current);self.read(c,'Body');c.branch(0xa6,'reject')
        c.emit(0x2a);c.ref(0xb4,e.ref('ArmedPlayerBeing','cojvrIndexedFaulted','Z',9));c.branch(0x9a,'reject')
        c.emit(0x04,0xac);c.label('reject');c.emit(0x03,0xac)
        self.matches=e.ref('ArmedPlayerBeing','cojvrIndexedCycleBodyMatches','()Z')
        e.helper('cojvrIndexedCycleBodyMatches','()Z',c.finish(),1)
        # OwnerMatches ends after its virtual readers and field-only wait
        # proof. Gate its success too, so inner indexed writers cannot operate
        # on a different body installed by a parameter/owner callback.
        def owner_match(code):
            if code[-4:]!=b'\x04\xac\x03\xac':
                raise ValueError('Unknown indexed owner success seam')
            tag=Code();tag.emit(0x2a);tag.ref(0xb6,self.matches)
            tag.branch(0x9a,'native');tag.emit(0x03,0xac);tag.label('native')
            pool=parse_constant_pool(Reader(u2(e.next)+e.data[10:e.cp_end]+e.extra))
            return insert_target_tag(code,len(code)-4,tag.finish(),pool,include_target=True)
        e.changes['cojvrIndexedOwnerMatches','(LWeapon;ILPlayerStateMashine;I)Z']=owner_match

    def read(self,c,name):c.emit(0x2a);c.ref(0xb4,self.fields[name])

    def boolean(self,c,name,value):
        c.emit(0x2a,0x04 if value else 0x03);c.ref(0xb5,self.fields[name])

    def retain(self,c,weapon,hand,machine):
        for name,slot in (('Weapon',weapon),('Hand',hand),('Machine',machine)):
            c.emit(0x2a,0x15 if name=='Hand' else 0x19,slot);c.ref(0xb5,self.fields[name])
        c.emit(0x2a,0x19,machine)
        c.ref(0xb4,self.e.ref('StateMashine','m_sCurrentState','LBaseState;',9))
        c.ref(0xb5,self.fields['Body'])
        c.emit(0x2a,0x2a)
        c.ref(0xb4,self.e.ref('ArmedPlayerBeing','m_aHandStateMashine','[LArmedPlayerBeing$HandStateMashine;',9))
        c.ref(0xb5,self.fields['Hands'])

    def same_body(self,c,failure):
        c.emit(0x2a);c.ref(0xb6,self.matches);c.branch(0x99,failure)

    def replay(self,c,machine,failure):
        self.same_body(c,failure)
        self.boolean(c,'SawClear',False);self.boolean(c,'SelfClear',True)
        c.emit(0x19,machine);self.read(c,'Body')
        c.ref(0xb6,self.e.ref('StateMashine','SetCurrentState','(LBaseState;)V'))
        self.same_body(c,failure)
        self.read(c,'SawClear');c.branch(0x99,failure)
        self.boolean(c,'SelfClear',False)

    def cleanup(self,c):
        c.emit(0x2a);c.ref(0xb6,self.fence)
        self.boolean(c,'SelfClear',False);self.boolean(c,'SawClear',False)
        for name in ('Machine','Body','Weapon','Hands'):
            c.emit(0x2a,0x01);c.ref(0xb5,self.fields[name])


def _append_indexed_session(player:bytes,*,helper_name='cojvrLabInsertWaitingIndexedUnit',runtime=False,
                            cycle_animation=False)->bytes:
    """Append all exact indexed transactions to one already-emitted probe.

    Admission/validity is owned by the caller. Public lab builders retain their
    original hash checks and exact-family validity; runtime supplies a mixed
    exact-class validator and adds the foreign-family entry guards afterwards.
    """
    if cycle_animation and runtime and helper_name!='cojvrInsertManualIndexedRound':
        raise ValueError('Indexed body cycle requires the canonical runtime appendix')
    declarations=ClassEdit(player)
    if cycle_animation and runtime:
        # The canonical caller has already emitted the native unit cycle and
        # declared its shared owner latches. It adds all family entry guards
        # after appending this endpoint; a standalone lab cannot opt into that
        # composition merely by selecting the public helper name.
        required_fields={('cojvrManualFamiliesActive','Z'),('cojvrManualFamiliesFaulted','Z'),
                         ('cojvrManualCycleActive','Z'),('cojvrManualCycleFaulted','Z')}
        methods={(m.name,m.descriptor) for m in parse_class(player)[1]}
        if (not required_fields.issubset(declarations.declared_fields) or
                ('cojvrInsertManualNativeCycleRound','()Z') not in methods):
            raise ValueError('Indexed body cycle requires the canonical runtime owner guards')
    declarations.field('cojvrIndexedSessionBusy','Z')
    declarations.field('cojvrIndexedSessionDispatch','Z')
    player=_append_indexed_helpers(declarations.finish(),_supervised=True)
    e=ClassEdit(player)
    cycle=_IndexedBodyCycle(e) if cycle_animation else None
    def ref(name,desc,tag=10):return e.ref('ArmedPlayerBeing',name,desc,tag)
    busy=ref('cojvrIndexedSessionBusy','Z',9)
    dispatch=ref('cojvrIndexedSessionDispatch','Z',9)
    fault=ref('cojvrIndexedFaulted','Z',9)
    def read(c,field):c.emit(0x2a);c.ref(0xb4,field)
    def put(c,field,value):c.emit(0x2a,0x04 if value else 0x03);c.ref(0xb5,field)
    def probe(c,name,desc):read(c,ref('cojvrReloadProbe'+name,desc,9))
    def cleanup(c):
        if cycle:cycle.cleanup(c)
        put(c,dispatch,False);put(c,busy,False)
    # Close never re-reads a mutable probe target after callbacks. Resolve the
    # native end state, revalidate, then use the retained machine exclusively.
    c=Code()
    c.emit(0x2a,0x2b,0x1c,0x2d,0x03)
    c.ref(0xb6,ref('cojvrIndexedOwnerMatches','(LWeapon;ILPlayerStateMashine;I)Z'));c.branch(0x99,'reject')
    c.emit(0x2d,0x10,22);c.ref(0xb6,e.ref('PlayerStateMashine','FindStateByID','(I)LPlayerState;'));c.emit(0x3a,4)
    c.emit(0x19,4);c.branch(0xc6,'reject')
    c.emit(0x2a,0x2b);c.ref(0xb6,ref('GetWeaponMoveState','(LWeapon;)I'));c.emit(0x36,5)
    _emit_retained_wait(e,c,1,2,3,'reject')
    if cycle:cycle.same_body(c,'reject')
    c.emit(0x2a,0x1c,0x15,5);c.ref(0xb6,ref('SetDesiredWeaponState','(II)V'))
    _emit_retained_wait(e,c,1,2,3,'reject',desired=False,move_local=5)
    if cycle:cycle.same_body(c,'reject')
    for name,value in (('Cancelled',1),('Status',3)):
        c.emit(0x2a,value+3);c.ref(0xb5,ref('cojvrReloadProbe'+name,'Z' if name=='Cancelled' else 'I',9))
    c.emit(0x2d,0x19,4);c.ref(0xb6,e.ref('StateMashine','SetCurrentState','(LBaseState;)V'))
    c.emit(0x04,0xac);c.label('reject');c.emit(0x03,0xac)
    e.helper('cojvrCloseIndexedSession','(LWeapon;ILPlayerStateMashine;)Z',c.finish(),6)

    c=Code()
    c.emit(0x1b);c.branch(0x9b,'reject');c.emit(0x1b,0x04);c.branch(0xa3,'reject')
    for field in (busy,fault):read(c,field);c.branch(0x9a,'reject')
    put(c,busy,True);c.label('try')
    probe(c,'Status','I');c.emit(0x05);c.branch(0xa0,'not_ready')
    probe(c,'Weapon','LWeapon;');c.emit(0x4d)
    probe(c,'Hand','I');c.emit(0x3e)
    probe(c,'Machine','LPlayerStateMashine;');c.emit(0x3a,4)
    c.emit(0x2c);c.branch(0xc6,'not_ready')
    c.emit(0x19,4);c.branch(0xc6,'not_ready')
    c.emit(0x1d);c.branch(0x9b,'not_ready');c.emit(0x1d,0x04);c.branch(0xa3,'not_ready')
    _emit_retained_wait(e,c,2,3,4,'not_ready')
    # The outer session owns callbacks before and after the inner writer too.
    # Keep the lease through remaining-query/close exceptions and probe loss.
    for name,slot,desc in (('Weapon',2,'LWeapon;'),('Hand',3,'I'),
                           ('Machine',4,'LPlayerStateMashine;')):
        c.emit(0x2a,0x15 if name=='Hand' else 0x19,slot)
        c.ref(0xb5,ref('cojvrIndexedDebt'+name,desc,9))
    c.emit(0x2a,0x19,4)
    c.ref(0xb4,e.ref('StateMashine','m_sCurrentState','LBaseState;',9))
    c.ref(0xb5,ref('cojvrIndexedDebtBody','LBaseState;',9))
    if cycle:cycle.retain(c,2,3,4)
    c.emit(0x2a);c.ref(0xb6,ref('cojvrManualReloadValid','()Z'));c.branch(0x99,'not_ready')
    if cycle:cycle.same_body(c,'fault')
    if runtime:
        # Mixed production validity also admits one-index families. Reject them
        # before the outer session reads either indexed slot or counter vector.
        for name,_,_,_,_ in PROFILES:
            c.emit(0x2c);c.ref(0xb6,e.ref('java/lang/Object','getClass','()Ljava/lang/Class;'))
            c.ref(0xb6,e.ref('java/lang/Class','getName','()Ljava/lang/String;'))
            literal=e.append(b'\x08'+e.utf8(name).to_bytes(2,'big'))
            c.ref(0x13,literal);c.ref(0xb6,e.ref('java/lang/String','equals','(Ljava/lang/Object;)Z'))
            c.branch(0x9a,'indexed_class')
        c.branch(0xa7,'not_ready');c.label('indexed_class')
    put(c,dispatch,True)
    c.emit(0x2a,0x2c,0x1d,0x1b)
    c.ref(0xb6,ref('cojvrLabInsertIndexedUnit','(LWeapon;II)Z'));c.emit(0x36,7)
    read(c,fault);c.branch(0x9a,'not_ready')

    # Keep the complete committed vector and original counter objects through
    # GetWeaponMaxAmmoToReload and the original 21->22 closing callbacks.
    # Locals 8/9: loaded; 10..12: reserves; 13..15: counters; 16..18: maxima.
    # This also covers a no-transfer close when the weapon/reserve is exhausted.
    counters=('Rifle','Shotgun','Pistol')
    for index in range(2):
        c.emit(0x2c,index+3)
        c.ref(0xb6,e.ref('Weapon','GetAmmoCount','(I)I'))
        c.emit(0x36,8+index)
    for pool,suffix in enumerate(counters):
        c.emit(0x2a,pool+3)
        c.ref(0xb6,e.ref('PawnArmed','FindAmmoCounter','(I)LInventoryAmmoCounter;'))
        c.emit(0x3a,13+pool)
        c.emit(0x19,13+pool);c.branch(0xc6,'fault')
        c.emit(0x19,13+pool)
        c.ref(0xb4,e.ref('InventoryAmmoCounter','cOwner','LPawnInventory;',9))
        c.emit(0x2a);c.branch(0xa6,'fault')
        c.emit(0x19,13+pool)
        c.ref(0xb6,e.ref('InventoryAmmoCounter','GetAmount','()I'))
        c.emit(0x36,10+pool)
        c.emit(0x2a);c.ref(0xb4,e.ref('PawnInventory','nAmmo'+suffix,'I',9))
        c.emit(0x15,10+pool);c.branch(0xa0,'fault')
        c.emit(0x19,13+pool)
        c.ref(0xb4,e.ref('InventoryAmmoCounter','nMaxAmount','I',9))
        c.emit(0x36,16+pool)

    verification=0
    def same_vector(failure):
        nonlocal verification
        verification+=1
        suffix=str(verification)
        # Original getters own the index-specific double-ammo storage. Keep
        # original counter identity, typed backing fields and owner in sync.
        for index in range(2):
            c.emit(0x2c,index+3)
            c.ref(0xb6,e.ref('Weapon','GetAmmoCount','(I)I'))
            c.emit(0x15,8+index);c.branch(0xa0,failure)
        for pool,kind in enumerate(counters):
            c.emit(0x2a,pool+3)
            c.ref(0xb6,e.ref('PawnArmed','FindAmmoCounter','(I)LInventoryAmmoCounter;'))
            c.emit(0x19,13+pool);c.branch(0xa6,failure)
            c.emit(0x19,13+pool)
            c.ref(0xb4,e.ref('InventoryAmmoCounter','cOwner','LPawnInventory;',9))
            c.emit(0x2a);c.branch(0xa6,failure)
            c.emit(0x19,13+pool)
            c.ref(0xb6,e.ref('InventoryAmmoCounter','GetAmount','()I'))
            c.emit(0x15,10+pool);c.branch(0xa0,failure)
            c.emit(0x2a);c.ref(0xb4,e.ref('PawnInventory','nAmmo'+kind,'I',9))
            c.emit(0x15,10+pool);c.branch(0xa0,failure)
        # A second count pass comes after every inventory callback.
        for index in range(2):
            c.emit(0x2c,index+3)
            c.ref(0xb6,e.ref('Weapon','GetAmmoCount','(I)I'))
            c.emit(0x15,8+index);c.branch(0xa0,failure)
        # Independent backing-field reads are last: a virtual getter may have
        # modified an unrelated count after producing its return value.
        c.emit(0x2c);c.ref(0xc0,e.cls('WeaponFireAmmo'))
        c.ref(0xb4,e.ref('WeaponFireAmmo','m_iAmmo0Count','I',9))
        c.emit(0x15,8);c.branch(0xa0,failure)
        c.emit(0x2c);c.ref(0xc1,e.cls('WeaponRifleDoubleAmmo'))
        c.branch(0x99,'pistol_backing_'+suffix)
        c.emit(0x2c);c.ref(0xc0,e.cls('WeaponRifleDoubleAmmo'))
        c.ref(0xb4,e.ref('WeaponRifleDoubleAmmo','m_iAmmo1Count','I',9))
        c.branch(0xa7,'second_backing_'+suffix)
        c.label('pistol_backing_'+suffix)
        c.emit(0x2c);c.ref(0xc0,e.cls('WeaponPistolDoubleAmmo'))
        c.ref(0xb4,e.ref('WeaponPistolDoubleAmmo','m_iAmmo1Count','I',9))
        c.label('second_backing_'+suffix)
        c.emit(0x15,9);c.branch(0xa0,failure)
        for pool,kind in enumerate(counters):
            c.emit(0x2a);c.ref(0xb4,e.ref('PawnInventory','nAmmo'+kind,'I',9))
            c.emit(0x15,10+pool);c.branch(0xa0,failure)
            c.emit(0x19,13+pool)
            c.ref(0xb4,e.ref('InventoryAmmoCounter','nMaxAmount','I',9))
            c.emit(0x15,16+pool);c.branch(0xa0,failure)
        c.emit(0x2c)
        c.ref(0xb4,e.ref('Weapon','cOwner','LPawnInventory;',9))
        c.emit(0x2a);c.branch(0xa6,failure)

    def closed_ticket(failure):
        for name,desc,slot in (('Weapon','LWeapon;',2),('Machine','LPlayerStateMashine;',4),
                               ('Hand','I',3)):
            probe(c,name,desc)
            c.emit(0x15 if name=='Hand' else 0x19,slot)
            c.branch(0xa0 if name=='Hand' else 0xa6,failure)
        probe(c,'Status','I');c.emit(0x06);c.branch(0xa0,failure)
        probe(c,'Cancelled','Z');c.branch(0x99,failure)
        c.emit(0x19,4)
        c.ref(0xb4,e.ref('StateMashine','cojvrReloadProbeOwner','LArmedPlayerBeing;',9))
        c.emit(0x2a);c.branch(0xa6,failure)
        c.emit(0x19,4)
        c.ref(0xb4,e.ref('StateMashine','m_sCurrentState','LBaseState;',9))
        c.branch(0xc6,failure)
        c.emit(0x19,4)
        c.ref(0xb4,e.ref('StateMashine','m_sCurrentState','LBaseState;',9))
        c.ref(0xc0,e.cls('PlayerState'))
        c.ref(0xb4,e.ref('PlayerState','m_iStateID','I',9))
        c.emit(0x10,22);c.branch(0xa0,failure)
        c.emit(0x2a);c.ref(0xb4,e.ref('ArmedPlayerBeing',
            'm_aHandStateMashine','[LArmedPlayerBeing$HandStateMashine;',9))
        c.emit(0x1d,0x32)
        c.ref(0xb4,e.ref('ArmedPlayerBeing$HandStateMashine',
            'm_cStateMashine','LPlayerStateMashine;',9))
        c.emit(0x19,4);c.branch(0xa6,failure)
        for state in ('m_cActualState','m_cDesiredState'):
            c.emit(0x2a);c.ref(0xb4,e.ref('ArmedPlayerBeing',
                'm_aHandStateMashine','[LArmedPlayerBeing$HandStateMashine;',9))
            c.emit(0x1d,0x32)
            c.ref(0xb4,e.ref('ArmedPlayerBeing$HandStateMashine',state,
                             'LArmedPlayerBeing$HandState;',9))
            c.ref(0xb4,e.ref('ArmedPlayerBeing$HandState','m_cWeapon','LWeapon;',9))
            c.emit(0x2c);c.branch(0xa6,failure)
        c.emit(0x2a);c.ref(0xb4,e.ref('ArmedPlayerBeing','ActiveWeapon','[LWeapon;',9))
        c.emit(0x1d,0x32,0x2c);c.branch(0xa6,failure)
        read(c,fault);c.branch(0x9a,failure)

    c.emit(0x2a,0x1d);c.ref(0xb6,ref('GetWeaponMaxAmmoToReload','(I)I'));c.emit(0x36,5)
    c.emit(0x2a,0x2c,0x1d,0x19,4,0x03)
    c.ref(0xb6,ref('cojvrIndexedOwnerMatches','(LWeapon;ILPlayerStateMashine;I)Z'));c.branch(0x99,'fault')
    same_vector('fault')
    _emit_retained_wait(e,c,2,3,4,'fault')
    c.emit(0x15,5);c.branch(0x9d,'replay' if cycle else 'accepted')
    c.emit(0x2a,0x2c,0x1d,0x19,4)
    c.ref(0xb6,ref('cojvrCloseIndexedSession','(LWeapon;ILPlayerStateMashine;)Z'));c.branch(0x99,'fault')
    closed_ticket('fault')
    same_vector('fault')
    closed_ticket('fault')
    if cycle:
        c.branch(0xa7,'accepted');c.label('replay')
        # No successful transfer means no animation credit. A rejected/full
        # selected slot cannot restart the body merely because another fits.
        c.emit(0x15,7);c.branch(0x99,'accepted')
        cycle.replay(c,4,'fault')
        c.emit(0x2a,0x2c,0x1d,0x19,4,0x03)
        c.ref(0xb6,ref('cojvrIndexedOwnerMatches','(LWeapon;ILPlayerStateMashine;I)Z'))
        c.branch(0x99,'fault')
        for index in range(len(PROFILES)):
            c.emit(0x2a,0x2c,index+3)
            c.ref(0xb6,ref('cojvrIndexedProfileMatches','(LWeapon;I)Z'))
            c.branch(0x9a,'replayed_profile')
        c.branch(0xa7,'fault');c.label('replayed_profile')
        same_vector('fault')
        _emit_retained_wait(e,c,2,3,4,'fault')
        cycle.same_body(c,'fault')
        # These are probe readiness fields, never native animation/frame clocks.
        # The next naturally scheduled pre-Clear veto must establish wait2.
        c.emit(0x2a,0x04);c.ref(0xb5,ref('cojvrReloadProbeStatus','I',9))
        for name in ('Since','Until'):
            c.emit(0x2a,0x0b);c.ref(0xb5,ref('cojvrReloadProbe'+name,'F',9))
    c.label('accepted');c.label('try_end')
    if not cycle:
        for name, desc in (('Weapon','LWeapon;'),('Machine','LPlayerStateMashine;'),('Body','LBaseState;')):
            c.emit(0x2a,0x01);c.ref(0xb5,ref('cojvrIndexedDebt'+name,desc,9))
    cleanup(c);c.emit(0x15,7,0xac)
    c.label('fault');put(c,fault,True)
    c.label('not_ready');cleanup(c);c.emit(0x03,0xac)
    c.label('catch');c.emit(0x3a,6);put(c,fault,True);cleanup(c);c.emit(0x19,6,0xbf)
    c.label('reject');c.emit(0x03,0xac)
    e.helper(helper_name,'(I)Z',c.finish(),19,
        exception_table=((c.labels['try'],c.labels['try_end'],c.labels['catch'],0),))
    def heartbeat(code):
        c=Code()
        for field in (busy,fault):read(c,field);c.branch(0x9a,'reject')
        c.branch(0xa7,'native');c.label('reject');c.emit(0x03,0xac);c.label('native')
        prefix=c.finish();return prefix+b'\0'*(-len(prefix)%4)+code
    e.changes['cojvrKeepManualReloadAlive','()Z']=heartbeat
    return e.finish()
