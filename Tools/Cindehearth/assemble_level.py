import unreal
import json
from pathlib import Path

ROOT=Path(unreal.Paths.project_dir())/'Tools/Cindehearth'
D=json.loads((ROOT/'layout.json').read_text(encoding='utf-8'))
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ES=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
LIB=unreal.EditorAssetLibrary
STAGE=globals().get('STAGE','audit')
MAP='/Game/Maps/L_Cindehearth'
FACTOR=100*D['world_scale']
def v(pos):return unreal.Vector(*[x*FACTOR for x in pos])
def r(yaw=0,pitch=0,roll=0):return unreal.Rotator(yaw=yaw,pitch=pitch,roll=roll)
def label(actor,name,zone,tags=()):
    actor.set_actor_label('CH_'+name)
    actor.set_folder_path('Cindehearth/'+zone)
    actor.tags=[unreal.Name('Cindehearth')]+[unreal.Name(t) for t in tags]
def world():return ES.get_editor_world()
def actor_index():return {a.get_actor_label():a for a in AS.get_all_level_actors()}

if STAGE=='create':
    assert not LIB.does_asset_exist(MAP),'Map exists; use build stages.'
    assert not unreal.WidgetService.is_pie_running()
    assert LS.save_current_level(),'Could not preserve the previously open level.'
    assert LS.new_level_from_template(MAP,'/Engine/Maps/Templates/Template_Default')
    print('CREATED',world().get_path_name())
    print([(a.get_actor_label(),a.get_class().get_name()) for a in AS.get_all_level_actors()])
else:
    assert world().get_path_name().startswith(MAP+'.'),world().get_path_name()

if STAGE=='setup':
    # Only new-map template defaults are removed; all authored actors have CH_ labels.
    template_actors=[a for a in AS.get_all_level_actors() if not a.get_actor_label().startswith('CH_')]
    for a in template_actors:
        if not isinstance(a,(unreal.WorldSettings,)):
            AS.destroy_actor(a)
    sun=AS.spawn_actor_from_class(unreal.DirectionalLight,v((0,0,70)),r(170,-48))
    label(sun,'Sun','Lighting',('CH.Sun',))
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_intensity(7.5)
    sun.light_component.set_light_color(unreal.LinearColor(.83,.86,.91))
    sun.light_component.set_editor_property('light_source_angle',2)
    sky=AS.spawn_actor_from_class(unreal.SkyLight,v((0,0,60)))
    label(sky,'SkyLight','Lighting',('CH.SkyLight',))
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_intensity(3.5)
    sky.light_component.set_editor_property('real_time_capture',True)
    dome=AS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
    label(dome,'GreyveinSky','Lighting',('CH.Sky',))
    dome.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'))
    dome.static_mesh_component.set_material(0,unreal.load_asset('/Game/Cindehearth/Materials/MI_CH_Sky'))
    dome.set_actor_scale3d(unreal.Vector(2000,2000,2000))
    dome.static_mesh_component.set_collision_profile_name('NoCollision')
    dome.static_mesh_component.set_cast_shadow(False)
    fog=AS.spawn_actor_from_class(unreal.ExponentialHeightFog,v((0,0,12)))
    label(fog,'Smog','Lighting',('CH.Smog',))
    fc=fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property('fog_density',.012)
    fc.set_editor_property('fog_height_falloff',.16)
    fc.set_editor_property('fog_inscattering_luminance',unreal.LinearColor(.30,.33,.37))
    fc.set_editor_property('start_distance',2300)
    pp=AS.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
    label(pp,'PostProcess','Lighting')
    pp.set_editor_property('unbound',True)
    settings=pp.get_editor_property('settings')
    settings.set_editor_property('override_dynamic_global_illumination_method',True)
    settings.set_editor_property('dynamic_global_illumination_method',unreal.DynamicGlobalIlluminationMethod.SCREEN_SPACE)
    settings.set_editor_property('override_reflection_method',True)
    settings.set_editor_property('reflection_method',unreal.ReflectionMethod.SCREEN_SPACE)
    for key,val in [('auto_exposure_min_brightness',1),('auto_exposure_max_brightness',1),('auto_exposure_bias',-.35),('bloom_intensity',.38),('vignette_intensity',.18),('depth_of_field_fstop',16)]:
        settings.set_editor_property('override_'+key,True);settings.set_editor_property(key,val)
    pp.set_editor_property('settings',settings)
    ps=AS.spawn_actor_from_class(unreal.PlayerStart,v((15.5,-80,1.2)),r(90))
    label(ps,'ArrivalStart','Gameplay')
    ws=world().get_world_settings()
    ws.set_editor_property('default_game_mode',LIB.load_blueprint_class('/Game/Developers/dongh/GameMode/BP_TestGamemode'))
    ws.set_editor_property('kill_z',-20000)
    LS.save_current_level()
    print('SETUP',[(a.get_actor_label(),a.get_class().get_name()) for a in AS.get_all_level_actors()])

if STAGE.startswith('place:'):
    _,start,end=STAGE.split(':');start,end=int(start),int(end)
    existing=actor_index()
    count=0
    for p in D['placements'][start:end]:
        source=p['module']
        parts={'Source':None} if source.startswith('@') else D['modules'][source]
        for mat in parts:
            name=p['name']+'_'+mat
            if 'CH_'+name in existing:continue
            path='/Game/Village/Meshes/'+source[1:] if source.startswith('@') else '/Game/Cindehearth/Meshes/SM_CH_'+source+'_'+mat
            mesh=unreal.load_asset(path);assert mesh,path
            actor=AS.spawn_actor_from_class(unreal.StaticMeshActor,v(p['pos']),r(p['yaw'],p.get('pitch',0),p.get('roll',0)))
            label(actor,name,p['zone']+'/'+p['name'],p['tags'])
            actor.static_mesh_component.set_static_mesh(mesh)
            actor.set_actor_scale3d(unreal.Vector(*[s*D['world_scale'] for s in p['scale']]))
            actor.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
            decorative=mat in ('Flame','Ember','Vein','NaiaGlow','Window','Dark','Paper','Cloth') or source in ('ToolSign','Laundry','StoneNaia','AshNaia','FreedLights','Rail4')
            actor.static_mesh_component.set_collision_profile_name('NoCollision' if decorative else 'BlockAll')
            if source=='@SM_Cliff_Block':actor.static_mesh_component.set_material(0,unreal.load_asset('/Game/Cindehearth/Materials/MI_CH_Rock'))
            if source=='@SM_Rock_A':actor.static_mesh_component.set_material(0,unreal.load_asset('/Game/Cindehearth/Materials/MI_CH_Rock'))
            if mat=='Window':actor.tags=list(actor.tags)+[unreal.Name('CH.Window')]
            if mat in ('Flame','Ember','Vein'):actor.static_mesh_component.set_cast_shadow(False)
            if 'CH.Q11.Freed' in p['tags']:
                actor.set_actor_hidden_in_game(True);actor.set_is_temporarily_hidden_in_editor(True)
            assert actor.get_actor_bounds(False)[1].length()>0,name
            count+=1
    AS.select_nothing();LS.save_current_level()
    print('PLACED',start,end,'actors',count,'TOTAL',len(AS.get_all_level_actors()))

if STAGE=='detail':
    existing=actor_index()
    for p in D['lights']:
        if 'CH_'+p['name'] in existing:continue
        actor=AS.spawn_actor_from_class(unreal.PointLight,v(p['pos']))
        label(actor,p['name'],'Lighting',('CH.Light.'+p['group'],))
        c=actor.light_component;c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_intensity(p['intensity']);c.set_attenuation_radius(p['radius']*FACTOR)
        c.set_light_color(unreal.LinearColor(*p['color']))
        c.set_editor_property('cast_shadows',True)
        c.set_editor_property('source_radius',40)
    for p in D['markers']:
        if 'CH_'+p['name'] in existing:continue
        actor=AS.spawn_actor_from_class(unreal.TargetPoint,v(p['pos']))
        label(actor,p['name'],'Story/'+p['zone'],p['tags'])
        actor.set_editor_property('is_editor_only_actor',True)
    # Actual signs use short Korean diegetic text, not debug labels.
    signs=[('VillageName','신데하스\nCINDEHEARTH',(8,-87,3.3),0,37),
           ('Rockfall','낙반 위험 구역',(12,96.8,9.15),0,20),
           ('ShiftBoard','채굴 배정표',(-21.82,-4,6),-90,15),
           ('Smithy','대장간',(-7,15.48,6.5),0,18),
           ('Tavern','주막 · 프레이',(-8,-62.05,3.2),0,17),
           ('Assay','감정소',(27.95,8,6.4),-90,14),
           ('Clinic','의무소',(31,-50.55,2.8),0,13),
           ('Gorm','고름',(-16.3,-39.08,1.3),0,9)]
    for name,txt,pos,yaw,size in signs:
        if 'CH_Sign_'+name in existing:continue
        a=AS.spawn_actor_from_class(unreal.TextRenderActor,v(pos),r(-90+yaw))
        label(a,'Sign_'+name,'Signs')
        c=a.text_render;c.set_text(txt);c.set_world_size(size)
        c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        c.set_text_render_color(unreal.Color(186,175,150,255))
    AS.select_nothing();LS.save_current_level();print('DETAIL READY',len(AS.get_all_level_actors()))

if STAGE=='audit':
    actors=AS.get_all_level_actors();invalid=[];counts={}
    for a in actors:
        folder=str(a.get_folder_path());zone=folder.split('/')[1] if '/' in folder else folder
        counts[zone]=counts.get(zone,0)+1
        if isinstance(a,unreal.StaticMeshActor) and a.get_actor_bounds(False)[1].length()<.001:invalid.append(a.get_actor_label())
    print(json.dumps({'map':world().get_path_name(),'actors':len(actors),'zones':counts,'zero_bounds':invalid},ensure_ascii=False))
    assert not invalid

if STAGE.startswith('shot:'):
    name=STAGE.split(':')[1]
    # Locations derive from authored plaza/terrain bounds, target + camera distance.
    poses={'overview':((0,0,7),(-52,80,0),350,58),
           'plaza':((-6,0,3.5),(-32,90,0),44,48),
           'arrival':((10,-52,2),(-18,90,0),40,55),
           'top':((0,0,0),(-90,90,0),180,58),
           'mine':((12,83,8),(-25,90,0),38,52),
           'cemetery':((112,66,14),(-33,-110,0),40,55)}
    target,rotation,distance,fov=poses[name];pitch,yaw,roll=rotation;rot=r(yaw,pitch,roll)
    pos=v(target)-rot.get_forward_vector()*(distance*FACTOR)
    unreal.ViewportService.set_camera_location(pos);unreal.ViewportService.set_camera_rotation(rot)
    unreal.ViewportService.set_exposure_game_settings()
    unreal.ViewportService.set_game_view(True)
    AS.select_nothing()
    print(unreal.ScreenshotService.capture_from_pose(pos,rot,'CH_'+name,fov,False,False))
