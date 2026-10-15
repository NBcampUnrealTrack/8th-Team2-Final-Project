import unreal,json
from pathlib import Path
S=unreal.MaterialService;L=unreal.EditorAssetLibrary
dest='/Game/Cindehearth/HD2D_v002/Materials'
assert L.does_asset_exist('/Game/Village/Materials/M_HD2D_Sky')
palettes={
 'Day':dict(ZenithCol=(.14,.24,.32),MidCol=(.40,.48,.50),HorizonCol=(.64,.62,.54),GroundCol=(.15,.17,.16),RidgeFar=(.31,.38,.40),RidgeNear=(.16,.21,.22),CloudCol=(.70,.72,.68),CloudShadow=(.38,.43,.46),SunCol=(.85,.78,.63)),
 'Night':dict(ZenithCol=(.010,.026,.047),MidCol=(.032,.055,.08),HorizonCol=(.15,.18,.20),GroundCol=(.022,.035,.045),RidgeFar=(.05,.09,.12),RidgeNear=(.027,.048,.065),CloudCol=(.12,.17,.21),CloudShadow=(.03,.05,.08),SunCol=(.035,.06,.09))}
for name,values in palettes.items():
    mi=dest+'/MI_CH_Sky_'+name
    assert not L.does_asset_exist(mi)
    assert S.create_instance('/Game/Cindehearth/Materials/MI_CH_Sky','MI_CH_Sky_'+name,dest).success
    for key,value in values.items():assert S.set_instance_vector_parameter(mi,key,*value)
    assert S.set_instance_vector_parameter(mi,'Tune',.42,.36,.00045,1)
    assert S.save_instance(mi)
actors={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
d=actors['CH_LevelDirector'];d.modify()
d.set_editor_property('day_sky',unreal.load_asset(dest+'/MI_CH_Sky_Day'))
d.set_editor_property('night_sky',unreal.load_asset(dest+'/MI_CH_Sky_Night'))
d.set_night(False)
actors['CH_Sun'].set_actor_rotation(unreal.Rotator(pitch=-38,yaw=-48),False)
fog=actors['CH_Smog'].get_component_by_class(unreal.ExponentialHeightFogComponent)
fog.set_editor_property('fog_density',.018)
fog.set_editor_property('fog_height_falloff',.16)
fog.set_editor_property('start_distance',1800.)
fog.set_editor_property('fog_inscattering_luminance',unreal.LinearColor(.38,.43,.44))
pp=actors['CH_PostProcess'];settings=pp.get_editor_property('settings')
for key,val in [('auto_exposure_bias',.05),('bloom_intensity',.35),('vignette_intensity',.23),('scene_color_tint',unreal.LinearColor(1.04,1.0,.94)),('color_saturation',unreal.Vector4(.91,.91,.91,1))]:
    settings.set_editor_property('override_'+key,True);settings.set_editor_property(key,val)
pp.set_editor_property('settings',settings)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
for name,p,t,fov in [('palette_square',(1550,-1500,930),(-300,0,160),43),('palette_sky',(0,-4900,600),(0,1600,1500),55)]:
    pos=unreal.Vector(*p);rot=unreal.MathLibrary.find_look_at_rotation(pos,unreal.Vector(*t))
    print(unreal.ScreenshotService.capture_from_pose(pos,rot,'CH_Restyle_'+name,fov,False,False))
print('DAY/NIGHT SKY AND PALETTE APPLIED')
