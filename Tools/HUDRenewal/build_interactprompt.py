# -*- coding: utf-8 -*-
"""Interaction cue (F) placement proposal: the cue sits on top of the quest badge and pulses like the quest effects.

Output: Docs/HUD_Interact_Proposal_20261014/interact_prompt.html (+ png/). Mocks are drawn here (PIL) at game size over a real
1080p frame where only the F cue is hidden in play (qa_hide_prompt.py; the real head marker stays), shown 2x.
Plates: Tools/CharacterCreateGlass/run.ps1 -Steps shot -Map /Game/Libregrove/Level_V8/Maps/L_Libregrove -States Tools/HUDRenewal/qa/states_interact_plate.json
Run: python Tools/HUDRenewal/build_interactprompt.py
"""
import base64
import io
import pathlib

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CAP = ROOT / 'Saved' / 'CharacterCreateGlass'
OUT = ROOT / 'Docs' / 'HUD_Interact_Proposal_20261014'
FONT = ROOT / 'Saved' / 'HUDRenewal' / 'fonts' / 'MaruBuri-Bold.ttf'
SS = 4
NAVY = (22, 41, 61)
IVORY = (241, 237, 230)
GOLD = (214, 190, 140)

# places in the 1080p plate (set from the capture)
BADGE = (820, 505)        # head marker diamond centre
BADGE_R = 42              # its half size on screen
FEET = (840, 722)         # the NPC's feet
PLAYER = (957, 640)       # the player's chest
WIN = (600, 360, 1240, 780)


def uri(im):
    buf = io.BytesIO()
    im.save(buf, 'PNG')
    return 'data:image/png;base64,' + base64.b64encode(buf.getvalue()).decode()


def font(px):
    return ImageFont.truetype(str(FONT), int(px))


def keycap(size=30, pressed=0.0, fill=IVORY, ink=NAVY, rim=NAVY, label='F'):
    """The interaction key: a neutral ivory keycap (the quest marks are gold), navy rim, a lip under it that shrinks when it
    is pressed --- the motion is a key being pushed, not a ring or a glow, so it never reads as a quest effect."""
    pad = 8
    W = size + pad * 2
    S = W * SS
    im = Image.new('RGBA', (S, S + 4 * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    x0, y0 = pad * SS, pad * SS
    x1, y1 = x0 + size * SS, y0 + size * SS
    lip = (1 - pressed) * 3.5 * SS
    push = pressed * 3.0 * SS
    r = 7 * SS
    shadow = Image.new('RGBA', im.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle([x0, y0 + 4 * SS, x1, y1 + 4 * SS], r, fill=(4, 10, 18, 120))
    im = Image.alpha_composite(im, shadow.filter(ImageFilter.GaussianBlur(3 * SS)))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([x0, y0 + push, x1, y1 + lip + push], r, fill=rim + (255,))                       # rim + lip
    d.rounded_rectangle([x0 + 2.2 * SS, y0 + 2.2 * SS + push, x1 - 2.2 * SS, y1 - 2.2 * SS + push], r - 2 * SS,
                        fill=tuple(int(c * (1 - 0.10 * pressed)) for c in fill) + (255,))
    d.rounded_rectangle([x0 + 4 * SS, y0 + 3.6 * SS + push, x1 - 4 * SS, y0 + size * 0.42 * SS + push], r - 3 * SS,
                        fill=(255, 255, 255, int(110 * (1 - pressed))))
    f = font(size * 0.58 * SS)
    box = d.textbbox((0, 0), label, font=f)
    w, h = box[2] - box[0], box[3] - box[1]
    d.text(((S - w) / 2 - box[0], y0 + (size * SS - h) / 2 - box[1] + push - 0.4 * SS), label, font=f, fill=ink + (255,))
    return im.resize((W, (S + 4 * SS) // SS), Image.LANCZOS)


def ground_ring(w=92, h=26, alpha=1.0):
    """B: a flat ellipse on the ground around the NPC's feet (lies on the floor --- nothing like the badge's rising rings)."""
    S = SS
    im = Image.new('RGBA', ((w + 20) * S, (h + 20) * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    box = [10 * S, 10 * S, (10 + w) * S, (10 + h) * S]
    d.ellipse(box, fill=(255, 250, 236, int(60 * alpha)))
    d.ellipse(box, outline=(255, 250, 236, int(230 * alpha)), width=int(3.0 * S))
    inner = [box[0] + 9 * S, box[1] + 3 * S, box[2] - 9 * S, box[3] - 3 * S]
    d.ellipse(inner, outline=(255, 250, 236, int(70 * alpha)), width=int(1.4 * S))
    im = im.resize((w + 20, h + 20), Image.LANCZOS)
    return im


def list_panel(rows, sel=0, slide=0.0, alpha=1.0):
    """C: a screen-anchored row beside the player, like Genshin's interact list: key + name; several targets stack."""
    rw, rh, gap = 196, 40, 4
    W, H = rw + 24, len(rows) * (rh + gap) + 20
    S = SS
    im = Image.new('RGBA', (W * S, H * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for i, name in enumerate(rows):
        y = (10 + i * (rh + gap)) * S
        x = (12 + slide) * S
        on = i == sel
        d.rounded_rectangle([x, y, x + rw * S, y + rh * S], 10 * S, fill=(14, 26, 40, int((236 if on else 190) * alpha)))
        if on:
            d.rounded_rectangle([x, y, x + rw * S, y + rh * S], 10 * S, outline=(214, 190, 140, int(200 * alpha)), width=int(1.4 * S))
        f = font(17 * S)
        d.text((x + 50 * S, y + 9.5 * S), name, font=f, fill=IVORY + (int((255 if on else 170) * alpha),))
    im = im.resize((W, H), Image.LANCZOS)
    for i, _ in enumerate(rows):
        if i == sel:
            k = keycap(26)
            k.putalpha(k.split()[3].point(lambda a: int(a * alpha)))
            im.alpha_composite(k, (int(12 + slide + 2), 10 + i * (rh + gap) + (rh - k.height) // 2 + 1))
    return im


def plate(name):
    return Image.open(CAP / (name + '_view.png')).convert('RGBA').crop(WIN)


def place(base, sprite, centre):
    base.alpha_composite(sprite, (int(centre[0] - WIN[0] - sprite.width / 2), int(centre[1] - WIN[1] - sprite.height / 2)))


def x2(im):
    return im.resize((im.width * 2, im.height * 2), Image.NEAREST)


def strip(frames, bg=(46, 62, 52), scale=3):
    W = sum(f.width * scale + 24 for f in frames) + 24
    H = max(f.height for f in frames) * scale + 40
    out = Image.new('RGBA', (W, H), bg + (255,))
    x = 24
    for f in frames:
        big = f.resize((f.width * scale, f.height * scale), Image.LANCZOS)
        out.alpha_composite(big, (x, (H - big.height) // 2))
        x += big.width + 24
    return out


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'png').mkdir(exist_ok=True)
    now = Image.open(CAP / 'hm_near_view.png').convert('RGBA').crop(WIN)
    clean = plate('ip_clean2')

    a = clean.copy()           # A: docked to the badge's lower right corner
    place(a, keycap(32), (BADGE[0] + BADGE_R * 0.84, BADGE[1] + BADGE_R * 0.62))
    b = clean.copy()           # B: at the feet
    place(b, ground_ring(112, 30), (FEET[0] + 4, FEET[1] + 2))
    place(b, keycap(32), (FEET[0] + 70, FEET[1] - 16))
    c = clean.copy()           # C: screen-anchored row beside the player
    panel = list_panel(['엘라돈'])
    place(c, panel, (PLAYER[0] + 40 + panel.width / 2, PLAYER[1] - 10))
    c2 = clean.copy()
    panel2 = list_panel(['엘라돈', '나무 상자'], sel=0)
    place(c2, panel2, (PLAYER[0] + 40 + panel2.width / 2, PLAYER[1] + 4))

    frames_key = strip([keycap(32, 0.0), keycap(32, 1.0), keycap(32, 0.0)])
    frames_ring = strip([ground_ring(alpha=0.55), ground_ring(alpha=1.0), ground_ring(alpha=0.55)], scale=2)
    frames_list = strip([list_panel(['엘라돈'], slide=14, alpha=0.3), list_panel(['엘라돈'], slide=5, alpha=0.8),
                         list_panel(['엘라돈'])], scale=2)
    shots = {'now': x2(now), 'a': x2(a), 'b': x2(b), 'c': x2(c), 'c2': x2(c2),
             'fkey': frames_key, 'fring': frames_ring, 'flist': frames_list}
    for k, im in shots.items():
        im.save(OUT / 'png' / (k + '.png'))
    html = PAGE
    for k, im in shots.items():
        html = html.replace('%%%s%%' % k.upper(), uri(im))
    (OUT / 'interact_prompt.html').write_text(html, encoding='utf-8')
    print('wrote', OUT / 'interact_prompt.html')


PAGE = '''<!doctype html><html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>상호작용 키 시안</title><style>
:root{--bg:#0b1219;--panel:#111c27;--text:#e8e4dc;--sub:#9aa7b1;--gold:#d6be8c}
@media (prefers-color-scheme: light){:root:not([data-theme="dark"]){--bg:#eef0f2;--panel:#fff;--text:#1d2731;--sub:#5d6b77}}
:root[data-theme="light"]{--bg:#eef0f2;--panel:#fff;--text:#1d2731;--sub:#5d6b77}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.65 "Pretendard","Malgun Gothic",sans-serif}
main{max-width:1180px;margin:0 auto;padding:40px 24px 80px}h1{font-size:28px;margin:0 0 6px}h2{font-size:20px;margin:44px 0 8px}
.lead{color:var(--sub);max-width:880px;margin:0 0 14px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(330px,1fr));gap:18px}
.card{background:var(--panel);border-radius:14px;padding:16px;border:1px solid rgba(128,140,150,.18)}.card h3{margin:0 0 8px;font-size:16px}
.card p,.card li{color:var(--sub);font-size:14px}.card p{margin:8px 0 0}img{max-width:100%;display:block;border-radius:10px}
.tag{font-size:12px;font-weight:700;padding:1px 8px;border-radius:99px;background:#2f4a3a;color:#bfe8c8;margin-left:6px}
.pick{outline:2px solid var(--gold);outline-offset:3px}ul{margin:6px 0 0;padding-left:18px}table{width:100%;border-collapse:collapse;font-size:14px}
td,th{padding:7px 8px;border-bottom:1px solid rgba(128,140,150,.2);text-align:left;vertical-align:top}th{color:var(--sub);font-weight:600}
a{color:var(--gold)}
</style></head><body><main>
<h1>상호작용 키(F) 자리·효과 시안</h1>
<p class="lead">지금은 F 키가 퀘스트 마름모 바로 위에 쌓이고, 둘 다 퍼지는 고리·빛을 가져 같은 계열의 연출로 보입니다.
원칙을 둘로 나눴습니다: <b>머리 위 = 이 NPC 의 상태(퀘스트)</b>, <b>F = 지금 누를 수 있는 행동</b> --- 자리와 색·움직임을 갈라 둘이 섞이지 않게.
그림은 실제 게임 화면(1080p, F 만 끄고 찍음)에 게임 크기로 얹어 2배로 보여 줍니다.</p>

<h2>지금</h2>
<div class="grid"><div class="card"><h3>F 가 배지 위에 쌓임</h3><img src="%NOW%">
<ul><li>세로로 길게 쌓여 NPC 머리 위가 무거워짐, 배지와 F 의 고리가 겹침</li>
<li>F 도 금빛·고리·빛무리 --- 퀘스트 표시·퀘스트 띠와 같은 언어라 "누르라"는 신호가 따로 읽히지 않음</li></ul></div></div>

<h2>다른 게임들</h2>
<div class="card"><table>
<tr><th>게임</th><th>상호작용 표시</th><th>배울 점</th></tr>
<tr><td>옥토패스 트래블러</td><td>범위 안에 들어가면 대상 가까이 작은 상자에 버튼(+쓸 수 있는 행동 아이콘)만. 세이브 지점은 늘 떠 있는 깃펜 아이콘(상태)이라 범위 표시가 따로 없어 아쉽다는 평</td><td>상태 아이콘과 "누를 수 있음" 표시는 별개여야 함</td></tr>
<tr><td>드래곤 퀘스트 XI</td><td>머리 위 말풍선은 색으로 역할 구분(보라 = 의뢰인, 분홍 = 단서) --- 말 걸기 표시와 섞지 않음</td><td>머리 위는 상태 전용</td></tr>
<tr><td>원신</td><td>화면 가운데 오른쪽에 고정된 줄 "[F] 이름", 여러 대상이면 목록·휠로 고름. 머리 위 퀘스트 표시와 자리가 아예 다름</td><td>대상이 여럿(NPC·상자·문)일 때 강함</td></tr>
<tr><td>하데스</td><td>대상의 발치 쪽에 버튼 + 짧은 글자, 대상 자체는 밝아짐</td><td>머리(상태)와 발(행동)로 위아래를 가름</td></tr>
<tr><td>할로우 나이트</td><td>대상 위에 작은 글자 표지가 서서히 떠오름, 버튼 그림 없음</td><td>움직임은 조용하게 --- 퍼지는 효과 없음</td></tr>
</table></div>

<h2>시안</h2>
<div class="grid" style="grid-template-columns:repeat(auto-fit,minmax(520px,1fr))">
<div class="card pick"><h3>A. 배지에 붙는 키 칩<span class="tag">추천</span></h3><img src="%A%"><img src="%FKEY%" style="margin-top:10px">
<p>F 를 마름모 오른쪽 아래 모서리에 작게 붙임 --- 위로 쌓이지 않아 머리 위 높이가 그대로. 퀘스트가 없는 NPC·물건은 그 자리에 키만.
색은 <b>상아(중립)</b>, 퀘스트는 금. 효과는 고리·빛 대신 <b>키가 눌렸다 올라오는 움직임</b>(1.6초마다, 아래 3칸) --- "누르세요"가 바로 읽힘.
옥토패스식 "대상 옆 작은 버튼". 구현이 가장 작음(지금 F 자리 계산을 배지 모서리로 옮기기만).</p></div>
<div class="card"><h3>B. 발밑 표시</h3><img src="%B%"><img src="%FRING%" style="margin-top:10px">
<p>NPC 발밑 바닥에 납작한 타원 + 그 옆에 키. 머리 = 퀘스트, 발 = 행동으로 위아래가 확실히 갈림(하데스식).
바닥 타원은 숨 쉬듯 밝아졌다 어두워질 뿐 퍼지지 않음. 다만 울타리·소품이 발을 가리는 자리에선 덜 보이고, 바닥 데칼(또는 평면)이 하나 더 필요.</p></div>
<div class="card"><h3>C. 화면 고정 줄</h3><img src="%C%"><img src="%FLIST%" style="margin-top:10px">
<p>플레이어 오른쪽에 화면 고정 줄 "[F] 엘라돈" (원신식). 월드 위 표시와 절대 겹치지 않고, 대상 이름까지 알려 줌.
대상이 여럿이면 줄이 쌓이고 휠·방향키로 고름(아래). 대신 화면 UI 가 하나 늘고, HD-2D 화면에선 조금 "온라인 게임" 느낌.</p>
<img src="%C2%" style="margin-top:10px"><p>대상 두 개(NPC + 상자)일 때.</p></div>
</div>

<h2>어느 안이든 같이</h2>
<div class="card"><ul>
<li>F 는 금빛·고리·빛무리를 쓰지 않음 --- 퀘스트 표시·퀘스트 띠의 언어(금, 퍼짐, 반짝임)와 갈라 둠</li>
<li>나타날 때는 짧게 톡(0.2초), 머무는 동안은 키 눌림만 --- 화면이 조용해짐</li>
<li>패드면 키 대신 패드 버튼 그림(같은 칩 모양)</li>
</ul></div>
<p class="lead" style="margin-top:28px">참고: <a href="https://www.ramblingaboutgames.com/blog/octopath-uiux">Octopath Traveler UI/UX Review</a> ·
<a href="https://www.gameuidatabase.com/gameData.php?id=741">Game UI Database: Octopath Traveler</a> ·
<a href="https://gamefaqs.gamespot.com/ps4/179347-dragon-quest-xi-echoes-of-an-elusive-age/faqs/76388/walkthrough-part-i">DQXI 말풍선 색 (GameFAQs)</a>.
원신·하데스·할로우 나이트는 플레이 기억 기준 설명.</p>
</main></body></html>'''

if __name__ == '__main__':
    build()
