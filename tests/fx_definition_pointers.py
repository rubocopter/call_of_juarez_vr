"""Replay exact shipped creation/release branches with signed LAA pointers."""
import hashlib,struct,sys,unittest,zipfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from inspect_java_bytecode import parse_class
from coj_fx_definitions import patch_fx_definitions
from native_settings_patch import original_archives

def signed(value):return struct.unpack('>i',struct.pack('>I',value))[0]
def reference(pool,index):
    entry=pool.get(index);nt=pool.get(entry[2])
    return pool.describe(entry[1]),pool.utf8(nt[1]),pool.utf8(nt[2])
def arguments(desc):
    count,at=0,1
    while desc[at]!=')':
        while desc[at]=='[':at+=1
        at=desc.index(';',at)+1 if desc[at]=='L' else at+1;count+=1
    return count
def execute(classes,cls,name,obj,calls,limit=None):
    pool,methods=classes[cls];code=next(m.code for m in methods if m.name==name and m.descriptor=='()V')
    pc=0;stack=[]
    while pc<(len(code) if limit is None else limit):
        start=pc;op=code[pc];pc+=1
        if op==0x2a:stack.append(obj)
        elif op==1:stack.append(None)
        elif 2<=op<=8:stack.append(op-3)
        elif op==0x57:stack.pop()
        elif op in (0xb2,0xb4,0xb5):
            idx=int.from_bytes(code[pc:pc+2],'big');pc+=2;_,field,_=reference(pool,idx)
            if op==0xb2:stack.append(.1)
            elif op==0xb4:stack.append(stack.pop().get(field,0))
            else:value=stack.pop();stack.pop()[field]=value
        elif op in (0x99,0x9a,0x9e):
            delta=int.from_bytes(code[pc:pc+2],'big',signed=True);pc+=2;value=stack.pop()
            if (value==0 if op==0x99 else value!=0 if op==0x9a else value<=0):pc=start+delta
        elif op in (0xb6,0xb7,0xb8):
            idx=int.from_bytes(code[pc:pc+2],'big');pc+=2;owner,method,desc=reference(pool,idx)
            args=[stack.pop() for _ in range(arguments(desc))][::-1];receiver=None if op==0xb8 else stack.pop()
            if op==0xb7 and owner=='FXEmitter' and method in ('Start','DestroyFXDefs'):
                execute(classes,owner,method,receiver,calls);continue
            calls.append((method,args));result=777 if method=='FXCreateParticleEmiter' else .05 if method=='RandomFloat' else 1
            if not desc.endswith('V'):stack.append(result)
        elif op==0xb1:return
        else:raise AssertionError(f'unsupported opcode {op:02x} at {start}')

class FxPointers(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        code,_=original_archives()
        with zipfile.ZipFile(code) as archive:
            cls.original=archive.read('FXEmitter.class');cls.fire=archive.read('FireEmitterCore.class')
    def test_creation_and_release_of_heap_pointers(self):
        patched=patch_fx_definitions(self.original)
        classes={'FXEmitter':parse_class(patched),'FireEmitterCore':parse_class(self.fire)}
        for a,b in ((0x123400,0x234500),(0x80001234,0x234500),(0x123400,0xF1234560),
                    (0x80001234,0xF1234560),(0,0),(0,0xF1234560),(0x80001234,0)):
            obj={'m_nFXEmitterDefID':signed(a),'m_nFXParticleDefID':signed(b)};calls=[]
            execute(classes,'FireEmitterCore','Start',obj,calls)
            creation=[args for name,args in calls if name=='FXCreateParticleEmiter']
            self.assertEqual(len(creation),int(bool(a and b)))
            if creation:self.assertIn(signed(a),creation[0]);self.assertIn(signed(b),creation[0])
            self.assertTrue(obj['m_bIsStarted'])
            self.assertTrue(any(name=='CallMethodEveryMinMax' for name,_ in calls))
            calls=[];execute(classes,'FXEmitter','DestroyFXDefs',obj,calls)
            self.assertEqual([args for name,args in calls if name=='FXReleaseEmiterDef'],[[signed(a)]] if a else [])
            self.assertEqual([args for name,args in calls if name=='FXReleaseParticleDef'],[[signed(b)]] if b else [])
            self.assertEqual(obj['m_nFXEmitterDefID'],0);self.assertEqual(obj['m_nFXParticleDefID'],0)
            calls=[];execute(classes,'FXEmitter','DestroyFXDefs',obj,calls);self.assertFalse(calls)
    def test_exact_change_set_and_unknown_identity(self):
        _,before=parse_class(self.original);_,after=parse_class(patch_fx_definitions(self.original))
        for old,new in zip(before,after):
            self.assertEqual((old.name,old.descriptor),(new.name,new.descriptor))
            diffs=[i for i,(a,b) in enumerate(zip(old.code or b'',new.code or b'')) if a!=b]
            self.assertEqual(diffs,{'Start':[11,18],'DestroyFXDefs':[4,23]}.get(old.name,[]))
            self.assertEqual(len(old.code or b''),len(new.code or b''))
        with self.assertRaises(ValueError):patch_fx_definitions(self.original[:-1]+b'!')
        with self.assertRaises(ValueError):patch_fx_definitions(patch_fx_definitions(self.original))
    def test_smoke_sparks_and_steam_creation_and_release(self):
        classes={'FXEmitter':parse_class(patch_fx_definitions(self.original)),
                 'FireEmitterCore':parse_class(patch_fx_definitions(self.fire))}
        fields=tuple('m_n'+effect+'FX'+kind+'DefID' for effect in ('Smoke','Sparks','Steam')
                     for kind in ('Emitter','Particle'))
        for value in (0,0x12345670,0x81234560):
            obj={f:signed(value) for f in fields};calls=[]
            execute(classes,'FireEmitterCore','Start',obj,calls)
            self.assertEqual(sum(name=='FXCreateParticleEmiter' for name,_ in calls),2 if value else 0)
            calls=[];execute(classes,'FireEmitterCore','StartSteam',obj,calls,48)
            self.assertEqual(sum(name=='FXCreateParticleEmiter' for name,_ in calls),int(bool(value)))
            calls=[];execute(classes,'FireEmitterCore','DestroyFXDefs',obj,calls)
            self.assertEqual(sum(name.startswith('FXRelease') for name,_ in calls),6 if value else 0)
            self.assertTrue(all(obj[field]==0 for field in fields))
        _,before=parse_class(self.fire);_,after=classes['FireEmitterCore']
        for old,new in zip(before,after):
            expected={'Start':[23,30,81,88],'StartSteam':[18,25],'DestroyFXDefs':[8,27,46,65,84,103]}.get(old.name,[])
            self.assertEqual([i for i,(a,b) in enumerate(zip(old.code or b'',new.code or b'')) if a!=b],expected)
            self.assertEqual(len(old.code or b''),len(new.code or b''))
if __name__=='__main__':unittest.main()
