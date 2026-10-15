# -*- coding: utf-8 -*-
# 만든 효과음을 들어 보고 고르는 도구를 만든다 => Saved/SFX/SFX_Audition.html (소리를 파일 안에 담은 한 파일, 브라우저로 연다)
# 소리마다 후보 A·B·C (줄에 alt 가 있으면 다른 느낌 D·E 도, extra 가 있으면 무료 실녹음·합성 F~I 도) 를 들어 보고 바꿔 고른다.
# 고른 것은 도구의 [바꾼 것 복사] 로 복사해 Saved/SFX/picks.json 에 넣고 generate_sfx.py process 를 돌리면 반영된다
# usage: python make_audition.py
import base64
import html
import io
import json
import os

from scipy.io import wavfile

HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")
ROOT = os.path.normpath(HERE + "/../../..").replace("\\", "/")
OUT = ROOT + "/Saved/SFX"
GROUPS = [("UI", "화면 조작"), ("Field", "필드·퀘스트·대화"), ("Battle", "전투")]
STYLE = ("<style>body{font-family:'Malgun Gothic',sans-serif;background:#16293d;color:#f1ede6;margin:24px}h1{font-size:20px}h2{font-size:16px;color:#c8b691;margin-top:28px}"
         "table{border-collapse:collapse}td{padding:4px 12px 4px 0;font-size:14px;vertical-align:middle;white-space:nowrap}td.id{color:#aab6bf;font-family:Consolas,monospace;font-size:12px}"
         "button{background:#1e303d;color:#f1ede6;border:1px solid #5d6f7d;border-radius:4px;padding:4px 10px;cursor:pointer;margin-right:2px}button:hover{background:#2b4458}"
         "button.on{border-color:#c8b691;background:#4a4426;color:#fff}button.alt{border-style:dashed}button.free{border-style:dotted;border-color:#7fb38a}"
         "button.top{border-color:#c8b691;padding:6px 16px}td.note{white-space:normal;font-size:12px;color:#aab6bf;min-width:300px;max-width:560px}b.new{color:#7fb38a}"
         "span.sec{color:#aab6bf;font-size:11px}p{color:#aab6bf;font-size:13px;max-width:980px;line-height:1.6}"
         "input,textarea{padding:6px 10px;background:#1e303d;color:#f1ede6;border:1px solid #c8b691;border-radius:4px}input{width:240px}textarea{width:720px;height:44px;font-family:Consolas,monospace}"
         "#bar{position:sticky;top:0;background:#16293d;padding:8px 0;border-bottom:1px solid #5d6f7d}</style>")
SCRIPT = ("<script>const base={};const picks={};let now=null;"
          "function play(b){if(now){now.pause()}now=new Audio(b.dataset.src);now.play()}"
          "function pick(id,c){picks[id]=c;document.querySelectorAll('button[data-id=\"'+id+'\"]').forEach(b=>b.classList.toggle('on',b.dataset.c===c));show()}"
          "function changed(){const o={};for(const k in picks){if(picks[k]!==base[k])o[k]=picks[k]}return o}"
          "function show(){const o=changed();document.getElementById('out').value=JSON.stringify(o);document.getElementById('msg').innerText='바꾼 것 '+Object.keys(o).length+'개'}"
          "function copy(){const t=document.getElementById('out');t.select();document.execCommand('copy');document.getElementById('msg').innerText='복사됨 (바꾼 것 '+Object.keys(changed()).length+'개)'}"
          "function filter(){const q=document.getElementById('q').value.toLowerCase();"
          "document.querySelectorAll('tr').forEach(r=>{r.style.display=r.innerText.toLowerCase().includes(q)?'':'none'})}</script>")

rows = json.load(io.open(HERE + "/sfx_palette.json", encoding="utf-8"))["sounds"]
report = json.load(io.open(OUT + "/report.json", encoding="utf-8"))


def embed(path):
    rate, data = wavfile.read(path)
    return "data:audio/wav;base64," + base64.b64encode(open(path, "rb").read()).decode("ascii"), len(data) / float(rate)


parts = ["<!doctype html><html lang='ko'><head><meta charset='utf-8'><title>효과음 들어 보고 고르기</title>", STYLE, SCRIPT, "</head><body><h1>효과음 들어 보고 고르기</h1>",
         "<p>소리마다 후보가 있습니다. <b>A·B·C</b> 는 같은 주문으로 받은 세 가지이고, 점선 <b>D·E</b> 는 다른 느낌(나무 건반 소리)으로 받은 것입니다. "
         "전투 소리는 찾기 칸에 <b>준비</b>·<b>휘두름</b>·<b>발사</b>·<b>타격</b>·<b>버프</b>·<b>필살기</b>·<b>보스</b>·<b>나이아</b> 를 넣으면 묶어서 볼 수 있습니다. "
         "글자를 누르면 게임에 들어갈 음량 그대로 들리고, 금색이 지금 게임에 들어갈 소리입니다(자동으로 고른 것 --- 저는 소리를 들을 수 없어 파형만 재서 골랐습니다). "
         "마음에 드는 후보가 따로 있으면 그 옆 [고르기]를 누르세요. 다 고른 뒤 아래 칸의 글자를 복사해 채팅에 붙여 넣어 주시면 반영합니다. "
         "후보가 전부 별로인 소리는 이름(id)과 어떤 느낌이면 좋을지 알려 주세요 --- 주문을 고쳐 다시 받겠습니다.</p>"
         "<p>초록 점선 <b class='new'>F~I</b> 는 새로 넣은 후보입니다: 무료 실녹음(CC0 --- 상업 사용 가능, 출처는 LICENSES.md)과 계산으로 만든 합성 소리, "
         "또는 지금 소리에 실녹음 층(검 '챙', 묵직한 타격, 불 타닥거림)을 겹친 것. 무엇인지는 줄 오른쪽에 적었습니다. "
         "찾기 칸에 <b>새 후보</b> 를 넣으면 이것들만 보입니다. 무료 실녹음은 들어 보고 고르실 때까지 게임에 넣지 않았고, 활 쏘기만 합성 소리(G)로 바꿔 두었습니다.</p>",
         "<div id='bar'><input id='q' placeholder='찾기 (예: 버튼, Battle)' oninput='filter()'> <button class='top' onclick='copy()'>바꾼 것 복사</button> <span id='msg'>바꾼 것 0개</span><br>"
         "<textarea id='out' readonly>{}</textarea></div>"]
count = total = 0
for folder, title in GROUPS:
    parts.append("<h2>%s</h2><table>" % title)
    for r in rows:
        if r["folder"] != folder:
            continue
        line = report.get(r["id"])
        cells = []
        notes = []
        for letter in "abcdefghi":
            wav = "%s/cand/%s_%s.wav" % (OUT, r["id"], letter)
            if line is None or not os.path.exists(wav) or "score" not in line["candidates"].get(letter, {}):
                continue
            src, sec = embed(wav)
            total += 1
            note = r.get("extra", {}).get(letter, {}).get("note", "")
            if note:
                notes.append("<b class='new'>%s</b> %s" % (letter.upper(), html.escape(note)))
            cells.append("<button class='%s' title='%s' data-src='%s' onclick='play(this)'>%s <span class='sec'>%.2f초</span></button>"
                         "<button data-id='%s' data-c='%s' class='%s' onclick=\"pick('%s','%s')\">고르기</button> &nbsp;"
                         % ("alt" if letter in "de" else "free" if letter in "fghi" else "", html.escape(note, quote=True), src, letter.upper(), sec,
                            r["id"], letter, "on" if line["chosen"] == letter else "", r["id"], letter))
        if cells:
            count += 1
            parts.append("<script>base['%s']=picks['%s']='%s'</script>" % (r["id"], r["id"], line["chosen"]))
        parts.append("<tr><td>%s%s</td><td class='id'>%s</td><td>%s</td><td class='note'>%s</td></tr>" % (
            html.escape(r["ko"]), " <b class='new'>새 후보</b>" if notes else "", r["id"], "".join(cells) or "아직 없음", "<br>".join(notes)))
    parts.append("</table>")
parts.append("</body></html>")
out = OUT + "/SFX_Audition.html"
io.open(out, "w", encoding="utf-8").write("".join(parts))
print("소리 %d개, 후보 %d개 => %s (%.1f MB)" % (count, total, out, os.path.getsize(out) / 1e6))
