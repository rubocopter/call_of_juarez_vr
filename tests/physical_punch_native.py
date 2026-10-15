"""Exercise emitted Java 1.4 boxing code against native permissions/collision stubs."""
import hashlib
import sys
import unittest
import zipfile
import re
import math
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import patch_coj_weapon_consumers as base
import coj_physical_punch as patch
from inspect_java_bytecode import parse_class,Reader,parse_constant_pool,disassemble

def original(name):
    p=Path('C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez/code.pak')
    for q in (p,p.with_name('code.cojvr-backup.pak')):
        if q.exists():
            with zipfile.ZipFile(q) as z:
                if hashlib.sha256(z.read('ArmedPlayerBeing.class')).hexdigest()==base.CLASS_SHA256:
                    return z.read(name+'.class')
    raise unittest.SkipTest('Exact shipped archive unavailable')

def exception_tables(data):
    r=Reader(data);r.read(8);pool=parse_constant_pool(r);r.read(6);r.read(r.u2()*2)
    for _ in range(r.u2()):
        r.read(6)
        for _ in range(r.u2()):r.u2();r.read(r.u4())
    result={}
    for _ in range(r.u2()):
        r.u2();name=pool.utf8(r.u2());desc=pool.utf8(r.u2())
        for _ in range(r.u2()):
            attr=pool.utf8(r.u2());raw=r.read(r.u4())
            if attr=='Code':
                cr=Reader(raw);cr.read(4);cr.read(cr.u4())
                result[(name,desc)]=[tuple(cr.u2() for _ in range(4)) for _ in range(cr.u2())]
    return result

class NativeFailure(Exception):pass
class VM:
    def __init__(self):
        player,self.melee=patch.patch_punch_classes(base.patch_class(original('ArmedPlayerBeing')),original('WeaponMelee'))
        self.pool,methods=parse_class(player);self.methods={(m.name,m.descriptor):m for m in methods}
        self.handlers=exception_tables(player)
        self.melee_pool,melee_methods=parse_class(self.melee)
        self.melee_methods={(m.name,m.descriptor):m for m in melee_methods}
        self.statics={};self.collisions=[];self.hits=[];self.transforms=0
        self.obstacle={'class':'target'}
        self.module={'class':'LawmanModuleSingle','frozen':False,'movie':False}
        self.player={'class':'ArmedPlayerBeing','m_Module':self.module,'dead':False,'carry':False,'net_owner':True,'cojvrContactBusy':False}
        self.weapon={'class':'WeaponHands','cOwner':self.player,'id':17,'cooldown':False}
        self.actual=self.desired=self.weapon;self.other_actual=self.other_desired=None
        self.boxing_hand=0;self.state=self.destiny=self.desired_state=1
        self.net=False;self.permitted=True;self.attacks=[];self.fail_attack=False;self.reenter=False
        self.nested_result=None;self.start={'class':'Vector','xyz':(1,2,3)};self.end={'class':'Vector','xyz':(1,2,-8)}
        self.punch_types=[];self.traces=[[self.start,self.end]]
        self.replace_during_mode=False;self.disable_during_preflight=False
        self.execute_melee=False;self.fail_collision=False;self.fail_hit=False
        self.failure=NativeFailure('native melee callback failure')
        self.owner_flag=True
        self.clamp_probe=False;self.probe_flags=[];self.probe_ignored=[];self.invalidate_probe=False;self.suppress_attack=False
        self.fail_clone=False;self.reenter_probe=False;self.nested_contact=None
        self.fail_damage_getter=False
        self.weapon.update(cojvrPhysicalMeleeEntries=0,cojvrPhysicalNativeHitCalls=0)
    def native(self,target,name,args):
        if name=='IsNetGame':return self.net
        if name=='IsNotAlive':return self.player['dead']
        if name=='IsCarrying':return self.player['carry']
        if name=='GetNetIsOwner':return self.player['net_owner']
        if name=='IsTimerFreezed':return self.module['frozen']
        if name=='IsAnyMoviePlaying':return self.module['movie']
        if name=='GetBoxingHand':return self.boxing_hand
        if name=='GetActualWeapon':return self.actual if args[0]==self.boxing_hand else self.other_actual
        if name=='GetDesiredWeapon':return self.desired if args[0]==self.boxing_hand else self.other_desired
        if name=='GetWeaponMoveState':return 1
        if name=='GetHandStateMashineState':return self.state
        if name=='GetHandStateMashineDestinyState':return self.destiny
        if name=='GetDesiredWeaponState':return self.desired_state
        if name=='GetThisID':return self.weapon['id']
        if name=='getClass':return target['class']
        if name=='getName':return target
        if name=='equals':return target==args[0]
        if name=='CanAttack':
            if target is self.player:return self.permitted
            if self.disable_during_preflight:self.permitted=False
            return not self.weapon['cooldown']
        if name=='GetAttackAlternate':return False
        if name=='GetAttackMode':
            if self.replace_during_mode:self.desired={'class':'WeaponHands'}
            return 0 if args[-1]==1 else 1
        if name=='GetAttackMeleeParams':return {'m_eDamageType':77+args[1]}
        if name=='Attack':
            if self.suppress_attack:return None
            self.punch_types.append(self.run('GetBoxingPunchType','()I'))
            self.attacks.append((args,self.weapon.get('cojvrPunchStart'),self.weapon.get('cojvrPunchEnd'),self.weapon.get('cojvrPunchTraces')))
            if self.reenter:self.nested_result=self.punch()
            if self.fail_attack:raise NativeFailure('original native callback failure')
            self.weapon['cooldown']=True
            if self.execute_melee:
                BoxingTests.prepare_melee(self)
                self.run('AttackMelee','(LWpnAttackMelee;Z)V',[self.params,False],target=self.weapon,melee=True)
            return None
        if name=='AttackMelee':
            return self.run(name,'(LWpnAttackMelee;LVector;LVector;Z)V',args,target=target,melee=True)
        if name=='<init>':
            if isinstance(target,dict) and target.get('class')=='Vector':
                if self.fail_clone:raise self.failure
                target['xyz']=args[0]['xyz']
            return None
        if name=='Set':target['xyz']=args[0]['xyz'];return None
        if name=='Normalize':
            p=target['xyz'];n=math.sqrt(sum(v*v for v in p))
            if n:target['xyz']=tuple(v/n for v in p)
            return None
        if name=='GetLeftVector2D':
            p=args[1]['xyz'];args[0]['xyz']=(-p[2],0,p[0]);return None
        if name=='CalcCross':
            a,b=args[0]['xyz'],args[1]['xyz'];target['xyz']=(a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]);return None
        if name=='Sub':target['xyz']=tuple(a-b for a,b in zip(target['xyz'],args[0]['xyz']));return None
        if name=='SetLen':target['xyz']=tuple(v*args[0] for v in target['xyz']);return None
        if name=='Negate':target['xyz']=tuple(-v for v in target['xyz']);return None
        if name=='IsSetFlag':return self.owner_flag
        if name=='ClearFlag':self.owner_flag=False;return None
        if name=='SetFlag':self.owner_flag=True;return None
        if name in ('AttackNetPre','ODEWalk_AddAdditionalLinVel','MakeNoiseOnPosition'):return None
        if name=='InvTransformCoord':
            self.transforms+=1;dst,src,left,up,forward,pos=args
            dst['xyz']=tuple(pos['xyz'][i]+src['xyz'][0]*left['xyz'][i]+src['xyz'][1]*up['xyz'][i]+src['xyz'][2]*forward['xyz'][i] for i in range(3));return None
        if name=='TraceForCollisionWithFaceFlags':
            self.probe_flags.append(self.owner_flag)
            self.probe_ignored.append(args[2])
            if self.reenter_probe:
                self.reenter_probe=False;self.nested_contact=self.contact()
            if self.fail_collision:raise self.failure
            self.collisions.append((args[0]['xyz'],args[1]['xyz'],args[3:]))
            if self.clamp_probe:args[1]['xyz']=(999,999,999)
            if self.invalidate_probe:self.player['dead']=True
            if self.obstacle is None:return None
            return {'Obstacle':self.obstacle,'ObstacleElement':0,'vNormal':{'xyz':(0,1,0)},'nMaterial':0,'nMaterialFlags':0}
        if name=='IsObjectInArrayList':return any(o is args[0] for o in args[1])
        if name=='add':target.append(args[0]);return True
        if name=='size':return len(target)
        if name=='GetDamage':
            if self.fail_damage_getter:raise self.failure
            return 10.
        if name=='GetPhysicsEffectRatio':return 1.
        if name=='GetFXHit':return None
        if name=='Hit':
            self.hits.append(args)
            if self.fail_hit:raise self.failure
            return None
        if name=='GetSelfDamage':return 0.
        raise AssertionError(name)
    def run(self,name,desc,args=(),target=None,melee=False):
        pool=self.melee_pool if melee else self.pool
        methods=self.melee_methods if melee else self.methods
        method=methods[(name,desc)];code=method.code;loc=[self.player if target is None else target]+list(args)+[None]*method.max_locals
        stack=[];pc=0
        while pc<len(code):
            if len(stack)>method.max_stack:raise AssertionError(f'{name} exceeds declared max_stack')
            at=pc;op=code[pc];pc+=1
            try:
                if 0x2a<=op<=0x2d:stack.append(loc[op-0x2a])
                elif 0x1a<=op<=0x1d:stack.append(loc[op-0x1a])
                elif 0x4b<=op<=0x4e:loc[op-0x4b]=stack.pop()
                elif 0x3b<=op<=0x3e:loc[op-0x3b]=stack.pop()
                elif op in (0x19,0x15):stack.append(loc[code[pc]]);pc+=1
                elif op in (0x3a,0x36,0x38):loc[code[pc]]=stack.pop();pc+=1
                elif op==0x01:stack.append(None)
                elif op==0x32:index=stack.pop();stack.append(stack.pop()[index])
                elif op==0xbe:stack.append(len(stack.pop()))
                elif op==0x59:stack.append(stack[-1])
                elif op==0x57:stack.pop()
                elif op in (0x60,0x62):stack.append(stack.pop()+stack.pop())
                elif op==0x5f:b,a=stack.pop(),stack.pop();stack.extend([b,a])
                elif op==0x17:stack.append(loc[code[pc]]);pc+=1
                elif 0x22<=op<=0x25:stack.append(loc[op-0x22])
                elif 0x0b<=op<=0x0d:stack.append(float(op-0x0b))
                elif op in (0x95,0x96):b,a=stack.pop(),stack.pop();stack.append(0 if a==b else 1 if a>b else -1)
                elif op==0x10:stack.append(int.from_bytes(code[pc:pc+1],'big',signed=True));pc+=1
                elif op==0x84:loc[code[pc]]+=int.from_bytes(code[pc+1:pc+2],'big',signed=True);pc+=2
                elif op==0x64:b,a=stack.pop(),stack.pop();stack.append(a-b)
                elif 0x03<=op<=0x08:stack.append(op-0x03)
                elif op==0x02:stack.append(-1)
                elif op==0x13:
                    idx=int.from_bytes(code[pc:pc+2],'big');pc+=2;stack.append(pool.utf8(pool.get(idx)[1]))
                elif op==0xbb:
                    idx=int.from_bytes(code[pc:pc+2],'big');pc+=2;name_=pool.describe(idx)
                    assert name_ in ('java/util/ArrayList','Vector');stack.append([] if name_=='java/util/ArrayList' else {'class':'Vector'})
                elif op in (0xb2,0xb4,0xb5,0xb6,0xb7,0xb8,0xc0,0xc1):
                    idx=int.from_bytes(code[pc:pc+2],'big');pc+=2;entry=pool.get(idx)
                    if op==0xc0:continue
                    if op==0xc1:
                        a=stack.pop();stack.append(a is not None and a['class']==pool.describe(idx));continue
                    nt=pool.get(entry[2]);key=pool.utf8(nt[1]);signature=pool.utf8(nt[2])
                    if op==0xb2:stack.append(self.statics.setdefault(key,{'xyz':(0,0,0)}));continue
                    if op==0xb4:
                        o=stack.pop();stack.append(o['xyz'][1] if key=='fY' else o.get(key));continue
                    if op==0xb5:
                        value=stack.pop();o=stack.pop()
                        if key=='fY':o['xyz']=(o['xyz'][0],value,o['xyz'][2])
                        else:o[key]=value
                        continue
                    types=re.findall(r'L[^;]+;|[IZF]',signature[1:signature.index(')')])
                    argv=[stack.pop() for _ in types][::-1];target=None if op==0xb8 else stack.pop()
                    result=self.run(key,signature,argv,target,melee=pool.describe(entry[1])=='WeaponMelee') if key.startswith('cojvr') else self.native(target,key,argv)
                    if signature[-1]!='V':stack.append(result)
                elif op in (0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,0xa0,0xa2,0xa3,0xa6,0xa7,0xc6,0xc7):
                    delta=int.from_bytes(code[pc:pc+2],'big',signed=True);pc+=2
                    if op==0xa7:take=True
                    elif op in (0x9f,0xa0,0xa2,0xa3,0xa6):
                        b,a=stack.pop(),stack.pop();take=(a==b if op==0x9f else a!=b if op==0xa0 else a>=b if op==0xa2 else a>b if op==0xa3 else a is not b)
                    else:
                        a=stack.pop();take=(a is None if op==0xc6 else a is not None if op==0xc7 else
                            a==0 if op==0x99 else a!=0 if op==0x9a else a<0 if op==0x9b else a>=0 if op==0x9c else a>0 if op==0x9d else a<=0)
                    if take:pc=at+delta
                elif op in (0xac,0xb0):return stack.pop()
                elif op==0xb1:return None
                elif op==0xbf:raise stack.pop()
                else:raise AssertionError(f'{op:02x} {at}')
            except NativeFailure as error:
                handler=next((h for start,end,h,kind in self.handlers.get((name,desc),[]) if not melee and start<=at<end),None)
                if handler is None:raise
                stack=[error];pc=handler
        raise AssertionError('no return')
    def ready(self):return self.run('cojvrBoxingOwner','()I')
    def punch(self,hand=0,owner=17,start=True):
        return self.run('cojvrPhysicalPunch','(II[[LVector;)Z',[hand,owner,self.traces if start else None])
    def contact(self,hand=0,owner=17,allow=True):
        self.assert_contact_helper()
        return self.run('cojvrPhysicalPunchContact','(IIZ[[LVector;)I',[hand,owner,allow,self.traces])
    def assert_contact_helper(self):
        if ('cojvrPhysicalPunchContact','(IIZ[[LVector;)I') not in self.methods:
            raise AssertionError('missing contact-time helper')

class BoxingTests(unittest.TestCase):
    def test_contact_probe_clear_has_no_attack_cooldown_or_native_entry(self):
        for flag in (False,True):
            vm=VM();vm.obstacle=None;vm.owner_flag=flag
            self.assertEqual(vm.contact(),0)
            self.assertEqual(vm.attacks,[]);self.assertFalse(vm.weapon['cooldown'])
            self.assertEqual(vm.weapon['cojvrPhysicalMeleeEntries'],0)
            self.assertEqual(vm.owner_flag,flag);self.assertEqual(vm.probe_flags,[False])
    def test_contact_probe_clones_endpoints_and_latch_queries_do_not_attack(self):
        vm=VM();vm.clamp_probe=True;before=(vm.start['xyz'],vm.end['xyz'])
        self.assertEqual(vm.contact(allow=False),1)
        self.assertEqual((vm.start['xyz'],vm.end['xyz']),before)
        self.assertEqual(vm.attacks,[]);self.assertTrue(vm.owner_flag)
        self.assertEqual(vm.collisions[0][2],[9,25])
        self.assertIs(vm.probe_ignored[0],vm.weapon,'native ignore object is the weapon, owner uses scoped flag')
    def test_contact_bundle_does_not_connect_independent_rows_or_mutate_input_on_miss(self):
        vm=VM();vm.obstacle=None;vm.clamp_probe=True
        vm.traces=[[{'xyz':(0,0,0)},{'xyz':(0,0,.5)}],
                   [{'xyz':(100,0,0)},{'xyz':(100,0,0)}]]
        self.assertEqual(vm.contact(allow=False),0)
        self.assertEqual([(a,b) for a,b,_ in vm.collisions],
            [((0,0,0),(0,0,.5)),((100,0,0),(100,0,0))])
        self.assertEqual(vm.traces[0][1]['xyz'],(0,0,.5))
        self.assertEqual(vm.traces[1][1]['xyz'],(100,0,0))
        self.assertEqual(vm.attacks,[])
    def test_contact_attack_once_with_native_entry_and_cooldown_still_reports_contact(self):
        for native_hand in (0,1):
            for physical_hand in (0,1):
                vm=VM();vm.boxing_hand=native_hand;vm.execute_melee=True
                self.assertEqual(vm.contact(hand=physical_hand),2)
                self.assertEqual(len(vm.attacks),1);self.assertEqual(len(vm.hits),1)
                self.assertTrue(vm.owner_flag)
                self.assertEqual(vm.contact(hand=physical_hand),1)
                self.assertEqual(len(vm.attacks),1)
        vm=VM();vm.suppress_attack=True
        self.assertEqual(vm.contact(),1,'void Attack returning without melee entry is not accepted')
    def test_contact_probe_and_attack_throws_restore_saved_flags_and_owned_scope(self):
        for flag in (False,True):
            for failure in ('fail_clone','fail_collision','fail_hit'):
                vm=VM();vm.owner_flag=flag;vm.execute_melee=True;setattr(vm,failure,True)
                with self.assertRaises(NativeFailure) as raised:vm.contact()
                self.assertIs(raised.exception,vm.failure)
                self.assertEqual(vm.owner_flag,flag)
                self.assertFalse(vm.player.get('cojvrContactBusy',False))
                for field in ('cojvrPunchStart','cojvrPunchEnd','cojvrPunchTraces'):
                    self.assertIsNone(vm.weapon.get(field))
                self.assertIsNone(vm.player.get('cojvrPunchWeapon'))
    def test_contact_owner_loss_reentry_and_malformed_bundles_fail_closed(self):
        vm=VM();vm.invalidate_probe=True
        self.assertEqual(vm.contact(),-1);self.assertEqual(vm.attacks,[]);self.assertTrue(vm.owner_flag)
        vm=VM();vm.reenter_probe=True
        self.assertEqual(vm.contact(allow=False),1);self.assertEqual(vm.nested_contact,-1)
        for rows in (None,[],[None],[[None,None]],[[{'xyz':(0,0,0)}]],[[{'xyz':(0,0,0)},{'xyz':(0,1,0)}]]*64):
            vm=VM();vm.traces=rows
            self.assertEqual(vm.contact(),-1);self.assertEqual(vm.collisions,[])
    def test_contact_permissions_remain_unavailable_and_cooldown_is_contact(self):
        vm=VM();vm.permitted=False
        self.assertEqual(vm.contact(),-1);self.assertEqual(vm.attacks,[])
        vm=VM();vm.weapon['cooldown']=True
        self.assertEqual(vm.contact(),1);self.assertEqual(vm.attacks,[])
        vm=VM();vm.replace_during_mode=True
        self.assertEqual(vm.contact(),-1);self.assertEqual(vm.attacks,[])
        vm=VM();vm.execute_melee=True;vm.fail_damage_getter=True
        with self.assertRaises(NativeFailure):vm.contact()
        self.assertEqual(vm.weapon['cojvrPhysicalNativeHitCalls'],0,'throwing argument getter is not a Hit call')
        self.assertTrue(vm.owner_flag)
    def test_exact_native_hand_resolution_does_not_need_counter_fields(self):
        for holding in (0,1):
            vm=VM();vm.boxing_hand=holding
            del vm.weapon['cojvrPhysicalMeleeEntries'];del vm.weapon['cojvrPhysicalNativeHitCalls']
            self.assertTrue(('cojvrPhysicalPunchNativeHand','(I)I') in vm.methods,'missing exact native-hand observer')
            self.assertEqual(vm.run('cojvrPhysicalPunchNativeHand','(I)I',[17]),holding)
            for owner in (0,-1,18):self.assertEqual(vm.run('cojvrPhysicalPunchNativeHand','(I)I',[owner]),-1)
            vm.weapon['class']='WeaponHandsOld'
            self.assertEqual(vm.run('cojvrPhysicalPunchNativeHand','(I)I',[17]),-1)
    def test_scoped_native_entry_and_deduplicated_hit_calls_are_separate_from_attempts(self):
        for obstacle,expected_hits in ((None,0),({'class':'target'},1)):
            vm=VM();vm.execute_melee=True;vm.obstacle=obstacle
            corner={'xyz':(5,4,-2)};vm.traces=[[vm.start,corner],[corner,vm.end]]
            self.assertTrue(vm.punch())
            self.assertEqual(vm.weapon['cojvrPhysicalMeleeEntries'],1)
            self.assertEqual(vm.weapon['cojvrPhysicalNativeHitCalls'],expected_hits)
            self.assertEqual(len(vm.collisions),2)
            self.assertTrue(vm.owner_flag)
            self.assertTrue(all(flags==[9,25] for _,_,flags in vm.collisions))
            self.assertFalse(vm.punch())
            self.assertEqual(vm.weapon['cojvrPhysicalMeleeEntries'],1,'cooldown rejection cannot increment entries')
    def test_native_collision_and_hit_exceptions_rethrow_unchanged_and_clear_punch_scope(self):
        for failure,expected_hits in (('fail_collision',0),('fail_hit',1)):
            vm=VM();vm.execute_melee=True;setattr(vm,failure,True)
            with self.assertRaises(NativeFailure) as raised:vm.punch()
            self.assertIs(raised.exception,vm.failure)
            self.assertEqual(vm.weapon['cojvrPhysicalMeleeEntries'],1)
            self.assertEqual(vm.weapon['cojvrPhysicalNativeHitCalls'],expected_hits)
            for field in ('cojvrPunchStart','cojvrPunchEnd','cojvrPunchTraces'):
                self.assertIsNone(vm.weapon[field])
            self.assertIsNone(vm.player['cojvrPunchWeapon'])
            self.assertEqual((vm.player['cojvrPunchAlternate'],vm.player['cojvrPunchMode']),(0,0))
    def test_unowned_native_melee_does_not_count_as_physical_punch(self):
        vm=VM();self.run_melee(vm,None)
        self.assertEqual((vm.weapon['cojvrPhysicalMeleeEntries'],vm.weapon['cojvrPhysicalNativeHitCalls']),(0,0))
    def test_hit_counter_forwarder_respects_original_operand_stack_limit(self):
        vm=VM();desc='(LWpnAttackMelee;LVector;LVector;Z)V'
        caller=vm.melee_methods[('AttackMelee',desc)]
        forward=next(m for (name,_),m in vm.melee_methods.items() if name=='cojvrPhysicalHit')
        self.assertEqual(caller.max_stack,14,'do not push an extra receiver above the original peak')
        self.assertGreaterEqual(forward.max_stack,14,'forward all native Hit operands within declared stack')
        self.assertGreaterEqual(forward.max_locals,14,'retain receiver and all native Hit arguments')
        vm.execute_melee=True;self.assertTrue(vm.punch())
        self.assertEqual(vm.hits[0][0],10.)
        self.assertEqual(vm.hits[0][2],77)
        self.assertEqual(vm.hits[0][-2:],[1,1])
    def test_both_physical_hands_deliver_scoped_sweeps_to_native_attack(self):
        for holding in (0,1):
            for hand in (0,1):
                vm=VM();vm.boxing_hand=holding;self.assertEqual(vm.ready(),17)
                self.assertTrue(vm.punch(hand));self.assertEqual(len(vm.attacks),1)
                args,start,end,traces=vm.attacks[0]
                self.assertEqual(args,[False,1 if hand==0 else 0,False])
                self.assertEqual(vm.punch_types,[77+(1 if hand==0 else 0)])
                self.assertIs(start,vm.start);self.assertIs(end,vm.end)
                self.assertIs(traces,vm.traces);self.assertIsNone(vm.weapon['cojvrPunchTraces'])
                self.assertIsNone(vm.weapon['cojvrPunchStart']);self.assertIsNone(vm.weapon['cojvrPunchEnd'])
                self.assertIsNone(vm.player['cojvrPunchWeapon'])
                self.assertEqual((vm.player['cojvrPunchAlternate'],vm.player['cojvrPunchMode']),(0,0))
                self.assertFalse(vm.punch(hand),'native cooldown still rejects')
    def test_cancellation_permissions_and_owner_are_rechecked_at_attempt(self):
        faults=['dead','carry','net','net_owner','movie','frozen','actual','desired','owner','state','destiny','desired_state','class','module','permission','other_actual','other_desired']
        for fault in faults:
            with self.subTest(fault=fault):
                vm=VM();self.assertEqual(vm.ready(),17)
                if fault in ('dead','carry'):vm.player[fault]=True
                elif fault=='net_owner':vm.player[fault]=False
                elif fault in ('frozen','movie'):vm.module[fault]=True
                elif fault=='actual':vm.actual=None
                elif fault=='desired':vm.desired={'class':'WeaponHands'}
                elif fault=='owner':vm.weapon['cOwner']={}
                elif fault=='class':vm.weapon['class']='WeaponHandsOld'
                elif fault=='module':vm.module['class']='LawmanModuleNet'
                elif fault=='permission':vm.permitted=False
                elif fault in ('other_actual','other_desired'):setattr(vm,fault,{'class':'WeaponPistolPeacemaker'})
                elif fault=='net':vm.net=True
                else:setattr(vm,fault,21)
                self.assertFalse(vm.punch());self.assertEqual(vm.attacks,[])
        for hand,owner,start in ((-1,17,True),(2,17,True),(0,0,True),(0,18,True),(0,17,False)):
            vm=VM();self.assertFalse(vm.punch(hand,owner,start));self.assertEqual(vm.attacks,[])
    def test_native_exception_restores_scope_and_rethrows_same_failure(self):
        vm=VM();vm.fail_attack=True
        with self.assertRaisesRegex(NativeFailure,'original native callback failure'):vm.punch()
        self.assertIsNone(vm.weapon['cojvrPunchStart']);self.assertIsNone(vm.weapon['cojvrPunchEnd'])
        self.assertIsNone(vm.player['cojvrPunchWeapon'])
    def test_reentrant_callback_cannot_overwrite_outer_sweep(self):
        vm=VM();vm.reenter=True;self.assertTrue(vm.punch());self.assertFalse(vm.nested_result)
        self.assertEqual(len(vm.attacks),1)
    def test_class_parameter_reads_cannot_leave_stale_player_permission_or_owner(self):
        for fault in ('replace_during_mode','disable_during_preflight'):
            vm=VM();setattr(vm,fault,True);self.assertFalse(vm.punch());self.assertEqual(vm.attacks,[])
    def test_curve_is_owned_as_ordered_segments_and_rejected_when_malformed(self):
        vm=VM();corner={'class':'Vector','xyz':(5,4,-2)}
        vm.traces=[[vm.start,corner],[corner,vm.end]]
        self.assertTrue(vm.punch());self.assertIs(vm.attacks[0][3],vm.traces)
        self.assertIsNone(vm.weapon['cojvrPunchTraces'])
        for path in ([],[None],[[vm.start]],[[None,vm.end]],[[vm.start,None]],[[vm.start,vm.end]]*64):
            vm=VM();vm.traces=path;self.assertFalse(vm.punch());self.assertEqual(vm.attacks,[])
    def test_exact_melee_trace_is_installed_without_changing_damage(self):
        player,melee=patch.patch_punch_classes(base.patch_class(original('ArmedPlayerBeing')),original('WeaponMelee'))
        pool,methods=parse_class(melee)
        m=next(m for m in methods if m.name=='AttackMelee' and m.descriptor=='(LWpnAttackMelee;LVector;LVector;Z)V')
        lines='\n'.join(disassemble(m.code,pool))
        self.assertIn('cojvrPunchTraces',lines)
        self.assertIn('cojvrMeleeTraces',lines)
        self.assertIn('TraceForCollisionWithFaceFlags',lines)
        self.assertIn('WpnAttackMelee.GetDamage',lines)
        self.assertIn('WeaponMelee.cojvrPhysicalHit',lines)
        forwarded=next(m for m in methods if m.name=='cojvrPhysicalHit')
        self.assertIn('WeaponMelee.Hit','\n'.join(disassemble(forwarded.code,pool)))
        self.assertIn('cojvrPhysicalPunch',str([m.name for m in parse_class(player)[1]]))
    def test_full_native_loop_consumes_curve_segments_once_per_target_without_transform_or_chord(self):
        vm=VM();corner={'xyz':(5,4,-2)};second={'xyz':(4,3,-5)}
        traces=[[vm.start,corner],[corner,second],[second,vm.end]]
        self.run_melee(vm,traces)
        self.assertEqual([(a,b) for a,b,flags in vm.collisions],[(a['xyz'],b['xyz']) for a,b in traces])
        self.assertEqual(vm.transforms,0,'world samples must bypass local authored transforms')
        self.assertEqual(len(vm.hits),1,'native list deduplicates repeated target contacts across one stroke')
        self.assertEqual(vm.hits[0][0],10.,'native damage readback stays authoritative')
    @staticmethod
    def prepare_melee(vm):
        for name in ('Position','Forward','Left','Up'):vm.weapon['m_vLastAttack'+name]={'xyz':(0,0,0)}
        vm.params={'m_avTraces':[[vm.start,vm.end]],'m_fDirY':.2,'m_fSpeed':12.,'m_eDamageType':77,'m_fHitNoise':0.,'m_eHitNoiseType':0}
    @staticmethod
    def run_melee(vm,traces):
        BoxingTests.prepare_melee(vm);vm.weapon['cojvrPunchTraces']=traces
        params=vm.params;native=params['m_avTraces']
        vm.run('AttackMelee','(LWpnAttackMelee;LVector;LVector;Z)V',
            [params,{'xyz':(0,0,0)},{'xyz':(0,0,1)},False],target=vm.weapon,melee=True)
        if params['m_avTraces'] is not native:raise AssertionError('shared native params modified')
    def test_unowned_native_melee_fallback_and_original_bytecode_are_preserved(self):
        vm=VM();self.run_melee(vm,None)
        self.assertEqual(vm.transforms,2);self.assertEqual(len(vm.collisions),1)
        self.assertEqual(len(vm.hits),1)
        pool,methods=parse_class(vm.melee)
        original_methods={(m.name,m.descriptor):m.code for m in parse_class(original('WeaponMelee'))[1]}
        for m in methods:
            if m.name!='AttackMelee' and not m.name.startswith('cojvr'):self.assertEqual(m.code,original_methods[(m.name,m.descriptor)])
        pool,player_methods=parse_class(patch.patch_punch_classes(base.patch_class(original('ArmedPlayerBeing')),original('WeaponMelee'))[0])
        original_type=next(m.code for m in parse_class(original('ArmedPlayerBeing'))[1] if m.name=='GetBoxingPunchType')
        patched_type=next(m.code for m in player_methods if m.name=='GetBoxingPunchType')
        self.assertEqual(patched_type[-len(original_type):],original_type)
        self.assertEqual((len(patched_type)-len(original_type))%4,0)
    def test_unknown_native_class_fails_closed(self):
        with self.assertRaises(ValueError):patch.patch_punch_classes(base.patch_class(original('ArmedPlayerBeing')),b'unknown')

if __name__=='__main__':unittest.main()
