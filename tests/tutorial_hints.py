"""Read-only originals + bounded interpreted execution; no JVM/compiler/staging."""
import hashlib
import importlib.util
import os
from pathlib import Path
import struct
import sys
import unittest
import zipfile
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_java_bytecode import parse_class
spec = importlib.util.find_spec('coj_tutorial_hints')
hints = __import__('coj_tutorial_hints') if spec else None


class MappingTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(hints, 'tutorial presentation implementation missing')

    def test_only_exact_hint_resource_locale_and_one_token(self):
        text = "Press '%KEY(_ACTION_TOGGLE_ALTERNATE)' to continue."
        self.assertEqual(hints.map_template('E03_H05_FastShootingA', '&E03_H05_FastShootingA&', 'En', text),
                         "Press 'Alternate fire' to continue.")
        for hint, resource, locale in [('other', '&E03_H05_FastShootingA&', 'En'),
            ('E03_H05_FastShootingA', '&E03_H05_FastShootingB&', 'En'),
            ('E03_H05_FastShootingA', '&E03_H05_FastShootingA&', 'Es'),
            ('E03_H05_FastShootingAX', '&E03_H05_FastShootingAX&', 'En')]:
            self.assertEqual(hints.map_template(hint, resource, locale, text), text)
        for text in ('Z', 'Press %KEY(_ACTION_KICK)', '%KEY(_ACTION_TOGGLE_ALTERNATE)'*2, '', None):
            self.assertEqual(hints.map_template('E03_H05_FastShootingA', '&E03_H05_FastShootingA&', 'En', text), text)

    def test_unknown_hashes_fail_before_transformation(self):
        with self.assertRaisesRegex(ValueError, 'SHA-256'):
            hints.patch_tutorial_classes(b'unknown', b'unknown', b'unknown', b'unknown')


class NativeOriginalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if hints is None:
            raise unittest.SkipTest('implementation missing (mapping tests fail)')
        game = Path(os.environ.get('COJ_GAME_DIRECTORY',
            'C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez'))
        code = game/'code.cojvr-backup.pak'
        if not code.exists(): code = game/'code.pak'
        if not code.exists(): raise unittest.SkipTest('original archives unavailable')
        if hashlib.sha256(code.read_bytes()).hexdigest() != hints.CODE_SHA256:
            raise AssertionError('present archive is not exact original code.pak')
        with zipfile.ZipFile(code) as z:
            cls.original_classes = {n:z.read(n) for n in z.namelist() if n.endswith('.class')}
            cls.base, cls.menu = z.read('HintBase.class'), z.read('MenuHint.class')
        with zipfile.ZipFile(game/'DataEn.pak') as z:
            cls.resource = z.read(hints.RESOURCE_NAME)
            cls.tip_resource = z.read('Data/TextsGenerated.scr')
        data = game/'Data0.cojvr-backup.pak'
        if not data.exists(): data = game/'Data0.pak'
        with zipfile.ZipFile(data) as z:
            cls.restrictions = z.read('Data/Hints.scr')
        if hashlib.sha256(cls.restrictions).hexdigest() != 'a6c483308b924ac0578bd6cdc9e9d7e9e1fbf60cafe3c8bc1f2bb76b21b4af12':
            raise AssertionError('present hint restrictions are not exact original')
        cls.base_after, cls.menu_after = hints.patch_tutorial_classes(cls.base, cls.menu, cls.resource, cls.tip_resource)

    def test_patch_only_menu_presentation_call_and_adds_base_helper(self):
        _, before = parse_class(self.base)
        cp, after = parse_class(self.base_after)
        by_name = {(m.name,m.descriptor): m for m in after}
        for method in before:
            self.assertEqual(method, by_name[(method.name,method.descriptor)])
        self.assertEqual(len(after), len(before)+2)
        a, b = parse_class(self.menu)[1], parse_class(self.menu_after)[1]
        for original, patched in zip(a,b):
            if original.name == 'Show':
                self.assertEqual(len(original.code),len(patched.code))
                changed = [i for i,(x,y) in enumerate(zip(original.code,patched.code)) if x!=y]
                self.assertTrue(changed and set(changed).issubset({84,85,95,96}))
                self.assertTrue(set(changed)&{95,96},'continuation PC label was not replaced')
            else: self.assertEqual(original,patched)

    def test_interpreted_scoped_localized_helper_and_unknown_fallback(self):
        templates = hints.read_templates(self.resource)
        for hint, template in templates.items():
            expected = hints.map_template(hint, '&'+hint+'&', 'En', template)
            self.assertNotIn('%KEY(', expected)
            self.assertNotIn('Circle',expected)
            self.assertEqual(self.execute(hint,'&'+hint+'&',template), expected)
            self.assertEqual(self.execute('other','&'+hint+'&',template),'NATIVE')
            self.assertEqual(self.execute(hint,'other',template),'NATIVE')
            self.assertEqual(self.execute(hint,'&'+hint+'&','Unverified localized text'),'NATIVE')
            self.assertEqual(self.execute(hint,'&'+hint+'&',None),'NATIVE')

    def test_modified_class_and_resource_rejected(self):
        for base,menu,resource in [(self.base_after,self.menu,self.resource),
            (self.base,self.menu_after,self.resource),(self.base,self.menu,self.resource+b' '),
            (self.base+b' ',self.menu,self.resource)]:
            with self.assertRaisesRegex(ValueError,'SHA-256'):
                hints.patch_tutorial_classes(base,menu,resource,self.tip_resource)
        with self.assertRaisesRegex(ValueError,'SHA-256'):
            hints.patch_tutorial_classes(self.base,self.menu,self.resource,self.tip_resource+b' ')

    def test_tip_keeps_native_readback_and_rejects_changed_restrictions(self):
        hint='E03_H05_FastShootingA';template=hints.read_templates(self.resource)[hint]
        self.assertEqual(self.execute(hint,'&'+hint+'&',template,tip=True),"Press 'Alternate fire' to continue")
        for conditions in (None, [], [None], [[None]], [[21]], [[20,20]], [[20],[20]]):
            self.assertEqual(self.execute(hint,'&'+hint+'&',template,tip=True,conditions=conditions),'NATIVE_TIP')
        for h,r,t in [('other','&'+hint+'&',template),(hint,'other',template),
                      (hint,'&'+hint+'&','Unknown locale'),(hint,'&'+hint+'&',None)]:
            self.assertEqual(self.execute(h,r,t,tip=True),'NATIVE_TIP')
        self.assertEqual(self.execute(hint,'&'+hint+'&',template,tip=True,tip_template='Pulse %s'),'NATIVE_TIP')
        self.assertIsNone(self.execute('unknown','unknown',None,tip=True,native_tip=None))
        with self.assertRaisesRegex(RuntimeError,'native assignment failure'):
            self.execute(hint,'&'+hint+'&',template,tip=True,native_tip_error=True)

    def test_emitted_game_member_references_resolve_in_original_hierarchy(self):
        from inspect_java_class import parse_class as declarations
        from inspect_java_bytecode import Reader, parse_constant_pool
        classes = dict(self.original_classes, **{'HintBase.class':self.base_after})
        def resolve(owner, name, desc, field):
            for _ in range(30):
                raw=classes.get(owner+'.class')
                self.assertIsNotNone(raw,'missing shipped reference owner '+owner)
                declared=declarations(raw)
                members=declared.fields if field else declared.methods
                if any(m.name==name and m.descriptor==desc for m in members):return
                r=Reader(raw);r.read(8);p=parse_constant_pool(r);r.u2();r.u2();parent=r.u2()
                if not parent:break
                owner=p.describe(parent)
            self.fail('unresolved shipped member '+owner+'.'+name+desc)
        for raw in (self.base_after,self.menu_after):
            cp,_=parse_class(raw)
            for entry in cp.entries:
                if not isinstance(entry,tuple) or entry[0] not in ('Fieldref','Methodref'):continue
                _,owner,nt=entry;owner=cp.describe(owner)
                if owner.startswith('java/'):continue
                _,name,desc=cp.get(nt)
                resolve(owner,cp.utf8(name),cp.utf8(desc),entry[0]=='Fieldref')
        before, after = declarations(self.base),declarations(self.base_after)
        self.assertEqual((before.major,before.minor,before.fields),(after.major,after.minor,after.fields))

    def test_required_native_action_is_not_an_accept_or_dismissal(self):
        import re
        body = re.search(rb'Hint\("E03_H05_FastShootingA"\)(.*?)Hint\("E03_H05_FastShootingB"\)',
                         self.restrictions, re.S).group(1)
        self.assertIn(b'PauseGame()',body)
        self.assertIn(b'ReleaseCondition()',body)
        self.assertIn(b'Action(_ACTION_TOGGLE_ALTERNATE)',body)
        self.assertEqual(hints.NATIVE_ACTION,20)
        cp, methods = parse_class(self.base_after)
        method = next(m for m in methods if m.name==hints.PRESENTATION_METHOD)
        from inspect_java_bytecode import disassemble
        instructions = '\n'.join(disassemble(method.code,cp))
        self.assertNotIn('putfield',instructions)
        self.assertNotIn('putstatic',instructions)
        for forbidden in ('DisableCurrentHint','ExecuteInput','GetTipText','GetInputAction','ReleaseConditions'):
            self.assertNotIn(forbidden,instructions)

    def execute(self, hint_id, resource, localized, *, tip=False, conditions=((20,),),
                tip_template='Press %s to continue',native_tip='NATIVE_TIP',native_tip_error=False):
        """Execute production helper bytes, rejecting all mutation/unknown calls."""
        cp, methods = parse_class(self.base_after)
        name=hints.TIP_PRESENTATION_METHOD if tip else hints.PRESENTATION_METHOD
        code = next(m.code for m in methods if m.name==name)
        obj = {'m_sId': hint_id, 'm_sText': resource, 'm_cReleaseConditions':conditions}
        locals_ = [obj]*5
        stack=[];pc=0;native_tip_calls=0
        for _ in range(500):
            start=pc;op=code[pc];pc+=1
            if 0x2a<=op<=0x2d: stack.append(locals_[op-0x2a])
            elif 0x4b<=op<=0x4e: locals_[op-0x4b]=stack.pop()
            elif op in (0x03,0x04):stack.append(op-0x03)
            elif op==0x10:stack.append(struct.unpack_from('b',code,pc)[0]);pc+=1
            elif op==0x13:
                i=struct.unpack_from('>H',code,pc)[0];pc+=2
                tag,value=cp.get(i);self.assertEqual(tag,'String');stack.append(cp.utf8(value))
            elif op==0xb4:
                i=struct.unpack_from('>H',code,pc)[0];pc+=2
                _,_,nt=cp.get(i);_,n,_=cp.get(nt);stack.append(stack.pop()[cp.utf8(n)])
            elif op in (0xb6,0xb8):
                i=struct.unpack_from('>H',code,pc)[0];pc+=2
                name=cp.describe(i)
                if name=='java/lang/String.equals(Ljava/lang/Object;)Z':
                    arg=stack.pop();receiver=stack.pop();stack.append(receiver==arg)
                elif name=='Text.Get(Ljava/lang/String;)Ljava/lang/String;':
                    key=stack.pop()
                    if key=='&MenuHint_PressKey&':stack.append(tip_template)
                    else:self.assertEqual(key,resource);stack.append(localized)
                elif name=='HintBase.GetText()Ljava/lang/String;':
                    self.assertIs(stack.pop(),obj);stack.append('NATIVE')
                elif name=='HintBase.GetTipText()Ljava/lang/String;':
                    self.assertIs(stack.pop(),obj);native_tip_calls+=1
                    if native_tip_error:raise RuntimeError('native assignment failure')
                    stack.append(native_tip)
                elif name in ('HintReleaseConditions.GetSize()I','HintReleaseCondition.GetSize()I'):
                    stack.append(len(stack.pop()))
                elif name in ('HintReleaseConditions.GetConditionAt(I)LHintReleaseCondition;',
                              'HintReleaseCondition.GetActionAt(I)LHintReleaseAction;'):
                    index=stack.pop();stack.append(stack.pop()[index])
                elif name=='HintReleaseAction.GetActionId()I':stack.append(stack.pop())
                else: self.fail('unapproved presentation call '+name)
            elif op in (0x99,0xa7,0xa0,0xc6):
                delta=struct.unpack_from('>h',code,pc)[0];pc+=2
                if op==0xa7:take=True
                elif op==0x99:take=not stack.pop()
                elif op==0xc6:take=stack.pop() is None
                else:take=stack.pop()!=stack.pop()
                if take:pc=start+delta
            elif op==0xb0:
                self.assertEqual(native_tip_calls,1 if tip else 0,'native tip checks must execute once')
                return stack.pop()
            else:self.fail('unapproved opcode '+hex(op))
        self.fail('presentation exceeded instruction bound')


if __name__=='__main__':unittest.main()
