"""Host controller for cooperative checkpoints, not instruction-level debugging."""
from __future__ import annotations
import asyncio
import json
from pathlib import Path
import re
import secrets
import struct
from dataclasses import dataclass, field
from typing import Protocol

MAGIC = 0x53414431
PAGE_SIZE = 1088
STATUS_SIZE = 256
STATES = {1: "running", 2: "paused", 3: "faulted", 4: "finished"}
REASONS = {0: "initial", 1: "pause", 2: "breakpoint", 3: "checkpoint_step", 4: "fault"}
RESULTS = {1: "bad command", 2: "invalid target state", 3: "invalid memory range/point", 4: "breakpoint table full", 5: "session changed", 6: "sequence mismatch"}

class DebugError(RuntimeError):
    pass

class HookTransport(Protocol):
    async def call(self, client: str, args: str) -> str: ...

class BridgeHooks:
    """Use existing CALLHOOK IPC; never write arbitrary Amiga/ARM addresses."""
    def __init__(self, require_connected):
        self.require_connected = require_connected
        self._id = secrets.randbelow(0x40000000) + 0x40000000
        self._lock = asyncio.Lock()

    async def call(self, client: str, args: str) -> str:
        async with self._lock:
            conn, _, bus = self.require_connected()
            self._id = (self._id + 1) & 0x7fffffff
            request_id = self._id
            async with bus.subscribe("cmd") as queue:
                conn.send({"type": "CALLHOOK", "id": request_id, "client": client,
                           "hook": "arm_debug", "args": args})
                deadline = asyncio.get_running_loop().time() + 7
                while True:
                    remaining = deadline - asyncio.get_running_loop().time()
                    if remaining <= 0:
                        raise DebugError("ARM relay timed out; command delivery is uncertain. Inspect status before retrying.")
                    try:
                        _, reply = await asyncio.wait_for(queue.get(), remaining)
                    except asyncio.TimeoutError as exc:
                        raise DebugError("ARM relay timed out; command delivery is uncertain. Inspect status before retrying.") from exc
                    if reply.get("id") != request_id:
                        continue
                    data = reply.get("data", "")
                    if reply.get("status", "").lower() != "ok":
                        raise DebugError(f"ARM relay: {data}")
                    return data

@dataclass
class Attachment:
    session: int
    build: int
    points: dict[int, dict] = field(default_factory=dict)

class ArmDebugger:
    def __init__(self, transport: HookTransport, workspace: Path):
        self.transport = transport
        self.workspace = workspace.resolve()
        self.attachments: dict[str, Attachment] = {}
        # One logical transaction at a time, including snapshots + acknowledgements.
        self.lock = asyncio.Lock()

    @staticmethod
    def client_name(client):
        if not isinstance(client, str) or not re.fullmatch(r"[A-Za-z0-9_.-]{1,32}", client):
            raise DebugError("Use the exact registered client name (1-32 letters, digits, _, . or -)")

    @staticmethod
    def uint(value, label, maximum=0xffffffff):
        if type(value) is not int or not 0 <= value <= maximum:
            raise DebugError(f"{label} must be an integer in 0..{maximum}")
        return value

    async def snapshot(self, client: str, logs=False):
        self.client_name(client)
        # Retry only read-only snapshots; never automatically retry a command write.
        for attempt in range(3):
            try:
                token_text = await self.transport.call(client, "S")
                if not re.fullmatch(r"[0-9a-fA-F]{8}", token_text) or int(token_text, 16) == 0:
                    raise DebugError("Invalid ARM relay snapshot token")
                size = PAGE_SIZE if logs else STATUS_SIZE
                raw = bytearray()
                for offset in range(0, size, 96):
                    count = min(96, size-offset)
                    text = await self.transport.call(client, f"R {token_text} {offset:x} {count:x}")
                    if not re.fullmatch(r"[0-9a-fA-F]{" + str(count*2) + "}", text):
                        raise DebugError("Malformed or truncated ARM relay snapshot")
                    raw.extend(bytes.fromhex(text))
                break
            except DebugError as exc:
                if attempt == 2 or not any(x in str(exc) for x in ("SNAPSHOT_BUSY", "STALE_SNAPSHOT")):
                    raise
                await asyncio.sleep(0.01)
        words = struct.unpack_from(">64I", raw)
        if words[0] != MAGIC or words[1] != 1 or not words[2] or words[4] & 1:
            raise DebugError("Target has no stable Sixies ARM debug ABI 1 channel")
        if words[5] not in STATES or words[6] not in REASONS or not words[17] & 1:
            raise DebugError("Invalid ARM state/capability record")
        if words[18] > 8 or words[19] > 8 or words[40] > 8 or words[11] > 64:
            raise DebugError("Invalid ARM snapshot bounds")
        return {
            "session": words[2], "build_id": words[3], "snapshot_sequence": words[4],
            "state": STATES[words[5]], "reason": REASONS[words[6]], "point": words[7],
            "checkpoint_hits": words[8], "command_ack": words[9], "command_result": words[10],
            "pause_pending": bool(words[21]), "values": list(words[24:24+words[19]]),
            "breakpoints": list(words[32:32+words[18]]), "region_count": words[40],
            "log_count": words[20], "execution": "host_demo" if words[17] & 2 else "arm_instrumented",
            "debug_mode": "cooperative_checkpoints", "instruction_stepping": False,
            "fault": {"code": words[12], "pc": words[13], "sp": words[14], "lr": words[15], "cpsr": words[16]} if words[5] == 3 else None,
            "_memory": bytes(raw[192:192+words[11]]), "_raw": bytes(raw),
        }

    def validate_session(self, client, status):
        attached = self.attachments.get(client)
        if not attached:
            raise DebugError("Attach to this instrumented client first")
        if (status["session"], status["build_id"]) != (attached.session, attached.build):
            del self.attachments[client]
            raise DebugError("Target restarted or build changed; attachment discarded. Attach again.")
        return attached

    def public(self, client, status):
        result = {k:v for k,v in status.items() if not k.startswith("_")}
        attached = self.attachments.get(client)
        if attached and status["point"] in attached.points:
            result["source_checkpoint"] = attached.points[status["point"]]
        return result

    async def attach(self, client, points_path=""):
        async with self.lock:
            status = await self.snapshot(client)
            points = {}
            if points_path:
                path = (self.workspace / points_path).resolve()
                if not path.is_relative_to(self.workspace) or path.stat().st_size > 65536:
                    raise DebugError("Checkpoint manifest must be inside the workspace and <=64 KiB")
                manifest = json.loads(path.read_text())
                if manifest["build_id"] != status["build_id"]:
                    raise DebugError("Checkpoint manifest belongs to a different build")
                for point in manifest["points"]:
                    ident = self.uint(point["id"], "checkpoint id")
                    if not ident or ident in points:
                        raise DebugError("Checkpoint IDs must be nonzero and unique")
                    if not isinstance(point.get("name"), str) or len(point["name"]) > 128:
                        raise DebugError("Checkpoint names must be strings <=128 characters")
                    file = point.get("file", "")
                    if not file or not (self.workspace/file).resolve().is_relative_to(self.workspace):
                        raise DebugError("Checkpoint source must be inside the workspace")
                    self.uint(point["line"], "source line")
                    if not point["line"]:
                        raise DebugError("Source lines start at one")
                    points[ident] = {k:point[k] for k in ("id", "name", "file", "line")}
            self.attachments[client] = Attachment(status["session"], status["build_id"], points)
            return self.public(client, status)

    async def status(self, client):
        async with self.lock:
            status = await self.snapshot(client)
            self.validate_session(client, status)
            return self.public(client, status)

    async def command(self, client, opcode, args=(), *, wait_state=None, timeout=5.0):
        if not isinstance(timeout, (int, float)) or not 0.1 <= timeout <= 15:
            raise DebugError("Timeout must be 0.1..15 seconds")
        for x in args:
            self.uint(x, "command argument")
        status = await self.snapshot(client)
        self.validate_session(client, status)
        seq = status["command_ack"] + 1
        if seq > 0xffffffff:
            raise DebugError("Command counter exhausted; restart the instrumented app")
        payload = (status["session"], seq, opcode, *args, *(0 for _ in range(3-len(args))))
        reply = await self.transport.call(client, "W " + " ".join(f"{v:x}" for v in payload))
        if reply != "OK":
            raise DebugError("Unexpected ARM command receipt; delivery is uncertain")
        deadline = asyncio.get_running_loop().time() + timeout
        while True:
            status = await self.snapshot(client)
            self.validate_session(client, status)
            if status["command_ack"] == seq:
                if status["command_result"]:
                    raise DebugError(RESULTS.get(status["command_result"], "unknown ARM command error"))
                if wait_state is None or status["state"] == wait_state:
                    return status
                if status["state"] in ("faulted", "finished"):
                    raise DebugError(f"Target became {status['state']} while waiting for {wait_state}")
            elif status["command_ack"] > seq:
                raise DebugError("Another debugger advanced the command channel; inspect status")
            if asyncio.get_running_loop().time() >= deadline:
                raise DebugError("ARM command not completed in time; it may still be pending. Inspect status; no forced CPU halt was attempted.")
            await asyncio.sleep(0.02)

    async def pause(self, client, timeout=5.0):
        async with self.lock:
            return self.public(client, await self.command(client, 1, wait_state="paused", timeout=timeout))

    async def resume(self, client):
        async with self.lock:
            return self.public(client, await self.command(client, 2))

    async def step(self, client, timeout=5.0):
        async with self.lock:
            return self.public(client, await self.command(client, 3, wait_state="paused", timeout=timeout))

    async def breakpoint(self, client, point, enabled=True):
        self.uint(point, "checkpoint id")
        if not point and enabled:
            raise DebugError("Checkpoint zero is reserved; zero only clears all breakpoints")
        async with self.lock:
            return self.public(client, await self.command(client, 4 if enabled else 5, (point,)))

    async def read_memory(self, client, region, offset, size):
        self.uint(region, "region", 7);self.uint(offset, "offset");self.uint(size, "size", 64)
        if not size: raise DebugError("Read size must be 1..64 bytes")
        async with self.lock:
            status = await self.command(client, 6, (region, offset, size))
            if len(status["_memory"]) != size:
                raise DebugError("Incomplete ARM memory response")
            return {"region":region,"offset":offset,"size":size,"hex":status["_memory"].hex(),
                    "byte_order":"raw application bytes; ARM scalar data is normally little-endian"}

    async def logs(self, client, after=0):
        self.uint(after, "log cursor")
        async with self.lock:
            status = await self.snapshot(client, logs=True)
            self.validate_session(client, status)
            count = status["log_count"]
            if after > count: raise DebugError("Log cursor is ahead of this session")
            oldest = max(1, count-7)
            events=[]
            for seq in range(max(after+1, oldest), count+1):
                pos=320+((seq-1)%8)*96
                actual,n=struct.unpack_from(">II",status["_raw"],pos)
                if actual!=seq or n>88:raise DebugError("Corrupt ARM log record")
                events.append({"sequence":seq,"text":status["_raw"][pos+8:pos+8+n].decode("utf-8",errors="replace")})
            return {"events":events,"cursor":count,"lost":max(0,oldest-after-1)}

    async def detach(self, client):
        async with self.lock:
            status = await self.snapshot(client)
            self.validate_session(client, status)
            if status["state"] not in ("faulted", "finished"):
                status = await self.command(client, 7)
            del self.attachments[client]
            return {"detached":True,"state":status["state"],"instruction_stepping":False}
