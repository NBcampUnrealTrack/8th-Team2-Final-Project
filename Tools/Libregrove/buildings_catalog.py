"""V6 design dimensions and bounded primitive requests; NOT Blender execution."""
import math

CATALOG=[
 {'id':'02_Inn','name':'물가 여관','width':4.50,'depth':3.15,'height':3.60,'eaves':2.475,'ridge':'Y','doors':1,'door_width':.70,'door_height':1.10,'windows':[(s*1.25,.78,.56,.52) for s in [-1,1]]+[(s*1.25,1.94,.56,.60) for s in [-1,0,1]],'awning':True,'budget_v':450,'budget_t':700},
 {'id':'03_VillageHall','name':'마을 회관','width':4.50,'depth':3.15,'height':3.15,'eaves':1.80,'ridge':'X','doors':2,'door_width':1.20,'door_height':1.26,'windows':[(s*1.43,.87,.59,.66) for s in [-1,1]],'budget_v':350,'budget_t':550},
 {'id':'04_Workshop','name':'공용 작업소','width':3.15,'depth':2.70,'height':2.52,'eaves':1.35,'ridge':'Y','doors':2,'door_width':1.05,'door_height':1.10,'windows':[(s*1.02,.79,.42,.42) for s in [-1,1]],'budget_v':350,'budget_t':550},
 {'id':'05_WatermillHouse','name':'물레방앗간 본체','width':3.15,'depth':2.70,'height':2.79,'eaves':1.53,'ridge':'Y','doors':1,'door_width':.70,'door_height':1.14,'windows':[(s*.98,.87,.46,.52) for s in [-1,1]],'budget_v':350,'budget_t':550},
 {'id':'09_MarketStall','name':'광장 공용 노점','width':1.80,'depth':1.35,'height':1.71,'doors':0,'budget_v':120,'budget_t':200},
]

def parts_for(spec):
    parts=[];groups=[];prefix='LGV6_'+spec['id']
    def cube(name,location,scale,rotation=(0,0,0),role='body'):
        parts.append({'object_name':prefix+'_'+name,'primitive':{'type':'cube','size':1,'calc_uvs':True},'location':list(location),'scale':list(scale),'rotation_degrees':list(rotation),'role':role})
        return parts[-1]['object_name']
    W,D,H=spec['width'],spec['depth'],spec['height']
    if spec['doors']==0:
        thick=.035;theta=math.atan(.36/D);length=(D-math.sin(theta)*thick)/math.cos(theta)
        cz=H-math.sin(theta)*length/2-math.cos(theta)*thick/2
        cube('ShedRoof',(0,0,cz),(length,W,thick),(0,math.degrees(theta),0))
        for x in [-.56,.56]:
            top=cz-math.tan(theta)*x-thick/(2*math.cos(theta))
            for y in [-.76,.76]:cube(f'Post_{len(parts)}',(x,y,top/2),(.07,.07,top))
        cube('CounterTop',(.40,0,.68),(.42,1.52,.05))
        cube('FrontPanel',(.565,0,.335),(.05,1.45,.67))
        for y in [-.735,.735]:cube('SidePanel_'+str(len(parts)),(.025,y,.315),(1.03,.05,.63))
        cube('RearLowPanel',(-.555,0,.315),(.04,1.45,.63))
        return parts,groups
    wallW,wallD=W-.30,D-.30;front=wallD/2;bottom=.08;eave=spec['eaves'];wallH=eave-bottom
    opening=spec['door_width'];top=bottom+spec['door_height'];frame=.055
    cube('Plinth',(0,0,.04),(wallD+.06,wallW+.06,.08))
    sidewidth=(wallW-opening)/2
    for side,sign in [('L',-1),('R',1)]:
        cube('FrontWall_'+side,(front-.06,sign*(opening/2+sidewidth/2),bottom+wallH/2),(.12,sidewidth,wallH))
        cube('SideWall_'+side,(0,sign*(wallW/2-.06),bottom+wallH/2),(wallD-.24,.12,wallH))
    cube('BackWall',(-front+.06,0,bottom+wallH/2),(.12,wallW,wallH))
    cube('HeaderWall',(front-.06,0,(eave+top)/2),(.12,opening,eave-top))
    ridgeheight=H-.02;rise=ridgeheight-eave
    if spec['ridge']=='X':
        cross,along=wallW,wallD;rotation=(90,0,90);roofspan=W;roofalong=D
    else:
        cross,along=wallD,wallW;rotation=(90,0,0);roofspan=D;roofalong=W
    parts.append({'object_name':prefix+'_Gable','primitive':{'type':'cylinder','radius':1,'depth':2,'vertices':3,'cap_fill':'NGON','calc_uvs':True},'scale':[cross/math.sqrt(3),rise/1.5,along/2],'location':[0,0,eave+rise/3],'rotation_degrees':rotation,'role':'body'})
    theta=math.atan(rise/(cross/2));thick=.04
    center=(roofspan/2-math.sin(theta)*thick/2)/2;length=2*center/math.cos(theta)
    cz=H-math.sin(theta)*length/2-math.cos(theta)*thick/2
    for sign in [-1,1]:
        if spec['ridge']=='X':cube('Roof_'+('A' if sign<0 else 'B'),(0,sign*center,cz),(roofalong,length,thick),(-sign*math.degrees(theta),0,0))
        else:cube('Roof_'+('A' if sign<0 else 'B'),(sign*center,0,cz),(length,roofalong,thick),(0,sign*math.degrees(theta),0))
    cube('RidgeCap',(0,0,H-.028),(D,.056,.056) if spec['ridge']=='X' else (.056,W,.056))
    for side,sign in [('L',-1),('R',1)]:cube('DoorFrame_'+side,(front+.005,sign*(opening/2+frame/2),(bottom+top)/2),(.09,frame,top-bottom))
    cube('DoorFrame_Top',(front+.005,0,top+frame/2),(.09,opening+2*frame,frame))
    for i,(cy,cz,ww,hh) in enumerate(spec['windows']):
        t=.045;left=cy-ww/2;right=cy+ww/2;low=cz-hh/2;high=cz+hh/2
        cube(f'Window{i}_Pane',(front+.009,cy,cz),(.018,ww-2*t,hh-2*t))
        for tag,y in [('L',left+t/2),('R',right-t/2)]:cube(f'Window{i}_{tag}',(front+.026,y,cz),(.062,t,hh-2*t))
        cube(f'Window{i}_Top',(front+.026,cy,high-t/2),(.062,ww,t))
        cube(f'Window{i}_Sill',(front+.045,cy,low+t/2),(.10,ww+.035,t))
        cube(f'Window{i}_Mullion',(front+.030,cy,cz),(.060,.025,hh-2*t))
    if spec.get('awning'):
        cube('DoorAwning',(front+.008,0,top+.18),(.28,opening+.30,.035),(0,8,0))
    leafBottom=.10;leafTop=top-.025;leafH=leafTop-leafBottom;hingeX=front+.065
    if spec['doors']==1:
        hingeY=-opening/2+.020;leafW=opening-.040;centers=[('Door',hingeY,1,leafW)]
    else:
        leafW=opening/2-.030;centers=[('DoorL',-opening/2+.020,1,leafW),('DoorR',opening/2-.020,-1,leafW)]
    for tag,hy,direction,leafW in centers:
        names=[]
        names.append(cube(tag+'_Leaf',(hingeX-.02,hy+direction*leafW/2,(leafBottom+leafTop)/2),(.04,leafW,leafH),role=tag))
        latch=hy+direction*(leafW-.07)
        for face,dx in [('Front',.015),('Back',-.055)]:names.append(cube(tag+'_Handle_'+face,(hingeX+dx,latch,leafBottom+leafH*.52),(.03,.04,.025),role=tag))
        groups.append({'tag':tag,'parts':names,'pivot_world_m':[hingeX,hy,leafBottom],'side':'left' if direction==1 else 'right','open_degrees':-100 if direction==1 else 100,'doorway_y_min':-opening/2,'doorway_y_max':opening/2,'doorway_z_min':bottom,'doorway_z_max':top,'wall_front_x':front})
    return parts,groups
