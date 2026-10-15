"""Rasterize Korean signage with a local font (no generated or downloaded art)."""
from PIL import Image, ImageDraw, ImageFont
from pathlib import Path
import json
ROOT=Path(__file__).resolve().parent
OUT=ROOT/'signs';OUT.mkdir(exist_ok=True)
SIGN_DATA=[
 ('VillageName',['신데하스','CINDEHEARTH'],(8,-87,3.3),(5,2.0),0),
 ('Rockfall',['낙반 위험 구역'],(12,96.7,9.15),(4.8,1.2),0),
 ('ShiftBoard',['채굴 배정표','1조 · 북갱 / 2조 · 서갱'],(-21.8,-4,5.8),(2.6,1.1),-90),
 ('Smithy',['대장간'],(-7,15.42,6.4),(3.0,.9),0),
 ('Tavern',['주막 · 프레이'],(-8,-62.10,3.2),(4.2,1.0),0),
 ('Assay',['감정소'],(27.92,8,6.25),(2.8,.8),-90),
 ('Clinic',['의무소'],(31,-50.60,2.8),(2.4,.8),0),
 ('Gorm',['고름'],(-23.3,-39.12,1.3),(.65,.3),0),
 ('DorenSeal',['안식원 수석 설계관','도렌'],(-23.8,-39.64,1.3),(.68,.40),0),
 ('ArchiveClue',['섀도우베일 반출 기록','정류 계획 자재 할당분','이중 고리 문양 설계 · 도렌'],(32,7.6,.70),(2.4,1.05),0),
]
for name,lines,*_ in SIGN_DATA:
    im=Image.new('RGBA',(768,256),(0,0,0,0));draw=ImageDraw.Draw(im)
    draw.rounded_rectangle((4,5,763,250),radius=6,fill=(38,37,33,255),outline=(134,115,78,255),width=6)
    font=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',70 if len(lines)==1 else 57 if len(lines)==2 else 45)
    for i,line in enumerate(lines):
        b=draw.textbbox((0,0),line,font=font);w=b[2]-b[0]
        y=(256-len(lines)*72)/2+i*72-b[1]
        draw.text(((768-w)/2,y),line,font=font,fill=(205,194,166,255))
    im.save(OUT/('T_CH_'+name+'.png'))
(ROOT/'signs.json').write_text(json.dumps(SIGN_DATA,ensure_ascii=False,indent=2),encoding='utf-8')
print('Rendered',len(SIGN_DATA),'Korean signs')
