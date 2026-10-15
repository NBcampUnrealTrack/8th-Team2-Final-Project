"""Syntax-check the real UE translation unit without rebuilding or loading DLLs."""
from pathlib import Path
import json
import subprocess
import time

ROOT = Path(__file__).resolve().parents[4]
OUT = Path(__file__).resolve().parent
SOURCE = ROOT / 'Source/ProjectM/Field/NUCindehearthDirector.cpp'
INPUT = ROOT / 'Intermediate/Build/Win64/x64/UnrealEditor/Development/ProjectM/NUCindehearthDirector.cpp.obj.rsp'
COMPILER = Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe')
ENGINE_SOURCE = Path('C:/Program Files/Epic Games/UE_5.8/Engine/Source')

# Do not let the diagnostic pass overwrite UBT object/dependency/log products.
lines = INPUT.read_text(encoding='utf-8-sig').splitlines()
lines = [x for x in lines if not x.startswith(('/Fo', '/experimental:log', '/sourceDependencies'))]
lines += ['/Zs', '/nologo']
response = OUT / 'syntax_only.rsp'
response.write_text('\n'.join(lines) + '\n', encoding='utf-8')
started = time.time()
result = subprocess.run([str(COMPILER), '@' + str(response)], cwd=ENGINE_SOURCE, capture_output=True)
log = result.stdout.decode('utf-8', errors='replace') + result.stderr.decode('utf-8', errors='replace')
(OUT / 'syntax_only.log').write_text(log, encoding='utf-8')
report = {'passed': result.returncode == 0, 'exit_code': result.returncode,
          'elapsed_seconds': time.time() - started, 'source': str(SOURCE),
          'mode': 'MSVC /Zs using existing UE 5.8 generated include/define response; no objects or DLLs produced',
          'runtime_verified': False, 'editor_mutated': False}
(OUT / 'syntax_only.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(log)
print(json.dumps(report, indent=2))
raise SystemExit(result.returncode)
