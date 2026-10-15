# -*- coding: utf-8 -*-
"""NPC dialogue UI proposal, in the HUD's glass tone (navy glass, ivory text, gold accents, diamond motif).

Output: Docs/HUD_Dialogue_Proposal_20261014/dialogue.html (+ png/). The current UI is a real game frame (dlg_open); the mocks are
drawn here (PIL) at 1080p over a real frame captured without screen UI (dlg_plate, scene + head marker only, the way the field HUD
would fold away during a conversation).
Plates: Tools/CharacterCreateGlass/run.ps1 -Steps shot -Map /Game/Libregrove/Level_V8/Maps/L_Libregrove -States Tools/HUDRenewal/qa/states_dialogue_plate.json
Run: python Tools/HUDRenewal/build_dialogue.py
"""
import base64
import io
import json
import pathlib

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CAP = ROOT / 'Saved' / 'CharacterCreateGlass'
OUT = ROOT / 'Docs' / 'HUD_Dialogue_Proposal_20261014'
FONTS = ROOT / 'Saved' / 'HUDRenewal' / 'fonts'
KIT = ROOT / 'ArtSource' / 'UI' / 'HUD2026' / 'kit'
ART = ROOT / 'Saved' / 'HUDRenewal' / 'npc_art'
SS = 2
NAVY = (13, 24, 37)
IVORY = (241, 237, 230)
GOLD = (214, 190, 140)
GOLD_HI = (244, 215, 143)
MUTED = (170, 182, 191)

NAME = '엘라던'
LINE = '그대가 하는 일을 모두 숲의 정령들이 속삭여주었다. 그대의 손에 여섯 정령의 기운이 고였으니, 외지인이여. 이제 단상으로 오라.'
CHOICES = [('quest', '단상에 오르는 자 (권장 레벨 20)'), ('talk', '대화하기'), ('leave', '나가기')]
NPC_HEAD = (822, 572)          # top of the dwarf's head in the 1080p plate (dlg_plate)
MARKER_TOP = 438               # top of the head marker ring above it


def font(px, semi=False):
    return ImageFont.truetype(str(FONTS / ('MaruBuri-SemiBold.ttf' if semi else 'MaruBuri-Bold.ttf')), int(px * SS))


def uri(im):
    buf = io.BytesIO()
    im.save(buf, 'PNG')
    return 'data:image/png;base64,' + base64.b64encode(buf.getvalue()).decode()


class Canvas:
    """Draws at SS x on a transparent layer the size of the frame, then downsamples onto the plate."""

    def __init__(self, base):
        self.base = base.convert('RGBA')
        self.W, self.H = self.base.size
        self.layer = Image.new('RGBA', (self.W * SS, self.H * SS), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.layer)

    def s(self, *v):
        return [x * SS for x in v]

    def shadow(self, box, r, alpha=0.45, blur=18, dy=6):
        sh = Image.new('RGBA', self.layer.size, (0, 0, 0, 0))
        x0, y0, x1, y1 = box
        ImageDraw.Draw(sh).rounded_rectangle(self.s(x0, y0 + dy, x1, y1 + dy), r * SS, fill=(2, 6, 12, int(255 * alpha)))
        self.layer = Image.alpha_composite(self.layer, sh.filter(ImageFilter.GaussianBlur(blur * SS / 2)))
        self.d = ImageDraw.Draw(self.layer)

    def glass(self, box, r=16, alpha=0.84, line=0.16, fill=NAVY, sheen=True):
        self.shadow(box, r)
        x0, y0, x1, y1 = box
        self.d.rounded_rectangle(self.s(x0, y0, x1, y1), r * SS, fill=fill + (int(255 * alpha),), outline=IVORY + (int(255 * line),), width=SS)
        if sheen:
            self.d.rounded_rectangle(self.s(x0 + 1, y0 + 1, x1 - 1, y0 + (y1 - y0) * 0.42), (r - 1) * SS, fill=(255, 255, 255, 10))

    def text(self, xy, s, px, colour=IVORY, semi=False, anchor='la', shadow=True):
        f = font(px, semi)
        x, y = xy
        if shadow:
            self.d.text((x * SS + SS, y * SS + 2 * SS), s, font=f, fill=(0, 0, 0, 140), anchor=anchor)
        self.d.text((x * SS, y * SS), s, font=f, fill=colour + (255,), anchor=anchor)

    def wrap(self, xy, s, px, width, colour=IVORY, lh=1.6, semi=True):
        f = font(px, semi)
        words, lines, cur = s.split(' '), [], ''
        for w in words:
            t = (cur + ' ' + w).strip()
            if self.d.textlength(t, font=f) / SS > width and cur:
                lines.append(cur)
                cur = w
            else:
                cur = t
        lines.append(cur)
        for i, l in enumerate(lines):
            self.text((xy[0], xy[1] + i * px * lh), l, px, colour, semi)
        return len(lines)

    def diamond(self, c, r, fill=GOLD_HI, line=None):
        x, y = c
        pts = [(x * SS, (y - r) * SS), ((x + r) * SS, y * SS), (x * SS, (y + r) * SS), ((x - r) * SS, y * SS)]
        self.d.polygon(pts, fill=fill + (255,), outline=(line + (255,)) if line else None)

    def paste(self, im, xy):
        big = im.resize((im.width * SS, im.height * SS), Image.LANCZOS)
        self.layer.alpha_composite(big, (int(xy[0] * SS), int(xy[1] * SS)))

    def done(self):
        small = self.layer.resize((self.W, self.H), Image.LANCZOS)
        return Image.alpha_composite(self.base, small)


def bust(size=118):
    """The NPC's own idle sprite, head and shoulders, at a whole multiple (3x), so the portrait keeps the game's dots."""
    rows = json.load(open(ART / 'npc_art.json', encoding='utf-8'))
    r = next(x for x in rows if x['flipbook'].endswith('FB_NPC_4017_Idle'))
    im = Image.open(r['png']).convert('RGBA')
    x, y = map(int, r['uv'])
    crop = im.crop((x + 14, y + 2, x + 14 + 40, y + 2 + 40))
    return crop.resize((40 * 3, 40 * 3), Image.NEAREST).resize((size, size), Image.NEAREST)


def medallion(d, img):
    m = Image.new('RGBA', (d * SS, d * SS), (0, 0, 0, 0))
    md = ImageDraw.Draw(m)
    md.ellipse([0, 0, d * SS - 1, d * SS - 1], fill=(22, 41, 61, 245))
    pic = img.resize((d * SS, d * SS), Image.NEAREST)
    mask = Image.new('L', m.size, 0)
    ImageDraw.Draw(mask).ellipse([3 * SS, 3 * SS, d * SS - 3 * SS, d * SS - 3 * SS], fill=255)
    m.paste(pic, (0, 0), Image.composite(pic.split()[3], Image.new('L', m.size, 0), mask))
    md.ellipse([SS, SS, d * SS - SS, d * SS - SS], outline=GOLD + (230,), width=2 * SS)
    return m


def kit(name, size):
    return Image.open(KIT / (name + '.png')).convert('RGBA').resize((size, size), Image.LANCZOS)


def keychip(c, xy, label='F', size=26):
    x, y = xy
    c.d.rounded_rectangle(c.s(x, y + 2, x + size, y + size + 2), 7 * SS, fill=(14, 26, 40, 255))
    c.d.rounded_rectangle(c.s(x, y, x + size, y + size), 7 * SS, fill=(14, 26, 40, 255))
    c.d.rounded_rectangle(c.s(x + 2, y + 2, x + size - 2, y + size - 2), 5 * SS, fill=IVORY + (255,))
    c.text((x + size / 2, y + size / 2 + 1), label, size * 0.55, (22, 41, 61), anchor='mm', shadow=False)


def choices(c, x, y_bottom, w, sel=0, rowh=50, gap=8):
    n = len(CHOICES)
    top = y_bottom - n * rowh - (n - 1) * gap
    for i, (kind, label) in enumerate(CHOICES):
        y = top + i * (rowh + gap)
        on = i == sel
        c.glass((x, y, x + w, y + rowh), r=12, alpha=0.90 if on else 0.78, line=0.30 if on else 0.12, sheen=False)
        if on:
            c.d.rounded_rectangle(c.s(x + 1, y + 9, x + 5, y + rowh - 9), 2 * SS, fill=GOLD_HI + (255,))
        if kind == 'quest':
            c.paste(kit('T_HUD_Badge_Offer', 26), (x + 16, y + (rowh - 26) / 2))
        else:
            c.diamond((x + 29, y + rowh / 2), 5, fill=(GOLD if on else MUTED))
        c.text((x + 52, y + rowh / 2 + 1), label, 19, GOLD_HI if on else IVORY, semi=True, anchor='lm')
    return top


def option_a(plate):
    c = Canvas(plate)
    x0, y0, x1, y1 = 330, 852, 1590, 1032
    c.glass((x0, y0, x1, y1), r=18, alpha=0.92, sheen=False)
    # name tab on the panel's top edge
    c.glass((x0 + 28, y0 - 22, x0 + 28 + 150, y0 + 18), r=12, alpha=0.96, line=0.0, fill=(22, 41, 61), sheen=False)
    c.d.rounded_rectangle(c.s(x0 + 28, y0 - 22, x0 + 178, y0 + 18), 12 * SS, outline=GOLD + (150,), width=SS)
    c.diamond((x0 + 50, y0 - 2), 5)
    c.text((x0 + 64, y0 - 2), NAME, 21, GOLD_HI, anchor='lm')
    # portrait medallion with the NPC's own dots
    m = medallion(124, bust())
    c.layer.alpha_composite(m, (int((x0 + 34) * SS), int((y0 + 30) * SS)))
    c.wrap((x0 + 190, y0 + 44), LINE, 24, 1000)
    # advance: gold chevron + key
    c.d.polygon(c.s(x1 - 64, y1 - 32, x1 - 50, y1 - 32, x1 - 57, y1 - 24), fill=GOLD_HI + (255,))
    keychip(c, (x1 - 44, y1 - 42))
    choices(c, x1 - 440, y0 - 40, 440)
    return c.done()


def option_b(plate):
    c = Canvas(plate)
    hx, hy = NPC_HEAD
    w = 620
    x0, y1 = hx - 170, MARKER_TOP - 26
    f = font(22, True)
    # measure lines
    lines = 0
    cur = ''
    for word in LINE.split(' '):
        t = (cur + ' ' + word).strip()
        if c.d.textlength(t, font=f) / SS > w - 56 and cur:
            lines += 1
            cur = word
        else:
            cur = t
    lines += 1
    h = 58 + lines * 22 * 1.6 + 18
    y0 = y1 - h
    c.glass((x0, y0, x0 + w, y1), r=16, alpha=0.92, sheen=False)
    c.d.polygon(c.s(hx - 14, y1 - 1, hx + 14, y1 - 1, hx, y1 + 18), fill=NAVY + (int(255 * 0.92),))
    c.diamond((x0 + 30, y0 + 28), 5)
    c.text((x0 + 44, y0 + 28), NAME, 19, GOLD_HI, anchor='lm')
    c.wrap((x0 + 28, y0 + 50), LINE, 22, w - 56)
    keychip(c, (x0 + w - 44, y1 - 40))
    choices(c, 1180, 1010, 420)
    return c.done()


def option_c(plate):
    c = Canvas(plate)
    # bottom scrim, no box
    W, H = c.W, c.H
    sc = Image.new('RGBA', (W * SS, H * SS), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sc)
    top = 760
    for y in range(top, H):
        t = (y - top) / (H - top)
        a = int(255 * min(1.0, t * 1.25) * 0.80)
        sd.line([(0, y * SS), (W * SS, y * SS)], fill=NAVY + (a,), width=SS)
    c.layer = Image.alpha_composite(sc, c.layer)
    c.d = ImageDraw.Draw(c.layer)
    c.diamond((402, 884), 6)
    c.text((420, 884), NAME, 26, GOLD_HI, anchor='lm')
    c.d.line(c.s(420, 906, 560, 906), fill=GOLD + (160,), width=SS)
    c.wrap((420, 930), LINE, 25, 1080)
    c.d.polygon(c.s(1516, 1012, 1532, 1012, 1524, 1021), fill=GOLD_HI + (255,))
    keychip(c, (1540, 1002))
    choices(c, 1240, 820, 420)
    return c.done()


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'png').mkdir(exist_ok=True)
    plate_path = CAP / 'dlg_plate_view.png'
    if not plate_path.exists():
        plate_path = CAP / 'dlg_near_view.png'
    plate = Image.open(plate_path).convert('RGBA')
    now = Image.open(CAP / 'dlg_open_view.png').convert('RGBA')
    shots = {'now': now, 'a': option_a(plate), 'b': option_b(plate), 'c': option_c(plate)}
    for k, im in shots.items():
        im.convert('RGB').save(OUT / 'png' / (k + '.png'))
    small = {k: v.convert('RGB').resize((1280, 720), Image.LANCZOS) for k, v in shots.items()}
    html = PAGE
    for k, im in small.items():
        html = html.replace('%%%s%%' % k.upper(), uri(im))
    (OUT / 'dialogue.html').write_text(html, encoding='utf-8')
    print('wrote', OUT / 'dialogue.html', 'plate', plate_path.name)


PAGE = '''<!doctype html><html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>대화창 시안</title><style>
:root{--bg:#0b1219;--panel:#111c27;--text:#e8e4dc;--sub:#9aa7b1;--gold:#d6be8c}
@media (prefers-color-scheme: light){:root:not([data-theme="dark"]){--bg:#eef0f2;--panel:#fff;--text:#1d2731;--sub:#5d6b77}}
:root[data-theme="light"]{--bg:#eef0f2;--panel:#fff;--text:#1d2731;--sub:#5d6b77}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.65 "Pretendard","Malgun Gothic",sans-serif}
main{max-width:1320px;margin:0 auto;padding:40px 24px 80px}h1{font-size:28px;margin:0 0 6px}h2{font-size:20px;margin:44px 0 8px}
.lead{color:var(--sub);max-width:900px;margin:0 0 14px}.card{background:var(--panel);border-radius:14px;padding:16px;border:1px solid rgba(128,140,150,.18);margin-bottom:18px}
.card h3{margin:0 0 8px;font-size:16px}.card p,.card li{color:var(--sub);font-size:14px}.card p{margin:8px 0 0}img{width:100%;display:block;border-radius:10px}
.tag{font-size:12px;font-weight:700;padding:1px 8px;border-radius:99px;background:#2f4a3a;color:#bfe8c8;margin-left:6px}
.pick{outline:2px solid var(--gold);outline-offset:3px}ul{margin:6px 0 0;padding-left:18px}table{width:100%;border-collapse:collapse;font-size:14px}
td,th{padding:7px 8px;border-bottom:1px solid rgba(128,140,150,.2);text-align:left;vertical-align:top}th{color:var(--sub);font-weight:600}a{color:var(--gold)}
</style></head><body><main>
<h1>NPC 대화창 시안</h1>
<p class="lead">HUD(글래스 엣지)와 같은 말투로: 남색 유리 판, 상아 글자, 금 강조, 마름모 무늬. 그림은 실제 게임 화면 1080p 위에 게임 크기로 그렸고,
"지금"만 실제 대화창 캡처입니다. 시안 화면은 대화 중 필드 HUD(채팅·메뉴·지도·퀘스트)가 접힌 상태입니다.</p>

<h2>지금</h2>
<div class="card"><img src="%NOW%"><ul>
<li>설정창과 같은 장식 금테·나침반 무늬 --- 둥근 유리 HUD 사이에서 혼자 다른 시대의 창처럼 보임</li>
<li>판이 화면 아래 1/4 을 덮고, 그 위로 채팅·하단 메뉴·퀘스트 추적·지도가 그대로 겹침</li>
<li>초상이 화면에 이미 보이는 NPC 전신을 작게 한 번 더 --- 얼굴이 안 읽힘</li>
<li>선택지가 판과 떨어진 별도 장식 창, 퀘스트 선택지 글자가 잘림("[진행 가능] 레벨 20...")</li>
<li>다음으로 넘기는 표시·키 안내가 약함</li></ul></div>

<h2>다른 게임들</h2>
<div class="card"><table>
<tr><th>게임</th><th>대화 표시</th><th>배울 점</th></tr>
<tr><td>옥토패스 트래블러 (HD-2D)</td><td>화면 아래 반투명 어두운 창 + 이름표, 지나가는 말은 머리 위 작은 말풍선. 글꼴은 장식 없이 또렷하게(장식 글꼴은 장 제목에만)</td><td>창은 얇고 투명하게, 장식은 아껴서</td></tr>
<tr><td>드래곤 퀘스트 XI</td><td>아래 창 + 이름, 선택지는 오른쪽 작은 창</td><td>선택지는 대화 창 옆에 붙여 시선 이동을 줄임</td></tr>
<tr><td>원신</td><td>판 없이 아래 그라데이션 위에 이름(금)·대사, 선택지는 오른쪽 목록(아이콘으로 퀘스트·일반 구분)</td><td>장면을 가리지 않음, 선택지 종류를 아이콘으로</td></tr>
<tr><td>씨 오브 스타즈 류 인디 RPG</td><td>말하는 캐릭터 위 말풍선</td><td>누가 말하는지 바로 보임, 대신 긴 문장엔 약함</td></tr>
</table></div>

<h2>시안</h2>
<div class="card pick"><h3>A. 글래스 하단 창 + 이름 탭 + 도트 흉상<span class="tag">추천</span></h3><img src="%A%">
<p>HUD 와 같은 유리 판(높이 180, 지금보다 낮고 투명). 이름은 판 위 가장자리에 걸친 탭(금 글자 + 마름모), 초상은 그 NPC 스프라이트의 머리·어깨를
정수 배(3배)로 키운 둥근 메달 --- 게임 도트 그대로라 얼굴이 읽힘. 선택지는 창 오른쪽 위에 붙은 유리 줄들: 퀘스트는 머리 위 표시와 같은 마름모 배지,
고른 줄은 금 막대 + 금 글자, 글자는 잘리지 않고 줄을 넘김. 다음 표시는 금 꺾쇠 + 키 칩. 옥토패스·드퀘 계열의 익숙한 자리라 긴 대사에도 강함.</p></div>
<div class="card"><h3>B. 머리 위 말풍선</h3><img src="%B%">
<p>말하는 NPC 머리 위에 꼬리 달린 유리 말풍선, 선택지는 화면 오른쪽 아래. 누가 말하는지 가장 확실하고 장면이 넓게 보이지만,
대사가 길면 풍선이 커지고 화면 위쪽 NPC 는 풍선이 잘릴 수 있어 위치 보정이 필요.</p></div>
<div class="card"><h3>C. 판 없는 자막형</h3><img src="%C%">
<p>아래쪽만 남색 그라데이션, 그 위에 이름(금)과 대사. 선택지는 오른쪽 목록. 가장 가볍고 영화 같지만 밝은 바닥 위에서 글자 대비가 그라데이션에 기대고,
초상이 없어 여러 명이 말할 때 구분이 약함.</p></div>

<h2>어느 안이든 같이</h2>
<div class="card"><ul>
<li>대화 중에는 필드 HUD(채팅·하단 메뉴·지도·퀘스트 추적)를 접음 --- 전투 때 접는 방식(FNUHudBattleFold)과 같게</li>
<li>선택지 글자는 자르지 않고 두 줄까지 넘김, 퀘스트 선택지는 머리 위 표시와 같은 배지 아이콘</li>
<li>창은 아래에서 12px 떠오르며 0.2초에 나타남, 글자는 지금처럼 한 글자씩, 다 나오면 꺾쇠가 살짝 위아래로</li>
<li>패드면 키 칩 대신 패드 버튼 그림. 바인딩 이름(DialoguePanel·SpeakerNameText·DialogueText·PortraitImage·OptionList 등)은 그대로라 C++ 수정 거의 없음</li>
</ul></div>
<p class="lead" style="margin-top:24px">참고: <a href="https://madegooddesigns.com/?p=10958">Octopath Traveler 글꼴(대화창은 읽기 쉬운 글꼴)</a> ·
<a href="https://www.abratabia.com/game-ui-design/dialogue-ui.php">Dialogue UI 기본 구성(이름 탭·초상·넘김 표시, 화면 아래 배치)</a> ·
<a href="https://en.wikipedia.org/wiki/HD-2D">HD-2D</a>. 드퀘·원신·말풍선 RPG 는 플레이 기억 기준 설명.</p>
</main></body></html>'''

if __name__ == '__main__':
    build()
