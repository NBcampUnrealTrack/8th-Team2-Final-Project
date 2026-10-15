#!/usr/bin/env bash
# GPT 이미지 접근이 풀릴 때까지 5분마다 확인하고, 풀리면 프롤로그 그림 4장 → 도트 처리 → 에셋 재빌드 → PIE 캡처 → 스토리보드까지 이어 간다.
# 각 단계는 한 줄씩 진행 상황을 출력한다(모니터용). 최대 대기: MAX_WAIT_MIN(기본 240분).
set -u
ROOT="C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM"
T="$ROOT/Tools/Prologue_20261014"
ART="$ROOT/ArtSource/Prologue_20261014"
MODEL="${CODEX_MODEL:-gpt-6.1-sol}"
MAX_WAIT_MIN="${MAX_WAIT_MIN:-240}"
export CODEX_MODEL="$MODEL" MSYS_NO_PATHCONV=1

probe() { echo "Reply OK." | (cd ~ && codex exec -m "$MODEL" --skip-git-repo-check -s read-only --json - 2>&1) | grep -q '"type":"turn.completed"'; }

start=$(date +%s)
until probe; do
  waited=$(( ($(date +%s) - start) / 60 ))
  if [ "$waited" -ge "$MAX_WAIT_MIN" ]; then echo "GIVEUP still blocked after ${waited}m"; exit 2; fi
  echo "WAIT gpt still blocked (${waited}m)"
  sleep 300
done
echo "OPEN $MODEL accepted requests"

cd "$ART"
full() { cat prompts/_style.txt "prompts/$1"; }
full p1_cradle.txt > prompts/full_p1.txt
bash "$T/gen_image.sh" "$ART/prompts/full_p1.txt" "$ART/gen/p1_cradle.png" "$ART/refs/libregrove_game_ref.png" "$ART/refs/naia_sprites_ref.png" || { echo "FAIL p1"; exit 3; }
echo "IMG p1 done"

# P2~P4 는 확정된 P1 을 화풍 참조로 붙여 같이 만든다.
pids=()
for n in p2_child p3_book p4_threshold; do
  { full "$n.txt"; echo; echo "3) The previously generated picture 1 of this same set (attached): match its exact pixel density, palette, lighting and spirit-wisp design."; } > "prompts/full_$n.txt"
  bash "$T/gen_image.sh" "$ART/prompts/full_$n.txt" "$ART/gen/$n.png" "$ART/refs/libregrove_game_ref.png" "$ART/refs/naia_sprites_ref.png" "$ART/gen/p1_cradle.png" > "gen/$n.result.txt" 2>&1 &
  pids+=($!)
done
for p in "${pids[@]}"; do wait "$p"; done
for n in p2_child p3_book p4_threshold; do echo "IMG $n $(head -1 gen/$n.result.txt)"; done

python -I "$T/process_art.py" "$ART/gen/p1_cradle.png" T_Prologue_01_Cradle
python -I "$T/process_art.py" "$ART/gen/p2_child.png" T_Prologue_02_Child
python -I "$T/process_art.py" "$ART/gen/p3_book.png" T_Prologue_03_Book
python -I "$T/process_art.py" "$ART/gen/p4_threshold.png" T_Prologue_04_Threshold
echo "PIXEL done $(ls $ART/final/*.png 2>/dev/null | grep -vc preview)"

"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$ROOT/ProjectM.uproject" -run=pythonscript -script="$T/build_prologue.py" -unattended -nop4 -nosplash -NoSound > "$ROOT/Saved/Prologue_20261014_build_assets.log" 2>&1
echo "ASSETS $(python -I -c "import json;print(json.load(open('$ROOT/Saved/Prologue_20261014/build_report.json',encoding='utf-8'))['textures'])")"

dir="$ROOT/Saved/Prologue_20261014/qa4"; rm -rf "$dir"; mkdir -p "$dir"
CC_SHOT="$dir/t00_01.3.png" CC_STEPS="$T/qa/states_timeline.json" CC_WAIT=16 PQA_LOG="$dir/stamps.jsonl" \
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "$ROOT/ProjectM.uproject" /Game/Libregrove/Level_V8/Maps/L_Libregrove \
  -ExecCmds="py $T/pie_capture.py" -unattended -nosplash -NoSound -abslog="$dir/editor.log"
echo "QA $(grep -c 'shot t' "$dir/editor.log") shots, $(grep '\[Prologue\] 종료' "$dir/editor.log" | tail -1 | cut -c40-)"
python -I "$T/make_storyboard.py" qa4 && echo "DONE storyboard updated"

# 뿌리 문 Tripo 참고 그림(선택): 정면·오른쪽·뒤 회색 + 색 정면
for n in tripo_gate_front tripo_gate_right tripo_gate_back tripo_gate_textured_front; do
  bash "$T/gen_image.sh" "$ART/prompts/$n.txt" "$ART/tripo_refs/$n.png" > "gen/$n.result.txt" 2>&1
  echo "TRIPO $n $(head -1 gen/$n.result.txt)"
done
echo "ALLDONE"
