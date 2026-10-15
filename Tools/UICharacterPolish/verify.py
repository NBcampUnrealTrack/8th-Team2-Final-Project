import unreal,json,re
from pathlib import Path
root=Path('D:/UnrealEngine/ProjectM');folder='/Game/Developers/MinkiCho/UI/CharacterMenu/';R=unreal.ObjectReflectionService
report=[]
for f in (root/'Saved/UICharacterPolish/before').glob('*.json'):
 old=json.loads(f.read_text(encoding='utf-8'));p=folder+f.stem;bp=unreal.load_asset(p)
 for key,prop in [('guids','WidgetVariableNameToGuidMap'),('bindings','Bindings'),('animations','Animations')]:
  now=R.get_property(bp.get_path_name(),prop)
  if key=='guids':
   before_map=dict(re.findall(r'\(\"([^\"]+)\", ([A-Fa-f0-9]+)\)',old[key]));after_map=dict(re.findall(r'\(\"([^\"]+)\", ([A-Fa-f0-9]+)\)',now));assert all(after_map.get(n)==g for n,g in before_map.items()),p
  else:assert now==old[key],(p,key)
 assert unreal.BlueprintService.describe_graph(p,'EventGraph')[0].export_text()==old['graph'],p
 snaps=unreal.WidgetService.get_widget_snapshot(p);new={s.widget_name:s for s in snaps}
 assert {w['name'] for w in old['layout']}<=set(new),p
 assert all(n.startswith('PolishShadow_') for n in set(new)-{w['name'] for w in old['layout']}),p
 for w in old['layout']:assert new[w['name']].parent_widget==w['parent'],(p,w['name'])
 result=unreal.BlueprintService.compile_blueprint(p);assert result.success,result.export_text()
 report.append({'asset':p,'widgets':len(snaps),'original_structure_guids_graph_bindings_animations_preserved':True,'compile':result.export_text()})
(root/'Saved/UICharacterPolish/verified.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('VERIFIED',len(report),'assets')
