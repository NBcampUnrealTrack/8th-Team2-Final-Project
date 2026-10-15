"""Display hub at 85% viewport fit, with larger relative labels/icons for readability."""
import unreal,json,shutil
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/UIPanelConsistency'
W=unreal.WidgetService;B=unreal.BlueprintService;E=unreal.EditorAssetLibrary
assert not W.is_pie_running()
paths=['/Game/Developers/dbals/UI/WBP_GameMenu','/Game/Developers/dbals/UI/GameMenuHub/WBP_GameMenuTileEntry']
original=json.loads((out/'20261006_153811/before.json').read_text(encoding='utf-8'))
backup=out/('compact_before_'+datetime.now().strftime('%Y%m%d_%H%M%S'));backup.mkdir()
report={}
for p in paths:
 src=root/'Content'/(p[6:]+'.uasset');shutil.copy2(src,backup/src.name)
 bp=unreal.load_asset(p);bp.modify();tree=unreal.find_object(bp,'WidgetTree');tree.modify()
 bindings=unreal.ObjectReflectionService.get_property(p,'Bindings')
 for r in original[p]:
  obj=unreal.find_object(tree,r['name'])
  if isinstance(obj,unreal.TextBlock) and 'Font' in r['props']:
   assert W.set_property(p,r['name'],'Font',r['props']['Font'])
   font=obj.get_editor_property('font');font.size=round(font.size*1.10);obj.set_font(font)
 if p==paths[0]:
  fit=unreal.find_object(tree,'HubFit');fit.slot.modify()
  fit.slot.set_anchors(unreal.Anchors(unreal.Vector2D(.075,.075),unreal.Vector2D(.925,.925)))
  fit.slot.set_offsets(unreal.Margin(0,0,0,0))
 else:
  icon=unreal.find_object(tree,'Img_Icon');icon.slot.set_position(unreal.Vector2D(52,29));icon.slot.set_size(unreal.Vector2D(96,96))
  for n,x,y,w,h in [('HubStatBar1',65.3,83.2,17.9,41.4),('HubStatBar2',92.2,49.6,17.9,75),('HubStatBar3',119.1,68.6,17.9,56),('HubStatBaseline',58.6,129.1,86.2,3.4)]:
   obj=unreal.find_object(tree,n);obj.slot.set_position(unreal.Vector2D(x,y));obj.slot.set_size(unreal.Vector2D(w,h))
 r=B.compile_blueprint(p);assert r.success and r.num_errors==0
 assert unreal.ObjectReflectionService.get_property(p,'Bindings')==bindings
 assert E.save_asset(p,False)
 report[p]={'errors':r.num_errors,'warnings':r.num_warnings,'bindings_unchanged':True}
(out/'compact_hub.json').write_text(json.dumps(report,indent=2))
