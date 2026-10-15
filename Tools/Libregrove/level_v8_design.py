"""Deterministic centimeter terrain, circulation and village placements (no UE writes)."""
import json,math,random
from pathlib import Path
OUT=Path(__file__).resolve().parents[2]/'Docs/Libregrove/Level_V8_20260908'
N=253;SPACING=36.0;ORIGIN=-4536.0
def smooth(a,b,x):
    t=max(0,min(1,(x-a)/(b-a)));return t*t*(3-2*t)
def seg(x,y,a,b):
    dx=b[0]-a[0];dy=b[1]-a[1];t=max(0,min(1,((x-a[0])*dx+(y-a[1])*dy)/(dx*dx+dy*dy)))
    return math.hypot(x-a[0]-t*dx,y-a[1]-t*dy)
def distance(x,y,points):return min(seg(x,y,a,b) for a,b in zip(points,points[1:]))
RIVER=[(-2350,2050),(-2140,1450),(-2180,700),(-2290,140),(-2060,-440),(-1610,-720),(-700,-855),(0,-720),(1100,-810),(1900,-680),(3300,-600),(4560,-610)]
BRIDGES=[{'id':'West','x':-1820,'y':-590,'length':750,'width':165,'yaw':-32}, {'id':'Central','x':0,'y':-720,'length':750,'width':205,'yaw':0}, {'id':'East','x':1900,'y':-680,'length':740,'width':165,'yaw':0}]
ROADS=[
 {'id':'SouthMain','kind':'Cobblestone','width':200,'points':[(0,-3480),(0,-1800),(0,-1070)]},
 {'id':'NorthMain','kind':'Cobblestone','width':200,'points':[(0,-370),(0,200),(0,220)]},
 {'id':'SanctuaryProcessional','kind':'Cobblestone','width':200,'points':[(0,1150),(0,1670),(0,2390)]},
 {'id':'LowerWest','kind':'Dirt','width':140,'points':[(0,-1890),(-780,-1890),(-1620,-1760),(-1650,-1090),(-2020,-905)]},
 {'id':'LowerEast','kind':'Dirt','width':140,'points':[(0,-1890),(930,-1910),(1900,-1700),(1900,-1050)]},
 {'id':'WestBank','kind':'Dirt','width':145,'points':[(-1620,-270),(-1390,-55),(-1450,290),(-1070,320),(-540,450),(-420,675)]},
 {'id':'MillFootpath','kind':'Dirt','width':120,'points':[(-1390,-55),(-1740,-205),(-2000,60),(-1790,370),(-1790,980),(-1910,1200)]},
 {'id':'EastBank','kind':'Dirt','width':150,'points':[(1900,-310),(1890,270),(1800,760),(1150,730),(450,675)]},
 {'id':'WorkshopLane','kind':'Dirt','width':145,'points':[(1800,760),(1800,1240),(1860,1740),(2520,1950),(3000,2160)]},
 {'id':'UpperWest','kind':'Dirt','width':135,'points':[(-420,820),(-940,1100),(-900,1700),(0,1870)]},
 {'id':'CouncilLane','kind':'Cobblestone','width':145,'points':[(0,1670),(650,1600),(1170,1660),(1800,1740)]},
 {'id':'PortalWalk','kind':'Cobblestone','width':145,'points':[(0,2170),(620,2170),(850,2300)]},
 {'id':'WestExit','kind':'Dirt','width':150,'points':[(-1620,-270),(-2560,-250),(-3400,-350),(-4000,-500)]},
 {'id':'SouthEastExit','kind':'Dirt','width':150,'points':[(1900,-1700),(2550,-2400),(3450,-2700),(4000,-2850)]},
 {'id':'LowerSouth','kind':'Dirt','width':130,'points':[(-1520,-1760),(-1560,-2520),(0,-2660),(1700,-2590),(2100,-2310)]},
]
BUILDINGS=[]
def house(ident,kind,x,y,yaw=180,scale=1,zone='LowerVillage'):
    BUILDINGS.append({'id':ident,'kind':kind,'x':x,'y':y,'yaw':yaw,'scale':scale,'zone':zone})
for i,(x,y,yaw,s) in enumerate([(-430,-1440,180,1),(-1070,-1350,174,.95),(-620,-2250,185,1),(-1320,-2150,180,.92),(-1100,-2890,180,1.02),(-450,-2990,185,.93),(490,-1510,180,1),(1290,-1380,173,.94),(710,-2300,180,1.04),(1450,-2200,187,.92),(520,-2990,180,.96),(1320,-2900,174,1),(2230,-1970,166,.88)]):house('Cottage_%02d'%i,'Cottage_A',x,y,yaw,s)
house('RiversideInn','Inn',-1215,675,180,1.15,'Waterfront')
house('NaiaShelter','NaiaShelter',-1930,700,180,1,'Waterfront')
house('Watermill','WatermillHouse',-1920,80,170,1,'Waterfront')
house('VillageHall','VillageHall',675,1090,180,1.12,'Square')
house('CouncilHall','VillageHall',720,1850,180,1.0,'Sanctuary')
house('Sanctuary','Sanctuary',0,2730,180,1.1,'Sanctuary')
for i,(x,y) in enumerate([(-650,1230),(-1250,1420),(-600,1890),(-1050,2230),(1250,300),(1150,1370)]):house('UpperCottage_%02d'%i,'Cottage_A',x,y,180,0.92,'UpperVillage')
for i,(x,y,yaw) in enumerate([(1490,1110,180),(2260,1150,180),(1510,210,180),(2340,350,180)]):house('Workshop_%02d'%i,'Workshop',x,y,yaw,1 if i==0 else .95,'Workshops')
for i,(x,y,yaw) in enumerate([(-530,100,180),(550,150,180),(-680,450,90),(580,650,270),(-280,1170,180)]):house('Market_%02d'%i,'MarketStall',x,y,yaw,.92,'Square')
def roadweight(x,y):
    cob=max(1-smooth(395,465,math.hypot(x,y-675)),1-smooth(270,325,math.hypot(x,y-2670)))
    dirt=0
    for r in ROADS:
        w=1-smooth(r['width']/2-15,r['width']/2+45,distance(x,y,r['points']))
        if r['kind']=='Cobblestone':cob=max(cob,w)
        else:dirt=max(dirt,w)
    return cob,dirt
def base(x,y):
    z=30*smooth(-550,-280,y)
    z+=195*smooth(1930,2450,y)*(1-smooth(520,1150,abs(x)))
    # Forest rim hills, particularly the north-west waterfall head.
    rim=max(smooth(2760,4210,abs(x)),smooth(3250,4370,abs(y)))
    z+=rim*(180+110*math.sin(x/540)*math.cos(y/690))
    z+=430*math.exp(-((x+2460)/630)**2-((y-2380)/740)**2)
    return z
def height(x,y):
    cob,dirt=roadweight(x,y)
    z=base(x,y)+(1-max(cob,dirt))*(8*math.sin(x/120)*math.sin(y/190)+5*math.cos((x+y)/75))
    for b in BUILDINGS:
        # Terraces use source exterior footprint; roads do not cross buildings.
        if b['kind']=='MarketStall':continue
        d=math.hypot(x-b['x'],y-b['y']);radius=220*b['scale']
        a=1-smooth(radius,radius+120,d)
        if a:z=z*(1-a)+base(b['x'],b['y'])*a
    rd=distance(x,y,RIVER)
    w=1-smooth(145,300,rd)
    bed=-140+13*math.sin(x/145)*math.cos(y/130)
    z=z*(1-w)+bed*w
    # Broad ornamental pool under the waterfall; same connected water level.
    lake=math.sqrt(((x+2160)/380)**2+((y-1480)/600)**2)
    w=1-smooth(.72,1.18,lake);z=z*(1-w)+(-130)*w
    # Bridge approaches are continuous bank-height ramps, no hidden steep ledges.
    for b in BRIDGES:
        ang=math.radians(b['yaw']);dx=x-b['x'];dy=y-b['y']
        lx=dx*math.cos(ang)+dy*math.sin(ang);ly=-dx*math.sin(ang)+dy*math.cos(ang)
        if abs(lx)<b['width']/2+55 and 340<abs(ly)<520:
            a=(1-smooth(b['width']/2,b['width']/2+55,abs(lx)))*(1-smooth(365,520,abs(ly)))
            z=z*(1-a)+45*a
    return round(z,3)
if __name__=='__main__':
    assert not (OUT/'design_v001.json').exists()
    heights=[];weights={k:[] for k in ['Grass','Dirt','Cobblestone','Moss']}
    for j in range(N):
        y=ORIGIN+j*SPACING
        for i in range(N):
            x=ORIGIN+i*SPACING;heights.append(height(x,y));c,d=roadweight(x,y)
            rd=distance(x,y,RIVER)
            d=max(d,(1-smooth(170,330,rd))*.85)
            for b in BUILDINGS:d=max(d,(1-smooth(145*b['scale'],260*b['scale'],math.hypot(x-b['x'],y-b['y'])))*.92)
            m=max(smooth(2300,3600,abs(x)),smooth(2800,3600,abs(y)))*.75
            d*=1-c;m*=(1-c)*(1-d);g=max(0,1-c-d-m)
            raw=[g,d,c,m];total=sum(raw);q=[int(round(v/total*255)) for v in raw];q[q.index(max(q))]+=255-sum(q)
            for k,v in zip(weights,q):weights[k].append(v/255)
    for b in BUILDINGS:b['z']=base(b['x'],b['y'])-1.5
    plan={'version':1,'units':'UE centimeters; player visual height 88cm','N':N,'spacing':SPACING,'origin':ORIGIN,'heights':heights,'weights':weights,'buildings':BUILDINGS,'roads':ROADS,'river':RIVER,'bridges':BRIDGES}
    with (OUT/'design_v001.json').open('x',encoding='utf8') as f:json.dump(plan,f,separators=(',',':'))
    print(json.dumps({'vertices':len(heights),'height_range':[min(heights),max(heights)],'buildings':len(BUILDINGS),'roads':len(ROADS),'bridges':len(BRIDGES)}))
