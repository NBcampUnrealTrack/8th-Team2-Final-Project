"""Keep the typed MCP session alive while its isolated Blender job runs."""
import asyncio,json,secrets,sys
from pathlib import Path
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client
ROOT=Path(__file__).resolve().parents[3]
async def main():
    tool,args_path=sys.argv[1:3]
    args=json.loads(Path(args_path).read_text(encoding='utf-8'))
    cfg=json.loads((ROOT/'.mcp.json').read_text(encoding='utf-8'))['mcpServers']['Blender-Repair-Cindehearth']
    evidence=ROOT/'Tools/Cindehearth/Pipeline/verification'/('repair_'+args['job_id']+'.json')
    history=[]
    async with stdio_client(StdioServerParameters(command=cfg['command'],args=cfg['args'],cwd=str(ROOT))) as (read,write):
        async with ClientSession(read,write) as session:
            session._request_id=secrets.randbits(48);await session.initialize()
            r=await session.call_tool(tool,args)
            while True:
                d=r.model_dump(mode='json',by_alias=True)
                payload=d.get('structuredContent') or json.loads(d['content'][0]['text'])
                history.append(payload);evidence.write_text(json.dumps(history,indent=2),encoding='utf-8')
                if d.get('isError') or payload.get('state')=='failed':
                    print(json.dumps(payload));raise SystemExit(1)
                if payload.get('state')=='finished':
                    print('FINISHED',args['job_id'],str(evidence));break
                await asyncio.sleep(2)
                r=await session.call_tool('blender_repair_job_status',{'job_id':args['job_id']})
if __name__=='__main__':asyncio.run(main())
