import unreal,json,sys,importlib,math
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003';sys.path.insert(0,str(root))
import ue_common as C
importlib.reload(C)
assert not unreal.WidgetService.is_pie_running();assert not (root/'verification/environment_refined.json').exists()
rows=[]
with unreal.ScopedEditorTransaction('Cindehearth mountain proportions and grounded hearth'):
 for label,a in list(C.ACTORS.items()):
  if label.endswith('_HD2D_P3') and any(k in label for k in ['RidgeMass','NorthCliff','SideCliff']):
   c,e=a.get_actor_bounds(False);bottom=c.z-e.z;s=a.get_actor_scale3d();mesh=a.static_mesh_component.static_mesh;b=mesh.get_bounding_box()
   width=(b.max.x-b.min.x)*s.x;h=(b.max.z-b.min.z)*s.z
   factor=min(1,width*.44/h);a.set_actor_scale3d(unreal.Vector(s.x,s.y,s.z*factor))
   c2,e2=a.get_actor_bounds(False);p=a.get_actor_location();a.set_actor_location(unreal.Vector(p.x,p.y,p.z+bottom-(c2.z-e2.z)),False,True)
   rows.append(dict(ridge=label,height_factor=factor))
 # A rock brow makes the mine read as an opening into the mountain, hiding its open-top tunnel in exterior views.
 C.spawn('CH_MainMineRockBrow_P3','Ridge',(540,4380,535),(760,580,290),0,tags=['CH.InteriorRoof'])
 for i in range(3):
  a=C.ACTORS['CH_HD2D_HearthFire'+str(i)];p=a.get_actor_location();s=a.get_actor_scale3d()
  a.set_actor_scale3d(unreal.Vector(s.x*.72,s.y*.72,s.z));a.set_actor_location(unreal.Vector(p.x,p.y,238),False,True)
 # A thin gray horizon separates the mountain layers while keeping the player's route readable.
 fog=C.ACTORS['CH_Smog'].get_component_by_class(unreal.ExponentialHeightFogComponent)
 fog.set_editor_property('fog_density',.012);fog.set_editor_property('fog_max_opacity',.58);fog.set_editor_property('start_distance',2100.)
 pp=C.ACTORS['CH_PostProcess'];settings=pp.get_editor_property('settings')
 settings.set_editor_property('override_bloom_intensity',True);settings.set_editor_property('bloom_intensity',.42);pp.set_editor_property('settings',settings)
 C.ACTORS['CH_LevelDirector'].set_night(False)
C.save_result('environment_refined',rows);print('ENVIRONMENT REFINED',len(rows))
