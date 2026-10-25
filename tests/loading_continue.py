"""Execute emitted loading gate with native exclusive-registration as a fixture."""
import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from inspect_java_bytecode import parse_class
from native_settings_patch import original_archives
from coj_loading_ui import patch_loading_ui
import zipfile

class LoadingTests(unittest.TestCase):
    def test_only_registered_fresh_loading_owner_queues_one_native_press(self):
        code,_=original_archives()
        with zipfile.ZipFile(code) as z:data=z.read('GameUILoading.class')
        pool,methods=parse_class(patch_loading_ui(data))
        method=next(m for m in methods if m.name=='cojvrContinueLoading')
        pending=False;ready=False;calls=[]
        def run():
            nonlocal pending
            stack=[];pc=0
            while True:
                start=pc;op=method.code[pc];pc+=1
                if op==0x2a:stack.append('ui')
                elif op in (3,4):stack.append(op-3)
                elif op in (0x99,0x9a):
                    delta=int.from_bytes(method.code[pc:pc+2],'big',signed=True);pc+=2
                    if bool(stack.pop())==(op==0x9a):pc=start+delta
                elif op in (0xb4,0xb5,0xb6):
                    ref=int.from_bytes(method.code[pc:pc+2],'big');pc+=2;name=pool.describe(ref)
                    if op==0xb4:self.assertEqual(stack.pop(),'ui');stack.append(pending)
                    elif op==0xb5:pending=bool(stack.pop());self.assertEqual(stack.pop(),'ui')
                    elif 'IsRegisteredKeyRequestExclusive' in name:self.assertEqual(stack.pop(),'ui');stack.append(ready)
                    elif '.OnInputKey' in name:
                        args=[stack.pop() for _ in range(4)];self.assertEqual(args,[0,1,1,'ui']);calls.append('key_down')
                    else:self.fail(name)
                elif op==0xac:return stack.pop()
                else:self.fail(hex(op))
        self.assertEqual(run(),0);self.assertFalse(calls)
        ready=True;self.assertEqual(run(),1);self.assertEqual(calls,['key_down'])
        for _ in range(10):self.assertEqual(run(),0)
        ready=False;pending=False;self.assertEqual(run(),0)
        ready=True;self.assertEqual(run(),1);self.assertEqual(len(calls),2)
        # Native dispatch, timers, mission entry and fade callbacks are untouched.
        _,originals=parse_class(data);old={(m.name,m.descriptor):m.code for m in originals}
        for name in ('OnInputKey','OnUserInput','OnUserInputAfterFade','StartMission'):
            desc='(IZC)V' if name=='OnInputKey' else '()V'
            self.assertEqual(next(m.code for m in methods if (m.name,m.descriptor)==(name,desc)),old[name,desc])
        for name,desc in (('Hide','()V'),('WaitForUserInput','()V'),('ShowCommon','(Ljava/lang/Object;)V')):
            self.assertEqual(next(m.code for m in methods if (m.name,m.descriptor)==('cojvrOriginal'+name,desc)),old[name,desc])

if __name__=='__main__':unittest.main()
