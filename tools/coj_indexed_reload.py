"""Isolated exact-archive indexed unit-accounting laboratory; NEVER installs a PAK.

The emitted helper is intentionally uncalled by all original methods. It is a
new selected-index transaction, not PawnArmed.WeaponReload's ascending selector.
Original indexed count and typed reserve setters retain their native clamping /
skin boundary. Partial effects are quarantined, never repaired, refunded or
retried. Interpreted evidence cannot admit these families to runtime reload UX.
"""
import hashlib
import io
import zipfile

from coj_reload_wait_probe import ClassEdit
from patch_coj_weapon_consumers import Code

ORIGINAL_ARCHIVE_SHA256 = 'f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12'
PROFILES = (
    ('WeaponRifleRemington', (1, 1), (1, 1), True, True),
    ('WeaponRifleRemingtonSawOff', (1, 1), (1, 1), True, True),
    ('WeaponPistolDerringer', (1, 1), (2, 2), True, False),
    ('WeaponPistolLemant', (1, 8), (0, 2), False, False),
)
# Types are original PawnInventory table indices, NOT the C++ diagnostic enum:
# 0 rifle, 1 shotgun, 2 pistol. Each shared reserve counter is retained once.
COUNTERS = ('Rifle', 'Shotgun', 'Pistol')


def build_indexed_reload_lab(original_archive: bytes) -> bytes:
    """Return ONLY an in-memory ArmedPlayerBeing derivative from exact originals.

    Input is the complete original archive, hashed before ZIP parsing. No path,
    output file, installer, runtime profile or archive integration is provided.
    The full archive identity pins every constructor and inherited setter too.
    Calling the derivative/repacked/tampered archive fails closed.

    Public lab entry: cojvrLabInsertIndexedUnit(Weapon, int armedHand,
    int selectedAmmoIndex) -> boolean. It requires coherent native reload-body
    state 21 and desired weapon state 21; that alone is NOT a supervised manual
    wait/gesture/port contract. No existing caller is connected to this method.
    Busy/Faulted belong to the player instance. JVM locals retain all references
    across this single synchronous invocation; there is no reset/quarantine bypass.
    """
    if hashlib.sha256(original_archive).hexdigest() != ORIGINAL_ARCHIVE_SHA256:
        raise ValueError('Unknown original code.pak SHA-256; indexed lab refuses mutation')
    with zipfile.ZipFile(io.BytesIO(original_archive)) as archive:
        player = archive.read('ArmedPlayerBeing.class')
    return _append_indexed_helpers(player)


def _append_indexed_helpers(player, *, _supervised=False, helper_name='cojvrLabInsertIndexedUnit'):
    """Internal composition seam; public builders own pinned input admission."""
    e = ClassEdit(player)
    if _supervised:
        from coj_reload_cycle import _retained_completion_veto
        arm_completion, _ = _retained_completion_veto(e, 'cojvrIndexedDebt')
    own = 'ArmedPlayerBeing'
    fields = {name: e.field('cojvrIndexed' + name, 'Z') for name in ('Busy', 'Faulted')}
    def ref(owner, name, desc, tag=10): return e.ref(owner, name, desc, tag)
    def pmethod(name, desc): return ref(own, name, desc)
    def aload(c, slot): c.emit(0x19, slot)
    def iload(c, slot): c.emit(0x15, slot)
    def astore(c, slot): c.emit(0x3a, slot)
    def istore(c, slot): c.emit(0x36, slot)
    def integer(c, value):
        if -1 <= value <= 5: c.emit(value + 3)
        else: c.emit(0x10, value)
    def put(c, field, value): c.emit(0x2a); integer(c, int(value)); c.ref(0xb5, fields[field])
    def flag(c, field): c.emit(0x2a); c.ref(0xb4, fields[field])
    def call(c, owner, name, desc): c.ref(0xb6, ref(owner, name, desc))
    def same_class_name(c, local, name):
        aload(c, local); call(c, 'java/lang/Object', 'getClass', '()Ljava/lang/Class;')
        call(c, 'java/lang/Class', 'getName', '()Ljava/lang/String;')
        c.ref(0x13, e.append(b'\x08' + e.utf8(name).to_bytes(2, 'big')))
        call(c, 'java/lang/String', 'equals', '(Ljava/lang/Object;)Z')
    weapon_owner = ref('Weapon', 'cOwner', 'LPawnInventory;', 9)
    counter_owner = ref('InventoryAmmoCounter', 'cOwner', 'LPawnInventory;', 9)
    forced = ref(own, 'm_bNetAttackForced', 'Z', 9)
    reserve_fields = [ref('PawnInventory', 'nAmmo' + suffix, 'I', 9) for suffix in COUNTERS]
    loaded_fields = [ref('WeaponFireAmmo', 'm_iAmmo0Count', 'I', 9),
                     # Both exact DoubleAmmo classes declare this field; choose
                     # the owning declaration below for direct readback checks.
                     None]

    # Read-only exact concrete class/parameter contract. No new parameter writer.
    c = Code()
    for profile, (name, capacities, types, whole, _) in enumerate(PROFILES):
        iload(c, 2); integer(c, profile); c.branch(0xa0, 'next_profile_' + str(profile))
        same_class_name(c, 1, name); c.branch(0x99, 'reject')
        aload(c, 1); call(c, 'Weapon', 'GetAmmoTypeCount', '()I'); integer(c, 2); c.branch(0xa0, 'reject')
        aload(c, 1); call(c, 'Weapon', 'IsReloadWholeClipAtOnce', '()Z'); integer(c, int(whole)); c.branch(0xa0, 'reject')
        for index in range(2):
            for method, value in (('GetAmmoCapacity', capacities[index]), ('GetAmmoType', types[index])):
                aload(c, 1); integer(c, index); call(c, 'Weapon', method, '(I)I')
                integer(c, value); c.branch(0xa0, 'reject')
        c.emit(0x04, 0xac)
        c.label('next_profile_' + str(profile))
    c.label('reject'); c.emit(0x03, 0xac)
    e.helper('cojvrIndexedProfileMatches', '(LWeapon;I)Z', c.finish(), 3)

    # Owner proof includes complete selected hand tuple, retained hand machine,
    # reload state, and exact shared/empty support-hand shape. JVM object identity
    # comparisons retain high-bit/native-ID independence (no integer-ID test).
    # Locals: self, weapon, hand, retained machine, shared-other-hand boolean.
    c = Code()
    aload(c, 1); c.ref(0xb4, weapon_owner); c.emit(0x2a); c.branch(0xa6, 'reject')
    for name in ('IsNotAlive', 'IsCarrying'):
        c.emit(0x2a); call(c, own, name, '()Z'); c.branch(0x9a, 'reject')
    c.emit(0x2a); c.ref(0xb4, forced); c.branch(0x9a, 'reject')
    c.emit(0x2a); call(c, own, 'GetNetIsOwner', '()Z'); c.branch(0x99, 'reject')
    c.ref(0xb8, pmethod('IsNetGame', '()Z')); c.branch(0x9a, 'reject')
    c.emit(0x2a); iload(c, 2); call(c, own, 'GetHandStateMashine', '(I)LPlayerStateMashine;')
    aload(c, 3); c.branch(0xa6, 'reject')
    for name in ('GetHandStateMashineState', 'GetDesiredWeaponState'):
        c.emit(0x2a); iload(c, 2); call(c, own, name, '(I)I'); integer(c, 21); c.branch(0xa0, 'reject')
    for i, name in enumerate(('GetActualWeapon', 'GetActiveWeapon', 'GetDesiredWeapon')):
        c.emit(0x2a); iload(c, 2); call(c, own, name, '(I)LWeapon;'); aload(c, 1); c.branch(0xa6, 'reject')
        c.emit(0x2a, 0x04); iload(c, 2); c.emit(0x64); call(c, own, name, '(I)LWeapon;')
        iload(c, 4); c.branch(0x99, 'empty_other_' + str(i))
        aload(c, 1); c.branch(0xa6, 'reject'); c.branch(0xa7, 'other_done_' + str(i))
        c.label('empty_other_' + str(i)); c.branch(0xc7, 'reject')
        c.label('other_done_' + str(i))
    # Recheck direct owner after virtual calls, without invoking another reader.
    if _supervised:
        c.emit(0x2a);call(c,own,'cojvrManualReloadValid','()Z');c.branch(0x99,'reject')
        for suffix,value in (('Status',2),('Cancelled',0)):
            c.emit(0x2a);c.ref(0xb4,ref(own,'cojvrReloadProbe'+suffix,'I' if suffix=='Status' else 'Z',9))
            integer(c,value);c.branch(0xa0,'reject')
        for suffix,slot,desc in (('Weapon',1,'LWeapon;'),('Hand',2,'I'),('Machine',3,'LPlayerStateMashine;')):
            c.emit(0x2a);c.ref(0xb4,ref(own,'cojvrReloadProbe'+suffix,desc,9))
            (iload if suffix=='Hand' else aload)(c,slot);c.branch(0xa0 if suffix=='Hand' else 0xa6,'reject')
        from coj_indexed_reload_session import _emit_retained_wait
        _emit_retained_wait(e,c,1,2,3,'reject')
    aload(c, 1); c.ref(0xb4, weapon_owner); c.emit(0x2a); c.branch(0xa6, 'reject')
    c.emit(0x04, 0xac); c.label('reject'); c.emit(0x03, 0xac)
    e.helper('cojvrIndexedOwnerMatches', '(LWeapon;ILPlayerStateMashine;I)Z', c.finish(), 5)

    # Main locals: 0 self, 1 weapon, 2 hand, 3 index; 4 machine, 5 profile,
    # 6/7 loaded; 8..10 counters; 11..13 reserve; 14..16 maxima;
    # 17 selected loaded, 18 selected reserve, 19 selected counter,
    # 20 selected type, 21 Throwable, 22 support-hand shape.
    c = Code()
    for name in ('Busy', 'Faulted'): flag(c, name); c.branch(0x9a, 'reject')
    if _supervised:
        dispatch=ref(own,'cojvrIndexedSessionDispatch','Z',9)
        c.emit(0x2a);c.ref(0xb4,dispatch);c.branch(0x99,'reject')
        c.emit(0x2a,0x03);c.ref(0xb5,dispatch)
    aload(c, 1); c.branch(0xc6, 'reject')
    for slot in (2, 3):
        iload(c, slot); c.branch(0x9b, 'reject'); iload(c, slot); c.emit(0x04); c.branch(0xa3, 'reject')
    put(c, 'Busy', True)  # Before getClass/any virtual read, including parameter factories.
    c.label('try')
    for profile, (name, _, _, _, _) in enumerate(PROFILES):
        same_class_name(c, 1, name); c.branch(0x99, 'next_class_' + str(profile))
        integer(c, profile); istore(c, 5); c.branch(0xa7, 'class_selected')
        c.label('next_class_' + str(profile))
    c.branch(0xa7, 'not_ready'); c.label('class_selected')

    def profile_matches(failure):
        c.emit(0x2a); aload(c, 1); iload(c, 5)
        call(c, own, 'cojvrIndexedProfileMatches', '(LWeapon;I)Z'); c.branch(0x99, failure)
    def owner_matches(failure):
        c.emit(0x2a); aload(c, 1); iload(c, 2); aload(c, 4); iload(c, 22)
        call(c, own, 'cojvrIndexedOwnerMatches', '(LWeapon;ILPlayerStateMashine;I)Z'); c.branch(0x99, failure)
    profile_matches('not_ready')
    c.emit(0x2a); iload(c, 2); call(c, own, 'GetHandStateMashine', '(I)LPlayerStateMashine;'); astore(c, 4)
    aload(c, 4); c.branch(0xc6, 'not_ready')
    c.emit(0x03); istore(c, 22)
    c.emit(0x2a, 0x04); iload(c, 2); c.emit(0x64); call(c, own, 'GetActiveWeapon', '(I)LWeapon;')
    c.branch(0xc6, 'support_ready')
    iload(c, 5); integer(c, 1); c.branch(0xa3, 'not_ready')  # Only exact long guns may share.
    c.emit(0x04); istore(c, 22); c.label('support_ready')
    owner_matches('not_ready')
    for index in range(2):
        aload(c, 1); integer(c, index); call(c, 'Weapon', 'GetAmmoCount', '(I)I'); istore(c, 6 + index)
        iload(c, 6 + index); c.branch(0x9b, 'not_ready')
        iload(c, 6 + index); aload(c, 1); integer(c, index); call(c, 'Weapon', 'GetAmmoCapacity', '(I)I')
        c.branch(0xa3, 'not_ready')
    for pool, suffix in enumerate(COUNTERS):
        c.emit(0x2a); integer(c, pool); call(c, 'PawnArmed', 'FindAmmoCounter', '(I)LInventoryAmmoCounter;'); astore(c, 8 + pool)
        aload(c, 8 + pool); c.branch(0xc6, 'not_ready')
        same_class_name(c, 8 + pool, 'InventoryAmmoCounter' + suffix); c.branch(0x99, 'not_ready')
        aload(c, 8 + pool); c.ref(0xb4, counter_owner); c.emit(0x2a); c.branch(0xa6, 'not_ready')
        aload(c, 8 + pool); call(c, 'InventoryAmmoCounter', 'GetMaxAmount', '()I'); istore(c, 14 + pool)
        iload(c, 14 + pool); c.branch(0x9b, 'not_ready')
        aload(c, 8 + pool); call(c, 'InventoryAmmoCounter', 'GetAmount', '()I'); istore(c, 11 + pool)
        iload(c, 11 + pool); c.branch(0x9b, 'not_ready')
        iload(c, 11 + pool); iload(c, 14 + pool); c.branch(0xa3, 'not_ready')
    # Select retained values with explicit branches; no arrays/ascending scan.
    iload(c, 3); c.branch(0x99, 'select_zero'); iload(c, 7); c.branch(0xa7, 'selected_loaded')
    c.label('select_zero'); iload(c, 6); c.label('selected_loaded'); istore(c, 17)
    iload(c, 17); aload(c, 1); iload(c, 3); call(c, 'Weapon', 'GetAmmoCapacity', '(I)I'); c.branch(0xa2, 'not_ready')
    aload(c, 1); iload(c, 3); call(c, 'Weapon', 'GetAmmoType', '(I)I'); istore(c, 20)
    for pool in range(3):
        iload(c, 20); integer(c, pool); c.branch(0xa0, 'next_pool_' + str(pool))
        aload(c, 8 + pool); astore(c, 19); iload(c, 11 + pool); istore(c, 18); c.branch(0xa7, 'pool_ready')
        c.label('next_pool_' + str(pool))
    c.branch(0xa7, 'not_ready'); c.label('pool_ready')
    iload(c, 18); c.branch(0x9e, 'not_ready')

    pass_id = 0
    def readback(loaded_added, reserve_debited):
        nonlocal pass_id
        pass_id += 1
        profile_matches('fault'); owner_matches('fault')
        # Discovery getters may temporarily inflate capacity then restore it.
        # Profile equality alone does not prove captured counts have room. Use
        # the pinned capacities without another virtual read, including the
        # untouched slot and the selected prewrite snapshot on every pass.
        for profile, (_, capacities, _, _, _) in enumerate(PROFILES):
            iload(c, 5); integer(c, profile); c.branch(0xa0, f'bounds_next_{pass_id}_{profile}')
            for index, capacity in enumerate(capacities):
                iload(c, 6+index); c.branch(0x9b, 'fault')
                iload(c, 6+index); integer(c, capacity); c.branch(0xa3, 'fault')
                iload(c, 3); integer(c, index); c.branch(0xa0, f'bounds_other_{pass_id}_{profile}_{index}')
                iload(c, 17); integer(c, capacity); c.branch(0xa2, 'fault')
                c.label(f'bounds_other_{pass_id}_{profile}_{index}')
            c.branch(0xa7, f'bounds_done_{pass_id}')
            c.label(f'bounds_next_{pass_id}_{profile}')
        c.branch(0xa7, 'fault'); c.label(f'bounds_done_{pass_id}')
        # The captured selector must still describe the actual selected index,
        # not merely a pool that also happens to exist in the retained vector.
        aload(c, 1); iload(c, 3); call(c, 'Weapon', 'GetAmmoType', '(I)I')
        iload(c, 20); c.branch(0xa0, 'fault')
        for index in range(2):
            aload(c, 1); integer(c, index); call(c, 'Weapon', 'GetAmmoCount', '(I)I')
            iload(c, 6 + index)
            if loaded_added:
                iload(c, 3); integer(c, index); c.branch(0xa0, f'count_{pass_id}_{index}')
                c.emit(0x04, 0x60); c.label(f'count_{pass_id}_{index}')
            c.branch(0xa0, 'fault')
        for pool, suffix in enumerate(COUNTERS):
            c.emit(0x2a); integer(c, pool); call(c, 'PawnArmed', 'FindAmmoCounter', '(I)LInventoryAmmoCounter;')
            aload(c, 8 + pool); c.branch(0xa6, 'fault')
            same_class_name(c, 8 + pool, 'InventoryAmmoCounter' + suffix); c.branch(0x99, 'fault')
            aload(c, 8 + pool); c.ref(0xb4, counter_owner); c.emit(0x2a); c.branch(0xa6, 'fault')
            aload(c, 8 + pool); call(c, 'InventoryAmmoCounter', 'GetMaxAmount', '()I')
            iload(c, 14 + pool); c.branch(0xa0, 'fault')
            def expected_reserve():
                iload(c, 11 + pool)
                if reserve_debited:
                    iload(c, 20); integer(c, pool); c.branch(0xa0, f'reserve_{pass_id}_{pool}_{expected_reserve.calls}')
                    c.emit(0x04, 0x64); c.label(f'reserve_{pass_id}_{pool}_{expected_reserve.calls}')
                expected_reserve.calls += 1
            expected_reserve.calls = 0
            aload(c, 8 + pool); call(c, 'InventoryAmmoCounter', 'GetAmount', '()I'); expected_reserve(); c.branch(0xa0, 'fault')
            # Read the backing vector independently after virtual counter calls.
            c.emit(0x2a); c.ref(0xb4, reserve_fields[pool]); expected_reserve(); c.branch(0xa0, 'fault')
        profile_matches('fault')
        for pool in range(3):
            aload(c, 8 + pool); call(c, 'InventoryAmmoCounter', 'GetMaxAmount', '()I')
            iload(c, 14 + pool); c.branch(0xa0, 'fault')
        # Every parameter/counter accessor precedes this full phase/desired/
        # owner/native eligibility proof; busy alone cannot prevent cancellation.
        owner_matches('fault')
        # Owner callbacks can replace the counter table. Re-resolve retained
        # identities immediately before the next writer; original FindAmmoCounter
        # only reads the inventory array and performs no native/user callbacks.
        for pool in range(3):
            c.emit(0x2a); integer(c, pool); call(c, 'PawnArmed', 'FindAmmoCounter', '(I)LInventoryAmmoCounter;')
            aload(c, 8 + pool); c.branch(0xa6, 'fault')
        # Final direct loaded/reserve/owner check after all callbacks. This
        # cannot prevent native reentry during the next setter; the post-setter
        # full proof detects that uncertainty and quarantines without retry.
        for index in range(2):
            aload(c, 1)
            if index == 0:
                c.emit(0xc0); c.emit(*ref_class_bytes(e, 'WeaponFireAmmo'))
                c.ref(0xb4, loaded_fields[0])
            else:
                iload(c, 5); c.emit(0x04); c.branch(0xa3, f'pistol_{pass_id}')
                c.emit(0xc0); c.emit(*ref_class_bytes(e, 'WeaponRifleDoubleAmmo'))
                c.ref(0xb4, ref('WeaponRifleDoubleAmmo', 'm_iAmmo1Count', 'I', 9))
                c.branch(0xa7, f'loaded_raw_{pass_id}')
                c.label(f'pistol_{pass_id}'); c.emit(0xc0); c.emit(*ref_class_bytes(e, 'WeaponPistolDoubleAmmo'))
                c.ref(0xb4, ref('WeaponPistolDoubleAmmo', 'm_iAmmo1Count', 'I', 9))
                c.label(f'loaded_raw_{pass_id}')
            iload(c, 6 + index)
            if loaded_added:
                iload(c, 3); integer(c, index); c.branch(0xa0, f'raw_{pass_id}_{index}')
                c.emit(0x04, 0x60); c.label(f'raw_{pass_id}_{index}')
            c.branch(0xa0, 'fault')
        for pool in range(3):
            aload(c, 8 + pool); c.ref(0xb4, counter_owner); c.emit(0x2a); c.branch(0xa6, 'fault')
            c.emit(0x2a); c.ref(0xb4, reserve_fields[pool]); iload(c, 11 + pool)
            if reserve_debited:
                iload(c, 20); integer(c, pool); c.branch(0xa0, f'raw_reserve_{pass_id}_{pool}')
                c.emit(0x04, 0x64); c.label(f'raw_reserve_{pass_id}_{pool}')
            c.branch(0xa0, 'fault')

    readback(False, False)
    if _supervised: arm_completion(c, 1, 2, 4)
    aload(c, 1); iload(c, 3); iload(c, 17); c.emit(0x04, 0x60); call(c, 'Weapon', 'SetAmmoCount', '(II)V')
    readback(True, False)
    aload(c, 19); iload(c, 18); c.emit(0x04, 0x64); call(c, 'InventoryAmmoCounter', 'SetAmount', '(I)V')
    readback(True, True)
    if _supervised:
        singleton=ref('GameMode','cSingleton','LGameMode;',9)
        c.ref(0xb2,singleton);c.branch(0xc6,'after_event')
        c.ref(0xb2,singleton);c.ref(0xb2,ref('EnumGameEvent','_WEAPON_RELOADED','I',9))
        call(c,'GameMode','OnGameEvent','(I)V')
        c.label('after_event');readback(True,True)
    c.label('try_end'); put(c, 'Busy', False); c.emit(0x04, 0xac)
    c.label('fault'); put(c, 'Faulted', True)
    c.label('not_ready'); put(c, 'Busy', False); c.emit(0x03, 0xac)
    c.label('catch'); astore(c, 21); put(c, 'Faulted', True); put(c, 'Busy', False); aload(c, 21); c.emit(0xbf)
    c.label('reject'); c.emit(0x03, 0xac)
    code = c.finish()
    e.helper(helper_name, '(LWeapon;II)Z', code, 23,
             exception_table=((c.labels['try'], c.labels['try_end'], c.labels['catch'], 0),))
    return e.finish()


def ref_class_bytes(editor, name):
    """Constant-pool class operand for verifier-required narrowed field access."""
    return editor.cls(name).to_bytes(2, 'big')
