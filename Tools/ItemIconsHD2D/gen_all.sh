#!/usr/bin/env bash
# Draws every item icon with GPT (Codex image tool) from Tools/ItemIconsHD2D/prompts, a few at a time.
# Output: ArtSource/ItemIcons_HD2D/_gen/<T_Item_...>.png (raw 1254 px pictures on magenta; finish_icons.py turns them into game icons).
# Already drawn ones are skipped, so it can be run again after a failure.  Usage: bash gen_all.sh [parallel=3] [names...]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
GEN="$ROOT/ArtSource/ItemIcons_HD2D/_gen"
STYLE_REF="$ROOT/ArtSource/ItemIcons_HD2D/_ref/style_player.png"
OLD="$ROOT/ArtSource/ItemIcons_Pixel"
P="${1:-3}"; shift || true
mkdir -p "$GEN"

one() {
  name="$1"
  out="$GEN/$name.png"
  [ -s "$out" ] && { echo "skip $name"; return 0; }
  refs=("$STYLE_REF")
  [ -f "$OLD/$name.png" ] && refs+=("$OLD/$name.png")
  for attempt in 1 2; do
    if bash "$ROOT/Tools/Prologue_20261014/gen_image.sh" "$HERE/prompts/$name.txt" "$out" "${refs[@]}" > /dev/null 2>&1; then
      echo "ok $name"; return 0
    fi
  done
  echo "FAILED $name"
}
export -f one
export GEN STYLE_REF OLD ROOT HERE

if [ $# -gt 0 ]; then names=("$@"); else names=($(ls "$HERE/prompts" | sed 's/\.txt$//')); fi
printf '%s\n' "${names[@]}" | xargs -P "$P" -I{} bash -c 'one {}'
echo "ALL DONE"
