"""Verify exact dynamite observation sites without starting the game."""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile

from bow_draw_patch import verifier_class
from inspect_java_bytecode import disassemble, parse_class
from patch_coj_weapon_consumers import PAK_SHA256, patch_archive


GAME = Path(os.environ.get("COJ_GAME_DIRECTORY",
    "C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez"))


def methods(data):
    pool, entries = parse_class(data)
    return {(entry.name, entry.descriptor): disassemble(entry.code, pool)
            for entry in entries if entry.code is not None}


def references(lines):
    return [line.split(" // ", 1)[1] for line in lines if " // " in line]


class DynamiteTraceTests(unittest.TestCase):
    def test_exact_native_receipts_and_shipped_verifier(self):
        source = next((p for p in (GAME / "code.pak", GAME / "code.cojvr-backup.pak")
                       if p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest() == PAK_SHA256), None)
        if source is None:
            self.skipTest("Exact original code.pak unavailable")
        with tempfile.TemporaryDirectory(prefix="cojvr-dynamite-") as directory:
            root = Path(directory)
            candidate = root / "code.pak"
            patch_archive(source, candidate, manual_reload=True, dynamite_motion=True)
            with zipfile.ZipFile(source) as stock, zipfile.ZipFile(candidate) as patched:
                for member in ("WeaponDynamiteStick", "WeaponDynamiteStickOld", "WeaponExplosive",
                               "WpnLogicDynamite", "PlayerWeaponStateMashine", "Weapon"):
                    self.assertEqual(patched.read(member + ".class"), stock.read(member + ".class"), member)
                stock_throw = methods(stock.read("WeaponThrow.class"))
                throw = methods(patched.read("WeaponThrow.class"))
                self.assertEqual(set(throw), set(stock_throw))
                changed = ("AttackThrow", "(LWpnAttackThrow;LVector;LVector;Z)V")
                for method in set(throw) - {changed}:
                    self.assertEqual(throw[method], stock_throw[method], method)
                refs = references(throw[changed])
                admit = refs.index("ArmedPlayerBeing.cojvrDynamiteRightOwner(LWeapon;)Z")
                velocity = refs.index("WeaponThrow.SetVelocity(LVector;LVector;)V")
                receipt = refs.index("ArmedPlayerBeing.cojvrObserveDynamiteRelease(LWeapon;LVector;LVector;)V")
                self.assertLess(admit, refs.index("WeaponThrow.ThrowAway()V"))
                self.assertLess(velocity, receipt)
                self.assertEqual([ref for ref in refs if ref.startswith("WeaponThrow.") and
                                  ref not in ("WeaponThrow.cOwnerLPawnInventory;") and ref.endswith("V")],
                                 [ref for ref in references(stock_throw[changed])
                                  if ref.startswith("WeaponThrow.") and ref.endswith("V")])

                player = methods(patched.read("ArmedPlayerBeing.class"))
                refs = references(player["WeaponAttackBegin", "(LWeapon;I)V"])
                self.assertLess(refs.index("Weapon.AttackBegin(ZIZ)V"),
                                refs.index("ArmedPlayerBeing.cojvrObserveDynamiteBegin(LWeapon;)V"))
                guard = references(player["cojvrDynamiteRightOwner", "(LWeapon;)Z"])
                self.assertIn("'WeaponDynamiteStick'", guard)
                self.assertIn("ArmedPlayerBeing.IsNetGame()Z", guard)
                self.assertIn("ArmedPlayerBeing.GetActualWeapon(I)LWeapon;", guard)
                motion = player["cojvrGetDynamiteMotionOwner", "()LWeapon;"]
                for getter in ("GetActualWeapon", "GetActiveWeapon", "GetDesiredWeapon"):
                    ref = f"ArmedPlayerBeing.{getter}(I)LWeapon;"
                    calls = [i for i,line in enumerate(motion) if line.endswith(ref)]
                    self.assertEqual(len(calls), 2)
                    self.assertIn("iconst_0", motion[calls[0]-1])
                    self.assertIn("iconst_1", motion[calls[1]-1])
                    self.assertIn("ifnonnull", motion[calls[1]+1])
                motion_refs = references(motion)
                self.assertIn("ArmedPlayerBeing.cojvrDynamiteRightOwner(LWeapon;)Z", motion_refs)
                self.assertIn("ArmedPlayerBeing.GetWeaponMoveState(LWeapon;)I", motion_refs)
                self.assertNotIn("ArmedPlayerBeing.Attack(II)Z", motion_refs)
                self.assertEqual(patched.read("PlayerController.class"), stock.read("PlayerController.class"))

            java = GAME / "jre/bin/java.exe"
            if java.is_file():
                for target in ("ArmedPlayerBeing", "WeaponThrow"):
                    (root / "VerifyBowDrawPatch.class").write_bytes(verifier_class(target))
                    verified = subprocess.run([str(java), "-Xverify:all", "-cp",
                                               f"{root};{candidate}", "VerifyBowDrawPatch"],
                                              capture_output=True, text=True, timeout=30)
                    self.assertEqual(verified.returncode, 0, verified.stdout + verified.stderr)


if __name__ == "__main__":
    unittest.main()
