"""Restore original ivory hub right panel, retaining current left art/compass."""
import unreal,json,shutil
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/UIPanelConsistency'
W=unreal.WidgetService;M=unreal.MaterialEditingLibrary;E=unreal.EditorAssetLibrary;B=unreal.BlueprintService
assert not W.is_pie_running()
before=json.loads((out/'20261006_153811/before.json').read_text(encoding='utf-8'))
hub='/Game/Developers/dbals/UI/WBP_GameMenu'
paths=[hub, '/Game/Developers/dbals/UI/GameMenuHub/WBP_GameMenuTileEntry','/Game/Developers/dbals/UI/GameMenuHub/WBP_GameMenuSystem']
backup=out/('hub_right_before_'+datetime.now().strftime('%Y%m%d_%H%M%S'));backup.mkdir()
for p in paths:
 f=root/'Content'/(p[6:]+'.uasset');shutil.copy2(f,backup/f.name)
art='/Game/Developers/MinkiCho/Art/UITheme/SettingsNavy'
mp=art+'/M_Hub_OriginalRight'
mat=unreal.load_asset(mp) if E.does_asset_exist(mp) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_Hub_OriginalRight',art,unreal.Material,unreal.MaterialFactoryNew())
M.delete_all_material_expressions(mat)
mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
left=M.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-400,0);left.set_editor_property('texture',unreal.load_asset(art+'/T_MenuHub_SettingsNavy'))
right=M.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-400,300);right.set_editor_property('texture',unreal.load_asset('/Game/Developers/dbals/UI/GameMenuHub/Textures/T_GameMenuHub_GoldPattern'))
uv=M.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-600,600)
for alpha,prop in [(False,unreal.MaterialProperty.MP_EMISSIVE_COLOR),(True,unreal.MaterialProperty.MP_OPACITY)]:
 c=M.create_material_expression(mat,unreal.MaterialExpressionCustom,0,300 if alpha else 0)
 c.set_editor_property('code','return lerp(Left,Right,step(0.242,UV.x));')
 c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1 if alpha else unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 inputs=[]
 for n in ['Left','Right','UV']:
  v=unreal.CustomInput();v.set_editor_property('input_name',n);inputs.append(v)
 c.set_editor_property('inputs',inputs)
 assert M.connect_material_expressions(left,'A' if alpha else 'RGB',c,'Left')
 assert M.connect_material_expressions(right,'A' if alpha else 'RGB',c,'Right')
 assert M.connect_material_expressions(uv,'',c,'UV')
 assert M.connect_material_property(c,'',prop)
assert not M.recompile_material(mat)
d=unreal.MaterialNodeService.get_material_diagnostics(mp);assert d.success and d.is_compiled_ok
assert E.save_loaded_asset(mat)
right_names={'TitleText','HubSubtitle','HubWordmark','HubHeaderRule','HubFooterRule','HubFooterSeparator0','HubFooterSeparator1','HubCloseButton','HubCloseX'}
report={}
for p in paths:
 for row in before[p]:
  if p==hub and row['name'] not in right_names:continue
  for prop in ['Brush','Background','ColorAndOpacity','BrushColor','WidgetStyle','BackgroundColor']:
   if prop in row['props']:assert W.set_property(p,row['name'],prop,row['props'][prop]),(p,row['name'],prop)
 if p==hub:
  bp=unreal.load_asset(p);image=unreal.find_object(unreal.find_object(bp,'WidgetTree'),'HubBackplate');image.set_brush_from_material(mat)
 r=B.compile_blueprint(p);assert r.success and r.num_errors==0
 assert E.save_asset(p,False)
 report[p]={'errors':r.num_errors,'warnings':r.num_warnings}
(out/'hub_right_restored.json').write_text(json.dumps(report,indent=2))
