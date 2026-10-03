"""Execute the injected bytecode paths without invoking game/JVM state."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from patch_coj_weapon_consumers import prefix, patch_class

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

if __name__=="__main__": unittest.main()
