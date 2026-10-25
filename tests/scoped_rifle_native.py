"""Execute exact scoped-mask derivative and retain original attack contracts."""
import ast,hashlib,subprocess,sys,tempfile,unittest,zipfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from inspect_java_bytecode import parse_class,disassemble
from native_settings_patch import original_archives

class ScopedRifleNative(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        code,_=original_archives()
        with zipfile.ZipFile(code) as z:
            cls.original=z.read('FPPCamera.class')
            cls.logic=z.read('WpnLogicRifle.class')
            cls.attack=z.read('ArmedPlayerBeing.class')
        cls.archive=code
    def test_shipped_jvm_verifies_camera_and_rejects_corrupt_wrapper(self):
        from bow_draw_patch import verifier_class
        from coj_reload_wait_probe import ClassEdit
        from patch_coj_weapon_consumers import patch_archive
        game=Path('C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez')
        java=game/'jre/bin/java.exe'
        if not java.is_file():self.skipTest('shipped JVM unavailable')
        evidence=Path(__file__).resolve().parents[1]/'work/scoped-rifle-dlss-20261025/jvm'
        evidence.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=evidence) as folder:
            root=Path(folder);candidate=root/'code.pak';patch_archive(self.archive,candidate,manual_reload=True)
            with zipfile.ZipFile(candidate) as z:camera=z.read('FPPCamera.class')
            (root/'VerifyBowDrawPatch.class').write_bytes(verifier_class('FPPCamera'))
            for valid in (True,False):
                current=root/('good' if valid else 'bad');current.mkdir()
                if not valid:
                    edit=ClassEdit(camera);edit.changes['UpdateScopeObject','()V']=lambda old:b'\xfe'+old[1:];camera=edit.finish()
                (current/'FPPCamera.class').write_bytes(camera)
                result=subprocess.run([str(java),'-Xverify:all','-cp',str(current)+';'+str(root)+';'+str(candidate),
                    'VerifyBowDrawPatch'],capture_output=True,text=True,timeout=30)
                (evidence/('good.log' if valid else 'bad.log')).write_text(
                    f'exit={result.returncode}\n'+result.stdout+result.stderr,encoding='utf-8')
                self.assertEqual(result.returncode==0,valid,result.stdout+result.stderr)
    def test_native_scope_button0_is_aim_and_button1_retains_attack(self):
        p,methods=parse_class(self.logic)
        m=next(m for m in methods if m.name=='GetAttackMode')
        self.assertEqual(m.code[14:22],bytes.fromhex('15059a000610feac'))
        self.assertIn('WpnLogicFire.GetAttackMode','\n'.join(disassemble(m.code,p)))
        p,methods=parse_class(self.attack)
        m=next(m for m in methods if m.name=='Attack' and m.descriptor=='(IZ)V')
        self.assertIn('ArmedPlayerBeing.Attack(II)Z','\n'.join(disassemble(m.code,p)))
    def test_mask_patch_preserves_original_update_and_rejects_unknown(self):
        from coj_scoped_rifle import patch_scope_camera
        patched=patch_scope_camera(self.original)
        _,before=parse_class(self.original);p,after=parse_class(patched)
        original={ (m.name,m.descriptor):m for m in before}
        helpers={(m.name,m.descriptor):m for m in after}
        self.assertEqual(helpers['cojvrOriginalUpdateScopeObject','()V'].code,
            original['UpdateScopeObject','()V'].code)
        for key,m in original.items():
            if key!=('UpdateScopeObject','()V'):self.assertEqual(helpers[key].code,m.code)
        for bad in (b'',self.original[:-1],patched):
            with self.assertRaises(ValueError):patch_scope_camera(bad)
    def test_only_local_stable_tracked_exact_scope_suppresses_desktop_mask(self):
        from coj_scoped_rifle import patch_scope_camera
        p,methods=parse_class(patch_scope_camera(self.original))
        code=next(m.code for m in methods if m.name=='UpdateScopeObject')
        def run(*,player=True,tracked=True,net=False,kind='WeaponRifleWinchesterScope',
                actual='rifle',desired='rifle',active='rifle',left=None):
            stack=[];locals_={0:'camera'};pc=0;calls=[]
            while pc<len(code):
                start=pc;op=code[pc];pc+=1
                if op in (0x2a,0x2b,0x2c,0x2d):stack.append(locals_.get(op-0x2a))
                elif op in (0x4b,0x4c,0x4d,0x4e):locals_[op-0x4b]=stack.pop()
                elif op in (3,4):stack.append(op-3)
                elif op==0x13:
                    ref=int.from_bytes(code[pc:pc+2],'big');pc+=2;stack.append(ast.literal_eval(p.describe(ref)))
                elif op in (0x99,0x9a,0xc6,0xc7,0xa5,0xa6):
                    delta=int.from_bytes(code[pc:pc+2],'big',signed=True);pc+=2;a=stack.pop()
                    if op in (0xa5,0xa6):b=stack.pop();take=(a==b)==(op==0xa5)
                    else:take={0x99:not bool(a),0x9a:bool(a),0xc6:a is None,0xc7:a is not None}[op]
                    if take:pc=start+delta
                elif op in (0xb4,0xb6,0xc0,0xc1):
                    ref=int.from_bytes(code[pc:pc+2],'big');pc+=2;name=p.describe(ref)
                    if op==0xc0:continue
                    if op==0xc1:stack.append(player and stack.pop()=='player');continue
                    if op==0xb4:
                        self.assertEqual(stack.pop(),'player')
                        stack.append(net if 'm_bNetAttackForced' in name else ('ray' if tracked else None))
                    elif '.GetBeing' in name:self.assertEqual(stack.pop(),'camera');stack.append('player' if player else None)
                    elif any('.'+n in name for n in ('GetActualWeaponNotEmpty','GetDesiredWeapon','GetActiveWeapon')):
                        hand=stack.pop();self.assertEqual(stack.pop(),'player')
                        stack.append(left if hand else actual if '.GetActualWeaponNotEmpty' in name else desired if '.GetDesiredWeapon' in name else active)
                    elif '.getClass' in name:stack.pop();stack.append('class')
                    elif '.getName' in name:self.assertEqual(stack.pop(),'class');stack.append(kind)
                    elif '.equals' in name:a=stack.pop();b=stack.pop();stack.append(a==b)
                    elif '.HideScopeObject' in name:self.assertEqual(stack.pop(),'camera');calls.append('hide')
                    elif '.cojvrOriginalUpdateScopeObject' in name:self.assertEqual(stack.pop(),'camera');calls.append('native')
                    else:self.fail(name)
                elif op==0xb1:return calls
                else:self.fail(f'unexpected opcode {op:02x}')
            self.fail('missing return')
        self.assertEqual(run(),['hide'])
        for overrides in ({'player':False},{'tracked':False},{'net':True},{'kind':'WeaponRifleWinchester'},
            {'kind':'UnknownScopedSubclass'},{'actual':None},{'desired':'replacement'},{'active':'replacement'},{'left':'pistol'}):
            self.assertEqual(run(**overrides),['native'],overrides)
        # Native post-being updates own restoration: the next untracked call
        # executes the complete original Show/Hide and transform lifecycle.
        self.assertEqual(run(tracked=False),['native'])

if __name__=='__main__':unittest.main()
