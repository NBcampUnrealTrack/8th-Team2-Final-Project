"""Bury exposed rectangular terrain gaps without changing traversable collision."""
import unreal,json,sys,importlib
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004';out=root/'verification'
assert not unreal.WidgetService.is_pie_running();assert not (out/'transition_banks.json').exists()
sys.path.insert(0,str(root.parent/'Polish_v003'));import ue_common as C;importlib.reload(C)
r=json.loads((root.parent/'Restyle/verification/import_Rock.json').read_text());C.MESH['Rock']=unreal.load_asset(r['meshes'][0]['path']);C.MAT['Rock']=unreal.load_asset('/Game/Cindehearth/HD2D_v002/Materials/MI_CH_P2_Rock')
cube=unreal.load_asset('/Engine/BasicShapes/Cube');rows=[]
def core(name,loc,size,mat):
 a=C.AS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc));a.set_actor_label('CH_D4_Transition_'+name);a.set_folder_path('Cindehearth/Dense_v004/Transitions')
 a.tags=['Cindehearth','CH.Dense_v004'];a.static_mesh_component.set_static_mesh(cube);a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)));a.static_mesh_component.set_material(0,unreal.load_asset('/Game/Cindehearth/HD2D_v002/Materials/MI_CH_'+mat));a.static_mesh_component.set_collision_profile_name('NoCollision');a.set_actor_enable_collision(False);rows.append(a.get_actor_label())
with unreal.ScopedEditorTransaction('Natural stone and soil banks across field joins'):
 core('AshFoundation',[-2465,140,-32.5],[330,1480,330],'Rock')
 for i,y in enumerate([-475,-250,-25,180,570,795]):
  a=C.spawn('CH_D4_Transition_AshRock_'+str(i),'Rock',[-2465+(i%2)*22,y,-10],[375,295,150],(i%3-1)*13,collision=False);a.set_folder_path('Cindehearth/Dense_v004/Transitions');a.tags=list(a.tags)+['CH.Dense_v004'];rows.append(a.get_actor_label())
 core('PineSoilVerge',[306.9,-2925,-85],[3330,610,170],'Soil')
 for side in [-1,1]:
  for i in range(3):
   a=C.spawn('CH_D4_Transition_PineRock_'+str(side)+'_'+str(i),'Rock',[306.9+side*(1590+i%2*20),-2700-i*190,-75],[310,280,125],i*17,collision=False);a.set_folder_path('Cindehearth/Dense_v004/Transitions');a.tags=list(a.tags)+['CH.Dense_v004'];rows.append(a.get_actor_label())
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(out/'transition_banks.json').write_text(json.dumps(dict(actors=rows,collision='All new bank meshes NoCollision; existing walked floors unchanged',purpose='Fill visible rectangular voids beside Ashslope bridge and Pinehollow shoulder'),indent=2))
print('TRANSITION BANKS',len(rows),'parts')
