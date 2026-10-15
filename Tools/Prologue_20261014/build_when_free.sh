#!/usr/bin/env bash
# ProjectM 에디터·커맨드렛이 하나도 없을 때 ProjectMEditor 를 빌드한다(다른 세션과 DLL 잠금 충돌 방지). 실패하면 다시 기다렸다 최대 4번.
ROOT="C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM"
busy() { powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'UnrealEditor%'\" | Where-Object { \$_.CommandLine -match 'ProjectM' }).Count" | tr -d '\r'; }
for attempt in 1 2 3 4; do
  until [ "$(busy)" = "0" ]; do sleep 10; done
  "C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" ProjectMEditor Win64 Development -Project="$ROOT/ProjectM.uproject" -WaitMutex -NoHotReloadFromIDE > "$ROOT/Saved/Prologue_20261014_build.log" 2>&1
  code=$?
  echo "attempt $attempt exit=$code $(grep -E 'Result:' "$ROOT/Saved/Prologue_20261014_build.log" | tail -1)"
  [ $code -eq 0 ] && exit 0
  grep -E "error" "$ROOT/Saved/Prologue_20261014_build.log" | head -5
  sleep 20
done
exit 1
