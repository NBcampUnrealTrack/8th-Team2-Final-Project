"""Restore the shop's pre-navy presentation, preserving current widget layout/logic."""
import unreal,json,shutil,hashlib
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();BASE=ROOT/'Saved/UIPanelConsistency'
W=unreal.WidgetService;E=unreal.EditorAssetLibrary;B=unreal.BlueprintService;R=unreal.ObjectReflectionService
assert not W.is_pie_running()
original=json.loads((BASE/'20261006_153811/before.json').read_text(encoding='utf-8'))
paths=[path for path in original if path.endswith(('/WBP_ShopPage','/WBP_ShopItemEntry','/WBP_ShopInventorySlot','/WBP_ShopWindow'))]
assert len(paths)==4
out=BASE/('shop_ivory_'+datetime.now().strftime('%Y%m%d_%H%M%S'));out.mkdir()
def backup(path):
 rel=Path(path[6:]+'.uasset');dst=out/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/'Content'/rel,dst)
def layout(path):return [(r.widget_name,r.widget_class,r.parent_widget,r.slot_info.export_text()) for r in W.get_widget_snapshot(path)]
before={path:layout(path) for path in paths};bindings={path:R.get_property(path,'Bindings') for path in paths}
for path in paths:backup(path)
# First material backup precedes the navy SurfaceColor overwrite. These are
# shop-private instances, not shared masters or generated navy wrappers.
base=BASE/'20261006_154526';sources=list((base/'Developers/MinkiCho/Art/Shop').rglob('MI_Shop_*.uasset'));assert len(sources)==18
temporary=[]
for src in sources:
 rel=src.relative_to(base);path='/Game/'+rel.with_suffix('').as_posix();backup(path)
 temp_rel=Path('__ShopIvoryRestore')/src.parent.name/src.name;dest=ROOT/'Content'/temp_rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
 old=unreal.load_asset('/Game/'+temp_rel.with_suffix('').as_posix());assert old
 temporary.append(old);a=unreal.load_asset(path);a.modify()
 color=unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(old,'SurfaceColor')
 unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(a,'SurfaceColor',color)
 unreal.MaterialEditingLibrary.update_material_instance(a);assert E.save_loaded_asset(a)
report={}
for path in paths:
 current={r.widget_name for r in W.get_widget_snapshot(path)};changed=0
 for row in original[path]:
  if row['name'] not in current:continue
  for prop in ['Brush','Background','ColorAndOpacity','BrushColor','WidgetStyle','BackgroundColor']:
   if prop in row['props']:
    assert W.set_property(path,row['name'],prop,row['props'][prop]),(path,row['name'],prop);changed+=1
 assert layout(path)==before[path],path
 assert R.get_property(path,'Bindings')==bindings[path],path
 c=B.compile_blueprint(path);assert c.success and not c.num_errors,c.export_text();assert E.save_asset(path,False)
 report[path]={'style_properties_restored':changed,'errors':c.num_errors,'warnings':c.num_warnings,'layout_and_bindings_preserved':True}
for asset in temporary:assert E.delete_loaded_asset(asset)
(out/'verified.json').write_text(json.dumps({'widgets':report,'shop_materials_restored':len(sources)},indent=2));print(json.dumps(report));print('BACKUP',out)
