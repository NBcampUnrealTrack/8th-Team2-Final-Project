"""Fix zero-layer preview scalar normals on an isolated master, not a live graph."""
import unreal,json
from pathlib import Path
OUT=Path(unreal.Paths.project_dir()).resolve()/'Docs/Libregrove/Level_V8_20260908/HD2D'
B='/Game/Libregrove/Level_V8/HD2D';SRC=B+'/M_LG8_HD2D_Landscape';DST=SRC+'_Stable'
E=unreal.MaterialEditingLibrary;A=unreal.EditorAssetLibrary;STAGE=globals().get('STAGE','create')
def save(n,r):
 with (OUT/n).open('x',encoding='utf8') as f:json.dump(r,f,indent=2)
if STAGE=='create':
 assert not A.does_asset_exist(DST);m=A.duplicate_asset(SRC,DST);assert m
 nodes=[n for n in E.get_material_expressions(m) if isinstance(n,unreal.MaterialExpressionCustom) and str(n.get_editor_property('code')).startswith('return normalize')];assert len(nodes)==1
 nodes[0].set_editor_property('code','float3 normalValue=N; return normalize(float3(normalValue.xy*max(0.,Strength),max(.001,normalValue.z)));')
 assert A.save_asset(DST);save('landscape_stable_graph.json',json.loads(unreal.MaterialNodeService.export_material_graph(DST)));print('Isolated master normal promoted explicitly to float3')
elif STAGE=='compile':
 r=unreal.MaterialService.compile_material(DST);save('landscape_stable_compile_request.json',{'requested':bool(r),'note':'Check subsequent shader log, return bool alone is insufficient'});assert r
 print('Unassigned stable material compile requested')
elif STAGE=='repair_superseded':
 assert not unreal.WidgetService.is_pie_running()
 actor=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.Landscape))
 assert actor.get_editor_property('landscape_material').get_editor_property('parent').get_path_name().split('.')[0]==DST
 m=unreal.load_asset(SRC);nodes=[n for n in E.get_material_expressions(m) if isinstance(n,unreal.MaterialExpressionCustom) and str(n.get_editor_property('code')).startswith('return normalize')];assert len(nodes)==1
 nodes[0].set_editor_property('code','float3 normalValue=N; return normalize(float3(normalValue.xy*max(0.,Strength),max(.001,normalValue.z)));')
 A.set_metadata_tag(m,'LibregroveStatus','Backup master; active landscape uses Landscape_Stable. Scalar preview normal fixed.')
 assert A.save_asset(SRC);save('superseded_master_repaired.json',{'path':SRC,'unassigned':True,'preview_scalar_normal_fixed':True});print('Unused backup master repaired without touching active terrain graph')
elif STAGE=='assign':
 assert not unreal.WidgetService.is_pie_running()
 actor=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.Landscape))
 old=actor.get_editor_property('landscape_material');path=B+'/Instances/MI_HD2D_Terrain_Stable';assert not A.does_asset_exist(path)
 mi=A.duplicate_asset(old.get_path_name(),path);E.set_material_instance_parent(mi,unreal.load_asset(DST));assert A.save_asset(path)
 actor.set_editor_property('landscape_material',mi);assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 save('landscape_stable_assignment.json',{'old':old.get_path_name(),'new':mi.get_path_name(),'master':DST,'reason':'Inactive landscape layer blend can be scalar 0; float3 promotion fixes standalone shader permutations'})
 print('Assigned stable terrain instance preserving current tile parameters')
else:raise RuntimeError(STAGE)
