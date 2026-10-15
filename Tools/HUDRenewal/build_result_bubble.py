# -*- coding: utf-8 -*-
"""Battle result window and chat speech bubble proposals (proposal only, no game assets touched).

Output: Docs/HUD_ResultBubble_Proposal_20261015/result_bubble.html (+ png/).
"Now" frames are real game captures:
  - result: Fernbrook local battle, the real dongh WBP_BattleResult filled with sample data by qa/qa_result_now.py
    (there is no win cheat and ShowResult isn't callable from Python, so the bound widgets are filled the way ShowResult does)
  - bubbles: Libregrove, the real dbals WBP_SpeechBubble / _Party through nu.Bubble.Say / SayParty (qa/states_bubble_now.json)
Mocks are drawn at 1080p over frames captured without screen UI (res_plate: enemies hidden, bub_plate: village with two fake players).
Character / Naia art is the game's sprite sheets at whole multiples, item icons ArtSource/ItemIcons_HD2D, reward / element icons
ArtSource/UI/HUD2026/reward_icons. Names and numbers are the same sample as the "now" capture.
Plates: Tools/CharacterCreateGlass/run.ps1 -Steps shot -States Tools/HUDRenewal/qa/states_result_now.json -Map /Game/Fernbrook/Entrance_V1/Maps/L_Fernbrook_Entrance
        Tools/CharacterCreateGlass/run.ps1 -Steps shot -States Tools/HUDRenewal/qa/states_bubble_now.json -Map /Game/Libregrove/Level_V8/Maps/L_Libregrove
Run: python Tools/HUDRenewal/build_result_bubble.py
"""
import importlib.util
import pathlib

import numpy as np
from PIL import Image, ImageDraw

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('build_selection', HERE / 'build_selection.py')
sel = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sel)
bd, bs = sel.bd, sel.bs
Canvas, font, SS = bd.Canvas, bd.font, bd.SS
NAVY, IVORY, GOLD, GOLD_HI, MUTED = bd.NAVY, bd.IVORY, bd.GOLD, bd.GOLD_HI, bd.MUTED
TEXT = sel.TEXT
rr, pix = bs.rr, bs.pix
glow, paste, icon, frost, tag, heading, para, fit_text, bar = sel.glow, sel.paste, sel.icon, sel.frost, sel.tag, sel.heading, sel.para, sel.fit_text, sel.bar

ROOT = HERE.parents[1]
CAP = ROOT / 'Saved' / 'CharacterCreateGlass'
OUT = ROOT / 'Docs' / 'HUD_ResultBubble_Proposal_20261015'
CHAR = ROOT / 'ArtSource' / 'Character'
SKY = (126, 190, 236)

# ---------------------------------------------------------------- sample (same as qa/qa_result_now.py)
ME = dict(name='리아', old=5, lv=6, ratio=0.18, remain=412)
PARTY = [(1004, '푸루나', 5), (1001, '볼또', 4), (1010, '아쿠퐁', 4)]
EXP, GOLD_N = 184, 62
ITEMS = [('M_HIDE_01', '가죽 재료', 3), ('C_VIT_01', '하급 체력 포션', 1), ('M_WOOD_01', '갱목 가공 재료', 2)]
CAUGHT = [(1004, '푸루나')]
PLAYER_FEET = (540, 908)        # the player avatar in res_plate (1080p)
PLAYER_HEAD = 738


def me_art():
    im = sel.sheet_frame(CHAR / 'Novice' / 'MCh_Body_Idle_Front_Sheet.png', 60)
    for layer in ('MCh_FaceEye_Idle_Front_Sheet.png', 'MCh_Hair1_Idle_Front_Sheet.png'):
        im.alpha_composite(sel.sheet_frame(CHAR / 'Novice' / layer, 60))
    return im


def face(im, size):
    """Head crop of a sprite, whole-pixel scaled into a square (portrait medallions)."""
    b = im.getbbox()
    w = b[2] - b[0]
    side = int(w * 0.62) if im.width > 64 else 30
    cx = (b[0] + b[2]) // 2
    top = b[1] + (0 if im.width > 64 else 1)
    crop = im.crop((cx - side // 2, top, cx + side // 2, top + side))
    k = max(1, size // side)
    return crop.resize((side * k, side * k), Image.NEAREST).resize((size, size), Image.NEAREST)


def naia_face(nid, size):
    return face(sel.naia_art(nid), size)


def naia_element(nid):
    return sel.naia_info(nid)['tail']


def medal(c, x, y, d, im, ring=GOLD):
    m = bd.medallion(d, im)
    if ring != GOLD:
        a = np.asarray(m).copy()
        md = ImageDraw.Draw(m)
        md.ellipse([SS, SS, d * SS - SS, d * SS - SS], outline=ring + (230,), width=2 * SS)
    c.layer.alpha_composite(m, (int(x * SS), int(y * SS)))


def scrim_right(c, start=1040, peak=0.62):
    W, H = c.W * SS, c.H * SS
    xs = np.arange(W, dtype=np.float32) / SS
    t = np.clip((xs - start) / (c.W - start), 0, 1)
    a = (t * t * (3 - 2 * t)) * peak
    arr = np.zeros((H, W, 4), np.uint8)
    arr[..., 0], arr[..., 1], arr[..., 2] = 7, 13, 22
    arr[..., 3] = (np.broadcast_to(a, (H, W)) * 255).astype(np.uint8)
    c.layer = Image.alpha_composite(c.layer, Image.fromarray(arr))
    c.d = ImageDraw.Draw(c.layer)


def victory(c, cx, y, big=64, backing=True):
    if backing:     # a soft navy pool behind the title, so it reads over sky and cliffs
        glow(c, cx, y + 54, 330, 130, (7, 13, 22), 0.62)
    c.text((cx, y), '전투 결과', 15, GOLD_HI, True, 'mm', shadow=True)
    glow(c, cx, y + 58, 210, 60, (244, 215, 143), 0.18)
    c.text((cx, y + 56), '승리', big, IVORY, False, 'mm')
    c.d.line(c.s(cx - 190, y + 108, cx - 22, y + 108), fill=GOLD + (170,), width=SS)
    c.d.line(c.s(cx + 22, y + 108, cx + 190, y + 108), fill=GOLD + (170,), width=SS)
    c.diamond((cx, y + 108), 5.5)


def levelup_chip(c, x, y, old, new, px=14):
    s = '레벨 업  %d → %d' % (old, new)
    f = font(px, True)
    w = c.d.textlength(s, font=f) / SS + 26
    glow(c, x + w / 2, y, w * 0.7, 22, (244, 215, 143), 0.25)
    rr(c, (x, y - px, x + w, y + px), px, (214, 190, 140, 255), (150, 118, 58, 255), 1.2)
    c.text((x + w / 2, y + 1), s, px, NAVY, True, 'mm', shadow=False)
    return x + w


def reward_tile(c, x, y, w, im, label, value):
    rr(c, (x, y, x + w, y + 84), 16, (6, 12, 20, 200), (241, 237, 230, 34), 1)
    paste(c, im, x + 14, y + 14)
    c.text((x + 84, y + 28), label, 14.5, MUTED, True, 'lm', shadow=False)
    c.text((x + 84, y + 58), '+{:,}'.format(value), 28, IVORY, False, 'lm', shadow=False)


def spoil_slot(c, x, y, im, count, label, size=76, new=False, naia=False):
    rr(c, (x, y, x + size, y + size), 14, (6, 12, 20, 255), GOLD + (230,) if new else (241, 237, 230, 40), 1.6 if new else 1)
    glow(c, x + size / 2, y + size / 2, size * 0.42, size * 0.42, (244, 215, 143) if new else (241, 237, 230), 0.16 if new else 0.07)
    if naia:
        b = im.getbbox()
        crop = im.crop(b)
        k = 1
        while crop.width * (k + 1) <= size - 10 and crop.height * (k + 1) <= size - 10:
            k += 1
        w, h = crop.width * k, crop.height * k
        pix(c, crop, (x + (size - w) / 2, y + size - 6 - h, x + (size + w) / 2, y + size - 6))
    else:
        paste(c, im, x + (size - im.width) / 2, y + (size - im.height) / 2 - 2)
    if count is not None:
        s = '{:,}'.format(count)
        f = font(13, False)
        w = c.d.textlength(s, font=f) / SS + 12
        rr(c, (x + size - w - 4, y + size - 22, x + size - 4, y + size - 4), 8, (14, 26, 40, 245), (241, 237, 230, 60), 1)
        c.text((x + size - 4 - w / 2, y + size - 12.5), s, 13, IVORY, False, 'mm', shadow=False)
    if new:
        tw = c.d.textlength('새 동료', font=font(11.5, True)) / SS + 14
        rr(c, (x + size / 2 - tw / 2, y - 9, x + size / 2 + tw / 2, y + 9), 9, (214, 190, 140, 255), (150, 118, 58, 255), 1)
        c.text((x + size / 2, y + 0.5), '새 동료', 11.5, NAVY, True, 'mm', shadow=False)
    c.text((x + size / 2, y + size + 16), fit_text(c, label, 13.5, size + 26), 13.5, TEXT, True, 'mm', shadow=False)


def spoils(c, x, y, size=76, gap=22):
    for row, name, n in ITEMS:
        spoil_slot(c, x, y, icon('T_Item_' + row, 60), n, name, size)
        x += size + gap
    for nid, name in CAUGHT:
        spoil_slot(c, x, y, sel.naia_art(nid), None, name, size, new=True, naia=True)
        x += size + gap
    return x


def party_rows(c, x, y, w, row_h=70, d=50):
    # me: portrait, name, level, level-up chip, gold bar + remaining
    medal(c, x, y + (row_h - d) / 2, d, face(me_art(), d))
    c.text((x + d + 16, y + 22), ME['name'], 19, IVORY, False, 'lm', shadow=False)
    c.text((x + d + 16 + c.d.textlength(ME['name'], font=font(19)) / SS + 12, y + 23), 'Lv.%d' % ME['lv'], 15, GOLD_HI, True, 'lm', shadow=False)
    levelup_chip(c, x + w - 148, y + 22, ME['old'], ME['lv'], 13)
    bx = x + d + 16
    bar(c, bx, y + 48, w - (bx - x) - 4, ME['ratio'], (232, 196, 110), 7)
    c.text((x + w - 4, y + 64), '다음 레벨까지 {:,}'.format(ME['remain']), 12.5, MUTED, True, 'rm', shadow=False)
    y += row_h + 6
    for nid, name, lv in PARTY:
        c.d.line(c.s(x, y - 3, x + w, y - 3), fill=(241, 237, 230, 18), width=SS)
        medal(c, x + 4, y + (row_h - 8 - (d - 8)) / 2, d - 8, naia_face(nid, d - 8), (170, 182, 191))
        c.text((x + d + 16, y + (row_h - 8) / 2), name, 17.5, IVORY, False, 'lm', shadow=False)
        nx = x + d + 16 + c.d.textlength(name, font=font(17.5)) / SS + 12
        c.text((nx, (y + (row_h - 8) / 2) + 1), 'Lv.%d' % lv, 14, MUTED, True, 'lm', shadow=False)
        tail = naia_element(nid)
        el_name, el_col = sel.ELEMENT[tail]
        paste(c, icon('T_Element_' + tail, 26), x + w - 30, y + (row_h - 8) / 2 - 13)
        y += row_h - 8
    return y


def hint(c, x, y, anchor='r'):
    f = font(15, True)
    w = 26 + 8 + c.d.textlength('계속', font=f) / SS
    x0 = x - w if anchor == 'r' else (x - w / 2 if anchor == 'm' else x)
    bd.keychip(c, (x0, y - 13), 'F', 26)
    c.text((x0 + 34, y), '계속', 15, IVORY, True, 'lm', shadow=False)


def player_levelup(c):
    x, y = PLAYER_FEET[0], PLAYER_HEAD - 34
    glow(c, x, PLAYER_HEAD + 60, 90, 120, (244, 215, 143), 0.16)
    s = '레벨 업!'
    c.text((x, y), s, 24, GOLD_HI, False, 'mm', shadow=True)
    for dx, dy, r in ((-62, -10, 4), (58, -18, 3.5), (-40, 18, 2.5), (66, 14, 2.5)):
        c.diamond((x + dx, y + dy), r)


# ---------------------------------------------------------------- result proposals
def result_a(plate):
    """A. victory title over the stage, the settlement sheet on the right (offer card / selection sheet family)."""
    c = Canvas(plate)
    scrim_right(c, 1000, 0.55)
    victory(c, 600, 140)
    player_levelup(c)
    X0, X1 = 1200, 1832
    Y0 = 176
    Y1 = Y0 + 760
    frost(c, (X0, Y0, X1, Y1), 22)
    c.glass((X0, Y0, X1, Y1), r=22, alpha=0.86, line=0.16, sheen=False)
    heading(c, X0 + 32, Y0 + 40, '보상')
    tw = (X1 - X0 - 64 - 16) / 2
    reward_tile(c, X0 + 32, Y0 + 62, tw, icon('T_Reward_Exp', 56), '경험치', EXP)
    reward_tile(c, X0 + 48 + tw, Y0 + 62, tw, icon('T_Reward_Gold', 56), '골드', GOLD_N)
    heading(c, X0 + 32, Y0 + 186, '파티')
    y = party_rows(c, X0 + 32, Y0 + 204, X1 - X0 - 64)
    heading(c, X0 + 32, y + 26, '획득')
    spoils(c, X0 + 32, y + 56, 76, 30)
    c.d.line(c.s(X0 + 32, Y1 - 70, X1 - 32, Y1 - 70), fill=(241, 237, 230, 30), width=SS)
    c.text((X0 + 32, Y1 - 36), '아무 키나 눌러도 넘어갑니다', 13.5, MUTED, True, 'lm', shadow=False)
    hint(c, X1 - 32, Y1 - 36)
    return c.done()


def result_b(plate):
    """B. classic settlement card in the middle: party on the left, rewards and spoils on the right."""
    c = Canvas(plate)
    sel.veil(c, 0.52, 0.74, 0.5)
    X0, Y0, X1, Y1 = 330, 232, 1590, 768
    frost(c, (X0, Y0, X1, Y1), 24)
    c.glass((X0, Y0, X1, Y1), r=24, alpha=0.88, line=0.18, sheen=False)
    victory(c, 960, 110, 58, False)
    mid = 930
    heading(c, X0 + 40, Y0 + 44, '파티')
    party_rows(c, X0 + 40, Y0 + 70, mid - X0 - 80, 78, 56)
    c.d.line(c.s(mid, Y0 + 36, mid, Y1 - 100), fill=(241, 237, 230, 26), width=SS)
    heading(c, mid + 40, Y0 + 44, '보상')
    tw = (X1 - mid - 80 - 16) / 2
    reward_tile(c, mid + 40, Y0 + 66, tw, icon('T_Reward_Exp', 56), '경험치', EXP)
    reward_tile(c, mid + 56 + tw, Y0 + 66, tw, icon('T_Reward_Gold', 56), '골드', GOLD_N)
    heading(c, mid + 40, Y0 + 200, '획득')
    spoils(c, mid + 40, Y0 + 232, 88, 36)
    c.d.line(c.s(X0 + 40, Y1 - 76, X1 - 40, Y1 - 76), fill=(241, 237, 230, 30), width=SS)
    hint(c, 960, Y1 - 40, 'm')
    return c.done()


def result_c(plate):
    """C. on the stage: the level-up rides above the character, the rewards run in one strip along the bottom."""
    c = Canvas(plate)
    victory(c, 960, 70, 58)
    # above the player: name, level, the bar filling
    x, top = PLAYER_FEET[0], PLAYER_HEAD - 150
    w = 280
    frost(c, (x - w / 2, top, x + w / 2, top + 104), 16)
    c.glass((x - w / 2, top, x + w / 2, top + 104), r=16, alpha=0.86, line=0.18, sheen=False)
    c.text((x - w / 2 + 18, top + 26), ME['name'], 18, IVORY, False, 'lm', shadow=False)
    c.text((x + w / 2 - 18, top + 26), 'Lv.%d' % ME['lv'], 15, GOLD_HI, True, 'rm', shadow=False)
    bar(c, x - w / 2 + 18, top + 54, w - 36, ME['ratio'], (232, 196, 110), 7)
    c.text((x + w / 2 - 18, top + 76), '다음 레벨까지 {:,}'.format(ME['remain']), 12.5, MUTED, True, 'rm', shadow=False)
    c.d.polygon(c.s(x - 9, top + 104, x + 9, top + 104, x, top + 114), fill=(13, 24, 37, 220))
    levelup_chip(c, x - 74, top - 22, ME['old'], ME['lv'], 13)
    # bottom strip
    sc = Image.new('RGBA', (c.W * SS, c.H * SS), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sc)
    for y in range(850, c.H):
        t = min(1, (y - 850) / 110)
        sd.line([(0, y * SS), (c.W * SS, y * SS)], fill=(9, 17, 27, int(255 * 0.86 * t * t * (3 - 2 * t))), width=SS)
    c.layer = Image.alpha_composite(sc, c.layer)
    c.d = ImageDraw.Draw(c.layer)
    y = 992
    sx = 250
    for im, label, v in ((icon('T_Reward_Exp', 48), '경험치', EXP), (icon('T_Reward_Gold', 48), '골드', GOLD_N)):
        paste(c, im, sx, y - 24)
        c.text((sx + 60, y - 12), label, 13.5, MUTED, True, 'lm', shadow=False)
        c.text((sx + 60, y + 14), '+{:,}'.format(v), 24, IVORY, False, 'lm', shadow=True)
        sx += 230
    c.d.line(c.s(sx - 14, y - 34, sx - 14, y + 34), fill=(241, 237, 230, 40), width=SS)
    spoils(c, sx + 18, y - 44, 64, 44)
    hint(c, 1720, y)
    return c.done()


# ---------------------------------------------------------------- speech bubbles
# where things stand in bub_plate (1080p): nameplate centres (the plate has no screen widgets, so plates are drawn here too)
SPEAKERS = [
    # (nameplate centre x, nameplate centre y, name, channel, line) --- positions measured on bub_plate (head top - ~38 px)
    (958, 528, '리아', 'local', '펀브룩 초입에서 푸루나 잡으실 분 구해요. 포획 도와드릴게요!'),
    (410, 338, 'Fake9302', 'party', '다들 준비됐으면 출발할게요'),
]
CHANNEL_LINES = [('리아', 'local', '안녕하세요!'), ('Fake9301', 'party', '다들 준비됐으면 출발할게요'),
                 ('Fake9302', 'world', '대장간 앞에서 거래해요~')]
CHAN = {'local': (None, None), 'party': ('파티', SKY), 'world': ('전체', GOLD_HI)}


def nameplate(c, cx, cy, name, colour=None):
    f = font(15, True)
    w = c.d.textlength(name, font=f) / SS + 30
    rr(c, (cx - w / 2, cy - 14, cx + w / 2, cy + 14), 14, (16, 24, 32, 175))
    if colour:
        c.diamond((cx - w / 2 + 12, cy), 3.5, fill=colour)
    c.text((cx + (5 if colour else 0), cy + 1), name, 15, IVORY, True, 'mm', shadow=False)


def balanced(c, s, px, maxw, semi=True):
    """Line count first, then even lines; with two lines prefer breaking after a sentence end (. ! ? ~)."""
    f = font(px, semi)
    full = c.d.textlength(s, font=f) / SS
    n = max(1, int(np.ceil(full / maxw)))
    words = s.split(' ')
    if n == 2:
        best = None
        for i in range(1, len(words)):
            a, b = ' '.join(words[:i]), ' '.join(words[i:])
            wa, wb = c.d.textlength(a, font=f) / SS, c.d.textlength(b, font=f) / SS
            if max(wa, wb) > maxw:
                continue
            cost = max(wa, wb) - (1000 if a[-1] in '.!?~' else 0)   # a sentence end that fits always wins
            if best is None or cost < best[0]:
                best = (cost, [a, b], max(wa, wb))
        if best:
            return best[1], best[2]
    target = min(maxw, full / n * 1.06)
    words, lines, cur = s.split(' '), [], ''
    for w in words:
        t = (cur + ' ' + w).strip()
        if c.d.textlength(t, font=f) / SS > target and cur:
            lines.append(cur)
            cur = w
        else:
            cur = t
    lines.append(cur)
    width = max(c.d.textlength(l, font=f) / SS for l in lines)
    return lines, width


def bubble_glass(c, cx, bottom, text, chan):
    label, col = CHAN[chan]
    px, lh = 17, 1.45
    lines, tw = balanced(c, text, px, 360)
    pad_x, pad_y = 16, 11
    chip_w = 0
    if label:
        chip_w = c.d.textlength(label, font=font(12, True)) / SS + 14
    w = tw + pad_x * 2 + (chip_w + 8 if label else 0)
    h = len(lines) * px * lh + pad_y * 2 - px * (lh - 1)
    x0, y1 = cx - w / 2, bottom - 9
    y0 = y1 - h
    c.shadow((x0, y0, x0 + w, y1), 14, 0.35, 10, 4)
    rr(c, (x0, y0, x0 + w, y1), 14, (19, 34, 51, 236), (241, 237, 230, 60), 1)
    c.d.polygon(c.s(cx - 8, y1 - 0.5, cx + 8, y1 - 0.5, cx, y1 + 8), fill=(19, 34, 51, 236))
    tx = x0 + pad_x
    if label:
        rr(c, (tx, y0 + pad_y + 1, tx + chip_w, y0 + pad_y + 21), 10, col + (46,), col + (200,), 1)
        c.text((tx + chip_w / 2, y0 + pad_y + 11.5), label, 12, col, True, 'mm', shadow=False)
        tx += chip_w + 8
    for i, l in enumerate(lines):
        c.text((tx, y0 + pad_y + 11 + i * px * lh), l, px, IVORY, True, 'lm', shadow=False)


def pixel_box(w, h, fill, line, unit=2):
    """A pixel-art bubble body drawn on the 2 px grid: stepped corners, 1-dot outline, darker bottom lip."""
    W, H = int(w // unit), int(h // unit)
    im = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rectangle([2, 0, W - 3, H - 1], fill=line)
    d.rectangle([1, 1, W - 2, H - 2], fill=line)
    d.rectangle([0, 2, W - 1, H - 3], fill=line)
    d.rectangle([3, 1, W - 4, H - 2], fill=fill)
    d.rectangle([2, 2, W - 3, H - 3], fill=fill)
    d.rectangle([1, 3, W - 2, H - 4], fill=fill)
    shade = tuple(int(v * 0.88) for v in fill[:3]) + (255,)
    d.rectangle([3, H - 3, W - 4, H - 3], fill=shade)
    return im


def pixel_tail(fill, line):
    im = Image.new('RGBA', (9, 6), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for i in range(5):
        d.line([(i, i), (8 - i, i)], fill=line)
        if i < 4:
            d.line([(i + 1, i), (7 - i, i)], fill=fill)
    return im


def bubble_paper(c, cx, bottom, text, chan):
    fills = {'local': ((246, 241, 230, 255), (34, 44, 58, 255)), 'party': ((226, 240, 250, 255), (36, 86, 132, 255)),
             'world': ((250, 241, 214, 255), (122, 88, 34, 255))}
    fill, line = fills[chan]
    label, col = CHAN[chan]
    px, lh = 17, 1.45
    lines, tw = balanced(c, text, px, 360)
    pad_x, pad_y = 18, 12
    chip = ''
    if label:
        chip = '[%s] ' % label
    cw = c.d.textlength(chip, font=font(px, False)) / SS if chip else 0
    w = int((tw + cw + pad_x * 2) / 2) * 2 + 2
    h = int((len(lines) * px * lh + pad_y * 2 - px * (lh - 1)) / 2) * 2 + 2
    x0, y1 = int(cx - w / 2) // 2 * 2, int(bottom - 12) // 2 * 2
    y0 = y1 - h
    body = pixel_box(w, h, fill, line)
    pix(c, body, (x0, y0, x0 + w, y1))
    tail = pixel_tail(fill, line)
    pix(c, tail, (cx - 9, y1 - 2, cx + 9, y1 + 10))
    tx = x0 + pad_x
    ink = line[:3]
    if chip:
        c.text((tx, y0 + pad_y + 11), chip, px, ink, False, 'lm', shadow=False)
        tx += cw
    for i, l in enumerate(lines):
        c.text((tx, y0 + pad_y + 11 + i * px * lh), l, px, (26, 32, 40), True, 'lm', shadow=False)


def bubble_merged(c, cx, cy, name, text, chan):
    """C. the nameplate becomes the bubble's name tab while the character talks (one element over the head)."""
    label, col = CHAN[chan]
    px, lh = 17, 1.45
    lines, tw = balanced(c, text, px, 360)
    pad_x, pad_y = 16, 12
    f = font(14, True)
    tab = name + ('  ·  ' + label if label else '')
    tab_w = c.d.textlength(tab, font=f) / SS + 26
    w = max(tw + pad_x * 2, tab_w + 30)
    h = len(lines) * px * lh + pad_y * 2 - px * (lh - 1) + 8
    x0, y1 = cx - w / 2, cy + 14
    y0 = y1 - h
    c.shadow((x0, y0, x0 + w, y1), 14, 0.35, 10, 4)
    rr(c, (x0, y0, x0 + w, y1), 14, (19, 34, 51, 236), (241, 237, 230, 60), 1)
    # name tab riding the top edge
    tcol = col or (170, 182, 191)
    rr(c, (x0 + 14, y0 - 14, x0 + 14 + tab_w, y0 + 12), 13, (13, 24, 37, 250), tcol + (210,), 1.2)
    c.text((x0 + 14 + tab_w / 2, y0 - 0.5), tab, 14, tcol if col else IVORY, True, 'mm', shadow=False)
    for i, l in enumerate(lines):
        c.text((x0 + pad_x, y0 + pad_y + 19 + i * px * lh), l, px, IVORY, True, 'lm', shadow=False)


def bubbles(plate, kind):
    c = Canvas(plate)
    for cx, cy, name, chan, line in SPEAKERS:
        if kind == 'merged':
            bubble_merged(c, cx, cy, name, line, chan)
            continue
        nameplate(c, cx, cy, name, CHAN[chan][1] if kind == 'glass' else None)
        (bubble_glass if kind == 'glass' else bubble_paper)(c, cx, cy - 16, line, chan)
    return c.done()


def channels(plate, kind):
    """The three channels side by side over the brightest ground in the plate (legibility check), 1.5x for the page."""
    bg = plate.crop((560, 760, 1840, 1040)).convert('RGBA')
    c = Canvas(bg)
    for i, (name, chan, line) in enumerate(CHANNEL_LINES):
        cx, cy = 213 + i * 427, 230
        if kind == 'merged':
            bubble_merged(c, cx, cy, name, line, chan)
            continue
        nameplate(c, cx, cy, name, CHAN[chan][1] if kind == 'glass' else None)
        (bubble_glass if kind == 'glass' else bubble_paper)(c, cx, cy - 16, line, chan)
    im = c.done()
    return im.resize((im.width * 3 // 2, im.height * 3 // 2), Image.NEAREST)


def uri(im):
    return sel.uri(im)


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'png').mkdir(exist_ok=True)
    rplate = Image.open(CAP / 'res_plate_view.png').convert('RGBA')
    bplate = Image.open(CAP / 'bub_plate_view.png').convert('RGBA')
    now_bub = [Image.open(CAP / ('bub_%s_view.png' % k)).convert('RGBA') for k in ('short', 'long', 'party')]
    strip = Image.new('RGBA', (1200, 380))
    for i, im in enumerate(now_bub):
        strip.paste(im.crop((760, 400, 1160, 780)), (i * 400, 0))
    shots = {
        'now_result': Image.open(CAP / 'res_now_view.png').convert('RGBA'),
        'res_a': result_a(rplate), 'res_b': result_b(rplate), 'res_c': result_c(rplate),
        'now_bubble': Image.open(CAP / 'bub_long_view.png').convert('RGBA'),
        'now_bubble_zoom': strip.resize((1800, 570), Image.NEAREST),
        'bub_a': bubbles(bplate, 'glass'), 'bub_b': bubbles(bplate, 'paper'), 'bub_c': bubbles(bplate, 'merged'),
    }
    for k, kind in (('bub_a', 'glass'), ('bub_b', 'paper'), ('bub_c', 'merged')):
        shots[k + '_zoom'] = channels(bplate, kind)
    for k, im in shots.items():
        im.convert('RGB').save(OUT / 'png' / (k + '.png'))
    html = PAGE
    for k, im in shots.items():
        small = im if im.width <= 1920 and im.height < 1000 else im.resize((1280, 720), Image.LANCZOS)
        html = html.replace('%%%s%%' % k.upper(), uri(small))
    (OUT / 'result_bubble.html').write_text(html, encoding='utf-8')
    print('wrote', OUT / 'result_bubble.html')


PAGE = '''<!doctype html><html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>전투 결과·말풍선 시안</title><style>
:root{--bg:#0b1219;--panel:#111c27;--text:#e8e4dc;--sub:#9aa7b1;--gold:#d6be8c}
@media (prefers-color-scheme: light){:root:not([data-theme="dark"]){--bg:#eef0f2;--panel:#fff;--text:#1d2731;--sub:#5d6b77}}
:root[data-theme="light"]{--bg:#eef0f2;--panel:#fff;--text:#1d2731;--sub:#5d6b77}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.65 "Pretendard","Malgun Gothic",sans-serif}
main{max-width:1320px;margin:0 auto;padding:40px 16px 80px}h1{font-size:28px;margin:0 0 6px}h2{font-size:22px;margin:52px 0 8px}
h2 small{font-size:14px;color:var(--sub);font-weight:400;margin-left:8px}
.lead{color:var(--sub);max-width:920px;margin:0 0 14px}.card{background:var(--panel);border-radius:14px;padding:16px;border:1px solid rgba(128,140,150,.18);margin-bottom:18px}
.card h3{margin:0 0 8px;font-size:16px}.card p,.card li{color:var(--sub);font-size:14px}.card p{margin:8px 0 0}img{width:100%;display:block;border-radius:10px}
.zoom{margin-top:10px}.tag{font-size:12px;font-weight:700;padding:1px 8px;border-radius:99px;background:#2f4a3a;color:#bfe8c8;margin-left:6px}
.pick{outline:2px solid var(--gold);outline-offset:3px}ul{margin:6px 0 0;padding-left:18px}.cap{font-size:13px;color:var(--sub);margin-top:6px}
nav{display:flex;gap:10px;flex-wrap:wrap;margin:10px 0 0}nav a{color:var(--gold);text-decoration:none;font-size:14px;border:1px solid rgba(214,190,140,.4);padding:3px 12px;border-radius:99px}
</style></head><body><main>
<h1>전투 결과창 · 대화 말풍선 시안</h1>
<p class="lead">두 화면 모두 실제 게임에서 띄운 "지금" 캡처에서 출발했습니다. 시안은 같은 장면(1080p, 화면 UI 없이 찍은 판) 위에 게임 크기로 그렸고,
캐릭터·나이아는 게임 스프라이트 원본(정수 배율), 아이템·경험치·골드·속성 아이콘은 지금 쓰는 도트 아이콘입니다. 게임 에셋·코드는 바꾸지 않았습니다.</p>
<nav><a href="#result">전투 결과창</a><a href="#bubble">대화 말풍선</a></nav>

<h2 id="result">1. 전투 결과창 <small>승리하거나 포획하면 뜨고, 닫혀야 전투가 끝남</small></h2>
<div class="card"><img src="%NOW_RESULT%"><div class="cap">지금 — dongh WBP_BattleResult (펀브룩 전투). 이기는 치트가 없어, 결과창 C++ 이 채우는 칸을 같은 방식으로 예시 값으로 채워 찍음</div>
<ul>
<li>제목 "Result", EXP · GOLD · NEXT · LEVEL UP 이 영어 + 엔진 기본 글꼴 — 새 HUD(남색 유리 · 금 · 마루부리)와 톤이 다르고, 이겼는지 잡았는지 말해 주지 않음</li>
<li>반투명 사각 판 네 장이 화면 위쪽에 흩어짐 — 전투원 판은 폭 1120px 중 절반이 비고, 판 너머 풀·나무가 비쳐 글이 묻힘</li>
<li>명단이 3줄마다 끊겨 넷째(아쿠퐁)만 옆 열 맨 위에 홀로. 나이아 줄은 경험치가 없는데 빈 막대 + "NEXT" 가 떠 0 으로 읽힘</li>
<li>레벨 업이 작은 영어 글자 한 줄 — 축하할 순간이 지나감</li>
<li>잡은 나이아는 그림 없이 이름만 (나이아 표의 아이콘 칸이 비어 있음)</li>
<li>확인 버튼이 화면 가운데 아래 혼자 떠 있고 무엇과도 줄이 안 맞음, 키 안내 없음 (실제로는 아무 키나 넘어감)</li></ul></div>

<div class="card pick"><h3>A. 승리 제목 + 오른쪽 정산 시트<span class="tag">추천</span></h3><img src="%RES_A%">
<p>왼쪽은 무대 그대로 — 큰 "승리" 제목과 캐릭터 머리 위 "레벨 업!". 오른쪽 유리 시트 한 장에 보상(경험치 · 골드 그림 칸) → 파티(얼굴 · 레벨 · 경험치 막대,
레벨 업은 금 칩 "5 → 6") → 획득(아이템 그림 칸 + 수량, 잡은 나이아는 금테 + "새 동료"). 의뢰 팝업 · 직업 선택과 같은 "무대 + 오른쪽 시트" 말투.
나이아 줄은 경험치가 생길 때까지 막대 대신 속성 아이콘. 숫자는 0 에서 차오르고, 막대는 이전 값에서 채워지며 넘치면 레벨 업 칩이 튑니다. F 계속(아무 키나 그대로).</p></div>
<div class="card"><h3>B. 가운데 정산서</h3><img src="%RES_B%">
<p>장면을 가라앉히고 큰 카드 한 장 — 왼쪽 파티, 오른쪽 보상 · 획득. 고전 JRPG 결과 화면. 한눈에 정리되지만 방금 싸운 무대가 가려져 여운이 짧습니다.</p></div>
<div class="card"><h3>C. 무대 위 정산</h3><img src="%RES_C%">
<p>판 없이 캐릭터 머리 위에 이름 · 레벨 · 막대 말풍선, 아래 띠에 보상과 획득을 한 줄로. 가장 가볍고 장면이 살지만, 파티 나이아 줄을 둘 자리가 없어
(나이아 경험치가 생기면 각자 머리 위에) 정보가 흩어집니다. 파티가 넷일 때 머리 위 판이 겹칠 수 있음.</p></div>

<h2 id="bubble">2. 대화 말풍선 <small>근처 · 전체 · 파티 채팅을 말한 캐릭터 머리 위에</small></h2>
<div class="card"><img src="%NOW_BUBBLE%"><img class="zoom" src="%NOW_BUBBLE_ZOOM%">
<div class="cap">지금 — dbals WBP_SpeechBubble / _Party (리베르그로브, nu.Bubble.Say · SayParty). 아래는 짧은 말 · 긴 말 · 파티 말 확대</div>
<ul>
<li>흰 판 + 검은 선 + 엔진 기본 고딕 — 그림 속 도트 세계와도, 새 HUD 유리 톤과도 따로 놂</li>
<li>바로 아래 이름표는 어두운 반투명 알약 — 한 머리 위에 모양이 다른 판 두 장이 쌓임</li>
<li>긴 말은 고정 폭에서 끊겨 "잡으실 / 분"처럼 어색한 줄바꿈, 글이 작음</li>
<li>파티는 하늘색 판으로만 구분, 전체 채팅은 근처와 똑같아 누구에게 한 말인지 모름</li></ul></div>

<div class="card pick"><h3>A. 유리 말풍선 — 이름표와 한 벌<span class="tag">추천</span></h3><img src="%BUB_A%"><img class="zoom" src="%BUB_A_ZOOM%">
<p>이름표와 같은 남색 유리 · 상아 글(마루부리 17px). 채널은 말풍선 안 앞머리 칩과 이름표 앞 점 색으로 — 근처는 칩 없음, 파티 하늘색, 전체 금색.
두 줄이면 문장 끝(. ! ? ~)에서 먼저 끊고, 아니면 줄 길이를 고르게 맞춰 "잡으실 / 분"처럼 끊기지 않게. 3줄 넘으면 "…". 뜰 때 살짝 튀어 오르고(0.18초) 흐려지며 사라짐.</p></div>
<div class="card"><h3>B. 도트 종이 말풍선</h3><img src="%BUB_B%"><img class="zoom" src="%BUB_B_ZOOM%">
<p>2px 격자 계단 모서리 · 한 도트 외곽선 · 아래 그늘 줄의 상아 종이 — 캐릭터 도트와 가장 잘 섞입니다. 파티 하늘 종이, 전체 노란 종이에 [파티]/[전체] 머리말.
밝은 바닥 위에서 가장 잘 읽히지만 HUD 유리 톤과는 다른 세계 쪽 말투입니다.</p></div>
<div class="card"><h3>C. 이름표 일체형</h3><img src="%BUB_C%"><img class="zoom" src="%BUB_C_ZOOM%">
<p>말하는 동안 이름표가 말풍선의 이름 탭이 됩니다(머리 위엔 한 장만). 탭에 이름 · 채널, 채널 색 테두리. 누가 말했는지 가장 분명하지만,
말풍선이 뜰 때 이름표를 숨기는 코드가 필요합니다(말풍선 컨트롤러가 이름표 컴포넌트를 이미 찾고 있어 손은 적음).</p></div>

<h2>어느 안이든 같이</h2>
<div class="card"><ul>
<li>결과창: 승리 / 포획만 / 레벨 업 없음 세 경우 제목이 바뀜 ("승리" · "포획 성공"), 열려 있는 동안 전투 HUD(턴 칩 · 이름표) 접기</li>
<li>결과창: 잡은 나이아 그림은 나이아 표 아이콘이 비어 있어 대기 플립북 첫 그림을 씀 (선택 창 카드와 같은 방식)</li>
<li>결과창: 지금 C++ 계약(MemberColumnBox · ExperienceText · GoldText · 선택 칸)은 그대로 두고 새 WBP 로 교체 — 예전 WBP 는 남겨 되돌리기 가능.
숫자 차오름 · 막대 채움 · 칩 튐은 C++ 선택 기능(기본 꺼짐)으로</li>
<li>말풍선: 전체 채팅용 모양 칸(WorldSpeechBubbleClass) 하나 추가 — 지금은 파티만 따로 지정 가능</li>
<li>말풍선: 서버 없이 확인은 지금처럼 nu.Bubble.Say / SayParty, 전체용 nu.Bubble.SayWorld 추가</li>
</ul></div>
</main></body></html>'''

if __name__ == '__main__':
    build()
