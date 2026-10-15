import unreal,json,random,math
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth';review=root/'Restyle/verification'
assert not (review/'dressing.json').exists()
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors={a.get_actor_label():a for a in AS.get_all_level_actors()}
base='/Game/Cindehearth/HD2D_v002/Materials';rng=random.Random(90540)
cube=unreal.load_asset('/Engine/BasicShapes/Cube');plane=unreal.load_asset('/Engine/BasicShapes/Plane')
rock=unreal.load_asset(json.loads((review/'import_Rock.json').read_text())['meshes'][0]['path'])
mats={k:unreal.load_asset(base+'/MI_CH_'+k) for k in ['Ash','Soil','Path','Stone','StoneLight','Rock','Timber','Wood','Iron','Brass','Cloth']}
added=[];hidden=[]
def hide(a):
    a.modify();a.static_mesh_component.set_visibility(False,True);a.static_mesh_component.set_hidden_in_game(True,True)
    a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True);a.set_actor_enable_collision(False);hidden.append(a.get_actor_label())
def add(name,mesh,p,scale,mat,rot=None,collision=False,tags=[]):
    assert name not in actors
    a=AS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p),rot or unreal.Rotator());a.set_actor_label(name)
    a.set_folder_path('Cindehearth/HD2D/Dressing');a.tags=['Cindehearth','CH.HD2D']+tags
    c=a.static_mesh_component;assert c.set_static_mesh(mesh);c.set_material(0,mat);a.set_actor_scale3d(unreal.Vector(*scale))
    c.set_collision_profile_name('BlockAll' if collision else 'NoCollision');actors[name]=a;added.append(name);return a
def box(name,p,size,mat,**kwargs):return add(name,cube,p,[x/100 for x in size],mat,**kwargs)
with unreal.ScopedEditorTransaction('Cindehearth ground transitions and lived-in dressing'):
    # A land base under the play surfaces removes sky-colored cracks and floating field edges.
    box('CH_HD2D_BasinUnderlay',(0,-3800,-160),(21000,17000,100),mats['Ash'])
    # Fill the unused sides of the stair cutouts while retaining the exact tested 180cm stair width.
    for row,y,z in [(0,-1350,0),(1,1350,157.5)]:
        for x in [-810,1980]:
            if row==0 and x==-810:continue # prior approved Gorm infill
            for side,dx in enumerate([-135,135]):
                box(f'CH_HD2D_StairCheek_{row}_{x}_{side}',(x+dx,y,z+76.5),(90,38.25,153),mats['Stone'],collision=True)
                box(f'CH_HD2D_StairCap_{row}_{x}_{side}',(x+dx,y,z+156),(90,44,8),mats['StoneLight'])
    # Continuous compacted shoulders alongside the single rail axis, with scattered embedded slabs.
    for i,y in enumerate(range(-4300,3950,92)):
        if -2000<y<-1270 or 700<y<1430:continue # rail slopes retain their own deck
        z=0 if y<-1350 else 157.5 if y<1350 else 315
        for side,x in enumerate([340,725]):
            a=add(f'CH_HD2D_RoadStone_{i}_{side}',rock,(x+rng.uniform(-28,28),y+rng.uniform(-20,20),z-3),(rng.uniform(.20,.38),rng.uniform(.19,.30),.055),mats['Path'],unreal.Rotator(yaw=rng.uniform(-35,35)))
    # Intermittent flagstones break the rectangular boundary of each small courtyard.
    for name in ['HearthSquare','SmithyApron','AssayWalk','TavernYard']:
        a=actors['CH_'+name+'_Path'];o,e=a.get_actor_bounds(False)
        for i in range(44 if name=='HearthSquare' else 16):
            side=i%4;t=rng.uniform(-1,1)
            x=o.x+(t*e.x if side<2 else (-e.x if side==2 else e.x))
            y=o.y+((-e.y if side==0 else e.y) if side<2 else t*e.y)
            add(f'CH_HD2D_YardEdge_{name}_{i}',rock,(x+rng.uniform(-22,22),y+rng.uniform(-22,22),o.z+e.z-3),(.35,.22,.045),mats['Path'],unreal.Rotator(yaw=rng.uniform(-180,180)))
    # Low service-yard structures fill the work terrace with ore sorting and stacked timber.
    for yard,x,y,z in [('West',-1700,2600,315),('East',2050,2400,315),('Smithy',-1680,440,157.5)]:
        roofmesh=unreal.load_asset(json.loads((review/'import_ClosedRoof.json').read_text())['meshes'][0]['path'])
        for i,(dx,dy) in enumerate([(-190,-110),(190,-110),(-190,110),(190,110)]):
            box(f'CH_HD2D_Yard_{yard}_Post{i}',(x+dx,y+dy,z+90),(13,13,180),mats['Timber'])
        add(f'CH_HD2D_Yard_{yard}_Roof',roofmesh,(x,y,z+45),(1.45,1.1,.65),unreal.load_asset(base+'/MI_CH_Roof'))
        for i in range(7):
            box(f'CH_HD2D_Yard_{yard}_Crate{i}',(x-145+(i%4)*90,y+80-(i//4)*85,z+29),(58,55,58),mats['Wood'],rot=unreal.Rotator(yaw=rng.uniform(-7,7)))
        for i in range(5):
            box(f'CH_HD2D_Yard_{yard}_Stack{i}',(x+280,y+(i%3)*22,z+11+(i//3)*22),(210,19,19),mats['Timber'])
    # Warm windows and practical doorstep details on the new house silhouettes.
    window=unreal.load_asset('/Game/Cindehearth/Materials/MI_CH_Window')
    for a in list(actors.values()):
        label=a.get_actor_label()
        if not (label.startswith('CH_MinersHome_') and label.endswith('_HD2D')):continue
        p=a.get_actor_location();s=a.get_actor_scale3d();ident=label.removeprefix('CH_').removesuffix('_HD2D')
        # Accepted mesh front points south after yaw=180.
        add('CH_HD2D_Window_'+ident,plane,(p.x-2*s.x,p.y-110*s.y,p.z+167*s.z),(.21*s.x,.23*s.z,1),window,unreal.Rotator(roll=90),tags=['CH.Window'])
        for i in range(2):
            box(f'CH_HD2D_Doorstep_{ident}_{i}',(p.x-88*s.x-i*26,p.y-130*s.y,p.z+15),(22,25,30),mats['Wood'])
        # Some homes have stone banks behind them to read as slope and burrow variants.
        if s.z<.9:
            add('CH_HD2D_BurrowBank_'+ident,rock,(p.x+90,p.y+100,p.z-12),(1.65,1.15,1.4),mats['Rock'])
    # Replace the generated cart as a whole, including its ore, preserving the three gameplay locations.
    receipt=json.loads((review/'import_Cart.json').read_text());assert receipt['status']=='finished'
    mesh=unreal.load_asset(receipt['meshes'][0]['path']);bb=mesh.get_bounding_box();lo=bb.min.to_tuple();hi=bb.max.to_tuple()
    cp=base+'/MI_CH_P2_Cart';S=unreal.MaterialService
    assert S.create_instance('/Game/Cindehearth/Tripo/GormHouse_P2_v001/MI_CH_GormP2_Pixel','MI_CH_P2_Cart',base).success
    for name,suffix in [('Albedo','Color'),('NormalMap','Normal')]:assert S.set_instance_texture_parameter(cp,name,next(p for p in receipt['assets'] if p.endswith('/'+suffix+'.'+suffix)))
    for name,value in [('PixelGrid',256),('NormalStrength',.02),('AmbientLift',.055),('Roughness',.97)]:assert S.set_instance_scalar_parameter(cp,name,value)
    S.save_instance(cp)
    for label in ['IncomingCart','OutgoingCart','AbandonedCart']:
        a=actors['CH_'+label+'_Iron'];p=a.get_actor_location()
        scale=[95/(hi[0]-lo[0]),135/(hi[1]-lo[1]),82/(hi[2]-lo[2])]
        location=(p.x-(lo[0]+hi[0])*.5*scale[0],p.y-(lo[1]+hi[1])*.5*scale[1],p.z-lo[2]*scale[2])
        add('CH_'+label+'_P2_HD2D',mesh,location,scale,unreal.load_asset(cp),collision=True)
        for suffix in ['Iron','Wood','Brass','Rock']:
            old=actors.get('CH_'+label+'_'+suffix)
            if old:hide(old)
    # Primary fire: three crossed crisp silhouettes with offset cadence and a grounded ember bed.
    hide(actors['CH_LivingEmbers_Flame'])
    for i,yaw in enumerate([0,60,120]):
        add('CH_HD2D_HearthFire'+str(i),plane,(-270+(i-1)*12,0,263),(1.8,1.8,1),unreal.load_asset(base+'/MI_CH_PixelFire'+str(i)),unreal.Rotator(roll=90,yaw=yaw))
    forge=actors['CH_WorkingForge_Flame'];o,e=forge.get_actor_bounds(False);hide(forge)
    for i,yaw in enumerate([0,90]):
        add('CH_HD2D_ForgeFire'+str(i),plane,(o.x,o.y,o.z),(max(e.x*2/100,.45),max(e.z*2/100,.6),1),unreal.load_asset(base+'/MI_CH_PixelFire'+str(i)),unreal.Rotator(roll=90,yaw=yaw))
    # Preserve old smoke sources but use the actual new chimney for each replaced home.
    sources={}
    for a in list(actors.values()):
        if '_Smoke' in a.get_actor_label():
            if a.get_actor_label().endswith('_Smoke0'):sources[a.get_actor_label().rsplit('_Smoke',1)[0]]=a.get_actor_location()
            hide(a)
    for name,p in sources.items():
        new=actors.get(name+'_HD2D')
        if new:
            q=new.get_actor_location();s=new.get_actor_scale3d();p=unreal.Vector(q.x-55*s.x,q.y+22*s.y,q.z+258*s.z)
        for i in range(4):
            a=add(name+'_HD2DSoot'+str(i),plane,(p.x,p.y,p.z+30),(1.15,1.15,1),unreal.load_asset(base+'/MI_CH_PixelSmoke'+str(i)),unreal.Rotator(roll=90,yaw=20))
            a.static_mesh_component.set_editor_property('bounds_scale',5.)
    director=next(a for a in actors.values() if a.get_class().get_name()=='NUCindehearthDirector')
    director.set_editor_property('day_sun_intensity',8.)
    director.set_editor_property('day_ambient_intensity',3.8)
    director.set_editor_property('night_sun_intensity',.42)
    director.set_editor_property('night_ambient_intensity',1.)
    director.set_editor_property('village_camera_pitch',-25.)
    director.set_editor_property('village_camera_distance',2100.)
    director.set_editor_property('village_camera_fov',32.)
    director.set_night(False);director.apply_presentation()
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(review/'dressing.json').write_text(json.dumps(dict(added=added,hidden=hidden),indent=2),encoding='utf-8')
print('DRESSING ADDED',len(added),'HIDDEN',len(hidden))
