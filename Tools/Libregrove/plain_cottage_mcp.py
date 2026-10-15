"""Typed MCP client only: no Blender Python, console or arbitrary operators."""
import argparse
import json
import sys
from pathlib import Path
import blender_mcp_client as transport

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / 'Docs/Libregrove/DirectModel_Cottage_V4_20260908/evidence'
transport.EVIDENCE = EVIDENCE

def main():
    global EVIDENCE
    parser = argparse.ArgumentParser()
    parser.add_argument('tool')
    parser.add_argument('--args')
    parser.add_argument('--evidence', required=True)
    parser.add_argument('--server', default='main', choices=['main','repair','precision','door'])
    parser.add_argument('--version', default='V4', choices=['V4','V5'])
    args = parser.parse_args()
    if args.version == 'V5':
        EVIDENCE = ROOT / 'Docs/Libregrove/DirectModel_Cottage_V5_20260908/evidence'
        transport.EVIDENCE = EVIDENCE
    client = transport.Client(args.server)
    try:
        if args.tool == 'list':
            result = client.rpc('tools/list', {})
            EVIDENCE.mkdir(parents=True, exist_ok=True)
            dest = EVIDENCE / (args.evidence + '.json')
            with dest.open('x', encoding='utf-8') as stream:
                json.dump(result, stream, ensure_ascii=False, indent=2)
            print(json.dumps([t['name'] for t in result['tools']], ensure_ascii=False))
        else:
            request = json.loads(Path(args.args).read_text(encoding='utf-8-sig')) if args.args else {}
            print(json.dumps(client.call(args.tool, request, args.evidence), ensure_ascii=False, indent=2))
    finally:
        client.close()

if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    main()
