# -*- coding: utf-8 -*-
"""Shop window proposal (proposal only, no game assets touched).

Output: Docs/HUD_Shop_Proposal_20261014/shop.html (+ png/). "Now" frames are real game captures (Libregrove, shop NPC 시안,
qa/states_shop.json): the conversation, the shop the game opens (dbals WBP_ShopWindow + the old inventory window) and
Minki's shop design page (MinkiCho WBP_ShopPage, nu.Shop.Preview, not wired to the game). The mocks are drawn here at 1080p
over a real frame captured without screen UI (sh_shop_clean); item icons are cropped from the real captures.
Run: python Tools/HUDRenewal/build_shop.py
"""
import base64
import importlib.util
import io
import json
import pathlib

from PIL import Image, ImageDraw

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('build_dialogue', HERE / 'build_dialogue.py')
bd = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bd)
Canvas, font, SS = bd.Canvas, bd.font, bd.SS
NAVY, IVORY, GOLD, GOLD_HI, MUTED = bd.NAVY, bd.IVORY, bd.GOLD, bd.GOLD_HI, bd.MUTED

ROOT = HERE.parents[1]
CAP = ROOT / 'Saved' / 'CharacterCreateGlass'
OUT = ROOT / 'Docs' / 'HUD_Shop_Proposal_20261014'
ART = ROOT / 'Saved' / 'HUDRenewal' / 'npc_art'

PAPER = (243, 236, 223)        # Minki's ivory page
CARD = (251, 247, 239)
CARD_LINE = (222, 211, 192)
INK = (27, 42, 58)
INK_MUTED = (118, 128, 138)
GOLD_INK = (150, 118, 58)      # gold that reads on ivory

MERCHANT = '시안'
SHOP_NAME = '잡화 좌판'
LINE = '필요한 포션이 있으면 직접 집어가고, 돈은 저 그릇에 던져둬.'
GOLD_HAVE = 10000
# (icon index in the page capture, name, description, price, owned, locked level)
ITEMS = [
    (0, '하급 체력 포션', '체력을 조금 회복하는 포션입니다.', 40, 5, 0),
    (1, '하급 SP 포션', 'SP를 조금 회복하는 포션입니다.', 50, 0, 0),
    (2, '화상 치료제', '화상을 치료하는 약품입니다.', 160, 0, 10),
    (3, '둔화 치료제', '둔화 상태를 치료하는 약품입니다.', 160, 0, 10),
    (4, '중독 치료제', '중독 상태를 치료하는 약품입니다.', 160, 0, 10),
    (5, '환각 치료제', '환각 상태를 치료하는 약품입니다.', 160, 0, 10),
]
SEL = 0
QTY = 3


def load_icons():
    page = Image.open(CAP / 'sh_page_view.png').convert('RGBA')
    icons = [page.crop((446, 414 + 76 * i, 501, 414 + 76 * i + 59)) for i in range(6)]
    shop = Image.open(CAP / 'sh_shop_view.png').convert('RGBA')
    bag = [shop.crop((1456 + 78 * c, 252 + 77 * r, 1456 + 78 * c + 70, 252 + 77 * r + 70)) for r in range(3) for c in range(5)][:12]
    return icons, bag


def bust(size):
    rows = json.load(open(ART / 'npc_art.json', encoding='utf-8'))
    r = next(x for x in rows if x['flipbook'].endswith('FB_NPC_4002_Idle'))
    im = Image.open(r['png']).convert('RGBA')
    crop = im.crop((4, 1, 44, 41))
    return crop.resize((size, size), Image.NEAREST)


# ---------------------------------------------------------------- drawing helpers on bd.Canvas
def rr(c, box, r, fill, line=None, width=1):
    """Rounded box blended over what is already drawn (ImageDraw alone would replace the pixels, punching holes)."""
    x0, y0, x1, y1 = box
    fill = tuple(fill) + (255,) if fill is not None and len(fill) == 3 else fill
    line = tuple(line) + (255,) if line is not None and len(line) == 3 else line
    pad = int(width * SS) + 2
    bx0, by0 = int(x0 * SS) - pad, int(y0 * SS) - pad
    tmp = Image.new('RGBA', (int((x1 - x0) * SS) + pad * 2 + 1, int((y1 - y0) * SS) + pad * 2 + 1), (0, 0, 0, 0))
    ImageDraw.Draw(tmp).rounded_rectangle([x0 * SS - bx0, y0 * SS - by0, x1 * SS - bx0, y1 * SS - by0], r * SS, fill=fill,
                                          outline=line, width=int(width * SS) if line else 0)
    c.layer.alpha_composite(tmp, (bx0, by0))
    c.d = ImageDraw.Draw(c.layer)


def close_x(c, x, y, r=16):
    c.d.ellipse(c.s(x - r, y - r, x + r, y + r), fill=(30, 44, 60, 255), outline=(241, 237, 230, 90), width=SS)
    k = r * 0.36
    for a, b in (((x - k, y - k), (x + k, y + k)), ((x - k, y + k), (x + k, y - k))):
        c.d.line(c.s(*a, *b), fill=IVORY + (255,), width=int(2 * SS))


def tri_down(c, x, y, s=5, colour=INK_MUTED):
    c.d.polygon(c.s(x - s, y - s * .5, x + s, y - s * .5, x, y + s * .6), fill=colour + (255,))


def ink(c, xy, s, px, colour=INK, semi=False, anchor='la'):
    c.text(xy, s, px, colour, semi, anchor, shadow=False)


def pix(c, im, box):
    """Paste pixel art (icons, portrait) into box with nearest scaling."""
    x0, y0, x1, y1 = box
    big = im.resize((int((x1 - x0) * SS), int((y1 - y0) * SS)), Image.NEAREST)
    c.layer.alpha_composite(big, (int(x0 * SS), int(y0 * SS)))


def coin(c, x, y, r=8):
    c.d.ellipse(c.s(x - r, y - r, x + r, y + r), fill=(214, 166, 64, 255), outline=(120, 86, 30, 255), width=int(1.2 * SS))
    c.d.ellipse(c.s(x - r * .45, y - r * .55, x + r * .1, y - r * .1), fill=(255, 226, 150, 230))


def money(c, x, y, value, px, colour, anchor='rm'):
    s = '{:,}'.format(value)
    f = font(px, False)
    w = c.d.textlength(s, font=f) / SS
    if anchor == 'rm':
        tx = x
        cx = x - w - 8 - px * 0.36
    else:
        cx = x + px * 0.36
        tx = x + px * 0.72 + 8 + w
    coin(c, cx, y + 1, px * 0.36)
    c.text((tx, y), s, px, colour, False, 'rm', shadow=colour == IVORY)


def lock_chip(c, x, y, level, dark=False):
    """Small 'Lv.10' chip with a lock, right-anchored at x."""
    s = 'Lv.%d 부터' % level
    f = font(13.5, False)
    w = c.d.textlength(s, font=f) / SS + 34
    fill = (40, 54, 70, 230) if dark else (236, 226, 206, 255)
    line = (214, 190, 140, 200) if dark else (196, 170, 120, 255)
    rr(c, (x - w, y - 13, x, y + 13), 13, fill, line, 1.2)
    lx = x - w + 13
    col = GOLD_HI if dark else GOLD_INK
    c.d.rounded_rectangle(c.s(lx - 5, y - 1, lx + 5, y + 7), 2 * SS, fill=col + (255,))
    c.d.arc(c.s(lx - 3.6, y - 7, lx + 3.6, y + 1), 180, 360, fill=col + (255,), width=int(1.8 * SS))
    c.text((x - 10, y + 0.5), s, 13.5, col, False, 'rm', shadow=False)


def stepper(c, x, y, qty, dark=False):
    """[−] qty [+] at (x, y) top-left, 132 wide, 36 high. Returns right edge."""
    fill = (22, 41, 61, 235) if dark else (255, 255, 255, 255)
    line = (241, 237, 230, 70) if dark else CARD_LINE + (255,)
    col = IVORY if dark else INK
    rr(c, (x, y, x + 132, y + 36), 10, fill, line, 1.2)
    for i, g in ((0, '−'), (2, '+')):
        bx = x + i * 48
        rr(c, (bx + 3, y + 3, bx + 36, y + 33), 8, (255, 255, 255, 18) if dark else (243, 236, 223, 255))
        c.text((bx + 19.5, y + 18), g, 19, col, True, 'mm', shadow=False)
    c.text((x + 66, y + 18.5), str(qty), 19, col, True, 'mm', shadow=False)
    return x + 132


def keychip(c, x, y, label, size=24):
    bd.keychip(c, (x, y), label, size)


def gold_button(c, box, label, key=None):
    x0, y0, x1, y1 = box
    rr(c, box, 12, (214, 190, 140, 255), (150, 118, 58, 255), 1.2)
    rr(c, (x0 + 2, y0 + 2, x1 - 2, y0 + (y1 - y0) * .45), 10, (255, 255, 255, 40))
    c.text(((x0 + x1) / 2 - (14 if key else 0), (y0 + y1) / 2 + 1), label, 20, NAVY, True, 'mm', shadow=False)
    if key:
        keychip(c, x1 - 44, (y0 + y1) / 2 - 12, key)


def tabs(c, x, y, names, sel, dark, w=112, h=40):
    for i, n in enumerate(names):
        bx = x + i * (w + 6)
        on = i == sel
        if dark:
            rr(c, (bx, y, bx + w, y + h), 12, (241, 237, 230, 235) if on else (241, 237, 230, 16), (241, 237, 230, 60), 1)
            c.text((bx + w / 2, y + h / 2 + 1), n, 18, NAVY if on else IVORY, True, 'mm', shadow=False)
        else:
            rr(c, (bx, y, bx + w, y + h), 12, (27, 42, 58, 255) if on else (255, 255, 255, 120), (27, 42, 58, 255) if on else CARD_LINE + (255,), 1)
            c.text((bx + w / 2, y + h / 2 + 1), n, 18, IVORY if on else INK, True, 'mm', shadow=False)
    return x + len(names) * (w + 6)


def pills(c, x, y, names, sel, dark):
    for i, n in enumerate(names):
        f = font(14.5, True)
        w = c.d.textlength(n, font=f) / SS + 28
        on = i == sel
        if dark:
            rr(c, (x, y, x + w, y + 30), 15, (214, 190, 140, 60) if on else (241, 237, 230, 10), (214, 190, 140, 190) if on else (241, 237, 230, 50), 1)
            c.text((x + w / 2, y + 15.5), n, 14.5, GOLD_HI if on else MUTED, True, 'mm', shadow=False)
        else:
            rr(c, (x, y, x + w, y + 30), 15, (214, 190, 140, 90) if on else (255, 255, 255, 0), (150, 118, 58, 255) if on else CARD_LINE + (255,), 1)
            c.text((x + w / 2, y + 15.5), n, 14.5, GOLD_INK if on else INK_MUTED, True, 'mm', shadow=False)
        x += w + 8
    return x


# ---------------------------------------------------------------- A. ivory page (Minki's design, finished and wired)
def option_a(plate, icons):
    c = Canvas(plate)
    X0, Y0, X1, Y1 = 300, 150, 1620, 936
    c.shadow((X0, Y0, X1, Y1), 18, alpha=0.55, blur=26, dy=10)
    rr(c, (X0, Y0, X1, Y1), 18, (14, 26, 40, 246), (214, 190, 140, 150), 1.4)
    rr(c, (X0 + 6, Y0 + 6, X1 - 6, Y1 - 6), 14, None, (214, 190, 140, 50), 1)
    # header: merchant and her real line, gold, close
    med = bust(92)
    c.d.ellipse(c.s(X0 + 30, Y0 + 22, X0 + 30 + 100, Y0 + 22 + 100), fill=(22, 41, 61, 255), outline=GOLD + (230,), width=2 * SS)
    mask = Image.new('L', (100 * SS, 100 * SS), 0)
    ImageDraw.Draw(mask).ellipse([3 * SS, 3 * SS, 97 * SS, 97 * SS], fill=255)
    big = med.resize((100 * SS, 100 * SS), Image.NEAREST)
    big.putalpha(Image.composite(big.split()[3], Image.new('L', big.size, 0), mask))
    c.layer.alpha_composite(big, (int((X0 + 30) * SS), int((Y0 + 22) * SS)))
    c.diamond((X0 + 148, Y0 + 48), 5)
    c.text((X0 + 162, Y0 + 48), MERCHANT, 22, GOLD_HI, anchor='lm')
    c.text((X0 + 162 + 52, Y0 + 49), '· ' + SHOP_NAME, 16, MUTED, True, 'lm')
    c.text((X0 + 148, Y0 + 90), '“' + LINE + '”', 19, IVORY, True, 'lm')
    c.text((X1 - 92, Y0 + 40), '소지금', 14, MUTED, True, 'rm')
    money(c, X1 - 40, Y0 + 72, GOLD_HAVE, 24, IVORY)
    close_x(c, X1 - 38, Y0 + 32)
    # tabs + categories
    ty = Y0 + 140
    tabs(c, X0 + 30, ty, ['구매', '판매'], 0, True)
    pills(c, X0 + 290, ty + 5, ['전체', '소모품', '재료', '기타'], 0, True)
    # paper body
    BX0, BY0, BX1, BY1 = X0 + 22, ty + 54, X1 - 22, Y1 - 52
    rr(c, (BX0, BY0, BX1, BY1), 14, PAPER + (255,), (214, 190, 140, 160), 1)
    # list
    LX0, LX1 = BX0 + 18, BX0 + 738
    y = BY0 + 16
    for i, (ic, name, desc, price, owned, lock) in enumerate(ITEMS):
        on = i == SEL
        h = 66
        rr(c, (LX0, y, LX1, y + h), 12, (255, 250, 238, 255) if on else CARD + (255,), (176, 140, 70, 255) if on else CARD_LINE + (255,), 2 if on else 1)
        if on:
            rr(c, (LX0 + 1, y + 12, LX0 + 6, y + h - 12), 2, (176, 140, 70, 255))
        rr(c, (LX0 + 12, y + 7, LX0 + 64, y + 59), 10, (14, 26, 40, 255))
        pix(c, icons[ic], (LX0 + 15, y + 9, LX0 + 61, y + 57))
        dim = lock > 0
        ink(c, (LX0 + 80, y + 12), name, 19, INK_MUTED if dim else INK, False)
        ink(c, (LX0 + 80, y + 40), desc, 14, INK_MUTED, True)
        if dim:
            rr(c, (LX0 + 12, y + 7, LX0 + 64, y + 59), 10, (243, 236, 223, 120))
            lock_chip(c, LX1 - 16, y + h / 2, lock)
            money(c, LX1 - 150, y + h / 2, price, 18, INK_MUTED)
        else:
            money(c, LX1 - 150, y + h / 2, price, 18, INK)
            ink(c, (LX1 - 18, y + h / 2), '보유 %d' % owned if owned else '—', 15, INK_MUTED, True, 'rm')
        y += h + 8
    tri_down(c, (LX0 + LX1) / 2 - 78, y + 14)
    ink(c, ((LX0 + LX1) / 2 + 6, y + 6), '1개 더 · 마비 치료제', 14, INK_MUTED, True, 'ma')
    # detail
    DX0, DX1 = LX1 + 22, BX1 - 18
    DY0, DY1 = BY0 + 16, BY1 - 16
    rr(c, (DX0, DY0, DX1, DY1), 12, CARD + (255,), CARD_LINE + (255,), 1)
    ic, name, desc, price, owned, lock = ITEMS[SEL]
    rr(c, (DX0 + 20, DY0 + 20, DX0 + 112, DY0 + 112), 14, (14, 26, 40, 255))
    pix(c, icons[ic], (DX0 + 26, DY0 + 24, DX0 + 106, DY0 + 108))
    ink(c, (DX0 + 132, DY0 + 34), name, 24, INK, False)
    rr(c, (DX0 + 132, DY0 + 72, DX0 + 196, DY0 + 98), 13, (214, 190, 140, 70), (150, 118, 58, 200), 1)
    ink(c, (DX0 + 164, DY0 + 85.5), '소모품', 13, GOLD_INK, True, 'mm')
    ink(c, (DX0 + 206, DY0 + 85.5), '보유 %d · 재고 20' % owned, 14.5, INK_MUTED, True, 'lm')
    ink(c, (DX0 + 20, DY0 + 136), desc, 16.5, INK, True)
    c.d.line(c.s(DX0 + 20, DY0 + 186, DX1 - 20, DY0 + 186), fill=CARD_LINE + (255,), width=SS)
    ink(c, (DX0 + 20, DY0 + 232), '수량', 16, INK_MUTED, True, 'lm')
    stepper(c, DX0 + 92, DY0 + 214, QTY)
    rr(c, (DX0 + 236, DY0 + 216, DX0 + 292, DY0 + 248), 10, (255, 255, 255, 0), CARD_LINE + (255,), 1)
    ink(c, (DX0 + 264, DY0 + 232.5), '최대', 14, INK_MUTED, True, 'mm')
    ink(c, (DX0 + 20, DY0 + 292), '합계', 16, INK_MUTED, True, 'lm')
    money(c, DX1 - 20, DY0 + 292, price * QTY, 26, INK)
    ink(c, (DX1 - 20, DY0 + 324), '구매 후 {:,}'.format(GOLD_HAVE - price * QTY), 14, INK_MUTED, True, 'rm')
    gold_button(c, (DX0 + 20, DY1 - 76, DX1 - 20, DY1 - 20), '구매하기', 'F')
    # footer hints
    hints = '↑↓ 고르기     ←→ 수량     F 구매     Q · E 탭     Esc 닫기'
    c.text(((X0 + X1) / 2, Y1 - 26), hints, 14, MUTED, True, 'mm', shadow=False)
    return c.done()


def option_a_sell(plate, icons, bag):
    """A's sell tab: the bag is inside the same window (no separate inventory window)."""
    c = Canvas(plate)
    X0, Y0, X1, Y1 = 300, 150, 1620, 936
    c.shadow((X0, Y0, X1, Y1), 18, alpha=0.55, blur=26, dy=10)
    rr(c, (X0, Y0, X1, Y1), 18, (14, 26, 40, 246), (214, 190, 140, 150), 1.4)
    c.diamond((X0 + 40, Y0 + 48), 5)
    c.text((X0 + 54, Y0 + 48), MERCHANT, 22, GOLD_HI, anchor='lm')
    c.text((X0 + 54 + 52, Y0 + 49), '· ' + SHOP_NAME, 16, MUTED, True, 'lm')
    c.text((X0 + 40, Y0 + 90), '“쓸 만한 게 있으면 사 줄게.”', 19, IVORY, True, 'lm')
    money(c, X1 - 40, Y0 + 72, GOLD_HAVE, 24, IVORY)
    ty = Y0 + 140
    tabs(c, X0 + 30, ty, ['구매', '판매'], 1, True)
    pills(c, X0 + 290, ty + 5, ['전체', '장비', '소모품', '재료', '기타'], 0, True)
    BX0, BY0, BX1, BY1 = X0 + 22, ty + 54, X1 - 22, Y1 - 52
    rr(c, (BX0, BY0, BX1, BY1), 14, PAPER + (255,), (214, 190, 140, 160), 1)
    # bag grid
    GX, GY, CELL = BX0 + 24, BY0 + 24, 82
    ink(c, (GX, GY + 4), '가방  12 / 30', 15, INK_MUTED, True)
    for k in range(30):
        r, col = divmod(k, 6)
        x, y = GX + col * (CELL + 8), GY + 32 + r * (CELL + 8)
        on = k == 0
        rr(c, (x, y, x + CELL, y + CELL), 10, (14, 26, 40, 255) if k < 12 else (14, 26, 40, 60), (176, 140, 70, 255) if on else None, 2.5 if on else 1)
        if k < len(bag):
            pix(c, bag[k], (x + 6, y + 6, x + CELL - 6, y + CELL - 6))
    # sell detail
    DX0, DX1 = GX + 6 * (CELL + 8) + 16, BX1 - 18
    DY0, DY1 = BY0 + 16, BY1 - 16
    rr(c, (DX0, DY0, DX1, DY1), 12, CARD + (255,), CARD_LINE + (255,), 1)
    rr(c, (DX0 + 20, DY0 + 20, DX0 + 112, DY0 + 112), 14, (14, 26, 40, 255))
    pix(c, icons[0], (DX0 + 26, DY0 + 24, DX0 + 106, DY0 + 108))
    ink(c, (DX0 + 132, DY0 + 34), '하급 체력 포션', 24, INK, False)
    ink(c, (DX0 + 132, DY0 + 85.5), '보유 5 · 개당 판매가', 14.5, INK_MUTED, True, 'lm')
    money(c, DX1 - 20, DY0 + 85.5, 20, 18, INK)
    c.d.line(c.s(DX0 + 20, DY0 + 136, DX1 - 20, DY0 + 136), fill=CARD_LINE + (255,), width=SS)
    ink(c, (DX0 + 20, DY0 + 182), '수량', 16, INK_MUTED, True, 'lm')
    stepper(c, DX0 + 92, DY0 + 164, 5)
    ink(c, (DX0 + 20, DY0 + 242), '받을 돈', 16, INK_MUTED, True, 'lm')
    money(c, DX1 - 20, DY0 + 242, 100, 26, INK)
    gold_button(c, (DX0 + 20, DY1 - 76, DX1 - 20, DY1 - 20), '판매하기', 'F')
    c.text(((X0 + X1) / 2, Y1 - 26), '방향키 칸 고르기     ←→ 수량     F 판매     Q · E 탭     Esc 닫기', 14, MUTED, True, 'mm', shadow=False)
    return c.done()


# ---------------------------------------------------------------- B. glass side sheet (HUD tone)
def option_b(plate, icons):
    c = Canvas(plate)
    # the merchant's line stays as the dialogue subtitle on the left
    W, H = c.W, c.H
    sc = Image.new('RGBA', (W * SS, H * SS), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sc)
    for y in range(780, H):
        t = (y - 780) / (H - 780)
        sd.line([(0, y * SS), (1180 * SS, y * SS)], fill=(9, 17, 27, int(255 * min(1, t * 1.3) * 0.86)), width=SS)
    c.layer = Image.alpha_composite(sc, c.layer)
    c.d = ImageDraw.Draw(c.layer)
    c.diamond((150, 918), 5)
    c.text((164, 918), MERCHANT, 24, GOLD_HI, anchor='lm')
    c.text((164, 966), LINE, 22, IVORY, True, 'lm')
    # sheet
    SX0, SY0, SX1 = 1200, 40, 1880
    SY1 = SY0 + 190 + len(ITEMS) * 72 + 270      # list, then the chosen item and the buy button right under it
    c.glass((SX0, SY0, SX1, SY1), r=18, alpha=0.88, line=0.18, sheen=False)
    c.diamond((SX0 + 34, SY0 + 46), 5)
    c.text((SX0 + 48, SY0 + 46), SHOP_NAME, 24, GOLD_HI, anchor='lm')
    money(c, SX1 - 34, SY0 + 46, GOLD_HAVE, 22, IVORY)
    tabs(c, SX0 + 26, SY0 + 86, ['구매', '판매'], 0, True, w=150)
    pills(c, SX0 + 26, SY0 + 142, ['전체', '소모품', '재료', '기타'], 0, True)
    y = SY0 + 190
    for i, (ic, name, desc, price, owned, lock) in enumerate(ITEMS):
        on = i == SEL
        h = 64
        rr(c, (SX0 + 22, y, SX1 - 22, y + h), 12, (22, 41, 61, 235) if on else (14, 26, 40, 160), (214, 190, 140, 200) if on else (241, 237, 230, 30), 1.4 if on else 1)
        if on:
            rr(c, (SX0 + 23, y + 12, SX0 + 27, y + h - 12), 2, GOLD_HI + (255,))
        rr(c, (SX0 + 36, y + 8, SX0 + 84, y + 56), 10, (6, 12, 20, 255))
        pix(c, icons[ic], (SX0 + 38, y + 10, SX0 + 82, y + 54))
        dim = lock > 0
        c.text((SX0 + 100, y + 22), name, 19, MUTED if dim else (GOLD_HI if on else IVORY), False, 'lm')
        c.text((SX0 + 100, y + 46), ('보유 %d' % owned) if owned else desc, 14, MUTED, True, 'lm', shadow=False)
        if dim:
            lock_chip(c, SX1 - 36, y + h / 2, lock, dark=True)
        else:
            money(c, SX1 - 38, y + h / 2, price, 18, IVORY)
        y += h + 8
    # detail + buy
    DY = SY0 + 190 + len(ITEMS) * 72 + 6
    c.d.line(c.s(SX0 + 26, DY, SX1 - 26, DY), fill=(241, 237, 230, 40), width=SS)
    ic, name, desc, price, owned, lock = ITEMS[SEL]
    c.text((SX0 + 30, DY + 34), name, 21, IVORY, anchor='lm')
    c.text((SX0 + 30, DY + 66), desc, 15.5, (205, 213, 219), True, 'lm', shadow=False)
    c.text((SX0 + 30, DY + 120), '수량', 16, MUTED, True, 'lm', shadow=False)
    stepper(c, SX0 + 92, DY + 102, QTY, dark=True)
    money(c, SX1 - 30, DY + 120, price * QTY, 24, IVORY)
    gold_button(c, (SX0 + 26, SY1 - 90, SX1 - 26, SY1 - 30), '구매하기', 'F')
    return c.done()


# ---------------------------------------------------------------- C. counter list (the conversation goes on)
def option_c(plate, icons):
    c = Canvas(plate)
    W, H = c.W, c.H
    sc = Image.new('RGBA', (W * SS, H * SS), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sc)
    for y in range(740, H):
        t = (y - 740) / (H - 740)
        a = t * t * (3 - 2 * t)
        sd.line([(0, y * SS), (W * SS, y * SS)], fill=(9, 17, 27, int(255 * min(.9, 0.06 + a * 0.95))), width=SS)
    c.layer = Image.alpha_composite(sc, c.layer)
    c.d = ImageDraw.Draw(c.layer)
    c.diamond((431, 869), 5.5)
    c.text((446, 869), MERCHANT, 26, GOLD_HI, anchor='lm')
    c.d.line(c.s(441, 892, 611, 892), fill=GOLD + (170,), width=int(2 * SS))
    c.text((441, 932), LINE, 25, IVORY, True, 'lm')
    # list where the choices were
    PX1, PW = 1660, 520
    PX0 = PX1 - PW
    rows = ITEMS
    rh, gap = 52, 6
    expanded = 44
    total = len(rows) * (rh + gap) + expanded
    PY1 = 818
    PY0 = PY1 - total
    # header chip: tabs + gold
    hy = PY0 - 54
    c.glass((PX0, hy, PX1, hy + 44), r=22, alpha=0.86, line=0.14, sheen=False)
    for i, (n, on) in enumerate((('구매', True), ('판매', False))):
        bx = PX0 + 8 + i * 92
        if on:
            rr(c, (bx, hy + 6, bx + 86, hy + 38), 16, (241, 237, 230, 235))
        c.text((bx + 43, hy + 23), n, 16, NAVY if on else IVORY, True, 'mm', shadow=False)
    money(c, PX1 - 22, hy + 22, GOLD_HAVE, 18, IVORY)
    y = PY0
    for i, (ic, name, desc, price, owned, lock) in enumerate(rows):
        on = i == SEL
        h = rh + (expanded if on else 0)
        c.glass((PX0, y, PX1, y + h), r=12, alpha=0.92 if on else 0.80, line=0.30 if on else 0.12, sheen=False)
        if on:
            rr(c, (PX0 + 1, y + 10, PX0 + 5, y + h - 10), 2, GOLD_HI + (255,))
            rr(c, (PX0, y, PX1, y + h), 12, None, GOLD + (170,), 1.2)
        rr(c, (PX0 + 14, y + 7, PX0 + 52, y + 45), 8, (6, 12, 20, 255))
        pix(c, icons[ic], (PX0 + 16, y + 8, PX0 + 50, y + 44))
        dim = lock > 0
        c.text((PX0 + 66, y + 26), name, 18.5, MUTED if dim else (GOLD_HI if on else IVORY), True, 'lm')
        if dim:
            lock_chip(c, PX1 - 14, y + 26, lock, dark=True)
        else:
            if owned:
                c.text((PX1 - 118, y + 26.5), '보유 %d' % owned, 14, MUTED, True, 'rm', shadow=False)
            money(c, PX1 - 18, y + 26, price, 17, IVORY)
        if on:
            stepper(c, PX0 + 66, y + 52, QTY, dark=True)
            c.text((PX0 + 214, y + 70), '합계', 14.5, MUTED, True, 'lm', shadow=False)
            money(c, PX0 + 330, y + 70, price * QTY, 18, GOLD_HI)
            keychip(c, PX1 - 108, y + 58, 'F')
            c.text((PX1 - 76, y + 70), '구매', 16, IVORY, True, 'lm', shadow=False)
        y += h + gap
    bd.keychip(c, (1540, 1002), 'Q')
    c.text((1572, 1014), '판매', 15, MUTED, True, 'lm', shadow=False)
    return c.done()


def uri(im):
    buf = io.BytesIO()
    im.convert('RGB').save(buf, 'JPEG', quality=88)
    return 'data:image/jpeg;base64,' + base64.b64encode(buf.getvalue()).decode()


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'png').mkdir(exist_ok=True)
    plate = Image.open(CAP / 'sh_shop_clean_view.png').convert('RGBA')
    icons, bag = load_icons()
    shots = {
        'now_talk': Image.open(CAP / 'sh_talk_view.png').convert('RGBA'),
        'now_shop': Image.open(CAP / 'sh_shop_view.png').convert('RGBA'),
        'now_page': Image.open(CAP / 'sh_page_view.png').convert('RGBA'),
        'a': option_a(plate, icons),
        'a_sell': option_a_sell(plate, icons, bag),
        'b': option_b(plate, icons),
        'c': option_c(plate, icons),
    }
    for k, im in shots.items():
        im.convert('RGB').save(OUT / 'png' / (k + '.png'))
    html = PAGE
    for k, im in shots.items():
        html = html.replace('%%%s%%' % k.upper(), uri(im.convert('RGB').resize((1280, 720), Image.LANCZOS)))
    (OUT / 'shop.html').write_text(html, encoding='utf-8')
    print('wrote', OUT / 'shop.html')


PAGE = '''<!doctype html><html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>상점창 시안</title><style>
:root{--bg:#0b1219;--panel:#111c27;--text:#e8e4dc;--sub:#9aa7b1;--gold:#d6be8c}
@media (prefers-color-scheme: light){:root:not([data-theme="dark"]){--bg:#eef0f2;--panel:#fff;--text:#1d2731;--sub:#5d6b77}}
:root[data-theme="light"]{--bg:#eef0f2;--panel:#fff;--text:#1d2731;--sub:#5d6b77}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.65 "Pretendard","Malgun Gothic",sans-serif}
main{max-width:1320px;margin:0 auto;padding:40px 24px 80px}h1{font-size:28px;margin:0 0 6px}h2{font-size:20px;margin:44px 0 8px}
.lead{color:var(--sub);max-width:900px;margin:0 0 14px}.card{background:var(--panel);border-radius:14px;padding:16px;border:1px solid rgba(128,140,150,.18);margin-bottom:18px}
.card h3{margin:0 0 8px;font-size:16px}.card p,.card li{color:var(--sub);font-size:14px}.card p{margin:8px 0 0}img{width:100%;display:block;border-radius:10px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:14px}@media (max-width:760px){.grid{grid-template-columns:1fr}}
.tag{font-size:12px;font-weight:700;padding:1px 8px;border-radius:99px;background:#2f4a3a;color:#bfe8c8;margin-left:6px}
.pick{outline:2px solid var(--gold);outline-offset:3px}ul{margin:6px 0 0;padding-left:18px}table{width:100%;border-collapse:collapse;font-size:14px}
td,th{padding:7px 8px;border-bottom:1px solid rgba(128,140,150,.2);text-align:left;vertical-align:top}th{color:var(--sub);font-weight:600}a{color:var(--gold)}
.cap{font-size:13px;color:var(--sub);margin-top:6px}
</style></head><body><main>
<h1>상점창 시안</h1>
<p class="lead">리베르그로브 상점 NPC 시안에게 말을 걸어 "상품 보기"로 연 실제 화면에서 출발했습니다. "지금"은 실제 게임 캡처,
시안은 같은 장면 1080p 위에 게임 크기로 그렸습니다. 아이템 아이콘·상인 얼굴은 게임 그림 그대로입니다. 에셋·코드는 바꾸지 않았습니다.</p>

<h2>지금</h2>
<div class="grid">
<div class="card"><img src="%NOW_TALK%"><div class="cap">대화 — 새 자막형 대화창, "상품 보기" 선택지</div></div>
<div class="card"><img src="%NOW_SHOP%"><div class="cap">게임이 실제로 여는 상점 (dbals WBP_ShopWindow + 옛 소지품 창)</div></div>
</div>
<div class="card"><img src="%NOW_PAGE%"><div class="cap">민기 님 상점 디자인 페이지 (MinkiCho WBP_ShopPage, nu.Shop.Preview — 게임 흐름엔 아직 연결 안 됨)</div>
<ul>
<li>실제 상점은 옛 남색 장식 창 + 따로 뜨는 소지품 창 두 개 — 닫기 X 둘, 소지금 두 번, 미니맵을 가림, 뒤에 채팅·하단 메뉴·F 키 표시가 그대로</li>
<li>잠긴 품목은 "Lv. 10 이상"이 이름·가격 위에 겹쳐 둘 다 안 읽힘</li>
<li>설명·보유 수·재고가 없고, 수량은 따로 뜨는 나누기 창에서</li>
<li>민기 님 상아 페이지는 방향이 좋지만(10/08 에 상점은 상아 바탕으로 되돌림) 미리보기 전용이고, 소지품이 따로 놀고, 상인 대사가 고정 문구</li>
<li>방금 바꾼 자막형 대화에서 넘어갈 때 톤이 끊김</li></ul></div>

<h2>다른 게임들 (플레이 기억 기준)</h2>
<div class="card"><table>
<tr><th>게임</th><th>상점 화면</th><th>배울 점</th></tr>
<tr><td>옥토패스 트래블러 I·II</td><td>왼쪽 품목·가격 목록, 아래 설명 줄, 장비는 오른쪽에 파티원별 능력치 비교, 위에 소지금</td><td>목록 + 고른 품목 설명을 한 화면에, 수량은 그 자리에서</td></tr>
<tr><td>드래곤 퀘스트 XI</td><td>품목 목록 + 보유 수 + 누가 장비할 수 있는지, 수량 선택 후 확인</td><td>보유 수를 목록에 바로</td></tr>
<tr><td>원신·현대 RPG</td><td>분류 탭 + 목록/격자 + 오른쪽 상세(설명·수량·합계·구매 버튼)</td><td>합계와 "구매 후 소지금"을 버튼 옆에</td></tr>
</table></div>

<h2>시안</h2>
<div class="card pick"><h3>A. 상아 상점 페이지 — 민기 님 디자인을 다듬어 실제로 연결<span class="tag">추천</span></h3><img src="%A%">
<p>10/08 에 되돌린 상아 바탕과 민기 님 페이지 구성을 그대로 잇고, 실제 흐름에 연결합니다. 남색 테 안에 상인 얼굴(게임 도트 그대로)과
방금 대화에서 한 실제 대사, 소지금. 상아 목록에는 설명·가격·보유 수, 잠긴 품목은 겹치지 않는 "Lv.10 부터" 자물쇠 칩.
오른쪽은 고른 품목 자세히 + 수량(−/+/최대) + 합계 + "구매 후" 소지금 + 금 구매 버튼. 아래 키 안내.</p>
<img src="%A_SELL%" style="margin-top:12px"><p>판매 탭은 같은 창 안에서 가방 칸을 보여 줍니다 — 따로 뜨는 소지품 창이 없어져 X·소지금이 하나로. (판매가 20 은 그림용 예시 — 실제는 상점 표의 되팔기율로 계산)</p></div>
<div class="card"><h3>B. 유리 사이드 시트 — HUD 톤</h3><img src="%B%">
<p>새 HUD·대화창과 같은 남색 유리. 장면과 상인을 왼쪽에 그대로 두고 상인 대사도 자막으로 남깁니다. 톤은 가장 일관되지만
상점만 상아로 되돌려 둔 결정과는 반대 방향입니다.</p></div>
<div class="card"><h3>C. 대화 이어지는 진열대 — 간단한 상점용</h3><img src="%C%">
<p>대화창의 선택지 자리에 품목 줄이 뜨고 고른 줄이 펼쳐져 그 자리에서 수량·구매. 상인 대사는 자막 그대로. 가장 가볍고
흐름이 끊기지 않지만, 품목이 많거나 장비 비교가 필요한 상점에는 좁습니다.</p></div>

<h2>어느 안이든 같이</h2>
<div class="card"><ul>
<li>상점이 열려 있는 동안 필드 HUD·F 키 표시 접기 (대화창과 같은 방식)</li>
<li>소지품 창을 따로 띄우지 않고 판매는 상점 안에서 — 닫기·소지금 하나</li>
<li>잠긴 품목은 자물쇠 칩, 품절은 "품절" 칩 — 이름·가격 위에 겹치지 않게</li>
<li>수량은 그 자리에서(−/+/최대, 패드 ←→), 합계와 구매 후 소지금 표시 — 나누기 창 없이</li>
<li>상인 얼굴·대사는 실제 대화한 NPC 것 (UNUShopPageWidget::SetMerchantActor 가 이미 있음)</li>
<li>A 로 가면 민기 님 WBP_ShopPage 작업과 겹치므로 민기 님과 나눠 맡을 범위를 먼저 정해야 함</li>
</ul></div>
</main></body></html>'''

if __name__ == '__main__':
    build()
