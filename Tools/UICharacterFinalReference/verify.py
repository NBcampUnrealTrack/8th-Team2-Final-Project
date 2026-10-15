"""Read-only identity and shader validation through Unreal after stopping PIE."""
import unreal,json,re
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/UICharacterFinalReference';report={}
assert not unreal.WidgetService.is_pie_running()
R=unreal.ObjectReflectionService
for stem in ['MenuBase','InfoPage']:
 name='WBP_Character'+stem;path='/Game/Developers/MinkiCho/UI/CharacterMenu/'+name
 old=json.loads((out/'before'/(name+'.json')).read_text(encoding='utf-8'));bp=unreal.load_asset(path)
 for key,prop in [('bindings','Bindings'),('animations','Animations')]:assert R.get_property(bp.get_path_name(),prop)==old[key],key
 assert unreal.BlueprintService.describe_graph(path,'EventGraph')[0].export_text()==old['graph']
 before=dict(re.findall(r'\("([^"]+)", ([A-Fa-f0-9]+)\)',old['guids']));after=dict(re.findall(r'\("([^"]+)", ([A-Fa-f0-9]+)\)',R.get_property(bp.get_path_name(),'WidgetVariableNameToGuidMap')))
 assert before and all(after.get(n)==g for n,g in before.items())
 current={s.widget_name:s for s in unreal.WidgetService.get_widget_snapshot(path)}
 assert all(w['name'] in current and current[w['name']].parent_widget==w['parent'] for w in old['layout'])
 result=unreal.BlueprintService.compile_blueprint(path);assert result.success,result.export_text();assert unreal.EditorAssetLibrary.save_asset(path,False)
 report[name]={'original_widgets':len(old['layout']),'guids_bindings_graphs_animations_parents_preserved':True,'compile':result.export_text()}
for p in unreal.EditorAssetLibrary.list_assets('/Game/Developers/MinkiCho/Art/CharacterMenu/FinalReference20261014',True,False):
 asset=unreal.load_asset(p);
 if isinstance(asset,unreal.Texture2D):continue
 assert isinstance(asset,unreal.Material),p
 unreal.MaterialEditingLibrary.recompile_material(asset)
 diagnostic=unreal.MaterialNodeService.get_material_diagnostics(str(p).split('.')[0]);assert diagnostic.success and diagnostic.is_compiled_ok,diagnostic.export_text()
 report[str(p)]={'material':diagnostic.export_text()}
(out/'preservation.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('DETAIL_PRESERVATION_AND_COMPILE_PASSED',len(report))

