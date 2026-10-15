"""Guarded staged world authoring through Unreal's live Python MCP."""
import unreal,json,math,random
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Docs/Libregrove/Level_V8_20260908'
P=json.loads((OUT/'design_v001.json').read_text(encoding='utf8'))
BASE='/Game/Libregrove/Level_V8';MAT=BASE+'/Materials';LAND='LG8_Terrain'
E=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith(BASE+'/Maps/L_Libregrove.')
existing={a.get_actor_label():a for a in E.get_all_level_actors()}
STAGE=globals().get('STAGE','audit')
report={'stage':STAGE,'created':[]}
def label(a,name,folder):
    a.set_actor_label('LG8_'+name);a.set_folder_path('Libregrove/'+folder);a.tags=[unreal.Name('Libregrove.V8'),unreal.Name('LG8.'+folder.split('/')[0])]
    report['created'].append(a.get_actor_label());existing[a.get_actor_label()]=a;return a
def actor(cls,name,pos,folder='Environment',yaw=0,pitch=0):
    assert 'LG8_'+name not in existing,name
    # Map east is screen-right: fixed camera looks +Y, so design east is UE -X.
    a=E.spawn_actor_from_class(cls,unreal.Vector(-pos[0],pos[1],pos[2]),unreal.Rotator(yaw=-yaw,pitch=pitch,roll=0));assert a
    return label(a,name,folder)
def sm(name,path,pos,scale=1,yaw=0,folder='Details',material=None,collision=False,pitch=0):
    a=actor(unreal.StaticMeshActor,name,pos,folder,yaw,pitch);m=unreal.load_asset(path);assert m,path
    c=a.static_mesh_component;assert c.set_static_mesh(m)
    a.set_actor_scale3d(unreal.Vector(*(scale if isinstance(scale,(tuple,list)) else [scale]*3)))
    c.set_mobility(unreal.ComponentMobility.STATIC)
    c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    if material:c.set_material(0,unreal.load_asset(material))
    center,ex=a.get_actor_bounds(False);assert max(ex.to_tuple())>0,name
    return a
def box(name,pos,dims,material,yaw=0,folder='Structures',collision=True,pitch=0):
    return sm(name,'/Engine/BasicShapes/Cube',pos,[v/100 for v in dims],yaw,folder,material,collision,pitch)
def prop(name,kind,pos,scale=1,yaw=0,folder='Details'):
    return sm(name,'/Game/Village/Meshes/SM_'+kind,pos,scale,yaw,folder)
def child(name,parent,scalars=None,vectors=None):
    path=MAT+'/MI_LG8_'+name;assert not unreal.EditorAssetLibrary.does_asset_exist(path)
    r=unreal.MaterialService.create_instance(parent,'MI_LG8_'+name,MAT);assert r.success
    for k,v in (scalars or {}).items():assert unreal.MaterialService.set_instance_scalar_parameter(path,k,v)
    for k,v in (vectors or {}).items():assert unreal.MaterialService.set_instance_vector_parameter(path,k,*v)
    assert unreal.EditorAssetLibrary.save_asset(path);return path
def ground(x,y):
    ix=(x-P['origin'])/P['spacing'];iy=(y-P['origin'])/P['spacing'];i=max(0,min(P['N']-2,int(ix)));j=max(0,min(P['N']-2,int(iy)))
    fx=ix-i;fy=iy-j;h=P['heights'];n=P['N']
    return (h[j*n+i]*(1-fx)+h[j*n+i+1]*fx)*(1-fy)+(h[(j+1)*n+i]*(1-fx)+h[(j+1)*n+i+1]*fx)*fy
def flip_rows(values):
    return [v for j in range(P['N']) for v in values[j*P['N']:(j+1)*P['N']][::-1]]

if STAGE=='environment':
    baseline=json.loads((OUT/'new_map_template.json').read_text(encoding='utf8'))
    assert {a.get_name() for a in existing.values()}=={r['name'] for r in baseline}
    # Remove only the eight recorded newly-created Basic-template defaults.
    for a in list(existing.values()):assert E.destroy_actor(a)
    report['removed_new_template']=[b['name'] for b in baseline];existing={}
    pp=actor(unreal.PostProcessVolume,'PostProcess',[0,0,0],'Lighting');pp.set_editor_property('unbound',True)
    s=pp.get_editor_property('settings')
    for k,v in [('dynamic_global_illumination_method',unreal.DynamicGlobalIlluminationMethod.SCREEN_SPACE),('reflection_method',unreal.ReflectionMethod.SCREEN_SPACE),('auto_exposure_min_brightness',1.0),('auto_exposure_max_brightness',1.0),('auto_exposure_bias',.05),('bloom_intensity',.32),('vignette_intensity',.16),('depth_of_field_fstop',16.0),('color_saturation',unreal.Vector4(.96,.96,.96,1.0))]:
        s.set_editor_property('override_'+k,True);s.set_editor_property(k,v)
    pp.set_editor_property('settings',s)
    sun=actor(unreal.DirectionalLight,'Sun',[0,0,3000],'Lighting',yaw=-38,pitch=-43)
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);sun.light_component.set_intensity(8.3)
    sun.light_component.set_light_color(unreal.LinearColor(1,.91,.78));sun.light_component.set_editor_property('light_source_angle',1.5)
    sky=actor(unreal.SkyLight,'SkyLight',[0,0,2500],'Lighting');sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_intensity(2.8);sky.light_component.set_editor_property('real_time_capture',True)
    d=sun.get_actor_forward_vector()*-1
    skypath=child('Sky_Day','/Game/Village/Materials/MI_HD2D_Sky',vectors={'ZenithCol':(.20,.39,.60),'MidCol':(.51,.67,.78),'HorizonCol':(.88,.81,.65),'GroundCol':(.07,.10,.08),'RidgeFar':(.32,.47,.50),'RidgeNear':(.12,.24,.23),'CloudCol':(1.0,.99,.90),'SunDir':d.to_tuple(),'Tune':(.35,.48,.0008,1)})
    dome=sm('SkyDome','/Engine/BasicShapes/Sphere',[0,0,0],2000,folder='Lighting',material=skypath)
    dome.static_mesh_component.set_cast_shadow(False)
    dome.static_mesh_component.set_editor_property('affect_dynamic_indirect_lighting',True)
    fog=actor(unreal.ExponentialHeightFog,'ForestHaze',[0,0,-250],'Lighting')
    fc=fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    for k,v in [('fog_density',.014),('fog_height_falloff',.19),('start_distance',2300.0),('fog_inscattering_luminance',unreal.LinearColor(.43,.55,.56))]:fc.set_editor_property(k,v)
    child('Terrain','/Game/Village/Materials/MI_VillagePixelLandscape2',{'TileCm_Grass':180,'TileCm_Dirt':180,'TileCm_Cobblestone':180,'TileCm_Moss':210,'TileCm_Cliff':240,'TileCm_Macro':1850,'TileCm_EdgeNoise':100,'EdgeNoise_Amount':.23,'MacroVariation_Amount':.22,'CliffSlopeStart':.34,'CliffSlopeEnd':.66})
    child('River','/Game/Village/Materials/MI_HD2D_Water',{'TileCm':160,'RippleScale':.64,'RippleSpeed':.12,'ShallowDistance':140,'FoamWidth':12,'DepthSteps':6,'OpacityBase':.78,'HighlightStrength':.7,'BandStrength':.22,'RoughnessBase':.23}, {'DeepColor':(.012,.13,.18),'ShallowColor':(.05,.32,.35),'FoamColor':(.50,.73,.74),'HighlightColor':(.65,.9,.94)})
    for name,source,sc,tint in [('Stone','Stone',100,(.56,.56,.49)),('Cobble','Cobble',115,(.68,.67,.57)),('Wood','Wood',75,(.72,.57,.37)),('Timber','Timber',90,(.57,.42,.28)),('Cliff','Cliff',150,(.55,.56,.46))]:
        child(name,'/Game/Village/Materials/Props/MI_Prop_'+source,{'TileCm':sc}, {'Tint':tint})
    child('ShrineGlow','/Game/Village/Materials/Props/MI_Prop_Glow',{'EmissiveStrength':2},{'Tint':(.08,.45,.63),'Emissive':(.04,.55,1)})
    start=actor(unreal.PlayerStart,'SouthArrival',[0,-3170,90],'Gameplay',yaw=90)
    report['map']=world.get_path_name()
elif STAGE=='terrain_create':
    assert LAND not in existing
    result=unreal.LandscapeService.create_landscape(unreal.Vector(P['origin'],P['origin'],0),unreal.Rotator(),unreal.Vector(P['spacing'],P['spacing'],50),1,63,4,4,LAND)
    assert result.success,result.error_message
    land=next(a for a in E.get_all_level_actors() if a.get_actor_label()==LAND)
    land.set_folder_path('Libregrove/Terrain');land.tags=[unreal.Name('Libregrove.V8')]
    land.set_editor_property('landscape_material',unreal.load_asset(MAT+'/MI_LG8_Terrain'))
    infos=[]
    for name in P['weights']:
        result=unreal.LandscapeMaterialService.create_layer_info_object(name,BASE+'/Landscape',True);assert result.success
        assert unreal.LandscapeService.add_layer(LAND,result.asset_path)
        unreal.EditorAssetLibrary.save_asset(result.asset_path);infos.append(result.asset_path)
    report['layers']=infos
elif STAGE=='orient_map':
    assert not (OUT/'build_orient_map_001.json').exists()
    for a in E.get_all_level_actors():
        if a.get_actor_label()==LAND or str(a.get_folder_path()).startswith('Libregrove/Lighting'):continue
        p=a.get_actor_location();r=a.get_actor_rotation()
        a.set_actor_location(unreal.Vector(-p.x,p.y,p.z),False,False)
        a.set_actor_rotation(unreal.Rotator(pitch=r.pitch,yaw=-r.yaw,roll=-r.roll),False)
    assert unreal.LandscapeService.set_height_in_region(LAND,0,0,P['N'],P['N'],flip_rows(P['heights']))
    for key in P['weights']:
        assert unreal.LandscapeService.set_weights_in_region(LAND,key,0,0,P['N'],P['N'],flip_rows(P['weights'][key]))
    report['east_direction']='-UE X; north +UE Y; fixed camera yaw 90'
elif STAGE=='height':
    assert LAND in existing
    assert unreal.LandscapeService.set_height_in_region(LAND,0,0,P['N'],P['N'],P['heights'])
    report['height_requested']=True
elif STAGE=='height_verify':
    values=list(unreal.LandscapeService.get_height_in_region(LAND,0,0,P['N'],P['N']))
    assert len(values)==len(P['heights'])
    err=max(abs(a-b) for a,b in zip(values,P['heights']));assert err<.4,err
    report.update(height_range=[min(values),max(values)],max_error_cm=err)
elif STAGE.startswith('paint_'):
    key=STAGE[6:];assert key in P['weights']
    assert unreal.LandscapeService.set_weights_in_region(LAND,key,0,0,P['N'],P['N'],P['weights'][key])
    values=list(unreal.LandscapeService.get_weights_in_region(LAND,key,0,0,P['N'],P['N']))
    assert len(values)==P['N']**2 and max(values)>.7
    report.update(layer=key,range=[min(values),max(values)],nonzero=sum(v>.01 for v in values))
elif STAGE=='buildings':
    inventory=json.loads((OUT/'reuse_inventory.json').read_text(encoding='utf8'))
    for b in P['buildings']:
        row=next(i for i in inventory if '/Textured_V7/' in i['path'] and i['type']=='StaticMesh' and b['kind']+'_V7' in i['path'])
        a=sm(b['id'],row['path'],[b['x'],b['y'],b['z']],b['scale'],b['yaw'],'Buildings/'+b['zone'])
        ex=row['extent'];s=b['scale']
        if b['kind']!='MarketStall':
            # Visible footings and separate conservative blockers preserve source mesh/UV.
            box(b['id']+'_Footing',[b['x'],b['y'],b['z']-8],[ex[0]*1.70*s,ex[1]*1.65*s,20],MAT+'/MI_LG8_Stone',b['yaw'],'Foundations')
            c=box(b['id']+'_Collision',[b['x'],b['y'],b['z']+ex[2]*.78*s],[ex[0]*1.40*s,ex[1]*1.35*s,ex[2]*1.56*s],MAT+'/MI_LG8_Stone',b['yaw'],'Collision')
            c.set_actor_hidden_in_game(True);c.set_is_temporarily_hidden_in_editor(True);c.static_mesh_component.set_cast_shadow(False)
elif STAGE=='water_bridges':
    a=sm('RiverSurface','/Engine/BasicShapes/Plane',[0,0,-44],(90,90,1),folder='Water',material=MAT+'/MI_LG8_River');a.static_mesh_component.set_cast_shadow(False)
    for b in P['bridges']:
        ang=math.radians(b['yaw'])
        def wp(x,y,z):return [b['x']+x*math.cos(ang)-y*math.sin(ang),b['y']+x*math.sin(ang)+y*math.cos(ang),z]
        w=b['width'];lng=b['length'];n=round(lng/26)
        # Bridge deck boards are above the water and continuous across both banks.
        for k in range(n):
            yy=-lng/2+(k+.5)*lng/n;rise=22*max(0,1-(yy/(lng/2))**2)
            box(b['id']+'_Board_%02d'%k,wp(0,yy,47+rise),(w,lng/n-1.0,12),MAT+'/MI_LG8_Wood',b['yaw'],'Bridges/'+b['id'])
        for side in [-1,1]:
            box(b['id']+'_Stringer_'+str(side),wp(side*(w/2-15),0,24),(14,lng+20,25),MAT+'/MI_LG8_Timber',b['yaw'],'Bridges/'+b['id'])
            for k in range(6):
                yy=-lng/2+k*lng/5
                box(b['id']+'_Post_%d_%d'%(side,k),wp(side*(w/2+5),yy,88),(12,12,100),MAT+'/MI_LG8_Timber',b['yaw'],'Bridges/'+b['id'])
            for tier in [80,128]:box(b['id']+'_Rail_%d_%d'%(side,tier),wp(side*(w/2+5),0,tier),(8,lng,8),MAT+'/MI_LG8_Wood',b['yaw'],'Bridges/'+b['id'])
        for end in [-1,1]:
            box(b['id']+'_Abutment_'+str(end),wp(0,end*(lng/2-20),-12),(w+55,70,120),MAT+'/MI_LG8_Stone',b['yaw'],'Bridges/'+b['id'])
elif STAGE=='landmarks':
    # Central fountain shares the source water/bowl transform exactly.
    prop('Fountain','Fountain',[0,675,33],2.15,folder='Square')
    prop('FountainWater','Fountain_Water',[0,675,33],2.15,folder='Square').static_mesh_component.set_cast_shadow(False)
    for i in range(48):
        t=i*math.tau/48;x=390*math.cos(t);y=675+390*math.sin(t)
        box('SquareBorder_%02d'%i,[x,y,34],[14,52,12],MAT+'/MI_LG8_Stone',math.degrees(t),'Square',False)
    for i,(x,y,yaw) in enumerate([(-280,600,90),(280,900,90),(-220,960,0),(210,390,0)]):
        box('BenchSeat_%d'%i,[x,y,ground(x,y)+37],[120,35,8],MAT+'/MI_LG8_Wood',yaw,'Square')
        for j in [-1,1]:
            xx=x+j*42*math.cos(math.radians(yaw));yy=y+j*42*math.sin(math.radians(yaw))
            box('BenchLeg_%d_%d'%(i,j),[xx,yy,ground(x,y)+19],[12,28,35],MAT+'/MI_LG8_Timber',yaw,'Square')
    sm('SanctuaryDais','/Engine/BasicShapes/Cylinder',[0,2690,211],(6.9,6.9,.38),folder='Sanctuary',material=MAT+'/MI_LG8_Cobble',collision=True)
    for i in range(14):
        top=30+(i+1)*195/14
        box('ProcessionalStep_%02d'%i,[0,1950+(i+.5)*34,top-60],[235,35,120],MAT+'/MI_LG8_Stone',folder='Sanctuary')
    for side in [-1,1]:
        for i in range(8):
            yy=2010+i*60
            prop('ShrineParapet_%d_%d'%(side,i),'StoneWall_Seg',[side*174,yy,ground(side*174,yy)-3],.32,90,'Sanctuary')
    for i in range(20):
        t=i*math.tau/20
        if math.sin(t)<-.76:continue
        x=345*math.cos(t);y=2690+345*math.sin(t)
        prop('SanctuaryCoping_%02d'%i,'StoneWall_Seg',[x,y,225],.50,math.degrees(t)+90,'Sanctuary')
    # Portal model is fitted as a whole from measured bounds, source stays untouched.
    inv=json.loads((OUT/'reuse_inventory.json').read_text(encoding='utf8'))
    row=next(r for r in inv if '/Polish_v003/Models/Portal/' in r['path'] and r['type']=='StaticMesh')
    s=250/(row['extent'][2]*2)
    sm('PortalArch',row['path'],[770,2310,ground(770,2310)-(row['origin'][2]-row['extent'][2])*s],s,90,'Sanctuary')
    sm('PortalCourt','/Engine/BasicShapes/Cylinder',[770,2300,ground(770,2300)-8],(4.7,4.7,.18),folder='Sanctuary',material=MAT+'/MI_LG8_Cobble')
    # Village gate is intentionally open along the processional road.
    for side in [-1,1]:
        prop('SouthGatePillar_'+str(side),'Gate_Pillar',[side*170,-3300,0],.6,0,'Entry')
        for i in range(5):prop('SouthGateWall_%d_%d'%(side,i),'StoneWall_Seg',[side*(280+i*95),-3310,ground(side*(280+i*95),-3310)-3],.50,0,'Entry')
    prop('ArrivalSign','Signpost',[200,-3130,ground(200,-3130)],.47,180,'Entry')
    prop('PublicWell','Well',[-660,-2630,ground(-660,-2630)],.48,180,'LowerVillage')
    # Screen-facing, supported wooden bank deck at the watermill.
    for i in range(12):box('MillDeck_%02d'%i,[-2050+i*22,150,22],[21,190,10],MAT+'/MI_LG8_Wood',folder='Waterfront')
    for i,(x,y) in enumerate([(-2070,70),(-1830,70),(-2070,240),(-1830,240)]):box('MillDeckPier_%d'%i,[x,y,-50],[20,20,160],MAT+'/MI_LG8_Timber',folder='Waterfront')
    # River banks mix low stone edge pieces with irregular rock clusters.
    rng=random.Random(818);idx=0
    for a,b in zip(P['river'],P['river'][1:]):
        dx=b[0]-a[0];dy=b[1]-a[1];lng=math.hypot(dx,dy)
        for i in range(max(2,int(lng/95))):
            t=(i+.5)/max(2,int(lng/95));cx=a[0]+dx*t;cy=a[1]+dy*t
            for side in [-1,1]:
                x=cx+side*(-dy/lng)*(220+rng.uniform(-20,35));y=cy+side*(dx/lng)*(220+rng.uniform(-20,35))
                if abs(x)>3600 or any(math.hypot(x-q['x'],y-q['y'])<450 for q in P['bridges']):continue
                prop('RiverStone_%03d'%idx,'Rock_A' if idx%3 else 'Rock_B',[x,y,ground(x,y)-20],rng.uniform(.55,1.15),rng.uniform(0,360),'Water/Banks');idx+=1
    for i in range(15):
        t=i*2.399;x=-2420+400*math.cos(t);y=2190+260*math.sin(t)
        prop('WaterfallCliff_%02d'%i,'Cliff_Block',[x,y,ground(x,y)-180],(.65,.65,.85),rng.uniform(0,360),'Water/Waterfall')
elif STAGE=='gardens_props':
    rng=random.Random(821)
    inv=json.loads((OUT/'reuse_inventory.json').read_text(encoding='utf8'))
    def p2(name,kind,x,y,height=70,yaw=180,zone='Details'):
        row=next(r for r in inv if '/Polish_v003/Models/'+kind+'/' in r['path'] and r['type']=='StaticMesh')
        sc=height/(row['extent'][2]*2)
        return sm(name,row['path'],[x,y,ground(x,y)-(row['origin'][2]-row['extent'][2])*sc],sc,yaw,zone)
    n=0
    for b in P['buildings']:
        if b['kind'] not in ['Cottage_A','Workshop','Inn']:continue
        x=b['x'];y=b['y'];s=b['scale']
        # Domestic gardens and low enclosures leave southern door aprons clear.
        gx=x+215*s;gy=y+20
        if x>2000:gx=x-215*s
        prop('Garden_%02d'%n,'GardenPlot',[gx,gy,ground(gx,gy)-4],(.38,.53,.55),90,'Gardens')
        for j in range(3):
            xx=x-150+j*145;yy=y+230
            prop('Fence_%02d_%02d'%(n,j),'Fence_Seg',[xx,yy,ground(xx,yy)-2],.72,0,'Gardens')
        for j,(kind,h) in enumerate([('Crate',43),('Barrel',55)]):p2('DoorGoods_%02d_%d'%(n,j),kind,x+145+j*42,y-165,h,rng.randrange(360),'Details/Domestic')
        if n%3==0:p2('Laundry_%02d'%n,'Laundry',x-80,y+300,115,90,'Details/Domestic')
        n+=1
    p2('NoticeBoard','Noticeboard',700,785,135,90,'Square')
    p2('PublicBell','Bell',-410,2500,150,90,'Sanctuary')
    for i,(x,y) in enumerate([(1550,770),(2180,850),(1390,-25),(2310,100)]):
        p2('Anvil_%d'%i,'Anvil',x,y,60,90,'Workshops')
        p2('ToolRack_%d'%i,'ToolRack',x+150,y+70,130,90,'Workshops')
        p2('WorkTable_%d'%i,'Table',x-120,y+70,65,90,'Workshops')
        for j in range(3):p2('WorkshopCrate_%d_%d'%(i,j),'Crate',x+220+j*43,y-10,45,rng.randrange(360),'Workshops')
    for i,b in enumerate([q for q in P['buildings'] if q['kind']=='MarketStall']):
        p2('MarketBarrel_%d'%i,'Barrel',b['x']+120,b['y'],50,rng.randrange(360),'Square')
        p2('MarketBox_%d'%i,'Crate',b['x']-110,b['y']-20,40,rng.randrange(360),'Square')
    for i in range(4):
        for j in range(4):
            x=2350+i*115;y=1430+j*145
            prop('Field_%d_%d'%(i,j),'GardenPlot',[x,y,ground(x,y)-3],(.27,.38,.48),0,'Workshops/Fields')
    for i in range(7):prop('FieldFence_%d'%i,'Fence_Seg',[2220+i*100,2030,ground(2220+i*100,2030)-2],.5,0,'Workshops/Fields')
    for i in range(5):prop('FieldEastFence_%d'%i,'Fence_Seg',[2870,1370+i*130,ground(2870,1370+i*130)-2],.65,90,'Workshops/Fields')
    for i,(x,y) in enumerate([(-1780,1000),(-1730,1040),(-1780,1080)]):p2('WashBank_%d'%i,'Laundry',x,y,100,90,'Waterfront')
    # Warm practicals mark junctions, bridge landings and landmarks.
    lanterns=[(-145,-3100),(145,-2400),(-145,-1750),(145,-1140),(-145,-310),(-315,440),(340,930),(-130,1520),(135,1930),(-150,2430),(230,2670),(-1650,-850),(-1570,-200),(2010,-1050),(2010,-280),(-1130,390),(-1770,740),(1820,1250),(1110,1720),(920,2190),(2390,1890)]
    for i,(x,y) in enumerate(lanterns):
        z=ground(x,y)
        prop('LanternPost_%02d'%i,'Lantern_Post',[x,y,z],.57,180,'Lighting/Practical')
        light=actor(unreal.PointLight,'LanternLight_%02d'%i,[x,y+17,z+157],'Lighting/Practical')
        lc=light.light_component;lc.set_mobility(unreal.ComponentMobility.MOVABLE);lc.set_intensity(60)
        lc.set_light_color(unreal.LinearColor(1,.56,.22));lc.set_editor_property('attenuation_radius',240)
        lc.set_cast_shadows(False)
elif STAGE=='foliage':
    import runpy
    design=runpy.run_path(str(ROOT/'Tools/Libregrove/level_v8_design.py'))
    dist=design['distance'];rng=random.Random(8821)
    treepath=next(r['path'] for r in json.loads((OUT/'reuse_inventory.json').read_text(encoding='utf8')) if '/Textured_V7/' in r['path'] and r['type']=='StaticMesh' and 'BroadleafTree' in r['path'])
    configs=[('Canopy',treepath,.75,1.25,330),('Saplings',treepath,.40,.65,240),('Bush','/Game/Village/Meshes/SM_Bush',.5,1.05,135),('Rock','/Game/Village/Meshes/SM_Rock_B',.4,.85,190)]
    result=[]
    for name,path,lo,hi,step in configs:
        ft=unreal.FoliageService.create_foliage_type(path,BASE+'/Foliage','FT_LG8_'+name,lo,hi,False,0,80,16000)
        assert ft.success,ft.error_message
        points=[]
        for ix in range(int(8300/step)):
            for iy in range(int(8300/step)):
                x=-4150+(ix+.5)*step+rng.uniform(-step*.35,step*.35);y=-4150+(iy+.5)*step+rng.uniform(-step*.35,step*.35)
                outer=abs(x)>2880 or y>3100 or y<-3390
                riverdist=dist(x,y,P['river']);road_dist=min(dist(x,y,q['points'])-q['width']/2 for q in P['roads'])
                bdist=min(math.hypot(x-b['x'],y-b['y'])-230*b['scale'] for b in P['buildings'])
                plaza=math.hypot(x,y-675);shrine=math.hypot(x,y-2690)
                if name in ['Canopy','Saplings']:
                    if (name=='Canopy' and not outer and rng.random()>.42) or (name=='Saplings' and outer):continue
                    gap=250 if name=='Canopy' else 165
                    if road_dist<gap or bdist<gap or riverdist<285 or plaza<650 or shrine<610:continue
                    if math.hypot(x+2200,y-1620)<700:continue
                    if 2140<x<3000 and 1240<y<2120:continue
                    if any(math.hypot(x-b['x'],y-b['y'])<560 for b in P['bridges']):continue
                else:
                    if road_dist<50 or bdist<30 or riverdist<240 or plaza<455 or shrine<365:continue
                    if name=='Bush' and rng.random()>(.72 if outer else .38):continue
                    if name=='Rock' and rng.random()>.27:continue
                    if 2120<x<3000 and 1250<y<2080:continue
                points.append(unreal.Vector(-x,y,1600))
        r=unreal.FoliageService.add_foliage_instances(ft.asset_path,points,lo,hi,False,True,True)
        assert r.success and r.instances_added>0,r.error_message
        result.append({'kind':name,'type':ft.asset_path,'requested':len(points),'added':r.instances_added,'rejected':r.instances_rejected})
        assert unreal.EditorAssetLibrary.save_asset(ft.asset_path)
    report['foliage']=result
else:raise RuntimeError('Unknown stage '+STAGE)
E.select_nothing();L.editor_invalidate_viewports();assert L.save_current_level()
report['total_actors']=len(E.get_all_level_actors())
with (OUT/('build_'+STAGE+'_001.json')).open('x',encoding='utf8') as f:json.dump(report,f,indent=2)
print(json.dumps(report))
