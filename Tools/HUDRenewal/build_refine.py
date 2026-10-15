# -*- coding: utf-8 -*-
"""HUD refinement proposal (v2): icon styles + the bottom-right dock + battle, after the first in-game pass.

Reuses build_proposal.py (concept A components, CSS, fonts, captures) and swaps its pixel icons for the vector sets
in hud_icons.py. Output: Docs/HUD_Refine_Proposal_20261014/hud_refine.html (one file, everything embedded).
"""
import html
import importlib.util
import pathlib
import re
import sys

from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import hud_icons as H  # noqa: E402

spec = importlib.util.spec_from_file_location('bp', HERE / 'build_proposal.py')
bp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bp)

ROOT = HERE.parents[1]
OUT = ROOT / 'Docs' / 'HUD_Refine_Proposal_20261014'
CAP = ROOT / 'Saved' / 'CharacterCreateGlass'

# in-game captures of the first pass (what the game shows today)
bp.BG['dock_now'] = bp.backdrop('dock_idle_view.png')
bp.BG['battle_now2'] = bp.backdrop('b_root_view.png')
bp.BG['target_now'] = bp.backdrop('b_target2_view.png')
bp.BG['field_now'] = bp.backdrop('fem_field_view.png')

PIXEL_ICON = bp.icon
STYLE = {'name': 'duo'}


def vicon(name, size=30, cls=''):
    """Drop-in for build_proposal.icon(): the current style's vector icon (pixel = the old PNGs)."""
    if STYLE['name'] == 'pixel':
        return PIXEL_ICON(name, size, cls)
    return H.svg(name, round(size * 0.87), STYLE['name'])


bp.icon = vicon

STYLES = [
    ('line', 'A', '라인', '1.7px 둥근 선 한 가지. 가볍고 현대적이라 유리 판과 잘 맞고, 작은 크기에서도 또렷합니다. 대신 밋밋해서 판타지 느낌은 약합니다.'),
    ('duo', 'B', '듀오톤', '같은 선에 몸통을 옅게 채우고 <b>한 곳만 금색</b>(일지의 느낌표, 가방 걸쇠, 새싹 잎 하나). 무게가 생겨 한눈에 들어오고, 유리 판·금 테두리와 같은 재료로 읽힙니다.'),
    ('medal', 'C', '메달', '작은 남색 동전에 금테, 그 위에 금빛 선. 판타지 RPG 느낌이 가장 강하지만 버튼이 동그랗게 고정되어 독이 무거워지고, 전투 명령처럼 글자와 같이 쓰면 시끄럽습니다.'),
    ('pixel', 'D', '지금 (2px 픽셀)', '지금 게임에 들어간 그림. 세계의 도트와 맞추려 했지만 유리 판·마루부리 글자 옆에서 거칠고 무겁게 보이고, 34px 그림을 줄여 그려 선 굵기가 들쭉날쭉합니다.'),
]

COMMANDS = [('sword', '공격', True), ('spark', '스킬', False), ('bag', '아이템', True), ('swap', '나이아 교체', False), ('flee', '도주', True)]


def mini_dock(style, hover='bag'):
    STYLE['name'] = style
    return f'<div class="mini bg-village">{bp.a_dock(hover)}</div>'


def command_panel(style, left=None, top=None, sel=0, extra=''):
    STYLE['name'] = style
    rows = []
    for i, (ic, label, ok) in enumerate(COMMANDS):
        cls = 'on' if i == sel else ('off' if not ok else '')
        rows.append(f'<div class="r-row {cls}">{vicon(ic, 26)}<span>{label}</span></div>')
    pos = f'left:{left}px;top:{top}px' if left is not None else 'position:relative'
    return f'<div class="a-glass r-cmd" style="{pos}">{"".join(rows)}</div>{extra}'


def ladder(style):
    STYLE['name'] = style
    cells = ''.join(f'<div class="r-lad">{vicon("quest", s)}{vicon("bag", s)}{vicon("naia", s)}<small>{round(s * .87) if style != "pixel" else s}px</small></div>'
                    for s in (22, 30, 38))
    return f'<div class="r-ladder">{cells}</div>'


def style_card(style, letter, name, text):
    rec = ' rec' if style == 'duo' else ''
    badge = '<span class="r-rec">추천</span>' if style == 'duo' else ''
    return f'''<article class="r-card{rec}">
  <header><span class="tag">{letter}</span><h3>{name}</h3>{badge}</header>
  {mini_dock(style)}
  <div class="r-pair"><div class="r-cmdbg bg-battle">{command_panel(style)}</div>{ladder(style)}</div>
  <p>{text}</p>
</article>'''


# ---------------------------------------------------------------- battle (matches what is in the game now)

def tag_ally(x, y, hp=100):
    return f'''<div class="r-tag" style="left:{x}px;top:{y}px"><div class="r-tagtop"><em>HP&nbsp; {hp} / 100</em></div>
<div class="r-bar hp"><i style="width:{hp}%"></i></div><div class="r-bar sp thin"><i style="width:30%"></i></div></div>'''


def tag_foe(x, y, mark=False, dim=False):
    cls = (' r-marked' if mark else '') + (' dim' if dim else '')
    return f'''<div class="r-tag foe{cls}" style="left:{x}px;top:{y}px"><div class="r-tagtop"><b>푸루나</b><span>Lv.10</span></div>
<div class="r-bar foe"><i style="width:100%"></i></div></div>'''


def battle_layers(style, target=False):
    STYLE['name'] = style
    turn = '''<div class="a-glass r-turn"><span>TURN</span><b>01</b><i></i><em>행동 선택</em></div>'''
    tags = tag_ally(478, 702) + tag_foe(1382, 455, mark=target) + tag_foe(1542, 470, dim=target)
    tether = '<i class="r-dot" style="left:574px;top:846px"></i><i class="r-line" style="left:582px;top:846px;width:28px"></i>'
    if target:
        STYLE['name'] = style
        panel = f'''<div class="a-glass r-cmd r-detail" style="left:610px;top:800px">
  <div class="r-cap">대상 고르기</div><div class="r-name"><span>‹</span><b>푸루나</b><span>›</span></div>
  <div class="r-sp"><span class="g">SP</span><i class="on"></i><i class="on"></i><i></i><em>2타 · SP 18</em></div></div>'''
        cursor = '<div class="r-cursor" style="left:1440px;top:418px"></div><div class="r-ring" style="left:1440px;top:592px"></div>'
        return turn + tags + tether + panel + cursor
    return turn + tags + tether + command_panel(style, 610, 738)


# ---------------------------------------------------------------- page

def figure(fid, bg, layers, caption):
    return f'''<figure class="cap">
<div class="bar-ctl">{bp.views(fid, False)}</div>
{bp.frame(fid, bg, layers)}
<figcaption>{caption}</figcaption></figure>'''


def shot(bg, caption):
    return f'''<figure class="cap"><div class="stage"><div class="canvas"><div class="bg bg-{bg}"></div></div></div>
<figcaption>{caption}</figcaption></figure>'''


def build():
    cards = ''.join(style_card(*s) for s in STYLES)
    STYLE['name'] = 'duo'
    village = bp.a_village(hover='bag')
    return f'''
<header class="top">
  <div class="eyebrow">ProjectM · 인게임 HUD · 2026-10-14 · 다듬기 시안 v2</div>
  <h1>HUD 다듬기 시안: 아이콘 스타일 4안과 하단 메뉴·전투</h1>
  <p class="lede">A 글래스 엣지 HUD를 게임에 넣어 보니 <b>2px 픽셀 아이콘</b>이 유리 판·글자와 따로 놀았습니다. 배치는 그대로 두고
  아이콘을 같은 모양 세 가지 스타일로 다시 그려 비교했고, 하단 메뉴와 전투 화면을 실제 게임 기준으로 다시 맞췄습니다. 추천은 <b>B 듀오톤</b>입니다.</p>
  <div class="bar-ctl"><button type="button" class="toggle" id="safe" aria-pressed="false">안전 여백·가운데 영역 보기</button></div>
</header>

<section class="block">
  <div class="head"><span class="tag">01</span><h2>지금 게임 화면 (1차 적용)</h2></div>
  <div class="grid2">
    {shot('dock_now', '<b>필드</b> · 리베르그로브 남문, 지금 들어간 HUD')}
    {shot('target_now', '<b>전투</b> · 대상 고르기 (금색 커서·발밑 고리·나머지 흐림)')}
  </div>
  <ol class="issues">
    <li><span class="n">1</span><div><b>아이콘이 거칠다.</b> 2px 도트 아이콘이 매끈한 유리 판·마루부리 글자 옆에서 투박하고 무겁게 보입니다. 34px 그림을 30px로 줄여 그려 선 굵기도 고르지 않습니다.</div></li>
    <li><span class="n">2</span><div><b>하단 메뉴가 시안과 다르다.</b> 버튼마다 테두리가 늘 보여 7개가 따로 놀고, 눈(HUD 숨김) 버튼이 바깥에 따로 떠 있었습니다 → 눈은 판 안으로 넣고 단축키 칸을 붙였습니다(바로 아래 표의 D가 지금 모습). 늘 보이는 테두리는 다음 적용 때 같이 없앱니다.</div></li>
    <li><span class="n">3</span><div><b>전투 명령 판에도 같은 픽셀 아이콘</b>이 들어가 있습니다. 아이콘을 바꾸면 필드·전투가 함께 바뀝니다.</div></li>
  </ol>
</section>

<section class="block">
  <div class="head"><span class="tag">02</span><h2>아이콘 스타일 4안</h2></div>
  <p class="note">같은 17개 아이콘을 한 벌의 모양(24 격자)으로 그리고, 스타일만 바꿨습니다. 각 카드: 실제 게임 배경 위 하단 메뉴(소지품에 마우스를 올린 상태) ·
  전투 명령 판 · 크기 3단계. 벡터라 어느 크기에서도 선이 고르고, 게임에는 2배 해상도 그림으로 넣습니다.</p>
  <div class="r-cards">{cards}</div>
</section>

<section class="block">
  <div class="head"><span class="tag">03</span><h2>추천 B 듀오톤으로 맞춘 화면</h2></div>
  <div class="grid2">
    {figure('b-village', 'village', village, '<b>필드</b> · 하단 메뉴는 한 판, 눈 버튼도 판 안. 테두리는 마우스를 올렸을 때만, 단축키 칸은 지금 묶인 키를 읽어 표시')}
    {figure('b-battle', 'battle', battle_layers('duo'), '<b>전투</b> · 명령 판은 행동하는 캐릭터 옆(지금 게임과 같은 배치), 아이콘만 듀오톤')}
  </div>
  <div class="grid2">
    {figure('b-target', 'battle', battle_layers('duo', True), '<b>대상 고르기</b> · 대상 위 커서, 발밑 고리, 다른 적 표시는 흐리게 (지금 게임과 같음)')}
    <div class="r-notes">
      <h3>바꾸는 것 / 그대로 두는 것</h3>
      <ul class="tight">
        <li><b>바꾸는 것</b>: 아이콘 그림 17개(필드 독·전투 명령·채팅·지도 표시), 하단 메뉴의 늘 보이는 테두리.</li>
        <li><b>그대로</b>: 배치·크기·색·유리 판·글꼴·연출. 이미 들어간 대상 연출·컴팩트 머리 위 표시·캐릭터 옆 명령 판도 그대로.</li>
        <li>아이콘은 벡터 원본(<code>Tools/HUDRenewal/hud_icons.py</code>)에서 64px PNG로 뽑아 넣습니다 → 나중에 모양을 고쳐도 한 번에 다시 뽑힙니다.</li>
        <li>다른 안을 고르면 같은 원본에서 스타일만 바꿔 뽑습니다(A·C도 바로 가능).</li>
      </ul>
    </div>
  </div>
</section>
'''


CSS = r'''
.r-cards { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 22px; }
.r-card { border: 1px solid var(--p-line); border-radius: 12px; padding: 16px; display: grid; gap: 12px; background: var(--p-raise); }
.r-card.rec { border-color: var(--p-accent); box-shadow: 0 0 0 1px var(--p-accent) inset; }
.r-card header { display: flex; align-items: center; gap: 10px; }
.r-card h3 { margin: 0; font-size: 18px; color: var(--p-strong); }
.r-card p { margin: 0; font-size: 14.5px; }
.r-rec { margin-left: auto; font-size: 12.5px; font-weight: 700; color: var(--p-fill-ink); background: var(--p-fill); padding: 2px 10px; border-radius: 999px; }
.mini { color: #f1ede6; position: relative; height: 170px; border-radius: 8px; overflow: hidden; background-size: 1920px 1080px; background-position: right bottom; }
.mini .a-dock { right: 18px !important; bottom: 18px !important; }
.r-pair { display: flex; gap: 14px; align-items: stretch; }
.r-cmdbg { flex: 0 0 auto; padding: 14px; border-radius: 10px; background-size: 1920px 1080px; background-position: -420px -700px; color: #f1ede6; }
.r-ladder { flex: 1; display: grid; grid-template-rows: repeat(3, 1fr); gap: 6px; background: #0e1a28; border-radius: 10px; padding: 8px 12px; }
.r-lad { display: flex; align-items: center; gap: 12px; color: #aab6bf; font-size: 12px; }
.r-lad small { margin-left: auto; }
.r-cmd { width: 212px; padding: 8px; border-radius: 16px; display: grid; gap: 3px; }
.canvas .r-cmd { position: absolute; }
.r-row { height: 40px; display: flex; align-items: center; gap: 12px; padding: 0 12px; border-radius: 10px; color: #f1ede6; font-size: 16.5px; font-weight: 700; position: relative; }
.r-row.on { background: rgba(214,190,140,.20); box-shadow: inset 0 0 0 1px rgba(240,215,157,.35); }
.r-row.on::before { content: ""; position: absolute; left: 0; top: 10px; bottom: 10px; width: 3px; border-radius: 2px; background: #f0d79d; }
.r-row.off { opacity: .38; }
.r-row .ic, .r-row .px { flex: 0 0 auto; }
.medal { display: inline-grid; place-items: center; border-radius: 50%; background: radial-gradient(circle at 40% 35%, #22384f, #101c2a);
  box-shadow: inset 0 0 0 1.5px #c9a85f, 0 2px 6px rgba(0,0,0,.5); }
.medal svg { width: 70% !important; height: 70% !important; }
.r-turn { left: 50%; top: 30px; transform: translateX(-50%); height: 62px; padding: 0 26px; border-radius: 31px; display: flex; align-items: center; gap: 12px; color: #f1ede6; }
.r-turn span { font-size: 13px; letter-spacing: 3px; color: #d6be8c; } .r-turn b { font-size: 30px; font-weight: 400; }
.r-turn i { width: 1px; height: 26px; background: rgba(241,237,230,.2); } .r-turn em { font-style: normal; font-weight: 700; font-size: 18px; }
.r-tag { position: absolute; width: 124px; padding: 4px 7px 5px; border-radius: 7px; background: rgba(11,21,32,.62); box-shadow: inset 0 0 0 1px rgba(241,237,230,.10); transform: translateX(-50%); }
.r-tag.foe { width: 116px; } .r-tag.dim { opacity: .4; }
.r-tag.r-marked { box-shadow: inset 0 0 0 1.5px #f0d79d, 0 0 0 3px rgba(240,215,157,.10); transform: translateX(-50%) scale(1.08); }
.r-tagtop { display: flex; align-items: baseline; gap: 5px; color: #f1ede6; font-size: 12.5px; font-weight: 700; }
.r-tagtop em { margin-left: auto; font-style: normal; font-size: 11px; color: #e7e2d9; } .r-tagtop span { font-size: 10px; color: #f1c8bd; }
.r-bar { height: 5px; border-radius: 3px; background: rgba(255,255,255,.1); margin-top: 3px; overflow: hidden; } .r-bar.thin { height: 3px; margin-top: 2px; }
.r-bar i { display: block; height: 100%; border-radius: 3px; }
.r-bar.hp i { background: linear-gradient(90deg, #6fbf63, #a9e08a); } .r-bar.sp i { background: linear-gradient(90deg, #5d9fdb, #9fd0f5); }
.r-bar.foe i { background: linear-gradient(90deg, #d9604d, #f08a6e); }
.r-dot { position: absolute; width: 11px; height: 11px; margin: -5.5px; background: #e8c46e; transform: rotate(45deg); box-shadow: 0 0 6px rgba(232,196,110,.6); }
.r-line { position: absolute; height: 1px; background: rgba(214,190,140,.55); }
.r-detail { width: 300px; padding: 12px 14px; color: #f1ede6; }
.r-cap { font-size: 12px; font-weight: 700; color: #d6be8c; letter-spacing: 1px; }
.r-name { display: flex; align-items: center; gap: 8px; font-size: 19px; margin-top: 6px; } .r-name b { flex: 1; } .r-name span { color: #aab6bf; }
.r-sp { display: flex; align-items: center; gap: 6px; margin-top: 10px; font-size: 12px; } .r-sp .g { color: #d6be8c; font-weight: 700; margin-right: 4px; }
.r-sp i { width: 10px; height: 10px; transform: rotate(45deg); border: 1.5px solid #f1ede6; opacity: .6; } .r-sp i.on { background: #e8c46e; border-color: #e8c46e; opacity: 1; }
.r-sp em { margin-left: auto; font-style: normal; color: #a6dcf2; font-weight: 700; }
.r-cursor { position: absolute; width: 40px; height: 32px; margin-left: -20px; background: #0b1520; clip-path: polygon(0 0, 100% 0, 50% 100%); filter: drop-shadow(0 0 4px rgba(240,215,157,.7)); }
.r-cursor::after { content: ""; position: absolute; inset: 3px 5px 6px; background: linear-gradient(180deg, #ffe7a0 0 34%, #ecb848 34%); clip-path: polygon(0 0, 100% 0, 50% 100%); }
.r-ring { position: absolute; width: 132px; height: 42px; margin: -21px 0 0 -66px; border-radius: 50%; border: 3.5px solid #face68; background: rgba(250,206,104,.18);
  box-shadow: 0 0 10px rgba(250,206,104,.45); }
.r-notes { border: 1px solid var(--p-line); border-radius: 12px; padding: 18px 20px; background: var(--p-raise); }
.r-notes h3 { margin: 0 0 8px; font-size: 17px; color: var(--p-strong); }
@media (max-width: 860px) { .r-cards { grid-template-columns: 1fr; } .r-pair { flex-direction: column; } }
'''


def main():
    body = build()
    text = html.unescape(re.sub(r'<[^>]+>', ' ', re.sub(r'url\([^)]*\)', '', body))) + bp.CSS + CSS + '0123456789px'
    page = f'''<!doctype html>
<html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>HUD 다듬기 시안 v2</title>
<style>
{bp.font_faces(text)}
{''.join('.bg-%s{background-image:url(%s)}' % (k, v) for k, v in bp.BG.items())}
{bp.CSS}
{CSS}
</style></head>
<body><main class="wrap">
{body}
</main>
<script>{bp.JS}</script>
</body></html>'''
    OUT.mkdir(parents=True, exist_ok=True)
    out = OUT / 'hud_refine.html'
    out.write_text(page, encoding='utf-8')
    print('written', out, '%.1f MB' % (out.stat().st_size / 1e6))


if __name__ == '__main__':
    main()
