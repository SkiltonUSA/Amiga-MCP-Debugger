"""Explicit live video profiling acceptance for already staged experimental builds.

Close unrelated Core1 applications and stage RAM:ZZVideoPerf first (see
 docs/amiga-video-arm-execution.md). Run from the workspace with its venv:
 python tests/amiga/video/live_profile.py baseline|icache|scalar|neon small|bframes|ntsc 1|2|3
Use --drawer RAM:ZZVideoNEON --output <path> for the matched NEON experiment.
The staged run scripts use --verify and exit at EOF. This checks actual frame
hashes, cache state restoration and clean release; never runs in the unit suite.
"""
import argparse,asyncio,json,re,sys,time
from pathlib import Path
from datetime import timedelta
from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client
ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT/'tests/amiga/sdl_fractal'))
from live_sdl_fractal import fetch
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('mode',choices=['baseline','icache','scalar','neon'])
parser.add_argument('clip',choices=['small','bframes','ntsc'])
parser.add_argument('index',choices=['1','2','3'])
parser.add_argument('--output',type=Path,default=ROOT/'.context/amiga/video-perf')
parser.add_argument('--drawer',default='RAM:ZZVideoPerf')
args=parser.parse_args();mode,clip,index=args.mode,args.clip,args.index;name=f'{mode}-{clip}-{index}'
if not re.fullmatch(r'RAM:[A-Za-z0-9_-]+',args.drawer):parser.error('Use an existing simple RAM drawer')
out=args.output;out.mkdir(parents=True,exist_ok=True);record={'run':name,'calls':[],'passed':False}
async def main():
 async with streamablehttp_client('http://127.0.0.1:55010/mcp') as (read,write,_):
  async with ClientSession(read,write) as session:
   await session.initialize()
   async def call(tool,**args):
    r=await session.call_tool(tool,args,read_timeout_seconds=timedelta(seconds=45))
    s='\n'.join(c.text for c in r.content if c.type=='text');record['calls'].append({'tool':tool,'args':args,'text':s})
    if r.isError or '[ERR]' in s or s.startswith('Error'):raise RuntimeError(s)
    return s
   ports=await call('amiga_list_ports');assert 'Sixies.ARM.Debug.Owner' not in ports
   await call('amiga_run_script',script=f'Run >NIL: Execute {args.drawer}/run-{name}',timeout=10)
   deadline=time.monotonic()+70
   while time.monotonic()<deadline:
    await asyncio.sleep(2)
    ports=await call('amiga_list_ports')
    if 'Sixies.ARM.Debug.Owner' not in ports:break
   else:raise RuntimeError('Core1 application did not exit; stop and inspect before another launch')
   data=fetch(f'{args.drawer}/{name}.log');(out/(name+'.log')).write_bytes(data);log=data.decode('latin1')
   expected=(ROOT/'.context/amiga/video/tests'/(clip+'.hashes')).read_text().splitlines()
   actual=[' '.join(x) for x in re.findall(r'^FRAME (\d+) hash=([0-9a-f]+)',log,re.M)]
   assert actual==expected,(len(actual),len(expected))
   assert 'MAPPING PASS' in log and 'VIDEO exit=0' in log and 'EXIT epilogue=52455431' in log and 'LAUNCHER exit=0' in log,log
   m=re.search(r'CACHE original=([0-9a-f]+) running=([0-9a-f]+) restored=([0-9a-f]+)',log);assert m,log
   original,running,restored=[int(x,16) for x in m.groups()]
   assert original==restored and original&0x1005==0 and running==(original|(0 if mode=='baseline' else 0x1000))
   if mode=='neon':
    n=re.search(r'NEON cpacr=([0-9a-f]+) fpexc=([0-9a-f]+) fpscr=([0-9a-f]+) mvfr0=([0-9a-f]+) mvfr1=([0-9a-f]+) witness=42 context_equal=1',log)
    assert n,log
    record['neon']={k:'0x'+v for k,v in zip(['cpacr','fpexc','fpscr','mvfr0','mvfr1'],n.groups())}
   eof=re.search(r'EOF frames=(\d+) elapsed_ms=(\d+).*draw_ms=(\d+) retries=(\d+) clock_hz=(\d+)',log)
   frames,elapsed,draw,retries,hz=map(int,eof.groups());assert frames==25 and hz>0
   p=re.search(r'PROFILE decode=(\d+):([0-9a-f]+) colour=(\d+):([0-9a-f]+) hash=(\d+):([0-9a-f]+) copy_ms=(\d+) host_hash_ms=(\d+)',log)
   vals=p.groups();ticks=lambda a,b:(int(a)<<32)|int(b,16)
   record.update(passed=True,frames=frames,fps=frames*1000/elapsed,elapsed_ms=elapsed,clock_hz=hz,retries=retries,
    per_frame_ms={'decode':ticks(*vals[:2])*1000/hz/frames,'colour':ticks(*vals[2:4])*1000/hz/frames,'arm_hash':ticks(*vals[4:6])*1000/hz/frames,'copy':int(vals[6])/frames,'host_hash':int(vals[7])/frames,'draw':draw/frames},sctlr={'original':hex(original),'running':hex(running),'restored':hex(restored)})
   await call('amiga_ping')
try:asyncio.run(main())
finally:(out/(name+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({k:v for k,v in record.items() if k!='calls'},indent=2))
