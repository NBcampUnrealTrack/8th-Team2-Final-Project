"""Cindehearth authoring source. Metres in design space; +Y is north.

Only this village uses these recipes. Regeneration never touches Libergrove.
"""
from collections import defaultdict
from pathlib import Path
import json
import math
import random

ROOT = Path(__file__).resolve().parent
R = random.Random(11020)
modules = {}
placements = []
lights = []
markers = []
meshes = defaultdict(list)

def part(mat, kind, pos, size, rot=(0, 0, 0)):
    meshes[mat].append(dict(kind=kind, pos=pos, size=size, rot=rot))

def box(mat, pos, size, rot=(0, 0, 0)):
    part(mat, 'box', pos, size, rot)

def sphere(mat, pos, size):
    part(mat, 'sphere', pos, size)

def torus(mat, pos, radius, tube, rot=(0, 0, 0)):
    part(mat, 'torus', pos, (radius, tube, 0), rot)

def cylinder(mat, pos, radius, height, rot=(0, 0, 0)):
    part(mat, 'cylinder', pos, (radius, height, 0), rot)

def beam(mat, a, b, width, depth=None):
    delta = [b[i]-a[i] for i in range(3)]
    length = math.sqrt(sum(d*d for d in delta))
    pitch = math.degrees(math.atan2(delta[2], math.hypot(delta[0], delta[1])))
    yaw = math.degrees(math.atan2(delta[1], delta[0]))
    box(mat, [(a[i]+b[i])/2 for i in range(3)], (length, width, depth or width), (pitch, yaw, 0))

def finish(name):
    modules[name] = dict(meshes)
    meshes.clear()

def place(name, module, pos, yaw=0, scale=(1, 1, 1), zone='Town', tags=()):
    placements.append(dict(name=name, module=module, pos=pos, yaw=yaw, scale=scale, zone=zone, tags=list(tags)))

def mark(name, pos, zone, tags=()):
    markers.append(dict(name=name, pos=pos, zone=zone, tags=list(tags)))

def light(name, pos, radius, intensity, group='Hearth', color=(1, .25, .045)):
    lights.append(dict(name=name, pos=pos, radius=radius, intensity=intensity, group=group, color=color))

def building(name, w, d, h, open_front=False, lean=False, windows=True):
    # Thick stone shell with an actual 1.6 x 2.6m doorway.
    for x in (-w/2+.24, w/2-.24):
        box('Stone', (x, 0, h/2), (.48, d, h))
    box('Stone', (0, d/2-.24, h/2), (w, .48, h))
    if not open_front:
        for side in (-1, 1):
            box('Stone', (side*(w/4+.4), -d/2+.24, h/2), (w/2-.8, .48, h))
        box('Stone', (0, -d/2+.24, (h+2.6)/2), (1.6, .48, h-2.6))
        box('Timber', (0, -d/2+.29, 1.26), (1.46, .14, 2.52))
        for z in (.4, 2.1):
            box('Iron', (0, -d/2+.18, z), (1.38, .1, .1))
        torus('Brass', (.48, -d/2+.06, 1.2), .12, .035, (90, 0, 0))
    # Stone quoins and foundations avoid decorative timber framing.
    for z in (.28, h-.15):
        box('StoneLight', (0, d/2, z), (w+.12, .24, .28))
        for x in (-w/2, w/2):
            box('StoneLight', (x, 0, z), (.24, d+.12, .28))
    for x in (-w/2, w/2):
        for z in (.8, 1.8, 2.8):
            box('StoneLight', (x, -d/2, z), (.64, .66, .34))
    # Unequal, overlapping steep plates. No turned eaves.
    slope = .80 if not lean else .53
    for side in (-1, 1):
        for row in range(4):
            xx = side*(row+.5)*(w/2+.4)/4
            for col in range(max(4, round(d/1.1))):
                n = max(4, round(d/1.1))
                yy = -d/2-.36 + (col+.5)*(d+.72)/n
                zz = h + (w/2+.4-abs(xx))*slope
                box('Roof' if (col+row)%3 else 'RoofPatch', (xx, yy, zz),
                    ((w/2+.4)/4*math.sqrt(1+slope*slope)+.13, (d+.72)/n+.09, .13),
                    (-side*math.degrees(math.atan(slope)), 0, 0))
    beam('Iron', (0, -d/2-.43, h+(w/2+.4)*slope+.1), (0, d/2+.43, h+(w/2+.4)*slope+.1), .18)
    # Gable infill steps under the roof, stone rather than half-timber.
    for i in range(8):
        width=w*(1-i/8)
        z=h+i*(w/2*slope)/8
        for yy in (-d/2+.25, d/2-.25):
            box('Stone', (0, yy, z+(w/2*slope)/16), (width, .42, (w/2*slope)/8))
    if windows:
        for x in (-w*.30, w*.30):
            for z in ([1.9, h-1.1] if h>5 else [1.9]):
                box('Iron', (x, -d/2-.035, z), (1.0, .16, 1.15))
                box('Window', (x, -d/2-.13, z), (.74, .04, .90))
                box('Iron', (x, -d/2-.17, z), (.055, .04, .90))
                box('Iron', (x, -d/2-.17, z), (.74, .04, .055))
    cx=w*.30
    box('Stone', (cx, d*.20, h+1.6), (.9, 1.05, 3.4))
    box('StoneLight', (cx, d*.20, h+3.35), (1.12, 1.22, .26))
    box('Dark', (cx, d*.20, h+3.49), (.7, .78, .04))
    finish(name)

def make_modules():
    box('Stone', (0,0,0), (1,1,1)); finish('Block')
    box('Soil', (0,0,0), (1,1,1)); finish('Earth')
    box('Path', (0,0,0), (1,1,1)); finish('Path')
    box('Roof', (0,0,0), (1,1,1)); finish('RoofSlab')
    building('RowHouse', 7, 6, 3.5)
    building('SlopeHouse', 6, 7, 4, lean=True)
    building('BurrowHouse', 7.5, 5.5, 3.25, lean=True)
    building('Tavern', 14, 10, 6.6)
    building('Smithy', 12, 9, 4.2, open_front=True, windows=False)
    building('Assay', 8, 6, 3.5, windows=False)
    building('Clinic', 6, 5, 3.3)
    building('Office', 5, 4, 3.2)
    # Rail modules fit end-to-end. Sleeper top .10, rail tread .22.
    for x in (-1.05, 1.05):
        box('Iron', (x,0,.16), (.10, 4, .12))
        box('Brass', (x,0,.225), (.16, 4, .025))
    for y in (-1.6,-.8,0,.8,1.6):
        box('Timber',(0,y,.055),(3,.25,.11))
        for x in (-1.05,1.05):
            box('Iron',(x,y,.13),(.30,.24,.07))
    finish('Rail4')
    for i in range(22):
        h=3.5*(i+1)/22
        box('Stone', (0, (i+.5)*.38, h/2), (4, .38, h))
    finish('Stairs35')
    # 10m communal hearth, rim doubles as seating, open to the sky.
    cylinder('Dark', (0,0,.15), 4.65, .30)
    for i in range(36):
        a=i*math.tau/36
        for z in (.18,.54):
            box('StoneLight' if i%4 else 'Stone', (4.68*math.cos(a),4.68*math.sin(a),z),(.76,.64,.34),(0,math.degrees(a)+90,0))
    for i in range(15):
        a=i*2.4
        x,y=2.1*math.cos(a)*(i%5)/4,2.1*math.sin(a)*(i%5)/4
        cylinder('Timber',(x,y,.48),.19,2.5,(90,i*31,0))
        sphere('Ember',(x,y,.45),(.65,.50,.22))
    finish('Hearth')
    for i in range(13):
        a=i*2.4
        rr=(i%4)*.42
        sphere('Flame' if i%3 else 'Ember',(rr*math.cos(a),rr*math.sin(a),.8+(i%3)*.32),(.40,.36,1+(i%4)*.22))
    finish('Fire')
    for y in (-.34,0,.34):
        box('Timber',(0,y,.85),(5,.31,.16))
    for x in (-1.8,1.8):
        box('Iron',(x,0,.44),(.14,.65,.82))
    for y in (-1.15,1.15):
        box('Timber',(0,y,.46),(5,.45,.14))
        for x in (-1.8,1.8):
            box('Stone',(x,y,.21),(.3,.35,.42))
    for x in (-1.3,-.5,.5,1.3):
        cylinder('Brass',(x,.18,.98),.10,.17)
    finish('Table')
    box('Stone',(0,0,.6),(2.4,1.7,1.2))
    box('Dark',(0,-.5,1.3),(1.9,1.0,.40))
    sphere('Flame',(0,-.38,1.35),(.9,.48,.45))
    box('Stone',(0,.50,2.5),(2.3,.65,4.5))
    box('Iron',(1.8,0,.75),(1.5,1.0,.5))
    cylinder('Brass',(2.7,.4,.60),.45,1.2,(90,0,0))
    finish('Forge')
    box('Stone',(0,0,.45),(1.0,.8,.9));box('Iron',(0,0,1.05),(1.7,.6,.35));box('Iron',(.6,0,1.22),(1.0,.45,.10))
    finish('Anvil')
    box('Iron',(0,0,.50),(2.5,3.4,.3))
    for x in (-1.15,1.15):
        box('Iron',(x,0,1.2),(.18,3.4,1.4))
        for y in (-1.1,1.1):
            cylinder('Iron',(x,y,.33),.38,.22,(0,90,90))
            torus('Brass',(x,y,.33),.30,.035,(0,90,90))
    for y in (-1.60,1.60):
        box('Iron',(0,y,1.2),(2.4,.18,1.4))
    for i in range(8):
        sphere('Rock',(R.uniform(-.8,.8),R.uniform(-1.15,1.15),R.uniform(.8,1.2)),(.50,.65,.45))
    finish('Minecart')
    # 6 x 4m mine arch, timber jambs, no wall across the opening.
    for x in (-3.35,3.35):
        box('Stone',(x,0,1.1),(.9,1.2,2.2));box('Timber',(x*.89,-.3,1.5),(.3,.35,3))
    for i in range(15):
        a=(i+.5)*math.pi/15
        x=3.3*math.cos(a);z=1.4+2.8*math.sin(a)
        box('StoneLight',(x,0,z),(.78,1.2,.66),(0,0,math.degrees(a)-90))
    box('Timber',(0,-.28,3.80),(6.2,.4,.4))
    finish('MinePortal')
    for x in (-2.8,0,2.8):box('Timber',(x,0,1.4),(.20,.25,2.8))
    for z in (.9,1.9):box('Timber',(0,-.1,z),(6.1,.18,.32))
    beam('Timber',(-2.8,-.24,.3),(2.8,-.24,2.4),.22)
    finish('Barricade')
    for x in (-1.4,1.4): box('Timber',(x,0,1.5),(.24,.28,3))
    box('Timber',(0,0,2),(3.2,.18,1.6))
    for i in range(7): box('Paper',(-1.10+(i%4)*.7,-.105,2.42-(i//4)*.64),(.53,.035,.51),(0,0,(-1)**i*3))
    finish('Notice')
    beam('Timber',(0,0,0),(0,0,2.4),.085)
    beam('Iron',(-.58,0,2.18),(.58,0,2.4),.16)
    torus('Brass',(.1,0,1.48),.15,.04,(90,0,0));finish('ToolSign')
    for x in (-2,2): box('Iron',(x,0,2.7),(.18,.18,5.4))
    beam('Iron',(-2,0,5.4),(2,0,5.4),.24)
    cylinder('Brass',(0,0,4.4),.55,.8)
    sphere('Brass',(0,0,4.7),(.52,.52,.36))
    beam('Iron',(0,0,4.4),(0,0,1.0),.045);finish('ShiftBell')
    for x in (-4,4):box('Timber',(x,0,1.4),(.16,.16,2.8))
    for i in range(12):
        x=-4+i*8/12
        beam('Iron',(x,0,2.8-.5*(1-(x/4)**2)),(x+8/12,0,2.8-.5*(1-((x+8/12)/4)**2)),.025)
    for i in range(5):box('Cloth' if i%2 else 'Paper',(-2.8+i*1.3,0,1.85),(.85,.04,1.35),(0,0,i%3*2))
    finish('Laundry')
    for i in range(8):
        a=i*2.4
        sphere('Rock',(.54*math.cos(a),.85*math.sin(a),.20),(.52,.53,.29))
    box('Rock',(0,.80,.45),(.58,.22,.85),(8,4,0))
    beam('Iron',(-.26,-.35,.34),(.26,-.24,.36),.10);finish('Grave')
    # Low-poly grey Naia stand-ins have no combat/capture components.
    sphere('Rock',(0,0,.40),(.6,.5,.46))
    sphere('StoneLight',(0,-.24,.67),(.35,.30,.30))
    for x in (-.14,.14):sphere('NaiaGlow',(x,-.515,.72),(.055,.03,.06))
    finish('StoneNaia')
    for i in range(7):
        sphere('Ash',(math.sin(i*2.4)*.27,math.cos(i*2.4)*.22,.25+i*.035),(.22,.20,.17))
    for x in (-.12,.12):sphere('NaiaGlow',(x,-.3,.44),(.035,.03,.035))
    finish('AshNaia')
    cylinder('Timber',(0,0,2),.28,4)
    for i in range(5):
        # Stacked conical silhouettes using tapered faceted spheres.
        z=3+i*1.15; r=2.2-i*.33
        sphere('Pine',(0,0,z),(r,r,1.45))
    finish('Pine')
    for x in (-2,2):box('Timber',(x,0,2.5),(.32,.36,5))
    box('Iron',(0,0,5),(4.8,.30,.35))
    for z in (.8,1.2,1.6,2,2.4,2.8,3.2,3.6,4,4.4):torus('Iron',(0,0,z),.20,.04,(90 if int(z*10)%8 else 0,0,0))
    torus('Brass',(0,0,4.5),.55,.10,(90,0,0))
    box('Iron',(0,0,.4),(3,.9,.18));finish('Pulley')
    for i in range(35):
        torus('Iron',(R.uniform(-3,3),R.uniform(-3,3),R.uniform(.2,1.8)),.32,.06,(R.uniform(0,90),R.uniform(0,180),0))
    finish('RingPile')
    for i in range(70):
        a=i*2.4;z=.3+i*.075;rad=2*math.sin(math.pi*(i+1)/72)
        torus('Iron',(rad*math.cos(a),rad*math.sin(a),z),.35,.055,(70,math.degrees(a),0))
    sphere('Vein',(0,0,2.8),(1.2,1.2,2.4));finish('Hollowcore')
    for i in range(22):sphere('Vein',(math.cos(i*2.4)*(1+i*.15),math.sin(i*2.4)*(1+i*.15),1+i*.25),(.08,.08,.15))
    finish('FreedLights')
    for z in (.3,1.2,2.1):box('Timber',(0,0,z),(4,.65,.15))
    for x in (-1.9,1.9):box('Iron',(x,0,1.2),(.12,.65,2.4))
    for i in range(20):box('Paper' if i%3 else 'Cloth',(-1.75+(i%10)*.38,0,.65+(i//10)*.9),(.27,.45,.50),(0,0,(i%3-1)*7))
    finish('LedgerShelf')
    box('Timber',(0,0,.9),(2.5,1.2,.2))
    for x in (-1,1): box('Iron',(x,0,.45),(.10,.8,.9))
    beam('Brass',(0,0,1),(0,0,1.8),.06)
    beam('Brass',(-.7,0,1.7),(.7,0,1.7),.06)
    for x in (-.65,.65): cylinder('Brass',(x,0,1.3),.25,.06);beam('Iron',(x,0,1.3),(x,0,1.7),.025)
    box('Paper',(.8,-.25,1.03),(.6,.45,.04));finish('AssayDesk')
    box('Timber',(0,0,.45),(1.3,2.2,.6));box('Cloth',(0,0,.8),(1.22,2.12,.20));box('Paper',(0,.72,.94),(.9,.50,.18));finish('Bed')
    box('Timber',(0,0,0),(2.2,2.6,.16))
    for x in (-.9,.9):box('Iron',(x,0,.10),(.10,2.4,.05))
    torus('Iron',(.65,-.7,.13),.12,.03);finish('Hatch')
    box('Paper',(0,0,0),(.45,.035,.32));cylinder('Seal',(0,-.03,0),.065,.025,(90,0,0));finish('Letter')

def ground_height(x,y):
    if y < -30: z=0
    elif y < 30: z=3.5
    else: z=7
    if 8.5 <= x <= 18.5:
        if -40 <= y <= -20: z=3.5*(y+40)/20
        if 20 <= y <= 40: z=3.5+3.5*(y-20)/20
    return z

def make_town():
    # Three terraces, central ramp cutouts, no hidden slab across the ramp.
    for band,(y0,y1,z) in enumerate([(-90,-30,0),(-30,30,3.5),(30,90,7)]):
        for x0,x1 in [(-75,8.5),(18.5,75)]:
            # Assay underground is under an intentionally hollow plinth.
            if band==1 and x0==18.5:
                for a,b,c,d in [(x0,29.9,y0,y1),(32.1,x1,y0,y1),(29.9,32.1,y0,5.2),(29.9,32.1,7.8,y1)]:
                    box('Soil',((a+b)/2,(c+d)/2,z-.4),(b-a,d-c,.8))
            else:
                box('Soil',((x0+x1)/2,(y0+y1)/2,z-.4),(x1-x0,y1-y0,.8))
        finish('Terrace'+str(band))
        place('Terrace'+str(band),'Terrace'+str(band),(0,0,0),zone='Terrain')
    # Central uninterrupted incline is 10m wide; 3m rails + 2m footpath.
    for y0,y1,z0,z1 in [(-90,-40,0,0),(-40,-20,0,3.5),(-20,20,3.5,3.5),(20,40,3.5,7),(40,96,7,7)]:
        beam('Path',(13.5,y0,z0-.14),(13.5,y1,z1-.14),10,.28)
    finish('Railbed');place('Railbed','Railbed',(0,0,0),zone='Terrain')
    for y in (-30,30):
        z=0 if y<0 else 3.5
        for x0,x1 in [(-75,-22),(-14,8.5),(18.5,40),(48,75)]:
            box('Stone',((x0+x1)/2,y,z+1.7),(x1-x0,.85,3.4))
            box('StoneLight',((x0+x1)/2,y,z+3.48),(x1-x0,.98,.18))
            for x in range(math.ceil(x0+2),int(x1),5):box('StoneLight',(x,y-.30,z+1.7),(.8,1.1,3.4))
        for x in (-18,44):
            place('Stairs_'+str(y)+'_'+str(x),'Stairs35',(x,y-8.36,z),zone='Terrain')
            # Stairwell top cutout through the terrace slab avoids a last-step wall.
    finish('RetainingWalls');place('RetainingWalls','RetainingWalls',(0,0,0),zone='Terrain')
    for begin,end in [(-90,-40),(-40,-20),(-20,20),(20,40),(40,98)]:
        y0=begin
        while y0<end:
            y1=min(y0+4,end);y=(y0+y1)/2
            z0=ground_height(12,y0);z1=ground_height(12,y1)
            slope=(z1-z0)/(y1-y0)
            place('Rail_%03d'%len(placements),'Rail4',(12,y,(z0+z1)/2+.01),scale=(1,math.hypot(y1-y0,z1-z0)/4,1),zone='Rail',tags=('CH.Rail',))
            placements[-1]['roll']=-math.degrees(math.atan(slope))
            y0=y1
    # Main cluster encircles a compact 30 x 24m communal plaza.
    place('HearthSquare','Path',(-6,0,3.53),scale=(30,24,.06),zone='Hearth')
    place('CommunalHearth_10m','Hearth',(-6,0,3.59),zone='Hearth')
    place('LivingEmbers','Fire',(-6,0,3.59),zone='Hearth',tags=('CH.Fire',))
    for i,a in enumerate([20,75,140,205,270]):
        r=math.radians(a)
        place('FeastTable_%d'%i,'Table',(-6+8.6*math.cos(r),8.6*math.sin(r),3.62),a+90,zone='Hearth')
    place('GormNoticeboard','Notice',(-22,-4,3.5),-90,zone='Hearth')
    place('ShiftBell','ShiftBell',(-20,10,3.5),zone='Hearth')
    light('HearthLight',(-6,0,5.2),27,6500)
    light('HearthBounce',(-6,-5,6.5),18,1800)
    buildings=[('DorikSmithy','Smithy',(-7,20,3.5),0,'Smithy'),
               ('AssayOffice','Assay',(31,8,3.5),-90,'Assay'),
               ('FreyaTavern','Tavern',(-8,-57,0),0,'Residential'),
               ('MarenClinic','Clinic',(31,-48,0),0,'Residential'),
               ('MineManagement','Office',(24,76,7),0,'Mine')]
    for name,module,pos,yaw,zone in buildings:place(name,module,pos,yaw,zone=zone)
    place('WorkingForge','Forge',(-8,22,3.5),zone='Smithy')
    place('Anvil','Anvil',(-10,17.8,3.5),zone='Smithy')
    place('BellowsNaia','StoneNaia',(-3.5,21,3.5),zone='Smithy',tags=('CH.Naia.NonCombat','CH.Q3',))
    light('SmithyLight',(-8,20,5.5),16,2600,'Forge')
    place('AssayWorktable','AssayDesk',(31,8,3.52),zone='Assay')
    # Side entrance leads to the hatch staging room; cellar is beneath the office.
    place('AssayHatch','Hatch',(31,6.5,3.65),zone='Assay',tags=('CH.Q7.Blocker',))
    for x in (28,34):place('ClinicCot_'+str(x),'Bed',(x,-48,0),zone='Residential')
    home_positions=[(-25,-36,'RowHouse'),(-34,-40,'SlopeHouse'),(-42,-37,'BurrowHouse'),(-54,-41,'RowHouse'),
                    (35,-39,'RowHouse'),(57,-41,'BurrowHouse'),(-29,-58,'RowHouse'),(-42,-56,'SlopeHouse'),
                    (-56,-59,'BurrowHouse'),(43,-57,'SlopeHouse'),(57,-59,'RowHouse'),
                    (-22,-77,'RowHouse'),(-37,-75,'BurrowHouse'),(-52,-78,'SlopeHouse'),
                    (28,-77,'RowHouse'),(43,-75,'SlopeHouse'),(57,-78,'BurrowHouse')]
    for i,(x,y,kind) in enumerate(home_positions):
        name='GormHouse' if i==0 else 'MinersHome_%02d'%i
        place(name,kind,(x,y,0),zone='Residential',tags=('CH.GormHouse',) if i==0 else ())
        place(name+'_ToolSign','ToolSign',(x+1.75,y-(3 if kind!='SlopeHouse' else 3.5)-.2,.15),zone='Residential',scale=(1.18,1.18,1.18) if i==0 else (1,1,1))
    for i,(x,y) in enumerate([(-35,-47),(-47,-67),(40,-66),(50,-49)]):
        place('Laundry_%d'%i,'Laundry',(x,y,0),zone='Residential')
        place('DustNaia_%d'%i,'AshNaia',(x+1,y,1.3),zone='Residential',tags=('CH.Naia.NonCombat','CH.Dust',))
    for i,(x,y) in enumerate([(-12,4),(-13,3)]):place('WarmingAshNaia_%d'%i,'AshNaia',(x,y,3.62),zone='Hearth',tags=('CH.Naia.NonCombat',))
    place('MineEntrance_6x4','MinePortal',(12,89,7),zone='Mine')
    place('DeepveinBarricade','Barricade',(12,98,7),zone='Mine',tags=('CH.Q8.Blocker',))
    mark('RockfallWarning',(12,96,9),'Mine',('CH.Q8','낙반 위험 구역'))
    place('IncomingCart','Minecart',(12,67,7.23),zone='Mine',tags=('CH.Q2.Cart',))
    place('OutgoingCart','Minecart',(12,-70,.23),zone='Rail')
    place('Weighbridge','AssayDesk',(20,55,7),zone='Mine')
    place('UnloadingShelter','Smithy',(-2,62,7),zone='Mine')
    for i in range(18):
        place('OrePile_%02d'%i,'@SM_Rock_A',(-5+(i%5)*2.5,48+(i//5)*2.7,7),R.uniform(0,360),scale=(1.2,1.2,1.2),zone='Mine')
    place('RescuedStoneNaia','StoneNaia',(16.5,59,7),zone='Mine',tags=('CH.Naia.NonCombat','CH.Q2.Rescue',))
    # Work and rest routes remain narrow; no east-west main boulevard.
    for name,x,y,w,d in [('SmithyApron',-7,14,16,7),('AssayWalk',24,4,14,3),('TavernYard',-8,-65,18,7),('GormSteps',-18,-32,4,8)]:
        place(name,'Path',(x,y,ground_height(x,y)+.055),scale=(w,d,.10),zone='Paths')
    for i in range(32):
        x=R.choice([-1,1])*R.uniform(25,66);y=R.uniform(-80,76)
        place('Crate_%02d'%i,'@SM_Crate',(x,y,ground_height(x,y)),R.choice([0,10,90]),zone='Dressing')
    for i in range(14):place('TavernCask_%02d'%i,'@SM_Barrel',(-17+(i%3),-55+(i//3),0),zone='Dressing')
    for i in range(35):
        x=R.uniform(-19,7);y=R.uniform(-11,11)
        if math.hypot(x+6,y)>5.4:box('StoneLight',(x,y,3.66),(R.uniform(.35,.9),R.uniform(.35,.8),.05),(0,R.uniform(0,35),0))
    finish('PlazaPavers');place('PlazaPavers','PlazaPavers',(0,0,0),zone='Hearth')
    # Greyvein mountain rim leaves a southern arrival vista and western field gap.
    for i in range(34):
        x=-88+i*5.5
        if abs(x-12)<7:continue
        place('NorthCliff_%d'%i,'@SM_Cliff_Block',(x,94+R.uniform(-2,5),5),R.uniform(-10,10),scale=(2.4,2.8,R.uniform(3.4,6.5)),zone='Greyvein')
    for side in (-1,1):
        for i in range(22):
            y=-86+i*8
            if side<0 and -4<y<26:continue
            place('SideCliff_%d_%d'%(side,i),'@SM_Cliff_Block',(side*82,y,ground_height(70,y)-2),R.uniform(-15,15)+90,scale=(2.2,2.3,R.uniform(1.9,4.2)),zone='Greyvein')
    # Story anchors are editor-only target points, never floating quest labels.
    for name,pos,zone in [('Q1_Arrival',(12,-86,0),'Arrival'),('Q2_Unloading',(18,58,7),'Mine'),
                           ('Q3_Dorik',(-9,17,3.5),'Smithy'),('Q6_Maren',(31,-50,0),'Residential'),
                           ('Q7_Archive',(31,8,-.1),'Assay'),('Q8_DeepveinGate',(12,97,7),'Mine'),
                           ('Q9_Assembly',(-6,-8,3.5),'Hearth'),('Q12_GormDoor',(-25,-39.1,0),'Residential')]:mark(name,pos,zone)
    mark('Epilogue_01_Plaza',(-6,-16,8),'Cinematics')
    mark('Epilogue_02_Stairs',(-18,-25,5),'Cinematics')
    mark('Epilogue_03_Door',(-25,-43,1.7),'Cinematics')
    mark('Epilogue_04_Seal',(-23.6,-39.5,1.3),'Cinematics',('안식원 수석 설계관 도렌',))

def make_surroundings():
    # Ashslope: five worked benches, broad connecting switchbacks.
    for i in range(5):
        x=-94-i*25;z=3.5+i*2.5
        place('AshslopeBench_%d'%i,'Earth',(x,12,z-.5),scale=(25,50,1),zone='Ashslope')
        for j in range(5):place('Ashslope_Rock_%d_%d'%(i,j),'@SM_Rock_A',(x+R.uniform(-8,8),R.uniform(-7,32),z),R.uniform(0,360),scale=(1.7,1.5,1.3),zone='Ashslope')
        place('StoneNaiaColony_%d'%i,'StoneNaia',(x,12,z),zone='Ashslope',tags=('CH.Spawn.StoneNaia',))
        if i<4:
            beam('Path',(x-10,(-8 if i%2 else 30),z),(x-20,(-8 if i%2 else 30),z+2.5),6,.30)
    finish('AshslopeRamps');place('AshslopeRamps','AshslopeRamps',(0,0,0),zone='Ashslope')
    place('AshslopeConnection','Path',(-79,12,3.35),scale=(16,8,.30),zone='Ashslope')
    place('AbandonedCart','Minecart',(-116,4,6),32,zone='Ashslope')
    mark('Q4_VeinSentinel',(-194,12,13.5),'Ashslope',('CH.Spawn.MiniBoss',))
    place('VeinSentinelProxy','StoneNaia',(-194,12,13.5),scale=(3,3,3),zone='Ashslope')
    # Pinehollow is a separate 100m clearing below the arrival route.
    place('PinehollowGround','Earth',(5,-154,-.7),scale=(100,100,1.4),zone='Pinehollow')
    place('ArrivalRoad','Path',(12,-144,.025),scale=(5,108,.05),zone='Pinehollow')
    for i in range(75):
        x=R.uniform(-43,54);y=R.uniform(-196,-107)
        if abs(x-12)<6 or abs(x+20)<13 and abs(y+145)<12:continue
        place('Pine_%02d'%i,'Pine',(x,y,0),R.uniform(0,360),scale=(1,1,R.uniform(.8,1.35)),zone='Pinehollow')
    place('AbandonedLoggingGear','Pulley',(-20,-145,0),zone='Pinehollow',tags=('CH.Q5',))
    place('BoundCogNaia','StoneNaia',(-20,-145,.6),zone='Pinehollow',tags=('CH.Q5.BoundNaia',))
    place('BearDen','MinePortal',(42,-181,0),90,zone='Pinehollow')
    place('ArcadiaCrate','@SM_Crate',(47,-181,0),zone='Pinehollow',tags=('CH.Q5_2.Clue','Arcadia.Seal',))
    mark('Q5_CogNaia',(-20,-145,0),'Pinehollow')
    mark('Q5_2_OldBear',(43,-181,0),'Pinehollow',('CH.Spawn.OldBear',))
    # Cemetery is on the NE back slope: no Naia colony, no shrine.
    place('CemeteryHill','Earth',(115,70,13.6),scale=(45,40,.8),zone='Cemetery')
    beam('Path',(66,48,7),(94,64,14),4,.35)
    finish('CemeteryTrail');place('CemeteryTrail','CemeteryTrail',(0,0,0),zone='Cemetery')
    for i in range(30):
        x=99+(i%6)*5.7+R.uniform(-.8,.8);y=56+(i//6)*6.1+R.uniform(-.8,.8)
        place('TendedGrave_%d'%i if i<2 else 'UnnamedGrave_%02d'%i,'Grave',(x,y,14),R.uniform(-18,18),zone='Cemetery',tags=('CH.Grave.Tended',) if i<2 else ())
    mark('Cemetery_VillageVista',(107,53,15.8),'Cemetery')
    # Cellar lives directly below the assay office; opening is cut at build time.
    place('ArchiveFloor','Block',(31,8,-.7),scale=(7,5,.35),zone='Archive')
    for x in (27.5,34.5):place('ArchiveWall_'+str(x),'Block',(x,8,1.15),scale=(.35,5,3.35),zone='Archive')
    for y in (5.5,10.5):place('ArchiveEnd_'+str(y),'Block',(31,y,1.15),scale=(7,.35,3.35),zone='Archive')
    for y in (6.0,10):place('LedgerShelf_'+str(y),'LedgerShelf',(31,y,-.52),zone='Archive')
    place('UniqueQ7Documents','AssayDesk',(32,8,-.52),zone='Archive',tags=('CH.Q7.Documents','Shadowvale.Export','Doren.DoubleRing','정류 계획 자재 할당분'))
    # Route extension beyond the mine: manufactured -> stockpile -> organic.
    for y0,y1,z0,z1 in [(89,106,7,7),(106,128,7,3),(128,152,3,3),(152,178,3,-1),(178,193,-1,-1)]:
        beam('Path',(12,y0,z0-.2),(12,y1,z1-.2),7,.4)
        for side in (-1,1):beam('Rock',(12+side*4,y0,z0+2.2),(12+side*4,y1,z1+2.2),1.4,5)
        # Roof left open for the HD-2D camera; wall height conveys enclosed tunnel.
    finish('DeepveinTunnel');place('DeepveinTunnel','DeepveinTunnel',(0,0,0),zone='Deepvein')
    for i,y in enumerate(range(102,149,7)):place('MineTimber_%d'%i,'MinePortal',(12,y,7 if y<=106 else max(3,7-(y-106)*4/22)),zone='Deepvein')
    place('RingDepotFloor','Path',(12,139,2.8),scale=(19,20,.4),zone='Deepvein')
    for i,(x,y) in enumerate([(5,133),(19,133),(5,143),(19,143)]):place('RestRingStockpile_%d'%i,'RingPile',(x,y,3),zone='Deepvein',tags=('CH.Q10.ScaleOnly',))
    for i in range(12):
        side=(-1)**i;y=155+i*2.7
        beam('Vein',(12+side*3.4,y,1),(12+side*3.2,y+1,3),.10)
    finish('OrganicVeins');place('OrganicVeins','OrganicVeins',(0,0,0),zone='Deepvein')
    cylinder('Rock',(12,207,-1.3),12.5,.6)
    for i in range(38):
        a=i*math.tau/38
        if math.sin(a)<-.94:continue
        sphere('Rock',(12+12.6*math.cos(a),207+12.6*math.sin(a),2),(1.6,1.6,3.3))
        sphere('Vein',(12+11.6*math.cos(a),207+11.6*math.sin(a),1.5),(.14,.16,1.0))
    finish('DeepveinChamber');place('Chamber_25m','DeepveinChamber',(0,0,0),zone='Deepvein')
    place('HollowcoreCocoon','Hollowcore',(12,207,-1),zone='Deepvein',tags=('CH.Q11.Cocoon',))
    place('HollowcoreFreed','FreedLights',(12,207,-1),zone='Deepvein',tags=('CH.Q11.Freed',))
    light('HollowcorePulse',(12,207,2),18,1700,'Vein',(.14,.44,.60))
    mark('Q10_RingDepot',(12,139,3),'Deepvein',('NoDuplicateDocuments',))
    mark('Q11_Hollowcore',(12,207,-1),'Deepvein')

def save():
    data=dict(version=1, world_scale=.45, design_extent_m=[150,180], north='+Y',
              terrace_heights_m=[0,3.5,7], modules=modules, placements=placements, lights=lights, markers=markers,
              notes=['Physical centimetres = design metres * 45.',
                     'Cemetery trail is compressed for the playable prototype; five-minute narrative journey remains a design intent.',
                     'Only Q7 archive contains the two documentary revelations.',
                     'Naia and NPC art is provisional; no combat or capture behaviour on village Naia.'])
    (ROOT/'layout.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    print('modules',len(modules),'mesh parts',sum(len(m) for m in modules.values()),'placements',len(placements),'markers',len(markers))
    # Hand-off plan remains vector based and inspectable in any browser.
    labels=[('MINE / Q8',12,89),('UNLOADING / Q2',0,60),('SMITHY / Q3',-7,20),('HEARTH / Q9',-6,0),('ASSAY / Q7',31,8),('TAVERN',-8,-57),('CLINIC / Q6',31,-48),('GORM / Q12',-25,-36)]
    def xy(x,y):return 500+x*4,470-y*4
    svg=['<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="980" viewBox="0 0 1000 980">',
         '<rect width="1000" height="980" fill="#191c20"/>',
         '<style>text{font-family:Segoe UI,sans-serif;fill:#d5d4cc} .small{font-size:12px;letter-spacing:1px}</style>',
         '<text x="45" y="45" font-size="29" letter-spacing="4">CINDEHEARTH</text>',
         '<text x="45" y="72" class="small">NAIA UNBOUND / 150 x 180 m / THREE TERRACES / NORTH UP</text>']
    for y0,y1,z,c in [(-90,-30,0,'#4a463f'),(-30,30,3.5,'#59564e'),(30,90,7,'#626362')]:
        x,y=xy(-75,y1);svg.append(f'<rect x="{x}" y="{y}" width="600" height="240" fill="{c}"/>')
        svg.append(f'<text x="{x+8}" y="{y+21}" class="small">+{z:.1f} m</text>')
    for p in placements:
        if p['module'] in ['RowHouse','SlopeHouse','BurrowHouse','Tavern','Smithy','Assay','Clinic','Office']:
            x,y=xy(*p['pos'][:2]);svg.append(f'<rect x="{x-14}" y="{y-12}" width="28" height="24" fill="#2b3034" stroke="#92918a"/>')
    for x in (10.95,13.05):
        xx,yy=xy(x,91);svg.append(f'<path d="M{xx},{yy} V{xy(x,-90)[1]}" stroke="#b29c6b" stroke-width="2"/>')
    for y in range(-90,92,4):
        x,yy=xy(10.5,y);svg.append(f'<path d="M{x},{yy} h12" stroke="#25282a" stroke-width="3"/>')
    hx,hy=xy(-6,0);svg.append(f'<circle cx="{hx}" cy="{hy}" r="20" fill="#e68443"/><circle cx="{hx}" cy="{hy}" r="43" fill="none" stroke="#c98c53"/>')
    for s,x,y in labels:
        xx,yy=xy(x,y);svg.append(f'<text x="{xx+19}" y="{yy-18}" font-size="12">{s}</text>')
    svg += ['<text x="34" y="416" class="small">WEST: ASHSLOPE</text>','<text x="785" y="204" class="small">NE: CEMETERY</text>','<text x="380" y="881" class="small">SOUTH: PINEHOLLOW / ARRIVAL</text>','<text x="45" y="936" class="small">STONE + BRASS / ASH / A HEARTH THAT NEVER GOES OUT</text>','</svg>']
    (ROOT/'plan.svg').write_text('\n'.join(svg),encoding='utf-8')

if __name__=='__main__':
    make_modules();make_town();make_surroundings();save()
