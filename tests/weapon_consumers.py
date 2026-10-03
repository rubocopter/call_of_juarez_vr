"""Execute the injected bytecode paths without invoking game/JVM state."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import patch_coj_weapon_consumers as patch
from patch_coj_weapon_consumers import prefix, patch_class

def execute_effects(code, owner, definitions=None, visual=False, hand=0):
    # Execute emitted JVM instructions and record world emitter arguments.
    definitions = definitions or {20: 101, 21: 102, 22: 201, 23: 202}
    weapon = {10: owner}
    locals_ = [owner if visual else weapon, hand if visual else definitions, None, None, None, None]
    stack=[]; pc=0; effects=[]
    while pc < len(code):
        at=pc; op=code[pc]; pc+=1
        if 0x2a <= op <= 0x2d: stack.append(locals_[op-0x2a])
        elif 0x1a <= op <= 0x1d: stack.append(locals_[op-0x1a])
        elif 0x4b <= op <= 0x4e: locals_[op-0x4b]=stack.pop()
        elif 0x3b <= op <= 0x3e: locals_[op-0x3b]=stack.pop()
        elif op == 0x19: stack.append(locals_[code[pc]]);pc+=1
        elif op == 0x3a: locals_[code[pc]]=stack.pop();pc+=1
        elif op == 0x01: stack.append(None)
        elif op == 0x02: stack.append(-1)
        elif op == 0x04: stack.append(1)
        elif op == 0x32:
            index=stack.pop();array=stack.pop()
            if index<0 or index>=len(array): raise IndexError(index)
            stack.append(array[index])
        elif op in (0xb4,0xb6,0xc0,0xc1):
            index=int.from_bytes(code[pc:pc+2],'big');pc+=2
            if op==0xb4: stack.append(stack.pop()[index])
            elif op==0xc0: pass
            elif op==0xc1: stack.append(isinstance(stack.pop(),dict) and owner.get('tracked_player',False))
            elif index==12: stack.pop();stack.pop();stack.append(hand)
            elif index==24:
                args=[stack.pop() for _ in range(6)][::-1];stack.pop();effects.append(args);stack.append(7)
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
        elif op==0xb1: return effects
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

class WeaponConsumers(unittest.TestCase):
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
        code=patch.fire_effect_prefix(10,11,3,12,[1,2],[5,6],[(20,21),(22,23)],24)
        owner={'tracked_player':True,1:[100,200,300],2:[400,500,600],5:[0,0,-1],6:[1,0,0],3:False}
        for hand in (0,1):
            effects=execute_effects(code,owner,hand=hand)
            self.assertEqual(effects,[[101,102,owner[hand+1],owner[hand+5],None,-1],
                                     [201,202,owner[hand+1],owner[hand+5],None,-1]])
        self.assertEqual(execute_effects(code,owner,{20:0,21:102,22:201,23:0}),[])
        for changes,hand in (({'tracked_player':False},0),({3:True},0),({1:None},0),({5:None},0),({},2),({},-1)):
            bad=dict(owner);bad.update(changes)
            self.assertEqual(execute_effects(code,bad,hand=hand),'original')
        self.assertEqual(execute_effects(code,None),'original')

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
