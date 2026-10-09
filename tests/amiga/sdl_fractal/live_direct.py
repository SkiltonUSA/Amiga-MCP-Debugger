"""Opt-in hardware acceptance of an already started --direct developer build."""
import asyncio
import json
from pathlib import Path
import re
import statistics
import time
from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client
from live_sdl_fractal import fetch, frame_reference, View, fnv, ROOT, ENDPOINT
OUT=ROOT/'.context/amiga/sdl-fractal-direct'

async def main():
    record={'passed':False,'build':json.loads((OUT/'build.json').read_text()),'calls':[],'renders':[]}
    async with streamablehttp_client(ENDPOINT) as (read,write,_):
      async with ClientSession(read,write) as session:
        await session.initialize()
        async def call(tool,**args):
            res=await session.call_tool(tool,args)
            text='\n'.join(c.text for c in res.content if c.type=='text')
            record['calls'].append({'tool':tool,'args':args,'text':text})
            if res.isError or '[ERR]' in text:raise RuntimeError(text)
            return text
        async def hook(args='status'):
            text=await call('amiga_call_hook',client='sdlfractal',hook='sdlfractal',args=args)
            d=dict(re.findall(r'(\w+)=([^\s]+)',text));assert d.get('ok')=='1',text
            assert d.get('error','0')=='0',text;return d
        async def complete():
            for _ in range(900):
                st=await hook()
                if st['running']=='0' and st['pending']=='0':assert st['tiles']=='150',st;return st
                await asyncio.sleep(.1)
            raise AssertionError('Render timed out')
        cache={}
        async def verify(label,readback=True):
            st=await complete();key=tuple(int(st[n]) for n in ('cx','cy','step','limit'))
            if key not in cache:cache[key]=await asyncio.to_thread(frame_reference,View(*key))
            assert st['hash']==f'{fnv(cache[key]):08x}',st
            direct=await hook('direct');metrics=await hook('metrics')
            if direct['enabled']=='1':assert direct['counts_readback']=='0',direct
            item={'label':label,'state':st,'metrics':metrics,'direct_before_verification':direct}
            if readback:
                await hook('save');actual=await asyncio.to_thread(fetch,'RAM:SixiesDev/sdl-fractal-counts.bin')
                assert actual==cache[key],label;item['all_counts_match_independent_oracle']=True
                if st['depth']!='0':
                    item['pixels']=await hook('pixels');assert item['pixels']['mismatches']=='0'
            record['renders'].append(item);print(label,st['ms'],metrics,direct,flush=True)
            return item
        try:
            await hook('automation on')
            await hook('arm');await verify('direct default')
            stable=await hook('metrics');await asyncio.sleep(.2);assert stable==await hook('metrics')
            for i in range(5):await hook('arm');await verify('direct repeat '+str(i),False)
            await hook('zoom 160 120');await verify('direct zoom')
            await hook('pan 1 0');await verify('direct pan')
            await hook('iterations 256');await verify('direct 256 iterations')
            await hook('zoom 160 120');await verify('direct deeper zoom 256 iterations')
            await hook('out');await hook('cpu');await verify('CPU same zoom and 256 iterations')
            await hook('reset');await verify('CPU default')
            for depth in (16,0):
                await hook('screen '+str(depth));await hook('arm');await verify('fallback '+str(depth))
            await hook('screen 32');await hook('arm');await complete()
            await hook('iconify');assert (await hook())['hidden']=='1'
            await hook('arm');await complete();await hook('restore');record['restore_pixels']=await hook('pixels')
            await hook('cover');await hook('arm');await complete();await hook('uncover')
            record['uncovered_pixels']=await hook('pixels')
            screens=await call('amiga_list_screens');screen=re.search(r'SDL ZZFractal[^\n]*@([0-9A-Fa-f]+)',screens).group(1)
            windows=await call('amiga_list_screen_windows',screen=screen)
            address=re.search(r'SDL ZZFractal 0.4 Direct[^\n]*@([0-9A-Fa-f]+)',windows).group(1)
            await call('amiga_window_move',window=address,x=150,y=70)
            await asyncio.sleep(.3);record['moved_pixels']=await hook('pixels')
            await call('amiga_arm_attach',client='sdlfractal',points_path=str(OUT/'fractal-points.json'))
            await call('amiga_arm_pause',client='sdlfractal')
            await hook('arm');await asyncio.sleep(.1);assert int((await hook())['pending'])>0
            await hook('cancel')
            for _ in range(50):
                st=await hook()
                if st['pending']=='0':break
                await asyncio.sleep(.05)
            assert st['running']=='0' and st['pending']=='0',st
            record['cancel_paused']=st
            await call('amiga_arm_detach',client='sdlfractal')
            for i in range(3):
                await hook('arm');await hook('zoom 160 120');await hook('cancel')
                for _ in range(50):
                    if (await hook())['pending']=='0':break
                    await asyncio.sleep(.05)
            await hook('reset');await verify('final default')
            record['screenshot']=await call('amiga_screenshot',window='SDL ZZFractal 0.4 Direct')
            await call('amiga_arm_attach',client='sdlfractal',points_path=str(OUT/'fractal-points.json'))
            await call('amiga_arm_pause',client='sdlfractal')
            await hook('arm');await asyncio.sleep(.1)
            record['quit_while_paused']=True
            record['passed']=True
        finally:
            try:await hook('quit')
            finally:
                for _ in range(50):
                    if 'sdlfractal(' not in await call('amiga_list_clients'):break
                    await asyncio.sleep(.1)
                await asyncio.sleep(.5)
                data=await asyncio.to_thread(fetch,'RAM:ZZFractalDirect/run.log');(OUT/'acceptance.log').write_bytes(data)
                record['clean_exit']=b'EXIT epilogue=52455431' in data and b'LAUNCHER exit=0; owned memory released' in data
                record['postflight_screens']=await call('amiga_list_screens')
                (OUT/'acceptance.json').write_text(json.dumps(record,indent=2)+'\n')
                assert record['clean_exit'],data.decode('latin1')
    assert record['passed'];print('Direct framebuffer acceptance passed; clean shutdown.',flush=True)
if __name__=='__main__':asyncio.run(main())
