# -*- coding: utf-8 -*-
"""Job selection, starter Naia selection and quest offer proposals (proposal only, no game assets touched).

Output: Docs/HUD_Selection_Proposal_20261015/selection.html (+ png/).
"Now" frames are real game captures (Libregrove next to Elladon, qa/states_selection*.json): WBP_JobSelection as the game builds it,
the native starter Naia window, and the quest offer (quest 10004, which has an item reward). The mocks are drawn here at 1080p over
a frame captured without screen UI (sel_plate). Character art is the game's own sprite sheets (ArtSource/Character, frame 0, whole
multiples), Naia art the game's sheets (Docs/NaiaPixelCast_20260923/source_sheets), job emblems ArtSource/UI/JobEmblems_20260927,
reward / element icons the new GPT pixel icons in ArtSource/UI/HUD2026/reward_icons (item-icon tone). Text comes from the data tables.
Run: python Tools/HUDRenewal/build_selection.py
"""
import base64
import csv
import importlib.util
import io
import pathlib

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('build_shop', HERE / 'build_shop.py')
bs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bs)
bd = bs.bd
Canvas, font, SS = bd.Canvas, bd.font, bd.SS
NAVY, IVORY, GOLD, GOLD_HI, MUTED = bd.NAVY, bd.IVORY, bd.GOLD, bd.GOLD_HI, bd.MUTED
rr, pix = bs.rr, bs.pix

ROOT = HERE.parents[1]
CAP = ROOT / 'Saved' / 'CharacterCreateGlass'
OUT = ROOT / 'Docs' / 'HUD_Selection_Proposal_20261015'
CHAR = ROOT / 'ArtSource' / 'Character'
NAIA = ROOT / 'Docs' / 'NaiaPixelCast_20260923' / 'source_sheets'
EMBLEM = ROOT / 'ArtSource' / 'UI' / 'JobEmblems_20260927'
ICONS = ROOT / 'ArtSource' / 'UI' / 'HUD2026' / 'reward_icons'
ITEMS = ROOT / 'ArtSource' / 'ItemIcons_HD2D'
DATA = ROOT / 'Scripts' / 'Data'
TEXT = (231, 226, 217)

# ---------------------------------------------------------------- data
def table(name, key):
    raw = (DATA / name).read_bytes().decode('utf-8-sig', errors='replace')
    out = {}
    for r in csv.DictReader(io.StringIO(raw)):
        out.setdefault(r[key], r)
    return out


SKILLS = table('JobSkillDefinition.csv', 'Skill_ID')
NAIA_ROWS = table('NaiaTable.csv', 'DataIndex')
PASSIVES = table('PassiveTable.csv', 'Name')
QUESTS = table('QuestTable.csv', 'Quest_ID')
ITEM_ROWS = table('ItemTable.csv', 'NumericId')
JOB_ROWS = table('CharacterProgression/JobDefinition.csv', 'JobId')
ADV = list(csv.DictReader(open(DATA / 'CharacterProgression' / 'JobBranchDefinition.csv', encoding='utf-8-sig')))

# accent per job, picked from each job's outfit (proposal --- the code's GetJobAccent colours don't match the art)
JOBS = [
    # id, key, emblem, field art, combat art, accent, skill ids
    (1001, 'NaiaKnight', 'NaiaKnight', ('M', 'NaiaKnight'), ('M', 'NaiaKnight'), (126, 155, 224), (3001, 3002, 3003)),
    (1002, 'Tracker', 'Tracker', ('M', 'Tracker'), ('M', 'Tracker'), (217, 167, 74), (3101, 3102, 3103)),
    (1003, 'Elementalist', 'Elementalist', ('F', 'Mage'), ('F', 'Mage'), (127, 200, 238), (3202, 3203, 3201)),
    (1004, 'Gambler', 'Gambler', ('M', 'Gambler'), ('M', 'Gambler'), (208, 106, 126), (3301, 3302, 3303)),
    (1005, 'Druid', 'Druid', ('M', 'Druid'), ('F', 'Druid'), (147, 181, 103), (3401, 3402, 3403)),
]
JOB_SEL = 1
STARTERS = [1001, 1004, 1007, 1010, 1013, 1016]
NAIA_SHEET = {1001: '1001_Bolto', 1004: '1004_Puruna_00', 1007: '1007_Bulcoi', 1010: '1010_Aqupong', 1013: '1013_Togra', 1016: '1016_Hwiry'}
ELEMENT = {  # tag tail -> (name, colour, icon)
    'Lightning': ('번개', (236, 202, 86)), 'Grass': ('풀', (120, 196, 110)), 'Fire': ('불', (236, 112, 70)),
    'Water': ('물', (96, 168, 236)), 'Earth': ('땅', (190, 142, 92)), 'Wind': ('바람', (128, 214, 204)),
}
NAIA_SEL = 2
STAT_KEYS = [('BaseHealth', '체력'), ('BaseSP', 'SP'), ('BaseAttackPower', '공격'), ('BaseMagicAttackPower', '마법 공격'),
             ('BaseDefense', '방어'), ('BaseSpeed', '속도')]


def sheet_frame(path, cell_w=None):
    im = Image.open(path).convert('RGBA')
    w = cell_w or im.width // 8
    return im.crop((0, 0, w, im.height))


def job_art(gender, job, combat):
    if gender == 'M' and not combat:
        im = sheet_frame(CHAR / job / ('MCh_%s_Body_Idle_Front_Sheet.png' % job), 60)
        for layer in ('Novice/MCh_FaceEye_Idle_Front_Sheet.png', 'Novice/MCh_Hair1_Idle_Front_Sheet.png'):
            im.alpha_composite(sheet_frame(CHAR / layer, 60))
        return im
    folder, prefix = (CHAR / job, 'MCh_' + job) if gender == 'M' else (CHAR / 'Female', 'WCh_' + job)
    kind = 'Combat_Idle' if combat else 'Idle_Front'
    im = sheet_frame(folder / ('%s_Body_%s_Sheet.png' % (prefix, kind)), None if combat else 60)
    for layer in ('FaceEye', 'Hair'):
        p = folder / ('%s_%s_%s_Sheet.png' % (prefix, layer, kind))
        if p.exists():
            im.alpha_composite(sheet_frame(p, im.width))
    return im


def naia_art(nid):
    folder = NAIA_SHEET[nid]
    return sheet_frame(NAIA / ('%s__Textures__%s.png' % (folder, folder.replace('_00', ''))), 128)


def naia_info(nid):
    r = NAIA_ROWS[str(nid)]
    tail = r['ElementTag'].split('.')[-1]
    el_name, el_col = ELEMENT[tail]
    nxt = NAIA_ROWS.get(r['NextEvolutionId'], {})
    psv = PASSIVES.get(r['PassiveSkill1Id'], {})
    return dict(id=nid, name=r['KoreanName'], tail=tail, element=el_name, colour=el_col,
                stats={k: float(r[k]) for k, _ in STAT_KEYS}, evo=(nxt.get('KoreanName', '?'), r['EvolutionLevel']),
                passive=(psv.get('KoreanName', ''), psv.get('Description', '')))


def icon(name, size, smooth=True):
    p = ICONS / (name + '.png')
    if not p.exists():
        p = ITEMS / (name + '.png')
    im = Image.open(p).convert('RGBA')
    return im.resize((size, size), Image.LANCZOS if smooth else Image.NEAREST)


def emblem(key, size, colour=None, alpha=1.0):
    im = Image.open(EMBLEM / ('T_JobEmblem_%s.png' % key)).convert('RGBA').resize((size, size), Image.LANCZOS)
    if colour is not None or alpha != 1.0:
        a = np.asarray(im).copy()
        if colour is not None:
            a[..., :3] = colour
        a[..., 3] = (a[..., 3] * alpha).astype(np.uint8)
        im = Image.fromarray(a)
    return im


def dimmed(im, k=0.5, alpha=0.9):
    im = ImageEnhance.Color(im).enhance(0.55)
    im = ImageEnhance.Brightness(im).enhance(k)
    a = np.asarray(im).copy()
    a[..., 3] = (a[..., 3] * alpha).astype(np.uint8)
    return Image.fromarray(a)


def silhouette(im, colour=(18, 28, 42), alpha=0.92):
    a = np.asarray(im).copy()
    mask = a[..., 3] > 0
    a[mask, 0], a[mask, 1], a[mask, 2] = colour
    a[..., 3] = (mask * 255 * alpha).astype(np.uint8)
    return Image.fromarray(a)


# ---------------------------------------------------------------- drawing helpers
def blurred(plate, radius=7):
    """The scene behind a full-screen choice: background blur (UMG BackgroundBlur), so the people standing in it don't read as part of the row."""
    return plate.filter(ImageFilter.GaussianBlur(radius))


def veil(c, centre=0.62, edge=0.86, cy=0.45):
    """Dark navy over the whole scene, a little lighter in the middle (the UI manager's dim, with a vignette)."""
    W, H = c.W * SS, c.H * SS
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    d = np.sqrt(((xx - W / 2) / (W * 0.62)) ** 2 + ((yy - H * cy) / (H * 0.62)) ** 2)
    a = np.clip(centre + (edge - centre) * d, 0, 1)
    arr = np.zeros((H, W, 4), np.uint8)
    arr[..., 0], arr[..., 1], arr[..., 2] = 7, 13, 22
    arr[..., 3] = (a * 255).astype(np.uint8)
    c.layer = Image.alpha_composite(c.layer, Image.fromarray(arr))
    c.d = ImageDraw.Draw(c.layer)


def glow(c, cx, cy, rx, ry, colour, alpha=0.5):
    W, H = int(rx * 2.4 * SS), int(ry * 2.4 * SS)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    d = np.sqrt(((xx - W / 2) / (rx * SS)) ** 2 + ((yy - H / 2) / (ry * SS)) ** 2)
    a = np.clip(1 - d, 0, 1) ** 1.6 * alpha
    arr = np.zeros((H, W, 4), np.uint8)
    arr[..., 0], arr[..., 1], arr[..., 2] = colour
    arr[..., 3] = (a * 255).astype(np.uint8)
    c.layer.alpha_composite(Image.fromarray(arr), (int(cx * SS - W / 2), int(cy * SS - H / 2)))


def beam(c, cx, top, bottom, w, colour, alpha=0.22):
    W, H = int(w * SS), int((bottom - top) * SS)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    across = 1 - np.abs(xx - W / 2) / (W / 2)
    a = np.clip(across, 0, 1) ** 1.5 * (yy / H) ** 0.7 * alpha
    arr = np.zeros((H, W, 4), np.uint8)
    arr[..., 0], arr[..., 1], arr[..., 2] = colour
    arr[..., 3] = (a * 255).astype(np.uint8)
    c.layer.alpha_composite(Image.fromarray(arr), (int((cx - w / 2) * SS), int(top * SS)))


def frost(c, box, r=20, radius=12):
    """Blur the scene under a glass panel (the HUD glass uses BackgroundBlur)."""
    x0, y0, x1, y1 = [int(v) for v in box]
    region = c.base.crop((x0 - 30, y0 - 30, x1 + 30, y1 + 30)).filter(ImageFilter.GaussianBlur(radius)).crop((30, 30, 30 + x1 - x0, 30 + y1 - y0))
    mask = Image.new('L', region.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, region.width - 1, region.height - 1], r, fill=255)
    c.base.paste(region, (x0, y0), mask)          # into the scene itself: the glass fill replaces layer pixels, so it can't sit in the layer


def shadow_ellipse(c, cx, y, rx, ry=None, alpha=0.5):
    ry = ry or rx * 0.22
    sh = Image.new('RGBA', (int(rx * 2 * SS) + 4, int(ry * 2 * SS) + 4), (0, 0, 0, 0))
    ImageDraw.Draw(sh).ellipse([0, 0, rx * 2 * SS, ry * 2 * SS], fill=(0, 0, 0, int(255 * alpha)))
    c.layer.alpha_composite(sh, (int((cx - rx) * SS), int((y - ry) * SS)))


def sprite(c, im, cx, bottom, scale, bbox=True):
    """Pixel art at a whole multiple, horizontally centred on its drawn pixels, feet at `bottom`."""
    b = im.getbbox() if bbox else (0, 0, im.width, im.height)
    crop = im.crop(b)
    w, h = crop.width * scale, crop.height * scale
    pix(c, crop, (cx - w / 2, bottom - h, cx + w / 2, bottom))
    return w, h


def paste(c, im, x, y):
    c.layer.alpha_composite(im.resize((im.width * SS, im.height * SS), Image.LANCZOS), (int(x * SS), int(y * SS)))


def fit_text(c, s, px, width, semi=True):
    f = font(px, semi)
    if c.d.textlength(s, font=f) / SS <= width:
        return s
    while s and c.d.textlength(s + '…', font=f) / SS > width:
        s = s[:-1]
    return s.rstrip() + '…'


def wrap_lines(c, s, px, width, semi=True, max_lines=9):
    f = font(px, semi)
    words, lines, cur = s.split(' '), [], ''
    for w in words:
        t = (cur + ' ' + w).strip()
        if c.d.textlength(t, font=f) / SS > width and cur:
            lines.append(cur)
            cur = w
        else:
            cur = t
    lines.append(cur)
    if len(lines) > max_lines:
        lines = lines[:max_lines]
        lines[-1] = fit_text(c, lines[-1] + ' …', px, width, semi)
    return lines


def para(c, xy, s, px, width, colour=TEXT, lh=1.55, semi=True, max_lines=9, shadow=False):
    lines = wrap_lines(c, s, px, width, semi, max_lines)
    for i, l in enumerate(lines):
        c.text((xy[0], xy[1] + i * px * lh), l, px, colour, semi, 'la', shadow=shadow)
    return len(lines) * px * lh


def tag(c, x, y, s, colour, px=13, fill_alpha=40):
    f = font(px, True)
    w = c.d.textlength(s, font=f) / SS + 20
    rr(c, (x, y - px * 0.9, x + w, y + px * 0.9), px, colour + (fill_alpha,), colour + (190,), 1)
    c.text((x + w / 2, y + 0.5), s, px, colour, True, 'mm', shadow=False)
    return x + w


def heading(c, x, y, s, px=14):
    c.diamond((x + 4, y), 3.5)
    c.text((x + 14, y), s, px, GOLD_HI, True, 'lm', shadow=False)


def pill(c, box, label, gold, key=None, px=20):
    x0, y0, x1, y1 = box
    h = y1 - y0
    if gold:
        rr(c, box, h / 2, (214, 190, 140, 255), (150, 118, 58, 255), 1.2)
        rr(c, (x0 + 3, y0 + 2, x1 - 3, y0 + h * 0.45), h / 2 - 2, (255, 255, 255, 40))
    else:
        rr(c, box, h / 2, (22, 41, 61, 235), (241, 237, 230, 70), 1.2)
    col = NAVY if gold else IVORY
    tx = (x0 + x1) / 2 - (16 if key else 0)
    c.text((tx, (y0 + y1) / 2 + 1), label, px, col, True, 'mm', shadow=False)
    if key:
        f = font(px, True)
        kx = tx + c.d.textlength(label, font=f) / SS / 2 + 12
        bd.keychip(c, (kx, (y0 + y1) / 2 - 12), key, 24)


def title_block(c, cap, title, y=64):
    c.text((960, y), cap, 15, GOLD_HI, True, 'mm', shadow=False)
    c.text((960, y + 46), title, 38, IVORY, False, 'mm')
    c.d.line(c.s(780, y + 84, 940, y + 84), fill=GOLD + (150,), width=SS)
    c.d.line(c.s(980, y + 84, 1140, y + 84), fill=GOLD + (150,), width=SS)
    c.diamond((960, y + 84), 5)


def hint_bar(c, y, items):
    """Key hints centred at y: [(key, label), ...]."""
    f = font(15, True)
    widths = [26 + 8 + c.d.textlength(l, font=f) / SS for _, l in items]
    total = sum(widths) + 28 * (len(items) - 1)
    x = 960 - total / 2
    for (k, l), w in zip(items, widths):
        if k:
            bd.keychip(c, (x, y - 13), k, 26)
            c.text((x + 34, y), l, 15, MUTED, True, 'lm', shadow=False)
        else:
            c.text((x, y), l, 15, MUTED, True, 'lm', shadow=False)
        x += w + 28


def bar(c, x, y, w, frac, colour, h=8):
    rr(c, (x, y - h / 2, x + w, y + h / 2), h / 2, (241, 237, 230, 26))
    rr(c, (x, y - h / 2, x + max(h, w * frac), y + h / 2), h / 2, colour + (255,))


def skill_rows(c, x, y, w, skill_ids, accent, desc=True, row_h=58):
    kind_col = {'패시브': (170, 182, 191), '액티브': accent, '시너지': GOLD_HI}
    for sid in skill_ids:
        s = SKILLS[str(sid)]
        kind = s['SkillType']
        nx = tag(c, x, y + 13, kind, kind_col.get(kind, MUTED), 12.5)
        c.text((nx + 10, y + 13), s['Name_KR'], 19, IVORY, False, 'lm', shadow=False)
        if desc:
            c.text((x, y + 40), fit_text(c, s['Description'], 14.5, w), 14.5, MUTED, True, 'lm', shadow=False)
        y += row_h
    return y


# ---------------------------------------------------------------- job selection
def job_detail(sel):
    jid, key, emb, field, combat, accent, skills = JOBS[sel]
    row = JOB_ROWS[str(jid)]
    adv = [a['DisplayName'] for a in ADV if a['ParentJobId'] == str(jid)]
    return row['DisplayName'], row['Description'], adv


def job_a(plate):
    """A. five paths on a stage: the player stands as each job, the chosen one steps into the light."""
    c = Canvas(blurred(plate))
    veil(c, 0.7, 0.9)
    title_block(c, '첫 직업 · 엘라던의 단상', '어떤 힘으로 나아갈 것인가')
    xs = [420, 690, 960, 1230, 1500]
    feet = 612
    for i, (jid, key, emb, field, combat, accent, skills) in enumerate(JOBS):
        on = i == JOB_SEL
        if on:
            beam(c, xs[i], 150, feet + 10, 300, accent, 0.20)
            glow(c, xs[i], feet + 2, 150, 34, accent, 0.55)
        shadow_ellipse(c, xs[i], feet, 62 if not on else 80, alpha=0.55)
        art = job_art(*(combat if on else field), combat=on)
        sprite(c, art if on else dimmed(art, 0.55), xs[i], feet + 4, 3)
        name = JOB_ROWS[str(jid)]['DisplayName']
        paste(c, emblem(emb, 40, GOLD_HI if on else (150, 160, 170), 1.0 if on else 0.7), xs[i] - 20, feet + 22)
        c.text((xs[i], feet + 88), name, 22 if on else 18, GOLD_HI if on else (150, 160, 170), not on, 'mm', shadow=on)
    # detail sheet
    name, motto, adv = job_detail(JOB_SEL)
    jid, key, emb, field, combat, accent, skills = JOBS[JOB_SEL]
    X0, Y0, X1, Y1 = 150, 736, 1770, 1012
    c.glass((X0, Y0, X1, Y1), r=20, alpha=0.88, line=0.16, sheen=False)
    rr(c, (X0 + 1, Y0 + 22, X0 + 5, Y1 - 22), 2, accent + (255,))
    paste(c, emblem(emb, 92, accent), X0 + 30, Y0 + 30)
    c.text((X0 + 142, Y0 + 58), name, 32, IVORY, False, 'lm')
    para(c, (X0 + 34, Y0 + 146), '“' + motto + '”', 18, 520, (214, 210, 200))
    c.d.line(c.s(X0 + 600, Y0 + 28, X0 + 600, Y1 - 28), fill=(241, 237, 230, 34), width=SS)
    heading(c, X0 + 630, Y0 + 40, '시작 스킬 3종 · 전직하면 바로 배움')
    skill_rows(c, X0 + 630, Y0 + 62, 560, skills, accent, row_h=66)
    c.d.line(c.s(X0 + 1230, Y0 + 28, X0 + 1230, Y1 - 28), fill=(241, 237, 230, 34), width=SS)
    heading(c, X0 + 1258, Y0 + 40, 'Lv.20 두 갈래 전직')
    ax = X0 + 1258
    for a in adv:
        ax = tag(c, ax, Y0 + 82, a, IVORY, 15, 18) + 10
    c.text((X0 + 1258, Y0 + 126), '지금 고르는 건 첫 직업 하나뿐입니다', 14, MUTED, True, 'lm', shadow=False)
    pill(c, (X0 + 1258, Y1 - 82, X1 - 30, Y1 - 28), name + '로 정하기', True, 'F', 19)
    hint_bar(c, 1046, [('←', ''), ('→', '고르기'), ('F', '정하기'), ('', '정한 뒤에는 바꿀 수 없습니다')])
    return c.done()


def job_b(plate):
    """B. five tall glass cards in one window."""
    c = Canvas(blurred(plate))
    veil(c, 0.6, 0.82)
    X0, Y0, X1, Y1 = 196, 96, 1724, 1000
    c.glass((X0, Y0, X1, Y1), r=22, alpha=0.9, line=0.16, sheen=False)
    c.text((X0 + 48, Y0 + 52), '첫 직업 · 엘라던', 15, GOLD_HI, True, 'lm', shadow=False)
    c.text((X0 + 48, Y0 + 96), '어떤 힘으로 나아갈 것인가', 34, IVORY, False, 'lm')
    cw, ch, gap = 270, 510, 21
    x = 960 - (5 * cw + 4 * gap) / 2
    for i, (jid, key, emb, field, combat, accent, skills) in enumerate(JOBS):
        on = i == JOB_SEL
        y0 = Y0 + 150 - (14 if on else 0)
        box = (x, y0, x + cw, y0 + ch)
        rr(c, box, 16, (10, 19, 30, 245), GOLD + (230,) if on else (241, 237, 230, 34), 2 if on else 1)
        # accent wash behind the figure
        wash = Image.new('RGBA', (int(cw * SS), int(330 * SS)), (0, 0, 0, 0))
        wa = np.zeros((wash.height, wash.width, 4), np.uint8)
        wa[..., 0], wa[..., 1], wa[..., 2] = accent
        wa[..., 3] = (np.linspace(0, 1, wash.height)[:, None] ** 1.3 * (90 if on else 40)).astype(np.uint8)
        c.layer.alpha_composite(Image.fromarray(wa), (int(x * SS), int((y0 + 40) * SS)))
        paste(c, emblem(emb, 180, accent, 0.18 if on else 0.10), x + cw / 2 - 90, y0 + 70)
        art = job_art(*(combat if on else field), combat=on)
        sprite(c, art if on else dimmed(art, 0.8, 1.0), x + cw / 2, y0 + 372, 3 if on else 3)
        rr(c, (x + 1, y0 + 376, x + cw - 1, y0 + 377), 0, (241, 237, 230, 30))
        name = JOB_ROWS[str(jid)]['DisplayName']
        c.text((x + cw / 2, y0 + 410), name, 23, GOLD_HI if on else IVORY, False, 'mm', shadow=False)
        lines = wrap_lines(c, JOB_ROWS[str(jid)]['Description'], 14, cw - 40, True, 3)
        for k, l in enumerate(lines):
            c.text((x + cw / 2, y0 + 446 + k * 22), l, 14, MUTED, True, 'mm', shadow=False)
        x += cw + gap
    jid, key, emb, field, combat, accent, skills = JOBS[JOB_SEL]
    name, motto, adv = job_detail(JOB_SEL)
    dy = Y1 - 150
    c.d.line(c.s(X0 + 40, dy - 12, X1 - 40, dy - 12), fill=(241, 237, 230, 30), width=SS)
    heading(c, X0 + 48, dy + 18, '시작 스킬')
    sx = X0 + 48
    for sid in skills:
        s = SKILLS[str(sid)]
        sx = tag(c, sx, dy + 58, s['SkillType'] + ' · ' + s['Name_KR'], IVORY, 15, 16) + 10
    heading(c, X0 + 48, dy + 100, 'Lv.20 전직  ' + ' · '.join(adv), 14)
    pill(c, (X1 - 420, dy + 30, X1 - 48, dy + 86), name + '로 정하기', True, 'F', 19)
    return c.done()


def job_c(plate):
    """C. list on the left, everything about the chosen job on the right (Octopath's job screen)."""
    c = Canvas(blurred(plate))
    veil(c, 0.62, 0.84)
    X0, Y0, X1, Y1 = 150, 84, 1770, 1000
    c.glass((X0, Y0, X1, Y1), r=22, alpha=0.92, line=0.16, sheen=False)
    c.text((X0 + 44, Y0 + 50), '첫 직업 · 엘라던', 15, GOLD_HI, True, 'lm', shadow=False)
    c.text((X0 + 44, Y0 + 92), '어떤 힘으로 나아갈 것인가', 30, IVORY, False, 'lm')
    ly = Y0 + 140
    for i, (jid, key, emb, field, combat, accent, skills) in enumerate(JOBS):
        on = i == JOB_SEL
        box = (X0 + 28, ly, X0 + 470, ly + 132)
        rr(c, box, 14, (22, 41, 61, 240) if on else (14, 26, 40, 150), GOLD + (200,) if on else (241, 237, 230, 22), 1.4 if on else 1)
        if on:
            rr(c, (X0 + 29, ly + 18, X0 + 33, ly + 114), 2, GOLD_HI + (255,))
        rr(c, (X0 + 46, ly + 18, X0 + 142, ly + 114), 14, accent + (70 if on else 26,), accent + (170 if on else 90,), 1)
        paste(c, emblem(emb, 80, IVORY if on else (170, 180, 190), 1.0 if on else 0.75), X0 + 54, ly + 26)
        c.text((X0 + 160, ly + 41), JOB_ROWS[str(jid)]['DisplayName'], 21, GOLD_HI if on else IVORY, False, 'lm', shadow=False)
        para(c, (X0 + 160, ly + 66), JOB_ROWS[str(jid)]['Description'], 13.5, 290, MUTED, 1.5, True, 2)
        ly += 142
    jid, key, emb, field, combat, accent, skills = JOBS[JOB_SEL]
    name, motto, adv = job_detail(JOB_SEL)
    DX0, DY0 = X0 + 500, Y0 + 32
    DX1, DY1 = X1 - 30, Y1 - 30
    rr(c, (DX0, DY0, DX1, DY1), 16, (8, 15, 24, 210), (241, 237, 230, 22), 1)
    glow(c, DX0 + 270, DY0 + 400, 230, 60, accent, 0.45)
    beam(c, DX0 + 270, DY0 + 20, DY0 + 410, 360, accent, 0.16)
    paste(c, emblem(emb, 300, accent, 0.10), DX0 + 120, DY0 + 60)
    shadow_ellipse(c, DX0 + 270, DY0 + 404, 110, alpha=0.5)
    sprite(c, job_art(*combat, combat=True), DX0 + 270, DY0 + 410, 4)
    tx = DX0 + 560
    paste(c, emblem(emb, 56, accent), tx, DY0 + 44)
    c.text((tx + 72, DY0 + 72), name, 36, IVORY, False, 'lm')
    para(c, (tx, DY0 + 128), '“' + motto + '”', 18, 470, (214, 210, 200))
    heading(c, tx, DY0 + 236, 'Lv.20 두 갈래 전직')
    ax = tx
    for a in adv:
        ax = tag(c, ax, DY0 + 276, a, IVORY, 15, 18) + 10
    c.d.line(c.s(DX0 + 30, DY0 + 452, DX1 - 30, DY0 + 452), fill=(241, 237, 230, 30), width=SS)
    heading(c, DX0 + 34, DY0 + 486, '시작 스킬 3종')
    sy = DY0 + 510
    for sid in skills:
        s = SKILLS[str(sid)]
        nx = tag(c, DX0 + 34, sy + 14, s['SkillType'], accent if s['SkillType'] == '액티브' else (GOLD_HI if s['SkillType'] == '시너지' else MUTED), 12.5)
        c.text((nx + 10, sy + 14), s['Name_KR'], 19, IVORY, False, 'lm', shadow=False)
        para(c, (DX0 + 34, sy + 32), s['Description'], 14.5, DX1 - DX0 - 80, MUTED, 1.5, True, 2)
        sy += 92
    pill(c, (DX1 - 380, DY1 - 72, DX1 - 30, DY1 - 20), name + '로 정하기', True, 'F', 19)
    c.text((DX0 + 34, DY1 - 46), '정한 뒤에는 바꿀 수 없습니다', 14, MUTED, True, 'lm', shadow=False)
    return c.done()


# ---------------------------------------------------------------- starter Naia
def naia_a(plate):
    """A. the six spirits gather in front of you, the chosen one comes forward."""
    c = Canvas(blurred(plate))
    veil(c, 0.64, 0.9)
    title_block(c, '시작 나이아 · 엘라던의 단상', '함께 걸을 나이아를 고르세요')
    infos = [naia_info(n) for n in STARTERS]
    arts = [naia_art(n) for n in STARTERS]
    widths = [(a.getbbox()[2] - a.getbbox()[0]) * (4 if i == NAIA_SEL else 3) for i, a in enumerate(arts)]
    total = sum(widths) + 56 * 5
    x = 960 - total / 2
    feet = 590
    for i, (info, art) in enumerate(zip(infos, arts)):
        on = i == NAIA_SEL
        cx = x + widths[i] / 2
        if on:
            beam(c, cx, 170, feet + 8, 340, info['colour'], 0.18)
            glow(c, cx, feet + 4, 170, 36, info['colour'], 0.55)
        shadow_ellipse(c, cx, feet + 2, widths[i] * 0.36, alpha=0.5)
        sprite(c, art if on else dimmed(art, 0.6), cx, feet + 6, 4 if on else 3)
        paste(c, icon('T_Element_' + info['tail'], 40 if on else 30), cx - (20 if on else 15), feet + 22)
        c.text((cx, feet + (86 if on else 74)), info['name'], 22 if on else 17, GOLD_HI if on else (150, 160, 170), not on, 'mm', shadow=on)
        x += widths[i] + 56
    info = infos[NAIA_SEL]
    X0, Y0, X1, Y1 = 150, 728, 1770, 1012
    c.glass((X0, Y0, X1, Y1), r=20, alpha=0.88, line=0.16, sheen=False)
    rr(c, (X0 + 1, Y0 + 22, X0 + 5, Y1 - 22), 2, info['colour'] + (255,))
    rr(c, (X0 + 30, Y0 + 30, X0 + 126, Y0 + 126), 16, (6, 12, 20, 255), info['colour'] + (160,), 1.2)
    paste(c, icon('T_Element_' + info['tail'], 84), X0 + 36, Y0 + 36)
    c.text((X0 + 148, Y0 + 58), info['name'], 32, IVORY, False, 'lm')
    tag(c, X0 + 150, Y0 + 104, info['element'] + ' 속성', info['colour'], 14)
    heading(c, X0 + 34, Y0 + 160, '타고난 힘')
    c.text((X0 + 34, Y0 + 192), info['passive'][0], 19, IVORY, False, 'lm', shadow=False)
    para(c, (X0 + 34, Y0 + 212), info['passive'][1], 15, 500, MUTED, 1.5, True, 2)
    c.d.line(c.s(X0 + 580, Y0 + 28, X0 + 580, Y1 - 28), fill=(241, 237, 230, 34), width=SS)
    heading(c, X0 + 610, Y0 + 40, '처음 능력치 · 여섯 나이아 중')
    mx = {k: max(i['stats'][k] for i in infos) for k, _ in STAT_KEYS}
    for k, (key, label) in enumerate(STAT_KEYS):
        col, row = divmod(k, 3)
        sx, sy = X0 + 610 + col * 300, Y0 + 86 + row * 62
        c.text((sx, sy), label, 15, MUTED, True, 'lm', shadow=False)
        c.text((sx + 260, sy), '%d' % info['stats'][key], 16, IVORY, False, 'rm', shadow=False)
        best = info['stats'][key] >= mx[key]
        bar(c, sx, sy + 24, 260, info['stats'][key] / mx[key], GOLD_HI if best else info['colour'])
    c.d.line(c.s(X0 + 1240, Y0 + 28, X0 + 1240, Y1 - 28), fill=(241, 237, 230, 34), width=SS)
    heading(c, X0 + 1268, Y0 + 40, '진화')
    sil = silhouette(naia_art(STARTERS[NAIA_SEL]))
    rr(c, (X0 + 1268, Y0 + 62, X0 + 1388, Y0 + 162), 14, (6, 12, 20, 255), (241, 237, 230, 40), 1)
    sprite(c, sil, X0 + 1328, Y0 + 150, 2)
    c.text((X0 + 1328, Y0 + 108), '?', 30, GOLD_HI, False, 'mm', shadow=False)
    c.text((X0 + 1404, Y0 + 94), 'Lv.%s 에' % info['evo'][1], 15, MUTED, True, 'lm', shadow=False)
    c.text((X0 + 1404, Y0 + 126), info['evo'][0], 21, IVORY, False, 'lm', shadow=False)
    pill(c, (X0 + 1268, Y1 - 82, X1 - 30, Y1 - 28), info['name'] + '와 함께하기', True, 'F', 19)
    hint_bar(c, 1046, [('←', ''), ('→', '고르기'), ('F', '함께하기'), ('', '정한 뒤에는 바꿀 수 없습니다')])
    return c.done()


def naia_b(plate):
    """B. six cards, element-tinted, in one window."""
    c = Canvas(blurred(plate))
    veil(c, 0.6, 0.82)
    infos = [naia_info(n) for n in STARTERS]
    X0, Y0, X1, Y1 = 250, 120, 1670, 960
    c.glass((X0, Y0, X1, Y1), r=22, alpha=0.9, line=0.16, sheen=False)
    c.text((X0 + 44, Y0 + 50), '시작 나이아 · 엘라던', 15, GOLD_HI, True, 'lm', shadow=False)
    c.text((X0 + 44, Y0 + 92), '함께 걸을 나이아를 고르세요', 32, IVORY, False, 'lm')
    cw, ch, gap = 206, 330, 16
    x = 960 - (6 * cw + 5 * gap) / 2
    for i, info in enumerate(infos):
        on = i == NAIA_SEL
        y0 = Y0 + 150 - (12 if on else 0)
        rr(c, (x, y0, x + cw, y0 + ch), 16, (10, 19, 30, 245), GOLD + (230,) if on else (241, 237, 230, 34), 2 if on else 1)
        wa = np.zeros((int(220 * SS), int(cw * SS), 4), np.uint8)
        wa[..., 0], wa[..., 1], wa[..., 2] = info['colour']
        wa[..., 3] = (np.linspace(1, 0, wa.shape[0])[:, None] ** 1.2 * (110 if on else 55)).astype(np.uint8)
        c.layer.alpha_composite(Image.fromarray(wa), (int(x * SS), int(y0 * SS)))
        paste(c, icon('T_Element_' + info['tail'], 34), x + 12, y0 + 12)
        art = naia_art(info['id'])
        shadow_ellipse(c, x + cw / 2, y0 + 218, 70, alpha=0.45)
        sprite(c, art if on else dimmed(art, 0.85, 1.0), x + cw / 2, y0 + 222, 3)
        c.text((x + cw / 2, y0 + 260), info['name'], 22, GOLD_HI if on else IVORY, False, 'mm', shadow=False)
        c.text((x + cw / 2, y0 + 294), info['element'] + ' 속성', 15, info['colour'], True, 'mm', shadow=False)
        x += cw + gap
    info = infos[NAIA_SEL]
    dy = Y0 + 520
    c.d.line(c.s(X0 + 40, dy, X1 - 40, dy), fill=(241, 237, 230, 30), width=SS)
    heading(c, X0 + 48, dy + 36, '타고난 힘 · ' + info['passive'][0])
    para(c, (X0 + 48, dy + 60), info['passive'][1], 16, 560, TEXT, 1.5, True, 2)
    heading(c, X0 + 48, dy + 132, '진화  Lv.%s → %s' % (info['evo'][1], info['evo'][0]))
    mx = {k: max(i['stats'][k] for i in infos) for k, _ in STAT_KEYS}
    for k, (key, label) in enumerate(STAT_KEYS):
        col, row = divmod(k, 3)
        sx, sy = X0 + 700 + col * 330, dy + 36 + row * 50
        c.text((sx, sy), label, 15, MUTED, True, 'lm', shadow=False)
        c.text((sx + 290, sy), '%d' % info['stats'][key], 16, IVORY, False, 'rm', shadow=False)
        bar(c, sx, sy + 20, 290, info['stats'][key] / mx[key], GOLD_HI if info['stats'][key] >= mx[key] else info['colour'], 7)
    pill(c, (X1 - 420, Y1 - 92, X1 - 48, Y1 - 38), info['name'] + '와 함께하기', True, 'F', 19)
    return c.done()


def naia_c(plate):
    """C. one at a time: the chosen spirit large in the middle, its neighbours either side."""
    c = Canvas(blurred(plate))
    veil(c, 0.62, 0.88)
    title_block(c, '시작 나이아 · 엘라던의 단상', '함께 걸을 나이아를 고르세요', 58)
    infos = [naia_info(n) for n in STARTERS]
    info = infos[NAIA_SEL]
    cx, feet = 700, 690
    glow(c, cx, feet, 260, 58, info['colour'], 0.6)
    beam(c, cx, 170, feet + 10, 460, info['colour'], 0.2)
    for d, k in ((-1, 0.42), (1, 0.42)):
        o = infos[(NAIA_SEL + d) % 6]
        sprite(c, dimmed(naia_art(o['id']), k, 0.8), cx + d * 330, feet - 40, 2)
        c.text((cx + d * 330, feet + 2), o['name'], 16, (130, 140, 150), True, 'mm', shadow=False)
    shadow_ellipse(c, cx, feet, 130, alpha=0.5)
    sprite(c, naia_art(info['id']), cx, feet + 8, 4)
    for d, glyph in ((-1, '‹'), (1, '›')):
        gx = cx + d * 460
        c.d.ellipse(c.s(gx - 26, feet - 186, gx + 26, feet - 134), fill=(14, 26, 40, 220), outline=(241, 237, 230, 80), width=SS)
        c.text((gx, feet - 163), glyph, 34, IVORY, False, 'mm', shadow=False)
    # page dots
    for i in range(6):
        on = i == NAIA_SEL
        c.diamond((cx - 75 + i * 30, feet + 60), 6 if on else 4, fill=infos[i]['colour'] if on else (90, 104, 116))
    X0, Y0, X1, Y1 = 1220, 200, 1780, 900
    c.glass((X0, Y0, X1, Y1), r=20, alpha=0.88, line=0.16, sheen=False)
    paste(c, icon('T_Element_' + info['tail'], 64), X0 + 30, Y0 + 30)
    c.text((X0 + 110, Y0 + 56), info['name'], 32, IVORY, False, 'lm')
    tag(c, X0 + 112, Y0 + 98, info['element'] + ' 속성', info['colour'], 14)
    heading(c, X0 + 34, Y0 + 158, '타고난 힘')
    c.text((X0 + 34, Y0 + 192), info['passive'][0], 19, IVORY, False, 'lm', shadow=False)
    para(c, (X0 + 34, Y0 + 214), info['passive'][1], 15, X1 - X0 - 70, MUTED, 1.5, True, 2)
    heading(c, X0 + 34, Y0 + 300, '처음 능력치')
    mx = {k: max(i['stats'][k] for i in infos) for k, _ in STAT_KEYS}
    for k, (key, label) in enumerate(STAT_KEYS):
        sy = Y0 + 336 + k * 40
        c.text((X0 + 34, sy), label, 15, MUTED, True, 'lm', shadow=False)
        bar(c, X0 + 140, sy, X1 - X0 - 240, info['stats'][key] / mx[key], GOLD_HI if info['stats'][key] >= mx[key] else info['colour'], 7)
        c.text((X1 - 34, sy), '%d' % info['stats'][key], 16, IVORY, False, 'rm', shadow=False)
    heading(c, X0 + 34, Y0 + 600, '진화  Lv.%s → %s' % (info['evo'][1], info['evo'][0]))
    pill(c, (X0 + 30, Y1 - 76, X1 - 30, Y1 - 24), info['name'] + '와 함께하기', True, 'F', 19)
    hint_bar(c, 1036, [('Q', ''), ('E', '넘기기'), ('F', '함께하기'), ('', '정한 뒤에는 바꿀 수 없습니다')])
    return c.done()


# ---------------------------------------------------------------- quest offer
QUEST_ID = '10004'
# the giver's last offer line (Scripts/Data/NPCDialogue.csv, Quest_10004_NPC4005_Offer #2)
OFFER_LINE = '…저 게시판 봐라. 거기 적힌 건 전부 사람 목숨이 걸린 일은 아니지만, 전부 누군가의 하루가 걸린 일이야.'
GIVER_FLIPBOOK = 'FB_NPC_4005_Idle'


def giver_bust(size):
    import json
    rows = json.load(open(ROOT / 'Saved' / 'HUDRenewal' / 'npc_art' / 'npc_art.json', encoding='utf-8'))
    r = next(x for x in rows if x['flipbook'].endswith(GIVER_FLIPBOOK))
    im = Image.open(r['png']).convert('RGBA')
    b = im.getbbox()
    crop = im.crop((b[0] + (b[2] - b[0]) // 2 - 20, b[1], b[0] + (b[2] - b[0]) // 2 + 20, b[1] + 40))
    return crop.resize((size, size), Image.NEAREST)


def quest_data():
    q = QUESTS[QUEST_ID]
    item = ITEM_ROWS[q['RewardItem1_ID']]
    return dict(title=q['Quest_KR'], region=q['Region_KR'], desc=q['Desc_KR'], exp=int(q['Reward_EXP']), gold=int(q['Reward_Gold']),
                item=(item['Name'], item['DisplayName'], item['Description'], int(q['RewardItem1Count'])), objective='루간과 대화하기')


def reward_slot(c, x, y, im, count, label, size=80, focus=False):
    rr(c, (x, y, x + size, y + size), 14, (6, 12, 20, 255), GOLD + (220,) if focus else (241, 237, 230, 40), 1.6 if focus else 1)
    glow(c, x + size / 2, y + size / 2, size * 0.42, size * 0.42, (241, 237, 230), 0.07)
    paste(c, im, x + (size - im.width) / 2, y + (size - im.height) / 2 - 2)
    if count is not None:
        s = '{:,}'.format(count)
        f = font(13, False)
        w = c.d.textlength(s, font=f) / SS + 12
        rr(c, (x + size - w - 4, y + size - 22, x + size - 4, y + size - 4), 8, (14, 26, 40, 245), (241, 237, 230, 60), 1)
        c.text((x + size - 4 - w / 2, y + size - 12.5), s, 13, IVORY, False, 'mm', shadow=False)
    c.text((x + size / 2, y + size + 18), label, 14, TEXT, True, 'mm', shadow=False)


def offer_card(c, X0, Y0, W, data, tooltip=True):
    X1 = X0 + W
    body_h = 560
    Y1 = Y0 + body_h
    frost(c, (X0, Y0, X1, Y1), 22)
    c.glass((X0, Y0, X1, Y1), r=22, alpha=0.86, line=0.16, sheen=False)
    med = bd.medallion(64, giver_bust(64))
    c.layer.alpha_composite(med, (int((X0 + 30) * SS), int((Y0 + 28) * SS)))
    c.text((X0 + 110, Y0 + 46), '새 의뢰 · 루간', 14, GOLD_HI, True, 'lm', shadow=False)
    c.text((X0 + 110, Y0 + 80), data['title'], 28, IVORY, False, 'lm')
    c.diamond((X0 + 34, Y0 + 122), 4)
    c.text((X0 + 46, Y0 + 122), data['region'], 14.5, MUTED, True, 'lm', shadow=False)
    c.d.line(c.s(X0 + 30, Y0 + 146, X1 - 30, Y0 + 146), fill=(241, 237, 230, 30), width=SS)
    h = para(c, (X0 + 30, Y0 + 162), data['desc'], 16, W - 60, TEXT, 1.6)
    y = Y0 + 172 + h
    heading(c, X0 + 30, y + 10, '목표')
    c.d.polygon(c.s(X0 + 36, y + 36, X0 + 42, y + 42, X0 + 36, y + 48, X0 + 30, y + 42), outline=GOLD_HI + (255,), width=int(1.4 * SS))
    c.text((X0 + 52, y + 42), data['objective'], 16, IVORY, True, 'lm', shadow=False)
    y += 80
    heading(c, X0 + 30, y, '보상')
    sx = X0 + 30
    slots = [(icon('T_Reward_Exp', 64), data['exp'], '경험치', False), (icon('T_Reward_Gold', 64), data['gold'], '골드', False),
             (icon('T_Item_' + data['item'][0], 64), data['item'][3], data['item'][1], tooltip)]
    for im, n, label, focus in slots:
        reward_slot(c, sx, y + 22, im, n, label, 80, focus)
        sx += 100
    if tooltip:
        tx, ty = sx + 6, y + 18
        tw = X1 - 26 - tx
        rr(c, (tx, ty, tx + tw, ty + 112), 12, (22, 41, 61, 250), GOLD + (150,), 1)
        c.d.polygon(c.s(tx, ty + 56, tx - 8, ty + 62, tx, ty + 68), fill=(22, 41, 61, 250))
        c.text((tx + 14, ty + 22), data['item'][1], 16, IVORY, False, 'lm', shadow=False)
        tag(c, tx + 14, ty + 50, '중요 물품', GOLD_HI, 12)
        para(c, (tx + 14, ty + 64), data['item'][2], 13, tw - 28, MUTED, 1.5, True, 2)
    by = Y1 - 82
    c.d.line(c.s(X0 + 30, by - 18, X1 - 30, by - 18), fill=(241, 237, 230, 30), width=SS)
    half = (W - 72) / 2
    pill(c, (X0 + 30, by, X0 + 30 + half, by + 54), '거절', False, None, 19)
    pill(c, (X0 + 42 + half, by, X1 - 30, by + 54), '수락', True, 'F', 19)
    return Y1


def offer_a(plate):
    """A. the offer card beside the conversation, picture icons in reward slots, the giver's face; field HUD folded."""
    c = Canvas(plate)
    data = quest_data()
    offer_card(c, 1130, 150, 640, data, tooltip=True)
    return c.done()


def offer_b(plate):
    """B. the offer inside the subtitle dialogue: the giver's line stays, the request sheet replaces the choices."""
    c = Canvas(plate)
    data = quest_data()
    W, H = c.W, c.H
    sc = Image.new('RGBA', (W * SS, H * SS), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sc)
    for y in range(740, H):
        t = (y - 740) / (H - 740)
        a = t * t * (3 - 2 * t)
        sd.line([(0, y * SS), (W * SS, y * SS)], fill=(9, 17, 27, int(255 * min(.9, 0.06 + a * 0.95))), width=SS)
    c.layer = Image.alpha_composite(sc, c.layer)
    c.d = ImageDraw.Draw(c.layer)
    c.diamond((331, 869), 5.5)
    c.text((346, 869), '루간', 26, GOLD_HI, anchor='lm')
    c.d.line(c.s(341, 892, 471, 892), fill=GOLD + (170,), width=int(2 * SS))
    para(c, (341, 914), OFFER_LINE, 24, 760, IVORY, 1.5, True, 3, shadow=True)
    # the request sheet where the choices were
    PX0, PX1 = 1150, 1700
    PY0, PY1 = 470, 822
    frost(c, (PX0, PY0, PX1, PY1), 18)
    c.glass((PX0, PY0, PX1, PY1), r=18, alpha=0.86, line=0.16, sheen=False)
    c.text((PX0 + 26, PY0 + 34), '새 의뢰', 13.5, GOLD_HI, True, 'lm', shadow=False)
    c.text((PX0 + 26, PY0 + 70), data['title'], 24, IVORY, False, 'lm')
    c.d.polygon(c.s(PX0 + 32, PY0 + 104, PX0 + 38, PY0 + 110, PX0 + 32, PY0 + 116, PX0 + 26, PY0 + 110), outline=GOLD_HI + (255,), width=int(1.4 * SS))
    c.text((PX0 + 48, PY0 + 110), data['objective'], 15.5, TEXT, True, 'lm', shadow=False)
    sx = PX0 + 26
    for im, n, label in ((icon('T_Reward_Exp', 54), data['exp'], '경험치'), (icon('T_Reward_Gold', 54), data['gold'], '골드'),
                         (icon('T_Item_' + data['item'][0], 54), data['item'][3], data['item'][1])):
        reward_slot(c, sx, PY0 + 138, im, n, label, 66)
        sx += 86
    for i, (label, key, gold) in enumerate((('수락', 'F', True), ('거절', None, False))):
        y = PY0 + 252 + i * 0
        bx0 = PX0 + 26 + i * 256
        pill(c, (bx0, PY1 - 70, bx0 + 240, PY1 - 22), label, gold, key, 17)
    return c.done()


def icon_sheet():
    W, H = 1500, 460
    base = Image.new('RGBA', (W, H), (11, 18, 25, 255))
    c = Canvas(base)
    c.text((40, 44), '새로 그린 그림 아이콘 — 아이템 아이콘과 같은 톤 (48칸 도트, 144px)', 20, IVORY, False, 'lm', shadow=False)
    names = [('T_Reward_Exp', '경험치'), ('T_Reward_Gold', '골드'), ('T_Element_Fire', '불'), ('T_Element_Water', '물'),
             ('T_Element_Grass', '풀'), ('T_Element_Earth', '땅'), ('T_Element_Lightning', '번개'), ('T_Element_Wind', '바람')]
    x = 40
    for n, label in names:
        rr(c, (x, 84, x + 156, 240), 16, (6, 12, 20, 255), (241, 237, 230, 40), 1)
        paste(c, icon(n, 144), x + 6, 90)
        c.text((x + 78, 262), label, 16, TEXT, True, 'mm', shadow=False)
        x += 176
    c.text((40, 320), '보상 칸 — 그림 + 수량 배지 + 이름 (지금은 반짝임 · 마름모 · 가방 기호)', 17, MUTED, True, 'lm', shadow=False)
    x = 40
    data = quest_data()
    for im, n, label in ((icon('T_Reward_Exp', 64), data['exp'], '경험치'), (icon('T_Reward_Gold', 64), data['gold'], '골드'),
                         (icon('T_Item_' + data['item'][0], 64), data['item'][3], data['item'][1]),
                         (icon('T_Item_C_VIT_01', 64), 3, '하급 체력 포션')):
        reward_slot(c, x, 346, im, n, label, 80)
        x += 120
    return c.done()


def uri(im):
    buf = io.BytesIO()
    im.convert('RGB').save(buf, 'JPEG', quality=88)
    return 'data:image/jpeg;base64,' + base64.b64encode(buf.getvalue()).decode()


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'png').mkdir(exist_ok=True)
    plate = Image.open(CAP / 'sel_plate_view.png').convert('RGBA')
    shots = {
        'now_job': Image.open(CAP / 'sel_job_view.png').convert('RGBA'),
        'now_naia': Image.open(CAP / 'sel_naia_view.png').convert('RGBA'),
        'now_offer': Image.open(CAP / 'sel_offer_view.png').convert('RGBA'),
        'job_a': job_a(plate), 'job_b': job_b(plate), 'job_c': job_c(plate),
        'naia_a': naia_a(plate), 'naia_b': naia_b(plate), 'naia_c': naia_c(plate),
        'offer_a': offer_a(plate), 'offer_b': offer_b(plate),
        'icons': icon_sheet(),
    }
    for k, im in shots.items():
        im.convert('RGB').save(OUT / 'png' / (k + '.png'))
    html = PAGE
    for k, im in shots.items():
        small = im if im.width <= 1500 else im.resize((1280, 720), Image.LANCZOS)
        html = html.replace('%%%s%%' % k.upper(), uri(small))
    (OUT / 'selection.html').write_text(html, encoding='utf-8')
    print('wrote', OUT / 'selection.html')


PAGE = '''<!doctype html><html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>직업·나이아·의뢰 시안</title><style>
:root{--bg:#0b1219;--panel:#111c27;--text:#e8e4dc;--sub:#9aa7b1;--gold:#d6be8c}
@media (prefers-color-scheme: light){:root:not([data-theme="dark"]){--bg:#eef0f2;--panel:#fff;--text:#1d2731;--sub:#5d6b77}}
:root[data-theme="light"]{--bg:#eef0f2;--panel:#fff;--text:#1d2731;--sub:#5d6b77}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.65 "Pretendard","Malgun Gothic",sans-serif}
main{max-width:1320px;margin:0 auto;padding:40px 24px 80px}h1{font-size:28px;margin:0 0 6px}h2{font-size:22px;margin:52px 0 8px}
h2 small{font-size:14px;color:var(--sub);font-weight:400;margin-left:8px}
.lead{color:var(--sub);max-width:920px;margin:0 0 14px}.card{background:var(--panel);border-radius:14px;padding:16px;border:1px solid rgba(128,140,150,.18);margin-bottom:18px}
.card h3{margin:0 0 8px;font-size:16px}.card p,.card li{color:var(--sub);font-size:14px}.card p{margin:8px 0 0}img{width:100%;display:block;border-radius:10px}
.tag{font-size:12px;font-weight:700;padding:1px 8px;border-radius:99px;background:#2f4a3a;color:#bfe8c8;margin-left:6px}
.pick{outline:2px solid var(--gold);outline-offset:3px}ul{margin:6px 0 0;padding-left:18px}.cap{font-size:13px;color:var(--sub);margin-top:6px}
nav{display:flex;gap:10px;flex-wrap:wrap;margin:10px 0 0}nav a{color:var(--gold);text-decoration:none;font-size:14px;border:1px solid rgba(214,190,140,.4);padding:3px 12px;border-radius:99px}
</style></head><body><main>
<h1>직업 선택 · 시작 나이아 선택 · 의뢰 팝업 시안</h1>
<p class="lead">세 화면 모두 리베르그로브 엘라던 곁에서 실제 게임으로 띄운 "지금" 캡처에서 출발했습니다. 시안은 같은 장면 1080p 위에 게임 크기로 그렸고,
캐릭터·나이아·NPC 얼굴은 게임 스프라이트 원본(정수 배율), 글은 데이터 표(직업·전직·직업 스킬·나이아·패시브·퀘스트·아이템) 그대로입니다.
경험치·골드·속성 아이콘 8종은 이번에 아이템 아이콘과 같은 톤으로 새로 그렸습니다. 게임 에셋·코드는 바꾸지 않았습니다.</p>
<nav><a href="#job">직업 선택</a><a href="#naia">시작 나이아</a><a href="#offer">의뢰 팝업</a><a href="#icons">새 아이콘</a></nav>

<h2 id="job">1. 직업 선택 <small>엘라던 → "단상에 오르는 자" 퀘스트 뒤, 처음 한 번</small></h2>
<div class="card"><img src="%NOW_JOB%"><div class="cap">지금 — zxcv1 WBP_JobSelection, 카드는 C++가 회색 버튼으로 만듦 (트래커를 고른 상태)</div>
<ul>
<li>카드에 이름 한 줄뿐 — 그림·문장·스킬이 없어 무엇을 고르는지 알 수 없음. 고르면 카드 색만 탁한 노랑으로</li>
<li>설명은 직업 표의 한 줄 문장만, 시작 스킬 3종·Lv.20 전직 갈래는 안 보임 (확인 창에서 "스킬 3종을 함께 획득"이라고만)</li>
<li>옛 남색 장식 테 + 엔진 기본 고딕 — 새 HUD·대화창·생성 화면 톤과 다름. 뒤 HUD가 그대로 비침</li>
<li>제목 "(직업)" 괄호, 아래 절반이 빈 공간, 키 안내 없음</li></ul></div>

<div class="card pick"><h3>A. 다섯 갈래 무대 — 내 캐릭터가 각 직업 차림으로 선다<span class="tag">추천</span></h3><img src="%JOB_A%">
<p>장면을 어둡게 가라앉히고 다섯 직업 차림의 내 캐릭터를 나란히 세웁니다(필드 스프라이트 3배). 고른 직업은 전투 대기 자세로 바뀌며 빛 기둥 안에 서고,
나머지는 어둡게 물러납니다. 아래 유리 판에 문장(직업 표의 1인칭 대사)·시작 스킬 3종(종류 표시 + 설명 한 줄)·Lv.20 두 갈래 전직을 한눈에.
캐릭터 생성 화면의 "무대 + 유리 시트"와 같은 말투라 처음 고르는 순간의 무게가 삽니다. ← → 로 넘기고 F 로 정합니다.</p></div>
<div class="card"><h3>B. 직업 카드 다섯 장</h3><img src="%JOB_B%">
<p>지금 구조(카드 다섯 장 + 아래 설명)를 그대로 두고 채웁니다. 카드마다 직업 색 노을·문장 워터마크·차림·이름·문장. 고른 카드는 떠오르며 전투 자세.
지금 C++ 카드 생성을 WBP 카드 클래스로 바꾸는 정도라 손이 가장 적게 갑니다.</p></div>
<div class="card"><h3>C. 목록 + 큰 상세 (옥토패스식)</h3><img src="%JOB_C%">
<p>왼쪽 목록(얼굴·문장·한 줄), 오른쪽에 전투 자세 4배와 스킬 설명 전문. 정보는 가장 많지만 "처음 한 번 고르는 의식"보다는 메뉴 느낌입니다.</p></div>

<h2 id="naia">2. 시작 나이아 선택 <small>직업을 정한 바로 다음</small></h2>
<div class="card"><img src="%NOW_NAIA%"><div class="cap">지금 — 네이티브 NUStarterNaiaSelectionWidget (WBP 미지정, C++가 트리를 만듦)</div>
<ul>
<li>그림은 들어가 있지만 카드가 납작한 회색, 속성은 색 글자 한 단어</li>
<li>설명 칸이 비어 "테이블에 등록된 시작 나이아만 선택할 수 있습니다" 같은 개발 문구가 보임 (나이아 표에 설명 칸이 없음)</li>
<li>능력치·타고난 패시브(불꽃의 의지 등)·진화(Lv.22 블레이코 등) — 고를 때 알고 싶은 것이 하나도 없음</li>
<li>직업 창과 모양이 따로 놀고, 초록 "선택 확정" 버튼만 다른 팔레트</li></ul></div>
<div class="card pick"><h3>A. 여섯 정령이 다가온다 — 직업 A와 같은 무대<span class="tag">추천</span></h3><img src="%NAIA_A%">
<p>직업 A와 같은 무대 말투로 이어집니다. 고른 나이아는 3배로 앞에 나와 속성 색 빛을 받고, 나머지는 2배로 물러납니다. 이름 위에 새로 그린 속성 그림 아이콘.
아래 판: 속성 · 타고난 힘(패시브 표 이름·설명) · 처음 능력치 6종(여섯 중 가장 높은 값은 금색) · 진화(실루엣 + 레벨·이름).</p></div>
<div class="card"><h3>B. 속성 카드 여섯 장</h3><img src="%NAIA_B%">
<p>지금 카드 구조를 유지하고 속성 색 노을·속성 아이콘·능력치 막대를 더한 안. 직업 B와 짝.</p></div>
<div class="card"><h3>C. 한 마리씩 넘기기</h3><img src="%NAIA_C%">
<p>가운데에 4배로 한 마리, 양옆에 이웃. 한 마리에 집중시키는 맛은 가장 좋지만 여섯을 한눈에 비교하기 어렵습니다.</p></div>

<h2 id="offer">3. 의뢰 팝업 <small>NPC 퀘스트 선택지 → "새 의뢰"</small></h2>
<div class="card"><img src="%NOW_OFFER%"><div class="cap">지금 — WBP_HUD_Quest 의 OfferPanel (퀘스트 10004, 아이템 보상이 있는 의뢰)</div>
<ul>
<li>보상 아이콘이 기호(반짝임 · 마름모 · 가방) — 아이템 보상도 실제 아이템 그림이 아니라 같은 가방 기호</li>
<li>화면 한가운데에 떠서 말을 건 NPC와 내 캐릭터를 가림, 필드 HUD·퀘스트 추적 창이 그대로 남음</li>
<li>누가 준 의뢰인지 없음, 아이템 보상이 무엇인지(설명) 알 수 없음, 수락 키 안내 없음</li>
<li>설명이 빈 의뢰는 위치 줄 아래가 휑하게 비어 보임 (사용자 캡처 "시안의 첫 거래")</li></ul></div>
<div class="card pick"><h3>A. 대화 옆 의뢰서 + 그림 보상 칸<span class="tag">추천</span></h3><img src="%OFFER_A%">
<p>카드를 오른쪽으로 옮겨 NPC와 캐릭터를 비우고, 대화창처럼 필드 HUD를 접습니다. 맨 위에 의뢰인 얼굴(NPC 도트)과 이름.
보상은 그림 칸: 경험치 결정 · 골드 동전 · 실제 아이템 아이콘 + 수량 배지 + 이름. 아이템 칸에 커서를 두면 아이템 설명 말풍선. 수락 F.</p></div>
<div class="card"><h3>B. 대화 안의 의뢰서</h3><img src="%OFFER_B%">
<p>자막형 대화를 그대로 두고, 선택지 자리에 의뢰서(제목·목표·보상 칸)와 수락/다음에 버튼. 흐름은 가장 매끄럽지만 설명이 긴 의뢰에는 좁습니다.</p></div>

<h2 id="icons">4. 새 그림 아이콘</h2>
<div class="card"><img src="%ICONS%">
<p>GPT로 아이템 아이콘과 같은 규칙(캐릭터 도트 기준 그림, 1px 검은 외곽선, 48칸 격자, 3배 = 144px)으로 그렸습니다.
원본 ArtSource/UI/HUD2026/reward_icons (프롬프트·원본·정리 스크립트 함께). 속성 6종은 나이아 창뿐 아니라 도감·전투 상태·지도 표시에도 같이 쓸 수 있습니다.</p></div>

<h2>어느 안이든 같이</h2>
<div class="card"><ul>
<li>세 창 모두 열려 있는 동안 필드 HUD 접기 (대화창·상점과 같은 bHideFieldHUD)</li>
<li>직업·나이아 창: 패드/키보드 ← → 고르기, F 정하기, 확인 창은 생성 화면 글래스 확인 창(WBP_CCG_ConfirmDialog)과 같은 모양으로</li>
<li>직업 색은 그림 차림에서 뽑은 새 값 제안 (지금 코드 GetJobAccent 의 주황·노랑·파랑·보라·초록은 차림과 안 맞음)</li>
<li>남자 엘리멘탈리스트·여자 나이아 나이트 그림이 아직 없음 — 시안 A·B 의 엘리멘탈리스트는 여자 그림을 씀. 성별별 그림이 다 들어오면 고른 캐릭터 성별·머리로 바로 그림</li>
<li>나이아 표에 설명 칸이 없음 — 짧은 소개 문장을 넣을지는 기획 결정 (시안은 패시브·능력치·진화로 대신)</li>
<li>보상 아이콘은 퀘스트 의뢰·추적 완료 띠·보상 창·전투 결과에서 같이 쓰도록 HUD 공용 텍스처로</li>
</ul></div>
</main></body></html>'''

if __name__ == '__main__':
    build()
