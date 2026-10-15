"""전투 VFX 후보 비교 — AI 초안(후보) → 뷰어 비교 → 사용자 선택.

뷰어는 바로가기로 떠도 후보를 늘 합쳐 읽는다:
  Tools/VFXViewer/Candidates/*.json   공용 (커밋, 작업마다 파일 하나)
  Saved/VFXViewer/Candidates.json     개인 (커밋 안 함) — 아래 명령에서 파일을 안 주면 이 파일
고른 후보(적용 대기)는 개인 것: Saved/VFXViewer/Pending.json — 3단계 [적용] 이 이 목록을 DT 에 씀

  python Tools/VFXViewer/vfx_compare.py new <후보.json> --skill 2001 --caster n1007 --slot impact --candidates NS_A,NS_B
  python Tools/VFXViewer/vfx_compare.py run <후보.json>       # 후보마다 같은 타격 기준 시각에 찍어 나란히 (격자 · compare.md)
  python Tools/VFXViewer/vfx_compare.py open [후보.json]     # 뷰어를 띄움, 파일을 주면 그 파일의 첫 스킬부터 ([ ] · Enter)
  python Tools/VFXViewer/vfx_compare.py pending              # 적용 대기 보기 (Saved/VFXViewer/Pending.json)

후보 파일 형식 (Tools/VFXViewer/README.md "후보 비교"):
  {"sets": [{"title": "...", "skill": 2001, "caster": "n1007",
             "variants": [{"label": "지금"},
                          {"label": "B", "impact": "NS_X", "color": [1.0, 0.5, 0.2], "note": "..."}]}]}
  칸(cast · projectile · impact) 키가 없으면 스킬 표 그대로, "" 이면 그 칸을 비움, 값은 전체 경로나 /Game/VFX 아래 이름.

결과: Saved/VFXCompare/<후보 이름>_<시각>/ — compare.md · sheets/set_NN.png · report.json
"""
import argparse
import datetime
import json
import os
import subprocess
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vfx_audit import (LABEL_W, PROJECT, SHOT_OFFSETS, TILE_H, TILE_W, WHITE_LEVEL,  # noqa: E402
                       asset_name, font, white_fraction)

SLOTS = ("cast", "projectile", "impact")

# 뷰어가 늘 읽는 후보 원본 (NUVFXViewerDirector 의 SharedCompareFolder · PersonalCompareFile) 과 고른 기록
SHARED_FOLDER = os.path.join(PROJECT, "Tools", "VFXViewer", "Candidates")
DEFAULT_FILE = os.path.join(PROJECT, "Saved", "VFXViewer", "Candidates.json")
PENDING_FILE = os.path.join(PROJECT, "Saved", "VFXViewer", "Pending.json")


def load_variants(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def run_viewer(variants_path, out_dir, stage, interactive):
    # 뷰어는 마지막 선택(무대 · 적 수 · 대상)을 기억하므로 구도가 바뀌는 값은 매번 고정함 (vfx_audit 과 같음)
    script = os.path.join(PROJECT, "Scripts", "run_vfx_viewer.ps1")
    command = "& '%s' -Stage '%s' -Enemies 1 -Target 0" % (script, stage)
    if variants_path:
        command += " -Compare '%s'" % variants_path
    if not interactive:
        offsets = ",".join(str(o) for o in SHOT_OFFSETS)
        command += " -ShotAtHit %s -Out '%s' -ResX 1280 -ResY 720 -Quit -Wait" % (offsets, out_dir)
    subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", command], cwd=PROJECT, check=False)


def describe_slots(variant):
    parts = []
    for slot in SLOTS:
        if slot in variant:
            parts.append("%s=%s" % (slot, asset_name(variant[slot]) or variant[slot] or "(비움)"))
    if "color" in variant:
        parts.append("color=%s" % (variant["color"],))
    return ", ".join(parts) or "표 그대로"


def build_set_sheet(set_index, data_set, reports, sheet_dir):
    variants = data_set.get("variants", [])
    big, small = font(16), font(13)
    sheet = Image.new("RGB", (LABEL_W + TILE_W * len(SHOT_OFFSETS), TILE_H * max(1, len(variants))), (18, 20, 26))
    draw = ImageDraw.Draw(sheet)
    for row, variant in enumerate(variants):
        rep = reports.get((set_index, row))
        y = row * TILE_H
        draw.text((8, y + 6), "%d  %s" % (row + 1, variant.get("label", "")), font=big, fill=(255, 216, 115))
        draw.text((8, y + 30), describe_slots(variant)[:40], font=small, fill=(160, 168, 180))
        if variant.get("note"):
            draw.text((8, y + 48), variant["note"][:40], font=small, fill=(160, 168, 180))
        if rep:
            draw.text((8, y + 70), "%s / %s / %s" % (asset_name(rep.get("cast")) or "-", asset_name(rep.get("projectile")) or "-",
                                                     asset_name(rep.get("impact")) or "-"), font=small, fill=(130, 138, 150))
            for col, shot in enumerate(rep.get("shots", [])[:len(SHOT_OFFSETS)]):
                if os.path.exists(shot):
                    tile = Image.open(shot).convert("RGB").resize((TILE_W, TILE_H), Image.BILINEAR)
                    sheet.paste(tile, (LABEL_W + col * TILE_W, y))
                    draw.text((LABEL_W + col * TILE_W + 6, y + 4), "hit%+.2f" % SHOT_OFFSETS[col], font=small, fill=(255, 255, 255))
        else:
            draw.text((8, y + 70), "뷰어 보고 없음", font=small, fill=(255, 90, 80))
        draw.line([(0, y + TILE_H - 1), (sheet.width, y + TILE_H - 1)], fill=(60, 64, 72))
    name = os.path.join(sheet_dir, "set_%02d.png" % set_index)
    sheet.save(name)
    return name


def cmd_run(args):
    variants_path = os.path.abspath(args.file)
    data = load_variants(variants_path)
    stem = os.path.splitext(os.path.basename(variants_path))[0]
    out_dir = os.path.abspath(args.out or os.path.join(PROJECT, "Saved", "VFXCompare",
                                                       "%s_%s" % (stem, datetime.datetime.now().strftime("%Y%m%d_%H%M"))))
    os.makedirs(out_dir, exist_ok=True)

    total = sum(len(s.get("variants", [])) for s in data.get("sets", []))
    print("뷰어 실행: 묶음 %d · 후보 %d" % (len(data.get("sets", [])), total))
    run_viewer(variants_path, out_dir, args.stage, interactive=False)

    report_path = os.path.join(out_dir, "report.json")
    if not os.path.exists(report_path):
        print("report.json 이 없음 — 뷰어 로그 Saved/Logs/ProjectM.log 확인")
        return 1
    with open(report_path, encoding="utf-8") as f:
        entries = json.load(f).get("entries", [])
    reports = {(e.get("compareSet", -1), e.get("variant", -1)): e for e in entries}

    sheet_dir = os.path.join(out_dir, "sheets")
    os.makedirs(sheet_dir, exist_ok=True)
    lines = ["# 후보 비교 — %s" % stem, "",
             "후보 파일 `%s` · %s · 격자는 타격 기준 %s초" % (
                 variants_path, datetime.datetime.now().strftime("%Y-%m-%d %H:%M"),
                 ", ".join("%+.2f" % o for o in SHOT_OFFSETS)), ""]
    for set_index, data_set in enumerate(data.get("sets", [])):
        sheet = build_set_sheet(set_index, data_set, reports, sheet_dir)
        lines.append("## 묶음 %d — 스킬 %s %s" % (set_index + 1, data_set.get("skill"), data_set.get("title", "")))
        lines.append("")
        lines.append("격자 `sheets/%s`" % os.path.basename(sheet))
        lines.append("")
        lines.append("| # | 후보 | 바꾼 칸 | 밝은 픽셀 최대 | 끝(초) | 경고 |")
        lines.append("|---|---|---|---|---|---|")
        for row, variant in enumerate(data_set.get("variants", [])):
            rep = reports.get((set_index, row))
            if not rep:
                lines.append("| %d | %s | %s | - | - | 뷰어 보고 없음 |" % (row + 1, variant.get("label", ""), describe_slots(variant)))
                continue
            white = [white_fraction(s) for s in rep.get("shots", []) if os.path.exists(s)]
            end = rep.get("end", -1)
            notes = list(rep.get("warnings", []))
            if rep.get("endedAtTailCap"):
                notes.insert(0, "끝나지 않는 이미터: " + " / ".join(rep.get("lingering", [])))
            lines.append("| %d | %s | %s | %.1f%% | %s | %s |" % (
                row + 1, variant.get("label", ""), describe_slots(variant).replace("|", "/"),
                max(white, default=0) * 100, "%.2f" % end if end >= 0 else "-", "; ".join(notes).replace("|", "/")))
        lines.append("")
    lines.append("- 밝은 픽셀 = 휘도 %d 이상이 3D 화면에서 차지하는 비율(찍은 장 중 최대). 전체 점검(vfx_audit)과 같은 기준" % WHITE_LEVEL)
    lines.append("- 고르기: `python Tools/VFXViewer/vfx_compare.py open %s` → 뷰어 스킬 목록에서 스킬을 고르면 후보가 나옴, [ ] 로 바꾸고 Enter" % args.file)

    with open(os.path.join(out_dir, "compare.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    if not args.keep_shots:
        for e in entries:
            for s in e.get("shots", []):
                if os.path.exists(s):
                    os.remove(s)

    print("끝: %s" % os.path.join(out_dir, "compare.md"))
    return 0


def cmd_open(args):
    run_viewer(os.path.abspath(args.file) if args.file else "", "", args.stage, interactive=True)
    print("뷰어를 띄움 — 스킬 목록에서 '후보 N' 이 붙은 스킬을 고르면 목록 아래에 후보가 나옴. "
          "[ ] 후보 · Shift+[ ] 후보 있는 스킬 · Enter 적용 대기에 담기. 적용 대기: %s" % PENDING_FILE)
    return 0


def cmd_new(args):
    candidates = [c.strip() for c in args.candidates.split(",") if c.strip()] if args.candidates else []
    variants = [{"label": "지금", "note": "스킬 표 그대로"}]
    for i, candidate in enumerate(candidates):
        variants.append({"label": chr(ord("B") + i), args.slot: candidate})

    data = load_variants(args.file) if os.path.exists(args.file) else {"sets": []}
    data["sets"].append({"title": args.title or "", "skill": args.skill, "caster": args.caster or "", "variants": variants})
    os.makedirs(os.path.dirname(os.path.abspath(args.file)), exist_ok=True)
    with open(args.file, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    print("%s 에 묶음 추가: 스킬 %d · 후보 %d" % (args.file, args.skill, len(variants)))
    return 0


def cmd_pending(args):
    if not os.path.exists(PENDING_FILE):
        print("적용 대기가 없음 (%s)" % PENDING_FILE)
        return 0
    with open(PENDING_FILE, encoding="utf-8") as f:
        changes = json.load(f).get("changes", [])
    if not changes:
        print("적용 대기가 없음 (%s)" % PENDING_FILE)
    for c in changes:
        # 스킬마다 하나, apply = 표 행 위에 덮을 값 (후보 파일이 없어져도 이것으로 적용), file = 고른 후보가 온 파일
        file_note = c.get("file", "") or "-"
        if c.get("file") and not os.path.exists(os.path.join(PROJECT, c["file"])):
            file_note += " (지금 없음)"
        print("스킬 %s → %s (%s) · %s · %s" % (c.get("skill"), c.get("label", ""), describe_slots(c.get("apply", {})),
                                             file_note, c.get("time", "")))
    return 0


def main():
    # 콘솔이 cp949 라도 한글 · 기호가 깨져 멈추지 않게
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    parser = argparse.ArgumentParser(description="전투 VFX 후보 비교 (VFX 뷰어)")
    sub = parser.add_subparsers(dest="command", required=True)

    file_help = "후보 파일 (기본 Saved/VFXViewer/Candidates.json)"

    p = sub.add_parser("run", help="후보마다 찍어 비교 격자 · compare.md 를 만듦")
    p.add_argument("file", nargs="?", default=DEFAULT_FILE, help=file_help)
    p.add_argument("--out", help="결과 폴더 (기본 Saved/VFXCompare/<이름>_<시각>)")
    p.add_argument("--stage", default="Grassland04", help="무대 이름 일부 (기본 Grassland04)")
    p.add_argument("--keep-shots", action="store_true", help="원본 캡처를 지우지 않음")
    p.set_defaults(func=cmd_run)

    p = sub.add_parser("open", help="뷰어를 띄움 (후보는 늘 공용 · 개인을 합쳐 읽음), 파일을 주면 그 파일의 첫 스킬부터")
    p.add_argument("file", nargs="?", default="", help="더 읽을 후보 파일 (없어도 됨)")
    p.add_argument("--stage", default="Grassland04")
    p.set_defaults(func=cmd_open)

    p = sub.add_parser("new", help="한 칸만 바꾼 후보 묶음을 후보 파일에 더함 (첫 후보는 표 그대로)")
    p.add_argument("file", nargs="?", default=DEFAULT_FILE, help=file_help)
    p.add_argument("--skill", type=int, required=True)
    p.add_argument("--caster", help="숫자 = 직업, n<번호> = 아군 나이아, e<번호> = 적 나이아")
    p.add_argument("--slot", choices=SLOTS, default="impact")
    p.add_argument("--candidates", help="쉼표로 이름이나 경로")
    p.add_argument("--title")
    p.set_defaults(func=cmd_new)

    p = sub.add_parser("pending", aliases=["choices"], help="적용 대기 보기 (Saved/VFXViewer/Pending.json)")
    p.set_defaults(func=cmd_pending)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
