"""Exact archive regressions for pause preview allocation crash; never launch CoJ."""
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from inspect_java_bytecode import parse_class,disassemble
import patch_coj_weapon_consumers as patch
from native_settings_patch import original_archives

class PausePreviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source,_=original_archives()
        cls.temp=tempfile.TemporaryDirectory(prefix='cojvr-pause-preview-')
        cls.output=Path(cls.temp.name)/'code.pak'
        patch.patch_archive(cls.source,cls.output,manual_reload=True)
        cls.before={};cls.after={}
        with zipfile.ZipFile(cls.source) as a,zipfile.ZipFile(cls.output) as b:
            for name in ('LawmanModuleSingle.class','MenuBackgroundModule.class','MenuSaveLoad.class'):
                cls.before[name]=a.read(name);cls.after[name]=b.read(name)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def method(self,source,name,method,desc='()V'):
        pool,methods=parse_class(source[name])
        return pool,next(m for m in methods if (m.name,m.descriptor)==(method,desc))
    def test_pause_capture_removed_both_modes_without_changing_pause_or_cleanup(self):
        name='LawmanModuleSingle.class'
        p,a=self.method(self.before,name,'TakeScreenshotsOnLevelBreak')
        _,b=self.method(self.after,name,'TakeScreenshotsOnLevelBreak')
        self.assertEqual(len(a.code),110);self.assertEqual(len(a.code),len(b.code))
        expected=bytearray(a.code);expected[30:34]=bytes(4);expected[63:81]=bytes(18)
        self.assertEqual(b.code,expected)
        lines='\n'.join(disassemble(b.code,p))
        self.assertNotIn('TakeScreenShotInGame',lines);self.assertNotIn('Game.TakeScreenshot',lines)
        self.assertNotIn('ForceRender',lines)
        self.assertIn('ClearInputQueue',lines);self.assertIn('ShowSprites',lines)
        for method in ('LevelBreak','DelayedLevelBreak','DelayedLevelBreakOnFrameEnd'):
            _,old=self.method(self.before,name,method);_,new=self.method(self.after,name,method)
            self.assertEqual((old.code,old.max_stack,old.max_locals),(new.code,new.max_stack,new.max_locals))
    def test_pause_background_uses_existing_native_null_cleanup_path(self):
        name='MenuBackgroundModule.class'
        _,a=self.method(self.before,name,'ShowScreenInGame');_,b=self.method(self.after,name,'ShowScreenInGame')
        expected=bytearray(a.code);expected[4:6]=b'\x01\x00'
        self.assertEqual(b.code,expected)
        at=self.before[name].find(a.code);self.assertGreaterEqual(at,0)
        self.assertEqual(self.before[name].count(a.code),1)
        raw=bytearray(self.before[name]);raw[at+4:at+6]=b'\x01\x00'
        self.assertEqual(self.after[name],raw) # Retain all metadata/unrelated members.
    def test_manual_save_cleans_old_preview_without_copying_missing_pause_image(self):
        name='MenuSaveLoad.class'
        p,a=self.method(self.before,name,'SaveScreenshot','(Ljava/lang/String;)V')
        _,b=self.method(self.after,name,'SaveScreenshot','(Ljava/lang/String;)V')
        expected=bytearray(a.code);expected[32:39]=bytes(7)
        self.assertEqual(b.code,expected)
        lines='\n'.join(disassemble(b.code,p))
        self.assertIn('FSFile._delete_file',lines);self.assertNotIn('CopyFile',lines)
        at=self.before[name].find(a.code);self.assertEqual(self.before[name].count(a.code),1)
        raw=bytearray(self.before[name]);raw[at+32:at+39]=bytes(7)
        self.assertEqual(self.after[name],raw)

    def test_unknown_or_already_patched_consumers_fail_closed(self):
        for name,background in (('MenuBackgroundModule.class',True),('MenuSaveLoad.class',False)):
            for data in (b'unknown',self.after[name]):
                with self.assertRaisesRegex(ValueError,'SHA-256'):patch.patch_pause_preview_consumer(data,background)
        _,method=self.method(self.before,'LawmanModuleSingle.class','TakeScreenshotsOnLevelBreak')
        for index in (30,31,63,75,78):
            data=bytearray(method.code);data[index]^=1
            with self.assertRaisesRegex(ValueError,'boundary'):patch.suppress_pause_screenshots(bytes(data))

    def test_pause_executes_input_cleanup_and_sprite_restoration_in_both_modes(self):
        _,method=self.method(self.after,'LawmanModuleSingle.class','TakeScreenshotsOnLevelBreak')
        code=method.code
        for ingame in (False,True):
            for hide in (False,True):
                for no_sprites in (False,True):
                    stack=[];locals_={0:'module'};events=[];pc=0;steps=0
                    while pc<len(code):
                        steps+=1;self.assertLess(steps,100)
                        start=pc;op=code[pc];pc+=1
                        if op==0:continue
                        if op in (0x03,0x04,0x0b):stack.append(1 if op==4 else 0)
                        elif op in (0x44,0x45,0x3e):locals_[{0x44:1,0x45:2,0x3e:3}[op]]=stack.pop()
                        elif op in (0x2a,0x1d):stack.append(locals_[0 if op==0x2a else 3])
                        elif op==0x13:pc+=2;stack.append('Out/save.tga')
                        elif op in (0x99,0x9a):
                            delta=int.from_bytes(code[pc:pc+2],'big',signed=True);pc+=2;value=bool(stack.pop())
                            if value==(op==0x9a):pc=start+delta
                        elif op in (0xb2,0xb4,0xb6,0xb8):
                            ref=int.from_bytes(code[pc:pc+2],'big');pc+=2
                            if op==0xb2:stack.append({357:ingame,315:no_sprites,172:'ingame',176:'save'}[ref])
                            elif op==0xb4:self.assertEqual(stack.pop(),'module');self.assertEqual(ref,62);stack.append(hide)
                            elif op==0xb6 and ref==358:
                                visible=stack.pop();self.assertEqual(stack.pop(),'module');events.append(('sprites',visible))
                            elif op==0xb6 and ref==363:stack.append(stack.pop()==stack.pop())
                            elif op==0xb8 and ref in (359,361):
                                events.append(('begin' if ref==359 else 'end',stack.pop()))
                                if ref==361:stack.append(0)
                            elif op==0xb8 and ref==365:events.append(('clear_input',))
                            else:self.fail('Pause attempted screenshot/ForceRender or unknown call '+hex(ref))
                        elif op==0xb1:break
                        else:self.fail('Unexpected pause opcode '+hex(op))
                    self.assertEqual(stack,[])
                    profile='ingame' if ingame else 'save'
                    expected=([('sprites',0)] if hide else [])+[('begin',profile),('end',profile),('clear_input',)]
                    if hide and not no_sprites:expected.append(('sprites',1))
                    self.assertEqual(events,expected)

if __name__=='__main__':unittest.main(verbosity=2)
