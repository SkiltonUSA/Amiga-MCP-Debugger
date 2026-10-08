#!/usr/bin/env python3
"""Standalone MCP software demo of the ARM debug protocol. No hardware access."""
import argparse
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from mcp.server.fastmcp import FastMCP
from amiga.arm_debug.demo import build_demo, NativeDemoTransport
from amiga.arm_debug.mcp_tools import register_tools

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port",type=int,default=55018)
    args=parser.parse_args()
    mcp=FastMCP("amiga-arm-debug-software-demo",host="127.0.0.1",port=args.port)
    transport=NativeDemoTransport(build_demo(ROOT/".context/amiga/arm-debug"))
    register_tools(mcp,transport=transport,workspace=ROOT)
    print(f"SOFTWARE DEMO ONLY: http://127.0.0.1:{args.port}/mcp ; client arm-debug-demo",flush=True)
    mcp.run(transport="streamable-http")
if __name__=="__main__":main()
