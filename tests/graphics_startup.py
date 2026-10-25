import subprocess,sys,tempfile,unittest
from pathlib import Path
EXE=sys.argv.pop(1)
class Startup(unittest.TestCase):
    def run_profile(self,path,*extra):return subprocess.run([EXE,'--file',str(path),*extra],capture_output=True,text=True,timeout=10)
    def test_profiles_strict_old_migration_and_missing_or_bad_file(self):
        base='version=4\nenabled=false\nsharpness=0\ncontrast=1\nsaturation=1\ngamma=1\nedge_smoothing=false\nreconstruction=native\n'
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/'graphics.ini'
            result=self.run_profile(path);self.assertEqual((result.returncode,result.stdout.strip()),(0,'Auto'))
            for index,profile in enumerate(('Auto','75%','100%','125%','150%','200%')):
                path.write_text(base+f'render_scale={index}\nhorse_comfort=0\n',encoding='ascii')
                before=path.read_bytes();stamp=path.stat().st_mtime_ns
                self.assertEqual(self.run_profile(path).stdout.strip(),profile)
                self.assertEqual((path.read_bytes(),path.stat().st_mtime_ns),(before,stamp))
            for index,profile in enumerate(('Auto','75%','100%','125%','Auto')):
                path.write_text(base.replace('version=4','version=3')+f'render_scale={index}\nhorse_comfort=0\n',encoding='ascii')
                before=path.read_bytes()
                self.assertEqual((self.run_profile(path).returncode,self.run_profile(path).stdout.strip()),(0,profile))
                self.assertEqual(path.read_bytes(),before)
            for version in (1,2):
                path.write_text(base.replace('version=4',f'version={version}').replace('reconstruction=native\n','' if version==1 else 'reconstruction=native\n'),encoding='ascii')
                self.assertEqual(self.run_profile(path).stdout.strip(),'Auto')
            path.write_text(base.replace('version=4','version=3')+'render_scale=5\nhorse_comfort=0\n',encoding='ascii')
            self.assertEqual(self.run_profile(path).returncode,3)
            path.write_text(base+'render_scale=6\nhorse_comfort=0\n',encoding='ascii')
            self.assertEqual(self.run_profile(path).returncode,3)
            self.assertEqual(subprocess.run([EXE,'--unknown',str(path)],capture_output=True).returncode,2)
if __name__=='__main__':unittest.main()
