import asyncio,json,sys
from datetime import timedelta
from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client
async def main():
 async with streamablehttp_client('http://127.0.0.1:55010/mcp') as (r,w,_):
  async with ClientSession(r,w) as s:
   await s.initialize()
   for name,args in json.loads(open(sys.argv[1][1:]).read() if sys.argv[1].startswith('@') else sys.argv[1]):
    res=await s.call_tool(name,args,read_timeout_seconds=timedelta(minutes=10))
    print(name, '\n'.join(getattr(x,'text','') for x in res.content),flush=True)
asyncio.run(main())
