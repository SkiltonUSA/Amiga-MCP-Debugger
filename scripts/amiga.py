#!/usr/bin/env python3
"""Workspace-local Amiga DevBench setup, launcher and cross-build."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / ".tools/amiga-devbench"
VENV = ROOT / ".tools/amiga-venv"
PYTHON = VENV / "bin/python"
LOCAL = ROOT / ".context/amiga"
SETTINGS = LOCAL / "settings.json"
PIN = json.loads((ROOT / "amiga/upstream.json").read_text())
FSUAE = Path("/Applications/FS-UAE.app/Contents/MacOS/fs-uae")
EMULATOR_CONFIG = LOCAL / "sixies.fs-uae"


def run(*args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, **kwargs)


def stage_probe():
    # DevBench discovers applications under examples/. Keep editable masters
    # in this repository; stage copies alongside its bridge for Docker mounts.
    dest = SOURCE / "examples/sixies_probe"
    dest.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / "amiga/examples/sixies_probe").iterdir():
        if source.is_file():
            shutil.copy2(source, dest / source.name)


def prepare_emulator_config():
    if not EMULATOR_CONFIG.exists():
        template = (ROOT / "amiga/emulator/sixies.fs-uae.example").read_text()
        template = template.replace("/absolute/path/to/workspace/.context/amiga/shared", str(LOCAL / "shared"))
        template = template.replace("<fsuae-port>", str(settings()["profiles"]["fsuae"]["port"]))
        EMULATOR_CONFIG.write_text(template)


def emulator():
    import configparser
    prepare_emulator_config()
    config = configparser.ConfigParser(interpolation=None)
    config.read(EMULATOR_CONFIG)
    for key in ("kickstart_file", "hard_drive_0"):
        path = Path(config["fs-uae"][key]).expanduser()
        if not path.is_absolute() or not path.exists():
            raise SystemExit(f"Set {key} to an existing absolute path in {EMULATOR_CONFIG}")
        if key == "kickstart_file" and not path.is_file():
            raise SystemExit(f"kickstart_file must be a ROM file: {path}")
    if not FSUAE.is_file():
        raise SystemExit(f"FS-UAE is not installed at {FSUAE}")
    run(FSUAE, EMULATOR_CONFIG)


def setup():
    if not shutil.which("uv"):
        raise SystemExit("uv is required for the local Python 3.12 environment.")
    SOURCE.parent.mkdir(parents=True, exist_ok=True)
    if not SOURCE.exists():
        run("git", "clone", PIN["repository"], SOURCE)
    head = subprocess.check_output(["git", "-C", str(SOURCE), "rev-parse", "HEAD"], text=True).strip()
    if head != PIN["revision"]:
        if subprocess.check_output(["git", "-C", str(SOURCE), "status", "--porcelain"], text=True).strip():
            raise SystemExit("DevBench has local changes; preserve them before changing its pinned revision.")
        run("git", "-C", SOURCE, "fetch", "origin", PIN["revision"])
        run("git", "-C", SOURCE, "checkout", "--detach", PIN["revision"])
    if not PYTHON.exists():
        run("uv", "venv", "--python", "3.12", VENV)
    for patch in sorted((ROOT / "amiga/patches").glob("*.patch")):
        applied = subprocess.run(["git", "-C", str(SOURCE), "apply", "--reverse", "--check", str(patch)],
                                 capture_output=True).returncode == 0
        if not applied:
            run("git", "-C", SOURCE, "apply", "--check", patch)
            run("git", "-C", SOURCE, "apply", patch)
    run("uv", "pip", "install", "--python", PYTHON, "-r", ROOT / "amiga/requirements.lock",
        "-e", SOURCE / "amiga-devbench")
    LOCAL.mkdir(parents=True, exist_ok=True)
    (LOCAL / "shared/Dev").mkdir(parents=True, exist_ok=True)
    if not SETTINGS.exists():
        port = int(os.environ.get("CONDUCTOR_PORT", "55010"))
        SETTINGS.write_text(json.dumps({
            "http_port": port,
            "container_command": ["docker"],
            "profiles": {
                "fsuae": {"host": "127.0.0.1", "port": port + 3},
                "hardware": {"host": None, "port": 2345, "amiga_dest": "RAM:SixiesDev"}
            }
        }, indent=2) + "\n")
    prepare_emulator_config()
    stage_probe()
    port = json.loads(SETTINGS.read_text())["http_port"]
    fragment = f'[mcp_servers.amiga]\nurl = "http://127.0.0.1:{port}/mcp"\n'
    (LOCAL / "codex-mcp.toml").write_text(fragment)
    config = ROOT / ".codex/config.toml"
    if not config.exists():
        config.parent.mkdir(exist_ok=True)
        config.write_text(fragment)
    else:
        print(f"Preserved {config}; MCP fragment is at {LOCAL / 'codex-mcp.toml'}")
    print(f"Host installed. Local settings: {SETTINGS}")
    print("Next: make amiga-doctor; make amiga-sim; make test-amiga")


def settings():
    if not SETTINGS.exists():
        raise SystemExit("Run make setup-amiga first.")
    return json.loads(SETTINGS.read_text())


def container_command():
    command = settings()["container_command"]
    if not isinstance(command, list) or not command or not all(isinstance(x, str) for x in command):
        raise SystemExit("container_command must be a JSON array, e.g. [\"docker\"].")
    return command


def build():
    command = container_command()
    run(*command, "info", stdout=subprocess.DEVNULL)
    stage_probe()
    for target in ("amiga-bridge", "examples/sixies_probe"):
        run(*command, "run", "--rm", "--platform", "linux/amd64",
            "-v", f"{SOURCE}:/work", "-w", "/work", PIN["image"],
            "make", "-C", target)
    for binary in (SOURCE / "amiga-bridge/amiga-bridge", SOURCE / "examples/sixies_probe/sixies_probe"):
        # Amiga HUNK_HEADER, not a host executable or Linux m68k ELF.
        if binary.read_bytes()[:4] != b"\x00\x00\x03\xf3":
            raise SystemExit(f"Not an Amiga Hunk executable: {binary}")
        shutil.copy2(binary, LOCAL / "shared/Dev" / binary.name)
    shutil.copy2(ROOT / "amiga/scripts/probe.rexx", LOCAL / "shared/Dev/probe.rexx")
    print(f"Amiga binaries and ARexx script staged in {LOCAL / 'shared/Dev'}")


def doctor():
    cfg = settings()
    target = json.loads((ROOT / "amiga/target.json").read_text())
    print(f"Target: {target['model']} / {target['accelerator']} / {target['cpu']}")
    print(f"OS: {target['os']}; RAM: {target['chip_ram_mb']} MB Chip + {target['fast_ram_mb']} MB Fast")
    print(f"Ethernet: {target['ethernet']}; TCP/IP stack: {target['tcp_ip_stack'] or 'not yet specified'}")
    print(f"DevBench revision: {PIN['revision']}")
    print(f"Python: {PYTHON} ({'installed' if PYTHON.exists() else 'missing'})")
    print(f"MCP / dashboard: http://127.0.0.1:{cfg['http_port']}/mcp")
    print(f"Mac emulator: {FSUAE} ({'installed' if FSUAE.is_file() else 'missing'})")
    print(f"Emulator config: {EMULATOR_CONFIG}")
    command = container_command()
    try:
        result = subprocess.run(command + ["info"], capture_output=True, timeout=15)
        print(f"Cross-build runtime: {'ready' if result.returncode == 0 else 'unavailable'} ({command})")
        if result.returncode:
            print(result.stderr.decode(errors="replace").strip())
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"Cross-build runtime unavailable: {error}")
    print("Emulator/OS readiness is not implied by a working protocol simulator.")
    for name, profile in cfg["profiles"].items():
        print(f"{name}: {profile['host'] or 'HOST NOT SET'}:{profile['port']}")
    print(f"Edit {SETTINGS} for the Amiga address and container command.")


def serve(profile, port_override=None):
    import logging
    from functools import partial
    from types import SimpleNamespace
    import uvicorn
    from amiga_devbench import builder, server, symbols
    from amiga_devbench.config import DevBenchConfig

    local = settings()
    port = port_override or local["http_port"]
    if not isinstance(port, int) or not 1024 <= port <= 65532:
        raise SystemExit("http_port must be between 1024 and 65532.")
    target = {"host": "127.0.0.1", "port": port + 1} if profile == "simulator" else local["profiles"][profile]
    if not target["host"]:
        raise SystemExit(f"Set profiles.{profile}.host in {SETTINGS} before connecting.")

    # Upstream's CLI kills a global PID and binds HTTP on all interfaces.
    # Use its app factory so each workspace owns its own process and ports.
    cfg = DevBenchConfig(
        project_root=str(SOURCE), serial_mode="tcp", serial_host=target["host"],
        serial_port=int(target["port"]), server_port=port, gdb_port=port + 2,
        simulator=(profile == "simulator"), emulator_auto_start=False,
        emulator_binary=str(FSUAE), emulator_config=str(EMULATOR_CONFIG),
        fsuae_rpc_enabled="off", crash_handler_auto_enable=False, llm_enabled=False,
        deploy_dir=str(LOCAL / "shared/Dev"),
    )
    builder.CONTAINER_NAME = "sixies-amiga-" + hashlib.sha256(str(ROOT).encode()).hexdigest()[:12]
    builder.DOCKER_IMAGE = PIN["image"]
    symbols.DOCKER_IMAGE = PIN["image"]
    class WorkspaceBuilder(builder.Builder):
        async def build(self, project=None):
            stage_probe()
            if project in (None, "sixies_probe", "examples/sixies_probe"):
                bridge = await super().build("amiga-bridge")
                if not bridge.success:
                    return bridge
                project = "examples/sixies_probe"
            return await super().build(project)

    server.Builder = WorkspaceBuilder
    # Upstream invokes docker by name. This workspace-only adapter permits an
    # explicitly selected Podman connection without changing global defaults.
    shim_dir = LOCAL / "bin"
    shim_dir.mkdir(exist_ok=True)
    command = container_command()
    resolved = shutil.which(command[0])
    if resolved:
        command[0] = resolved
    shim = shim_dir / "docker"
    shim.write_text(f"#!{PYTHON}\nimport os, sys\ncommand = {command!r}\nos.execv(command[0], command + sys.argv[1:])\n")
    shim.chmod(0o755)
    os.environ["PATH"] = str(shim_dir) + os.pathsep + os.environ["PATH"]
    logging.basicConfig(level=logging.INFO)
    if cfg.simulator:
        # Pinned upstream simulator sends HB for PING, but the MCP tool
        # waits for PONG. Match the real daemon's response framing.
        original_simulator = server.AmigaSimulator

        class ProtocolSimulator(original_simulator):
            def _handle_command(self, send_line, line):
                if line == "PING":
                    send_line(f"PONG|1|{self._free_chip}|{self._free_fast}")
                else:
                    super()._handle_command(send_line, line)

        server.AmigaSimulator = ProtocolSimulator
    args = SimpleNamespace(simulator=cfg.simulator)
    # Keep ARM debugger sources in this repository, not the generated checkout.
    # The application-side relay uses existing CALLHOOK IPC; no daemon patch.
    sys.path.insert(0, str(ROOT))
    from amiga.arm_debug.mcp_tools import register_tools
    from amiga_devbench import mcp_tools
    register_tools(mcp_tools.mcp, mcp_tools._require_connected, workspace=ROOT)
    app = server.create_app(args, cfg)
    # Remote targets must transfer files over the bridge, never report a
    # local shared-folder copy as successful deployment to real hardware.
    if profile == "hardware":
        server._deployer._force_bridge = True
        server._deployer.deploy_smart = partial(
            server._deployer.deploy_smart,
            amiga_dest=target.get("amiga_dest", "DH2:Dev"),
        )
    print(f"Profile: {profile}; dashboard http://127.0.0.1:{port}/", flush=True)
    uvicorn.run(app, host="127.0.0.1", port=port, log_level="info")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["setup", "doctor", "build", "emulator", "serve"])
    parser.add_argument("--profile", choices=["simulator", "fsuae", "hardware"], default="simulator")
    parser.add_argument("--port", type=int, help="Override HTTP port (tests use an isolated port range)")
    args = parser.parse_args()
    if args.command == "serve":
        serve(args.profile, args.port)
    else:
        globals()[args.command]()


if __name__ == "__main__":
    try:
        main()
    except (subprocess.CalledProcessError, OSError, ValueError) as error:
        raise SystemExit(str(error)) from error
