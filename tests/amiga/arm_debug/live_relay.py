"""Explicit live test against an already-running 68k probe or ZZ9000 launcher.

Not part of unittest discovery. Does not deploy or launch anything.
Run using the workspace venv and a fresh probe's --session hex nonce.
"""
import argparse
import asyncio
from datetime import datetime, timezone
import json
from pathlib import Path
import time

from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client

ROOT = Path(__file__).resolve().parents[3]
CLIENT = "amiga-relay-probe"


async def verify(args):
    physical_arm = args.target == "zz9000"
    client = "zzarm-debug" if physical_arm else CLIENT
    record = {"utc": datetime.now(timezone.utc).isoformat(),
              "endpoint": args.endpoint, "execution": "physical_zz9000_core1" if physical_arm else "physical_amiga_68k_probe",
              "arm_execution_verified": False, "calls": [], "passed": False}
    async with streamablehttp_client(args.endpoint) as (reader, writer, _):
        async with ClientSession(reader, writer) as session:
            await session.initialize()

            async def call(name, **kw):
                start = time.monotonic()
                result = await session.call_tool(name, {"client": client, **kw})
                text = "\n".join(c.text for c in result.content if c.type == "text")
                entry = {"tool": name, "args": kw, "seconds": round(time.monotonic()-start, 3),
                         "error": bool(result.isError), "result": text}
                record["calls"].append(entry)
                if result.isError:
                    raise RuntimeError(text)
                value = json.loads(text)
                entry["result"] = value
                print(name, value.get("state", "OK"), flush=True)
                return value

            attached = False
            try:
                state = await call("amiga_arm_attach")
                # Verify identity before sending any mutating debug command.
                assert state["execution"] == ("arm_instrumented" if physical_arm else "host_demo"), state
                assert state["session"] == int(args.session, 16), state
                path = ".context/amiga/arm-debug/zz9000/build.json" if physical_arm else ".context/amiga/arm-debug/build.json"
                build = json.loads((ROOT/path).read_text())
                assert state["build_id"] == build["build_id" if physical_arm else "relay_probe_build_id"], state
                assert not state["instruction_stepping"]
                if physical_arm:
                    assert state["values"][2] & 255 == 1, state
                    assert not state["values"][3] & 0x1005, state
                attached = True
                paused = await call("amiga_arm_pause")
                assert paused["state"] == "paused"
                stable = await call("amiga_arm_status")
                assert stable["checkpoint_hits"] == paused["checkpoint_hits"]
                memory = await call("amiga_arm_read_memory", region=0, offset=0, size=64)
                assert memory["hex"] == bytes(range(64)).hex(), memory
                stepped = await call("amiga_arm_step_checkpoint")
                assert stepped["state"] == "paused"
                assert stepped["checkpoint_hits"] == paused["checkpoint_hits"] + 1
                assert stepped["point"] != paused["point"]
                assert stepped["reason"] == "checkpoint_step"
                point = 1 if stepped["point"] == 2 else 2
                await call("amiga_arm_breakpoint", point=point)
                await call("amiga_arm_continue")
                state = await call("amiga_arm_status")
                assert state["state"] == "paused" and state["reason"] == "breakpoint", state
                assert state["point"] == point
                logs = await call("amiga_arm_logs")
                assert ("Physical Cortex-A9 Core1" if physical_arm else "NOT ARM") in json.dumps(logs), logs
                await call("amiga_arm_breakpoint", point=0, enabled=False)
                detached = await call("amiga_arm_detach")
                attached = False
                assert detached["detached"] and detached["state"] == "running"
                record["passed"] = True
                record["arm_execution_verified"] = physical_arm
            finally:
                if attached:
                    try:
                        await call("amiga_arm_detach")
                    except Exception as exc:
                        record["cleanup_error"] = str(exc)
                Path(args.output).write_text(json.dumps(record, indent=2)+"\n")
    print("PASS: " + record["execution"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session", required=True, help="fresh probe nonce, hexadecimal")
    parser.add_argument("--target", choices=["relay", "zz9000"], default="relay")
    parser.add_argument("--endpoint", default="http://127.0.0.1:55010/mcp")
    parser.add_argument("--output", default=str(ROOT/".context/amiga/arm-debug/live-relay-validation.json"))
    asyncio.run(verify(parser.parse_args()))
