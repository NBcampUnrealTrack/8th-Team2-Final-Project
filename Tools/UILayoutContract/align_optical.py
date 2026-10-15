"""Final optical alignment pass; run after contract / hub artwork passes."""
import unreal, importlib.util, json, shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
s=importlib.util.spec_from_file_location('optical_contract',ROOT/'Tools/UILayoutContract/apply.py');a=importlib.util.module_from_spec(s);s.loader.exec_module(a)
h=a.h

def apply():
 out=ROOT/'Saved/UILayoutContract'/('optical_'+datetime.now().strftime('%Y%m%d_%H%M%S'));out.mkdir(parents=True)
 sk=a.k.s.Skin('WBP_OptionsWindow')
 for path in [sk.path,h.MAIN,h.ART+'/M_Hub_ContractRail']:
  shutil.copy2(ROOT/'Content'/(path[6:]+'.uasset'),out/(path.split('/')[-1]+'.uasset'))
 # 36px gear (20% larger), with both visual centers at header y98.
 sk.rect('SettingsGearIcon',70,80,36,36,4)
 sk.rect('TitleText',115,78,789,48,4)
 rule=sk.get('SettingsHeaderRule').slot.get_position().y
 for i in range(4):
  o=sk.get('SettingsTabMark'+str(i));p=o.slot.get_position();o.slot.set_position(unreal.Vector2D(p.x,rule-o.slot.get_size().y+1))
 # A symmetric X has no font ascender/descender offsets. Button owns its icon.
 mat=h.mat('M_SettingsCenteredCloseIcon');uv=h.M.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-400,0)
 alpha=h.custom(mat,'float2 p=UV-.5;float d=min(abs(p.x-p.y),abs(p.x+p.y))*.707107;float aa=max(fwidth(d),.002);return (1-smoothstep(.045-aa,.045+aa,d))*(1-smoothstep(.36,.39,max(abs(p.x),abs(p.y))));',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('UV',uv,'')])
 c=h.M.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,0,150);c.set_editor_property('constant',unreal.LinearColor(.60,.58,.50,1));h.M.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);h.M.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY);h.finishmat(mat)
 button=sk.get('HeaderCloseButton');style=button.get_editor_property('widget_style').copy();style.normal_padding=unreal.Margin();style.pressed_padding=unreal.Margin();button.set_style(style);button.set_render_transform_pivot(unreal.Vector2D(.5,.5))
 glyph=unreal.find_object(sk.tree,'HeaderCloseGlyph')
 if glyph:glyph.remove_from_parent()
 icon=sk.add('Image','SettingsCloseIcon','HeaderCloseButton');icon.set_brush_from_material(mat);icon.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
 icon.slot.set_padding(unreal.Margin(8,8,8,8));icon.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL);icon.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
 # The rail contains baked footer labels; sample only those 5px lower.
 path=h.ART+'/M_Hub_ContractRail'
 for e in json.loads(unreal.MaterialNodeService.export_material_graph(path))['expressions']:
  if e['class']!='Custom':continue
  code=e['properties']['Code'];new=code
  if code.strip()=='return UV*float2(740,899)/float2(1672,941);':
   new='float2 q=UV*float2(740,899);if(q.y>824&&((q.x>105&&q.x<199)||(q.x>241&&q.x<394)||q.x>442))q.y-=5;return q/float2(1672,941);'
  if 'float header=Mask' in code:new=code.replace('q.y>854','q.y>859')
  if new!=code:assert unreal.MaterialNodeService.set_expression_property(path,e['id'],'Code',new)
 h.finishmat(unreal.load_asset(path))
 bp,t=h.tree(h.MAIN);h.rect(h.obj(t,'HubBannerProject'),102,45,330,44)
 for path in [sk.path,h.MAIN]:
  result=h.B.compile_blueprint(path);assert result.success and not result.num_errors;assert h.E.save_asset(path,False);print(path,result.export_text())
if __name__=='__main__':apply()
