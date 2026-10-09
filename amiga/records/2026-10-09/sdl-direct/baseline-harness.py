import asyncio,json,re,sys,statistics
from pathlib import Path
sys.path.insert(0,'tests/amiga/sdl_fractal')
from live_sdl_fractal import ClientSession,streamablehttp_client,ENDPOINT,fetch
out=Path('.context/amiga/sdl-fractal-direct');record={'renders':[]}
async def main():
 async with streamablehttp_client(ENDPOINT) as (r,w,_):
  async with ClientSession(r,w) as s:
   await s.initialize()
   async def call(name,**kw):
    x=await s.call_tool(name,kw);return '\n'.join(c.text for c in x.content if c.type=='text')
   async def hook(args):
    t=await call('amiga_call_hook',client='sdlfractal',hook='sdlfractal',args=args)
    d=dict(re.findall(r'(\w+)=([^\s]+)',t));assert d.get('ok')=='1',t;return d
   async def done():
    for _ in range(150):
     d=await hook('status')
     if d['running']=='0' and d['pending']=='0' and d['tiles']=='150':assert d['hash']=='fb32f6c6',d;return d
     await asyncio.sleep(.1)
    raise AssertionError('timeout')
   try:
    await done();await hook('automation on')
    for depth in (0,32):
     await hook('screen '+str(depth))
     for mode in ('arm','cpu'):
      for i in range(5):
       await hook(mode);st=await done();m=await hook('metrics')
       record['renders'].append({'depth':depth,'mode':mode,'state':st,'metrics':m})
       print(depth,mode,i,m['wall_us'],flush=True)
   finally:
    await hook('quit');await asyncio.sleep(1)
    data=await asyncio.to_thread(fetch,'RAM:ZZFractalDirect/baseline.log');(out/'baseline.log').write_bytes(data)
    record['clean_exit']=b'LAUNCHER exit=0' in data
    (out/'baseline.json').write_text(json.dumps(record,indent=2)+'\n')
asyncio.run(main())
