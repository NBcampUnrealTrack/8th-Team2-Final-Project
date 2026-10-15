#!/usr/bin/env bash
# 펀브룩에 있는 캐릭터로 프롤로그를 끝까지 재생하며 매 프레임 시간을 기록한다. 인자: 결과 폴더 이름
ROOT="C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM"
export MSYS_NO_PATHCONV=1
dir="$ROOT/Saved/Prologue_20261014/$1"; rm -rf "$dir"; mkdir -p "$dir"
CC_SHOT="$dir/x.png" CC_STEPS="$ROOT/Tools/Prologue_20261014/qa/${2:-states_fern_trace.json}" CC_WAIT=14 PQA_TRACE="$dir/trace.jsonl" PQA_LOG="$dir/stamps.jsonl" \
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "$ROOT/ProjectM.uproject" /Game/Libregrove/Level_V8/Maps/L_Libregrove \
  -ExecCmds="py $ROOT/Tools/Prologue_20261014/pie_capture.py" -unattended -nosplash -NoSound -abslog="$dir/editor.log"
grep -E "\[Prologue\]|PROLOGUE-QA\] (to_fern|trace|helper ERROR|ERROR)" "$dir/editor.log" | cut -c1-200
