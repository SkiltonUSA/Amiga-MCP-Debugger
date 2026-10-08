"""Sequential opt-in physical performance runs; requires an idle staged developer app.
Never run concurrently with another bridge/MCP consumer. Does not launch programs.
"""
import argparse,asyncio,json,re,sys,time,statistics
from pathlib import Path
from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client
from live_sdl_fractal import fetch
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tests/amiga/fractal'))
from test_fractal import View,frame_reference,fnv
async def main():
 p=argparse.ArgumentParser();p.add_argument('phase');args=p.parse_args()
 out=ROOT/'.context/amiga/sdl-performance'/args.phase;out.mkdir(parents=True,exist_ok=True)
 record={'passed':False,'phase':args.phase,'build':json.loads((out/'build.json').read_text()),'renders':[],'calls':[]}
 cache={}
 async with streamablehttp_client('http://127.0.0.1:55010/mcp') as (r,w,_):
  async with ClientSession(r,w) as s:
   await s.initialize()
   async def call(n,**a):
    res=await s.call_tool(n,a);t='\n'.join(getattr(x,'text','') for x in res.content)
    record['calls'].append(dict(tool=n,args=a,text=t))
    if res.isError:raise RuntimeError(t)
    return t
   async def hook(cmd='status'):
    t=await call('amiga_call_hook',client='sdlfractal',hook='sdlfractal',args=cmd)
    d=dict(re.findall(r'(\w+)=([^\s]+)',t));assert d.get('ok')=='1' and d.get('error','0')=='0',t;return d
   async def complete():
    until=time.monotonic()+150
    while time.monotonic()<until:
     st=await hook()
     if st['running']=='0' and st['pending']=='0':assert st['tiles']=='150',st;return st
     await asyncio.sleep(.25)
    raise AssertionError('Render timed out')
   async def verify(label,full):
    st=await complete();key=tuple(int(st[k]) for k in ('cx','cy','step','limit'))
    if key not in cache:cache[key]=await asyncio.to_thread(frame_reference,View(*key))
    expected=cache[key];hash_matches=st['hash']==f'{fnv(expected):08x}'
    metrics=await hook('metrics');assert metrics['gen']==st['gen'] and metrics['running']=='0'
    if full or not hash_matches:
     await hook('save');actual=await asyncio.to_thread(fetch,'RAM:SixiesDev/sdl-fractal-counts.bin')
     if actual!=expected:
      (out/'mismatch-actual.bin').write_bytes(actual);(out/'mismatch-expected.bin').write_bytes(expected)
      record['failed_render']=dict(label=label,state=st,metrics=metrics)
     assert actual==expected,(label,st)
    assert hash_matches,st
    scheduling=await hook('scheduling') if args.phase!='baseline' else None
    record['renders'].append(dict(label=label,state=st,metrics=metrics,scheduling=scheduling,full_frame_checked=full))
    print(label,metrics,flush=True)
   try:
    record['preflight']=await call('amiga_ping')
    for _ in range(30):
     if 'sdlfractal' in await call('amiga_list_clients'):break
     await asyncio.sleep(.2)
    await hook('automation on');await complete()
    for i in range(3):
     for mode in (('arm','cpu') if i%2==0 else ('cpu','arm')):
      await hook(mode);await verify(f'default {mode} {i+1}',i==0)
    await hook('zoom 160 120');await verify('zoom CPU',True)
    await hook('arm');await verify('zoom ARM',True)
    record['passed']=True
   finally:
    try:
     if 'sdlfractal' in await call('amiga_list_clients'):await call('amiga_call_hook',client='sdlfractal',hook='sdlfractal',args='quit')
     for _ in range(50):
      await asyncio.sleep(.1)
      if 'sdlfractal' not in await call('amiga_list_clients'):break
     record['postflight']=await call('amiga_ping')
     log=await asyncio.to_thread(fetch,'RAM:SixiesDev/sdl-performance.log');(out/'run.log').write_bytes(log)
     record['clean_exit']=b'epilogue=52455431' in log and bool(re.search(rb'LAUNCHER exit=(?:0|20); owned memory released',log))
     assert record['clean_exit'], 'Worker ownership was not released safely'
     if record['passed']:assert b'LAUNCHER exit=0; owned memory released' in log
    finally:(out/'acceptance.json').write_text(json.dumps(record,indent=2)+'\n')
 for mode in ['ARM','CPU']:
  group=[int(r['metrics']['wall_us']) for r in record['renders'] if r['label'].startswith('default') and r['state']['mode']==mode]
  print(mode,'default median us',statistics.median(group),flush=True)
 print('Performance phase passed',args.phase,flush=True)
if __name__=='__main__':asyncio.run(main())
