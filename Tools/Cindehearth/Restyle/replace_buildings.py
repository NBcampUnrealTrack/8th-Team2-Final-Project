import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth'
review=root/'Restyle/verification'
assert not (review/'buildings_replaced.json').exists()
assert not unreal.WidgetService.is_pie_running()
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors={a.get_actor_label():a for a in AS.get_all_level_actors()}
layout=json.loads((root/'layout.json').read_text(encoding='utf-8'))
base='/Game/Cindehearth/HD2D_v002/Materials'
roof=unreal.load_asset(json.loads((review/'import_ClosedRoof.json').read_text(encoding='utf-8'))['meshes'][0]['path'])
assert roof and roof.get_num_triangles(0)==20
roof_mat=unreal.load_asset(base+'/MI_CH_Roof')
house=unreal.load_asset('/Game/Cindehearth/Tripo/GormHouse_P2_Repair_v006/CH_GormP2_Clean_v006/StaticMeshes/CH_GormP2_Clean_v006')
assert house and house.get_num_triangles(0)==1994
body_base='/Game/Cindehearth/Tripo/GormHouse_P2_v001/MI_CH_GormP2_Pixel'
house_roof=unreal.load_asset('/Game/Cindehearth/Tripo/GormHouse_P2_Repair_v006/MI_CH_RepairedRoof_Pixel')
body_mats={}
for key,tint in [('RowHouse',(1.25,1.21,1.12)),('SlopeHouse',(1.13,1.15,1.15)),('BurrowHouse',(1.10,1.06,.98))]:
    mi=base+'/MI_CH_'+key
    assert not unreal.EditorAssetLibrary.does_asset_exist(mi)
    assert unreal.MaterialService.create_instance(body_base,'MI_CH_'+key,base).success
    assert unreal.MaterialService.set_instance_vector_parameter(mi,'Tint',*tint)
    assert unreal.MaterialService.save_instance(mi)
    body_mats[key]=unreal.load_asset(mi)
def hide(a):
    a.modify();a.static_mesh_component.set_visibility(False,True);a.static_mesh_component.set_hidden_in_game(True,True)
    a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True);a.set_actor_enable_collision(False)
def spawn(label,mesh,pos,rotation,scale,tags):
    assert label not in actors
    a=AS.spawn_actor_from_class(unreal.StaticMeshActor,pos,rotation);a.set_actor_label(label)
    a.set_folder_path('Cindehearth/HD2D/Architecture');a.tags=['Cindehearth','CH.HD2D']+tags
    assert a.static_mesh_component.set_static_mesh(mesh)
    a.set_actor_scale3d(unreal.Vector(*scale));a.static_mesh_component.set_collision_profile_name('BlockAll')
    assert a.get_actor_bounds(False)[1].length()>0
    actors[label]=a;return a
result=[]
with unreal.ScopedEditorTransaction('Cindehearth verified houses and closed roofs'):
    for p in layout['placements']:
        key=p['module'];prefix='CH_'+p['name']
        if key in body_mats and p['name']!='GormHouse':
            source=actors[prefix+'_Stone'];pos=source.get_actor_location()
            hidden=[]
            for mat in layout['modules'][key]:
                a=actors.get(prefix+'_'+mat)
                if a:hide(a);hidden.append(a.get_actor_label())
            scale={'RowHouse':(1.,1.,1.),'SlopeHouse':(.90,1.14,.97),'BurrowHouse':(1.08,.94,.83)}[key]
            a=spawn(prefix+'_HD2D',house,pos,unreal.Rotator(yaw=180),scale,[])
            a.static_mesh_component.set_material(0,body_mats[key]);a.static_mesh_component.set_material(1,house_roof)
            result.append(dict(kind='house',label=a.get_actor_label(),hidden=hidden,location=list(pos.to_tuple()),scale=scale))
        elif key in ('Tavern','Smithy','Assay','Clinic','Office'):
            old=actors[prefix+'_Roof'];origin,extent=old.get_actor_bounds(False)
            # Reconstruct the full ridge/eaves envelope from both old roof materials.
            bounds=[a.get_actor_bounds(False) for a in [old,actors[prefix+'_RoofPatch']]]
            lo=[min(o.to_tuple()[i]-e.to_tuple()[i] for o,e in bounds) for i in range(3)]
            hi=[max(o.to_tuple()[i]+e.to_tuple()[i] for o,e in bounds) for i in range(3)]
            # The verified gable is 309 x 243.9 x 154 cm; its base is z=90 cm.
            scale=[(hi[0]-lo[0])/309.,(hi[1]-lo[1])/243.9,(hi[2]-lo[2])/154.]
            center=[(lo[i]+hi[i])*.5 for i in range(3)]
            # GLB->UE flips Y; preserve the original module's south-facing ridge.
            location=unreal.Vector(center[0]-3.15*scale[0],center[1]+9.65*scale[1],lo[2]-90.*scale[2])
            tags=['CH.InteriorRoof'] if old.actor_has_tag('CH.InteriorRoof') else []
            a=spawn(prefix+'_HD2DRoof',roof,location,unreal.Rotator(),scale,tags)
            for slot in range(a.static_mesh_component.get_num_materials()):a.static_mesh_component.set_material(slot,roof_mat)
            hide(old);hide(actors[prefix+'_RoofPatch'])
            result.append(dict(kind='roof',label=a.get_actor_label(),hidden=[old.get_actor_label(),prefix+'_RoofPatch'],location=list(location.to_tuple()),scale=scale))
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(review/'buildings_replaced.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print('REPLACED',len(result),'house/roof groups')
