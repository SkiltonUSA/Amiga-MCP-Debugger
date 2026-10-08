"""Distribution boundary checks: release identity, icon stack, and archive allowlist."""
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('package_fractal',ROOT/'scripts/package_fractal.py')
pkg=importlib.util.module_from_spec(spec);spec.loader.exec_module(pkg)
class PackageTests(unittest.TestCase):
    def test_icon_stack_and_type_required(self):
        data=(ROOT/'amiga/distribution/ZZFractal/ZZFractal.info').read_bytes()
        pkg.check_icon(data,3)
        with self.assertRaises(ValueError):pkg.check_icon(data[:40],3)
        bad=bytearray(data);bad[74:78]=(4096).to_bytes(4,'big')
        with self.assertRaises(ValueError):pkg.check_icon(bad,3)
        with self.assertRaises(ValueError):pkg.check_icon(data,2)
    def test_refuses_debug_build_and_mismatched_executable(self):
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);binary=b'\0\0\x03\xf3test';(d/'ZZFractal').write_bytes(binary)
            record={'build_id':123,'standalone_release':False,'files':{'ZZFractal':hashlib.sha256(binary).hexdigest()}}
            (d/'build.json').write_text(json.dumps(record))
            with self.assertRaises(ValueError):pkg.package(d,d/'out')
            record['standalone_release']=True;record['files']['ZZFractal']='wrong'
            (d/'build.json').write_text(json.dumps(record))
            with self.assertRaises(ValueError):pkg.package(d,d/'out')
    def test_exact_payloads_no_stale_output_and_repeatable_zip(self):
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);binary=b'\0\0\x03\xf3test';(d/'ZZFractal').write_bytes(binary)
            record={'build_id':123,'standalone_release':True,'files':{'ZZFractal':hashlib.sha256(binary).hexdigest()}}
            (d/'build.json').write_text(json.dumps(record))
            stage=d/'out/package';stage.mkdir(parents=True);(stage/'private.txt').write_text('must not ship')
            with contextlib.redirect_stdout(io.StringIO()):archive=pkg.package(d,d/'out')
            first=archive.read_bytes()
            with zipfile.ZipFile(archive) as z:
                self.assertNotIn('private.txt',z.namelist());self.assertEqual(len(z.namelist()),8)
                self.assertEqual(z.read('ZZFractal/ZZFractal'),binary)
                self.assertEqual((z.getinfo('ZZFractal/ZZFractal').external_attr>>16)&0o777,0o755)
                for line in z.read('SHA256SUMS.txt').decode().splitlines():
                    digest,name=line.split('  ');self.assertEqual(hashlib.sha256(z.read(name)).hexdigest(),digest)
            with contextlib.redirect_stdout(io.StringIO()):pkg.package(d,d/'out')
            self.assertEqual(archive.read_bytes(),first)
