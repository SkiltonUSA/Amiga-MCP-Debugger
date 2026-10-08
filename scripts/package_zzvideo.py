#!/usr/bin/env python3
"""Package the standalone video preview, native icons and an explicit test clip."""
import argparse,hashlib,json,struct,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def package(build,demo,out):
    record=json.loads((build/'ZZVideo-build.json').read_text());binary=(build/'ZZVideo').read_bytes()
    if record['developer'] or binary[:4]!=b'\0\0\x03\xf3' or hashlib.sha256(binary).hexdigest()!=record['files']['ZZVideo']:
        raise ValueError('Standalone executable/build record mismatch')
    assets=ROOT/'amiga/video/distribution';name='ZZVideo-0.1';files={name+'/ZZVideo':binary,name+'/demo.mpg':demo.read_bytes()}
    if not 0<len(files[name+'/demo.mpg'])<=4*1024*1024:raise ValueError('Demo must fit preview limit')
    for src,dest,kind in [('drawer.info',name+'.info',2),('ZZVideo.info',name+'/ZZVideo.info',3)]:
        data=(assets/src).read_bytes()
        if data[:4]!=b'\xe3\x10\0\1' or data[48]!=kind or struct.unpack_from('>I',data,74)[0]!=131072:
            raise ValueError('Invalid native icon/stack')
        files[dest]=data
    for n in ['ReadMe.txt','ThirdParty.txt','COPYING3','COPYING.RUNTIME','LICENSE-SDL.txt']:files[name+'/'+n]=(assets/n).read_bytes()
    files[name+'/LICENSE-MIT.txt']=(ROOT/'amiga/video/vendor/LICENSE-MIT.txt').read_bytes()
    sums=''.join(hashlib.sha256(d).hexdigest()+'  '+n+'\n' for n,d in sorted(files.items()))
    files['SHA256SUMS.txt']=sums.encode();out.mkdir(parents=True,exist_ok=True)
    archive=out/(name+'-XX19c.zip')
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for n,d in sorted(files.items()):
            i=zipfile.ZipInfo(n,date_time=(2026,10,8,0,0,0));i.create_system=3;i.compress_type=zipfile.ZIP_DEFLATED
            i.external_attr=((0o100755 if n==name+'/ZZVideo' else 0o100644)<<16);z.writestr(i,d)
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None or any(z.read(n)!=d for n,d in files.items()):raise ValueError('Archive integrity failure')
    result=dict(archive=archive.name,bytes=archive.stat().st_size,sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
        executable_sha256=record['files']['ZZVideo'],build_id=record['build_id'],members=len(files))
    (out/'package.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build-dir',type=Path,default=ROOT/'.context/amiga/video');p.add_argument('--demo',type=Path,required=True)
    p.add_argument('--output-dir',type=Path,default=ROOT/'.context/amiga/video/dist');a=p.parse_args()
    package(a.build_dir,a.demo,a.output_dir)
