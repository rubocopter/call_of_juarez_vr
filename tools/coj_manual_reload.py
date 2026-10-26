"""Exact opt-in persistent session on the demonstrated zero-transfer veto.

Native callbacks still own opening/closing. Only an immediate admitted insertion
calls the shipped ammunition authority, once; there is no credit/round cache.
"""
from coj_reload_wait_probe import ClassEdit, patch_probe_classes
from patch_coj_weapon_consumers import Code

# Concrete one-index/non-whole-clip native body-cycle contracts. Other families
# require their own accounting and cannot enter via inheritance or name prefix.
NATIVE_CYCLE_PROFILES={'WeaponRifleWinchester':12,'WeaponRifleWinchesterScope':12,
                       'WeaponPistolVolcanic':8}

def runtime_reload_policy():
    from patch_coj_weapon_consumers import SINGLE_ROUND_REVOLVERS
    from coj_indexed_reload import PROFILES
    from coj_clip_reload import CLIP_WEAPONS
    return {**{name:(False,1) for name in SINGLE_ROUND_REVOLVERS+tuple(NATIVE_CYCLE_PROFILES)},
            **{name:(whole,2) for name,_,_,whole,_ in PROFILES},
            **{name:(True,1) for name in CLIP_WEAPONS}}

def _runtime_entry_guard(e,c):
    for field in ('cojvrManualFamiliesActive','cojvrManualFamiliesFaulted',
                  'cojvrManualCycleActive','cojvrManualCycleFaulted',
                  'cojvrIndexedBusy','cojvrIndexedFaulted','cojvrIndexedSessionBusy',
                  'cojvrClipBusy','cojvrClipFaulted'):
        c.emit(0x2a);c.ref(0xb4,e.ref('ArmedPlayerBeing',field,'Z',9));c.branch(0x9a,'reject')

def _exact_class_branch(e,c,read_weapon,names,destination):
    for weapon in names:
        read_weapon(c);c.ref(0xb6,e.ref('java/lang/Object','getClass','()Ljava/lang/Class;'))
        c.ref(0xb6,e.ref('java/lang/Class','getName','()Ljava/lang/String;'))
        name=e.append(b'\x08'+e.utf8(weapon).to_bytes(2,'big'))
        c.ref(0x13,name);c.ref(0xb6,e.ref('java/lang/String','equals','(Ljava/lang/Object;)Z'));c.branch(0x9a,destination)


def patch_manual_classes(player, machine, *, _offline_weapon=None, _offline_cycle=False,
                         _offline_indexed=None, _offline_clip=None):
    if _offline_cycle and _offline_weapon is None:
        raise ValueError('Body-cycle experiment requires an exact offline weapon profile')
    runtime=all(x is None for x in (_offline_weapon,_offline_indexed,_offline_clip))
    from patch_coj_weapon_consumers import SINGLE_ROUND_REVOLVERS
    player, machine = patch_probe_classes(player, machine, _offline_weapon=_offline_weapon,
                                          _offline_indexed=_offline_indexed, _offline_clip=_offline_clip,
                                          _runtime_weapons=tuple(runtime_reload_policy()) if runtime else None)
    e = ClassEdit(player)
    def field(name, desc): return e.ref('ArmedPlayerBeing', 'cojvrReloadProbe'+name, desc, 9)
    def method(name, desc): return e.ref('ArmedPlayerBeing', name, desc)
    def read(c, name, desc): c.emit(0x2a); c.ref(0xb4, field(name, desc))
    def hand_call(c, name, desc):
        c.emit(0x2a); read(c, 'Hand', 'I'); c.ref(0xb6, method(name, desc))
    def reject(c): c.label('reject'); c.emit(0x03, 0xac)
    clock=e.ref('PlayerStateMashine','m_fCurrentTime','F',9)

    # Validate the cached native admission and the actual current machine afresh.
    # Opening includes the request's idle->begin scheduling window. Waiting must
    # be body 21. No ordinary, dual, carried or replaced owner can receive rounds.
    c=Code(); read(c,'Weapon','LWeapon;'); c.branch(0xc6,'reject')
    read(c,'Cancelled','Z'); c.branch(0x9a,'reject')
    read(c,'Status','I'); c.emit(0x04); c.branch(0x9f,'opening')
    read(c,'Status','I'); c.emit(0x05); c.branch(0xa0,'reject')
    hand_call(c,'GetHandStateMashineState','(I)I'); c.emit(0x10,21); c.branch(0xa0,'reject')
    c.branch(0xa7,'owner')
    c.label('opening'); hand_call(c,'GetHandStateMashineState','(I)I'); c.emit(0x3c)
    for n in (1,20,21): c.emit(0x1b,0x10,n); c.branch(0x9f,'owner')
    c.branch(0xa7,'reject')
    c.label('owner'); read(c,'Machine','LPlayerStateMashine;')
    hand_call(c,'GetHandStateMashine','(I)LPlayerStateMashine;'); c.branch(0xa6,'reject')
    read(c,'Machine','LPlayerStateMashine;'); c.branch(0xc6,'reject')
    read(c,'Machine','LPlayerStateMashine;')
    c.ref(0xb4,e.ref('StateMashine','cojvrReloadProbeOwner','LArmedPlayerBeing;',9))
    c.emit(0x2a); c.branch(0xa6,'reject')
    read(c,'Machine','LPlayerStateMashine;'); c.ref(0xb4,clock); c.emit(0x38,2)
    c.emit(0x17,2,0x17,2,0x66,0x0b,0x95); c.branch(0x9a,'reject')
    read(c,'Status','I'); c.emit(0x05); c.branch(0xa0,'safety')
    c.emit(0x17,2); read(c,'Since','F'); c.emit(0x95); c.branch(0x9b,'reject')
    c.emit(0x17,2); read(c,'Until','F'); c.emit(0x96); c.branch(0x9c,'reject')
    c.label('safety')
    c.emit(0x2a); c.ref(0xb6,method('IsNotAlive','()Z')); c.branch(0x9a,'reject')
    c.emit(0x2a); c.ref(0xb6,method('IsCarrying','()Z')); c.branch(0x9a,'reject')
    c.emit(0x2a); c.ref(0xb4,e.ref('ArmedPlayerBeing','m_bNetAttackForced','Z',9)); c.branch(0x9a,'reject')
    c.emit(0x2a); c.ref(0xb6,method('GetNetIsOwner','()Z')); c.branch(0x99,'reject')
    c.ref(0xb8,method('IsNetGame','()Z')); c.branch(0x9a,'reject')
    hand_call(c,'GetDesiredWeaponState','(I)I'); c.emit(0x10,21); c.branch(0xa0,'reject')
    for name in ('GetActualWeapon','GetActiveWeapon','GetDesiredWeapon'):
        hand_call(c,name,'(I)LWeapon;'); read(c,'Weapon','LWeapon;'); c.branch(0xa6,'reject')
        c.emit(0x2a,0x2a); read(c,'Hand','I'); c.ref(0xb6,method('GetOtherHand','(I)I'))
        c.ref(0xb6,method(name,'(I)LWeapon;')); c.branch(0xc7,'reject')
    whole_clip = False
    if _offline_indexed is not None:
        from coj_indexed_reload_session import _profile
        whole_clip = _profile(_offline_indexed)[3]
    if _offline_clip is not None:
        from coj_clip_reload import CLIP_WEAPONS
        if _offline_clip not in CLIP_WEAPONS:
            raise ValueError('No exact offline clip profile')
        whole_clip = True
    if runtime:
        for index,(name,(clip,count)) in enumerate(runtime_reload_policy().items()):
            _exact_class_branch(e,c,lambda c:read(c,'Weapon','LWeapon;'),(name,),'profile_'+str(index))
            c.branch(0xa7,'next_profile_'+str(index));c.label('profile_'+str(index))
            read(c,'Weapon','LWeapon;');c.ref(0xb6,e.ref('Weapon','IsReloadWholeClipAtOnce','()Z'));c.branch(0x99 if clip else 0x9a,'reject')
            read(c,'Weapon','LWeapon;');c.ref(0xb6,e.ref('Weapon','GetAmmoTypeCount','()I'));c.emit(3+count);c.branch(0xa0,'reject')
            c.branch(0xa7,'profile_valid');c.label('next_profile_'+str(index))
        c.branch(0xa7,'reject');c.label('profile_valid')
    else:
        read(c,'Weapon','LWeapon;'); c.ref(0xb6,e.ref('Weapon','IsReloadWholeClipAtOnce','()Z')); c.branch(0x99 if whole_clip else 0x9a,'reject')
        read(c,'Weapon','LWeapon;'); c.ref(0xb6,e.ref('Weapon','GetAmmoTypeCount','()I')); c.emit(0x05 if _offline_indexed else 0x04); c.branch(0xa0,'reject')
    if runtime:
        # This final proof uses fields only, after any virtual parameter reads.
        # Cached hand/ticket equality alone cannot establish native inventory
        # ownership, including for the inherited revolver insertion route.
        read(c,'Weapon','LWeapon;');c.ref(0xb4,e.ref('Weapon','cOwner','LPawnInventory;',9))
        c.emit(0x2a);c.branch(0xa6,'reject')
    c.emit(0x04,0xac); reject(c)
    e.helper('cojvrManualReloadValid','()Z',c.finish(),3)

    # Watchdog remains native: missed heartbeat closes after the proven interval.
    # Float subtraction rejects both infinity and NaN before extending a deadline.
    import struct
    wait=e.append(b'\x04'+struct.pack('>f',1.5))
    c=Code()
    if runtime:_runtime_entry_guard(e,c)
    c.emit(0x2a); c.ref(0xb6,method('cojvrManualReloadValid','()Z')); c.branch(0x99,'reject')
    read(c,'Machine','LPlayerStateMashine;'); c.ref(0xb4,clock); c.emit(0x38,1)
    c.emit(0x17,1,0x17,1,0x66,0x0b,0x95); c.branch(0x9a,'reject')
    read(c,'Status','I'); c.emit(0x05); c.branch(0xa0,'accepted')
    c.emit(0x2a,0x17,1); c.ref(0x13,wait); c.emit(0x62); c.ref(0xb5,field('Until','F'))
    c.label('accepted'); c.emit(0x04,0xac); reject(c)
    e.helper('cojvrKeepManualReloadAlive','()Z',c.finish(),2)

    # PawnArmed.WeaponReload(LWeapon;)Z caps inventory/capacity and takes one
    # round for these single-ammo non-whole-clip weapons. Result and event are
    # exactly the native authority; close is still the zero-transfer callback.
    c=Code()
    if runtime:
        permit=e.ref('ArmedPlayerBeing','cojvrManualFamiliesDispatch','Z',9)
        c.emit(0x2a);c.ref(0xb4,permit);c.branch(0x99,'reject')
        c.emit(0x2a,0x03);c.ref(0xb5,permit)
    c.emit(0x2a); c.ref(0xb6,method('cojvrManualReloadValid','()Z')); c.branch(0x99,'reject')
    read(c,'Status','I'); c.emit(0x05); c.branch(0xa0,'reject')
    if runtime:
        _exact_class_branch(e,c,lambda c:read(c,'Weapon','LWeapon;'),SINGLE_ROUND_REVOLVERS,'revolver')
        c.branch(0xa7,'reject');c.label('revolver')
    c.emit(0x2a); read(c,'Weapon','LWeapon;'); c.ref(0xb6,method('WeaponReload','(LWeapon;)Z')); c.emit(0x3c)
    c.emit(0x1b); c.branch(0x99,'capacity')
    singleton=e.ref('GameMode','cSingleton','LGameMode;',9)
    c.ref(0xb2,singleton); c.branch(0xc6,'capacity')
    c.ref(0xb2,singleton); c.ref(0xb2,e.ref('EnumGameEvent','_WEAPON_RELOADED','I',9))
    c.ref(0xb6,e.ref('GameMode','OnGameEvent','(I)V'))
    c.label('capacity'); hand_call(c,'GetWeaponMaxAmmoToReload','(I)I'); c.branch(0x9d,'result')
    c.emit(0x2a); c.ref(0xb6,method('cojvrCancelReloadProbe','()V'))
    c.label('result'); c.emit(0x1b,0xac); reject(c)
    if not _offline_cycle and _offline_indexed is None and _offline_clip is None:
        e.helper('cojvrInsertManualRevolverRoundImpl' if runtime else 'cojvrInsertManualRound','()Z',c.finish(),2)
    player=e.finish()
    if runtime:
        from coj_reload_cycle import _add_offline_cycle
        player=_add_offline_cycle(player,tuple(NATIVE_CYCLE_PROFILES),
                                  helper_name='cojvrInsertManualNativeCycleRound')
        from coj_manual_families import append_runtime_families
        player=append_runtime_families(player)
        e=ClassEdit(player)
        from coj_reload_cycle import _retained_completion_veto
        from coj_indexed_reload_session import _emit_retained_wait
        arm_wait,_ = _retained_completion_veto(e,'cojvrHeartbeatDebt')
        c=Code();_runtime_entry_guard(e,c)
        for name,slot,desc in (('Weapon',1,'LWeapon;'),('Hand',2,'I'),('Machine',3,'LPlayerStateMashine;')):
            c.emit(0x2a);c.ref(0xb4,e.ref('ArmedPlayerBeing','cojvrReloadProbe'+name,desc,9))
            c.emit(0x36 if name=='Hand' else 0x3a,slot)
        for slot in (1,3):c.emit(0x19,slot);c.branch(0xc6,'reject')
        c.emit(0x1c);c.branch(0x9b,'reject');c.emit(0x1c,0x04);c.branch(0xa3,'reject')
        _emit_retained_wait(e,c,1,2,3,'reject')
        arm_wait(c,1,2,3)
        c.label('reject');c.emit(0xb1)
        e.helper('cojvrFenceManualWaitBody','()V',c.finish(),4)
        # The first heartbeat can precede every insertion. Its field-only fence
        # must run before the existing virtual validator and guard its pending
        # native completion even if that validator loses the probe and throws.
        def fence_heartbeat(code):
            c=Code();c.emit(0x2a)
            c.ref(0xb6,e.ref('ArmedPlayerBeing','cojvrFenceManualWaitBody','()V'))
            prefix=c.finish()
            return prefix+b'\0'*(-len(prefix)%4)+code
        e.changes[('cojvrKeepManualReloadAlive','()Z')]=fence_heartbeat
        arm_completion,_ = _retained_completion_veto(e,'cojvrRevolverDebt')
        permit=e.field('cojvrManualFamiliesDispatch','Z')
        active=e.ref('ArmedPlayerBeing','cojvrManualFamiliesActive','Z',9)
        fault=e.ref('ArmedPlayerBeing','cojvrManualFamiliesFaulted','Z',9)
        c=Code();_runtime_entry_guard(e,c)
        for field in (active,permit):c.emit(0x2a,0x04);c.ref(0xb5,field)
        c.label('try')
        for name,slot,desc in (('Weapon',3,'LWeapon;'),('Hand',4,'I'),('Machine',5,'LPlayerStateMashine;')):
            c.emit(0x2a);c.ref(0xb4,e.ref('ArmedPlayerBeing','cojvrReloadProbe'+name,desc,9))
            c.emit(0x36 if name=='Hand' else 0x3a,slot)
        for slot in (3,5):c.emit(0x19,slot);c.branch(0xc6,'not_ready')
        c.emit(0x15,4);c.branch(0x9b,'not_ready')
        c.emit(0x15,4,0x04);c.branch(0xa3,'not_ready')
        _emit_retained_wait(e,c,3,4,5,'not_ready')
        arm_completion(c,3,4,5)
        c.emit(0x2a);c.ref(0xb6,e.ref('ArmedPlayerBeing','cojvrInsertManualRevolverRoundImpl','()Z'));c.emit(0x3c)
        c.branch(0xa7,'result')
        c.label('not_ready');c.emit(0x03,0x3c)
        c.label('result')
        c.label('try_end')
        for field in (permit,active):c.emit(0x2a,0x03);c.ref(0xb5,field)
        c.emit(0x1b,0xac)
        c.label('catch');c.emit(0x4d,0x2a,0x04);c.ref(0xb5,fault)
        for field in (permit,active):c.emit(0x2a,0x03);c.ref(0xb5,field)
        c.emit(0x2c,0xbf);c.label('reject');c.emit(0x03,0xac)
        e.helper('cojvrInsertManualRevolverRound','()Z',c.finish(),6,
                 exception_table=((c.labels['try'],c.labels['try_end'],c.labels['catch'],0),))
        player=e.finish();e=ClassEdit(player)
        c=Code()
        _runtime_entry_guard(e,c)
        c.emit(0x2a);c.ref(0xb4,e.ref('ArmedPlayerBeing','cojvrReloadProbeWeapon','LWeapon;',9));c.emit(0x4c)
        c.emit(0x2b);c.branch(0xc6,'reject')
        _exact_class_branch(e,c,lambda c:c.emit(0x2b),NATIVE_CYCLE_PROFILES,'cycle')
        from coj_clip_reload import CLIP_WEAPONS
        _exact_class_branch(e,c,lambda c:c.emit(0x2b),CLIP_WEAPONS,'clip')
        _exact_class_branch(e,c,lambda c:c.emit(0x2b),SINGLE_ROUND_REVOLVERS,'revolver')
        c.branch(0xa7,'reject')
        c.label('revolver');c.emit(0x2a);c.ref(0xb6,e.ref('ArmedPlayerBeing','cojvrInsertManualRevolverRound','()Z'));c.emit(0xac)
        c.label('cycle');c.emit(0x2a);c.ref(0xb6,e.ref('ArmedPlayerBeing','cojvrInsertManualNativeCycleRound','()Z'));c.emit(0xac)
        c.label('clip');c.emit(0x2a);c.ref(0xb6,e.ref('ArmedPlayerBeing','cojvrInsertManualClip','()Z'));c.emit(0xac)
        c.label('reject');c.emit(0x03,0xac)
        e.helper('cojvrInsertManualRound','()Z',c.finish(),2)
        player=e.finish()
    if _offline_cycle:
        from coj_reload_cycle import _add_offline_cycle
        player=_add_offline_cycle(player, _offline_weapon)
    if _offline_clip is not None:
        from coj_clip_reload import _add_offline_clip
        player=_add_offline_clip(player, _offline_clip)
    return player, machine
