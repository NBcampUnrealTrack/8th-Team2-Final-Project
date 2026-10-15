"""Typed MCP transport and evidence recorder; never executes Python in Blender."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
SERVER = ROOT / 'Plugins/Revvy/Content/Python/revvy_blender_mcp.py'
EVIDENCE = ROOT / 'Docs/Libregrove/BlenderQA_20260908/evidence'

class Client:
    def __init__(self, server_kind='main', poll_interval=2.0, quiet=False):
        self.server_kind = server_kind
        self.poll_interval = max(.25,float(poll_interval))
        self.quiet = quiet
        server = {'main':SERVER,'repair':ROOT / 'Tools/Cindehearth/Pipeline/blender_repair_mcp.py','precision':ROOT / 'Tools/Libregrove/precision_mcp.py','door':ROOT / 'Tools/Libregrove/door_mcp.py','buildings':ROOT / 'Tools/Libregrove/buildings_mcp.py'}[server_kind]
        self.process = subprocess.Popen([sys.executable, str(server)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, encoding='utf-8')
        self.rpc('initialize', {'protocolVersion':'2025-06-18','capabilities':{},'clientInfo':{'name':'libregrove-audit','version':'1.0'}})
        self.process.stdin.write(json.dumps({'jsonrpc':'2.0','method':'notifications/initialized'})+'\n')
        self.process.stdin.flush()
    def rpc(self, method, params):
        request = {'jsonrpc':'2.0','id':uuid.uuid4().hex,'method':method,'params':params}
        self.process.stdin.write(json.dumps(request)+'\n')
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError('MCP server disconnected; do not retry a mutation blindly')
        result = json.loads(line)
        if result.get('error'):
            raise RuntimeError(json.dumps(result['error']))
        return result['result']
    def close(self):
        self.process.stdin.close()
        self.process.wait(timeout=10)
    def call(self, name, arguments, evidence):
        EVIDENCE.mkdir(parents=True, exist_ok=True)
        path = EVIDENCE / (evidence+'.json')
        if path.exists():
            raise FileExistsError('Evidence exists; inspect it before retrying: '+str(path))
        history = [{'tool':name,'arguments':arguments}]
        def save():
            path.write_text(json.dumps(history,ensure_ascii=False,indent=2),encoding='utf-8')
        save()
        result = self.rpc('tools/call', {'name':name,'arguments':arguments})
        while True:
            history.append(result)
            save()
            value = result.get('structuredContent')
            if value is None:
                value = json.loads(next(x['text'] for x in result.get('content',[]) if x['type']=='text'))
            if result.get('isError') or value.get('success') is False:
                raise RuntimeError(json.dumps(value,ensure_ascii=False))
            payload = value.get('result',value)
            if payload.get('job_id') and payload.get('state') in ('running','queued','running_or_interrupted'):
                if not self.quiet:print('POLL '+str(payload['job_id'])+' '+payload['state'],file=sys.stderr,flush=True)
                time.sleep(self.poll_interval)
                status_tool={'main':'blender_job_status','repair':'blender_repair_job_status','precision':'blender_precision_job_status','door':'blender_door_job_status','buildings':'blender_building_job_status'}[self.server_kind]
                result = self.rpc('tools/call',{'name':status_tool,'arguments':{'job_id':payload['job_id']}})
                continue
            if payload.get('state') in ('failed','error') or payload.get('error'):
                raise RuntimeError(json.dumps(payload,ensure_ascii=False))
            return value

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('tool')
    parser.add_argument('--args-file')
    parser.add_argument('--evidence')
    parser.add_argument('--server', choices=['main','repair','precision'],default='main')
    args = parser.parse_args()
    client = Client(args.server)
    try:
        if args.tool == 'list':
            value = client.rpc('tools/list',{})
            EVIDENCE.mkdir(parents=True,exist_ok=True)
            dest = EVIDENCE / ('live_tools.json' if args.server=='main' else 'live_repair_tools.json')
            if dest.exists():
                raise FileExistsError(dest)
            dest.write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')
            print(json.dumps([t['name'] for t in value['tools']],indent=2))
        else:
            arguments=json.loads(Path(args.args_file).read_text(encoding='utf-8-sig')) if args.args_file else {}
            print(json.dumps(client.call(args.tool,arguments,args.evidence or args.tool),ensure_ascii=False,indent=2))
    finally:
        client.close()

if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    main()
