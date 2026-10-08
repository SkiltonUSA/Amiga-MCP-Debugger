"""Explicit hardware acceptance for the staged developer SDL fractal. One MCP client.
Run only after the launcher has been staged and Core1 exclusivity checked.
"""
import asyncio
import json
from pathlib import Path
import re
import struct
import sys
import time
import urllib.parse
import urllib.request
from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tests/amiga/fractal'))
from test_fractal import View,frame_reference,fnv
OUT=ROOT/'.context/amiga/sdl-fractal/acceptance.json'
ENDPOINT='http://127.0.0.1:55010/mcp'
READ_RETRIES=[]

def fetch(path):
    base=ENDPOINT.replace('/mcp','/api/')
    parent,name=path.rsplit('/',1)
    def get(ep,args):
        with urllib.request.urlopen(base+ep+'?'+urllib.parse.urlencode(args),timeout=60) as f:return json.load(f)
    size=next(e['size'] for e in get('dir',{'path':parent})['entries'] if e['name'].lower()==name.lower())
    data=bytearray()
    for off in range(0,size,2048):
        n=min(2048,size-off)
        for attempt in range(3):
            response=get('file',{'path':path,'offset':off,'size':n})
            if 'hexData' in response and len(response['hexData'])==n*2:break
            READ_RETRIES.append({'path':path,'offset':off,'response':response});time.sleep(.15)
        else:raise AssertionError(response)
        chunk=bytes.fromhex(response['hexData']);assert len(chunk)==n;data.extend(chunk)
    return bytes(data)

async def main():
    record={'passed':False,'build':json.loads((OUT.parent/'build.json').read_text()),'calls':[],'renders':[]}
    async with streamablehttp_client(ENDPOINT) as (read,write,_):
      async with ClientSession(read,write) as session:
        await session.initialize()
        async def call(tool,**args):
            res=await session.call_tool(tool,args)
            text='\n'.join(c.text for c in res.content if c.type=='text')
            record['calls'].append({'tool':tool,'args':args,'text':text})
            if res.isError or '[ERR]' in text:raise RuntimeError(text)
            return text
        async def hook(command='status'):
            text=await call('amiga_call_hook',client='sdlfractal',hook='sdlfractal',args=command)
            d=dict(re.findall(r'(\w+)=([^\s]+)',text));assert d.get('ok')=='1',text
            assert d.get('error','0')=='0',text;return d
        async def complete():
            for _ in range(240):
                st=await hook()
                if st['running']=='0' and st['pending']=='0':assert st['tiles']=='150',st;return st
                await asyncio.sleep(.5)
            raise AssertionError('Render timed out')
        cache={}
        async def verify(label):
            st=await complete();key=tuple(int(st[n]) for n in ('cx','cy','step','limit'))
            if key not in cache:cache[key]=await asyncio.to_thread(frame_reference,View(*key))
            want=cache[key];assert st['hash']==f'{fnv(want):08x}',st
            await hook('save');actual=await asyncio.to_thread(fetch,'RAM:SixiesDev/sdl-fractal-counts.bin')
            assert actual==want,label+' pixel mismatch'
            metrics=await hook('metrics')
            assert metrics['gen']==st['gen'] and metrics['running']=='0', 'Input changed the render during measurement'
            assert int(metrics['wall_us'])>0 and int(metrics['draw_us'])>0
            if st['mode']=='ARM':
                assert 320000000<int(metrics['rate'])<345000000,metrics
                assert 0<int(metrics['arm_us_est'])<int(metrics['wall_us']),metrics
                assert int(metrics['transfer_us'])>0 and int(metrics['colour_us'])>0,metrics
            else:assert 0<int(metrics['cpu_us'])<int(metrics['wall_us']),metrics
            record['renders'].append({'label':label,'state':st,'metrics':metrics,'scheduling':await hook('scheduling'),'all_76800_pixels_match':True})
            print(label,st['ms']+' ms',st['hash'],metrics,flush=True)
            return st
        try:
            record['preflight']=await call('amiga_ping')
            for _ in range(50):
                if 'sdlfractal' in await call('amiga_list_clients'):break
                await asyncio.sleep(.2)
            await hook('automation on')
            record['clock']=await hook('clock')
            st=await verify('Workbench ARM default')
            metrics=await hook('metrics');await asyncio.sleep(.3);assert metrics==await hook('metrics'),'Completed metrics drift while idle'
            await hook('cpu');await verify('Workbench CPU default')
            await hook('zoom 160 120');await verify('Workbench CPU zoom')
            await hook('arm');await verify('Workbench ARM zoom')
            await hook('pan 1 0');await verify('Workbench ARM pan')
            await hook('iterations 64');await verify('Workbench ARM 64 iterations')
            await hook('cpu');await verify('Workbench CPU 64 iterations')
            for depth in (16,32):
                st=await hook('screen '+str(depth));assert st['depth']==str(depth)
                record['screen_'+str(depth)]=await call('amiga_list_screens')
                await hook('arm');await verify(str(depth)+'-bit screen ARM')
                await hook('cpu');await verify(str(depth)+'-bit screen CPU')
                record['pixel_readback_'+str(depth)]=await hook('pixels')
                windows=await call('amiga_list_screen_windows')
                record['screen_windows_'+str(depth)]=windows
                shot=await call('amiga_screenshot');record['screenshot_'+str(depth)]=shot
            await hook('screen 0');await hook('reset');await hook('arm');await verify('Workbench restored ARM')
            await call('amiga_arm_attach',client='sdlfractal',points_path=str(OUT.parent/'fractal-points.json'))
            await call('amiga_arm_pause',client='sdlfractal')
            await hook('arm');await asyncio.sleep(.1)
            paused=await hook();assert int(paused['pending'])>0,paused
            await hook('cancel')
            for _ in range(60):
                paused=await hook()
                if paused['pending']=='0':break
                await asyncio.sleep(.02)
            assert paused['pending']=='0' and paused['running']=='0',paused
            record['cancel_while_paused']=paused
            await call('amiga_arm_detach',client='sdlfractal')
            for i in range(6):
                await hook('arm');await asyncio.sleep(.03);await hook('zoom 160 120');await hook('cancel')
                for _ in range(60):
                    st=await hook()
                    if st['pending']=='0':break
                    await asyncio.sleep(.02)
                assert st['pending']=='0' and st['running']=='0',st
            await hook('reset');st=await verify('Final ARM default')
            await hook('iconify');assert (await hook())['hidden']=='1'
            assert 'SDL ZZFractal 0.3' not in await call('amiga_list_screen_windows')
            await hook('restore');assert (await hook())['hash']==st['hash']
            await hook('cover');await hook('arm');await verify('Overlapped ARM render')
            record['covered_screenshot']=await call('amiga_screenshot')
            await hook('uncover')
            windows=await call('amiga_list_screen_windows')
            address=re.search(r'SDL ZZFractal 0.3[^\n]*@([0-9A-Fa-f]+)',windows).group(1)
            await call('amiga_window_move',window=address,x=680,y=300)
            for _ in range(20):
                await asyncio.sleep(.1) # Intuition movement is asynchronous.
                moved=await call('amiga_list_screen_windows')
                if re.search(r'SDL ZZFractal 0.3\s+pos=\(680,300\)',moved):break
            assert re.search(r'SDL ZZFractal 0.3\s+pos=\(680,300\)',moved),moved
            record['moved_window']=moved
            record['final_screenshot']=await call('amiga_screenshot',window='SDL ZZFractal 0.3')
            await hook('arm');await hook('iconify');await complete();await hook('restore')
            assert (await hook())['hash']=='fb32f6c6'
            record['stress']=[]
            for _ in range(20):
                await hook('arm');st=await complete();assert st['hash']=='fb32f6c6',st
                record['stress'].append(dict(state=st,scheduling=await hook('scheduling')))
            record['passed']=True
        finally:
            try:await hook('quit')
            finally:
                for _ in range(100):
                    if 'sdlfractal(' not in await call('amiga_list_clients'):break
                    await asyncio.sleep(.1)
                await asyncio.sleep(.3)
                try:
                    data=await asyncio.to_thread(fetch,'RAM:SixiesDev/sdl-performance.log');(OUT.parent/'acceptance.log').write_bytes(data)
                    record['clean_exit']='EXIT epilogue=52455431' in data.decode('latin1') and 'LAUNCHER exit=0; owned memory released' in data.decode('latin1')
                    assert record['clean_exit'],data.decode('latin1')
                finally:
                    record['read_retries']=READ_RETRIES
                    record['postflight']=await call('amiga_ping');OUT.write_text(json.dumps(record,indent=2)+'\n')
    assert record['passed'];print('SDL hardware acceptance passed; application stopped.',flush=True)
if __name__=='__main__':asyncio.run(main())
