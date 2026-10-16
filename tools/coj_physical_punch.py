"""Exact-game physical boxing consumers, installed by the canonical patcher."""
import hashlib
from inspect_java_bytecode import Reader,parse_constant_pool
from coj_reload_wait_probe import ClassEdit
from patch_coj_weapon_consumers import Code,u2,u4,insert_target_tag

MELEE_SHA256='954722bfa001172ee9c1f6884f171dd401e50b06e357db554b0938881e55eff6'

def scoped_helper(e,name,desc,c,locals_,start,end,handler):
    code=c.finish()
    table=u2(1)+u2(start)+u2(end)+u2(handler)+u2(0)
    attr=u2(8)+u2(locals_)+u4(len(code))+code+table+u2(0)
    e.helpers.append(u2(1)+u2(e.utf8(name))+u2(e.utf8(desc))+u2(1)+
                     u2(e.utf8('Code'))+u4(len(attr))+attr)

def stacked_helper(e,name,desc,c,locals_,stack):
    code=c.finish()
    attr=u2(stack)+u2(locals_)+u4(len(code))+code+u2(0)+u2(0)
    e.helpers.append(u2(1)+u2(e.utf8(name))+u2(e.utf8(desc))+u2(1)+
                     u2(e.utf8('Code'))+u4(len(attr))+attr)

def patch_punch_classes(player, melee):
    if hashlib.sha256(melee).hexdigest()!=MELEE_SHA256:
        raise ValueError('Unknown WeaponMelee.class SHA-256; no physical punch mutation')
    p=ClassEdit(player);m=ClassEdit(melee)
    start=m.field('cojvrPunchStart','LVector;');end=m.field('cojvrPunchEnd','LVector;')
    traces=m.field('cojvrPunchTraces','[[LVector;')
    melee_entries=m.field('cojvrPhysicalMeleeEntries','I')
    hit_calls=m.field('cojvrPhysicalNativeHitCalls','I')
    owner='ArmedPlayerBeing'
    punch_weapon=p.field('cojvrPunchWeapon','LWeaponMelee;')
    punch_alt=p.field('cojvrPunchAlternate','Z')
    punch_mode=p.field('cojvrPunchMode','I')
    def method(n,d):return p.ref(owner,n,d)
    actual=method('GetActualWeapon','(I)LWeapon;')
    desired=method('GetDesiredWeapon','(I)LWeapon;')
    boxing_hand=method('GetBoxingHand','()I')
    get_move=method('GetWeaponMoveState','(LWeapon;)I')
    can_attack=method('CanAttack','(II)Z')
    ready=method('cojvrBoxingOwner','()I')
    native_id=p.ref('Weapon','GetThisID','()I')
    get_class=p.ref('java/lang/Object','getClass','()Ljava/lang/Class;')
    get_name=p.ref('java/lang/Class','getName','()Ljava/lang/String;')
    equals=p.ref('java/lang/String','equals','(Ljava/lang/Object;)Z')
    hands_name=p.append(b'\x08'+u2(p.utf8('WeaponHands')))
    c=Code()
    c.emit(0x2a);c.ref(0xb4,punch_weapon);c.branch(0xc7,'no')
    c.ref(0xb8,method('IsNetGame','()Z'));c.branch(0x9a,'no')
    for n in ('IsNotAlive','IsCarrying'):
        c.emit(0x2a);c.ref(0xb6,method(n,'()Z'));c.branch(0x9a,'no')
    c.emit(0x2a);c.ref(0xb6,method('GetNetIsOwner','()Z'));c.branch(0x99,'no')
    c.emit(0x2a);c.ref(0xb4,p.ref(owner,'m_Module','LLawmanModule;',9));c.emit(0x4c)
    c.emit(0x2b);c.ref(0xc1,p.cls('LawmanModuleSingle'));c.branch(0x99,'no')
    for n in ('IsTimerFreezed','IsAnyMoviePlaying'):
        c.emit(0x2b);c.ref(0xc0,p.cls('LawmanModuleSingle'))
        c.ref(0xb6,p.ref('LawmanModuleSingle',n,'()Z'));c.branch(0x9a,'no')
    c.emit(0x2a);c.ref(0xb6,boxing_hand);c.emit(0x3d)
    c.emit(0x1c);c.branch(0x9b,'no');c.emit(0x1c,0x04);c.branch(0xa3,'no')
    c.emit(0x2a,0x1c);c.ref(0xb6,actual);c.emit(0x4e)
    c.emit(0x2d);c.branch(0xc6,'no')
    c.emit(0x2d);c.ref(0xb6,get_class);c.ref(0xb6,get_name);c.ref(0x13,hands_name)
    c.ref(0xb6,equals);c.branch(0x99,'no')
    c.emit(0x2d);c.ref(0xb4,p.ref('Weapon','cOwner','LPawnInventory;',9));c.emit(0x2a);c.branch(0xa6,'no')
    c.emit(0x2a,0x1c);c.ref(0xb6,desired);c.emit(0x2d);c.branch(0xa6,'no')
    # Shared fists cannot claim a hand carrying/equipping another weapon.
    for index,read in enumerate((actual,desired)):
        c.emit(0x2a,0x04,0x1c,0x64);c.ref(0xb6,read);c.emit(0x3a,4)
        c.emit(0x19,4);c.branch(0xc6,'other_ok'+str(index))
        c.emit(0x19,4,0x2d);c.branch(0xa6,'no')
        c.label('other_ok'+str(index))
    c.emit(0x2a,0x2d);c.ref(0xb6,get_move);c.emit(0x36,4)
    for n in ('GetHandStateMashineState','GetHandStateMashineDestinyState','GetDesiredWeaponState'):
        c.emit(0x2a,0x1c);c.ref(0xb6,method(n,'(I)I'));c.emit(0x15,4);c.branch(0xa0,'no')
    c.emit(0x2d);c.ref(0xb6,native_id);c.emit(0xac)
    c.label('no');c.emit(0x03,0xac)
    p.helper('cojvrBoxingOwner','()I',c.finish(),5)

    # Identity is independent of optional diagnostic counters. Reuse exact
    # Hands admission and confirm the native slot rather than the physical fist.
    c=Code();c.emit(0x1b);c.branch(0x99,'no')
    c.emit(0x1b,0x02);c.branch(0x9f,'no')
    c.emit(0x2a);c.ref(0xb6,ready);c.emit(0x1b);c.branch(0xa0,'no')
    c.emit(0x2a);c.ref(0xb6,boxing_hand);c.emit(0x3d)
    c.emit(0x1c);c.branch(0x9b,'no');c.emit(0x1c,0x04);c.branch(0xa3,'no')
    c.emit(0x2a,0x1c);c.ref(0xb6,actual);c.emit(0x4e)
    c.emit(0x2d);c.branch(0xc6,'no')
    c.emit(0x2d);c.ref(0xb6,native_id);c.emit(0x1b);c.branch(0xa0,'no')
    c.emit(0x2a);c.ref(0xb6,ready);c.emit(0x1b);c.branch(0xa0,'no')
    c.emit(0x2a);c.ref(0xb6,boxing_hand);c.emit(0x1c);c.branch(0xa0,'no')
    c.emit(0x1c,0xac);c.label('no');c.emit(0x02,0xac)
    p.helper('cojvrPhysicalPunchNativeHand','(I)I',c.finish(),4)

    # Synchronous attempt: the physical hand is independent of the hand that
    # owns the shared WeaponHands. Native logic selects its original jab mode.
    c=Code()
    c.emit(0x1b);c.branch(0x9b,'no');c.emit(0x1b,0x04);c.branch(0xa3,'no')
    c.emit(0x2d);c.branch(0xc6,'no')
    c.emit(0x2d,0xbe);c.branch(0x99,'no');c.emit(0x2d,0xbe,0x10,63);c.branch(0xa3,'no')
    c.emit(0x2d,0x3a,10,0x03,0x36,11)
    c.label('validate')
    c.emit(0x15,11,0x19,10,0xbe);c.branch(0xa2,'validated')
    c.emit(0x19,10,0x15,11,0x32);c.branch(0xc6,'no')
    c.emit(0x19,10,0x15,11,0x32,0xbe,0x05);c.branch(0xa0,'no')
    for point in (0x03,0x04):
        c.emit(0x19,10,0x15,11,0x32,point,0x32);c.branch(0xc6,'no')
    c.emit(0x84,11,1);c.branch(0xa7,'validate')
    c.label('validated')
    c.emit(0x19,10,0x03,0x32,0x03,0x32,0x4e)
    # First tangent defines only native auxiliary origin/basis. Collision uses
    # every world segment, including vertical motion and returning curves.
    c.emit(0x19,10,0x03,0x32,0x04,0x32,0x3a,4)
    c.emit(0x1c);c.branch(0x99,'no')
    c.emit(0x1c,0x02);c.branch(0x9f,'no')
    c.emit(0x2a);c.ref(0xb6,ready);c.emit(0x1c);c.branch(0xa0,'no')
    c.emit(0x2a);c.ref(0xb6,boxing_hand);c.emit(0x36,5)
    c.emit(0x2a,0x15,5,0x1b);c.ref(0xb6,can_attack);c.branch(0x99,'no')
    c.emit(0x2a,0x15,5);c.ref(0xb6,actual);c.ref(0xc0,p.cls('WeaponMelee'));c.emit(0x3a,6)
    c.emit(0x19,6);c.ref(0xb6,native_id);c.emit(0x1c);c.branch(0xa0,'no')
    # A nested attempt cannot replace the outer sweep, even during callbacks.
    ps=p.ref('WeaponMelee','cojvrPunchStart','LVector;',9)
    pe=p.ref('WeaponMelee','cojvrPunchEnd','LVector;',9)
    pt=p.ref('WeaponMelee','cojvrPunchTraces','[[LVector;',9)
    for f in (ps,pe,pt):c.emit(0x19,6);c.ref(0xb4,f);c.branch(0xc7,'no')
    c.emit(0x19,6,0x2a,0x15,5,0x1b)
    c.ref(0xb6,p.ref('Weapon','GetAttackAlternate','(LPawnInventory;II)Z'));c.emit(0x36,7)
    c.emit(0x19,6,0x2a,0x15,7,0x15,5,0x1b)
    c.ref(0xb6,p.ref('Weapon','GetAttackMode','(LPawnInventory;ZII)I'));c.emit(0x36,8)
    c.emit(0x19,6,0x15,7,0x15,8,0x03)
    c.ref(0xb6,p.ref('Weapon','CanAttack','(ZIZ)Z'));c.branch(0x99,'no')
    # Parameter construction/readback may call into native code. Recheck the
    # player/weapon admission immediately before publishing the owned scope.
    c.emit(0x2a);c.ref(0xb6,ready);c.emit(0x1c);c.branch(0xa0,'no')
    c.emit(0x2a,0x15,5,0x1b);c.ref(0xb6,can_attack);c.branch(0x99,'no')
    # Catch all Java exceptions only to clear owned transient endpoints, then
    # rethrow unchanged. Never suppress a native damage/callback failure.
    start_pc=len(c.data)
    c.emit(0x2a,0x15,7);c.ref(0xb5,punch_alt)
    c.emit(0x2a,0x15,8);c.ref(0xb5,punch_mode)
    c.emit(0x2a,0x19,6);c.ref(0xb5,punch_weapon)
    c.emit(0x19,6,0x2d);c.ref(0xb5,ps)
    c.emit(0x19,6,0x19,4);c.ref(0xb5,pe)
    c.emit(0x19,6,0x19,10);c.ref(0xb5,pt)
    c.emit(0x19,6,0x15,7,0x15,8,0x03);c.ref(0xb6,p.ref('Weapon','Attack','(ZIZ)V'))
    end_pc=len(c.data)
    def clear():
        for f in (ps,pe,pt):c.emit(0x19,6,0x01);c.ref(0xb5,f)
        c.emit(0x2a,0x01);c.ref(0xb5,punch_weapon)
        for f in (punch_alt,punch_mode):c.emit(0x2a,0x03);c.ref(0xb5,f)
    clear();c.emit(0x04,0xac)
    c.label('no');c.emit(0x03,0xac)
    handler=len(c.data);c.emit(0x3a,9);clear();c.emit(0x19,9,0xbf)
    scoped_helper(p,'cojvrPhysicalPunch','(II[[LVector;)Z',c,12,start_pc,end_pc,handler)

    # Contact bouts use only the current independent world-space endpoint pairs.
    # The native query writes its end Vector (CE db69, RVA b3a80 -> aa810), so
    # private clones are mandatory even for a query that never attacks.
    busy=p.field('cojvrContactBusy','Z')
    entry=p.ref('WeaponMelee','cojvrPhysicalMeleeEntries','I',9)
    c=Code();c.emit(0x2a);c.ref(0xb4,busy);c.branch(0x9a,'denied')
    c.emit(0x1b);c.branch(0x9b,'denied');c.emit(0x1b,0x04);c.branch(0xa3,'denied')
    c.emit(0x19,4);c.branch(0xc6,'denied')
    c.emit(0x19,4,0xbe);c.branch(0x99,'denied')
    c.emit(0x19,4,0xbe,0x10,63);c.branch(0xa3,'denied')
    c.emit(0x03,0x36,7);c.label('validate_contact')
    c.emit(0x15,7,0x19,4,0xbe);c.branch(0xa2,'contact_validated')
    c.emit(0x19,4,0x15,7,0x32);c.branch(0xc6,'denied')
    c.emit(0x19,4,0x15,7,0x32,0xbe,0x05);c.branch(0xa0,'denied')
    for endpoint in (0x03,0x04):
        c.emit(0x19,4,0x15,7,0x32,endpoint,0x32);c.branch(0xc6,'denied')
    c.emit(0x84,7,1);c.branch(0xa7,'validate_contact')
    c.label('contact_validated')
    c.emit(0x1c);c.branch(0x99,'denied')
    c.emit(0x1c,0x02);c.branch(0x9f,'denied')
    c.emit(0x2a);c.ref(0xb6,ready);c.emit(0x1c);c.branch(0xa0,'denied')
    c.emit(0x2a,0x2a);c.ref(0xb6,boxing_hand);c.ref(0xb6,actual)
    c.ref(0xc0,p.cls('WeaponMelee'));c.emit(0x3a,5)
    c.emit(0x19,5);c.ref(0xb6,native_id);c.emit(0x1c);c.branch(0xa0,'denied')
    # Retain the original owner reference only in this Java stack frame so
    # owner replacement cannot redirect flag cleanup to a different object.
    c.emit(0x19,5);c.ref(0xb4,p.ref('Weapon','cOwner','LPawnInventory;',9));c.emit(0x3a,14)
    c.emit(0x19,14,0x04);c.ref(0xb6,p.ref('PawnInventory','IsSetFlag','(I)Z'));c.emit(0x36,6)
    start_pc=len(c.data)
    c.emit(0x2a,0x04);c.ref(0xb5,busy)
    c.emit(0x19,14,0x04);c.ref(0xb6,p.ref('PawnInventory','ClearFlag','(I)V'))
    c.emit(0x03,0x36,7,0x03,0x36,11)
    c.label('probe')
    c.emit(0x15,7,0x19,4,0xbe);c.branch(0xa2,'probed')
    for endpoint,slot in ((0x03,8),(0x04,9)):
        c.ref(0xbb,p.cls('Vector'));c.emit(0x59,0x19,4,0x15,7,0x32,endpoint,0x32)
        c.ref(0xb7,p.ref('Vector','<init>','(LVector;)V'));c.emit(0x3a,slot)
    c.emit(0x19,5,0x19,8,0x19,9,0x19,5,0x10,9,0x10,25)
    c.ref(0xb6,p.ref('WeaponMelee','TraceForCollisionWithFaceFlags','(LVector;LVector;LControlObject;II)LCollision;'))
    c.branch(0xc6,'next_probe')
    c.emit(0x04,0x36,11);c.branch(0xa7,'probed')
    c.label('next_probe');c.emit(0x84,7,1);c.branch(0xa7,'probe')
    def restore_flag():
        c.emit(0x15,6);c.branch(0x99,'restore_clear_'+str(len(c.data)))
        clear_label=c.fixups[-1][1];done_label='restore_done_'+str(len(c.data))
        c.emit(0x19,14,0x04);c.ref(0xb6,p.ref('PawnInventory','SetFlag','(I)V'));c.branch(0xa7,done_label)
        c.label(clear_label);c.emit(0x19,14,0x04);c.ref(0xb6,p.ref('PawnInventory','ClearFlag','(I)V'))
        c.label(done_label)
    c.label('probed');restore_flag()
    c.emit(0x02,0x36,15)
    c.emit(0x2a);c.ref(0xb6,ready);c.emit(0x1c);c.branch(0xa0,'contact_done')
    c.emit(0x15,11,0x36,15);c.emit(0x15,11);c.branch(0x99,'contact_done')
    c.emit(0x1d);c.branch(0x99,'contact_done')
    c.emit(0x02,0x36,15)
    c.emit(0x2a,0x2a);c.ref(0xb6,boxing_hand);c.emit(0x1b);c.ref(0xb6,can_attack);c.branch(0x99,'contact_done')
    c.emit(0x04,0x36,15)
    c.emit(0x19,5);c.ref(0xb4,entry);c.emit(0x36,12)
    c.emit(0x2a,0x1b,0x1c,0x19,4);c.ref(0xb6,method('cojvrPhysicalPunch','(II[[LVector;)Z'));c.branch(0x99,'attack_not_entered')
    c.emit(0x19,5);c.ref(0xb4,entry);c.emit(0x15,12);c.branch(0x9f,'contact_done')
    c.emit(0x05,0x36,15);c.branch(0xa7,'contact_done')
    c.label('attack_not_entered');c.emit(0x02,0x36,15)
    c.emit(0x2a);c.ref(0xb6,ready);c.emit(0x1c);c.branch(0xa0,'contact_done')
    c.emit(0x2a,0x2a);c.ref(0xb6,boxing_hand);c.emit(0x1b);c.ref(0xb6,can_attack);c.branch(0x99,'contact_done')
    c.emit(0x04,0x36,15)
    c.label('contact_done');end_pc=len(c.data)
    restore_flag();c.emit(0x2a,0x03);c.ref(0xb5,busy);c.emit(0x15,15,0xac)
    c.label('denied');c.emit(0x02,0xac)
    handler=len(c.data);c.emit(0x3a,13);restore_flag()
    c.emit(0x2a,0x03);c.ref(0xb5,busy);c.emit(0x19,13,0xbf)
    scoped_helper(p,'cojvrPhysicalPunchContact','(IIZ[[LVector;)I',c,16,start_pc,end_pc,handler)
    # AI's synchronous boxing reaction sees the exact selected native damage
    # type even though a controller stroke does not enter an animation state.
    c=Code()
    c.emit(0x2a);c.ref(0xb4,punch_weapon);c.branch(0xc6,'native')
    c.emit(0x2a);c.ref(0xb4,punch_weapon)
    c.emit(0x2a);c.ref(0xb4,punch_alt)
    c.emit(0x2a);c.ref(0xb4,punch_mode)
    c.ref(0xb6,p.ref('WeaponMelee','GetAttackMeleeParams','(ZI)LWpnAttackMelee;'))
    c.ref(0xb4,p.ref('WpnAttackMelee','m_eDamageType','I',9));c.emit(0xac)
    c.label('native');punch_type=c.finish()
    # Original GetBoxingPunchType contains a lookupswitch. Its padding and
    # relative destinations remain valid only with a four-byte prefix length.
    punch_type+=b'\x00'*((-len(punch_type))%4)
    p.changes[('GetBoxingPunchType','()I')]=lambda code:punch_type+code

    # The one native loop owns hit de-duplication across ALL physical segments.
    # Shared authored params remain untouched. Unowned melee keeps its array.
    c=Code();c.emit(0x2a);c.ref(0xb4,traces);c.branch(0xc6,'native')
    c.emit(0x2a);c.ref(0xb4,traces);c.emit(0xb0)
    c.label('native');c.emit(0x2b);c.ref(0xb4,m.ref('WpnAttackMelee','m_avTraces','[[LVector;',9));c.emit(0xb0)
    m.helper('cojvrMeleeTraces','(LWpnAttackMelee;)[[LVector;',c.finish(),2)
    trace_array=m.ref('WeaponMelee','cojvrMeleeTraces','(LWpnAttackMelee;)[[LVector;')
    c=Code()
    c.emit(0x2a);c.ref(0xb4,traces);c.branch(0xc6,'native')
    for endpoint,static in ((0x03,'sm_vrStart'),(0x04,'sm_vrEnd')):
        c.ref(0xb2,m.ref('WeaponMelee',static,'LVector;',9));c.emit(0x2a);c.ref(0xb4,traces)
        c.emit(0x15,7,0x32,endpoint,0x32)
        c.ref(0xb6,m.ref('Vector','Set','(LVector;)V'))
    c.branch(0xa7,'collision');c.label('native')
    # Exact original 150..204 transforms authored local-space points. Physical
    # segments are already world-space and jump directly to original collision.
    c.labels['collision']=len(c.data)+54
    override=c.finish()
    def count_scoped(field):
        # No counting for ordinary/native-animation melee. Diagnostic integers
        # retain evidence even when the following native callback throws.
        c=Code();c.emit(0x2a);c.ref(0xb4,traces);c.branch(0xc6,'done')
        c.emit(0x2a,0x59);c.ref(0xb4,field);c.emit(0x04,0x60);c.ref(0xb5,field)
        c.label('done');return c.finish()
    def trace(code):
        expected=b'\xb8'+u2(m.refs['BeingTools.InvTransformCoord(LVector;LVector;LVector;LVector;LVector;LVector;)V'])
        if len(code)!=527 or code[201:204]!=expected or code[134:137]!=b'\xb4'+u2(m.refs['WpnAttackMelee.m_avTraces[[LVector;']):
            raise ValueError('Unexpected exact melee world trace boundary')
        pool=parse_constant_pool(Reader(u2(m.next)+m.data[10:m.cp_end]+m.extra))
        # Original 324 starts the complete native Hit argument construction,
        # after collision admission and native object-list de-duplication.
        expected_hit=b'\xb6'+u2(m.refs['WeaponMelee.Hit(FFILControlObject;ILVector;LVector;LVector;IILWpnFXHit;ZZ)V'])
        if code[386:389]!=expected_hit or code[324]!=0x2a:
            raise ValueError('Unexpected exact melee Hit boundary')
        # The same-signature forwarding helper counts only after every native
        # argument getter completed, without increasing the caller's stack.
        code=code[:386]+b'\xb6'+u2(count_hit)+code[389:]
        code=insert_target_tag(code,150,override,pool,include_target=True)
        code=insert_target_tag(code,134,b'\x2a\x5f',pool,include_target=True)
        code=code[:136]+b'\xb6'+u2(trace_array)+code[139:]
        return count_scoped(melee_entries)+code
    hit_desc='(FFILControlObject;ILVector;LVector;LVector;IILWpnFXHit;ZZ)V'
    c=Code();c.emit(*count_scoped(hit_calls));c.emit(0x2a,0x23,0x24,0x1d,0x19,4,0x15,5)
    for slot in (6,7,8):c.emit(0x19,slot)
    c.emit(0x15,9,0x15,10,0x19,11,0x15,12,0x15,13)
    c.ref(0xb6,m.ref('WeaponMelee','Hit',hit_desc));c.emit(0xb1)
    stacked_helper(m,'cojvrPhysicalHit',hit_desc,c,14,14)
    count_hit=m.ref('WeaponMelee','cojvrPhysicalHit',hit_desc)
    m.changes[('AttackMelee','(LWpnAttackMelee;LVector;LVector;Z)V')]=trace
    c=Code()
    for f in (start,end):c.emit(0x2a);c.ref(0xb4,f);c.branch(0xc6,'native')
    origin=m.ref('WeaponMelee','sm_vrAttackOrigin','LVector;',9)
    direction=m.ref('WeaponMelee','sm_vrAttackDir','LVector;',9)
    for static,f in ((origin,start),(direction,end)):
        c.ref(0xb2,static);c.emit(0x2a);c.ref(0xb4,f);c.ref(0xb6,m.ref('Vector','Set','(LVector;)V'))
    c.ref(0xb2,direction);c.ref(0xb2,origin);c.ref(0xb6,m.ref('Vector','Sub','(LVector;)V'))
    c.emit(0x2a,0x2b);c.ref(0xb2,origin);c.ref(0xb2,direction);c.emit(0x1c)
    c.ref(0xb6,m.ref('WeaponMelee','AttackMelee','(LWpnAttackMelee;LVector;LVector;Z)V'));c.emit(0xb1)
    c.label('native');physical_origin=c.finish()
    m.changes[('AttackMelee','(LWpnAttackMelee;Z)V')]=lambda code:physical_origin+code
    return p.finish(),m.finish()
