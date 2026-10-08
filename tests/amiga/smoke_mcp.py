#!/usr/bin/env python3
"""Exercise real MCP HTTP transport against the protocol simulator, not AmigaOS."""
import asyncio
import contextlib
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from urllib.request import urlopen

from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client

ROOT = Path(__file__).resolve().parents[2]


def available_ports():
    for _ in range(100):
        with contextlib.ExitStack() as stack:
            first = stack.enter_context(socket.socket())
            first.bind(("127.0.0.1", 0))
            base = first.getsockname()[1]
            if base > 65532:
                continue
            try:
                for port in (base + 1, base + 2):
                    sock = stack.enter_context(socket.socket())
                    sock.bind(("127.0.0.1", port))
            except OSError:
                continue
            return base
    raise RuntimeError("Could not reserve three test ports")


async def check_mcp(port):
    async with streamablehttp_client(f"http://127.0.0.1:{port}/mcp") as (read, write, _):
        async with ClientSession(read, write) as session:
            await session.initialize()
            tools = (await session.list_tools()).tools
            names = {tool.name for tool in tools}
            required = {"amiga_ping", "amiga_list_tasks", "amiga_arexx_ports",
                        "amiga_arexx_send", "amiga_debug_attach", "amiga_step", "amiga_run_script",
                        "amiga_arm_attach", "amiga_arm_status", "amiga_arm_pause",
                        "amiga_arm_continue", "amiga_arm_step_checkpoint", "amiga_arm_breakpoint",
                        "amiga_arm_read_memory", "amiga_arm_logs", "amiga_arm_detach"}
            assert required <= names, required - names
            for name, expected in (("amiga_ping", "Amiga alive"), ("amiga_list_tasks", "input.device")):
                result = await session.call_tool(name, {})
                output = "\n".join(getattr(block, "text", "") for block in result.content)
                assert not result.isError and expected in output, (name, output)
            print(f"PASS: MCP initialize, {len(tools)} tools discovered, ping and task inspection")
            print("ARexx/debug tool schemas present; execution requires a booted AmigaOS target.")


def main():
    port = available_ports()
    with tempfile.TemporaryFile(mode="w+") as log:
        proc = subprocess.Popen([sys.executable, str(ROOT / "scripts/amiga.py"), "serve",
                                 "--profile", "simulator", "--port", str(port)],
                                cwd=ROOT, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 30
            while True:
                if proc.poll() is not None:
                    raise RuntimeError("Test server exited during startup")
                try:
                    with urlopen(f"http://127.0.0.1:{port}/health", timeout=1) as response:
                        assert response.status == 200
                    break
                except OSError:
                    if time.monotonic() > deadline:
                        raise TimeoutError("Test server did not become ready")
                    time.sleep(0.2)
            asyncio.run(check_mcp(port))
        except BaseException:
            log.seek(0)
            print(log.read(), file=sys.stderr)
            raise
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()


if __name__ == "__main__":
    main()
