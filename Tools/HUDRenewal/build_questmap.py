# -*- coding: utf-8 -*-
"""Quest tracker / quest banners / minimap markers proposal (after the first in-game pass of the new HUD).

Output: Docs/HUD_QuestMap_Proposal_20261014/quest_map.html (one file, everything embedded) + png/ captures.
Uses the real kit pictures (make_hud_kit.py), the baked village map and the user's in-game screenshot.
Run: python Tools/HUDRenewal/build_questmap.py [screenshot.png]
"""
import base64
import io
import pathlib
import sys

from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
KIT = ROOT / 'ArtSource' / 'UI' / 'HUD2026' / 'kit'
OUT = ROOT / 'Docs' / 'HUD_QuestMap_Proposal_20261014'
FONTS = ROOT / 'Saved' / 'HUDRenewal' / 'fonts'
MAP = ROOT / 'Saved' / 'HUDRenewal' / 'maps' / 'T_Map_L_Libregrove.png'
sys.path.insert(0, str(HERE))
import hud_icons as H  # noqa: E402


def uri(im, fmt='PNG'):
    buf = io.BytesIO()
    im.save(buf, fmt, quality=88) if fmt == 'JPEG' else im.save(buf, fmt)
    return 'data:image/%s;base64,%s' % (fmt.lower(), base64.b64encode(buf.getvalue()).decode())


def kit(name):
    return uri(Image.open(KIT / (name + '.png')))


def font(name):
    return 'data:font/ttf;base64,' + base64.b64encode((FONTS / name).read_bytes()).decode()


def build(shot_path):
    OUT.mkdir(parents=True, exist_ok=True)
    shot = Image.open(shot_path).convert('RGB')
    (OUT / 'png').mkdir(exist_ok=True)
    shot.save(OUT / 'png' / 'now_village.png')
    W, Hh = shot.size
    scale = 1080 / Hh                                   # the screenshot is 810 high: back to 1080p units
    full = shot.resize((round(W * scale), 1080), Image.LANCZOS)
    corner_now = full.crop((full.width - 520, 0, full.width, 520))
    field_bg = full.crop((full.width // 2 - 660, 120, full.width // 2 + 660, 720))
    tracker_bg = full.crop((0, 360, 560, 660))           # bright roofs, no old tracker under it

    # minimap picture at the in-game zoom (map texture 1024, zoom 3 => 341 map px across 236 screen px)
    m = Image.open(MAP).convert('RGB')
    cx, cy = 548, 540
    half = 1024 / 3 / 2
    mini = m.crop((round(cx - half), round(cy - half), round(cx + half), round(cy + half))).resize((236, 236), Image.LANCZOS)

    css = CSS.replace('%BOLD%', font('MaruBuri-Bold.ttf')).replace('%SEMI%', font('MaruBuri-SemiBold.ttf'))
    css = css.replace('%FIELD%', uri(field_bg, 'JPEG')).replace('%TRACKBG%', uri(tracker_bg, 'JPEG'))
    css = css.replace('%MINI%', uri(mini, 'JPEG')).replace('%RAYS%', kit('T_HUD_Rays')).replace('%SPARK%', kit('T_HUD_Sparkle'))
    css = css.replace('%GLOW%', kit('T_HUD_BannerGlow')).replace('%RULEL%', kit('T_HUD_RuleL')).replace('%RULER%', kit('T_HUD_RuleR'))
    css = css.replace('%RING%', kit('T_HUD_Ring'))

    page = PAGE
    page = page.replace('%CSS%', css)
    page = page.replace('%CORNER_NOW%', uri(corner_now, 'JPEG'))
    page = page.replace('%TRACKER_NEW%', tracker_card())
    page = page.replace('%FX_ACCEPT%', fx_card('accept', '새 의뢰', '뿌리 사이로', '리베르그로브 아랫마을에 도착하기'))
    page = page.replace('%FX_READY%', fx_card('ready', '목표 달성', '뿌리 사이로', '촌장 에델에게 보고하기'))
    page = page.replace('%FX_DONE%', fx_card('done', '의뢰 완료', '뿌리 사이로', '보상을 받았습니다'))
    page = page.replace('%MAP_A%', minimap('a')).replace('%MAP_B%', minimap('b')).replace('%MAP_C%', minimap('c'))
    page = page.replace('%LEGEND%', legend())
    page = page.replace('%EDGE%', minimap('a', edge=True))
    page = page.replace('%INGAME%', ingame())
    out = OUT / 'quest_map.html'
    out.write_text(page, encoding='utf-8')
    print('wrote', out)
    return out


CAP = ROOT / 'Saved' / 'CharacterCreateGlass'
# in-game captures after the change (pie_capture, 1920x1080): (file, crop, caption)
INGAME = [
    ('qm_idle_view.png', (1560, 0, 1920, 480), '추적 창 유리 판 + 미니맵 (서 있을 때 화살표 = 캐릭터가 보는 쪽)'),
    ('qm_move_view.png', (1600, 0, 1920, 300), '이동 중: 화살표·부채꼴이 이동 방향(오른쪽 위)으로'),
    ('q3_ready_after_view.png', (1560, 300, 1920, 480), '보고 가능 줄: 금 제목 + "보고" 칩 + 금빛 바탕'),
    ('q3_edge_view.png', (1600, 0, 1920, 300), '미니맵 밖 퀘스트: 테두리에 붙고 꼭지가 방향을 가리킴'),
    ('qm_accept_view.png', (520, 160, 1400, 400), '수락 — 새 의뢰 (실제 화면)'),
    ('q2_ready_view.png', (520, 160, 1400, 400), '목표 달성 — 퀘스트 10001 진행 완료 때 실제로 뜬 띠'),
    ('q2_ready_after_view.png', (520, 160, 1400, 400), '의뢰 완료 — 이어서 자동으로 뜬 완료 띠 (빛살·반짝이)'),
]


def ingame():
    out = ''
    for name, box, caption in INGAME:
        path = CAP / name
        if not path.exists():
            continue
        im = Image.open(path).convert('RGB').crop(box)
        im.save(OUT / 'png' / name)
        out += f'<figure class="card"><img class="shot" src="{uri(im, "JPEG")}" alt=""><p>{caption}</p></figure>'
    return out or '<p class="lead">아직 캡처 없음</p>'


def tracker_card(ready=False):
    quest = H.svg('quest', 18, 'duo')
    return f'''
    <div class="trk">
      <div class="trk-head"><span class="trk-ic">{quest}</span><b>퀘스트</b><span class="trk-n">2</span>
        <span class="trk-btn">▲</span><span class="trk-btn">×</span></div>
      <div class="trk-rule"></div>
      <div class="trk-entry"><div class="trk-t"><i class="dia"></i>뿌리 사이로</div>
        <div class="trk-o">리베르그로브 아랫마을에 도착하기</div></div>
      <div class="trk-entry ready"><div class="trk-t"><i class="dia"></i>잃어버린 종<span class="chip">보고</span></div>
        <div class="trk-o">대장장이 브론에게 보고하기</div></div>
    </div>'''


def fx_card(kind, cap, title, sub):
    burst = ''
    if kind == 'done':
        burst = '<i class="rays"></i><i class="ringb"></i>'
    sparks = ''.join(f'<i class="sp s{i}"></i>' for i in range(8 if kind == 'done' else 4))
    return f'''
    <div class="fx fx-{kind}" data-kind="{kind}">
      <div class="fx-stage">
        <div class="fx-root">
          <i class="fx-glow"></i>{burst}
          <div class="fx-stack">
            <div class="fx-cap"><i class="dia"></i>{cap}<i class="dia"></i></div>
            <div class="fx-title"><i class="rl"></i><span>{title}</span><i class="rr"></i></div>
            <div class="fx-sub">{sub}</div>
          </div>
          <div class="fx-sparks">{sparks}</div>
        </div>
      </div>
      <button class="replay" onclick="replay(this)">▶ 다시 보기</button>
    </div>'''


MARKERS = [  # (kit name, x, y) on the 236 px minimap
    ('T_HUD_Badge_Offer', 52, 70), ('T_HUD_Badge_TurnIn', 176, 98), ('T_HUD_Badge_InProgress', 64, 176),
    ('T_HUD_Badge_Target', 186, 190), ('T_HUD_Badge_Portal', 206, 38),
]


def minimap(option, edge=False):
    pl = ''
    if option == 'a':
        pl = (f'<span class="pl pl-a"><img class="cone" src="{kit("T_HUD_MapPlayerCone")}"><img class="halo" src="{kit("T_HUD_MapPlayerHalo")}">'
              f'<i class="pulse"></i><img class="arrow" src="{kit("T_HUD_MapPlayer")}"></span>')
    elif option == 'b':
        pl = '<span class="pl pl-b"><i class="dot"></i><i class="notch"></i><i class="pulse"></i></span>'
    else:
        pl = f'<span class="pl pl-c"><img src="{kit("T_HUD_PlayerArrow")}"></span>'
    marks = ''
    if option == 'c':
        old = [('T_HUD_Marker_Request', 52, 70), ('T_HUD_Marker_TurnIn', 176, 98), ('T_HUD_Marker_InProgress', 64, 176),
               ('T_HUD_Marker_Request', 186, 190), ('T_HUD_Marker_Portal', 206, 38)]
        marks = ''.join(f'<img class="mk old" style="left:{x}px;top:{y}px" src="{kit(n)}">' for n, x, y in old)
    else:
        for n, x, y in MARKERS:
            pulse = '<i class="mk-pulse"></i>' if n in ('T_HUD_Badge_TurnIn', 'T_HUD_Badge_Target') else ''
            marks += f'<span class="mk" style="left:{x}px;top:{y}px">{pulse}<img src="{kit(n)}"></span>'
    if edge:
        marks += (f'<span class="mk edge" style="left:220px;top:150px"><i class="mk-pulse"></i><img src="{kit("T_HUD_Badge_TurnIn")}">'
                  f'<img class="ptr" style="transform:translate(14px,-6px) rotate(70deg)" src="{kit("T_HUD_EdgePointer")}"></span>'
                  f'<span class="mk edge" style="left:30px;top:222px"><img src="{kit("T_HUD_Badge_Target")}">'
                  f'<img class="ptr" style="transform:translate(-12px,10px) rotate(-135deg)" src="{kit("T_HUD_EdgePointer")}"></span>')
    return f'''
    <div class="mm"><div class="mm-clip"><div class="mm-map"></div>{marks}{pl}<i class="mm-vig"></i></div><b class="mm-n">N</b></div>'''


def legend():
    rows = [('T_HUD_Badge_Offer', '받을 수 있는 퀘스트', '금빛 마름모 + 느낌표. 지금 원판보다 큰 26px, 짙은 테두리와 그늘로 지붕·길 위에서도 떠 보입니다.'),
            ('T_HUD_Badge_TurnIn', '보고 가능', '밝은 금 + 물음표, 바깥으로 퍼지는 고리가 계속 돕니다. 미니맵 밖이면 가장자리에 붙고 방향 꼭지가 생깁니다.'),
            ('T_HUD_Badge_Target', '진행 중 목표 지점', '청록 빈 마름모 + 점. 금색(NPC)과 구별되는 "가야 할 곳" 색. 고리 + 가장자리 붙임.'),
            ('T_HUD_Badge_InProgress', '진행 중인 퀘스트의 NPC', '회청색, 점 세 개. 지금 할 일이 아님을 낮은 대비로.'),
            ('T_HUD_Badge_Locked', '조건 미달', '남색 + 회색 느낌표. 가장 흐리게.'),
            ('T_HUD_Badge_Portal', '지역 이동 포탈', '청록 문(아치) 그림. 지금의 빈 고리는 무엇인지 안 읽혀서.')]
    out = ''
    for n, t, d in rows:
        out += f'<div class="lg"><img src="{kit(n)}"><div><b>{t}</b><p>{d}</p></div></div>'
    return out


CSS = r'''
@font-face{font-family:Maru;src:url(%BOLD%);font-weight:700}
@font-face{font-family:Maru;src:url(%SEMI%);font-weight:500}
:root{--navy:#16293d;--glass:rgba(14,26,40,.80);--gold:#d6be8c;--goldhi:#f4d78f;--ivory:#f1ede6;--muted:#aab6bf;--line:rgba(241,237,230,.16);--teal:#7fe0d6;
 --bg:#0b1219;--panel:#111c27;--text:#e8e4dc;--sub:#9aa7b1}
@media (prefers-color-scheme: light){:root:not([data-theme="dark"]){--bg:#eef0f2;--panel:#ffffff;--text:#1d2731;--sub:#5d6b77}}
:root[data-theme="light"]{--bg:#eef0f2;--panel:#ffffff;--text:#1d2731;--sub:#5d6b77}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.65 "Pretendard","Apple SD Gothic Neo","Malgun Gothic",sans-serif}
main{max-width:1180px;margin:0 auto;padding:40px 24px 80px}
h1{font:700 30px/1.3 Maru,serif;margin:0 0 6px}
h2{font:700 21px/1.35 Maru,serif;margin:56px 0 6px}
.lead{color:var(--sub);margin:0 0 18px;max-width:820px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:18px}.grid.one{grid-template-columns:1fr}
.card{background:var(--panel);border-radius:14px;padding:16px;border:1px solid rgba(128,140,150,.18)}
.card h3{margin:0 0 4px;font-size:16px}.card p{margin:6px 0 0;color:var(--sub);font-size:14px}
.tag{display:inline-block;font-size:12px;font-weight:700;padding:1px 8px;border-radius:99px;background:#2f4a3a;color:#bfe8c8;margin-left:6px;vertical-align:2px}
.tag.no{background:#4a2f2f;color:#f0c0b8}
.shot{width:100%;border-radius:10px;display:block}
ol.issues{margin:10px 0 0;padding-left:20px}ol.issues li{margin:4px 0}
.cmp{display:grid;grid-template-columns:1fr 1fr;gap:18px}
@media (max-width:760px){.cmp{grid-template-columns:1fr}}
.bgtrack{position:relative;height:300px;border-radius:12px;overflow:hidden;background:url(%TRACKBG%) center/cover}
.bgtrack .trk{position:absolute;right:24px;top:22px}
/* tracker card */
.trk{width:300px;background:var(--glass);border-radius:18px;padding:12px 16px 14px;box-shadow:0 8px 28px rgba(0,0,0,.45),inset 0 0 0 1px rgba(214,190,140,.28);backdrop-filter:blur(10px);font-family:Maru;color:var(--ivory)}
.trk-head{display:flex;align-items:center;gap:8px;font-size:15px}.trk-head b{font-weight:700;letter-spacing:.06em}
.trk-ic svg{display:block}.trk-n{font-size:12px;color:var(--navy);background:var(--gold);border-radius:99px;padding:0 7px;line-height:18px;margin-right:auto}
.trk-btn{width:24px;height:24px;border-radius:7px;display:grid;place-items:center;font-size:12px;color:rgba(241,237,230,.85);background:rgba(241,237,230,.08)}
.trk-rule{height:1px;background:linear-gradient(90deg,rgba(214,190,140,.55),rgba(214,190,140,0));margin:10px 0 10px}
.trk-entry{padding:6px 8px 7px;margin:0 -8px;border-radius:10px}.trk-entry+.trk-entry{margin-top:4px}
.trk-t{display:flex;align-items:center;gap:9px;font-size:17px;font-weight:700}
.trk-o{padding-left:19px;font-size:14.5px;color:#dce1e5;margin-top:2px}
.dia{display:inline-block;width:9px;height:9px;background:var(--gold);transform:rotate(45deg);flex:none}
.trk-entry.ready{background:linear-gradient(90deg,rgba(244,215,143,.16),rgba(244,215,143,0))}
.trk-entry.ready .trk-t{color:var(--goldhi)}.trk-entry.ready .dia{background:var(--goldhi);box-shadow:0 0 6px var(--goldhi)}
.chip{margin-left:auto;font-size:12px;color:var(--navy);background:var(--goldhi);border-radius:99px;padding:0 9px;line-height:20px}
/* banners */
.fx{position:relative}
.fx-stage{position:relative;height:330px;border-radius:12px;overflow:hidden;background:url(%FIELD%) center/cover}
.fx-root{position:absolute;left:50%;top:44%;transform:translate(-50%,-50%);width:760px;height:260px;display:grid;place-items:center;pointer-events:none;font-family:Maru}
.fx-glow{position:absolute;inset:20px -40px;background:url(%GLOW%) center/100% 100% no-repeat;opacity:0}
.fx-stack{position:relative;text-align:center;color:var(--ivory)}
.fx-cap{display:inline-flex;align-items:center;gap:10px;font-size:15px;font-weight:700;letter-spacing:.32em;color:var(--goldhi);text-shadow:0 2px 6px rgba(0,0,0,.7);opacity:0}
.fx-cap .dia{width:7px;height:7px}
.fx-title{display:flex;align-items:center;justify-content:center;gap:20px;margin-top:6px;opacity:0}
.fx-title span{font-size:38px;font-weight:700;text-shadow:0 3px 10px rgba(0,0,0,.75);letter-spacing:.04em}
.fx-title .rl,.fx-title .rr{width:120px;height:2px;background:url(%RULEL%) center/100% 100%}.fx-title .rr{background-image:url(%RULER%)}
.fx-sub{font-size:16px;color:#dfe3e6;margin-top:6px;text-shadow:0 2px 6px rgba(0,0,0,.8);opacity:0}
.fx-sparks .sp{position:absolute;left:50%;top:50%;width:26px;height:26px;margin:-13px;background:url(%SPARK%) center/contain no-repeat;opacity:0}
.rays{position:absolute;left:50%;top:50%;width:520px;height:520px;margin:-260px;background:url(%RAYS%) center/contain no-repeat;opacity:0}
.ringb{position:absolute;left:50%;top:50%;width:300px;height:300px;margin:-150px;background:url(%RING%) center/contain no-repeat;opacity:0}
.fx-done .fx-title span{font-size:44px;background:linear-gradient(#fff6dc,#f2cf7c);-webkit-background-clip:text;background-clip:text;color:transparent;text-shadow:none;filter:drop-shadow(0 3px 8px rgba(0,0,0,.7))}
.fx-ready .fx-cap{color:var(--teal)}
.play .fx-glow{animation:glow 2.8s both}
.play .fx-cap{animation:capIn 2.8s both}
.play .fx-title{animation:titleIn 2.8s both}
.play .fx-sub{animation:subIn 2.8s both}
.fx-done.play .fx-title{animation:stamp 3.1s both}
.fx-done.play .fx-cap,.fx-done.play .fx-sub,.fx-done.play .fx-glow{animation-duration:3.1s}
.play .rays{animation:rays 3.1s both}
.play .ringb{animation:ringb 1.1s .1s both}
.play .rl{animation:ruleL 2.8s both}.play .rr{animation:ruleR 2.8s both}
@keyframes glow{0%{opacity:0;transform:scale(.7)}14%{opacity:1;transform:none}82%{opacity:1}100%{opacity:0}}
@keyframes capIn{0%,2%{opacity:0;transform:translateY(-10px)}18%{opacity:1;transform:none}82%{opacity:1;transform:none}100%{opacity:0;transform:translateY(-12px)}}
@keyframes titleIn{0%,5%{opacity:0;transform:translateY(14px) scale(.92)}22%{opacity:1;transform:none}82%{opacity:1;transform:none}100%{opacity:0;transform:translateY(-12px)}}
@keyframes subIn{0%,10%{opacity:0;transform:translateY(8px)}28%{opacity:1;transform:none}82%{opacity:1;transform:none}100%{opacity:0;transform:translateY(-12px)}}
@keyframes stamp{0%,3%{opacity:0;transform:scale(1.35)}15%{opacity:1;transform:scale(.98)}20%{transform:none}84%{opacity:1;transform:none}100%{opacity:0;transform:translateY(-14px)}}
@keyframes rays{0%{opacity:0;transform:scale(.5) rotate(0)}12%{opacity:.95}55%{opacity:.5}100%{opacity:0;transform:scale(1.05) rotate(25deg)}}
@keyframes ringb{0%{opacity:1;transform:scale(.25)}100%{opacity:0;transform:scale(1.25)}}
@keyframes ruleL{0%,8%{opacity:0;transform:translateX(60px) scaleX(.2)}28%{opacity:1;transform:none}82%{opacity:1}100%{opacity:0}}
@keyframes ruleR{0%,8%{opacity:0;transform:translateX(-60px) scaleX(.2)}28%{opacity:1;transform:none}82%{opacity:1}100%{opacity:0}}
.play .sp{animation:spark 1.2s both}
@keyframes spark{0%{opacity:0;transform:translate(0,0) scale(.3)}25%{opacity:1}100%{opacity:0;transform:translate(var(--x),var(--y)) scale(1) rotate(45deg)}}
.sp.s0{--x:-250px;--y:-30px;animation-delay:.15s!important}.sp.s1{--x:240px;--y:-40px;animation-delay:.2s!important}
.sp.s2{--x:-180px;--y:60px;animation-delay:.25s!important}.sp.s3{--x:200px;--y:55px;animation-delay:.18s!important}
.sp.s4{--x:-90px;--y:-95px;animation-delay:.12s!important}.sp.s5{--x:110px;--y:-90px;animation-delay:.14s!important}
.sp.s6{--x:-300px;--y:15px;animation-delay:.3s!important}.sp.s7{--x:290px;--y:20px;animation-delay:.28s!important}
.replay{position:absolute;right:12px;bottom:12px;font:600 13px/1 inherit;padding:8px 12px;border-radius:99px;border:0;background:rgba(14,26,40,.8);color:var(--ivory);cursor:pointer}
table.time{border-collapse:collapse;width:100%;font-size:14px;margin-top:10px}
table.time td,table.time th{padding:6px 8px;border-bottom:1px solid rgba(128,140,150,.2);text-align:left;vertical-align:top}
table.time th{color:var(--sub);font-weight:600}
/* minimap */
.mmwrap{display:flex;gap:20px;align-items:flex-start;flex-wrap:wrap}
.mm{position:relative;width:252px;height:252px;border-radius:24px;padding:8px;background:var(--glass);box-shadow:0 8px 26px rgba(0,0,0,.45),inset 0 0 0 1px rgba(241,237,230,.14);flex:none}
.mm-clip{position:relative;width:236px;height:236px;border-radius:17px;overflow:hidden}
.mm-map{position:absolute;inset:0;background:url(%MINI%) center/cover}
.mm-vig{position:absolute;inset:0;border-radius:17px;box-shadow:inset 0 0 18px rgba(8,14,22,.55);pointer-events:none}
.mm-n{position:absolute;left:50%;top:-10px;transform:translateX(-50%);font:700 12px Maru;color:var(--gold);background:var(--navy);border:1px solid rgba(214,190,140,.8);border-radius:99px;padding:1px 9px 2px}
.mk{position:absolute;width:26px;height:26px;margin:-13px}.mk img{position:absolute;inset:0;width:26px;height:26px}
.mk.old,.mk.old img{width:22px;height:22px}img.mk.old{margin:-11px}
.mk-pulse{position:absolute;left:50%;top:50%;width:46px;height:46px;margin:-23px;border-radius:50%;border:2px solid rgba(244,215,143,.9);animation:mp 1.6s infinite}
@keyframes mp{0%{transform:scale(.35);opacity:.9}100%{transform:scale(1);opacity:0}}
.mk .ptr{position:absolute;left:4px;top:7px;width:18px;height:12px;inset:auto}
.pl{position:absolute;left:118px;top:118px}
.pl-a .cone{position:absolute;width:96px;height:96px;left:-48px;top:-48px;transform:rotate(35deg)}
.pl-a .halo{position:absolute;width:44px;height:44px;left:-22px;top:-22px}
.pl-a .arrow{position:absolute;width:30px;height:30px;left:-15px;top:-15px;transform:rotate(35deg)}
.pl .pulse{position:absolute;width:40px;height:40px;left:-20px;top:-20px;border-radius:50%;border:2px solid rgba(255,248,228,.85);animation:mp 2s infinite}
.pl-b .dot{position:absolute;width:16px;height:16px;left:-8px;top:-8px;border-radius:50%;background:#fff8e4;box-shadow:0 0 0 3px #e8c45c,0 0 0 5px #16293d,0 2px 6px rgba(0,0,0,.6)}
.pl-b .notch{position:absolute;width:0;height:0;left:-6px;top:-22px;border:6px solid transparent;border-bottom:9px solid #f4d78f;transform-origin:6px 22px;transform:rotate(35deg)}
.pl-c img{position:absolute;width:20px;height:22px;left:-10px;top:-11px}
.lg{display:flex;gap:12px;align-items:flex-start;margin:10px 0}.lg img{width:34px;height:34px;flex:none;margin-top:2px}
.lg b{font-size:15px}.lg p{margin:2px 0 0;color:var(--sub);font-size:13.5px}
.pick{outline:2px solid #d6be8c;outline-offset:3px}
figure.card{margin:0}figure.card p{margin:8px 0 0}
code{font:13px Consolas,monospace;background:rgba(128,140,150,.15);padding:1px 5px;border-radius:5px}
'''

PAGE = '''<!doctype html><html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>퀘스트·미니맵 개선 시안</title><style>%CSS%</style></head><body><main>
<h1>퀘스트 추적 · 수락/완료 연출 · 미니맵 마커</h1>
<p class="lead">새 HUD(시안 A 글래스 엣지) 1차 적용 뒤 화면에서 보인 문제를 고치는 시안입니다. 아래 그림은 실제 게임 화면과 게임에 들어갈 그림(킷 PNG, 마을 지도)으로 만들었습니다. <b>추천안(추적 유리 판, 띠 3종, A 화살표, 마름모 배지 + 가장자리 붙이기)을 게임에 적용했습니다 — 6장에 실제 화면.</b></p>

<h2>1. 지금 화면에서 보인 문제</h2>
<div class="cmp">
  <img class="shot" src="%CORNER_NOW%" alt="지금 오른쪽 위">
  <div><ol class="issues">
    <li><b>"퀘스트" 제목이 안 보임</b> — 13.5px 금색 글자가 판 없이 옅은 그늘(오른쪽만 진한 그라데이션) 위에 떠 있어서 밝은 지붕 위에서는 묻힙니다. ▲·× 버튼도 같은 이유로 거의 안 보입니다.</li>
    <li><b>지역 줄 "리베르그로브 · —"</b> — 장소 이름이 없을 때 대시가 남아 잘린 것처럼 보입니다.</li>
    <li><b>미니맵 내 위치</b> — 화살표가 방향을 돌지 않아 "어느 쪽을 보고 있는지"가 안 읽히고, 지도 줌 때문에 작게 그려집니다.</li>
    <li><b>퀘스트 표시</b> — 22px 원판에 글자라 지도 그림(갈색 지붕·노란 길)에 묻히고, 받을 퀘스트/보고/목표 지점이 거의 같은 금색입니다. 미니맵 밖에 있으면 아예 안 보입니다.</li>
    <li><b>수락·완료 순간</b> — 추적 창에 줄이 생기고 보상 창이 뜨는 것뿐이라 "받았다/끝냈다"는 손맛이 없습니다.</li>
  </ol></div>
</div>

<h2>2. 퀘스트 추적 창 — 유리 판으로</h2>
<p class="lead">미니맵·상태 창과 같은 남색 유리 판(흐림 + 0.80 남색 + 금 실선)에 올립니다. 제목 줄은 퀘스트 아이콘 + 15px 상아색, 고정한 퀘스트 수, 24px 버튼. 보고 가능한 퀘스트는 금색 줄 + "보고" 칩으로 한눈에 구별합니다.</p>
<div class="cmp">
  <div class="card"><h3>바꾼 모습 <span class="tag">추천</span></h3><div class="bgtrack">%TRACKER_NEW%</div>
    <p>같은 자리(미니맵 아래, 오른쪽 끝 맞춤), 너비 300. 지붕 위·하늘 위 어디서나 글자 대비가 같습니다.</p></div>
  <div class="card"><h3>규칙</h3><p>· 판 없는 글자는 화면 어디에도 두지 않는다 (지역 줄은 큰 글자 + 그림자라 예외)<br>· 상태 색: 금 = 보고, 청록 = 가야 할 곳, 회청 = 진행 중<br>· 장소 이름이 없으면 "·" 와 "—" 를 숨기고 지역 이름만<br>· 전투 중에는 지금처럼 접힘</p></div>
</div>

<h2>3. 수락 · 목표 달성 · 완료 연출</h2>
<p class="lead">화면 위쪽 1/3(캐릭터 머리 위)에 잠깐 떴다 사라지는 띠입니다. 지역 진입 배너와 같은 금 줄·글자 계열이라 한 게임처럼 보이고, 완료만 빛살·고리·반짝이를 더해 가장 크게 축하합니다. 여러 개가 겹치면 차례로, 전투 중에 생긴 것은 전투가 끝난 뒤 나옵니다.</p>
<div class="grid one">
  <div class="card"><h3>수락 — 새 의뢰</h3>%FX_ACCEPT%<p>금 머리말이 위에서, 제목이 아래에서 올라와 금 줄이 양옆으로 펴집니다. 2.8초. 추적 창에 새 줄이 미끄러져 들어오며 한 번 빛납니다.</p></div>
  <div class="card"><h3>목표 달성 — 보고하러 가기</h3>%FX_READY%<p>목표를 다 채운 순간(보고 가능). 청록 머리말 + 보고 대상. 추적 창의 그 줄이 금색으로 바뀌며 빛납니다.</p></div>
  <div class="card"><h3>완료 — 의뢰 완료</h3>%FX_DONE%<p>제목이 도장처럼 크게 찍히고(1.35배 → 1배), 빛살이 돌며 퍼지고, 고리 한 번 + 반짝이 8개. 3.1초. 보상 창은 1.3초 뒤(빛살이 걷힌 뒤)에 뜹니다.</p></div>
</div>
<table class="time"><tr><th>순간</th><th>게임 신호</th><th>추적 창</th><th>띠</th></tr>
<tr><td>수락</td><td>퀘스트 수락(OnQuestAccepted)</td><td>새 줄이 오른쪽에서 들어오며 금빛 한 번</td><td>새 의뢰 · 제목 · 첫 목표</td></tr>
<tr><td>목표 달성</td><td>진행 중 → 보고 가능으로 바뀜</td><td>줄이 금색 + "보고" 칩, 금빛 한 번</td><td>목표 달성 · 제목 · 보고 대상</td></tr>
<tr><td>완료</td><td>퀘스트 완료(OnQuestCompleted, 장면 연출 뒤)</td><td>줄이 빠짐</td><td>의뢰 완료 · 제목 + 빛살, 이어서 보상 창</td></tr></table>

<h2>4. 미니맵 — 내 위치</h2>
<p class="lead">같은 마을 지도, 게임과 같은 확대(지도 폭의 1/3)입니다. 세 안 모두 이동 방향으로 돕니다(카메라가 고정이라 지도 위 = 화면 안쪽).</p>
<div class="mmwrap">
  <div class="card pick"><h3>A. 화살표 + 시야 부채꼴 <span class="tag">추천</span></h3>%MAP_A%<p>상아·금 두 면 화살표, 짙은 테두리와 그늘, 옅은 부채꼴이 바라보는 쪽을 넓게 보여 줍니다. 발밑 고리가 2초마다 퍼져 "여기"가 바로 찾아집니다.</p></div>
  <div class="card"><h3>B. 점 + 방향 꼭지</h3>%MAP_B%<p>금테 흰 점과 작은 꼭지. 가장 차분하지만 방향이 작게 읽힙니다.</p></div>
  <div class="card"><h3>C. 지금</h3>%MAP_C%<p>돌지 않는 작은 화살표와 22px 원판 표시.</p></div>
</div>

<h2>5. 미니맵 — 퀘스트 표시</h2>
<p class="lead">HUD의 마름모 모양으로 통일하고, 상태마다 색을 나눴습니다. 크기 22 → 26px, 짙은 테두리 + 그늘.</p>
<div class="cmp">
  <div class="card">%LEGEND%</div>
  <div class="card"><h3>미니맵 밖의 퀘스트 — 가장자리에 붙이기</h3>%EDGE%
    <p>보고 가능·목표 지점·받을 수 있는 퀘스트는 미니맵 밖에 있어도 테두리 안쪽에 붙어 보이고, 작은 꼭지가 실제 방향을 가리킵니다(오른쪽 = 보고, 왼쪽 아래 = 목표). 포탈·진행 중·조건 미달은 붙이지 않습니다(가장자리가 시끄러워지지 않게).</p></div>
</div>

<h2>6. 게임 적용 결과 (PIE 캡처)</h2>
<p class="lead">리베르그로브 마을, 1920x1080. 띠는 <code>nu.HUD.QuestFx</code> 미리보기와 실제 퀘스트 진행(<code>nu.Quest.Progress 10001</code>) 두 가지로 확인했습니다.</p>
<div class="grid">%INGAME%</div>

<h2>7. 게임에 넣는 방법</h2>
<div class="card"><p style="color:var(--text)">
· 그림: <code>Tools/HUDRenewal/make_hud_kit.py</code> → <code>/Game/Art/UI/HUD2026/Kit</code> (새 그림 13장, 옛 그림은 그대로 둠)<br>
· 위젯: <code>WBP_HUD_Quest</code>(추적 판 + 연출 띠), <code>WBP_HUD_QuestEntry</code>, <code>WBP_HUD_MapMarker</code>, <code>WBP_HUD_MapView</code>, <code>WBP_HUD_Location</code>을 스크립트로 다시 만듦<br>
· C++: 추적 줄의 보고 상태 표시·빛(<code>UNUQuestTrackerEntryWidget</code>), 수락/달성/완료 띠(<code>FNUQuestFx</code>, 퀘스트 위젯이 호출), 미니맵 화살표 회전·가장자리 붙이기(<code>UNU_MapImageViewWidget</code>), 표시 고리·꼭지(<code>UNUWorldMarkerWidget</code>), 빈 장소 숨김(지역 위젯). 새로 넣은 위젯 이름은 전부 선택 바인딩이라 옛 WBP는 그대로 동작합니다.<br>
· 되돌리기: <code>Tools/HUDRenewal/run.ps1 -Steps revert</code> (HUD 전체를 옛 위젯으로)</p></div>
</main>
<script>
function replay(btn){const fx=btn.closest('.fx');fx.classList.remove('play');void fx.offsetWidth;fx.classList.add('play')}
const io=new IntersectionObserver(es=>es.forEach(e=>{if(e.isIntersecting){e.target.classList.add('play');io.unobserve(e.target)}}),{threshold:.4});
document.querySelectorAll('.fx').forEach(f=>io.observe(f));
</script>
</body></html>'''

if __name__ == '__main__':
    build(sys.argv[1] if len(sys.argv) > 1 else str(ROOT / 'Docs' / 'HUD_QuestMap_Proposal_20261014' / 'png' / 'now_village.png'))
