"""Exercise the real C core/relay through the Python controller and MCP HTTP.
No test here executes on ARM, modifies firmware, or connects to the Amiga.
"""
import asyncio
from contextlib import asynccontextmanager
import ctypes
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import unittest

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT))
from amiga.arm_debug.demo import NativeDemoTransport, build_demo
from amiga.arm_debug.host import ArmDebugger, BridgeHooks, DebugError
CLIENT="arm-debug-demo"

class NativeTests(unittest.IsolatedAsyncioTestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory()
        cls.library=build_demo(Path(cls.tmp.name))
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    async def asyncSetUp(self):
        self.transport=NativeDemoTransport(self.library)
        self.debug=ArmDebugger(self.transport,ROOT)
        await self.debug.attach(CLIENT)

    async def test_pause_step_read_resume_detach(self):
        paused=await self.debug.pause(CLIENT)
        self.assertEqual(paused["state"],"paused")
        self.assertEqual(paused["execution"],"host_demo")
        self.assertFalse(paused["instruction_stepping"])
        memory=await self.debug.read_memory(CLIENT,0,64,64)
        self.assertEqual(bytes.fromhex(memory["hex"]),bytes(range(64,128)))
        stepped=await self.debug.step(CLIENT)
        self.assertEqual(stepped["checkpoint_hits"],paused["checkpoint_hits"]+1)
        self.assertNotEqual(stepped["point"],paused["point"])
        self.assertEqual(stepped["reason"],"checkpoint_step")
        await self.debug.breakpoint(CLIENT,1)
        detached=await self.debug.detach(CLIENT)
        self.assertTrue(detached["detached"])
        await self.debug.attach(CLIENT)
        status=await self.debug.status(CLIENT)
        self.assertEqual(status["state"],"running")
        self.assertEqual(status["breakpoints"],[])

    async def test_breakpoint_hits_and_eight_slot_limit(self):
        await self.debug.pause(CLIENT)
        for i in range(1,9):await self.debug.breakpoint(CLIENT,i)
        # Duplicate does not consume a slot.
        await self.debug.breakpoint(CLIENT,1)
        with self.assertRaisesRegex(DebugError,"full"):await self.debug.breakpoint(CLIENT,9)
        await self.debug.resume(CLIENT)
        status=await self.debug.status(CLIENT)
        self.assertEqual(status["state"],"paused")
        self.assertEqual(status["reason"],"breakpoint")
        await self.debug.breakpoint(CLIENT,0,False)
        self.assertEqual((await self.debug.status(CLIENT))["breakpoints"],[])

    async def test_memory_bounds_and_state(self):
        with self.assertRaisesRegex(DebugError,"state"):await self.debug.read_memory(CLIENT,0,0,4)
        with self.assertRaisesRegex(DebugError,"state"):await self.debug.step(CLIENT)
        await self.debug.pause(CLIENT)
        for region,offset,size in [(1,0,4),(0,65,64),(0,0xffffffff,4),(0,128,1)]:
            with self.assertRaisesRegex(DebugError,"range"):await self.debug.read_memory(CLIENT,region,offset,size)
        for region,offset,size in [(0,0,0),(0,0,65),(0,-1,4),(-1,0,4)]:
            with self.assertRaises(DebugError):await self.debug.read_memory(CLIENT,region,offset,size)
        self.assertEqual((await self.debug.read_memory(CLIENT,0,127,1))["hex"],"7f")

    async def test_session_restart_rejects_existing_attachment(self):
        self.transport.reset(2)
        with self.assertRaisesRegex(DebugError,"restarted"):await self.debug.pause(CLIENT)
        self.assertNotIn(CLIENT,self.debug.attachments)
        self.assertEqual((await self.debug.attach(CLIENT))["session"],2)

    async def test_pause_timeout_is_pending_and_can_be_cancelled(self):
        self.transport.auto_tick=False
        with self.assertRaisesRegex(DebugError,"pending"):await self.debug.pause(CLIENT,timeout=0.1)
        status=await self.debug.status(CLIENT)
        self.assertEqual(status["state"],"running")
        self.assertTrue(status["pause_pending"])
        self.assertFalse((await self.debug.resume(CLIENT))["pause_pending"])

    async def test_fault_preserved_and_not_resumable(self):
        self.transport.lib.demo_fault()
        status=await self.debug.status(CLIENT)
        self.assertEqual(status["state"],"faulted")
        self.assertEqual(status["fault"]["pc"],0x30001004)
        self.assertEqual(status["fault"]["code"],0xdab)
        with self.assertRaisesRegex(DebugError,"state"):await self.debug.resume(CLIENT)
        self.assertEqual((await self.debug.read_memory(CLIENT,0,0,4))["hex"],"00010203")
        self.assertEqual((await self.debug.detach(CLIENT))["state"],"faulted")

    async def test_finished_target_stays_finished(self):
        self.transport.lib.demo_finish()
        with self.assertRaisesRegex(DebugError,"state"):await self.debug.pause(CLIENT)
        self.assertEqual((await self.debug.detach(CLIENT))["state"],"finished")

    async def test_log_wrap_and_cursor(self):
        for i in range(20):self.transport.lib.demo_log(f"message {i}".encode())
        logs=await self.debug.logs(CLIENT)
        self.assertEqual(logs["lost"],13)
        self.assertEqual(len(logs["events"]),8)
        self.assertEqual(logs["events"][-1]["text"],"message 19")
        self.assertEqual((await self.debug.logs(CLIENT,logs["cursor"]))["events"],[])
        with self.assertRaisesRegex(DebugError,"cursor"):await self.debug.logs(CLIENT,999)
        self.transport.lib.demo_log(b"x"*200)
        self.assertEqual(len((await self.debug.logs(CLIENT,21))["events"][0]["text"]),88)

    async def test_native_relay_rejects_stale_snapshot_and_bad_ranges(self):
        token=await self.transport.call(CLIENT,"S")
        await self.transport.call(CLIENT,"S")
        with self.assertRaisesRegex(DebugError,"STALE_SNAPSHOT"):
            await self.transport.call(CLIENT,f"R {token} 0 4")
        token=await self.transport.call(CLIENT,"S")
        for args in [f"R {token} ffffffff 4",f"R {token} 0 61",f"R {token} 440 1",f"R {token} -1 4",f"R {token} 0 4 garbage"]:
            with self.assertRaises(DebugError):await self.transport.call(CLIENT,args)

    async def test_native_relay_rejects_foreign_session_and_replayed_commands(self):
        with self.assertRaisesRegex(DebugError,"BAD_SESSION"):
            await self.transport.call(CLIENT,"W 2 1 1 0 0 0")
        with self.assertRaisesRegex(DebugError,"BAD_SEQUENCE"):
            await self.transport.call(CLIENT,"W 1 2 1 0 0 0")
        await self.transport.call(CLIENT,"W 1 1 1 0 0 0")
        with self.assertRaisesRegex(DebugError,"BAD_SEQUENCE"):
            await self.transport.call(CLIENT,"W 1 1 1 0 0 0")
        for args in ["W 1 2 ff 0 0 0","W 1 2 1 0 0","W 1 2 1 0 0 100000000"]:
            with self.assertRaises(DebugError):await self.transport.call(CLIENT,args)

    async def test_pending_mailbox_cannot_be_overwritten(self):
        # Bypass fixture servicing to leave a committed request unconsumed.
        out=ctypes.create_string_buffer(256)
        self.assertEqual(self.transport.lib.demo_hook(b"W 1 1 1 0 0 0",out,256),0)
        self.assertNotEqual(self.transport.lib.demo_hook(b"W 1 2 2 0 0 0",out,256),0)
        self.assertEqual(out.value,b"COMMAND_PENDING")

    async def test_protocol_corruption_and_torn_snapshot(self):
        self.transport.auto_tick=False
        for offset,value in [(0,0),(4,999),(20,99),(72,999),(76,999),(160,999),(16,3)]:
            self.transport.reset(1)
            self.transport.lib.demo_corrupt(offset,value)
            with self.assertRaises(DebugError):await self.debug.status(CLIENT)

    async def test_command_core_rejects_foreign_session_even_without_relay(self):
        self.transport.auto_tick=False
        for offset,value in [(256,999),(264,1),(260,1)]:self.transport.lib.demo_corrupt(offset,value)
        self.transport.lib.demo_service()
        status=await self.debug.status(CLIENT)
        self.assertEqual(status["command_ack"],0)
        self.assertFalse(status["pause_pending"])

    async def test_client_name_validation(self):
        for name in ["", "x|evil", "x\nINJECT", "a"*33]:
            with self.assertRaises(DebugError):await self.debug.attach(name)

    async def test_checkpoint_manifest_build_match_and_source_location(self):
        with tempfile.TemporaryDirectory(dir=ROOT/".context") as tmp:
            path=Path(tmp)/"points.json"
            points={"build_id":0x53495831,"points":[{"id":1,"name":"before","file":"amiga/arm_debug/examples/arm_worker.c","line":17}]}
            path.write_text(json.dumps(points))
            result=await self.debug.attach(CLIENT,str(path.relative_to(ROOT)))
            self.assertEqual(self.debug.attachments[CLIENT].points[1]["name"],"before")
            points["build_id"]=7;path.write_text(json.dumps(points))
            with self.assertRaisesRegex(DebugError,"different build"):
                await self.debug.attach(CLIENT,str(path.relative_to(ROOT)))

    async def test_malformed_snapshot_is_never_partially_used(self):
        real=self.transport.call
        async def bad(client,args):
            data=await real(client,args)
            return data[:-2] if args.startswith("R ") else data
        self.transport.call=bad
        with self.assertRaisesRegex(DebugError,"truncated"):await self.debug.status(CLIENT)

class TransportTests(unittest.IsolatedAsyncioTestCase):
    async def test_callhook_response_correlation_and_errors(self):
        queue=asyncio.Queue()
        class Bus:
            @asynccontextmanager
            async def subscribe(self,*events):yield queue
        class Connection:
            fail=False
            def send(self,command):
                self.command=command
                queue.put_nowait(("cmd",{"id":command["id"]-1,"status":"ok","data":"wrong reply"}))
                queue.put_nowait(("cmd",{"id":command["id"],"status":"err" if self.fail else "ok","data":"right reply"}))
        conn=Connection();transport=BridgeHooks(lambda:(conn,None,Bus()))
        self.assertEqual(await transport.call(CLIENT,"S"),"right reply")
        self.assertEqual(conn.command["hook"],"arm_debug")
        first=conn.command["id"];conn.fail=True
        with self.assertRaisesRegex(DebugError,"right reply"):await transport.call(CLIENT,"S")
        self.assertEqual(conn.command["id"],first+1)

class HttpTests(unittest.TestCase):
    def test_mcp_http_controls_the_native_c_target(self):
        from mcp import ClientSession
        from mcp.client.streamable_http import streamablehttp_client
        with socket.socket() as sock:
            sock.bind(("127.0.0.1",0));port=sock.getsockname()[1]
        async def exercise():
            async with streamablehttp_client(f"http://127.0.0.1:{port}/mcp") as (r,w,_):
                async with ClientSession(r,w) as session:
                    await session.initialize()
                    self.assertEqual(len((await session.list_tools()).tools),9)
                    async def call(name,**args):
                        result=await session.call_tool(name,{"client":CLIENT,**args})
                        self.assertFalse(result.isError,result)
                        return result.structuredContent or json.loads(result.content[0].text)
                    result=await call("amiga_arm_attach")
                    self.assertEqual(result["execution"],"host_demo")
                    stopped=await call("amiga_arm_pause")
                    stepped=await call("amiga_arm_step_checkpoint")
                    self.assertEqual(stepped["checkpoint_hits"],stopped["checkpoint_hits"]+1)
                    memory=await call("amiga_arm_read_memory",region=0,offset=0,size=4)
                    self.assertEqual(memory["hex"],"00010203")
                    self.assertTrue((await call("amiga_arm_logs"))["events"])
                    self.assertTrue((await call("amiga_arm_detach"))["detached"])
        with tempfile.TemporaryFile(mode="w+") as log:
            proc=subprocess.Popen([sys.executable,str(ROOT/"scripts/arm_debug_demo.py"),"--port",str(port)],cwd=ROOT,stdout=log,stderr=log)
            try:
                deadline=time.monotonic()+15
                while True:
                    if proc.poll() is not None:raise RuntimeError("Demo server exited")
                    try:
                        with socket.create_connection(("127.0.0.1",port),timeout=.2):break
                    except OSError:
                        if time.monotonic()>deadline:raise TimeoutError("Demo server not ready")
                        time.sleep(.05)
                asyncio.run(exercise())
            except BaseException:
                log.seek(0);print(log.read());raise
            finally:
                proc.terminate()
                try:proc.wait(5)
                except subprocess.TimeoutExpired:proc.kill();proc.wait()

if __name__=="__main__":unittest.main()
