"""Call the authenticated local Revvy proxy. Never prints credentials."""
import argparse
import json
import os
from pathlib import Path
import sys
import urllib.request
import winreg

def call(tool, arguments):
    token = os.environ.get('REVVY_FULL_ACCESS_TOKEN')
    if not token:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, 'Environment') as key:
            token = winreg.QueryValueEx(key, 'REVVY_FULL_ACCESS_TOKEN')[0]
    request = urllib.request.Request(
        os.environ.get('CINDEHEARTH_REVVY_URL', 'http://127.0.0.1:8091/mcp'),
        data=json.dumps({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
                         'params': {'name': tool, 'arguments': arguments}}).encode(),
        headers={'Authorization': 'Bearer ' + token, 'Content-Type': 'application/json'})
    with urllib.request.urlopen(request, timeout=600) as response:
        result = json.load(response)
    if 'error' in result:
        raise RuntimeError(result['error'])
    result = result['result']
    values = []
    for item in result.get('content', []):
        if item.get('type') == 'text':
            try:
                values.append(json.loads(item['text']))
            except ValueError:
                values.append(item['text'])
    value = values[0] if len(values) == 1 else values
    if result.get('isError') or isinstance(value, dict) and value.get('success') is False:
        raise RuntimeError(json.dumps(value, ensure_ascii=False))
    return value

if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('tool', nargs='?', default='execute_python_code')
    parser.add_argument('--args', default='{}')
    parser.add_argument('--arg', action='append', default=[])
    parser.add_argument('--file')
    parser.add_argument('--stage')
    parser.add_argument('--out')
    args = parser.parse_args()
    arguments = json.loads(args.args)
    for item in args.arg:
        key, value = item.split('=', 1)
        arguments[key] = value
    if args.file:
        code = Path(args.file).read_text(encoding='utf-8-sig')
        if args.stage:
            code = 'import unreal\nSTAGE = ' + repr(args.stage) + '\n' + code
        arguments = {'code': code}
    result = call(args.tool, arguments)
    if args.out:
        Path(args.out).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    if isinstance(result, dict) and 'output' in result:
        print(result['output'])
        if result.get('error_message'):
            print(result['error_message'])
    else:
        print(json.dumps(result, ensure_ascii=False, indent=2))
