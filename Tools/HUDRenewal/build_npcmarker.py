# -*- coding: utf-8 -*-
"""NPC quest marker (the "!" over NPC heads) proposal, with the new key-only interaction cue.

Output: Docs/HUD_NpcMarker_Proposal_20261014/npc_marker.html (+ png/). Mock markers are drawn here (PIL) at game size over a real
1080p game frame with the head markers hidden in play (qa_hide_markers.py; the real new F cue stays), shown 2x; option A reuses the real minimap badges (make_hud_kit.py).
Plates: Tools/CharacterCreateGlass/run.ps1 -Steps shot -Map /Game/Libregrove/Level_V8/Maps/L_Libregrove -States Tools/HUDRenewal/qa/states_npc_clean.json
Run: python Tools/HUDRenewal/build_npcmarker.py
"""
import base64
import io
import pathlib

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
KIT = ROOT / 'ArtSource' / 'UI' / 'HUD2026' / 'kit'
NOW = ROOT / 'Saved' / 'HUDRenewal' / 'npc_markers'
OUT = ROOT / 'Docs' / 'HUD_NpcMarker_Proposal_20261014'
FONT = ROOT / 'Saved' / 'HUDRenewal' / 'fonts' / 'MaruBuri-Bold.ttf'
SS = 4
NAVY = (22, 41, 61)


def uri(im, fmt='PNG'):
    buf = io.BytesIO()
    im.save(buf, fmt)
    return 'data:image/%s;base64,%s' % (fmt.lower(), base64.b64encode(buf.getvalue()).decode())


# ---------------------------------------------------------------- B. clean pixel bubble (16 x 16 art, drawn at whole multiples)
OUTLINE, FILL, SHADE, LIGHT = (58, 38, 20), (255, 247, 226), (234, 214, 170), (255, 255, 255)
GLYPHS = {
    '!': (['.XX.', '.XX.', '.XX.', '.XX.', '....', '.XX.'], (226, 160, 40), (150, 92, 20)),
    '?': (['XXXX', '...X', '.XXX', '.X..', '....', '.X..'], (226, 160, 40), (150, 92, 20)),
    '.': (['X.X.X'], (120, 132, 148), (70, 80, 96)),
    '*': (['..X..', '.XXX.', 'XXXXX', '.XXX.', '..X..'], (70, 196, 184), (24, 110, 104)),
}


def pixel_bubble(glyph, scale=3, tint=None):
    """16 x 16: a rounded speech bubble with a 1-art-pixel outline, a light top-left, a shaded bottom row and a tail."""
    W = H = 16
    im = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    px = im.load()
    fill = tint or FILL
    shade = tuple(max(0, int(c * 0.9)) for c in fill) if tint else SHADE
    # body 0..15 x 0..11 with cut corners (two-pixel round), tail at x 6..8 rows 12..14
    for y in range(12):
        for x in range(W):
            cut = (y in (0, 11) and (x < 2 or x > 13)) or (y in (1, 10) and (x < 1 or x > 14))
            if cut:
                continue
            edge = (y in (0, 11)) or (x in (0, 15)) or (y in (1, 10) and x in (1, 14))
            px[x, y] = OUTLINE + (255,) if edge else (shade if y == 10 else fill) + (255,)
    for (x, y) in ((6, 12), (7, 12), (8, 12), (6, 13), (7, 13), (7, 14)):
        px[x, y] = OUTLINE + (255,)
    px[7, 12] = fill + (255,)
    px[2, 2] = LIGHT + (255,); px[3, 2] = LIGHT + (255,); px[2, 3] = LIGHT + (255,)
    rows, color, dark = GLYPHS[glyph]
    gw, gh = len(rows[0]), len(rows)
    ox, oy = (W - gw) // 2, 2 + (8 - gh) // 2 + (1 if gh < 6 else 0)
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch == 'X':
                px[ox + i, oy + j] = (dark if j == gh - 1 and glyph in '!?' else color) + (255,)
    return im.resize((W * scale, H * scale), Image.NEAREST)


# ---------------------------------------------------------------- C. floating crest (vector glyph, no bubble)
def crest(glyph, size=56, top=(255, 226, 140), bottom=(214, 150, 40), rim=NAVY):
    S = size * SS
    mask = Image.new('L', (S, S), 0)
    d = ImageDraw.Draw(mask)
    font = ImageFont.truetype(str(FONT), int(S * 0.86))
    box = d.textbbox((0, 0), glyph, font=font)
    w, h = box[2] - box[0], box[3] - box[1]
    d.text(((S - w) / 2 - box[0], (S - h) / 2 - box[1]), glyph, font=font, fill=255)
    outline = mask.filter(ImageFilter.MaxFilter(int(SS * 3) * 2 + 1))
    grad = Image.new('RGBA', (S, S))
    gd = ImageDraw.Draw(grad)
    for y in range(S):
        t = y / S
        gd.line([(0, y), (S, y)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,))
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    im.paste(Image.new('RGBA', (S, S), rim + (255,)), (0, 0), outline)
    im.paste(grad, (0, 0), mask)
    sheen = Image.new('L', (S, S), 0)
    ImageDraw.Draw(sheen).rectangle([0, 0, S * 0.48, S], fill=60)
    im.paste(Image.new('RGBA', (S, S), (255, 255, 255, 255)), (0, 0), Image.composite(sheen, Image.new('L', (S, S), 0), mask))
    im = im.resize((size, size), Image.LANCZOS)
    glow = Image.new('RGBA', im.size, (255, 214, 120, 0))
    glow.putalpha(im.split()[3].filter(ImageFilter.GaussianBlur(5)).point(lambda a: int(a * 0.7)))
    return Image.alpha_composite(glow, im)


def kit(name, size):
    return Image.open(KIT / (name + '.png')).convert('RGBA').resize((size, size), Image.LANCZOS)


def ring(size, colour=(244, 215, 143), alpha=0.7):
    S = size * SS
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(im).ellipse([SS * 3, SS * 3, S - SS * 3, S - SS * 3], outline=colour + (int(255 * alpha),), width=int(2 * SS))
    return im.resize((size, size), Image.LANCZOS)


CAP = ROOT / 'Saved' / 'CharacterCreateGlass'
MARK = (820, 518)                       # the quest marker's centre in the 1080p captures (qa_interact.py pose)
WIN = (MARK[0] - 210, MARK[1] - 190, MARK[0] + 210, MARK[1] + 150)


def plate(name):
    """A real 1080p game frame (pie_capture), cropped around the dwarf, drawn 2x so the marker is easy to judge."""
    im = Image.open(CAP / (name + '_view.png')).convert('RGBA').crop(WIN)
    return im


def scene(marker):
    """The clean frame (all head markers hidden in play, the real new F cue kept) with a mock marker where the old one was."""
    base = plate('npc_clean')
    base.alpha_composite(marker, (MARK[0] - WIN[0] - marker.width // 2, MARK[1] - WIN[1] - marker.height // 2))
    return base


def x2(im):
    return im.resize((im.width * 2, im.height * 2), Image.NEAREST)


def zoom_row(images, scale=3, bg=(118, 146, 88)):
    W = sum(i.width * scale + 30 for i in images) + 30
    H = max(i.height for i in images) * scale + 60
    out = Image.new('RGBA', (W, H), bg + (255,))
    x = 30
    for im in images:
        big = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
        out.alpha_composite(big, (x, (H - big.height) // 2))
        x += big.width + 30
    return out


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'png').mkdir(exist_ok=True)
    now = plate('npc_near')

    # A: the minimap diamond badges, 34 px, soft glow
    a_set = [kit('T_HUD_Badge_Offer', 52), kit('T_HUD_Badge_TurnIn', 52), kit('T_HUD_Badge_InProgress', 52), kit('T_HUD_Badge_Target', 52)]
    # B: clean pixel bubble, 16 px art at exactly 3x
    b_set = [pixel_bubble('!'), pixel_bubble('?'), pixel_bubble('.'), pixel_bubble('*')]
    # C: floating crest
    c_set = [crest('!', 62), crest('?', 62), crest('…', top=(206, 214, 222), bottom=(130, 142, 156)), crest('◆', 44, top=(160, 240, 230), bottom=(40, 160, 150))]

    shots = {
        'now': x2(now),
        'a': x2(scene(a_set[0])), 'b': x2(scene(b_set[0])), 'c': x2(scene(c_set[0])),
        'a_zoom': zoom_row(a_set, 2), 'b_zoom': zoom_row([pixel_bubble(g, 1) for g in '!?.*'], 9), 'c_zoom': zoom_row(c_set, 2),
        'now_zoom': zoom_row([Image.open(NOW / (n + '.png')).convert('RGBA') for n in ('T_NPCQuest_Request', 'T_NPCQuest_TurnIn', 'T_NPCQuest_InProgress')], 3),
        'fk': Image.open(CAP / 'it_key_view.png').convert('RGBA').crop((760, 381, 880, 501)).resize((360, 360), Image.LANCZOS),
    }
    for k, im in shots.items():
        im.save(OUT / 'png' / ('%s.png' % k))
    html = PAGE
    for k, im in shots.items():
        html = html.replace('%%%s%%' % k.upper(), uri(im))
    out = OUT / 'npc_marker.html'
    out.write_text(html, encoding='utf-8')
    print('wrote', out)


PAGE = '''<!doctype html><html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>NPC 퀘스트 표시 시안</title><style>
:root{--bg:#0b1219;--panel:#111c27;--text:#e8e4dc;--sub:#9aa7b1;--gold:#d6be8c}
@media (prefers-color-scheme: light){:root:not([data-theme="dark"]){--bg:#eef0f2;--panel:#fff;--text:#1d2731;--sub:#5d6b77}}
:root[data-theme="light"]{--bg:#eef0f2;--panel:#fff;--text:#1d2731;--sub:#5d6b77}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.65 "Pretendard","Malgun Gothic",sans-serif}
main{max-width:1180px;margin:0 auto;padding:40px 24px 80px}h1{font-size:28px;margin:0 0 6px}h2{font-size:20px;margin:48px 0 8px}
.lead{color:var(--sub);max-width:860px;margin:0 0 16px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(330px,1fr));gap:18px}
.card{background:var(--panel);border-radius:14px;padding:16px;border:1px solid rgba(128,140,150,.18)}.card h3{margin:0 0 8px;font-size:16px}
.card p{margin:8px 0 0;color:var(--sub);font-size:14px}img{max-width:100%;display:block;border-radius:10px;image-rendering:auto}
img.px{image-rendering:pixelated}.tag{font-size:12px;font-weight:700;padding:1px 8px;border-radius:99px;background:#2f4a3a;color:#bfe8c8;margin-left:6px}
.pick{outline:2px solid var(--gold);outline-offset:3px}ul{margin:6px 0 0;padding-left:18px;color:var(--sub);font-size:14px}
code{font:13px Consolas,monospace;background:rgba(128,140,150,.15);padding:1px 5px;border-radius:5px}
</style></head><body><main>
<h1>NPC 머리 위 퀘스트 표시 · 상호작용 키</h1>
<p class="lead">지금 "!" 가 깨져 보이는 이유와 바꿀 모양 세 가지입니다. 위 그림은 실제 게임 화면(1080p, 머리 위 표시만 끄고 찍음)에 게임 크기로 얹어 2배로 보여 줍니다. 아래 줄은 4가지 상태(받을 퀘스트 / 보고 / 진행 중 / 목표)를 확대한 것입니다.
상호작용 안내는 요청대로 <b>글자를 빼고 F 키만</b> 남겨 이미 게임에 넣었습니다(위 그림의 F 는 실제 게임 화면).</p>

<h2>지금</h2>
<div class="grid">
<div class="card"><h3>게임 화면</h3><img src="%NOW%"><p>48px 도트 그림을 화면에서 약 1.35배(정수 배가 아님)로 늘려, 칸마다 1~2px 로 들쭉날쭉 그려집니다.</p></div>
<div class="card"><h3>원본 그림 (48px, 3배 확대)</h3><img class="px" src="%NOW_ZOOM%">
<ul><li>월드 공간 위젯을 마스크 재질로 그려 가장자리가 계단처럼 끊기고, 화면 흐림(TSR·초점)을 그대로 받아 번집니다</li>
<li>받을 퀘스트 노란 말풍선 / 보고 초록 체크 / 진행 중 파란 점 --- 색이 제각각이고 HUD·미니맵 표시와 모양이 다릅니다</li></ul></div>
</div>

<h2>시안</h2>
<div class="grid">
<div class="card pick"><h3>A. HUD 마름모 배지<span class="tag">추천</span></h3><img src="%A%"><img src="%A_ZOOM%" style="margin-top:10px">
<p>미니맵에 넣은 배지와 <b>같은 그림</b>(금 ! / 밝은 금 ? / 회청 … / 청록 목표). 지도에서 본 표시가 머리 위에도 그대로 떠서 한눈에 이어집니다.
벡터라 크기와 상관없이 매끈하고, 위아래로 천천히 떠다니며 보고 가능일 때만 고리가 퍼집니다.</p></div>
<div class="card"><h3>B. 정돈된 도트 말풍선</h3><img src="%B%"><img class="px" src="%B_ZOOM%" style="margin-top:10px">
<p>지금 말풍선을 16px 도트로 다시 그려 <b>정확히 3배</b>로만 그립니다. 외곽선이 늘 1칸이라 캐릭터 도트와 같은 결로 보입니다.
도트 세계와 가장 잘 섞이지만 HUD·미니맵과는 모양이 다릅니다.</p></div>
<div class="card"><h3>C. 떠 있는 문장</h3><img src="%C%"><img src="%C_ZOOM%" style="margin-top:10px">
<p>말풍선 없이 금빛 "!" 하나만, 남색 테두리와 빛. 가장 간결하고 멀리서도 잘 보이지만 상태 구분이 글자 모양에만 기댑니다.</p></div>
</div>

<h2>상호작용 키 (적용함)</h2>
<div class="grid"><div class="card"><h3>F 키만 · 빛 · 퍼지는 고리 (게임 화면 3배)</h3><img src="%FK%">
<p>"상호작용" 글자를 지웠습니다. 남색 테두리 안 금빛 키, 키 위로 지나가는 반짝임, 1.5초마다 두 겹으로 퍼지는 상아 고리(바깥에 남색 그늘이 붙어 밝은 잔디 위에서도 보임), 나타날 때 톡 튀어나옵니다. 키는 입력 설정을 따라 바뀝니다(F를 바꾸면 그 키).</p></div></div>

<h2>어떤 안을 고르든 함께 고칠 것</h2>
<div class="card"><ul>
<li>그리는 방식: 마스크 대신 가장자리를 부드럽게 섞는 반투명 + 화면 흐림을 거치지 않는 재질(캐릭터 스프라이트와 같은 방식). 도트안(B)이면 화면 크기를 정수 배로 고정</li>
<li>떠다니기(위아래 3px, 2.4초), 처음 보일 때 톡 튀어나오기, 상호작용 키가 뜨면 표시를 조금 흐리게 해 둘이 겹쳐 보이지 않게</li>
<li>미니맵 배지와 같은 상태 이름(받을 / 보고 / 진행 중 / 목표 / 조건 미달)을 그대로 씀 --- 판정 코드는 그대로</li>
</ul></div>
</main></body></html>'''

if __name__ == '__main__':
    build()
