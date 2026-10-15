"""Orchestrate one advertised typed Blender MCP tool; persist and poll its job.

No code is executed in Blender. Inspection and each logical edit remain separate
explicit invocations. Never retries a mutation after an ambiguous response.
"""
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
    p=argparse.ArgumentParser()
    p.add_argument('tool');p.add_argument('args_file');p.add_argument('evidence_name')
    args=p.parse_args()
    payload=json.loads(Path(args.args_file).read_text(encoding='utf-8'))
    conf=json.loads((ROOT/'.mcp.json').read_text(encoding='utf-8-sig'))['mcpServers']['Blender-Cindehearth']
    dest=Path(__file__).parent/'verification'/(args.evidence_name+'.json')
    if dest.exists():raise ValueError('Evidence exists: inspect it before re-submitting.')
    history=[]
    def save():dest.write_text(json.dumps(history,ensure_ascii=False,indent=2),encoding='utf-8')
    async with stdio_client(StdioServerParameters(command=conf['command'],args=conf['args'],cwd=str(ROOT))) as (read,write):
        async with ClientSession(read,write) as session:
            session._request_id=secrets.randbits(48)
            await session.initialize()
            history.append({'tool':args.tool,'arguments':payload,'request_id':session._request_id});save()
            result=await session.call_tool(args.tool,payload)
            while True:
                data=result.model_dump(mode='json',by_alias=True)
                history.append(data);save()
                content=data.get('structuredContent')
                if content is None:content=json.loads(data['content'][0]['text'])
                if data.get('isError') or content.get('success') is False:
                    print(json.dumps(content,ensure_ascii=False));raise SystemExit(1)
                value=content.get('result',content)
                if value.get('job_id') and value.get('state') in ('queued','running'):
                    print('POLL',value['job_id'],value['state'],flush=True)
                    await asyncio.sleep(2)
                    result=await session.call_tool('blender_job_status',{'job_id':value['job_id']})
                    continue
                print(json.dumps(content,ensure_ascii=False,indent=2))
                if value.get('error'):raise SystemExit(1)
                break

if __name__=='__main__':
    sys.stdout.reconfigure(encoding='utf-8');asyncio.run(main())
