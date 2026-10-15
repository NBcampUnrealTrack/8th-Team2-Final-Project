"""전투 VFX 전체 점검 — DT_SkillVFX 의 모든 행과 기본 공격을 VFX 뷰어로 돌려 문제를 뽑는다.

  python Tools/VFXViewer/vfx_audit.py                 # 전체 (약 10분)
  python Tools/VFXViewer/vfx_audit.py --skills 3206,2201
  python Tools/VFXViewer/vfx_audit.py --report-only <폴더>   # 이미 돌린 결과로 보고서만 다시
  python Tools/VFXViewer/vfx_audit.py --stage Grassland01  # 무대 바꾸기 (기본 Grassland04, 적 1 · 대상 0 고정)

결과: Saved/VFXAudit/SkillAudit_<시각>/
  audit.md      문제 목록 · 항목별 결과 · 점검 기준
  audit.json    같은 내용 (기계용)
  sheets/       항목마다 한 줄(타격 기준 4장), 12줄씩 한 장 — 눈으로 보는 검토용
  report.json   뷰어 원본 보고 (시간표 · 뜬 이펙트 · 끝 · 경고)

시전자는 표에서 찾는다 (값을 적어 넣지 않음):
  직업 스킬  JobSkillDefinition.Class → JobDefinition.JobKey → JobId (그 직업 전투 그림이 있는 성별로 뷰어가 고름)
  나이아 스킬 NaiaActiveLearnTable 의 주인 중 전투 그림이 있는 나이아, 없으면 스킬 공격 속성과 같은 속성의 스타팅 나이아
  보스 스킬  EnemyPatternRuleTable(SkillId → PatternId) → EnemyPatternMapTable(PatternId → NaiaId)
뷰어 사용법은 Tools/VFXViewer/README.md
"""
import argparse
import csv
import datetime
import json
import math
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

PROJECT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DATA = os.path.join(PROJECT, "Scripts", "Data")

# 타격 기준 찍는 시각 (시전 무렵 · 임팩트 · 임팩트 퍼짐 · 잔상), 타격 = 화면의 첫 타격 모션 시각
SHOT_OFFSETS = [-0.15, 0.05, 0.2, 0.5]

# 판정 기준 — 바꾸면 audit.md 의 "점검 기준" 도 같이 맞춰 쓸 것
TIMING_TOLERANCE = 0.05      # 시전 · 임팩트가 예정 시각에서 이만큼(초) 넘게 어긋나면
POSITION_TOLERANCE = 0.15    # 3D 칸 높이 대비 비율, 시전자 · 대상에서 이만큼 넘게 떨어져 뜨면
WHITE_LEVEL = 230            # 휘도(0~255)가 이 값 이상이면 밝은 픽셀, 톤매핑 탓에 세 채널이 다 245 를 넘는 일은 거의 없음
WHITE_TOLERANCE = 0.04       # 밝은 픽셀이 바탕보다 3D 칸의 이 비율 넘게 늘면 흰색 과다
LONG_TAIL = 1.2              # 행동(복귀)이 끝난 뒤 이펙트가 이만큼(초) 넘게 남으면

# 화면에 안 그려도 되는 에셋 (보이지 않는 운반체 투사체 - 임팩트만 실어 나름)
INVISIBLE_BY_DESIGN = ("NS_Job_Proj_Carrier",)

TILE_W, TILE_H = 384, 216
LABEL_W = 300
ROWS_PER_SHEET = 12


def read_csv(name):
    path = os.path.join(DATA, name)
    with open(path, encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def asset_name(path):
    return path.split("/")[-1].split(".")[-1] if path else ""


# ------------------------------------------------------------------ 점검 목록

def build_entries(only=None):
    vfx_rows = read_csv("SkillVFXTable.csv")
    job_skills = {r["Skill_ID"]: r for r in read_csv("JobSkillDefinition.csv")}
    jobs = read_csv(os.path.join("CharacterProgression", "JobDefinition.csv"))
    job_by_key = {r["JobKey"]: r for r in jobs}
    actives = {r["SkillId"]: r for r in read_csv("ActiveSkillTable.csv")}
    bosses = {r["SkillId"]: r for r in read_csv("BossSkillTable.csv")}
    naias = {r["DataIndex"]: r for r in read_csv("NaiaTable.csv")}
    learn = read_csv("NaiaActiveLearnTable.csv")
    rules = read_csv("EnemyPatternRuleTable.csv")
    pattern_map = read_csv("EnemyPatternMapTable.csv")

    animated = [n for n, r in naias.items() if r.get("AnimationClass")]
    starters = [n for n in animated if naias[n]["Category"] == "Starter"]
    wilds = [n for n in animated if naias[n]["Category"] == "Wild"]

    owners = {}
    for r in learn:
        owners.setdefault(r["Active_ID"], []).append(r["Naia_ID"])

    boss_owner = {}
    naia_by_pattern = {r["PatternId"]: r["NaiaId"] for r in pattern_map}
    for r in rules:
        if r["SkillId"] in bosses and r["PatternId"] in naia_by_pattern:
            boss_owner.setdefault(r["SkillId"], naia_by_pattern[r["PatternId"]])

    entries = []

    def add(skill, who, name, category, note=""):
        entries.append(dict(skill=int(skill), who=who, name=name, category=category, note=note))

    # 기본 공격 — 직업마다(노비스 포함), 나이아, 적
    add(0, "0", "기본 공격", "기본 · 노비스")
    for job in jobs:
        add(0, job["JobId"], "기본 공격", "기본 · " + job["DisplayName"])
    if starters:
        add(0, "n" + starters[0], "기본 공격", "기본 · 나이아")
    if wilds:
        add(0, "e" + wilds[0], "기본 공격", "기본 · 적")

    for row in vfx_rows:
        skill = row["SkillId"]
        if skill in bosses:
            naia = boss_owner.get(skill)
            add(skill, "e" + naia if naia else "e", bosses[skill]["KoreanName"], "보스",
                "" if naia else "보스 패턴 표에 없어 지금 적으로")
        elif skill in job_skills:
            job = job_by_key.get(job_skills[skill]["Class"])
            add(skill, job["JobId"] if job else "", job_skills[skill]["Name_KR"],
                job["DisplayName"] if job else job_skills[skill]["Class"],
                "" if job else "직업 표에 Class 가 없어 지금 캐릭터로")
        elif skill in actives:
            element = actives[skill]["AttackElement"]
            owned = [n for n in owners.get(skill, []) if n in animated]
            same_element = [n for n in starters if naias[n]["ElementTag"] == element]
            naia = (owned or same_element or starters or [None])[0]
            note = "" if owned else "주인 나이아에 전투 그림이 없어 같은 속성 나이아로"
            add(skill, "n" + naia if naia else "n", actives[skill]["KoreanName"], "나이아", note)
        else:
            add(skill, "", "(스킬 표에 없음)", "?")

    if only:
        entries = [e for e in entries if e["skill"] in only]
    return entries


# ------------------------------------------------------------------ 뷰어 실행

def run_viewer(entries, out_dir, stage):
    # -File 로 부르면 쉼표 목록이 배열로 안 풀려서 -Command 로 ps1 을 부름 (항목은 숫자 · 영문 · ':' 뿐이라 작은따옴표로 충분)
    skills = ",".join("'%d:%s'" % (e["skill"], e["who"]) if e["who"] else "'%d'" % e["skill"] for e in entries)
    offsets = ",".join(str(o) for o in SHOT_OFFSETS)
    script = os.path.join(PROJECT, "Scripts", "run_vfx_viewer.ps1")
    # 뷰어는 마지막 선택(무대 · 적 수 · 대상)을 기억하므로 구도가 바뀌는 값은 매번 고정함
    command = "& '%s' -Skills %s -ShotAtHit %s -Stage '%s' -Enemies 1 -Target 0 -Out '%s' -ResX 1280 -ResY 720 -Quit -Wait" % (
        script, skills, offsets, stage, out_dir)
    print("뷰어 실행: 항목 %d개" % len(entries))
    subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", command], cwd=PROJECT, check=False)


# ------------------------------------------------------------------ 판정

def white_fraction(path):
    im = Image.open(path).convert("L")
    return sum(im.histogram()[WHITE_LEVEL:]) / float(im.width * im.height)


def distance(a, b):
    if min(a + b) < 0:
        return None
    return math.hypot(a[0] - b[0], a[1] - b[1])


def judge(entry, rep, white_base):
    issues = []   # (등급, 글)

    def add(level, text):
        issues.append((level, text))

    if rep is None:
        add("오류", "뷰어 보고가 없음 (뷰어가 중간에 멈췄거나 항목을 건너뜀)")
        return issues

    effects = rep.get("effects", [])
    timeline = rep.get("timeline", {})
    spec = {"cast": rep.get("cast", ""), "projectile": rep.get("projectile", ""), "impact": rep.get("impact", "")}

    # 1. 끝나지 않는 이미터
    if rep.get("endedAtTailCap"):
        add("오류", "끝나지 않는 이미터 — 잔상 대기 상한까지 남음: " + " / ".join(rep.get("lingering", [])))

    # 2. 에셋을 못 읽음 · 실행 준비 못 함
    for w in rep.get("warnings", []):
        if "못 읽음" in w or "실행 준비" in w:
            add("오류", w)

    # 3. 칸에 넣은 이펙트가 한 번도 안 보임
    for slot, label in (("cast", "Cast"), ("projectile", "Proj"), ("impact", "Impact")):
        path = spec[slot]
        if not path or asset_name(path) in INVISIBLE_BY_DESIGN:
            continue
        if not any(e["slot"] == slot for e in effects):
            add("오류", "%s 칸 %s 이(가) 한 번도 안 보임" % (label, asset_name(path)))

    # 4. 시각이 예정과 어긋남 (시전 = 시전 지점, 임팩트 = 투사체 도착 = Hit)
    def nearest(times, t):
        return min(times, key=lambda x: abs(x - t)) if times else None

    for e in effects:
        if e["slot"] == "cast":
            expect = nearest(timeline.get("castTimes", []), e["firstSeen"])
        elif e["slot"] == "impact":
            expect = nearest(timeline.get("hitTimes", []), e["firstSeen"])
        else:
            continue
        if expect is not None and abs(e["firstSeen"] - expect) > TIMING_TOLERANCE:
            add("주의", "%s %s 이(가) 예정(%.2f초)보다 %+.2f초에 처음 보임" % (
                e["slot"], asset_name(e["asset"]), expect, e["firstSeen"] - expect))

    # 5. 엉뚱한 자리에서 뜸 (시전 = 그때의 시전자, 임팩트 = 대상)
    for e in effects:
        if e["slot"] == "cast":
            ref, who = e.get("casterScreenAtFirst", [-1, -1]), "시전자"
        elif e["slot"] == "impact":
            ref, who = rep.get("targetScreen", [-1, -1]), "대상"
        else:
            continue
        scr = e.get("firstScreen", [-1, -1])
        if min(scr) < 0 or max(scr) > 1:
            add("주의", "%s %s 이(가) 3D 화면 밖에서 처음 보임 %s" % (e["slot"], asset_name(e["asset"]), [round(v, 2) for v in scr]))
            continue
        d = distance(scr, ref)
        if d is not None and d > POSITION_TOLERANCE:
            add("주의", "%s %s 이(가) %s에서 화면 %.0f%% 떨어져 뜸" % (e["slot"], asset_name(e["asset"]), who, d * 100))

    # 6. 흰색 과다 (타격 무렵 찍은 장 중 최대)
    worst = max((w - white_base for w in rep.get("_white", [])), default=0)
    if worst > WHITE_TOLERANCE:
        add("주의", "흰색 과다 — 밝은 픽셀이 바탕보다 화면의 %.1f%% 늘어남" % (worst * 100))

    # 7. 잔상이 김 (복귀 끝 뒤)
    end = rep.get("end", -1)
    tail = end - timeline.get("returnEnd", end) if end >= 0 else 0
    if not rep.get("endedAtTailCap") and tail > LONG_TAIL:
        add("참고", "행동이 끝난 뒤 이펙트가 %.1f초 남음" % tail)

    # 8. 뷰어 경고 (제자리 · 속성색 무시 등) · 시전자 대체
    for w in rep.get("warnings", []):
        if not ("못 읽음" in w or "실행 준비" in w):
            add("참고", w)
    if entry.get("note"):
        add("참고", entry["note"])
    want = entry["who"]
    if want.startswith("n") and want[1:] and str(rep.get("allyNaiaId")) != want[1:]:
        add("참고", "나이아 %s 는 전투 그림이 없어 %s 로 틂" % (want[1:], rep.get("allyNaiaId")))
    if want.startswith("e") and want[1:] and str(rep.get("enemyNaiaId")) != want[1:]:
        add("참고", "적 %s 는 전투 그림이 없어 %s 로 틂" % (want[1:], rep.get("enemyNaiaId")))

    return issues


# ------------------------------------------------------------------ 격자 · 보고서

def font(size):
    for path in (r"C:/Windows/Fonts/malgun.ttf", r"C:/Windows/Fonts/arial.ttf"):
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def build_sheets(entries, reports, results, out_dir):
    sheet_dir = os.path.join(out_dir, "sheets")
    os.makedirs(sheet_dir, exist_ok=True)
    big, small = font(16), font(13)
    sheets = []
    for start in range(0, len(entries), ROWS_PER_SHEET):
        chunk = list(range(start, min(start + ROWS_PER_SHEET, len(entries))))
        sheet = Image.new("RGB", (LABEL_W + TILE_W * len(SHOT_OFFSETS), TILE_H * len(chunk)), (18, 20, 26))
        draw = ImageDraw.Draw(sheet)
        for row, i in enumerate(chunk):
            e, rep, issues = entries[i], reports[i], results[i]
            y = row * TILE_H
            worst = issues[0][0] if issues else ""
            color = {"오류": (255, 90, 80), "주의": (255, 200, 80)}.get(worst, (220, 225, 235))
            draw.text((8, y + 6), "#%d  %d  %s" % (i, e["skill"], e["name"]), font=big, fill=color)
            draw.text((8, y + 30), "%s · 시전 %s" % (e["category"], e["who"] or "-"), font=small, fill=(160, 168, 180))
            if rep:
                draw.text((8, y + 48), "%s / %s / %s" % (asset_name(rep.get("cast")) or "-", asset_name(rep.get("projectile")) or "-",
                                                         asset_name(rep.get("impact")) or "-"), font=small, fill=(160, 168, 180))
            for k, (level, text) in enumerate([x for x in issues if x[0] != "참고"][:7]):
                draw.text((8, y + 70 + k * 18), level + " " + text[:26], font=small, fill=color)
            for col, shot in enumerate((rep or {}).get("shots", [])[:len(SHOT_OFFSETS)]):
                if os.path.exists(shot):
                    tile = Image.open(shot).convert("RGB").resize((TILE_W, TILE_H), Image.BILINEAR)
                    sheet.paste(tile, (LABEL_W + col * TILE_W, y))
                    draw.text((LABEL_W + col * TILE_W + 6, y + 4), "hit%+.2f" % SHOT_OFFSETS[col], font=small, fill=(255, 255, 255))
            draw.line([(0, y + TILE_H - 1), (sheet.width, y + TILE_H - 1)], fill=(60, 64, 72))
        name = os.path.join(sheet_dir, "sheet_%02d.png" % (start // ROWS_PER_SHEET))
        sheet.save(name)
        sheets.append((name, chunk[0], chunk[-1]))
    return sheets


def write_report(entries, reports, results, sheets, out_dir, white_base):
    counts = {"오류": 0, "주의": 0, "참고": 0}
    for issues in results:
        for level, _ in issues:
            counts[level] = counts.get(level, 0) + 1
    flagged = sum(1 for issues in results if any(l in ("오류", "주의") for l, _ in issues))

    lines = ["# 전투 VFX 전체 점검 — %s" % datetime.datetime.now().strftime("%Y-%m-%d %H:%M"), ""]
    lines.append("항목 %d개 (스킬 행 + 기본 공격) · 오류 %d · 주의 %d · 참고 %d · 오류나 주의가 있는 항목 %d개" % (
        len(entries), counts["오류"], counts["주의"], counts["참고"], flagged))
    lines.append("")
    lines.append("격자: `sheets/` (항목마다 타격 기준 %s초, 12줄씩) · 원본 보고 `report.json`" % ", ".join("%+.2f" % o for o in SHOT_OFFSETS))

    # 구도는 무대 · 적 자리가 정함, 적이 화면 가장자리에 가까우면 큰 임팩트가 잘려 보임
    first = next((r for r in reports if r), None)
    aimed = next((r for r in reports if r and r.get("caster") != "enemy" and r.get("motion") != "제자리"), None)
    if first:
        where = ""
        if aimed and min(aimed.get("targetScreen", [-1])) >= 0:
            where = " · 적(임팩트 자리) 화면 위치 x %.2f, y %.2f (3D 칸 0~1)" % tuple(aimed["targetScreen"])
        lines.append("")
        lines.append("무대 `%s` · 적 나이아 %s 1명 · 아군 나이아 %s%s" % (
            first.get("stage", "?"), first.get("enemyNaiaId", "?"), first.get("allyNaiaId", "?"), where))
    lines.append("")
    lines.append("## 문제 목록 (오류 · 주의)")
    lines.append("")
    lines.append("| # | 스킬 | 이름 | 분류 | 등급 | 내용 |")
    lines.append("|---|---|---|---|---|---|")
    for i, (e, issues) in enumerate(zip(entries, results)):
        for level, text in issues:
            if level in ("오류", "주의"):
                lines.append("| %d | %d | %s | %s | %s | %s |" % (i, e["skill"], e["name"], e["category"], level, text.replace("|", "/")))
    lines.append("")
    lines.append("## AI 육안 검토")
    lines.append("")
    lines.append("(격자를 보고 채움 — 묻힘 · 높이 · 좌우 · 같은 속성 구분 등 숫자로 못 잡는 것)")
    lines.append("")
    lines.append("## 항목별")
    lines.append("")
    lines.append("| # | 스킬 | 이름 | 분류 | 시전 | Cast / Proj / Impact | 끝(초) | 격자 | 참고 |")
    lines.append("|---|---|---|---|---|---|---|---|---|")
    sheet_of = {}
    for name, first, last in sheets:
        for i in range(first, last + 1):
            sheet_of[i] = os.path.basename(name)
    for i, (e, rep, issues) in enumerate(zip(entries, reports, results)):
        notes = "; ".join(t for l, t in issues if l == "참고")
        slots = " / ".join(asset_name((rep or {}).get(k)) or "-" for k in ("cast", "projectile", "impact"))
        end = "%.2f" % rep["end"] if rep and rep.get("end", -1) >= 0 else "-"
        lines.append("| %d | %d | %s | %s | %s | %s | %s | %s | %s |" % (
            i, e["skill"], e["name"], e["category"], e["who"] or "-", slots, end, sheet_of.get(i, "-"), notes.replace("|", "/")))
    lines.append("")
    lines.append("## 점검 기준")
    lines.append("")
    lines.append("- **오류** — 끝나지 않는 이미터(뷰어 잔상 대기 상한까지 남음) · 에셋을 못 읽음 · 칸에 넣은 이펙트가 입자 하나 없이 끝남(운반체 `%s` 제외) · 뷰어 보고 없음" % ", ".join(INVISIBLE_BY_DESIGN))
    lines.append("- **주의** — 시전 · 임팩트가 예정 시각에서 %.2f초 넘게 어긋남 · 시전자(그 순간) · 대상에서 3D 칸의 %.0f%% 넘게 떨어져 뜨거나 화면 밖 · 밝은 픽셀(휘도 %d 이상)이 바탕보다 화면의 %.0f%% 넘게 늚(바탕 %.1f%%)" % (
        TIMING_TOLERANCE, POSITION_TOLERANCE * 100, WHITE_LEVEL, WHITE_TOLERANCE * 100, white_base * 100))
    lines.append("- **참고** — 복귀 끝 뒤 잔상 %.1f초 넘음 · 뷰어 경고(제자리 · 속성색 무시 등) · 시전자 대체" % LONG_TAIL)
    lines.append("- 위치는 이펙트 컴포넌트가 처음 보인 자리(대개 스폰 지점)라 큰 어긋남만 잡는다. 시각은 입자가 처음 생긴 프레임(1/60초) 기준")
    lines.append("- 시전자: 직업 스킬 = 그 직업 캐릭터, 나이아 스킬 = 주인 나이아(전투 그림이 없으면 같은 속성 스타팅 나이아), 보스 = 보스 패턴 표의 나이아")

    with open(os.path.join(out_dir, "audit.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    with open(os.path.join(out_dir, "audit.json"), "w", encoding="utf-8") as f:
        json.dump([dict(index=i, **e, issues=[dict(level=l, text=t) for l, t in issues],
                        white=[round(w, 4) for w in (rep or {}).get("_white", [])],
                        report={k: v for k, v in (rep or {}).items() if k != "_white"})
                   for i, (e, rep, issues) in enumerate(zip(entries, reports, results))], f, ensure_ascii=False, indent=1)


def analyze(out_dir, entries, keep_shots):
    with open(os.path.join(out_dir, "report.json"), encoding="utf-8") as f:
        raw = json.load(f).get("entries", [])
    if len(raw) != len(entries):
        print("주의: 뷰어 보고 %d개 · 점검 항목 %d개 — 앞에서부터 맞춤" % (len(raw), len(entries)))
    reports = [raw[i] if i < len(raw) else None for i in range(len(entries))]

    # 밝은 픽셀 바탕 = 모든 장 중 가장 적은 값 (같은 무대라 폭포 같은 원래 흰 곳이 들어 있음)
    whites = []
    for rep in reports:
        if rep is None:
            continue
        rep["_white"] = [white_fraction(s) for s in rep.get("shots", []) if os.path.exists(s)]
        whites += rep["_white"]
    white_base = min(whites) if whites else 0.0

    results = [judge(e, rep, white_base) for e, rep in zip(entries, reports)]
    for issues in results:
        issues.sort(key=lambda x: {"오류": 0, "주의": 1, "참고": 2}[x[0]])
    sheets = build_sheets(entries, reports, results, out_dir)
    write_report(entries, reports, results, sheets, out_dir, white_base)

    if not keep_shots:
        for rep in reports:
            for s in (rep or {}).get("shots", []):
                if os.path.exists(s):
                    os.remove(s)

    flagged = [i for i, issues in enumerate(results) if any(l in ("오류", "주의") for l, _ in issues)]
    print("끝: %s — 항목 %d개, 오류 · 주의가 있는 항목 %d개" % (os.path.join(out_dir, "audit.md"), len(entries), len(flagged)))


def main():
    # 콘솔이 cp949 라도 한글 · 기호가 깨져 멈추지 않게
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    parser = argparse.ArgumentParser(description="전투 VFX 전체 점검 (VFX 뷰어 묶음 캡처)")
    parser.add_argument("--out", help="결과 폴더 (기본 Saved/VFXAudit/SkillAudit_<시각>)")
    parser.add_argument("--skills", help="이 스킬 번호만 (쉼표, 0 = 기본 공격)")
    parser.add_argument("--keep-shots", action="store_true", help="원본 캡처를 지우지 않음")
    parser.add_argument("--report-only", help="이미 돌린 결과 폴더로 보고서만 다시 만듦 (원본 캡처가 남아 있어야 격자가 나옴)")
    parser.add_argument("--stage", default="Grassland04", help="무대 이름 일부 (기본 Grassland04 - 정식 그림이 들어간 무대)")
    args = parser.parse_args()

    only = {int(s) for s in args.skills.split(",")} if args.skills else None
    entries = build_entries(only)
    if not entries:
        print("점검할 항목이 없음")
        return 1

    if args.report_only:
        out_dir = os.path.abspath(args.report_only)
    else:
        out_dir = os.path.abspath(args.out or os.path.join(PROJECT, "Saved", "VFXAudit",
                                                             "SkillAudit_" + datetime.datetime.now().strftime("%Y%m%d_%H%M")))
        os.makedirs(out_dir, exist_ok=True)
        with open(os.path.join(out_dir, "entries.json"), "w", encoding="utf-8") as f:
            json.dump(entries, f, ensure_ascii=False, indent=1)
        run_viewer(entries, out_dir, args.stage)

    if not os.path.exists(os.path.join(out_dir, "report.json")):
        print("report.json 이 없음 — 뷰어 로그 Saved/Logs/ProjectM.log 확인")
        return 1

    analyze(out_dir, entries, args.keep_shots or bool(args.report_only))
    return 0


if __name__ == "__main__":
    sys.exit(main())
