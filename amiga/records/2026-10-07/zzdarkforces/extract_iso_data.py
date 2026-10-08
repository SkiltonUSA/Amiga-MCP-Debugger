# Archived extractor for the recorded StarWarsDarkForces.iso extent map only.
# Usage: python3 extract_iso_data.py /path/to/StarWarsDarkForces.iso
from pathlib import Path
import json,struct,hashlib,sys
r=Path(__file__).resolve().parent;iso=Path(sys.argv[1]);rows=json.loads((r/'iso-extents.json').read_text());dest=r/'iso-extracted';dest.mkdir(exist_ok=True);manifest=[]
with iso.open('rb') as f:
 for row in rows:
  if not row['path'].startswith('DARK/') or row['directory']:continue
  assert row['complete'];p=Path(row['path']);assert not p.is_absolute() and '..' not in p.parts
  f.seek(row['offset']);b=f.read(row['bytes']);assert len(b)==row['bytes']
  o=dest/p;o.parent.mkdir(parents=True,exist_ok=True);o.write_bytes(b)
  if p.suffix.lower()=='.gob':
   assert b[:4]==b'GOB\n',(p,b[:4]);off=struct.unpack_from('<I',b,4)[0];count=struct.unpack_from('<I',b,off)[0];assert off+4+count*21<=len(b)
   for i in range(count):
    a,n=struct.unpack_from('<II',b,off+4+i*21);assert a+n<=len(b),(p,i,a,n)
   print(p.name,'GOB valid',count,'entries',len(b),'bytes')
  if p.suffix.lower()=='.lfd':
   off=0;chunks=0
   while off<len(b):
    assert off+16<=len(b),(p,off,'short header');n=struct.unpack_from('<I',b,off+12)[0];off+=16+n;chunks+=1;assert off<=len(b),(p,off,len(b))
   assert off==len(b)
  manifest.append({**row,'sha256':hashlib.sha256(b).hexdigest(),'md5':hashlib.md5(b).hexdigest(),'install':p.suffix.lower()!='.exe','destination_relative':str(p.relative_to('DARK'))})
(r/'iso-data-manifest.json').write_text(json.dumps(manifest,indent=2))
print('Extracted',len(manifest),'files;',sum(x['install'] for x in manifest),'data files to install;',sum(x['bytes'] for x in manifest if x['install']),'bytes')
