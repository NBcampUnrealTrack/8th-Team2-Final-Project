# -*- coding: utf-8 -*-
"""In-game HUD renewal proposal page (field + battle), drawn in HTML over real play captures.

Inputs   Saved/CharacterCreateGlass/hud_*.png      play captures (clean = no viewport widgets, *_ui_view = current HUD)
         Saved/HUDRenewal/maps/*.png               baked field maps (export_map_textures.py)
         Saved/HUDRenewal/src/*.png                portrait heads cut from play captures on their own pixel grid
         Saved/HUDRenewal/fonts/MaruBuri-*.ttf      the game's UI font, pulled out of the font assets
         Docs/HUD_Renewal_Proposal_20261014/assets/icons/*.png   pixel icons (make_icons.py)
Output   Docs/HUD_Renewal_Proposal_20261014/hud_proposal.html  (one file, everything embedded)

Every mock is a 1920x1080 canvas in 1080p pixels, scaled to the page width.
"""
import base64
import html
import io
import pathlib
import re

from fontTools import subset
from fontTools.ttLib import TTFont
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'Docs' / 'HUD_Renewal_Proposal_20261014'
CAP = ROOT / 'Saved' / 'CharacterCreateGlass'
SRC = ROOT / 'Saved' / 'HUDRenewal'


# ---------------------------------------------------------------- assets

def uri(img, fmt='PNG', quality=86):
    buf = io.BytesIO()
    if fmt == 'JPEG':
        img.convert('RGB').save(buf, 'JPEG', quality=quality, optimize=True, progressive=True)
        mime = 'image/jpeg'
    else:
        img.save(buf, 'PNG', optimize=True)
        mime = 'image/png'
    return 'data:%s;base64,%s' % (mime, base64.b64encode(buf.getvalue()).decode())


def backdrop(name):
    return uri(Image.open(CAP / name), 'JPEG', 84)


BG = {
    'village': backdrop('hud_village_clean.png'),
    'village_now': backdrop('hud_village_ui_view.png'),
    'forest': backdrop('hud_forest_clean.png'),
    'battle': backdrop('hud_battle_clean.png'),
    'battle_now': backdrop('hud_battle_ui_view.png'),
}


def map_crop(name, cx, cy, size=300):
    im = Image.open(SRC / 'maps' / name).convert('RGB')
    return uri(im.crop((cx - size // 2, cy - size // 2, cx + size // 2, cy + size // 2)), 'JPEG', 88)


# player at world (0,-3170) on a +-4762.8 map = (512, 853); the crop leans north so the village ahead shows
MAP = {'village': map_crop('T_Map_L_Libregrove.png', 512, 812), 'forest': map_crop('T_Map_L_Fernbrook_Entrance.png', 520, 690)}


def head(name, box=None, scale=1):
    im = Image.open(SRC / 'src' / name).convert('RGBA')
    if box:
        im = im.crop(box)
    if scale != 1:
        im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
    return uri(im)


FACE = {
    'knight': head('knight_head_art.png', (2, 0, 32, 30), 2),      # 60x60: art pixels doubled, the field's 2 px grid
    'blue': head('druid_blue_head.png', (14, 6, 74, 66)),          # already 2 px per art pixel
    'red': head('druid_red_head.png', (14, 6, 74, 66)),
}
slime = Image.open(CAP / 'hud_battle_clean.png').convert('RGB').crop((1400, 505, 1480, 585))
FACE['slime'] = uri(slime)

ICON = {p.stem: uri(Image.open(p)) for p in sorted((OUT / 'assets' / 'icons').glob('*.png'))}


def icon(name, size=30, cls=''):
    return '<img class="px %s" src="%s" width="%d" height="%d" alt="">' % (cls, ICON[name], size, size)


def kc(label, cls=''):
    return '<span class="kc %s">%s</span>' % (cls, label)


# ---------------------------------------------------------------- concept A: glass edge

def a_me(x, y, hp=82, sp=60, hp_num='812', sp_num='60'):
    return f'''<div class="a-glass a-me" style="left:{x}px;top:{y}px">
  <div class="a-face" style="background-image:url({FACE['knight']})"></div><div class="a-lv">12</div>
  <div class="a-name">새벽<span>나이아 나이트</span></div>
  <div class="a-row" style="top:52px"><b>HP</b><div class="a-bar hp"><i style="width:{hp}%"></i></div><em>{hp_num}</em></div>
  <div class="a-row" style="top:74px"><b>SP</b><div class="a-bar sp"><i style="width:{sp}%"></i></div><em>{sp_num}</em></div>
</div>'''


def a_party(x, y):
    rows = [('blue', '하늘', 'Lv.3 드루이드', 92), ('red', '모래', 'Lv.1 노비스', 54)]
    out = []
    for i, (face, name, sub, hp) in enumerate(rows):
        out.append(f'''<div class="a-glass a-pm" style="left:{x}px;top:{y + i * 54}px">
  <div class="a-pface" style="background-image:url({FACE[face]})"></div>
  <div class="a-pname">{name}<span>{sub}</span></div><div class="a-bar thin"><i style="width:{hp}%"></i></div>
</div>''')
    return '\n'.join(out)


def a_map(which, place, zone, markers=True, arrow_top=147):
    marks = ''
    if markers:
        marks = '''<i class="a-mk quest" style="left:88px;top:92px">!</i>
  <i class="a-mk party" style="left:70px;top:118px"></i><i class="a-mk party" style="left:96px;top:124px"></i>
  <i class="a-mk portal" style="left:124px;top:214px"></i>'''
    return f'''<div class="a-glass a-map" style="right:40px;top:36px">
  <div class="a-mapimg" style="background-image:url({MAP[which]})"></div>
  <div class="a-mapvig"></div>
  {marks}
  <svg class="a-me-arrow" style="top:{arrow_top}px" viewBox="0 0 20 22" width="20" height="22"><path d="M10 1 L19 20 L10 15 L1 20 Z" fill="#f4d78f" stroke="#16293d" stroke-width="2" stroke-linejoin="round"/></svg>
  <span class="a-north">N</span>
  <div class="a-zoom"><span>+</span><span>−</span></div>
  <div class="a-maphint">{kc('M')}지도</div>
</div>
<div class="a-loc ts" style="right:42px;top:300px"><i class="dia"></i><b>{place}</b><span>{zone}</span></div>'''


def a_quest(top=340, faded=False, updated=False):
    op = ' style="opacity:.5"' if faded else ''
    first_obj = ('<span class="done">수레길 진입로에 도착하기</span>' if updated
                 else '리베르그로브 아랫마을에 도착하기')
    return f'''<div class="a-quest" style="right:40px;top:{top}px"><div{op}>
  <div class="a-qhead ts"><span>퀘스트</span><em>2</em><span class="r">{kc('J')}일지</span></div>
  <div class="a-q main ts"><i class="dia"></i><b>뿌리 사이로</b><span class="tag">메인</span></div>
  <div class="a-qobj ts">{first_obj}</div>
  <div class="a-q ts"><i class="dia hollow"></i><b>잃어버린 망치</b></div>
  <div class="a-qobj ts">대장장이 브론과 이야기하기 <em>0/1</em></div>
</div></div>'''


DOCK = [('quest', '퀘스트 일지', 'J'), ('character', '캐릭터', 'C'), ('stats', '능력치', 'K'),
        ('growth', '성장', 'T'), ('bag', '소지품', 'I'), ('naia', '나이아', 'N')]


def a_dock(hover=None):
    btns = []
    for name, label, key in DOCK:
        cls = ' on' if name == hover else ''
        tip = f'<div class="a-tiprow"><div class="a-tip">{label}{kc(key)}</div></div>' if name == hover else ''
        btns.append(f'<div class="a-btn{cls}">{icon(name)}{tip}</div>')
    return f'''<div class="a-glass a-dock" style="right:40px;bottom:36px">
  {''.join(btns)}<i class="a-div"></i>
  <div class="a-btn">{icon('menu')}</div><div class="a-btn">{icon('eye')}</div>
</div>'''


CHAT = [('sys', '알림', '리베르그로브 공방 구역에 들어왔습니다.'),
        ('all', '일반', '<b>모래</b> 망치 퀘스트 같이 하실 분 계신가요?'),
        ('party', '파티', '<b>하늘</b> 대장간 앞에서 만나요!')]


def a_chat(faded=False):
    lines = ''.join(f'<div class="a-line ts" style="opacity:{o}"><span class="ch {c}">{t}</span>{m}</div>'
                    for (c, t, m), o in zip(CHAT, (.55, .8, 1)))
    op = ' style="opacity:.4"' if faded else ''
    return f'''<div class="a-chat" style="left:40px;bottom:36px"{op}>
  <div class="a-lines">{lines}</div>
  <div class="a-glass a-chatin">{icon('chat', 26)}{kc('Enter')}<span>대화하기</span></div>
</div>'''


def a_chat_open():
    more = [('sys', '알림', '하늘 님이 파티에 들어왔습니다.'),
            ('party', '파티', '<b>모래</b> 저 아직 노비스라 천천히 가요 ㅎㅎ'),
            ('all', '일반', '<b>달빛</b> 펀브룩 심부 같이 가실 분 Lv.8 이상'),
            ('whisper', '귓속말', '<b>하늘</b> 망치는 대장간 뒤편에 있대요')] + CHAT
    lines = ''.join(f'<div class="a-line"><span class="ch {c}">{t}</span>{m}</div>' for c, t, m in more)
    tabs = ''.join(f'<span class="{"on" if i == 0 else ""}">{t}</span>' for i, t in enumerate(('전체', '일반', '파티', '귓속말', '알림')))
    return f'''<div class="a-glass a-chatbox" style="left:40px;bottom:36px">
  <div class="a-tabs">{tabs}<span class="add">+</span></div>
  <div class="a-lines open">{lines}</div>
  <div class="a-input"><span class="chan party">파티 ▾</span><span class="field">대장간 앞에서 만나요<i class="caret"></i></span>{kc('Enter')}</div>
</div>'''


def plate(x, y, name, sub='', kind=''):
    sub_html = f'<span>{sub}</span>' if sub else ''
    return f'<div class="nplate {kind}" style="left:{x}px;top:{y}px">{name}{sub_html}</div>'


def a_prompt(x, y, key, text):
    return f'<div class="a-glass a-prompt" style="left:{x}px;top:{y}px">{kc(key, "gold")}<span>{text}</span></div>'


def frame(fid, bg, layers, now=None, label=''):
    # bg / now are keys of BG: each picture is embedded once, as a class (the page reuses them)
    now_html = f'<div class="now bg-{now}"></div>' if now else ''
    return f'''<div class="stage" id="{fid}" data-view="new" aria-label="{html.escape(label)}">
<div class="canvas"><div class="bg bg-{bg}"></div>
<div class="hud">{layers}</div>{now_html}<div class="safe"><i></i><b></b></div></div></div>'''


def views(fid, has_now):
    if fid.startswith('now-'):
        opts = [('new', '번호 보기'), ('off', '번호 끄기')]
    else:
        opts = [('new', '새 HUD'), ('off', 'HUD 끄기')] + ([('now', '지금 HUD')] if has_now else [])
    btns = ''.join(f'<button type="button" data-for="{fid}" data-view="{v}" aria-pressed="{"true" if v == "new" else "false"}">{t}</button>' for v, t in opts)
    return f'<div class="seg" role="group" aria-label="보기">{btns}</div>'


def a_village(hover='bag', chat_open=False):
    layers = [a_me(40, 36), a_party(40, 152), a_map('village', '리베르그로브', '공방 구역'), a_quest(),
              a_dock(hover), a_chat_open() if chat_open else a_chat(),
              plate(960, 598, '새벽'), plate(410, 338, '하늘', '', 'party'), plate(740, 268, '브론', '대장장이', 'npc')]
    if not chat_open:
        layers.append(a_prompt(800, 404, 'F', '대화하기'))
    return '\n'.join(layers)


def a_forest():
    banner = '''<div class="a-banner"><div class="glow"></div>
  <div class="eye ts">펀브룩</div><div class="title"><i></i><b>수레길 진입로</b><i></i></div><div class="sub ts">사냥터 · 권장 Lv.1 ~ 5</div></div>'''
    toast = f'''<div class="crow" style="top:32px"><div class="a-glass a-toast"><div class="ic">{icon('quest', 26)}</div>
  <div><em>퀘스트 갱신</em><b>뿌리 사이로</b><span>수레길 진입로에 도착했습니다</span></div><i class="ok">✓</i></div></div>'''
    invite = f'''<div class="a-glass a-invite" style="left:40px;top:152px">
  <div class="a-pface big" style="background-image:url({FACE['blue']})"></div>
  <em>파티 초대</em><b>하늘 님이 파티에 초대했습니다</b>
  <div class="btns"><span class="yes">{kc('Y', 'dark')}수락</span><span class="no">{kc('N')}거절</span></div>
  <div class="timer"><i style="width:62%"></i></div>
</div>'''
    return '\n'.join([a_me(40, 36, 64, 45, '634', '45'), invite, a_map('forest', '펀브룩 초입', '수레길 진입로', markers=False, arrow_top=115),
                      a_quest(faded=True, updated=True), a_dock(), a_chat(faded=True), toast, banner, plate(960, 598, '새벽')])


def a_battle_cmd():
    order = ''.join(f'<div class="o {c}"><div class="a-pface" style="background-image:url({FACE[f]})"></div></div>' + ('<i class="arr">›</i>' if i < 2 else '')
                    for i, (f, c) in enumerate((('knight', 'cur'), ('slime', 'foe'), ('slime', 'foe'))))
    cmds = [('sword', '공격', 'on', ''), ('spark', '스킬', '', 'SP 1~3'), ('bag', '아이템', '', '4'),
            ('naia', '교감', '', '포획'), ('swap', '정령 교체', 'off', '동행 정령 없음'), ('flee', '도주', '', '')]
    items = ''.join(f'<div class="c {cls}">{icon(ic, 26)}<b>{t}</b><span>{r}</span></div>' for ic, t, cls, r in cmds)
    return f'''<div class="crow" style="top:30px"><div class="a-glass a-turn"><em>TURN</em><b>01</b><i></i><span>행동 선택</span></div></div>
<div class="crow" style="top:100px"><div class="a-order">{order}</div></div>
{a_ally(40, 1080 - 36 - 132)}
<div class="a-foe sel ts" style="left:1440px;top:478px"><i class="chev">▼</i><div class="nm"><b>푸루나</b><span>Lv.4</span></div><div class="a-bar foe"><i style="width:100%"></i></div></div>
<div class="a-foe ts" style="left:1600px;top:500px"><div class="nm"><b>푸루나</b><span>Lv.4</span></div><div class="a-bar foe"><i style="width:100%"></i></div></div>
<div class="a-glass a-desc"><em>공격</em><p>무기로 적 하나를 공격합니다.<br>SP를 넣으면 더 강해집니다.</p>
  <div class="boost">SP 투입<span class="pips"><i class="on"></i><i></i><i></i></span>{kc('←')}{kc('→')}</div></div>
<div class="a-glass a-cmd"><div class="h"><em>새벽의 행동</em><span>{kc('↑')}{kc('↓')}{kc('Enter')}</span></div>{items}</div>'''


def a_ally(x, y, hp=82, ghost=None):
    gh = f'<s style="left:{hp}%;width:{ghost - hp}%"></s>' if ghost else ''
    return f'''<div class="a-glass a-ally" style="left:{x}px;top:{y}px">
  <div class="a-face" style="background-image:url({FACE['knight']})"></div><div class="a-lv">12</div>
  <div class="a-name">새벽<span>나이아 나이트</span></div>
  <div class="a-row wide" style="top:56px"><b>HP</b><div class="a-bar hp big"><i style="width:{hp}%"></i>{gh}</div><em>{int(990 * hp / 100)} / 990</em></div>
  <div class="a-row wide" style="top:86px"><b>SP</b><span class="pips sp"><i class="on"></i><i class="on"></i><i class="on"></i><i></i><i></i></span>
    <b class="res">자원</b><span class="pips res"><i class="on"></i><i class="on"></i><i></i></span></div>
</div>'''


def a_battle_parry():
    return f'''<div class="crow" style="top:30px"><div class="a-glass a-turn"><em>TURN</em><b>01</b><i></i><span class="foe">적의 행동</span></div></div>
<div class="crow" style="top:100px"><div class="a-glass a-action"><div class="a-pface" style="background-image:url({FACE['slime']})"></div><span>푸루나</span><b>몸통 박치기</b></div></div>
{a_ally(40, 1080 - 36 - 132, 64, 72)}
<div class="a-foe act ts" style="left:1440px;top:478px"><i class="chev">!</i><div class="nm"><b>푸루나</b><span>Lv.4</span></div><div class="a-bar foe"><i style="width:100%"></i></div></div>
<div class="a-foe ts" style="left:1600px;top:500px"><div class="nm"><b>푸루나</b><span>Lv.4</span></div><div class="a-bar foe"><i style="width:100%"></i></div></div>
<div class="a-parry" style="left:536px;top:842px"><i class="r1"></i><i class="r2"></i></div>
<div class="crow" style="top:812px;left:610px;right:auto;width:360px"><div class="a-glass a-cue"><div class="bar"><i class="win"></i><i class="needle"></i></div>
  <div class="keys"><span>{kc('Space', 'gold')}패링</span><span>{kc('Shift')}회피</span></div></div></div>'''


# ---------------------------------------------------------------- concept B: adventurer's journal

def b_village():
    return f'''<div class="b-card b-me" style="left:36px;top:30px">
  <div class="b-face" style="background-image:url({FACE['knight']})"></div>
  <div class="b-name">새벽 <span>Lv.12 · 나이아 나이트</span></div>
  <div class="b-row" style="top:56px"><b>체력</b><div class="b-bar hp"><i style="width:82%"></i></div><em>812</em></div>
  <div class="b-row" style="top:80px"><b>기력</b><div class="b-bar sp"><i style="width:60%"></i></div><em>60</em></div>
</div>
<div class="b-card b-pm" style="left:52px;top:158px"><div class="b-pface" style="background-image:url({FACE['blue']})"></div>하늘<span>Lv.3</span></div>
<div class="b-card b-pm" style="left:52px;top:204px"><div class="b-pface" style="background-image:url({FACE['red']})"></div>모래<span>Lv.1</span></div>
<div class="b-compass" style="right:34px;top:22px"><div class="b-mapimg" style="background-image:url({MAP['village']})"></div><i class="n">N</i>
  <svg class="b-arrow" viewBox="0 0 20 22" width="20" height="22"><path d="M10 1 L19 20 L10 15 L1 20 Z" fill="#b8433a" stroke="#f2e8d0" stroke-width="2"/></svg></div>
<div class="b-ribbon" style="right:38px;top:296px">리베르그로브 · 공방 구역</div>
<div class="b-note" style="right:44px;top:368px"><i class="pin"></i>
  <b>뿌리 사이로</b><p>□ 리베르그로브 아랫마을에 도착하기</p><b class="s">잃어버린 망치</b><p>□ 대장장이 브론과 이야기하기</p></div>
<div class="b-strap" style="right:36px;bottom:30px">{''.join('<span>' + icon(n, 28) + '</span>' for n, _, _ in DOCK)}<span>{icon('menu', 28)}</span></div>
<div class="b-scroll" style="left:36px;bottom:30px"><p><em class="sys">알림</em>리베르그로브 공방 구역에 들어왔습니다.</p>
  <p><em>일반</em><b>모래</b> 망치 퀘스트 같이 하실 분 계신가요?</p><p><em class="party">파티</em><b>하늘</b> 대장간 앞에서 만나요!</p></div>
<div class="b-plate" style="left:960px;top:598px">새벽</div><div class="b-plate party" style="left:410px;top:338px">하늘</div>
<div class="b-plate npc" style="left:740px;top:268px">브론 <span>대장장이</span></div>
<div class="b-sign" style="left:800px;top:404px"><span class="key">F</span>대화하기</div>'''


# ---------------------------------------------------------------- concept C: clear overlay

def c_village():
    return f'''<div class="c2 c-me ts" style="left:44px;top:40px"><b>새벽</b><span>Lv.12 나이아 나이트</span>
  <div class="c2 c-line hp"><i style="width:82%"></i></div><div class="c2 c-line sp"><i style="width:60%"></i></div>
  <div class="c2 c-pm"><b>하늘</b><div class="c2 c-line hp sm"><i style="width:92%"></i></div></div>
  <div class="c2 c-pm"><b>모래</b><div class="c2 c-line hp sm"><i style="width:54%"></i></div></div></div>
<div class="c2 c-map" style="right:44px;top:34px"><div class="img" style="background-image:url({MAP['village']})"></div>
  <svg class="c-arrow" viewBox="0 0 20 22" width="18" height="20"><path d="M10 1 L19 20 L10 15 L1 20 Z" fill="#fff" stroke="#000" stroke-opacity=".5" stroke-width="2"/></svg><i class="n ts">N</i></div>
<div class="c2 c-loc ts" style="right:44px;top:248px">리베르그로브<span>공방 구역</span></div>
<div class="c2 c-quest ts" style="right:44px;top:318px"><b>뿌리 사이로</b><p>리베르그로브 아랫마을에 도착하기</p><b class="s">잃어버린 망치</b><p>대장장이 브론과 이야기하기 0/1</p></div>
<div class="c2 c-chat ts" style="left:44px;bottom:44px"><p style="opacity:.6"><em>일반</em>모래 · 망치 퀘스트 같이 하실 분 계신가요?</p><p><em class="party">파티</em>하늘 · 대장간 앞에서 만나요!</p></div>
<div class="c2 c-hint ts" style="right:44px;bottom:44px">{kc('Tab')} 바로가기 <i></i>{kc('Esc')} 메뉴</div>
<div class="c2 c-plate ts" style="left:960px;top:604px">새벽</div><div class="c2 c-plate party ts" style="left:410px;top:344px">하늘</div>
<div class="c2 c-plate npc ts" style="left:740px;top:276px">브론</div>
<div class="c2 c-prompt ts" style="left:800px;top:410px">{kc('F')} 대화하기</div>'''


# ---------------------------------------------------------------- current HUD notes

def marks(points):
    return ''.join(f'<div class="mark" style="left:{x}px;top:{y}px">{n}</div>' for n, x, y in points)


FIELD_MARKS = [(1, 136, 128), (2, 196, 296), (3, 1770, 66), (4, 960, 520), (5, 1855, 1010), (6, 240, 600)]
BATTLE_MARKS = [(7, 210, 360), (8, 1000, 690), (9, 540, 655), (10, 1530, 380), (11, 230, 96)]


# ---------------------------------------------------------------- CSS

CSS = r'''
:root {
  color-scheme: light;
  --p-bg: #f4f1ea; --p-raise: #e9e4d9; --p-line: #d3ccbd; --p-text: #36424c; --p-strong: #14202c; --p-muted: #5f6c76;
  --p-accent: #6f5a2c; --p-fill: #16293d; --p-fill-ink: #f1ede6; --p-chip-line: rgba(111, 90, 44, .45);
  --ui: "MaruBuriNU", "Noto Serif KR", serif;
  --body: "Pretendard", "IBM Plex Sans KR", "Apple SD Gothic Neo", "Malgun Gothic", system-ui, sans-serif;
}
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { color-scheme: dark; --p-bg: #0d1217; --p-raise: #141b22; --p-line: #25313b; --p-text: #c9d0d4; --p-strong: #f1ede6; --p-muted: #98a4ac; --p-accent: #c8b691; --p-fill: #c8b691; --p-fill-ink: #0f1d2c; --p-chip-line: rgba(200, 182, 145, .45); } }
:root[data-theme="dark"] { color-scheme: dark; --p-bg: #0d1217; --p-raise: #141b22; --p-line: #25313b; --p-text: #c9d0d4; --p-strong: #f1ede6; --p-muted: #98a4ac; --p-accent: #c8b691; --p-fill: #c8b691; --p-fill-ink: #0f1d2c; --p-chip-line: rgba(200, 182, 145, .45); }
* { box-sizing: border-box; }
html, body { margin: 0; background: var(--p-bg); color: var(--p-text); }
body { font-family: var(--body); font-size: 15.5px; line-height: 1.7; }
.wrap { max-width: 1240px; margin: 0 auto; padding: 36px 20px 96px; display: grid; gap: 56px; }
h1, h2, h3 { font-family: var(--ui); font-weight: 700; margin: 0; letter-spacing: -.01em; color: var(--p-strong); text-wrap: balance; }
h1 { font-size: clamp(28px, 4.2vw, 42px); line-height: 1.2; }
h2 { font-size: 25px; }
h3 { font-size: 17px; color: var(--p-accent); margin: 0 0 6px; }
p { margin: 0; }
.eyebrow { font-size: 12.5px; letter-spacing: .16em; text-transform: uppercase; color: var(--p-accent); font-weight: 600; }
header.top { display: grid; gap: 14px; }
.lede { max-width: 72ch; font-size: 16.5px; }
.lede b, .sub b, li b, td b { color: var(--p-strong); font-weight: 600; }
section.block { display: grid; gap: 18px; }
.sub { color: var(--p-muted); max-width: 78ch; }
.head { display: flex; flex-wrap: wrap; align-items: center; gap: 10px 14px; }
.tag { font-family: var(--ui); font-weight: 700; font-size: 14px; color: var(--p-fill-ink); background: var(--p-fill); padding: 2px 10px; border-radius: 3px; }
.pick { font-size: 13px; font-weight: 600; color: var(--p-accent); border: 1px solid var(--p-accent); padding: 1px 9px; border-radius: 99px; }
.bar-ctl { display: flex; flex-wrap: wrap; align-items: center; gap: 10px 16px; }
.seg { display: inline-flex; border: 1px solid var(--p-line); border-radius: 6px; overflow: hidden; }
.seg button { font: 500 13.5px var(--body); color: var(--p-muted); background: transparent; border: 0; padding: 7px 13px; cursor: pointer; }
.seg button + button { border-left: 1px solid var(--p-line); }
.seg button[aria-pressed="true"] { background: #16293d; color: #f1ede6; }
.seg button:focus-visible, .toggle:focus-visible { outline: 2px solid var(--p-accent); outline-offset: 2px; }
.toggle { font: 500 13.5px var(--body); color: var(--p-muted); background: transparent; border: 1px solid var(--p-line); border-radius: 6px; padding: 7px 13px; cursor: pointer; }
.toggle[aria-pressed="true"] { color: var(--p-strong); border-color: var(--p-accent); }
.note { font-size: 13.5px; color: var(--p-muted); max-width: 90ch; }
.two { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); gap: 14px 40px; }
@media (max-width: 820px) { .two { grid-template-columns: 1fr; } }
ul.tight { margin: 0; padding-left: 18px; display: grid; gap: 7px; font-size: 14.5px; }
ol.issues { margin: 0; padding-left: 0; list-style: none; display: grid; gap: 9px; font-size: 14.5px; }
ol.issues li { display: grid; grid-template-columns: 26px 1fr; gap: 10px; align-items: start; }
ol.issues li span.n { display: inline-grid; place-items: center; width: 24px; height: 24px; border-radius: 50%; background: #c0503f; color: #fff; font: 700 12.5px var(--body); margin-top: 1px; }
.tblwrap { overflow-x: auto; border: 1px solid var(--p-line); border-radius: 6px; }
table.cmp { border-collapse: collapse; width: 100%; min-width: 860px; font-size: 14px; }
.cmp th, .cmp td { text-align: left; padding: 11px 14px; border-bottom: 1px solid var(--p-line); vertical-align: top; }
.cmp thead th { font-family: var(--ui); font-size: 15px; color: var(--p-accent); background: var(--p-raise); white-space: nowrap; }
.cmp tbody th { font-weight: 600; color: var(--p-strong); white-space: nowrap; background: var(--p-raise); }
.cmp td.win { color: var(--p-strong); font-weight: 600; }
.cmp tr:last-child th, .cmp tr:last-child td { border-bottom: 0; }
.grid2 { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 22px; }
@media (max-width: 900px) { .grid2 { grid-template-columns: 1fr; } }
.cap { display: grid; gap: 10px; }
.cap figcaption { font-size: 14px; color: var(--p-muted); }
.cap figcaption b { color: var(--p-strong); }
figure { margin: 0; }
.steps { margin: 0; padding-left: 20px; display: grid; gap: 9px; font-size: 14.5px; }
code { font-family: ui-monospace, "Cascadia Mono", Consolas, monospace; font-size: .9em; background: var(--p-raise); padding: 1px 5px; border-radius: 3px; overflow-wrap: anywhere; }
.decide { display: grid; gap: 10px; counter-reset: d; margin: 0; padding: 0; list-style: none; }
.decide li { counter-increment: d; display: grid; grid-template-columns: 30px 1fr; gap: 10px; font-size: 14.5px; }
.decide li::before { content: counter(d); display: inline-grid; place-items: center; width: 26px; height: 26px; border-radius: 6px; background: var(--p-fill); color: var(--p-fill-ink); font: 700 13px var(--body); }
.palette { display: flex; flex-wrap: wrap; gap: 10px; }
.palette span { display: inline-flex; align-items: center; gap: 8px; font-size: 13px; border: 1px solid var(--p-line); border-radius: 99px; padding: 3px 12px 3px 4px; }
.palette i { width: 22px; height: 22px; border-radius: 50%; border: 1px solid rgba(0,0,0,.15); }
.icons { display: flex; flex-wrap: wrap; gap: 8px; padding: 14px; background: #16293d; border-radius: 8px; }
.icons img { width: 45px; height: 45px; image-rendering: pixelated; }

/* ---------- stage: a 1920x1080 canvas scaled to the column */
.stage { position: relative; width: 100%; aspect-ratio: 16 / 9; overflow: hidden; border-radius: 8px; background: #0b1118; box-shadow: 0 1px 0 rgba(0,0,0,.05), 0 10px 30px rgba(10, 20, 30, .18); }
.canvas { position: absolute; left: 0; top: 0; width: 1920px; height: 1080px; transform-origin: 0 0; transform: scale(var(--k, .5)); overflow: hidden;
  font-family: "MaruBuriNU", serif; color: #f1ede6; line-height: 1.3; }
.canvas .bg, .canvas .now { position: absolute; inset: 0; background-size: 1920px 1080px; }
.canvas .hud { position: absolute; inset: 0; }
.canvas .now { display: none; }
.stage[data-view="off"] .hud, .stage[data-view="now"] .hud { display: none; }
.stage[data-view="now"] .now { display: block; }
.canvas .safe { position: absolute; inset: 0; display: none; pointer-events: none; }
body.safe-on .canvas .safe { display: block; }
.safe i { position: absolute; inset: 36px 40px; border: 2px dashed rgba(255, 220, 120, .8); }
.safe b { position: absolute; left: 30%; right: 30%; top: 22%; bottom: 18%; background: rgba(120, 200, 255, .14); border: 2px solid rgba(120, 200, 255, .7); }
.canvas .mark { position: absolute; width: 44px; height: 44px; margin: -22px 0 0 -22px; border-radius: 50%; background: #c0503f; color: #fff; display: grid; place-items: center;
  font: 700 22px "Pretendard", "Malgun Gothic", sans-serif; box-shadow: 0 0 0 4px rgba(255,255,255,.9), 0 6px 16px rgba(0,0,0,.4); }
.px { image-rendering: pixelated; display: block; }
.ts { text-shadow: 0 1px 2px rgba(0,0,0,.7), 0 0 10px rgba(0,0,0,.35); }
.kc { display: inline-grid; place-items: center; min-width: 24px; height: 24px; padding: 0 7px; border-radius: 6px; border: 1px solid rgba(241,237,230,.38);
  background: rgba(241,237,230,.09); font: 600 12.5px "Pretendard", "Malgun Gothic", sans-serif; color: #f1ede6; box-shadow: inset 0 -2px 0 rgba(0,0,0,.28); line-height: 1; letter-spacing: 0; text-shadow: none; }
.kc.gold { background: #d6be8c; border-color: #d6be8c; color: #16293d; }
.kc.dark { background: #16293d; border-color: #16293d; color: #f1ede6; }
.dia { display: inline-block; width: 9px; height: 9px; background: #d6be8c; transform: rotate(45deg); flex: none; }
.dia.hollow { background: transparent; box-shadow: inset 0 0 0 1.5px #d6be8c; }
.nplate { position: absolute; transform: translate(-50%, -100%); padding: 3px 12px; border-radius: 99px; background: rgba(8, 14, 22, .42); font-weight: 700; font-size: 16px;
  white-space: nowrap; text-shadow: 0 1px 2px rgba(0,0,0,.6); }
.nplate.party { color: #a6dcf2; }
.nplate.npc { color: #f0d79d; }
.nplate span { font-size: 12.5px; font-weight: 600; color: #c8d0d6; margin-left: 7px; }

/* ---------- A: glass edge */
.crow { position: absolute; left: 0; right: 0; display: flex; justify-content: center; pointer-events: none; }
.crow > * { position: relative !important; left: auto !important; top: auto !important; }
.a-glass { position: absolute; background: linear-gradient(180deg, rgba(22, 37, 55, .80), rgba(11, 20, 31, .76)); backdrop-filter: blur(14px) saturate(1.2);
  -webkit-backdrop-filter: blur(14px) saturate(1.2); border: 1px solid rgba(241,237,230,.16); border-radius: 16px;
  box-shadow: 0 12px 30px rgba(3, 8, 14, .30), inset 0 1px 0 rgba(255,255,255,.08); }
.a-me { width: 384px; height: 104px; }
.a-ally { width: 470px; height: 132px; }
.a-face { position: absolute; left: 16px; top: 16px; width: 72px; height: 72px; border-radius: 50%; background-color: #26394e; background-size: 60px 60px;
  background-position: 6px 8px; background-repeat: no-repeat; image-rendering: pixelated; box-shadow: 0 0 0 2px #d6be8c, 0 0 0 5px rgba(10, 18, 28, .55); }
.a-ally .a-face { width: 96px; height: 96px; background-size: 80px 80px; background-position: 8px 10px; }
.a-lv { position: absolute; left: 66px; top: 68px; min-width: 28px; height: 24px; padding: 0 6px; border-radius: 12px; background: #d6be8c; color: #16293d;
  font: 700 13px "Pretendard", "Malgun Gothic", sans-serif; display: grid; place-items: center; box-shadow: 0 0 0 3px rgba(14,25,38,.9); }
.a-ally .a-lv { left: 86px; top: 92px; }
.a-name { position: absolute; left: 104px; top: 13px; font-size: 21px; font-weight: 700; white-space: nowrap; }
.a-ally .a-name { left: 132px; top: 16px; font-size: 23px; }
.a-name span { font-size: 13.5px; font-weight: 600; color: #aab6bf; margin-left: 9px; }
.a-row { position: absolute; left: 104px; right: 16px; display: flex; align-items: center; gap: 8px; height: 18px; }
.a-ally .a-row { left: 132px; right: 20px; }
.a-row b { font: 700 11.5px "Pretendard", "Malgun Gothic", sans-serif; color: #d6be8c; width: 20px; letter-spacing: .04em; }
.a-row em { font: 600 13px "Pretendard", "Malgun Gothic", sans-serif; font-style: normal; color: #e7e2d9; min-width: 34px; text-align: right; }
.a-row b.res { width: auto; margin-left: 22px; }
.a-bar { position: relative; flex: 1; height: 8px; border-radius: 4px; background: rgba(255,255,255,.10); box-shadow: inset 0 1px 2px rgba(0,0,0,.35); overflow: hidden; }
.a-bar i { position: absolute; left: 0; top: 0; bottom: 0; border-radius: 4px; }
.a-bar.hp i { background: linear-gradient(90deg, #6fbf63, #a9e08a); }
.a-bar.sp i { background: linear-gradient(90deg, #5d9fdb, #9fd0f5); }
.a-bar.big { height: 12px; border-radius: 6px; }
.a-bar.big i { border-radius: 6px; }
.a-bar s { position: absolute; top: 0; bottom: 0; background: #e3705d; }
.a-bar.thin { position: absolute; left: 52px; top: 30px; width: 150px; height: 5px; flex: none; }
.a-bar.thin i { background: linear-gradient(90deg, #6fbf63, #a9e08a); }
.a-bar.foe { height: 6px; width: 120px; flex: none; margin-top: 5px; }
.a-bar.foe i { background: linear-gradient(90deg, #d9604d, #f08a6e); }
.a-pm { width: 300px; height: 46px; border-radius: 23px; }
.a-pface { position: absolute; left: 6px; top: 6px; width: 34px; height: 34px; border-radius: 50%; background-color: #26394e; background-size: 60px 60px;
  background-position: 50% 92%; image-rendering: pixelated; box-shadow: 0 0 0 1.5px rgba(166, 220, 242, .8); }
.a-pface.big { width: 48px; height: 48px; left: 18px; top: 18px; background-size: 60px 60px; background-position: 50% 88%; }
.a-pname { position: absolute; left: 52px; top: 5px; font-size: 15.5px; font-weight: 700; color: #a6dcf2; white-space: nowrap; }
.a-pname span { font: 600 12px "Pretendard", "Malgun Gothic", sans-serif; color: #aab6bf; margin-left: 7px; }
.a-map { width: 252px; height: 252px; border-radius: 24px; }
.a-mapimg { position: absolute; inset: 8px; border-radius: 17px; background-size: 236px 236px; }
.a-mapvig { position: absolute; inset: 8px; border-radius: 17px; box-shadow: inset 0 0 0 1px rgba(0,0,0,.25), inset 0 0 26px rgba(8, 14, 22, .55); }
.a-me-arrow { position: absolute; left: 116px; top: 132px; filter: drop-shadow(0 1px 2px rgba(0,0,0,.6)); }
.a-north { position: absolute; left: 50%; top: -11px; transform: translateX(-50%); width: 30px; height: 22px; border-radius: 11px; background: #16293d;
  border: 1px solid rgba(214,190,140,.8); color: #d6be8c; font: 700 12px "Pretendard", sans-serif; display: grid; place-items: center; }
.a-zoom { position: absolute; right: 14px; bottom: 14px; display: grid; gap: 4px; }
.a-zoom span { width: 26px; height: 26px; border-radius: 8px; background: rgba(12, 22, 34, .72); border: 1px solid rgba(241,237,230,.18); display: grid; place-items: center; font: 600 17px sans-serif; line-height: 1; }
.a-maphint { position: absolute; left: 14px; bottom: 14px; display: flex; align-items: center; gap: 6px; padding: 3px 9px 3px 3px; border-radius: 9px; background: rgba(12, 22, 34, .72); font-size: 13px; font-weight: 600; }
.a-maphint .kc { min-width: 22px; height: 20px; font-size: 11.5px; }
.a-mk { position: absolute; display: grid; place-items: center; font-style: normal; }
.a-mk.quest { width: 22px; height: 22px; border-radius: 50%; background: #e8c46e; color: #16293d; font: 800 14px "Pretendard", sans-serif; box-shadow: 0 0 0 2px #16293d; }
.a-mk.party { width: 10px; height: 10px; border-radius: 50%; background: #8fd3f0; box-shadow: 0 0 0 2px #16293d; }
.a-mk.portal { width: 18px; height: 18px; border-radius: 50%; border: 3px solid #7fe0d6; box-shadow: 0 0 8px #7fe0d6; }
.a-loc { position: absolute; display: flex; align-items: center; gap: 9px; white-space: nowrap; }
.a-loc b { font-size: 21px; font-weight: 700; }
.a-loc span { font-size: 15.5px; color: #d5dade; font-weight: 600; }
.a-loc span::before { content: "·"; margin-right: 8px; color: #d6be8c; }
.a-quest { position: absolute; width: 360px; padding: 14px 0 16px 40px; background: linear-gradient(270deg, rgba(10, 18, 28, .66) 35%, rgba(10, 18, 28, 0)); border-radius: 0 16px 16px 0; }
.a-qhead { display: flex; align-items: center; gap: 8px; font-size: 13.5px; color: #d6be8c; letter-spacing: .18em; font-weight: 700; margin-bottom: 10px; }
.a-qhead em { font: 700 11.5px "Pretendard", sans-serif; font-style: normal; letter-spacing: 0; background: rgba(214,190,140,.22); color: #f0d79d; border-radius: 8px; padding: 1px 7px; }
.a-qhead .r { margin-left: auto; display: flex; align-items: center; gap: 6px; letter-spacing: .02em; color: #c8d0d6; font-size: 13px; padding-right: 16px; }
.a-q { display: flex; align-items: center; gap: 10px; font-size: 18.5px; margin-top: 4px; }
.a-q .tag { font-size: 11.5px; font-weight: 700; color: #f0d79d; border: 1px solid rgba(214,190,140,.6); border-radius: 6px; padding: 0 6px; text-shadow: none; }
.a-qobj { font-size: 15px; color: #dfe3e6; padding-left: 19px; margin: 3px 0 10px; }
.a-qobj em { font-style: normal; color: #f0d79d; margin-left: 6px; font-family: "Pretendard", sans-serif; font-weight: 600; }
.a-qobj .done { color: #b9e6a4; }
.a-qobj .done::before { content: "✓ "; }
.a-dock { display: flex; align-items: center; gap: 4px; padding: 7px; border-radius: 22px; }
.a-btn { position: relative; width: 52px; height: 52px; border-radius: 15px; display: grid; place-items: center; }
.a-btn.on { background: rgba(241,237,230,.13); box-shadow: inset 0 0 0 1px rgba(214,190,140,.75); transform: translateY(-3px); }
.a-btn.on::after { content: ""; position: absolute; left: 16px; right: 16px; bottom: 5px; height: 2px; border-radius: 1px; background: #d6be8c; }
.a-div { width: 1px; height: 30px; background: rgba(241,237,230,.2); margin: 0 6px; }
.a-tiprow { position: absolute; bottom: 64px; left: -60px; right: -60px; display: flex; justify-content: center; }
.a-tip { display: flex; align-items: center; gap: 9px; padding: 7px 10px 7px 14px; border-radius: 11px; white-space: nowrap; font-size: 15px; font-weight: 700;
  background: rgba(12, 22, 34, .94); border: 1px solid rgba(214,190,140,.55); box-shadow: 0 8px 18px rgba(0,0,0,.35); }
.a-chat { position: absolute; width: 560px; }
.a-lines { display: grid; gap: 5px; padding: 12px 18px 12px 14px; margin: 0 0 10px -14px; border-radius: 14px;
  background: linear-gradient(90deg, rgba(8, 14, 22, .62), rgba(8, 14, 22, .40) 62%, rgba(8, 14, 22, 0)); }
.a-line { font-size: 15.5px; font-weight: 600; color: #ece8e0; white-space: nowrap; }
.a-line b { color: #fff; margin-right: 6px; }
.ch { display: inline-block; font: 700 11.5px "Pretendard", sans-serif; border-radius: 5px; padding: 1px 6px; margin-right: 8px; vertical-align: 2px; text-shadow: none; }
.ch.sys { background: rgba(214,190,140,.24); color: #f0d79d; }
.ch.all { background: rgba(241,237,230,.16); color: #e7e2d9; }
.ch.party { background: rgba(120, 200, 235, .22); color: #a6dcf2; }
.ch.whisper { background: rgba(214, 150, 210, .22); color: #f0b8e8; }
.a-chatin { position: relative; display: inline-flex; align-items: center; gap: 9px; height: 42px; padding: 0 16px 0 10px; border-radius: 21px; font-size: 15px; font-weight: 600; color: #c8d0d6; }
.a-chatbox { width: 600px; height: 360px; border-radius: 18px; }
.a-tabs { position: absolute; left: 16px; right: 16px; top: 12px; display: flex; gap: 4px; font-size: 14.5px; font-weight: 700; color: #aab6bf; border-bottom: 1px solid rgba(241,237,230,.12); }
.a-tabs span { padding: 6px 12px 9px; }
.a-tabs span.on { color: #f1ede6; box-shadow: inset 0 -2px 0 #d6be8c; }
.a-tabs .add { margin-left: auto; color: #d6be8c; }
.a-lines.open { position: absolute; left: 18px; right: 18px; top: 56px; bottom: 64px; background: none; padding: 0; margin: 0; align-content: end; gap: 6px; }
.a-input { position: absolute; left: 12px; right: 12px; bottom: 12px; height: 44px; border-radius: 12px; background: rgba(4, 10, 18, .45); border: 1px solid rgba(214,190,140,.55);
  display: flex; align-items: center; gap: 10px; padding: 0 10px; }
.a-input .chan { font: 700 13px "Pretendard", sans-serif; padding: 4px 9px; border-radius: 7px; background: rgba(120, 200, 235, .2); color: #a6dcf2; }
.a-input .field { flex: 1; font-size: 15.5px; font-weight: 600; }
.caret { display: inline-block; width: 2px; height: 18px; background: #f1ede6; margin-left: 2px; vertical-align: -3px; }
.a-prompt { display: flex; align-items: center; gap: 10px; height: 44px; padding: 0 16px 0 10px; border-radius: 22px; font-size: 17px; font-weight: 700; }
.a-banner { position: absolute; left: 0; right: 0; top: 186px; display: grid; justify-items: center; gap: 8px; }
.a-banner .glow { position: absolute; left: 50%; top: 50%; width: 900px; height: 230px; transform: translate(-50%, -50%); background: radial-gradient(closest-side, rgba(8, 14, 22, .55), rgba(8, 14, 22, 0)); }
.a-banner .eye { position: relative; font-size: 17px; letter-spacing: .5em; color: #f0d79d; font-weight: 700; padding-left: .5em; }
.a-banner .title { position: relative; display: flex; align-items: center; gap: 22px; }
.a-banner .title b { font-size: 52px; font-weight: 700; text-shadow: 0 2px 4px rgba(0,0,0,.6), 0 0 22px rgba(0,0,0,.4); letter-spacing: .04em; }
.a-banner .title i { width: 150px; height: 1.5px; background: linear-gradient(90deg, rgba(214,190,140,0), #d6be8c); position: relative; }
.a-banner .title i:last-child { background: linear-gradient(270deg, rgba(214,190,140,0), #d6be8c); }
.a-banner .sub { position: relative; font-size: 15.5px; color: #dfe3e6; font-weight: 600; }
.a-toast { display: flex; align-items: center; gap: 14px; height: 64px; padding: 0 22px 0 12px; border-radius: 20px; white-space: nowrap; }
.a-toast .ic { width: 42px; height: 42px; border-radius: 12px; background: rgba(214,190,140,.2); display: grid; place-items: center; }
.a-toast em { display: block; font: 700 12px "Pretendard", sans-serif; font-style: normal; color: #f0d79d; letter-spacing: .08em; }
.a-toast b { font-size: 17px; margin-right: 10px; }
.a-toast span { font-size: 15px; color: #dfe3e6; font-weight: 600; }
.a-toast .ok { font-style: normal; color: #b9e6a4; font-size: 22px; margin-left: 4px; }
.a-invite { width: 384px; height: 140px; }
.a-invite em { position: absolute; left: 80px; top: 18px; font: 700 12px "Pretendard", sans-serif; font-style: normal; color: #f0d79d; letter-spacing: .1em; }
.a-invite > b { position: absolute; left: 80px; top: 38px; font-size: 17px; }
.a-invite .btns { position: absolute; left: 80px; top: 76px; display: flex; gap: 8px; }
.a-invite .btns span { display: flex; align-items: center; gap: 8px; height: 38px; padding: 0 16px 0 7px; border-radius: 19px; font-size: 15px; font-weight: 700; }
.a-invite .yes { background: #d6be8c; color: #16293d; }
.a-invite .no { border: 1px solid rgba(241,237,230,.3); }
.a-invite .timer { position: absolute; left: 20px; right: 20px; bottom: 9px; height: 3px; border-radius: 2px; background: rgba(255,255,255,.1); }
.a-invite .timer i { position: absolute; left: 0; top: 0; bottom: 0; border-radius: 2px; background: #d6be8c; }
.a-turn { display: flex; align-items: center; gap: 12px; height: 56px; padding: 0 24px; border-radius: 28px; white-space: nowrap; }
.a-turn em { font: 700 13px "Pretendard", sans-serif; font-style: normal; letter-spacing: .24em; color: #d6be8c; }
.a-turn b { font-size: 28px; font-family: "Pretendard", sans-serif; }
.a-turn i { width: 1px; height: 24px; background: rgba(241,237,230,.25); margin: 0 4px; }
.a-turn span { font-size: 18px; font-weight: 700; }
.a-turn span.foe { color: #f39a85; }
.a-order { display: flex; align-items: center; gap: 8px; }
.a-order .o { position: relative; width: 48px; height: 48px; }
.a-order .a-pface { left: 0; top: 0; width: 48px; height: 48px; background-size: 60px 60px; background-position: -6px -2px; box-shadow: 0 0 0 2px rgba(241,237,230,.5), 0 4px 10px rgba(0,0,0,.4); }
.a-order .cur .a-pface { box-shadow: 0 0 0 3px #d6be8c, 0 0 14px rgba(214,190,140,.7); }
.a-order .foe .a-pface { box-shadow: 0 0 0 2px #e3705d, 0 4px 10px rgba(0,0,0,.4); background-size: 64px 64px; background-position: -8px -6px; }
.a-order .arr { font-style: normal; color: rgba(241,237,230,.7); font-size: 24px; text-shadow: 0 1px 3px #000; }
.pips { display: inline-flex; gap: 5px; align-items: center; }
.pips i { width: 11px; height: 11px; transform: rotate(45deg); border: 1.5px solid rgba(241,237,230,.45); }
.pips.sp i.on { background: #8fc7f2; border-color: #8fc7f2; box-shadow: 0 0 8px rgba(143, 199, 242, .7); }
.pips.res i.on, .boost .pips i.on { background: #e8c46e; border-color: #e8c46e; box-shadow: 0 0 8px rgba(232, 196, 110, .6); }
.a-foe { position: absolute; transform: translate(-50%, -100%); display: grid; justify-items: center; white-space: nowrap; font-size: 16px; }
.a-foe b { font-weight: 700; }
.a-foe span { font: 600 12.5px "Pretendard", sans-serif; color: #f1c8bd; margin-left: 6px; }
.a-foe .nm { display: flex; align-items: baseline; }
.a-foe.act { padding: 6px 12px 8px; border-radius: 12px; background: rgba(40, 12, 10, .5); box-shadow: inset 0 0 0 1.5px #e3705d; }
.a-foe.act .chev { color: #f39a85; font-weight: 800; font-family: "Pretendard", sans-serif; }
.a-foe.sel { padding: 6px 12px 8px; border-radius: 12px; background: rgba(12, 22, 34, .55); box-shadow: inset 0 0 0 1.5px #d6be8c; }
.a-foe .chev { position: absolute; top: -30px; left: 50%; transform: translateX(-50%); font-style: normal; color: #f4d78f; font-size: 20px; text-shadow: 0 2px 4px #000; }
.a-cmd { right: 40px; bottom: 36px; width: 360px; padding: 12px; border-radius: 18px; }
.a-cmd .h { display: flex; align-items: center; justify-content: space-between; padding: 2px 6px 10px; border-bottom: 1px solid rgba(241,237,230,.1); margin-bottom: 6px; }
.a-cmd .h em { font: 700 12.5px "Pretendard", sans-serif; font-style: normal; letter-spacing: .1em; color: #d6be8c; }
.a-cmd .h span { display: flex; gap: 4px; }
.a-cmd .h .kc { min-width: 22px; height: 21px; font-size: 11px; padding: 0 5px; }
.a-cmd .c { position: relative; display: flex; align-items: center; gap: 14px; height: 52px; padding: 0 14px; border-radius: 12px; }
.a-cmd .c b { font-size: 19px; }
.a-cmd .c span { margin-left: auto; font: 600 12.5px "Pretendard", sans-serif; color: #aab6bf; }
.a-cmd .c.on { background: linear-gradient(90deg, rgba(214,190,140,.26), rgba(214,190,140,.06)); box-shadow: inset 3px 0 0 #d6be8c; }
.a-cmd .c.off { opacity: .38; }
.a-desc { right: 412px; bottom: 268px; width: 330px; padding: 16px 18px; border-radius: 16px; }
.a-desc em { display: block; font-style: normal; font-size: 19px; font-weight: 700; margin-bottom: 6px; }
.a-desc p { font-size: 15px; color: #dfe3e6; font-weight: 600; line-height: 1.5; }
.a-desc .boost { display: flex; align-items: center; gap: 10px; margin-top: 12px; padding-top: 12px; border-top: 1px solid rgba(241,237,230,.1); font-size: 14px; font-weight: 700; color: #d6be8c; }
.a-desc .boost .pips { margin-right: auto; margin-left: 4px; }
.a-desc .boost .kc { min-width: 22px; height: 21px; }
.a-action { display: flex; align-items: center; gap: 12px; height: 60px; padding: 0 26px 0 8px; border-radius: 30px; white-space: nowrap;
  box-shadow: inset 0 -2px 0 rgba(227, 112, 93, .8), 0 12px 30px rgba(3,8,14,.3); }
.a-action .a-pface { position: relative; left: 0; top: 0; width: 44px; height: 44px; background-size: 60px 60px; background-position: -8px -6px; box-shadow: 0 0 0 2px #e3705d; }
.a-action span { font-size: 15px; color: #f1c8bd; font-weight: 700; }
.a-action b { font-size: 22px; }
.a-parry { position: absolute; width: 0; height: 0; }
.a-parry i { position: absolute; border-radius: 50%; transform: translate(-50%, -50%) scaleY(.42); }
.a-parry .r1 { width: 230px; height: 230px; border: 3px solid rgba(244, 215, 143, .95); box-shadow: 0 0 18px rgba(244, 215, 143, .6); }
.a-parry .r2 { width: 300px; height: 300px; border: 2px dashed rgba(241, 237, 230, .55); }
.a-cue { padding: 12px 16px; border-radius: 16px; display: grid; gap: 10px; }
.a-cue .bar { position: relative; width: 300px; height: 12px; border-radius: 6px; background: rgba(255,255,255,.1); }
.a-cue .win { position: absolute; left: 62%; width: 16%; top: -2px; bottom: -2px; border-radius: 4px; background: rgba(232, 196, 110, .55); box-shadow: 0 0 10px rgba(232,196,110,.6); }
.a-cue .needle { position: absolute; left: 54%; top: -6px; width: 3px; height: 24px; border-radius: 2px; background: #fff; box-shadow: 0 0 6px #fff; }
.a-cue .keys { display: flex; justify-content: center; gap: 18px; font-size: 15px; font-weight: 700; }
.a-cue .keys span { display: flex; align-items: center; gap: 7px; }

/* ---------- B: adventurer's journal */
.b-card { position: absolute; background: linear-gradient(180deg, #f1e6cd, #e1d1ae); border: 2px solid #6a4e2f; border-radius: 10px; color: #3b2a1a;
  box-shadow: 0 5px 0 rgba(44, 28, 14, .35), 0 12px 24px rgba(0,0,0,.25), inset 0 0 0 4px rgba(241, 230, 205, .9), inset 0 0 0 5px rgba(106, 78, 47, .45); }
.b-me { width: 404px; height: 112px; }
.b-face { position: absolute; left: 14px; top: 16px; width: 76px; height: 76px; border-radius: 50%; background-color: #3a4f63; background-size: 64px 64px; background-position: 6px 8px;
  image-rendering: pixelated; box-shadow: 0 0 0 4px #c9a45c, 0 0 0 6px #6a4e2f; }
.b-name { position: absolute; left: 108px; top: 14px; font-size: 22px; font-weight: 700; white-space: nowrap; }
.b-name span { font-size: 13.5px; color: #6c5539; margin-left: 6px; }
.b-row { position: absolute; left: 108px; right: 18px; display: flex; align-items: center; gap: 8px; }
.b-row b { font-size: 13px; color: #6a4e2f; width: 30px; }
.b-row em { font-style: normal; font-size: 13.5px; font-weight: 700; min-width: 32px; text-align: right; }
.b-bar { position: relative; flex: 1; height: 11px; border-radius: 6px; background: #3b2a1a; box-shadow: inset 0 1px 3px #000; padding: 2px; }
.b-bar i { position: absolute; left: 2px; top: 2px; bottom: 2px; border-radius: 4px; }
.b-bar.hp i { background: linear-gradient(180deg, #e06a55, #a8342a); }
.b-bar.sp i { background: linear-gradient(180deg, #6fa0dc, #335f9c); }
.b-pm { display: flex; align-items: center; gap: 10px; height: 40px; padding: 0 14px 0 4px; font-size: 15.5px; font-weight: 700; border-radius: 20px; }
.b-pm span { font-size: 12.5px; color: #6c5539; }
.b-pface { width: 32px; height: 32px; border-radius: 50%; background-size: 60px 60px; background-position: 50% 92%; image-rendering: pixelated; box-shadow: 0 0 0 2px #c9a45c; }
.b-compass { position: absolute; width: 270px; height: 270px; border-radius: 50%;
  background: repeating-conic-gradient(from 0deg, #4a3520 0deg 1.2deg, transparent 1.2deg 7.5deg), radial-gradient(circle, transparent 112px, #e3c27a 113px, #9a7434 124px, #5c4122 126px, #c99d4f 129px, #6b4c26 135px);
  box-shadow: 0 10px 22px rgba(0,0,0,.4); }
.b-mapimg { position: absolute; inset: 22px; border-radius: 50%; background-size: 226px 226px; box-shadow: inset 0 0 22px rgba(60, 40, 20, .6); filter: sepia(.18); }
.b-compass .n { position: absolute; left: 50%; top: -2px; transform: translateX(-50%); font-style: normal; font-weight: 700; color: #b8433a; font-size: 18px; background: #f1e6cd; border: 2px solid #6a4e2f; border-radius: 50%; width: 30px; height: 30px; display: grid; place-items: center; }
.b-arrow { position: absolute; left: 125px; top: 140px; }
.b-ribbon { position: absolute; height: 40px; padding: 0 34px; display: grid; place-items: center; background: #7a2e25; color: #f6ecd2; font-size: 16.5px; font-weight: 700; white-space: nowrap;
  clip-path: polygon(0 0, 100% 0, calc(100% - 14px) 50%, 100% 100%, 0 100%, 14px 50%); box-shadow: inset 0 2px 0 rgba(255,255,255,.15); }
.b-note { position: absolute; width: 330px; padding: 30px 24px 26px; background: linear-gradient(180deg, #f5ecd6, #e8dab9); color: #3b2a1a; transform: rotate(-1.6deg);
  box-shadow: 0 10px 20px rgba(0,0,0,.35); clip-path: polygon(0 0, 100% 0, 100% 92%, 94% 100%, 86% 95%, 74% 100%, 62% 96%, 50% 100%, 38% 95%, 26% 100%, 14% 96%, 4% 100%, 0 94%); }
.b-note .pin { position: absolute; left: 50%; top: 9px; width: 16px; height: 16px; border-radius: 50%; background: radial-gradient(circle at 35% 35%, #f08a7a, #a8342a); transform: translateX(-50%); box-shadow: 0 2px 3px rgba(0,0,0,.4); }
.b-note b { display: block; font-size: 19px; }
.b-note b.s { font-size: 17px; margin-top: 12px; }
.b-note p { font-size: 15px; color: #5a4430; margin-top: 4px; font-weight: 600; }
.b-strap { position: absolute; display: flex; gap: 10px; padding: 9px 16px; border-radius: 14px; background: linear-gradient(180deg, #5b3f2a, #3f2a1b);
  box-shadow: inset 0 0 0 2px #2c1c10, inset 0 0 0 4px rgba(201, 164, 92, .0), 0 8px 18px rgba(0,0,0,.4); outline: 1.5px dashed rgba(214, 180, 120, .7); outline-offset: -7px; }
.b-strap span { width: 54px; height: 54px; border-radius: 50%; display: grid; place-items: center; background: radial-gradient(circle at 40% 35%, #3d4e5e, #1d2a36 70%);
  box-shadow: 0 0 0 3px #c9a45c, 0 0 0 5px #5c4122, 0 3px 6px rgba(0,0,0,.5); }
.b-scroll { position: absolute; width: 560px; padding: 18px 34px; background: linear-gradient(180deg, #f2e7cf, #e3d4b2); color: #3b2a1a; border-radius: 4px;
  box-shadow: -14px 0 0 -4px #c9b48a, 14px 0 0 -4px #c9b48a, 0 10px 20px rgba(0,0,0,.35); font-size: 15px; font-weight: 600; display: grid; gap: 5px; }
.b-scroll em { font-style: normal; font-size: 12px; font-weight: 700; color: #6a4e2f; border: 1px solid #6a4e2f; border-radius: 4px; padding: 0 5px; margin-right: 8px; }
.b-scroll em.sys { color: #8a5a10; border-color: #8a5a10; }
.b-scroll em.party { color: #1e5a86; border-color: #1e5a86; }
.b-scroll b { margin-right: 6px; }
.b-plate { position: absolute; transform: translate(-50%, -100%); font-size: 16px; font-weight: 700; color: #fbf3df; white-space: nowrap; padding: 2px 12px;
  background: rgba(70, 45, 25, .62); border: 1px solid rgba(201, 164, 92, .7); border-radius: 4px; }
.b-plate.party { color: #bfe3f5; }
.b-plate.npc { color: #f5d98e; }
.b-plate span { font-size: 12.5px; color: #e0cfa9; margin-left: 6px; }
.b-sign { position: absolute; display: flex; align-items: center; gap: 10px; height: 46px; padding: 0 18px 0 8px; background: linear-gradient(180deg, #8a6038, #5d3e22); color: #fbf3df;
  border-radius: 6px; font-size: 17px; font-weight: 700; box-shadow: inset 0 0 0 2px #3b2615, 0 6px 12px rgba(0,0,0,.4); }
.b-sign .key { width: 30px; height: 30px; border-radius: 50%; display: grid; place-items: center; background: radial-gradient(circle at 40% 35%, #f0d48c, #b08636); color: #3b2615; font-family: "Pretendard", sans-serif; }

/* ---------- C: clear overlay */
.c2.ts, .c2 .ts { text-shadow: 0 0 2px rgba(0,0,0,.95), 0 1px 3px rgba(0,0,0,.9), 0 0 16px rgba(0,0,0,.6); }
.c-me { position: absolute; width: 260px; }
.c-me > b { font-size: 22px; display: block; }
.c-me > span { font-size: 14px; color: #e3e7ea; font-weight: 600; }
.c-line { position: relative; height: 3px; width: 220px; background: rgba(255,255,255,.22); margin-top: 8px; border-radius: 2px; box-shadow: 0 1px 2px rgba(0,0,0,.5); }
.c-line i { position: absolute; left: 0; top: 0; bottom: 0; border-radius: 2px; }
.c-line.hp i { background: #b9ef9c; }
.c-line.sp i { background: #a9d6fb; }
.c-line.sm { width: 120px; margin-top: 4px; }
.c-pm { margin-top: 16px; }
.c-pm b { font-size: 15px; color: #c3e8f8; }
.c-pm + .c-pm { margin-top: 10px; }
.c-map { position: absolute; width: 204px; height: 204px; }
.c-map .img { position: absolute; inset: 0; border-radius: 50%; background-size: 204px 204px; -webkit-mask: radial-gradient(circle, #000 58%, transparent 71%); mask: radial-gradient(circle, #000 58%, transparent 71%); opacity: .92; }
.c-arrow { position: absolute; left: 93px; top: 100px; }
.c-map .n { position: absolute; left: 50%; top: 2px; transform: translateX(-50%); font-style: normal; font-weight: 700; font-size: 14px; }
.c-loc { position: absolute; width: 204px; text-align: center; font-size: 17px; font-weight: 700; }
.c-loc span { display: block; font-size: 13.5px; color: #e3e7ea; font-weight: 600; }
.c-quest { position: absolute; width: 300px; text-align: right; }
.c-quest b { display: block; font-size: 18px; }
.c-quest b.s { margin-top: 14px; font-size: 16px; opacity: .85; }
.c-quest p { font-size: 14.5px; color: #e7eaec; font-weight: 600; margin-top: 2px; }
.c-chat { position: absolute; font-size: 15px; font-weight: 600; display: grid; gap: 4px; }
.c-chat em { font-style: normal; font-size: 12px; color: #e3e7ea; margin-right: 8px; border-bottom: 1px solid rgba(255,255,255,.5); }
.c-chat em.party { color: #c3e8f8; border-color: #c3e8f8; }
.c-hint { position: absolute; display: flex; align-items: center; gap: 8px; font-size: 14px; font-weight: 600; color: #eef1f3; }
.c-hint i { width: 1px; height: 14px; background: rgba(255,255,255,.5); margin: 0 6px; }
.c-plate { position: absolute; transform: translate(-50%, -100%); font-size: 15px; font-weight: 700; white-space: nowrap; }
.c-plate.party { color: #c3e8f8; }
.c-plate.npc { color: #f5dfa8; }
.c-prompt { position: absolute; display: flex; align-items: center; gap: 8px; font-size: 16px; font-weight: 700; }
'''

JS = r'''
const ro = new ResizeObserver(es => { for (const e of es) e.target.style.setProperty('--k', e.contentRect.width / 1920); });
document.querySelectorAll('.stage').forEach(s => ro.observe(s));
document.querySelectorAll('.seg button[data-for]').forEach(b => b.addEventListener('click', () => {
  const stage = document.getElementById(b.dataset.for);
  stage.dataset.view = b.dataset.view;
  b.parentElement.querySelectorAll('button').forEach(x => x.setAttribute('aria-pressed', x === b ? 'true' : 'false'));
}));
const safe = document.getElementById('safe');
safe.addEventListener('click', () => {
  const on = document.body.classList.toggle('safe-on');
  safe.setAttribute('aria-pressed', on ? 'true' : 'false');
});
'''


# ---------------------------------------------------------------- page

def figure(fid, bg, layers, caption, now=None, label=''):
    return f'''<figure class="cap">
<div class="bar-ctl">{views(fid, now is not None)}</div>
{frame(fid, bg, layers, now, label)}
<figcaption>{caption}</figcaption></figure>'''


def build():
    body = f'''
<header class="top">
  <div class="eyebrow">ProjectM · 인게임 HUD 리뉴얼 · 2026-10-14 · 제안</div>
  <h1>인게임 HUD를 처음부터 다시 짠 시안 3종</h1>
  <p class="lede">필드와 전투에서 늘 떠 있는 HUD를 새로 구성했습니다. 지금 HUD의 그림과 위젯은 쓰지 않고, <b>무엇을 어디에 둘지부터 다시 정했습니다.</b>
  시안은 모두 실제 플레이 캡처(1920×1080) 위에 1080p 실제 크기로 그렸습니다. 추천은 <b>A 글래스 엣지</b>입니다.
  생성·선택 화면과 같은 남색 유리로 네 모서리만 쓰고, 화면 가운데는 비웁니다.</p>
  <div class="bar-ctl"><button type="button" class="toggle" id="safe" aria-pressed="false">안전 여백·가운데 영역 보기</button>
  <span class="note">점선 = 1080p 기준 안전 여백(좌우 40, 위아래 36). 파란 영역 = 캐릭터·NPC·전투가 사는 곳(HUD가 들어가지 않음).</span></div>
</header>

<section class="block">
  <div class="head"><span class="tag">01</span><h2>지금 HUD에서 고칠 점</h2></div>
  <div class="grid2">
    {figure('now-field', 'village_now', marks(FIELD_MARKS), '<b>필드</b> · 리베르그로브 남문 (지금 HUD)')}
    {figure('now-battle', 'battle_now', marks(BATTLE_MARKS), '<b>전투</b> · 펀브룩 초입 조우 (지금 HUD)')}
  </div>
  <div class="two">
    <ol class="issues">
      <li><span class="n">1</span><div>미니맵·지역 이름·퀘스트 창이 <b>왼쪽 위에 세로로 쌓여</b> 화면 왼쪽 1/5을 덮고, 퀘스트 창이 미니맵 아래를 가립니다.</div></li>
      <li><span class="n">2</span><div>지역 이름 띠가 작고, 미니맵과 따로 놀아서 <b>어디에 있는지</b>가 한눈에 안 들어옵니다.</div></li>
      <li><span class="n">3</span><div>오른쪽 위 바로가기 6개가 <b>큰 팔각 버튼에 그림만</b> 있어 무겁고, 처음 보면 무엇인지 모릅니다. 키도 안 보입니다.</div></li>
      <li><span class="n">4</span><div>이름표가 작고 받침이 없어 <b>밝은 돌길 위에서 안 읽힙니다.</b> 내 이름·파티원·NPC 구분도 없습니다.</div></li>
      <li><span class="n">5</span><div>HUD 숨김 버튼만 홀로 오른쪽 아래에 떠 있습니다. <b>내 레벨·HP·SP는 필드에 아예 없습니다.</b></div></li>
      <li><span class="n">6</span><div>채팅·파티 창은 온라인에서만 뜨는데, 들어갈 자리가 정해져 있지 않아 <b>다른 창과 겹칠 위험</b>이 있습니다.</div></li>
    </ol>
    <ol class="issues" start="7">
      <li><span class="n">7</span><div>전투 중에도 필드의 <b>지역 이름·퀘스트 창이 그대로 남아</b> 전투 화면을 가립니다.</div></li>
      <li><span class="n">8</span><div>전투 명령이 <b>화면 한가운데(캐릭터 옆)</b>에 떠서 전장을 가립니다. 못 쓰는 명령(스킬·교체)이 흐린 글씨로만 있고 이유가 없습니다.</div></li>
      <li><span class="n">9</span><div>아군 HP가 이름·숫자 없이 <b>막대 두 줄</b>로만 떠 있어 무엇인지 알기 어렵습니다.</div></li>
      <li><span class="n">10</span><div>적 이름·HP가 화면 오른쪽 위로 떨어져 있어 <b>어느 적인지</b> 이어서 보기 어렵습니다. 대상 표시도 없습니다.</div></li>
      <li><span class="n">11</span><div>턴 표시는 있지만 <b>행동 순서</b>가 없어 다음에 누가 움직이는지 모릅니다.</div></li>
      <li><span class="n">·</span><div>생성·선택 화면(남색 유리), 설정(장식 테두리), HUD(청록 팔각)가 <b>서로 다른 재료</b>로 되어 있어 한 게임처럼 보이지 않습니다.</div></li>
    </ol>
  </div>
</section>

<section class="block">
  <div class="head"><span class="tag">02</span><h2>설계 원칙</h2></div>
  <div class="two">
    <ul class="tight">
      <li><b>가운데는 비운다.</b> 화면 가운데(위 파란 영역)는 캐릭터·NPC·전투의 자리입니다. HUD는 들어가지 않고, 이름표·상호작용 안내만 대상 옆에 잠깐 붙습니다.</li>
      <li><b>모서리마다 맡는 일이 하나.</b> 왼쪽 위 = 나와 파티, 오른쪽 위 = 어디·무엇을(지도·지역·퀘스트), 왼쪽 아래 = 사람(채팅), 오른쪽 아래 = 할 수 있는 것(메뉴·전투 명령).</li>
      <li><b>필요할 때만 크게.</b> 지역 진입 배너·퀘스트 갱신·초대는 몇 초 뜨고 사라집니다. 걷는 동안 채팅·퀘스트는 옅어지고, 멈추면 돌아옵니다.</li>
      <li><b>전투에 들어가면 필드 HUD는 접는다.</b> 전투 HUD는 같은 재료와 같은 모서리 규칙을 따릅니다(명령은 오른쪽 아래, 아군은 왼쪽 아래).</li>
    </ul>
    <ul class="tight">
      <li><b>생성·선택 화면과 같은 재료.</b> 남색 반투명 유리, 아이보리 글자, 금색 포인트, 게임 글꼴(MaruBuri). 숫자·키 표시는 읽기 쉬운 고딕을 섞는 것을 제안합니다(글꼴 추가 필요).</li>
      <li><b>아이콘은 화면 도트와 같은 2px 격자.</b> 필드 스프라이트가 그림 1칸 = 2px이므로 아이콘도 15칸 그림을 2배로 찍습니다.</li>
      <li><b>모든 버튼에 키를 보인다.</b> 키캡을 함께 그리고, 패드에서는 같은 자리에 패드 버튼 그림이 나옵니다.</li>
      <li><b>해상도.</b> 1080p 기준으로 설계하고 높이에 맞춰 배율을 줍니다(생성·선택 화면과 같은 규칙). 21:9에서는 모서리가 바깥으로 벌어지고 가운데는 그대로입니다.</li>
    </ul>
  </div>
  <div class="palette">
    <span><i style="background:#16293d"></i>남색 #16293D</span><span><i style="background:rgba(22,36,52,.62)"></i>유리 62%</span>
    <span><i style="background:#f1ede6"></i>아이보리 #F1EDE6</span><span><i style="background:#aab6bf"></i>보조 #AAB6BF</span>
    <span><i style="background:#d6be8c"></i>금색 #D6BE8C</span><span><i style="background:#8fd17a"></i>HP</span><span><i style="background:#8fc7f2"></i>SP</span>
    <span><i style="background:#a6dcf2"></i>파티</span><span><i style="background:#e3705d"></i>적</span>
  </div>
  <div class="icons">{''.join('<img src="%s" alt="%s" title="%s">' % (u, n, n) for n, u in ICON.items())}</div>
  <p class="note">아이콘은 시안용 초안입니다(<code>Tools/HUDRenewal/make_icons.py</code>). 확정되면 같은 격자로 다듬어 게임 텍스처로 굽습니다.</p>
</section>

<section class="block">
  <div class="head"><span class="tag">03</span><h2>시안 3종 · 같은 장면 비교</h2></div>
  <p class="sub">리베르그로브 남문, 파티원 둘과 함께 대장장이 NPC 앞에 선 순간입니다. 이름·수치·퀘스트 문구는 자리 채움이고,
  키 표시(J·C·K·T·N·M 등)도 자리를 보여 주는 예시입니다. 실제로는 키 설정에 맞춰 나옵니다(소지품 I만 지금 키와 같음).</p>

  <div class="head"><span class="tag">A</span><h2>글래스 엣지</h2><span class="pick">추천</span></div>
  {figure('a-village', 'village', a_village(), '생성·선택 화면의 유리 시트를 네 모서리로 나눈 안. 소지품 버튼에 마우스를 올린 상태(키 <b>I</b> 표시)와 NPC 상호작용 안내가 함께 보입니다.', 'village_now', 'A 마을')}

  <div class="head"><span class="tag">B</span><h2>모험가 수첩</h2></div>
  {figure('b-village', 'village', b_village(), '양피지·가죽·놋쇠로 그린 안. 판타지 정취가 가장 강하지만, 불투명한 종이가 장면을 많이 가리고 생성·선택 화면과 재료가 달라집니다.', 'village_now', 'B 마을')}

  <div class="head"><span class="tag">C</span><h2>투명 오버레이</h2></div>
  {figure('c-village', 'village', c_village(), '테두리 없이 글자와 가는 선만 남긴 안. 장면이 가장 잘 보이지만, 밝은 돌길·하늘 위에서 글자가 묻히고 메뉴가 숨어 있어 처음 하는 사람이 찾기 어렵습니다.', 'village_now', 'C 마을')}

  <div class="tblwrap"><table class="cmp">
    <thead><tr><th></th><th>A 글래스 엣지</th><th>B 모험가 수첩</th><th>C 투명 오버레이</th></tr></thead>
    <tbody>
      <tr><th>인상</th><td class="win">깔끔한 HD-2D, 요즘 RPG</td><td>손때 묻은 판타지</td><td>장면이 주인공, 영화 같음</td></tr>
      <tr><th>밝은 장면 가독성</th><td class="win">좋음 (유리 흐림이 받침)</td><td class="win">좋음 (불투명 종이)</td><td>약함 (그림자만으로 받침)</td></tr>
      <tr><th>장면 가림</th><td>적음</td><td>많음 (종이·띠 면적이 큼)</td><td class="win">가장 적음</td></tr>
      <tr><th>생성·선택 화면과 통일</th><td class="win">같은 재료</td><td>다름 (두 화면도 다시 맞춰야 함)</td><td>절반</td></tr>
      <tr><th>파티·채팅 수용</th><td class="win">자리가 정해져 있음</td><td>종이가 넓어짐</td><td>넣으면 원칙이 깨짐</td></tr>
      <tr><th>새 그림 양</th><td class="win">적음 (유리 9분할 3장 + 아이콘)</td><td>많음 (테두리·놋쇠·종이·띠 손그림)</td><td class="win">가장 적음</td></tr>
      <tr><th>패드·처음 하는 사람</th><td class="win">버튼과 키가 늘 보임</td><td>버튼이 보임</td><td>메뉴가 숨어 있어 학습 필요</td></tr>
      <tr><th>위험</th><td>배경 흐림 비용 (저사양은 흐림 끄고 진하게)</td><td>장면과 경쟁, 화면이 무거움</td><td>정보 누락, 가독성</td></tr>
    </tbody></table></div>
</section>

<section class="block">
  <div class="head"><span class="tag">04</span><h2>추천안 A · 상황별 화면</h2></div>
  {figure('a-forest', 'forest', a_forest(), '<b>사냥터 진입.</b> 지역 배너가 가운데 위에 3초 뜨고, 퀘스트 갱신 알림이 맨 위에 뜹니다. 파티 초대는 파티 자리(왼쪽 위)에 카드로 옵니다. 걷는 중이라 채팅·퀘스트가 옅어진 상태입니다.', None, 'A 사냥터')}
  {figure('a-chat', 'village', a_village(hover=None, chat_open=True), '<b>채팅 열기.</b> Enter를 누르면 왼쪽 아래 줄이 창으로 펼쳐집니다(탭·채널·입력). 닫으면 다시 최근 세 줄만 남습니다.', None, 'A 채팅')}
  {figure('a-battle', 'battle', a_battle_cmd(), '<b>전투 · 행동 선택.</b> (가운데 위 파란 사각형은 전투 배경 그림의 하늘 카드 경계로, HUD가 아닙니다 — 아래 "함께 본 것" 참고.) 필드 HUD는 접히고, 가운데 위에 턴과 행동 순서, 왼쪽 아래 아군 카드, 오른쪽 아래 명령이 옵니다. 고른 명령의 설명과 SP 투입은 명령 옆 카드에, 대상 적은 금색 테두리와 화살표로 보입니다. 못 쓰는 명령은 이유를 같이 적습니다. 설명 문구·수치·직업 자원 이름은 자리 채움입니다.', 'battle_now', 'A 전투 명령')}
  {figure('a-parry', 'battle', a_battle_parry(), '<b>전투 · 적 행동과 패링.</b> 적의 기술 이름이 가운데 위 띠로 뜨고, 내 캐릭터 발밑에 막을 타이밍 막대와 키가 나옵니다. 받을 피해는 HP 막대에 붉게 미리 보입니다.', None, 'A 패링')}
  <p class="note">HUD 숨김(눈 버튼)을 누르면 오른쪽 아래 버튼 줄의 눈만 남습니다. 지금 HUD와 같은 동작입니다.</p>
</section>

<section class="block">
  <div class="head"><span class="tag">05</span><h2>A로 갈 때의 구성과 순서</h2></div>
  <div class="two">
    <div>
      <h3>새로 만드는 것</h3>
      <ul class="tight">
        <li>새 폴더 <code>/Game/Art/UI/HUD2026/</code>에 키트(유리 9분할·아이콘·키캡)와 위젯을 새로 만듭니다. 지금 <code>WBP_FieldHUD</code>·<code>WBP_BattleHUD</code>와 기존 HUD 그림은 쓰지 않습니다.</li>
        <li>생성·선택 화면처럼 스크립트(<code>Tools/HUDRenewal/run.ps1</code>)가 위젯을 처음부터 짜서, 다시 돌리면 같은 결과가 나오게 합니다.</li>
        <li>필드: 내 상태 판, 파티 줄, 미니맵 틀, 지역 줄, 퀘스트 추적, 채팅(접힘/펼침), 바로가기 줄, 이름표, 상호작용 안내, 알림, 지역 진입 배너.</li>
        <li>전투: 턴·행동 순서, 아군 카드, 적 표시(대상 포함), 명령 판과 설명 카드, 기술 이름 띠, 패링 타이밍.</li>
      </ul>
    </div>
    <div>
      <h3>그대로 쓰는 것 (로직)</h3>
      <ul class="tight">
        <li>C++ 클래스와 위젯 이름 계약은 유지합니다. 생김새만 새로 그리고, 데이터를 받는 코드는 그대로 탑니다.
          예: <code>UNU_FieldHUDWidget</code>(바로가기 6개·숨김 버튼), <code>UNU_BattleHUDWidget</code>(<code>Txt_Turn</code>·<code>Panel_Allies</code>·<code>Panel_TurnOrder</code>), 미니맵·지역·퀘스트·채팅·파티·알림 위젯.</li>
        <li>필드 내 상태 판은 이미 있는 <code>UNU_PlayerVitalWidget</code>(HP·SP 이름 계약)을 씁니다. 필드에서도 값이 들어오는지는 확인이 필요합니다.</li>
        <li>적용은 플레이어 컨트롤러·UI 설정의 <b>위젯 클래스 지정만 바꿉니다.</b> 되돌리기도 지정만 되돌리면 됩니다.</li>
      </ul>
    </div>
  </div>
  <div class="two">
    <div>
      <h3>C++에서 조금 더할 것</h3>
      <ul class="tight">
        <li>전투에 들어가면 필드 쪽 조각(지역·퀘스트·채팅)도 접기 (지금 7번 문제).</li>
        <li>걷는 동안 채팅·퀘스트를 옅게 (이동 상태만 보고 0.3초 페이드).</li>
        <li>지역 진입 배너: 지역 위젯이 이미 장소 변경을 받으므로 연출만 붙입니다.</li>
        <li>못 쓰는 전투 명령의 이유 문구, 다음 행동 순서 표시용 데이터 확인.</li>
      </ul>
    </div>
    <div>
      <h3>순서</h3>
      <ol class="steps">
        <li>키트: 유리 9분할, 아이콘 확정, 키캡·패드 그림</li>
        <li>필드 틀: 네 모서리 자리와 해상도 규칙</li>
        <li>필드 조각: 퀘스트·채팅·파티·이름표·상호작용·알림·배너</li>
        <li>전투 HUD: 턴·순서·아군·적·명령·패링</li>
        <li>검증: 16:9·16:10·21:9 캡처, 패드 이동, 온라인 파티·채팅</li>
      </ol>
    </div>
  </div>
  <p class="note">퀘스트 추적·시트 창 일부는 민기 님 작업 영역이고, 전투 명령·연출은 동현 님 작업과 겹칩니다. 새 위젯을 따로 만들고 지정만 바꾸는 방식이라 파일 충돌은 적지만, 바꾸는 시점은 같이 정하는 것이 좋습니다.</p>
</section>

<section class="block">
  <div class="head"><span class="tag">06</span><h2>정해 주실 것</h2></div>
  <ol class="decide">
    <li><div><b>시안</b> — A(추천) / B / C, 또는 섞기(예: A에 C의 "걸을 때 옅게"는 이미 넣었습니다).</div></li>
    <li><div><b>필드에 내 HP·SP를 보일지</b> — 지금은 없습니다. 전투가 턴제라 필드 HP가 의미 있는지에 따라 정합니다.</div></li>
    <li><div><b>미니맵 모양</b> — 둥근 사각(A) / 원(B·C).</div></li>
    <li><div><b>채팅 기본 상태</b> — 최근 세 줄만(추천) / 늘 창으로.</div></li>
    <li><div><b>전투 명령 자리</b> — 오른쪽 아래 고정(추천) / 지금처럼 캐릭터 옆.</div></li>
    <li><div><b>숫자·키용 고딕 글꼴 추가</b> — 지금 게임 글꼴은 MaruBuri 두 벌뿐입니다.</div></li>
  </ol>
</section>

<section class="block">
  <div class="head"><span class="tag">07</span><h2>함께 본 것 (HUD 밖)</h2></div>
  <ul class="tight">
    <li><b>전투 배경 하늘의 파란 사각형.</b> 이번 캡처의 전투 배경에서, 화면 가운데 위(1080p 기준 x 810~1085, y 85~185)에 하늘보다 밝은 파란 사각형이 보입니다.
      HUD 없이 찍은 화면에도 그대로 있어 배경 그림(하늘 카드의 경계로 보임) 쪽 문제입니다. 지금 HUD의 턴 표시 바로 아래라 눈에 띄지 않았던 것으로 보입니다. 배경 담당 확인이 필요합니다.</li>
    <li><b>캡처 방법.</b> 오프라인 플레이(서버 없음)로 찍었습니다. 플레이어가 기본 몸(회색)으로 나오고, 파티원은 개발용 가짜 원격 캐릭터입니다.
      시안의 초상화는 생성·선택 화면 캡처에서 그 화면의 도트 격자대로 잘라 썼습니다.</li>
  </ul>
</section>
'''
    return body


def font_faces(text):
    chars = set(text) | set(chr(c) for c in range(0x20, 0x7f)) | set('·…—–→←↑↓▾▼✓◆◇›')
    faces = []
    for weight, name in ((600, 'MaruBuri-SemiBold'), (700, 'MaruBuri-Bold')):
        font = TTFont(SRC / 'fonts' / (name + '.ttf'))
        options = subset.Options()
        options.flavor = 'woff2'
        options.layout_features = ['*']
        sub = subset.Subsetter(options)
        sub.populate(text=''.join(sorted(chars)))
        sub.subset(font)
        buf = io.BytesIO()
        font.flavor = 'woff2'
        font.save(buf)
        faces.append('@font-face { font-family: "MaruBuriNU"; font-weight: %d; font-display: swap; src: url(data:font/woff2;base64,%s) format("woff2"); }'
                     % (weight, base64.b64encode(buf.getvalue()).decode()))
    return '\n'.join(faces)


def main():
    body = build()
    text = html.unescape(re.sub(r'<[^>]+>', ' ', re.sub(r'url\([^)]*\)', '', body))) + CSS
    page = f'''<!doctype html>
<html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>인게임 HUD 리뉴얼 시안</title>
<style>
{font_faces(text)}
{''.join('.bg-%s{background-image:url(%s)}' % (k, v) for k, v in BG.items())}
{CSS}
</style></head>
<body><main class="wrap">
{body}
</main>
<script>{JS}</script>
</body></html>'''
    OUT.mkdir(parents=True, exist_ok=True)
    out = OUT / 'hud_proposal.html'
    out.write_text(page, encoding='utf-8')
    print('written', out, '%.1f MB' % (out.stat().st_size / 1e6))


if __name__ == '__main__':
    main()
