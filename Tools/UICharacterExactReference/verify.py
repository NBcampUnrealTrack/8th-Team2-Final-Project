import unreal,json,re,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/UICharacterExactReference';report={}
assert not unreal.WidgetService.is_pie_running()
R=unreal.ObjectReflectionService
for stem in ['MenuBase','InfoPage']:
 name='WBP_Character'+stem;p='/Game/Developers/MinkiCho/UI/CharacterMenu/'+name;bp=unreal.load_asset(p)
 old=json.loads((out/'before'/(name+'.json')).read_text(encoding='utf-8'))
 for key,prop in [('bindings','Bindings'),('animations','Animations')]:assert R.get_property(bp.get_path_name(),prop)==old[key],key
 assert unreal.BlueprintService.describe_graph(p,'EventGraph')[0].export_text()==old['graph']
 before=dict(re.findall(r'\("([^"]+)", ([A-Fa-f0-9]+)\)',old['guids']));after=dict(re.findall(r'\("([^"]+)", ([A-Fa-f0-9]+)\)',R.get_property(bp.get_path_name(),'WidgetVariableNameToGuidMap')))
 assert before and all(after.get(n)==g for n,g in before.items())
 current={s.widget_name:s for s in unreal.WidgetService.get_widget_snapshot(p)};assert all(w['name'] in current for w in old['layout'])
 r=unreal.BlueprintService.compile_blueprint(p);assert r.success,r.export_text();assert unreal.EditorAssetLibrary.save_asset(p,False)
 report[name]={'original_names_guids_bindings_graphs_animations_retained':True,'compile':r.export_text(),'intentional_reparenting':'info live values/portrait; shared controls only relocate transiently at runtime'}
for p in unreal.EditorAssetLibrary.list_assets('/Game/Developers/MinkiCho/Art/CharacterMenu/ExactReference20261014',True,False):
 a=unreal.load_asset(p)
 if isinstance(a,unreal.Texture2D):continue
 assert isinstance(a,unreal.Material),p
 d=unreal.MaterialNodeService.get_material_diagnostics(str(p).split('.')[0]);assert d.success and d.is_compiled_ok,d.export_text();report[str(p)]=d.export_text()
a=root/'ArtSource/UI/CharacterExactReference_20261014/T_ReferenceAtlas.png';b=root/'Saved/UICharacterDetail/Game/Developers/MinkiCho/Art/캐릭터_정보_통합_메뉴.PNG'
assert a.read_bytes()==b.read_bytes();report['original_atlas_sha256']=hashlib.sha256(a.read_bytes()).hexdigest()
(out/'verified.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('EXACT_REFERENCE_VERIFIED',len(report))
