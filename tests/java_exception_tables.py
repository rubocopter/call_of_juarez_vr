"""Execute emitted catch-all/finally bytecode without javac or a game VM."""
import inspect
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).parent))
from reload_wait_probe import VM, original
from coj_reload_wait_probe import ClassEdit
from inspect_java_bytecode import parse_class


class JavaExceptionTableTests(unittest.TestCase):
    def helper(self, code, table):
        self.assertIn('exception_table', inspect.signature(ClassEdit.helper).parameters)
        edit = ClassEdit(original('ArmedPlayerBeing'))
        edit.helper('cojvrExceptionFixture', '(Ljava/lang/Throwable;)I', code, 3,
                    exception_table=table)
        pool, methods = parse_class(edit.finish())
        vm = VM()
        vm.classes['ArmedPlayerBeing'] = pool, {(m.name, m.descriptor): m for m in methods}
        return vm

    def test_catch_all_preserves_exception_handler_and_executes_cleanup(self):
        vm = self.helper(bytes.fromhex('2b bf 4d 04 ac'), ((0, 2, 2, 0),))
        method = vm.classes['ArmedPlayerBeing'][1][('cojvrExceptionFixture', '(Ljava/lang/Throwable;)I')]
        self.assertEqual(method.exception_table, ((0, 2, 2, 0),))
        self.assertEqual(vm.run('ArmedPlayerBeing', 'cojvrExceptionFixture',
                                '(Ljava/lang/Throwable;)I', vm.player, [{'class': 'Throwable'}]), 1)

    def test_invalid_handler_or_region_rejected_before_emission(self):
        self.assertIn('exception_table', inspect.signature(ClassEdit.helper).parameters)
        for table in (((0, 0, 0, 0),), ((0, 8, 1, 0),), ((0, 2, 5, 0),), ((-1, 2, 1, 0),)):
            with self.subTest(table=table):
                edit = ClassEdit(original('ArmedPlayerBeing'))
                with self.assertRaises(ValueError):
                    edit.helper('cojvrExceptionFixture', '(Ljava/lang/Throwable;)I',
                                bytes.fromhex('2b bf 4d 04 ac'), 3, exception_table=table)

    def test_finally_can_rethrow_same_exception_object(self):
        vm = self.helper(bytes.fromhex('2b bf 4d 2c bf'), ((0, 2, 2, 0),))
        import reload_wait_probe
        exception = {'class': 'Throwable', 'identity': object()}
        with self.assertRaises(reload_wait_probe.JVMThrown) as caught:
            vm.run('ArmedPlayerBeing', 'cojvrExceptionFixture',
                   '(Ljava/lang/Throwable;)I', vm.player, [exception])
        self.assertIs(caught.exception.value, exception)

    def test_operand_offsets_and_nonclass_catch_type_are_rejected(self):
        code=bytes.fromhex('10 01 57 2b bf 4d 04 ac')
        for boundary in ('start', 'end', 'handler', 'type'):
            with self.subTest(boundary=boundary):
                edit=ClassEdit(original('ArmedPlayerBeing'))
                table=((1 if boundary=='start' else 0,
                        1 if boundary=='end' else 5,
                        1 if boundary=='handler' else 5,
                        edit.utf8('NotAClass') if boundary=='type' else 0),)
                with self.assertRaises(ValueError):
                    edit.helper('cojvrExceptionFixture','(Ljava/lang/Throwable;)I',code,3,
                                exception_table=table)

    def test_real_class_catch_type_and_code_end_boundary_are_retained(self):
        edit=ClassEdit(original('ArmedPlayerBeing'))
        catch=edit.cls('java/lang/Throwable')
        code=bytes.fromhex('2b bf 4d 04 ac')
        edit.helper('cojvrExceptionFixture','(Ljava/lang/Throwable;)I',code,3,
                    exception_table=((0,len(code),2,catch),))
        pool, methods=parse_class(edit.finish())
        method=next(m for m in methods if m.name=='cojvrExceptionFixture')
        self.assertEqual(pool.describe(method.exception_table[0][3]),'java/lang/Throwable')

    def test_switch_and_wide_operands_are_not_instruction_starts(self):
        from inspect_java_bytecode import instruction_offsets
        import struct
        switch=(b'\xaa\0\0\0'+struct.pack('>iiii',20,0,0,20)+b'\xb1')
        self.assertEqual(instruction_offsets(switch),frozenset((0,20)))
        self.assertEqual(instruction_offsets(bytes.fromhex('c4 84 01 00 00 01 b1')),
                         frozenset((0,6)))
        for bad in (switch[:-3],bytes.fromhex('c4 00 00 01'),bytes.fromhex('b6 01'),b'\xff'):
            with self.assertRaises(ValueError):instruction_offsets(bad)


if __name__ == '__main__': unittest.main()
