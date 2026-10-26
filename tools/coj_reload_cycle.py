"""Exact offline body21 transaction; no frame/time setter or runtime admission.

One synchronous native unit request is bracketed by retained references and
readback. Only its same-body Clear may omit the terminating Reload handler.
Started, skin/audio bookkeeping, real close and watchdog remain native.
An exception clears all scope fields and rethrows; uncertain writes quarantine
further insertions/heartbeat for this player instance, without retry or refund.
"""
from coj_reload_wait_probe import ClassEdit
from patch_coj_weapon_consumers import Code, insert_target_tag, u2
from inspect_java_bytecode import Reader, parse_constant_pool


def _retained_completion_veto(e, prefix):
    """Field-only lease independent of the mutable probe and transient latches.

    A completion still executes its native move/end bookkeeping. Only its ammo
    authority is replaced by false while this exact body remains outstanding.
    End/hand-update observations release the lease after the body has exited.
    """
    fields = {name: e.field(prefix + name, desc) for name, desc in (
        ('Weapon', 'LWeapon;'), ('Machine', 'LPlayerStateMashine;'),
        ('Body', 'LBaseState;'), ('Hand', 'I'))}
    def read(c, name): c.emit(0x2a); c.ref(0xb4, fields[name])
    def release(c):
        for name in ('Weapon', 'Machine', 'Body'):
            c.emit(0x2a, 0x01); c.ref(0xb5, fields[name])
    def arm(c, weapon, hand, machine):
        for name, slot in (('Weapon', weapon), ('Hand', hand), ('Machine', machine)):
            c.emit(0x2a, 0x15 if name == 'Hand' else 0x19, slot); c.ref(0xb5, fields[name])
        c.emit(0x2a, 0x19, machine)
        c.ref(0xb4, e.ref('StateMashine', 'm_sCurrentState', 'LBaseState;', 9))
        c.ref(0xb5, fields['Body'])
    current = e.ref('StateMashine', 'm_sCurrentState', 'LBaseState;', 9)
    def wrap(target, transform):
        previous = e.changes.get(target)
        e.changes[target] = lambda code: transform(previous(code) if previous else code)
    def call_prefix(code, name, desc):
        c = Code(); c.emit(0x2a, 0x1b)
        c.ref(0xb6, e.ref('ArmedPlayerBeing', name, desc))
        tag = c.finish(); return tag + b'\0' * (-len(tag) % 4) + code

    c = Code(); read(c, 'Machine'); c.branch(0xc6, 'done')
    c.emit(0x1b); read(c, 'Hand'); c.branch(0xa0, 'done')
    read(c, 'Machine'); c.ref(0xb4, current); read(c, 'Body'); c.branch(0xa5, 'done')
    release(c); c.label('done'); c.emit(0xb1)
    e.helper(prefix + 'Retire', '(I)V', c.finish(), 2)
    for target in (('OnHandStateFinished_ReloadEnd', '(IIZ)V'), ('UpdateHandState', '(IZ)V'),
                   ('OnHandStateStarted_ReloadBegin', '(IIZ)V')):
        wrap(target, lambda code: call_prefix(code, prefix + 'Retire', '(I)V'))

    c = Code()
    for name in ('Machine', 'Body', 'Weapon'): read(c, name); c.branch(0xc6, 'reject')
    c.emit(0x1b); read(c, 'Hand'); c.branch(0xa0, 'reject')
    c.emit(0x1c, 0x10, 21); c.branch(0xa0, 'reject')
    read(c, 'Hand'); c.branch(0x9b, 'reject')
    read(c, 'Hand'); c.emit(0x04); c.branch(0xa3, 'reject')
    read(c, 'Machine'); c.ref(0xb4, current); read(c, 'Body'); c.branch(0xa6, 'reject')
    c.emit(0x2a); c.ref(0xb4, e.ref('ArmedPlayerBeing', 'm_aHandStateMashine',
                                  '[LArmedPlayerBeing$HandStateMashine;', 9))
    c.branch(0xc6, 'reject')
    c.emit(0x2a); c.ref(0xb4, e.ref('ArmedPlayerBeing', 'm_aHandStateMashine',
                                  '[LArmedPlayerBeing$HandStateMashine;', 9))
    read(c, 'Hand'); c.emit(0x32, 0x4e, 0x2d); c.branch(0xc6, 'reject')
    c.emit(0x2d); c.ref(0xb4, e.ref('ArmedPlayerBeing$HandStateMashine', 'm_cStateMashine',
                                 'LPlayerStateMashine;', 9))
    read(c, 'Machine'); c.branch(0xa6, 'reject')
    # Original completion obtains GetActualWeapon again. A replacement must
    # not inherit this old body's transfer: the lease fences the captured
    # machine/body/hand, even when its retained weapon has since been detached.
    c.emit(0x04, 0xac); c.label('reject'); c.emit(0x03, 0xac)
    e.helper(prefix + 'Suppress', '(II)Z', c.finish(), 4)
    def completion(code):
        native = b'\x2a\x19\x04\xb6' + u2(e.ref('ArmedPlayerBeing', 'WeaponReload', '(LWeapon;)Z')) + b'\x36\x07'
        if code.count(native) != 1: raise ValueError('Unknown retained completion transfer seam')
        c = Code(); c.emit(0x2a, 0x1b, 0x1c)
        c.ref(0xb6, e.ref('ArmedPlayerBeing', prefix + 'Suppress', '(II)Z'))
        c.branch(0x99, 'native'); c.emit(0x03, 0x36, 7); c.branch(0xa7, 'after')
        c.label('native'); c.labels['after'] = len(c.data) + len(native)
        pool = parse_constant_pool(Reader(u2(e.next) + e.data[10:e.cp_end] + e.extra))
        return insert_target_tag(code, code.index(native), c.finish(), pool, include_target=True)
    wrap(('OnHandStateFinished_Reload', '(IIZ)V'), completion)
    return arm, release


def _add_offline_cycle(player: bytes, weapon_class, *, helper_name='cojvrInsertManualRound') -> bytes:
    capacities={'WeaponRifleWinchester':12,'WeaponRifleWinchesterScope':12,'WeaponPistolVolcanic':8}
    profiles=(weapon_class,) if isinstance(weapon_class,str) else tuple(weapon_class)
    if not profiles or any(name not in capacities for name in profiles):
        raise ValueError('No qualified offline body-cycle profile')
    e = ClassEdit(player)
    f = {name: e.field('cojvrManualCycle'+name, desc) for name, desc in (
        ('Active','Z'),('SelfClear','Z'),('SawClear','Z'),('Faulted','Z'),
        ('Weapon','LWeapon;'),('Machine','LPlayerStateMashine;'),('Hand','I'))}
    def method(name, desc): return e.ref('ArmedPlayerBeing', name, desc)
    def probe(name, desc): return e.ref('ArmedPlayerBeing', 'cojvrReloadProbe'+name, desc,9)
    def read(c,name): c.emit(0x2a);c.ref(0xb4,f[name])
    def p_read(c,name,desc): c.emit(0x2a);c.ref(0xb4,probe(name,desc))
    def put_bool(c,name,value): c.emit(0x2a,0x04 if value else 0x03);c.ref(0xb5,f[name])
    def reject(c): c.label('reject');c.emit(0x03,0xac)
    def clear(c):
        put_bool(c,'SelfClear',False);put_bool(c,'SawClear',False);put_bool(c,'Active',False)
        for name in ('Weapon','Machine'): c.emit(0x2a,0x01);c.ref(0xb5,f[name])
    def hand_call(c,name,desc):
        c.emit(0x2a);read(c,'Hand');c.ref(0xb6,method(name,desc))
    weapon_owner=e.ref('Weapon','cOwner','LPawnInventory;',9)
    ammo_count=e.ref('Weapon','GetAmmoCount','(I)I')
    ammo_capacity=e.ref('Weapon','GetAmmoCapacity','(I)I')
    ammo_type=e.ref('Weapon','GetAmmoType','(I)I')
    find_counter=method('FindAmmoCounter','(I)LInventoryAmmoCounter;')
    counter_amount=e.ref('InventoryAmmoCounter','GetAmount','()I')
    valid=method('cojvrManualReloadValid','()Z')
    owner_matches=method('cojvrManualCycleOwnerMatches','()Z')

    # This permits coherent closing too; ManualReloadValid intentionally does
    # not. References, cOwner and both hand tuples must still be exact.
    c=Code();read(c,'Active');c.branch(0x99,'reject')
    for name in ('Weapon','Machine'): read(c,name);c.branch(0xc6,'reject')
    read(c,'Hand');c.branch(0x9b,'reject');read(c,'Hand');c.emit(0x04);c.branch(0xa3,'reject')
    for name,desc in (('Weapon','LWeapon;'),('Machine','LPlayerStateMashine;'),('Hand','I')):
        read(c,name);p_read(c,name,desc);c.branch(0xa0 if name=='Hand' else 0xa6,'reject')
    read(c,'Weapon');c.ref(0xb4,weapon_owner);c.emit(0x2a);c.branch(0xa6,'reject')
    read(c,'Machine');hand_call(c,'GetHandStateMashine','(I)LPlayerStateMashine;');c.branch(0xa6,'reject')
    read(c,'Machine');c.ref(0xb4,e.ref('StateMashine','cojvrReloadProbeOwner','LArmedPlayerBeing;',9))
    c.emit(0x2a);c.branch(0xa6,'reject')
    for name in ('GetActualWeapon','GetActiveWeapon','GetDesiredWeapon'):
        hand_call(c,name,'(I)LWeapon;');read(c,'Weapon');c.branch(0xa6,'reject')
        c.emit(0x2a,0x2a);read(c,'Hand');c.ref(0xb6,method('GetOtherHand','(I)I'))
        c.ref(0xb6,method(name,'(I)LWeapon;'));c.branch(0xc7,'reject')
    for name in ('IsNotAlive','IsCarrying'):
        c.emit(0x2a);c.ref(0xb6,method(name,'()Z'));c.branch(0x9a,'reject')
    c.emit(0x2a);c.ref(0xb4,e.ref('ArmedPlayerBeing','m_bNetAttackForced','Z',9));c.branch(0x9a,'reject')
    c.emit(0x2a);c.ref(0xb6,method('GetNetIsOwner','()Z'));c.branch(0x99,'reject')
    c.ref(0xb8,method('IsNetGame','()Z'));c.branch(0x9a,'reject')
    c.emit(0x04,0xac);reject(c)
    e.helper('cojvrManualCycleOwnerMatches','()Z',c.finish(),1)

    # A synchronous self-Clear must never fall through to the native reload
    # callback, even if a virtual getter cancels the probe during Clear. Keep
    # the veto until SetCurrentState returns so nested callbacks cannot consume
    # it prematurely; the caller checks ownership and the witnessed completion.
    # Ordinary close and other hand/state callbacks remain native.
    c=Code();read(c,'SelfClear');c.branch(0x99,'reject')
    c.emit(0x1b);read(c,'Hand');c.branch(0xa0,'reject')
    c.emit(0x1c,0x10,21);c.branch(0xa0,'reject')
    put_bool(c,'SawClear',True);c.emit(0x04,0xac);reject(c)
    e.helper('cojvrManualCycleSuppressCompletion','(II)Z',c.finish(),3)

    # On a failed transaction after native entry, close its still-owned body21
    # even if the probe disappeared before the attempted body restart. Keep
    # the scoped completion veto during close: orphaned body21 would otherwise
    # finish on a later Update and transfer another native round.
    c=Code();read(c,'Active');c.branch(0x99,'done')
    read(c,'Machine');c.branch(0xc6,'done')
    read(c,'Machine');hand_call(c,'GetHandStateMashine','(I)LPlayerStateMashine;');c.branch(0xa6,'done')
    read(c,'Machine');c.ref(0xb4,e.ref('StateMashine','cojvrReloadProbeOwner','LArmedPlayerBeing;',9))
    c.emit(0x4c,0x2b);c.branch(0xc6,'free')
    c.emit(0x2b,0x2a);c.branch(0xa6,'done')
    c.label('free')
    for name in ('GetActualWeapon','GetActiveWeapon','GetDesiredWeapon'):
        hand_call(c,name,'(I)LWeapon;');read(c,'Weapon');c.branch(0xa6,'done')
    hand_call(c,'GetHandStateMashineState','(I)I');c.emit(0x10,21);c.branch(0xa0,'done')
    read(c,'Machine');c.emit(0x10,22)
    c.ref(0xb6,e.ref('PlayerStateMashine','FindStateByID','(I)LPlayerState;'))
    c.emit(0x4c,0x2b);c.branch(0xc6,'done')
    read(c,'Machine');c.emit(0x2b)
    c.ref(0xb6,e.ref('StateMashine','SetCurrentState','(LBaseState;)V'))
    c.label('done');c.emit(0xb1)
    e.helper('cojvrManualCycleAbortBody','()V',c.finish(),2)
    def completion(code):
        c=Code();c.emit(0x2a,0x1b,0x1c)
        c.ref(0xb6,method('cojvrManualCycleSuppressCompletion','(II)Z'))
        c.branch(0x99,'native');c.emit(0xb1);c.label('native')
        prefix=c.finish()
        return prefix+b'\x00'*(-len(prefix)%4)+code
    e.changes[('OnHandStateFinished_Reload','(IIZ)V')]=completion
    def heartbeat(code):
        c=Code()
        for name in ('Active','Faulted'):read(c,name);c.branch(0x9a,'reject')
        c.branch(0xa7,'native');reject(c);c.label('native')
        prefix=c.finish()
        return prefix+b'\x00'*(-len(prefix)%4)+code
    e.changes[('cojvrKeepManualReloadAlive','()Z')]=heartbeat
    arm_completion, release_completion = _retained_completion_veto(e, 'cojvrManualCycleDebt')

    # Locals: 1 weapon, 2 machine, 3 counter, 4 loaded, 5 reserve,
    # 6 capacity, 7 ammo type, 8 saved hand, 9 caught Throwable,
    # 10 native remaining result retained across readback.
    c=Code()
    for name in ('Active','Faulted'):read(c,name);c.branch(0x9a,'reject')
    p_read(c,'Weapon','LWeapon;');c.emit(0x4c)
    p_read(c,'Machine','LPlayerStateMashine;');c.emit(0x4d)
    p_read(c,'Hand','I');c.emit(0x36,8)
    for name,opcode,local in (('Weapon',0x19,1),('Machine',0x19,2),('Hand',0x15,8)):
        c.emit(0x2a,opcode,local);c.ref(0xb5,f[name])
    # Acquire before any virtual eligibility/parameter read: a callback during
    # a lazy native parameter lookup cannot enter another insertion transaction.
    put_bool(c,'Active',True)
    c.label('try')
    # A callback in the initial validator may itself lose the probe and throw.
    # Capture a cached body wait before any virtual call, not just before ammo.
    for slot in (1, 2): c.emit(0x19,slot); c.branch(0xc6,'validate')
    p_read(c,'Status','I');c.emit(0x05);c.branch(0xa0,'validate')
    arm_completion(c, 1, 8, 2)
    c.label('validate')
    c.emit(0x2a);c.ref(0xb6,valid);c.branch(0x99,'not_ready')
    p_read(c,'Status','I');c.emit(0x05);c.branch(0xa0,'not_ready')
    c.emit(0x2b);c.ref(0xb4,weapon_owner);c.emit(0x2a);c.branch(0xa6,'not_ready')
    c.emit(0x2b,0x03);c.ref(0xb6,ammo_count);c.emit(0x36,4)
    c.emit(0x2b,0x03);c.ref(0xb6,ammo_capacity);c.emit(0x36,6)
    # Bind the expected capacity to the exact current native class. A combined
    # production derivative must not accept the Volcanic capacity for a rifle.
    for index,name in enumerate(profiles):
        c.emit(0x2b);c.ref(0xb6,e.ref('java/lang/Object','getClass','()Ljava/lang/Class;'))
        c.ref(0xb6,e.ref('java/lang/Class','getName','()Ljava/lang/String;'))
        literal=e.append(b'\x08'+e.utf8(name).to_bytes(2,'big'))
        c.ref(0x13,literal);c.ref(0xb6,e.ref('java/lang/String','equals','(Ljava/lang/Object;)Z'))
        c.branch(0x99,'next_profile_'+str(index))
        c.emit(0x15,6,0x10,capacities[name]);c.branch(0xa0,'not_ready')
        c.branch(0xa7,'profile_valid')
        c.label('next_profile_'+str(index))
    c.branch(0xa7,'not_ready');c.label('profile_valid')
    c.emit(0x15,4);c.branch(0x9b,'not_ready');c.emit(0x15,4,0x15,6);c.branch(0xa2,'not_ready')
    c.emit(0x2b,0x03);c.ref(0xb6,ammo_type);c.emit(0x36,7)
    c.emit(0x2a,0x15,7);c.ref(0xb6,find_counter);c.emit(0x4e,0x2d);c.branch(0xc6,'not_ready')
    c.emit(0x2d);c.ref(0xb6,counter_amount);c.emit(0x36,5,0x15,5);c.branch(0x9e,'not_ready')
    c.emit(0x2a);c.ref(0xb6,owner_matches);c.branch(0x99,'fault')
    # Lazy virtual parameter reads may cancel/change state while retaining the
    # same references. Recheck full waiting eligibility before the authority.
    c.emit(0x2a);c.ref(0xb6,valid);c.branch(0x99,'not_ready')
    p_read(c,'Status','I');c.emit(0x05);c.branch(0xa0,'not_ready')
    c.emit(0x2a,0x2b);c.ref(0xb6,method('WeaponReload','(LWeapon;)Z'));c.branch(0x99,'fault')

    def readback():
        c.emit(0x2a);c.ref(0xb6,owner_matches);c.branch(0x99,'fault')
        c.emit(0x2b,0x03);c.ref(0xb6,ammo_count);c.emit(0x15,4,0x04,0x60);c.branch(0xa0,'fault')
        c.emit(0x2b,0x03);c.ref(0xb6,ammo_capacity);c.emit(0x15,6);c.branch(0xa0,'fault')
        c.emit(0x2b,0x03);c.ref(0xb6,ammo_type);c.emit(0x15,7);c.branch(0xa0,'fault')
        c.emit(0x2a,0x15,7);c.ref(0xb6,find_counter);c.emit(0x2d);c.branch(0xa6,'fault')
        c.emit(0x2d);c.ref(0xb6,counter_amount);c.emit(0x15,5,0x04,0x64);c.branch(0xa0,'fault')
    readback()
    # A native true is insufficient (the original non-owner path can return
    # true without loading). Publish only after the retained +1/-1 proof.
    singleton=e.ref('GameMode','cSingleton','LGameMode;',9)
    c.ref(0xb2,singleton);c.branch(0xc6,'after_event')
    c.ref(0xb2,singleton);c.ref(0xb2,e.ref('EnumGameEvent','_WEAPON_RELOADED','I',9))
    c.ref(0xb6,e.ref('GameMode','OnGameEvent','(I)V'))
    c.label('after_event');readback()
    # A native remaining query may trigger callbacks after the first witness.
    # Save its result so a failed readback reaches fault with an empty stack.
    hand_call(c,'GetWeaponMaxAmmoToReload','(I)I');c.emit(0x36,10)
    readback()
    c.emit(0x15,10);c.branch(0x9d,'still_waiting')
    c.emit(0x2a);c.ref(0xb6,method('cojvrCancelReloadProbe','()V'))
    c.emit(0x2a);c.ref(0xb6,owner_matches);c.branch(0x99,'fault')
    c.label('still_waiting')
    p_read(c,'Status','I');c.emit(0x06);c.branch(0x9f,'closing')
    c.emit(0x2a);c.ref(0xb6,valid);c.branch(0x99,'fault')
    p_read(c,'Status','I');c.emit(0x05);c.branch(0xa0,'fault')
    put_bool(c,'SawClear',False);put_bool(c,'SelfClear',True)
    c.emit(0x2c,0x2c);c.ref(0xb6,e.ref('StateMashine','GetCurrentState','()LBaseState;'))
    c.ref(0xb6,e.ref('StateMashine','SetCurrentState','(LBaseState;)V'))
    read(c,'SawClear');c.branch(0x99,'fault')
    put_bool(c,'SelfClear',False)
    readback()
    c.emit(0x2a);c.ref(0xb6,valid);c.branch(0x99,'fault')
    # New body completion must reach the ordinary native veto before another
    # cartridge is accepted. These are probe fields, not animation clocks.
    c.emit(0x2a,0x04);c.ref(0xb5,probe('Status','I'))
    for name in ('Since','Until'):c.emit(0x2a,0x0b);c.ref(0xb5,probe(name,'F'))
    c.branch(0xa7,'accepted')
    c.label('closing')
    p_read(c,'Cancelled','Z');c.branch(0x99,'fault')
    hand_call(c,'GetHandStateMashineState','(I)I');c.emit(0x10,22);c.branch(0xa0,'fault')
    hand_call(c,'GetWeaponMaxAmmoToReload','(I)I');c.emit(0x36,10)
    readback()
    c.emit(0x15,10);c.branch(0x9d,'fault')
    c.label('accepted');c.label('try_end');release_completion(c);clear(c);c.emit(0x04,0xac)
    c.label('fault');put_bool(c,'Faulted',True)
    put_bool(c,'SelfClear',False)
    c.label('abort_try')
    c.emit(0x2a);c.ref(0xb6,method('cojvrManualCycleAbortBody','()V'))
    c.label('abort_end')
    clear(c);c.emit(0x03,0xac)
    c.label('not_ready');clear(c);c.emit(0x03,0xac)
    c.label('catch');c.emit(0x3a,9);put_bool(c,'Faulted',True);clear(c);c.emit(0x19,9,0xbf)
    reject(c)
    code=c.finish()
    e.helper(helper_name,'()Z',code,11,
             exception_table=((c.labels['try'],c.labels['try_end'],c.labels['catch'],0),
                              (c.labels['abort_try'],c.labels['abort_end'],c.labels['catch'],0)))
    return e.finish()
