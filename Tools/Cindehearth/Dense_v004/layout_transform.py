"""Deterministic v004 transforms from the saved v003 actor baseline (UE cm)."""
import math,re,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent
FX,FY=.74,.67
def affine(p):return [140.4+FX*p[0],FY*p[1],p[2]]
ROWS=json.loads((ROOT/'verification/level_before.json').read_text(encoding='utf-8'))
BASE={r['label']:r for r in ROWS}
GROUPS={}
def group(key,old,new=None,factor=1):GROUPS[key]=(old,new or affine(old),factor)
group('Hearth',[-270,0,157.5])
group('Smithy',[-315,900,157.5],[-92.7,715,157.5])
group('Tavern',[-360,-2565,0])
group('Assay',[1395,360,157.5],[1172.7,263.475,157.5])
group('Clinic',[1395,-2160,0],[1172.7,-1530,0])
group('Unloading',[-90,2790,315])
group('Management',[1080,3420,315])
group('GormHouse',[-1125,-1620,0],factor=.9)
OVERRIDES={1:(-1060,-1230),2:(-1435,-1080),3:(-1860,-1260),7:(-1350,-1688.4),8:(-1790,-1780)}
for i in range(1,17):
 key=f'MinersHome_{i:02}';old=BASE['CH_'+key+'_Stone']['location'];new=affine(old)
 if i in OVERRIDES:new[:2]=OVERRIDES[i]
 group(key,old,new,.9)
for k,c in [('West',[-1700,2600,315]),('East',[2050,2400,315]),('Smithy',[-1680,440,157.5])]:group('Yard_'+k,c)

def cluster_point(key,p):
 old,new,k=GROUPS[key];return [new[i]+k*(p[i]-old[i]) for i in range(3)]

def cluster(n):
 if 'GormHouse' in n or n in ('CH_Messenger','CH_Q12_GormDoor','CH_EpilogueFill','CH_Lettering_Gorm','CH_Lettering_DorenSeal') or 'DorenLetter' in n or n.startswith(('CH_Epilogue_03','CH_Epilogue_04')):return 'GormHouse'
 m=re.search(r'MinersHome_\d\d',n)
 if m:return m.group()
 for k in ('West','East','Smithy'):
  if 'HD2D_Yard_'+k+'_' in n:return 'Yard_'+k
 if any(s in n for s in ('CommunalHearth','LivingEmbers','FeastTable','WarmingAshNaia','HearthSquare','PlazaPavers','EveningResident','HearthFire','HearthLight','HearthBounce','GormNoticeboard','ShiftBell')) or n in ('CH_Q9_Assembly','CH_Epilogue_01_Plaza','CH_Lettering_ShiftBoard'):return 'Hearth'
 if any(s in n for s in ('DorikSmithy','WorkingForge','Anvil','BellowsNaia','SmithyApron')) or n in ('CH_Dorik','CH_Q3_Dorik','CH_SmithyLight','CH_Lettering_Smithy'):return 'Smithy'
 if any(s in n for s in ('FreyaTavern','TavernYard','TavernCask')) or n in ('CH_Freya','CH_Lettering_Tavern'):return 'Tavern'
 if any(s in n for s in ('Assay','Archive','LedgerShelf','UniqueQ7')) or n=='CH_YoungAssayer':return 'Assay'
 if any(s in n for s in ('MarenClinic','ClinicCot')) or n in ('CH_Maren','CH_Q6_Maren','CH_Lettering_Clinic'):return 'Clinic'
 if 'UnloadingShelter' in n:return 'Unloading'
 if 'MineManagement' in n:return 'Management'
 return None

AFFINE_TOKENS=('Terrace','Railbed','RetainingWalls','Stairs_','StairCheek','StairCap','AshslopeRamps','CemeteryTrail','DeepveinTunnel','OrganicVeins','Chamber_25m','BasinUnderlay','BackgroundBedrock','DepotFloor','ChamberFloor','LandSkirt','MineBackfill','AshslopeBench','CemeteryHill','PinehollowGround','AshslopeNotch','TunnelWall','OrganicWall','ChamberRock','DistantRidge','NorthCliff','SideCliff','RidgeMass','WallScree','AshslopeConnection','PinehollowConnector','ArrivalRoad')
FIXED={'CH_GreyveinSky','CH_Sun','CH_SkyLight','CH_Smog','CH_PostProcess','CH_LevelDirector'}
def transform(r):
 n=r['label'];p=list(r['location']);s=list(r['scale']);rot=list(r['rotation']);k=cluster(n)
 if n in FIXED or not n.startswith('CH_'):mode='fixed'
 elif k:
  p=cluster_point(k,p);f=GROUPS[k][2]
  # Sprite residents keep their physical size. House meshes/accessories uniformly shrink.
  if r['class_name']=='StaticMeshActor':s=[v*f for v in s]
  mode='cluster:'+k
 else:
  p=affine(p);mode='position'
  if n.startswith('CH_Rail_'):
   a=math.radians(rot[0]);rot[0]=math.degrees(math.atan(math.tan(a)/FY))
   s[1]*=math.sqrt((FY*math.cos(a))**2+math.sin(a)**2)
   mode='rail-gauge-preserved'
  elif any(t in n for t in AFFINE_TOKENS):
   a=math.radians(rot[2]);s[0]*=math.hypot(FX*math.cos(a),FY*math.sin(a));s[1]*=math.hypot(FX*math.sin(a),FY*math.cos(a));mode='terrain'
  if n=='CH_Stairs_-30_-18_Stone' or 'StairCheek_0_-810_' in n or 'StairCap_0_-810_' in n or n=='CH_Epilogue_02_Stairs':p[0]+=60
 return dict(label=n,location=p,rotation=rot,scale=s,mode=mode)

def camera_route():
 old=json.loads((ROOT/'verification/director_before.json').read_text())['route'];out=[]
 for i,t in enumerate(old):
  p=t['location']
  if i==0:p=cluster_point('Hearth',p)
  elif i==1:
   anchor=BASE['CH_Epilogue_02_Stairs']['location'];target=affine(anchor);target[0]+=60;p=[target[j]+p[j]-anchor[j] for j in range(3)]
  else:p=cluster_point('GormHouse',p)
  out.append(dict(t,location=p))
 return out

if __name__=='__main__':
 import collections
 plan=[transform(r) for r in ROWS]
 (ROOT/'verification/layout_plan.json').write_text(json.dumps(plan,indent=2),encoding='utf-8')
 print(json.dumps(collections.Counter(x['mode'] for x in plan),indent=2));print('Actors',len(plan),'footprint',111,120.6)
