#!/usr/bin/env bash
# GPT(Codex 내장 image_gen)로 프롤로그 그림 한 장을 만든다.
# 사용: gen_image.sh <프롬프트 파일> <출력 PNG 절대경로> [참조 이미지...]
# Codex 세션이 만든 원본은 ~/.codex/generated_images/<thread id>/ 에 남는다. 파일 복사는 Codex 에 맡기지 않고
# 이 스크립트가 그 폴더에서 직접 가져온다(샌드박스에서 셸 도구가 막혀도 되게).
# 모델은 CODEX_MODEL(기본 gpt-6.1-sol). 이미지 도구는 sol 계열에만 있다.
set -u
PROMPT_FILE="$1"; OUT="$2"; shift 2
LOG="${OUT%.png}.codex.jsonl"
IMG_ARGS=()
for ref in "$@"; do IMG_ARGS+=(-i "$ref"); done
mkdir -p "$(dirname "$OUT")"

{
  echo "Use your built-in image generation tool (\$imagegen / image_gen) exactly once to create the image described below."
  echo "Do not draw it with code, PIL, SVG or any procedural method; it must be a native image-generation result."
  echo "Do not run shell commands. After the image is generated, reply with only the word DONE."
  echo
  echo "Attached images (if any) are references only, as described in the brief."
  echo
  cat "$PROMPT_FILE"
} | codex exec -m "${CODEX_MODEL:-gpt-6.1-sol}" --skip-git-repo-check -s read-only --json "${IMG_ARGS[@]}" - > "$LOG" 2>&1
code=$?
thread=$(grep -o '"thread_id":"[^"]*"' "$LOG" | head -1 | cut -d'"' -f4)
dir="$(cygpath -u "${CODEX_HOME:-$HOME/.codex}")/generated_images/$thread"
src=$(ls -t "$dir"/*.png 2>/dev/null | head -1)
if [ -n "$thread" ] && [ -n "$src" ]; then
  cp "$src" "$OUT"
  echo "ok exit=$code thread=$thread src=$src out=$OUT"
  exit 0
fi
echo "FAILED exit=$code thread=$thread log=$LOG"
grep -o '"message":"[^"]*"' "$LOG" | tail -2
exit 1
