"""Register workspace-owned ARM tools on the existing DevBench MCP server."""
from pathlib import Path
from .host import ArmDebugger, BridgeHooks

def register_tools(mcp, require_connected=None, *, transport=None, workspace: Path):
    debugger = ArmDebugger(transport or BridgeHooks(require_connected), workspace)

    @mcp.tool()
    async def amiga_arm_attach(client: str, points_path: str = "") -> dict:
        """Attach to an instrumented application's arm_debug hook. Optional build-matched checkpoint JSON. Does not halt the CPU or attach to arbitrary games."""
        return await debugger.attach(client, points_path)

    @mcp.tool()
    async def amiga_arm_status(client: str) -> dict:
        """Read a coherent cooperative ARM snapshot, checkpoint values and any reported fault PC/SP/LR/CPSR. No live register capture."""
        return await debugger.status(client)

    @mcp.tool()
    async def amiga_arm_pause(client: str, timeout: float = 5.0) -> dict:
        """Request pause at the next instrumented checkpoint; wait up to timeout (0.1..15s). Cannot interrupt an infinite loop without checkpoints."""
        return await debugger.pause(client, timeout)

    @mcp.tool()
    async def amiga_arm_continue(client: str) -> dict:
        """Resume the instrumented ARM application or cancel a pending cooperative pause."""
        return await debugger.resume(client)

    @mcp.tool()
    async def amiga_arm_step_checkpoint(client: str, timeout: float = 5.0) -> dict:
        """From paused state, advance to the next checkpoint. This is NOT an ARM instruction or source-line single step."""
        return await debugger.step(client, timeout)

    @mcp.tool()
    async def amiga_arm_breakpoint(client: str, point: int, enabled: bool = True) -> dict:
        """Set/clear a named checkpoint ID breakpoint (eight slots). point=0 with enabled=false clears all. Does not patch executable instructions."""
        return await debugger.breakpoint(client, point, enabled)

    @mcp.tool()
    async def amiga_arm_read_memory(client: str, region: int, offset: int, size: int) -> dict:
        """Read 1..64 bytes from an app-registered RAM region while paused/faulted. Region IDs are not CPU addresses; arbitrary memory and MMIO access are unavailable."""
        return await debugger.read_memory(client, region, offset, size)

    @mcp.tool()
    async def amiga_arm_logs(client: str, after: int = 0) -> dict:
        """Read the eight-record ARM log ring after a cursor, reporting overwritten records. Cursor is scoped to the attached session."""
        return await debugger.logs(client, after)

    @mcp.tool()
    async def amiga_arm_detach(client: str) -> dict:
        """Clear cooperative breakpoints and resume before detaching. Faulted/finished targets are left stopped."""
        return await debugger.detach(client)

    return debugger
