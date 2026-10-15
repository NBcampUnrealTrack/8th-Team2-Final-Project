"""Full build only; coordinator must save and orderly close UE beforehand."""
from pathlib import Path
import json
import subprocess
import time

ROOT = Path(__file__).resolve().parents[4]
OUT = Path(__file__).resolve().parent
command = [r'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat',
           'ProjectMEditor', 'Win64', 'Development', '-Project='+str(ROOT/'ProjectM.uproject'),
           '-WaitMutex', '-NoHotReloadFromIDE']
began = time.time()
with (OUT/'full_build.log').open('wb') as output:
    result = subprocess.run(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT,
                            creationflags=subprocess.CREATE_NO_WINDOW)
report = dict(passed=result.returncode == 0, exit_code=result.returncode,
              elapsed_seconds=time.time()-began, command=command,
              mode='full ProjectMEditor build with UE closed')
(OUT/'full_build.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
raise SystemExit(result.returncode)
