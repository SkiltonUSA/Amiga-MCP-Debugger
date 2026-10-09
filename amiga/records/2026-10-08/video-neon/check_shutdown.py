import asyncio,json,sys
from pathlib import Path
from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client
sys.path.insert(0,'tests/amiga/sdl_fractal')
from live_sdl_fractal import fetch
out=Path('.context/amiga/video-neon');calls=[]
async def main():
 async with streamablehttp_client('http://127.0.0.1:55010/mcp') as (r,w,_):
  async with ClientSession(r,w) as s:
   await s.initialize()
   async def call(name,**args):
    r=await s.call_tool(name,args);t='\n'.join(c.text for c in r.content if c.type=='text');calls.append(dict(tool=name,args=args,text=t))
    assert not r.isError and '[ERR]' not in t and not t.startswith('Error'),t
    return t
   assert 'Sixies.ARM.Debug.Owner' not in await call('amiga_list_ports')
   await call('amiga_run_script',script='Run >NIL: Execute RAM:ZZVideoNEON/run-controls',timeout=10)
   await asyncio.sleep(3)
   await call('amiga_arm_attach',client='zzvideo')
   a=json.loads(await call('amiga_arm_pause',client='zzvideo',timeout=5))
   await asyncio.sleep(1)
   b=json.loads(await call('amiga_arm_status',client='zzvideo'))
   assert a['state']==b['state']=='paused' and a['checkpoint_hits']==b['checkpoint_hits']
   await call('amiga_arm_read_memory',client='zzvideo',region=0,offset=0,size=64)
   await call('amiga_call_hook',client='zzvideo',hook='video',args='quit')
   await asyncio.sleep(3)
   assert 'Sixies.ARM.Debug.Owner' not in await call('amiga_list_ports')
   data=fetch('RAM:ZZVideoNEON/controls.log');(out/'controls.log').write_bytes(data);text=data.decode('latin1')
   assert 'VIDEO exit=0' in text and 'LAUNCHER exit=0' in text and 'EXIT epilogue=52455431' in text,text
   assert 'CACHE original=08c50878 running=08c51878 restored=08c50878' in text
   assert 'witness=42 context_equal=1' in text,text
   await call('amiga_ping');await call('amiga_list_screen_windows')
try:asyncio.run(main())
finally:(out/'shutdown.json').write_text(json.dumps(calls,indent=2)+'\n')
print('PASS: stable cooperative pause, memory read, quit while paused, SCTLR restored, RET1, exit 0 and owner released.')
