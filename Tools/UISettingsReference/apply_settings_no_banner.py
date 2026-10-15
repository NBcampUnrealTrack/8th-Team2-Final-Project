"""Final layout pass: centered settings content, small gear instead of banner."""
import unreal,importlib.util,json,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
s=importlib.util.spec_from_file_location('geo',ROOT/'Tools/UISettingsReference/apply_key_geometric_frame.py')
g=importlib.util.module_from_spec(s);s.loader.exec_module(g);k=g.k;p=g.p;M=g.M;E=g.E

def gear():
 name='M_SettingsGearIcon';path=p.PRIVATE+'/'+name
 a=unreal.load_asset(path)
 if a:return a
 a=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,p.PRIVATE,unreal.Material,unreal.MaterialFactoryNew())
 a.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);a.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 uv=M.create_material_expression(a,unreal.MaterialExpressionTextureCoordinate,-300,0)
 n=M.create_material_expression(a,unreal.MaterialExpressionCustom,0,0)
 ci=unreal.CustomInput();ci.set_editor_property('input_name','UV');n.set_editor_property('inputs',[ci]);n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
 n.set_editor_property('code', '''float2 p=(UV-.5)*2;
float r=length(p);float angle=atan2(p.y,p.x);
float teeth=smoothstep(.15,.55,cos(angle*8));
float outer=.69+.18*teeth;
float aa=max(fwidth(r),.015);
return (1-smoothstep(outer-aa*.5,outer+aa*.5,r))*smoothstep(.30-aa*.5,.30+aa*.5,r);''')
 assert M.connect_material_expressions(uv,'',n,'UV');assert M.connect_material_property(n,'',unreal.MaterialProperty.MP_OPACITY)
 c=M.create_material_expression(a,unreal.MaterialExpressionConstant3Vector,0,180);c.set_editor_property('constant',p.col('gold'));assert M.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 assert not M.recompile_material(a);assert unreal.MaterialNodeService.get_material_diagnostics(path).is_compiled_ok;assert E.save_loaded_asset(a);return a

def apply():
 assert not p.W.is_pie_running()
 out=ROOT/'Saved/UIKeySettingsPolish'/('no_banner_'+datetime.now().strftime('%Y%m%d_%H%M%S'));out.mkdir()
 baseline=ROOT/'Tools/UISettingsReference/banner_layout_baseline.json'
 if baseline.exists():original=json.loads(baseline.read_text())
 else:
  original={}
  for name in ['WBP_OptionsWindow','WBP_KeyBindingsWindow']:
   sk=k.s.Skin(name);stage=sk.get('SettingsStage');original[name]={}
   for i in range(stage.get_children_count()):
    o=stage.get_child_at(i);v=o.slot.get_position();original[name][o.get_name()]=[v.x,v.y]
  baseline.write_text(json.dumps(original,indent=2))
 for name in original:shutil.copy2(ROOT/'Content/Developers/dbals/UI'/(name+'.uasset'),out/(name+'.uasset'))
 g.apply();icon=gear();result={}
 for name,delta in [('WBP_OptionsWindow',78),('WBP_KeyBindingsWindow',52)]:
  sk=k.s.Skin(name)
  for widget,pos in original[name].items():
   if widget in ['WindowBG','ContentBox']:continue
   sk.get(widget).slot.set_position(unreal.Vector2D(pos[0]-delta,pos[1]))
  # Gear/title share the content left edge; close stays aligned with content right.
  x,y=(72,50) if name=='WBP_OptionsWindow' else (100,55)
  o=sk.add('Image','SettingsGearIcon','SettingsStage');o.set_brush(k.q.brush(icon));o.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE);sk.rect('SettingsGearIcon',x,y,30,30,4)
  sk.get('TitleText').slot.set_position(unreal.Vector2D(x+42,43 if name=='WBP_OptionsWindow' else 46))
  if name=='WBP_OptionsWindow':sk.get('TitleText').set_text('설정')
  c=p.B.compile_blueprint(sk.path);assert c.success and not c.num_errors;assert E.save_asset(sk.path,False);result[name]=c.export_text()
 (out/'compile.json').write_text(json.dumps(result,indent=2));print(result)
if __name__=='__main__':apply()
