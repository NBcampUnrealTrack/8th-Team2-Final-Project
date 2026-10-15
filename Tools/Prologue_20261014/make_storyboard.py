# -*- coding: utf-8 -*-
"""프롤로그 스토리보드 HTML 을 만든다: Docs/Prologue_20261014/index.html (+ captures/)

- timeline.json 의 컷·자막·시각
- PIE 캡처(Saved/Prologue_20261014/<qa 폴더>/t??_*.png) 중 컷마다 대표 한 장
- GPT·Suno·Tripo·Grok 요청 글(복사 버튼)
사용: python -I make_storyboard.py <qa 폴더 이름>
"""
import html
import json
import os
import re
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOLS = os.path.join(ROOT, 'Tools', 'Prologue_20261014')
DOCS = os.path.join(ROOT, 'Docs', 'Prologue_20261014')
PROMPTS = os.path.join(ROOT, 'ArtSource', 'Prologue_20261014', 'prompts')
QA = os.path.join(ROOT, 'Saved', 'Prologue_20261014', sys.argv[1] if len(sys.argv) > 1 else 'qa3')
CAP = os.path.join(DOCS, 'captures')
TL = json.load(open(os.path.join(TOOLS, 'timeline.json'), encoding='utf-8'))


def read(path):
    return open(path, encoding='utf-8').read()


def stamps():
    rows = []
    path = os.path.join(QA, 'stamps.jsonl')
    if os.path.exists(path):
        for line in open(path, encoding='utf-8'):
            try:
                rows.append(json.loads(line))
            except ValueError:
                pass
    return rows


def captures():
    """(시퀀스 시각, 파일) — 도우미가 기록한 시각을 캡처 순서대로 붙인다."""
    shots = sorted(f for f in os.listdir(QA) if re.match(r'[tvc]\d\d_.*\.png$', f))
    times = [s.get('seq_time') for s in stamps()]
    out = []
    for i, f in enumerate(shots):
        t = times[i] if i < len(times) else None   # 도우미는 캡처 직후에 시각을 남긴다: i번째 기록 = i번째 캡처
        out.append((t, f))
    return out


def export(src, name):
    os.makedirs(CAP, exist_ok=True)
    im = Image.open(os.path.join(QA, src)).convert('RGB')
    w, h = im.size
    if h > 1080:                      # 창 제목줄(36px) 제거
        im = im.crop((0, h - 1080 - 2, w, h - 2))
    im.thumbnail((1280, 720))
    im.save(os.path.join(CAP, name), quality=88)
    return 'captures/' + name


def pick(caps, t0, t1):
    """구간 안에서 구간의 60% 지점에 가장 가까운 캡처(전환 중인 프레임을 피한다)."""
    target = t0 + (t1 - t0) * 0.6
    inside = [(abs(t - target), f) for t, f in caps if t is not None and t0 <= t <= t1]
    return min(inside)[1] if inside else None


def copy_block(label, text):
    tid = 'c%d' % abs(hash(label + text[:40]))
    return ('<div class="copy"><div class="copyhead"><span>%s</span><button data-t="%s">복사</button></div>'
            '<pre id="%s">%s</pre></div>') % (html.escape(label), tid, tid, html.escape(text))


def main():
    caps = captures()
    beats = []
    s = TL['slides']
    sw = [x['switch'] for x in s] + [TL['length']]
    title_in = TL['tracks']['TitleOpacity'][1][0]
    plan = [
        ('그림 P1 · 요람', '밤, 고목 뿌리 사이 요람(아기 손만)과 낡은 책. 나이아 여섯(아쿠퐁·볼토·푸루나·불코이·토그라·휘리)이 아기를 지킨다. 오르골 도입에 맞춰 천천히 밝아지고 도트 반짝이가 떠오른다', 0.0, sw[1], s[0]['lines'], 'p1_cradle.txt'),
        ('그림 P2 · 술래잡기', '아침 숲 공터, 뒷모습의 아이가 나이아들과 술래잡기. 곡의 첫 프레이즈(11.2초)에서 P1 위로 녹아들 듯 크로스페이드', sw[1], sw[2], s[1]['lines'], 'p2_child.txt'),
        ('그림 P3 · 책', '늦은 오후, 열 살 아이가 요람의 그 책을 펼치고 나이아들이 둘러앉는다(불코이의 불이 등불)', sw[2], sw[3], s[2]['lines'], 'p3_book.txt'),
        ('그림 P4 · 문턱', '새벽, 뿌리 아치 아래 두건 쓴 여행자. 나이아들이 배웅하고 휘리가 따라 나선다. 곡이 숨을 고르는 30.4초에 따뜻한 새벽빛이 화면을 덮는다', sw[3], 30.4, s[3]['lines'][:1], 'p4_threshold.txt'),
        ('실제 게임 화면', '빛이 걷히며 지금 게임 화면이 나오고 위아래 레터박스가 내려온다. 큰 섹션이 시작되는 31.4초에 맞춰 내 캐릭터가 남문으로 걸어 들어간다. 자막은 검은 띠 위', 30.4, sw[4], s[3]['lines'][1:], None),
        ('마을 · 와이드 → 클로즈', '38.2초 박자에 짧게 암전 컷. 광장 와이드(사람과 나이아) → 46초 2절에서 인물·나이아 클로즈', sw[4], title_in, s[4]['lines'], None),
        ('타이틀', '56.45초 클라이맥스에 빛 번쩍임과 함께 위로 물러나는 카메라, 타이틀이 살짝 줄어들며 자리 잡고 반짝이가 올라온다 → 검게 닫힘 → 게임 화면이 밝아지며 조작 시작', title_in, TL['length'], [], None),
    ]
    style = read(os.path.join(PROMPTS, '_style.txt'))
    cards = []
    for i, (name, desc, t0, t1, lines, prompt) in enumerate(plan):
        f = pick(caps, t0 + 0.5, t1)
        img = '<img src="%s" alt="%s">' % (export(f, 'beat%d.jpg' % i), html.escape(name)) if f else '<div class="noimg">캡처 없음</div>'
        subs = ''.join('<li><b>%.1f초</b> %s</li>' % (l['start'], html.escape(l['text'])) for l in lines) or '<li class="muted">자막 없음</li>'
        extra = copy_block('GPT 프롬프트 (공통 화풍 + 이 그림)', style + '\n' + read(os.path.join(PROMPTS, prompt))) if prompt else ''
        cards.append('<section class="beat"><div class="shot">%s</div><div class="txt"><h3><span class="tc">%.1f – %.1f초</span>%s</h3>'
                     '<p>%s</p><ul>%s</ul>%s</div></section>' % (img, t0, min(t1, TL['length']), html.escape(name), html.escape(desc), subs, extra))

    # 타임라인 막대
    total = TL['length']
    segs = [(p[2], min(p[3], total), p[0]) for p in plan]
    bar = ''.join('<div class="seg s%d" style="left:%.2f%%;width:%.2f%%" title="%s"><span>%s</span></div>'
                  % (i, a / total * 100, max(0.5, (b - a) / total * 100), html.escape(n), html.escape(n)) for i, (a, b, n) in enumerate(segs))

    suno = read(os.path.join(DOCS, 'SUNO_BGM.md'))
    blocks = re.findall(r'```\n(.*?)```', suno, re.S)
    tripo = read(os.path.join(DOCS, 'TRIPO_ROOT_GATE.md'))
    tripo_prompt = re.findall(r'```\n(.*?)```', tripo, re.S)[0]
    grok = read(os.path.join(DOCS, 'GROK_VIDEO.md'))
    grok_common = re.findall(r'```\n(.*?)```', grok, re.S)[0]
    requests = (
        '<h2>Suno — 프롤로그 BGM</h2><p>Instrumental 켬, 70초 안팎. 받은 파일은 <code>ArtSource/Prologue_20261014/audio/BGM_Prologue_Spirit.wav</code>.</p>'
        + copy_block('Style of Music', blocks[0].strip()) + copy_block('Exclude Styles', blocks[1].strip()) + copy_block('Lyrics 칸(구조 지시)', blocks[2].strip())
        + '<h2>Tripo — PR01 정령 숲의 뿌리 문 (선택)</h2><p>380×170×330 게임cm, 통로 180×230, Quad 1,500~2,500, 게임 삼각형 5,000 이하. 자세한 건 TRIPO_ROOT_GATE.md.</p>'
        + copy_block('Tripo 프롬프트', tripo_prompt.strip())
        + '<h2>Grok — 그림책 미세 움직임 (선택)</h2><p>기본안엔 필요 없음. 그림 확정 뒤 원하면.</p>' + copy_block('공통 조건', grok_common.strip())
    )
    # 연출 장면(시퀀스를 정확한 시각으로 옮겨 멈춘 캡처): qa8
    trans_dir = os.path.join(ROOT, 'Saved', 'Prologue_20261014', 'qa8')
    labels = [(4.5, '도트 반짝이 입자'), (11.55, '그림 크로스페이드 P1→P2'), (19.3, '크로스페이드 P2→P3'), (25.3, '크로스페이드 P3→P4'),
              (30.0, '새벽빛이 그림을 덮기 시작'), (30.55, '빛이 화면 전체를 덮음'), (31.4, '빛이 걷히며 게임 화면 + 레터박스'),
              (38.15, '박자에 맞춘 짧은 암전 컷'), (56.5, '클라이맥스 빛 번쩍임'), (58.5, '타이틀 + 반짝이')]
    tiles = []
    for i, (t, label) in enumerate(labels):
        src = 's%02d.png' % (i + 1)
        if os.path.exists(os.path.join(trans_dir, src)):
            os.makedirs(CAP, exist_ok=True)
            im = Image.open(os.path.join(trans_dir, src)).convert('RGB')
            w, h = im.size
            if h > 1080:
                im = im.crop((0, h - 1080 - 2, w, h - 2))
            im.thumbnail((960, 540))
            name = 'trans%02d.jpg' % i
            im.save(os.path.join(CAP, name), quality=88)
            tiles.append('<figure><img src="captures/%s" alt="%s"><figcaption><b>%.1f초</b> %s</figcaption></figure>' % (name, html.escape(label), t, html.escape(label)))
    transitions = '<h2>전환·모션그래픽 (해당 시각에 멈춰 찍은 실제 화면)</h2><div class="grid">' + ''.join(tiles) + '</div>' if tiles else ''
    page = TEMPLATE.replace('{{BAR}}', bar).replace('{{TRANSITIONS}}', transitions).replace('{{CARDS}}', '\n'.join(cards)).replace('{{REQUESTS}}', requests).replace('{{LEN}}', '%.1f' % total)
    open(os.path.join(DOCS, 'index.html'), 'w', encoding='utf-8').write(page)
    print('written', os.path.join(DOCS, 'index.html'), len(caps), 'captures')


TEMPLATE = r'''<!doctype html>
<html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>프롤로그 스토리보드</title>
<style>
:root{--bg:#f4f1ea;--panel:#fffdf8;--ink:#1f2a33;--muted:#6b7680;--line:#ddd5c6;--gold:#b08d4a;--navy:#16293d;--code:#f0ebe0}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#11161b;--panel:#182029;--ink:#e9e4da;--muted:#9aa6b0;--line:#2b3742;--gold:#d1b47c;--navy:#0d1822;--code:#0f151b}}
:root[data-theme="dark"]{--bg:#11161b;--panel:#182029;--ink:#e9e4da;--muted:#9aa6b0;--line:#2b3742;--gold:#d1b47c;--navy:#0d1822;--code:#0f151b}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.6 "Pretendard","Malgun Gothic",system-ui,sans-serif}
main{max-width:1180px;margin:0 auto;padding:28px 16px 64px;overflow-wrap:anywhere}
h1{font-size:28px;margin:0 0 4px}h2{font-size:20px;margin:36px 0 10px;border-bottom:1px solid var(--line);padding-bottom:6px}
.sub{color:var(--muted);margin:0 0 18px}
.status{background:var(--panel);border:1px solid var(--line);border-left:4px solid var(--gold);padding:12px 16px;border-radius:6px}
.status li{margin:2px 0}
.bar{position:relative;height:44px;background:var(--panel);border:1px solid var(--line);border-radius:6px;margin:14px 0 4px;overflow:hidden}
.seg{position:absolute;top:0;bottom:0;border-right:1px solid var(--bg);display:flex;align-items:center;padding:0 6px;font-size:12px;color:#fff;white-space:nowrap;overflow:hidden}
.s0,.s1,.s2,.s3{background:#41566a}.s4{background:#5c7a3f}.s5{background:#7b6a3a}.s6{background:#1d1d1d}
.axis{display:flex;justify-content:space-between;color:var(--muted);font-size:12px}
.beat{display:grid;grid-template-columns:minmax(0,1.25fr) minmax(0,1fr);gap:18px;background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:14px;margin:14px 0}
.shot img{width:100%;display:block;border-radius:4px;image-rendering:auto}
.noimg{aspect-ratio:16/9;display:flex;align-items:center;justify-content:center;background:var(--code);color:var(--muted);border-radius:4px}
.txt h3{margin:0 0 6px;font-size:18px}.tc{display:inline-block;font-size:12px;color:var(--gold);margin-right:8px;font-weight:600}
.txt ul{padding-left:18px;margin:8px 0}.muted{color:var(--muted)}
.copy{margin:10px 0;border:1px solid var(--line);border-radius:6px;overflow:hidden}
.copyhead{display:flex;justify-content:space-between;align-items:center;background:var(--code);padding:6px 10px;font-size:13px}
.copyhead button{border:1px solid var(--line);background:var(--panel);color:var(--ink);border-radius:4px;padding:2px 10px;cursor:pointer}
pre{margin:0;padding:10px;white-space:pre-wrap;word-break:break-word;font:12px/1.5 Consolas,monospace;max-height:220px;overflow:auto;background:var(--panel)}
figure{margin:0}figure img{width:100%;display:block;border-radius:4px}figcaption{font-size:13px;color:var(--muted);margin-top:4px}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:12px}
code{background:var(--code);padding:1px 5px;border-radius:3px;font-size:13px}
table{border-collapse:collapse;width:100%;table-layout:fixed}th{width:7em}td,th{border-bottom:1px solid var(--line);padding:6px 8px;text-align:left;vertical-align:top;overflow-wrap:anywhere}
@media (max-width:760px){.beat{grid-template-columns:1fr}.seg span{display:none}}
</style></head><body><main>
<h1>게임 시작 프롤로그 — 정령이 키운 아이</h1>
<p class="sub">새 캐릭터가 처음 필드에 들어올 때 한 번. 도트 그림책 4장 → 실제 게임 화면 → 마을 전경 → 타이틀 → 바로 조작. 총 {{LEN}}초.</p>
<div class="status"><b>현재 상태 (2026-10-14)</b><ul>
<li>엔진: 재생·실제 캐릭터 걷기·마을 컷·타이틀·게임 복귀까지 PIE로 확인. 자동 재생 조건 3종(새 캐릭터 재생 / 본 캐릭터 안 함 / 입장 실패 화면에선 안 함) 통과.</li><li>미검증: 서버 접속 상태의 실제 신규 캐릭터 흐름, 키보드·패드 실제 길게 누르기, 소리.</li>
<li>그림 4장: <b>GPT로 생성해 반영</b> — 실제 게임 나이아 6종(아쿠퐁·볼토·푸루나·불코이·토그라·휘리)을 스프라이트 그대로 그림. P1을 화풍 기준으로 P2~P4 생성, 704×396 도트 격자. Tripo 참고 그림 4장도 생성.</li>
<li>음악: <b>Suno 곡 'Nia Unbound'(64.9초) 연결</b> — 컷·자막·연출을 곡 구간(6.5 / 11.2 / 19.0 / 25.0 / 30.4 / 31.4 / 46.0 / 56.4초)에 맞춤.</li><li>연출: 그림 크로스페이드, 도트 반짝이 입자, 새벽빛 전환, 시네마 레터박스(자막은 띠 위), 마을 3컷 카메라, 클라이맥스 빛 번쩍임, 타이틀 축소 등장, 자막 페이드(살짝 떠오름).</li>
<li>기존 zxcv1 님의 아르카디아/그리드폴 프롤로그 시안은 손대지 않음 — 정본 결정 필요.</li></ul></div>
<h2>타임라인</h2>
<div class="bar">{{BAR}}</div><div class="axis"><span>0초</span><span>{{LEN}}초</span></div>
<h2>컷별 (사진은 PIE 실제 캡처)</h2>
{{CARDS}}
{{TRANSITIONS}}
<h2>재생 규칙</h2>
<table>
<tr><th>자동 재생</th><td>캐릭터 선택을 거쳐 필드에 들어왔고, 레벨 1 이하, 이 PC에서 그 캐릭터로 처음. 로딩 화면이 걷히고(온라인이면 필드 입장 확정 뒤) 0.4초 뒤.</td></tr>
<tr><th>스킵</th><td>"길게 눌러 건너뛰기" 버튼 또는 Esc / 패드 B·메뉴를 1초. 끝까지 보거나 스킵하면 본 것으로 기록.</td></tr>
<tr><th>끝</th><td>검은 화면 → 게임 화면이 1.2초 동안 밝아지고 입력·HUD 복귀. 재생 중 레벨 BGM은 꺼짐.</td></tr>
<tr><th>콘솔</th><td><code>nu.Prologue.Play</code> 다시 보기 · <code>nu.Prologue.ResetSeen</code> 기록 지우기 · <code>nu.Prologue.Auto 0</code> 자동 재생 끄기</td></tr>
</table>
{{REQUESTS}}
</main>
<script>
document.querySelectorAll('.copyhead button').forEach(function(b){b.addEventListener('click',function(){
var t=document.getElementById(b.dataset.t).innerText;var done=function(){b.textContent='복사됨';setTimeout(function(){b.textContent='복사'},1200)};
if(navigator.clipboard&&navigator.clipboard.writeText){navigator.clipboard.writeText(t).then(done,function(){fallback(t);done()})}else{fallback(t);done()}})});
function fallback(t){var a=document.createElement('textarea');a.value=t;document.body.appendChild(a);a.select();try{document.execCommand('copy')}catch(e){}a.remove()}
</script></body></html>'''

if __name__ == '__main__':
    main()
