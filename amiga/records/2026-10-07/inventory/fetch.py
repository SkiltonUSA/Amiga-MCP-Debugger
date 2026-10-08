import urllib.request,urllib.parse,json,sys
from pathlib import Path
base='http://127.0.0.1:55010/api/'
def get(endpoint,args):
 return json.load(urllib.request.urlopen(base+endpoint+'?'+urllib.parse.urlencode(args),timeout=60))
def fetch(path,dest,size=None):
 if size is None:
  if '/' in path: parent,name=path.rsplit('/',1)
  else: parent,name=path.split(':',1);parent+=':'
  j=get('dir',{'path':parent})
  size=next(e['size'] for e in j['entries'] if e['name'].lower()==name.lower())
 data=bytearray()
 for off in range(0,size,2048):
  j=get('file',{'path':path,'offset':off,'size':min(2048,size-off)})
  chunk=bytes.fromhex(j['hexData']);assert len(chunk)==min(2048,size-off),(path,off,j)
  data.extend(chunk)
 Path(dest).write_bytes(data)
 return bytes(data)
if __name__=='__main__':
 r=Path('.context/amiga/system-report')
 for name,path in json.load(open(r/'base-files.json')).items():
  data=fetch(path,r/(name+'.txt'));print(name,len(data),data[:110].decode('latin1').replace('\n',' / '))
