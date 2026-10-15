"""Repair the reference frame's interior opacity without rebuilding its WBP."""
import unreal, shutil, json
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
PATH='/Game/Developers/MinkiCho/Art/UITheme/PopupReference/M_ReferencePanel'
OLD='float inside=step(514,p.x)*step(p.x,1158)*step(264,p.y)*step(p.y,600);'
NEW='float2 edge=min(p-float2(514,264),float2(1158,600)-p);float d=min(min(edge.x,edge.y),edge.x+edge.y-5);float inside=saturate((d+.5)/max(.65,fwidth(d)));'
STRIP='float corner=min(min(p.x-514,1158-p.x),min(p.y-264,600-p.y)); if(corner<4&&!star)a*=smoothstep(.03,.15,c.r-c.b);'
assert not unreal.WidgetService.is_pie_running()
mat=unreal.load_asset(PATH);assert mat
out=ROOT/'Saved/UIPopupReference'/('fill_'+datetime.now().strftime('%Y%m%d_%H%M%S'));out.mkdir(parents=True)
shutil.copy2(ROOT/'Content'/(PATH[6:]+'.uasset'),out/'M_ReferencePanel.uasset')
graph=json.loads(unreal.MaterialNodeService.export_material_graph(PATH))
opacity=next(o['expression_id'] for o in graph['output_connections'] if o['property']=='Opacity')
custom_id=next(c['source_id'] for c in graph['connections'] if c['target_id']==opacity)
node=next(e for e in graph['expressions'] if e['id']==custom_id)
assert node['class']=='Custom'
code=node['properties']['Code'];assert OLD in code or NEW in code
fixed=code.replace(OLD,NEW).replace(STRIP,'')
mat.modify();assert unreal.MaterialNodeService.set_expression_property(PATH,custom_id,'Code',fixed)
assert not unreal.MaterialEditingLibrary.recompile_material(mat)
diag=unreal.MaterialNodeService.get_material_diagnostics(PATH);assert diag.success and diag.is_compiled_ok,diag.export_text()
assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
readback=json.loads(unreal.MaterialNodeService.export_material_graph(PATH))
current=next(e['properties']['Code'] for e in readback['expressions'] if e['id']==custom_id)
assert STRIP not in current and NEW in current
assert next(c['source_id'] for c in readback['connections'] if c['target_id']==opacity)==custom_id
print('FILLED',out,diag.export_text())
