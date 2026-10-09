"""Exact opt-in persistent session on the demonstrated zero-transfer veto.

Native callbacks still own opening/closing. Only an immediate admitted insertion
calls the shipped ammunition authority, once; there is no credit/round cache.
"""
from coj_reload_wait_probe import ClassEdit, patch_probe_classes
from patch_coj_weapon_consumers import Code


def patch_manual_classes(player, machine):
    player, machine = patch_probe_classes(player, machine)
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
    read(c,'Weapon','LWeapon;'); c.ref(0xb6,e.ref('Weapon','IsReloadWholeClipAtOnce','()Z')); c.branch(0x9a,'reject')
    read(c,'Weapon','LWeapon;'); c.ref(0xb6,e.ref('Weapon','GetAmmoTypeCount','()I')); c.emit(0x04); c.branch(0xa0,'reject')
    c.emit(0x04,0xac); reject(c)
    e.helper('cojvrManualReloadValid','()Z',c.finish(),3)

    # Watchdog remains native: missed heartbeat closes after the proven interval.
    # Float subtraction rejects both infinity and NaN before extending a deadline.
    import struct
    wait=e.append(b'\x04'+struct.pack('>f',1.5))
    c=Code(); c.emit(0x2a); c.ref(0xb6,method('cojvrManualReloadValid','()Z')); c.branch(0x99,'reject')
    read(c,'Machine','LPlayerStateMashine;'); c.ref(0xb4,clock); c.emit(0x38,1)
    c.emit(0x17,1,0x17,1,0x66,0x0b,0x95); c.branch(0x9a,'reject')
    read(c,'Status','I'); c.emit(0x05); c.branch(0xa0,'accepted')
    c.emit(0x2a,0x17,1); c.ref(0x13,wait); c.emit(0x62); c.ref(0xb5,field('Until','F'))
    c.label('accepted'); c.emit(0x04,0xac); reject(c)
    e.helper('cojvrKeepManualReloadAlive','()Z',c.finish(),2)

    # PawnArmed.WeaponReload(LWeapon;)Z caps inventory/capacity and takes one
    # round for these single-ammo non-whole-clip weapons. Result and event are
    # exactly the native authority; close is still the zero-transfer callback.
    c=Code(); c.emit(0x2a); c.ref(0xb6,method('cojvrManualReloadValid','()Z')); c.branch(0x99,'reject')
    read(c,'Status','I'); c.emit(0x05); c.branch(0xa0,'reject')
    c.emit(0x2a); read(c,'Weapon','LWeapon;'); c.ref(0xb6,method('WeaponReload','(LWeapon;)Z')); c.emit(0x3c)
    c.emit(0x1b); c.branch(0x99,'capacity')
    singleton=e.ref('GameMode','cSingleton','LGameMode;',9)
    c.ref(0xb2,singleton); c.branch(0xc6,'capacity')
    c.ref(0xb2,singleton); c.ref(0xb2,e.ref('EnumGameEvent','_WEAPON_RELOADED','I',9))
    c.ref(0xb6,e.ref('GameMode','OnGameEvent','(I)V'))
    c.label('capacity'); hand_call(c,'GetWeaponMaxAmmoToReload','(I)I'); c.branch(0x9d,'result')
    c.emit(0x2a); c.ref(0xb6,method('cojvrCancelReloadProbe','()V'))
    c.label('result'); c.emit(0x1b,0xac); reject(c)
    e.helper('cojvrInsertManualRound','()Z',c.finish(),2)
    return e.finish(), machine
