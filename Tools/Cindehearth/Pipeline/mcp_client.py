"""Small MCP stdio client for the configured Tripo and typed Blender servers."""
import argparse
import asyncio
import json
from pathlib import Path
import secrets
import sys
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

ROOT=Path(__file__).resolve().parents[3]
async def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('server',choices=['tripo','blender','repair'])
    parser.add_argument('tool',nargs='?',default='list')
    parser.add_argument('--args-file')
    parser.add_argument('--out')
    args=parser.parse_args()
    name={'tripo':'Tripo-Cindehearth','blender':'Blender-Cindehearth','repair':'Blender-Repair-Cindehearth'}[args.server]
    config=json.loads((ROOT/'.mcp.json').read_text(encoding='utf-8-sig'))['mcpServers'][name]
    params=StdioServerParameters(command=config['command'],args=config['args'],cwd=str(ROOT))
    async with stdio_client(params) as (read,write):
        async with ClientSession(read,write) as session:
            # Pinned MCP SDK 1.29.1 increments this counter. Blender uses the
            # wire request ID as its persistent job ID, so fresh processes must
            # not all submit their first mutation as job "1".
            session._request_id=secrets.randbits(48)
            await session.initialize()
            if args.tool=='list':result=await session.list_tools()
            else:
                payload=json.loads(Path(args.args_file).read_text(encoding='utf-8')) if args.args_file else {}
                if args.server=='blender':
                    print(f'Blender request ID: {session._request_id}',file=sys.stderr)
                result=await session.call_tool(args.tool,payload)
            data=result.model_dump(mode='json',by_alias=True)
    if args.out:Path(args.out).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(data,ensure_ascii=False,indent=2))
    if data.get('isError'):raise SystemExit(1)

if __name__=='__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    asyncio.run(main())
