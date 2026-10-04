"""Execute the injected bytecode paths without invoking game/JVM state."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import patch_coj_weapon_consumers as patch
from patch_coj_weapon_consumers import prefix, patch_class

def execute_effects(code, owner, definitions=None, visual=False, hand=0, fail_frame=False, fail_create=False,
                    fail_detach=False, require_current_frame=False, diagnostics=None):
    # Execute emitted JVM instructions and record world emitter arguments.
    definitions = definitions or {20: 101, 21: 102, 22: 201, 23: 202}
    weapon = diagnostics if diagnostics is not None else {}
    weapon[10]=owner;weapon.setdefault(30,0);weapon.setdefault(31,0)
    locals_ = [owner if visual else weapon, hand if visual else definitions, None, None, None, None, None, None, None]
    stack=[]; pc=0; effects=[]; committed=set()
    while pc < len(code):
        at=pc; op=code[pc]; pc+=1
        if 0x2a <= op <= 0x2d: stack.append(locals_[op-0x2a])
        elif 0x1a <= op <= 0x1d: stack.append(locals_[op-0x1a])
        elif 0x4b <= op <= 0x4e: locals_[op-0x4b]=stack.pop()
        elif 0x3b <= op <= 0x3e: locals_[op-0x3b]=stack.pop()
        elif op in (0x19,0x15): stack.append(locals_[code[pc]]);pc+=1
        elif op in (0x3a,0x36): locals_[code[pc]]=stack.pop();pc+=1
        elif op == 0x01: stack.append(None)
        elif op == 0x03: stack.append(0)
        elif op == 0x02: stack.append(-1)
        elif op == 0x04: stack.append(1)
        elif op == 0x32:
            index=stack.pop();array=stack.pop()
            if index<0 or index>=len(array): raise IndexError(index)
            stack.append(array[index])
        elif op in (0x10,0x11):
            size=1 if op==0x10 else 2
            stack.append(int.from_bytes(code[pc:pc+size],'big',signed=True));pc+=size
        elif op==0x60: stack.append(stack.pop()+stack.pop())
        elif op==0x80: stack.append(stack.pop()|stack.pop())
        elif op in (0xb4,0xb5,0xb6,0xc0,0xc1):
            index=int.from_bytes(code[pc:pc+2],'big');pc+=2
            if op==0xb4: stack.append(stack.pop()[index])
            elif op==0xb5:
                value=stack.pop();stack.pop()[index]=value
            elif op==0xc0: pass
            elif op==0xc1: stack.append(isinstance(stack.pop(),dict) and owner.get('tracked_player',False))
            elif index==12: stack.pop();stack.pop();stack.append(hand)
            elif index==24:
                args=[stack.pop() for _ in range(6)][::-1];stack.pop();
                if not visual: assert args[3] is None, "World creation must not mistake aim for emitter +Y"
                effects.append(None if fail_create else args);stack.append(0 if fail_create else len(effects))
            elif index==25:
                forward=stack.pop();up=stack.pop();position=stack.pop();effect=stack.pop();stack.pop()
                effects[effect-1][2]=position;effects[effect-1][3]=up;effects[effect-1].append(forward);stack.append(not fail_frame)
            elif index==26:
                effect=stack.pop();stack.pop();effects[effect-1]=None
            elif index==27:
                effect=stack.pop();stack.pop()
                if not fail_detach: committed.add(effect-1)
                stack.append(not fail_detach)
            else: raise AssertionError(index)
        elif op==0x57: stack.pop()
        elif op in (0xc6,0x99,0x9a,0xa0,0xa7):
            delta=int.from_bytes(code[pc:pc+2],'big',signed=True);pc+=2
            if op==0xa7: take=True
            elif op==0xa0: take=stack.pop()!=stack.pop()
            else:
                value=stack.pop();take=(value is None if op==0xc6 else value==0 if op==0x99 else value!=0)
            if take: pc=at+delta
        elif op==0xb0: return stack.pop()
        elif op==0xb1:
            if require_current_frame:
                assert all(i in committed for i,effect in enumerate(effects) if effect is not None), \
                    'Emitter starting frame never reached the active world transform'
            return [effect for effect in effects if effect is not None]
        else: raise AssertionError(hex(op))
    if stack: raise AssertionError('Fallback leaked stack operands')
    return 'original'

def execute(code, hand, out, net=False, right=None, left=None, weapon=False):
    fields = {1:right, 2:left, 3:net}; obj = fields
    locals_ = [obj, hand if not weapon else "weapon", out, None, None]
    stack=[]; pc=0
    while pc < len(code):
        at=pc; op=code[pc]; pc+=1
        if 0x2a <= op <= 0x2d: stack.append(locals_[op-0x2a])
        elif op in (0x1b, 0x1d): stack.append(locals_[op-0x1a])
        elif op == 0x04: stack.append(1)
        elif op == 0x3e: locals_[3]=stack.pop()
        elif op == 0x4e: locals_[3]=stack.pop()
        elif op == 0x3a: locals_[code[pc]]=stack.pop(); pc+=1
        elif op == 0x19: stack.append(locals_[code[pc]]); pc+=1
        elif op in (0xb4,0xb6):
            index=int.from_bytes(code[pc:pc+2],"big"); pc+=2
            if op==0xb4: stack.append(stack.pop()[index])
            elif index==4: stack.pop(); stack.pop(); stack.append(hand)
            elif index==5:
                value=stack.pop(); dest=stack.pop(); dest[:] = value; stack.append(True)
            else: raise AssertionError(index)
        elif op in (0xc6,0x99,0x9a,0xa0,0xa7):
            delta=int.from_bytes(code[pc:pc+2],"big",signed=True); pc+=2
            if op==0xa7: take=True
            elif op==0xa0: take=stack.pop()!=stack.pop()
            else:
                value=stack.pop(); take=(value is None if op==0xc6 else value==0 if op==0x99 else value!=0)
            if take: pc=at+delta
        elif op==0xac: return stack.pop()
        else: raise AssertionError(hex(op))
    if stack: raise AssertionError("Fallback left operands on the stack")
    return "original"

def execute_interaction(code, out, origin, direction, native_result=True):
    owner = {1: origin, 2: direction}
    locals_ = [owner, out]
    stack = []; pc = 0; native_calls = []
    while pc < len(code):
        at = pc; op = code[pc]; pc += 1
        if op in (0x2a, 0x2b): stack.append(locals_[op - 0x2a])
        elif op == 0xb4:
            field = int.from_bytes(code[pc:pc+2], 'big'); pc += 2
            stack.append(stack.pop()[field])
        elif op == 0xb6:
            method = int.from_bytes(code[pc:pc+2], 'big'); pc += 2
            arg = stack.pop(); receiver = stack.pop()
            if method == 3:
                receiver[:] = arg; stack.append(True)
            else:
                native_calls.append((method, receiver, arg))
                if arg is not None: arg[:] = [70, 80, 90]
                stack.append(native_result)
        elif op == 0xc6:
            delta = int.from_bytes(code[pc:pc+2], 'big', signed=True); pc += 2
            if stack.pop() is None: pc = at + delta
        elif op == 0xac:
            result = stack.pop()
            if stack: raise AssertionError('Interaction helper leaked operands')
            return result, native_calls
        else: raise AssertionError(hex(op))
    raise AssertionError('Interaction helper did not return')

def execute_attack_guard(code, hand, button, tracked=True, single=True, firearm=True, net=False):
    owner={1: [1,2,3] if tracked else None, 2: [1,2,3] if tracked else None, 3:net}
    locals_=[owner,hand,button,None]; stack=[]; pc=0
    while pc<len(code):
        at=pc; op=code[pc];pc+=1
        if 0x2a<=op<=0x2d: stack.append(locals_[op-0x2a])
        elif 0x1a<=op<=0x1d: stack.append(locals_[op-0x1a])
        elif op==0x03: stack.append(0)
        elif op==0x04: stack.append(1)
        elif op==0x4e: locals_[3]=stack.pop()
        elif op in (0xb4,0xb6,0xc1):
            ref=int.from_bytes(code[pc:pc+2],'big');pc+=2
            if op==0xb4: stack.append(stack.pop()[ref])
            elif op==0xc1: stack.pop();stack.append(firearm)
            elif ref==4: stack.pop();stack.pop();stack.append('actual_weapon')
            elif ref==5: stack.pop();stack.pop();stack.append(not single)
            else: raise AssertionError(ref)
        elif op in (0xc6,0x99,0x9a,0x9f,0xa0,0xa7):
            delta=int.from_bytes(code[pc:pc+2],'big',signed=True);pc+=2
            if op==0xa7: take=True
            elif op in (0x9f,0xa0):
                equal=stack.pop()==stack.pop();take=equal if op==0x9f else not equal
            else:
                value=stack.pop();take=value is None if op==0xc6 else value==0 if op==0x99 else value!=0
            if take: pc=at+delta
        elif op==0xac:
            result=stack.pop();assert not stack;return result
        else: raise AssertionError(hex(op))
    assert not stack
    return 'original'

def execute_event_prefix(code, tracked=True, net=False, cache=True, suppressed=0):
    owner={1:[1,2,3] if cache else None,2:None,3:net,'tracked_player':tracked}
    weapon={10:owner,30:7,31:0}; locals_=[weapon,None,None,None,suppressed]
    stack=[];pc=0
    while pc<len(code):
        at=pc;op=code[pc];pc+=1
        if op==0x2a: stack.append(weapon)
        elif op==0x15: stack.append(locals_[code[pc]]);pc+=1
        elif op==0x04: stack.append(1)
        elif op==0x60: stack.append(stack.pop()+stack.pop())
        elif op in (0xb4,0xb5,0xc0,0xc1):
            ref=int.from_bytes(code[pc:pc+2],'big');pc+=2
            if op==0xb4: stack.append(stack.pop()[ref])
            elif op==0xb5:
                value=stack.pop();stack.pop()[ref]=value
            elif op==0xc1: stack.pop();stack.append(tracked)
        elif op in (0xc6,0x99,0x9a,0xa7):
            delta=int.from_bytes(code[pc:pc+2],'big',signed=True);pc+=2
            if op==0xa7: take=True
            else:
                value=stack.pop();take=value is None if op==0xc6 else value==0 if op==0x99 else value!=0
            if take: pc=at+delta
        else: raise AssertionError(hex(op))
    assert not stack
    return weapon

class WeaponConsumers(unittest.TestCase):
    def test_effect_status_distinguishes_creation_and_world_commit_failures(self):
        code=patch.fire_effect_prefix(10,11,3,12,[1,2],[5,6],[(20,21),(22,23)],24,[7,8],[9,10],25,26,27,(30,31,32,33))
        owner={'tracked_player':True,1:[10,20,30],5:[0,0,-1],7:[0,1,0],9:[0,0,1],3:False}
        for faults,status in (({},63),({'fail_create':True},3),({'fail_frame':True},207),({'fail_detach':True},207)):
            diagnostics={32:123,33:456}
            execute_effects(code,owner,diagnostics=diagnostics,require_current_frame=True,**faults)
            self.assertEqual(diagnostics[30],1)
            self.assertEqual(diagnostics[31],status)
            self.assertEqual([diagnostics[32],diagnostics[33]],
                [1,2] if not faults else [0,0], 'Stale or uncommitted native FX handles were published')
        diagnostics={}
        execute_effects(code,owner,definitions={20:0,21:0,22:0,23:0},diagnostics=diagnostics)
        self.assertEqual(diagnostics[31],0)
        self.assertEqual([diagnostics.get(32),diagnostics.get(33)],[0,0])

    def test_actual_shot_and_hit_observation_is_local_and_read_only(self):
        emit=getattr(patch,'tracked_event_prefix',None)
        self.assertTrue(callable(emit),'Native shot/hit observation prefix is missing')
        code=emit(10,11,3,[1,2],30,31)
        observed=execute_event_prefix(code,suppressed=1)
        self.assertEqual(observed[30],8)
        self.assertEqual(observed[31],1)
        for override in ({'tracked':False},{'net':True},{'cache':False}):
            observed=execute_event_prefix(code,**override)
            self.assertEqual(observed[30],7)
        hit=execute_event_prefix(emit(10,11,3,[1,2],30))
        self.assertEqual(hit[30],8)

    def test_tracked_single_hand_firearm_cannot_use_other_trigger(self):
        guard=getattr(patch,'single_hand_attack_guard',None)
        self.assertTrue(callable(guard),'Exact native fallback guard is missing')
        code=guard([1,2],3,4,6,5)
        for hand in (0,1):
            self.assertEqual(execute_attack_guard(code,hand,hand),'original')
            self.assertEqual(execute_attack_guard(code,hand,1-hand),0)
            for override in ({'tracked':False},{'single':False},{'firearm':False},{'net':True}):
                self.assertEqual(execute_attack_guard(code,hand,1-hand,**override),'original')
        self.assertEqual(execute_attack_guard(code,-1,0),'original')

    def test_starting_frame_commits_to_current_native_emitter_frame(self):
        code=patch.fire_effect_prefix(10,11,3,12,[1,2],[5,6],[(20,21),(22,23)],24,[7,8],[9,10],25,26,27)
        owner={'tracked_player':True,1:[10,20,30],5:[0,0,-1],7:[0,1,0],9:[0,0,1],3:False}
        effects=execute_effects(code,owner,require_current_frame=True)
        self.assertEqual(len(effects),2)
        self.assertEqual(execute_effects(code,owner,fail_detach=True,require_current_frame=True),[])

    def test_interaction_helpers_use_only_complete_local_cache(self):
        helper = getattr(patch, 'interaction_getter', None)
        self.assertTrue(callable(helper), 'Local interaction getter helper is missing')
        for field, expected in ((1, [1, 2, 3]), (2, [0, 0, -1])):
            code = helper([1, 2], field, 3, 4)
            out = []
            result, calls = execute_interaction(code, out, [1, 2, 3], [0, 0, -1])
            self.assertIs(result, True); self.assertEqual(calls, [])
            self.assertEqual(out, expected)
            for origin, direction, dest in ((None, [0, 0, -1], []),
                    ([1, 2, 3], None, []), (None, None, []),
                    ([1, 2, 3], [0, 0, -1], None)):
                for native_result in (False, True):
                    result, calls = execute_interaction(code, dest, origin, direction, native_result)
                    self.assertIs(result, native_result)
                    self.assertEqual(len(calls), 1)
                    self.assertEqual(calls[0][0], 4)
                    self.assertIs(calls[0][2], dest)

    def test_unknown_interaction_class_fails_closed(self):
        patcher = getattr(patch, 'patch_triggered_class', None)
        self.assertTrue(callable(patcher), 'Exact interaction class patch is missing')
        with self.assertRaisesRegex(ValueError, 'BeingTriggered.class SHA-256'):
            patcher(b'unknown')

    def test_visual_direction_preserves_tracked_base_and_native_fallback(self):
        code=patch.visual_direction_source([1,2],3,4)
        owner={1:[1,2,3],2:[4,5,6],3:False,4:[[7,8,9],[10,11,12]]}
        for hand in (0,1):
            self.assertEqual(execute_effects(code,owner,visual=True,hand=hand),owner[hand+1])
            owner[hand+1]=None
            self.assertEqual(execute_effects(code,owner,visual=True,hand=hand),owner[4][hand])
        owner[3]=True;owner[1]=[1,2,3]
        self.assertEqual(execute_effects(code,owner,visual=True),owner[4][0])
        for hand in (-1,2):
            with self.assertRaises(IndexError): execute_effects(code,owner,visual=True,hand=hand)

    def test_fire_effects_use_verified_world_muzzle_and_fallback(self):
        code=patch.fire_effect_prefix(10,11,3,12,[1,2],[5,6],[(20,21),(22,23)],24,[7,8],[9,10],25,26,27)
        owner={'tracked_player':True,1:[100,200,300],2:[400,500,600],5:[0,0,-1],6:[1,0,0],7:[0,1,0],8:[0,0,1],9:[-1,0,0],10:[0,1,0],3:False}
        for hand in (0,1):
            effects=execute_effects(code,owner,hand=hand)
            self.assertEqual(effects,[[101,102,owner[hand+1],owner[hand+7],None,-1,owner[hand+9]],
                                     [201,202,owner[hand+1],owner[hand+7],None,-1,owner[hand+9]]])
        self.assertEqual(execute_effects(code,owner,{20:0,21:102,22:201,23:0}),[])
        for changes,hand in (({'tracked_player':False},0),({3:True},0),({1:None},0),({5:None},0),({7:None},0),({9:None},0),({},2),({},-1)):
            bad=dict(owner);bad.update(changes)
            self.assertEqual(execute_effects(code,bad,hand=hand),'original')
        self.assertEqual(execute_effects(code,None),'original')
        self.assertEqual(execute_effects(code,owner,fail_frame=True),[])
        # Failed creation never attempts to write handle zero.
        self.assertEqual(execute_effects(code,owner,fail_create=True),[])

    def test_authored_minus_x_particle_axis_follows_barrel_with_roll(self):
        import math
        code=patch.fire_effect_prefix(10,11,3,12,[1,2],[5,6],[(20,21),(22,23)],24,[7,8],[9,10],25,26,27)
        for angle in (0,0.5,1.57,3.14):
            up=[math.cos(angle),math.sin(angle),0]
            forward=[-math.sin(angle),math.cos(angle),0]
            owner={'tracked_player':True,1:[10,20,30],5:[0,0,-1],7:up,9:forward,3:False}
            for effect in execute_effects(code,owner):
                # Exact engine stores +X = up x forward; shipped barrel FX use -X.
                u,f=effect[3],effect[6]
                axis=[-(u[1]*f[2]-u[2]*f[1]),-(u[2]*f[0]-u[0]*f[2]),-(u[0]*f[1]-u[1]*f[0])]
                for actual,expected in zip(axis,owner[5]): self.assertAlmostEqual(actual,expected)
                self.assertEqual(effect[2],owner[1])

    def test_per_hand_and_fallback(self):
        for weapon in (False,True):
            code=prefix([1,2],3,4,5,weapon)
            for hand,expected in ((0,[10,20,30]),(1,[40,50,60])):
                out=[]
                self.assertIs(execute(code,hand,out,right=[10,20,30],left=[40,50,60],weapon=weapon),True)
                self.assertEqual(out,expected)
            for args in ({"hand":-1},{"hand":2},{"hand":0,"net":True},{"hand":0,"right":None},{"hand":1,"left":None},{"hand":0,"out":None}):
                kwargs=dict(hand=0,out=[],right=[1],left=[2],weapon=weapon); kwargs.update(args)
                self.assertEqual(execute(code,**kwargs),"original")
    def test_unknown_class_fails_closed(self):
        with self.assertRaisesRegex(ValueError,"SHA-256"): patch_class(b"unknown")
        with self.assertRaisesRegex(ValueError,"SHA-256"): patch.patch_fire_class(b"unknown")

if __name__=="__main__": unittest.main()
