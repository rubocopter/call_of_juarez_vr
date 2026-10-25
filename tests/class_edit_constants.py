"""Constant-pool composition must distinguish String literals from Utf8 names."""
from pathlib import Path
import sys
import unittest
sys.path[:0]=[str(Path(__file__).parent),str(Path(__file__).resolve().parents[1]/'tools')]
from reload_wait_probe import original
from coj_reload_wait_probe import ClassEdit
from inspect_java_bytecode import Reader,parse_constant_pool


class ConstantTests(unittest.TestCase):
    def test_literal_does_not_replace_utf8_symbol_when_editing_a_derivative(self):
        first=ClassEdit(original('ArmedPlayerBeing'))
        name='cojvrFixtureSharedLiteral'
        utf=first.utf8(name)
        first.append(b'\x08'+utf.to_bytes(2,'big'))
        second=ClassEdit(first.finish())
        actual=second.utf8(name)
        self.assertEqual(second.pool.get(actual),('Utf8',name))
        second.cls(name)
        reader=Reader(second.finish());reader.read(8)
        cp=parse_constant_pool(reader)
        for i,e in enumerate(cp.entries):
            if i and e is not None:cp.describe(i)


if __name__=='__main__':unittest.main()
