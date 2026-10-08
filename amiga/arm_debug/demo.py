"""Native C software test target; deliberately labelled host_demo on the wire."""
from pathlib import Path
import ctypes
import shutil
import subprocess
from .host import DebugError

def build_demo(output: Path) -> Path:
    root = Path(__file__).resolve().parent
    output.mkdir(parents=True, exist_ok=True)
    library = output / "libarm_debug_demo.so"
    cc = shutil.which("cc")
    if not cc:
        raise RuntimeError("A native C compiler is required for the ARM debug software tests")
    subprocess.run([cc, "-std=c99", "-shared", "-fPIC", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root), str(root/"core.c"), str(root/"relay.c"),
                    str(root/"examples/host_demo.c"), "-o", str(library)], check=True)
    return library

class NativeDemoTransport:
    def __init__(self, library: Path):
        self.lib = ctypes.CDLL(str(library.resolve()))
        self.lib.demo_reset.argtypes=[ctypes.c_uint32]
        self.lib.demo_reset.restype=ctypes.c_int
        self.lib.demo_hook.argtypes=[ctypes.c_char_p,ctypes.c_void_p,ctypes.c_int]
        self.lib.demo_hook.restype=ctypes.c_int
        self.lib.demo_corrupt.argtypes=[ctypes.c_uint,ctypes.c_uint32]
        self.lib.demo_log.argtypes=[ctypes.c_char_p]
        self.auto_tick=True
        self.reset(1)

    def reset(self, session):
        if self.lib.demo_reset(session):raise RuntimeError("Native debug fixture initialization failed")

    async def call(self, client, args):
        if client != "arm-debug-demo":raise DebugError("Client not found")
        self.lib.demo_service()
        if self.auto_tick and args == "S":self.lib.demo_tick()
        out=ctypes.create_string_buffer(256)
        result=self.lib.demo_hook(args.encode("ascii"),out,len(out))
        text=out.value.decode("ascii")
        if result:raise DebugError(f"ARM relay: {text}")
        return text
