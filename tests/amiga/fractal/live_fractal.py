"""Explicit hardware acceptance. Requires an idle Core1 and staged RAM:SixiesDev/zzfractal.
Runs render/cancel/debug/quit cycles; leaves Workbench and bridge running.
Host tests never imply hardware execution. No firmware or startup modifications.
"""
import argparse
import asyncio
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import secrets
import struct
import sys
import time
import urllib.parse
import urllib.request
from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client
from test_fractal import View,frame_reference,reference,fnv

ROOT=Path(__file__).resolve().parents[3]
CLIENT='zzfractal'

async def main(a):
    output=Path(a.output);output.parent.mkdir(parents=True,exist_ok=True)
    record={'utc':datetime.now(timezone.utc).isoformat(),'passed':False,'calls':[], 'renders':[], 'cycles':[]}
    build=json.loads((ROOT/'.context/amiga/fractal/build.json').read_text())
    async with streamablehttp_client(a.endpoint) as (r,w,_):
      async with ClientSession(r,w) as s:
        await s.initialize()
        async def call(name,**kw):
            t=time.monotonic();res=await s.call_tool(name,kw)
            text='\n'.join(c.text for c in res.content if c.type=='text')
            record['calls'].append({'tool':name,'args':kw,'result':text,'seconds':round(time.monotonic()-t,3)})
            if res.isError:raise RuntimeError(text)
            return text
        async def hook(args='status'):
            txt=await call('amiga_call_hook',client=CLIENT,hook='fractal',args=args)
            d=dict(re.findall(r'(\w+)=([^\s]+)',txt))
            assert d.get('ok')=='1',txt
            assert d.get('error')=='0',txt
            return d
        async def debug(name,**kw):
            return json.loads(await call('amiga_arm_'+name,client=CLIENT,**kw))
        async def ready():
            for _ in range(100):
                if 'zzfractal(' in await call('amiga_list_clients'):return
                await asyncio.sleep(.1)
            raise AssertionError('Fractal launcher did not register')
        async def launch():
            nonce=secrets.token_hex(4)
            script=f'Stack 65536\nRAM:SixiesDev/zzfractal {nonce} RUN >RAM:SixiesDev/fractal.log\n'
            await call('amiga_write_file',path='RAM:SixiesDev/start-fractal',offset=0,hex_data=script.encode().hex())
            await call('amiga_run_script',script='Run >NIL: Execute RAM:SixiesDev/start-fractal\n',timeout=30)
            await ready()
            st=await debug('attach')
            assert st['session']==int(nonce,16) and st['build_id']==build['build_id'],st
            assert st['execution']=='arm_instrumented' and not st['instruction_stepping'],st
            return st
        async def finish():
            await hook('quit')
            for _ in range(100):
                if 'zzfractal(' not in await call('amiga_list_clients'):break
                await asyncio.sleep(.1)
            else:raise AssertionError('Client failed to exit')
            # Unregistration precedes firmware idle/reset and memory release.
            await asyncio.sleep(.4)
            # Fetch in bounded chunks: verbose client logs exceed the DOS
            # script tool's output limit and hide its completion marker.
            def fetch_log():
                base=a.endpoint.replace('/mcp','/api/')
                with urllib.request.urlopen(base+'dir?'+urllib.parse.urlencode({'path':'RAM:SixiesDev'}),timeout=30) as f:
                    size=next(e['size'] for e in json.load(f)['entries'] if e['name'].lower()=='fractal.log')
                data=bytearray()
                for off in range(0,size,2048):
                    n=min(2048,size-off)
                    q=urllib.parse.urlencode({'path':'RAM:SixiesDev/fractal.log','offset':off,'size':n})
                    with urllib.request.urlopen(base+'file?'+q,timeout=30) as f:
                        chunk=bytes.fromhex(json.load(f)['hexData'])
                    assert len(chunk)==n
                    data.extend(chunk)
                return data.decode('latin1')
            log=await asyncio.to_thread(fetch_log)
            assert 'LAUNCHER exit=0; owned memory released' in log,log
            assert 'EXIT epilogue=52455431' in log,log
            assert 'MAPPING PASS' in log and 'MPIDR=80000001' in log and 'SCTLR=08c50878' in log,log
            record['cycles'].append(log)
        async def completed():
            for _ in range(240):
                st=await hook()
                if st['running']=='0':
                    assert st['tiles']=='150' and st['pending']=='0',st
                    return st
                await asyncio.sleep(.5)
            raise AssertionError('Render timed out')
        def fetch_frame():
            result=bytearray()
            for off in range(0,153600,2048):
                n=min(2048,153600-off)
                q=urllib.parse.urlencode({'path':'RAM:SixiesDev/fractal-counts.bin','offset':off,'size':n})
                with urllib.request.urlopen(a.endpoint.replace('/mcp','/api/file?')+q,timeout=30) as response:
                    chunk=bytes.fromhex(json.load(response)['hexData'])
                assert len(chunk)==n
                result.extend(chunk)
            return bytes(result)
        async def verify_frame(v,label):
            st=await completed();want=frame_reference(v)
            assert st['hash']==f'{fnv(want):08x}',st
            await hook('save');actual=await asyncio.to_thread(fetch_frame)
            assert actual==want,label+' pixel mismatch'
            record['renders'].append({'label':label,**st,'all_76800_pixels_match':True})
            print(label,st['ms']+' ms',st['hash'],'all pixels match',flush=True)
        running=False
        try:
            record['preflight']=await call('amiga_ping')
            assert 'zzfractal(' not in await call('amiga_list_clients'),'Stop existing demo before this explicit acceptance run'
            await launch();running=True
            await verify_frame(View(-12288,0,153,128),'ARM default')
            await hook('cpu');await verify_frame(View(-12288,0,153,128),'68060 default')
            await hook('zoom 160 120');await verify_frame(View(-12288,0,76,128),'68060 zoom')
            await hook('arm');await verify_frame(View(-12288,0,76,128),'ARM zoom')
            for i in range(20):
                await hook('reset');await asyncio.sleep(.04)
                await hook('zoom 160 120');await asyncio.sleep(.04)
                await hook('cancel')
                for _ in range(30):
                    st=await hook()
                    if st['pending']=='0':break
                    await asyncio.sleep(.02)
                assert st['pending']=='0' and st['running']=='0' and int(st['cancel_ms'])<=250,st
                record.setdefault('cancel_cycles',[]).append(st)
            print('20 render/zoom/cancel cycles passed',flush=True)
            await debug('breakpoint',point=3)
            await hook('arm')
            for _ in range(100):
                st=await debug('status')
                if st['state']=='paused':break
                await asyncio.sleep(.02)
            assert st['state']=='paused' and st['point']==3,st
            before=st['checkpoint_hits']
            memory=await debug('read_memory',region=0,offset=0,size=64)
            expected=b''.join(struct.pack('<H',reference(View(-12288,0,76,128),x,0)) for x in range(32))
            assert memory['hex']==expected.hex(),memory
            st=await debug('step_checkpoint')
            # The host may not have submitted the next tile yet: idle (4) is
            # a real checkpoint, as is the next tile dispatch (1).
            assert st['state']=='paused' and st['checkpoint_hits']==before+1 and st['point'] in (1,4),st
            await hook('cancel')
            for _ in range(30):
                st=await hook()
                if st['pending']=='0':break
                await asyncio.sleep(.02)
            assert st['pending']=='0' and int(st['cancel_ms'])<=250,st
            record['paused_cancel']=st
            logs=await debug('logs');assert 'Mandelbrot' in str(logs)
            await debug('detach')
            await debug('attach')
            await debug('pause')
            await finish();running=False
            for i in range(9):
                await launch();running=True
                await debug('pause')
                await finish();running=False
                print(f'Paused launch/quit {i+2}/10 passed',flush=True)
            record['postflight']=await call('amiga_ping')
            record['screens']=await call('amiga_list_screens')
            record['clients']=await call('amiga_list_clients')
            assert 'Clients (0)' in record['clients'],record['clients']
            record['passed']=True;record['arm_execution_verified']=True;record['build']=build
        finally:
            if running:
                try:await finish()
                except Exception as e:record['cleanup_error']=str(e)
            output.write_text(json.dumps(record,indent=2)+'\n')
    print('PASS: physical fractal acceptance',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--endpoint',default='http://127.0.0.1:55010/mcp')
    p.add_argument('--output',default=str(ROOT/'.context/amiga/fractal/live-acceptance.json'))
    asyncio.run(main(p.parse_args()))
