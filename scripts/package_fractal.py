#!/usr/bin/env python3
"""Stage and ZIP the standalone Amiga app; no firmware or development tools."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import zipfile

ROOT=Path(__file__).resolve().parents[1]
NAME='ZZFractal-0.1-XX19c'

def check_icon(data,kind):
    if len(data)<78 or data[:4]!=b'\xe3\x10\x00\x01':
        raise ValueError('Expected classic Workbench DiskObject')
    if data[48]!=kind or struct.unpack_from('>I',data,74)[0]!=65536:
        raise ValueError('Icon must have correct tool/drawer type and 65536-byte stack')

def package(build_dir,output_dir):
    record=json.loads((build_dir/'build.json').read_text())
    binary=(build_dir/'ZZFractal').read_bytes()
    if not record.get('standalone_release') or binary[:4]!=b'\0\0\x03\xf3':
        raise ValueError('Build the standalone Amiga Hunk with --fractal --release first')
    if hashlib.sha256(binary).hexdigest()!=record['files']['ZZFractal']:
        raise ValueError('Executable does not match its build record')
    assets=ROOT/'amiga/distribution'
    check_icon((assets/'ZZFractal.info').read_bytes(),2)
    check_icon((assets/'ZZFractal/ZZFractal.info').read_bytes(),3)
    stage=output_dir/'package';stage.mkdir(parents=True,exist_ok=True)
    # Explicit allowlist prevents tools, developer records or stale outputs
    # entering an archive. Existing unrelated output files are left alone.
    files={'ZZFractal/ZZFractal':binary,'ZZFractal.info':(assets/'ZZFractal.info').read_bytes()}
    for name in ('ZZFractal.info','ReadMe.txt','ThirdParty.txt','COPYING3','COPYING.RUNTIME'):
        files['ZZFractal/'+name]=(assets/'ZZFractal'/name).read_bytes()
    sums=''.join(hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name,data in sorted(files.items()))
    files['SHA256SUMS.txt']=sums.encode('ascii')
    for name,data in files.items():
        path=stage/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    archive=output_dir/(NAME+'.zip')
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name,data in sorted(files.items()):
            info=zipfile.ZipInfo(name,date_time=(2026,10,8,0,0,0));info.create_system=3
            info.external_attr=((0o100755 if name=='ZZFractal/ZZFractal' else 0o100644)<<16)
            info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None and set(z.namelist())==set(files)
        for name,data in files.items():assert z.read(name)==data
    metadata={'version':'0.1','firmware':'XX19c / XACP 1.7','build_id':record['build_id'],
        'executable_bytes':len(binary),'archive':archive.name,'archive_bytes':archive.stat().st_size,
        'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
        'files':{n:hashlib.sha256(d).hexdigest() for n,d in sorted(files.items())}}
    (output_dir/'package.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(json.dumps(metadata,indent=2))
    return archive

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build-dir',type=Path,default=ROOT/'.context/amiga/fractal-release')
    p.add_argument('--output-dir',type=Path,default=ROOT/'.context/amiga/fractal-release/dist')
    a=p.parse_args();package(a.build_dir,a.output_dir)
