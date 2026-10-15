#!/usr/bin/env bash
# 자동 재생 조건 검증 3종. 결과: Saved/Prologue_20261014/qa_suite/<run>/editor.log + 캡처
ROOT="C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM"
export MSYS_NO_PATHCONV=1
run() {  # name map states
  local dir="$ROOT/Saved/Prologue_20261014/qa_suite/$1"; rm -rf "$dir"; mkdir -p "$dir"
  CC_SHOT="$dir/x.png" CC_STEPS="$ROOT/Tools/Prologue_20261014/qa/$3" CC_WAIT=14 PQA_LOG="$dir/stamps.jsonl" \
  "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "$ROOT/ProjectM.uproject" "$2" \
    -ExecCmds="py $ROOT/Tools/Prologue_20261014/pie_capture.py" -unattended -nosplash -NoSound -abslog="$dir/editor.log"
  echo "== $1"; grep -E "\[Prologue\]|field select|입장 실패" "$dir/editor.log" | cut -c1-200
}
rm -f "$ROOT/Saved/SaveGames/NUPrologue.sav"
run 1_new_character /Game/Libregrove/Level_V8/Maps/L_Libregrove states_autoplay_field2.json
echo "save: $(ls -la $ROOT/Saved/SaveGames/NUPrologue.sav 2>&1)"
run 2_seen_character /Game/Libregrove/Level_V8/Maps/L_Libregrove states_autoplay_field.json
rm -f "$ROOT/Saved/SaveGames/NUPrologue.sav"
run 3_offline_entry_failed /Game/Maps/L_CharacterSelect states_autoplay.json
