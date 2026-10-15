"""Options-only palette pilot. Run in editor, PIE stopped; never recolors shared assets."""
import unreal,json,re,shutil,hashlib
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/UISettingsPalette';OUT.mkdir(exist_ok=True)
W=unreal.WidgetService;E=unreal.EditorAssetLibrary;B=unreal.BlueprintService;M=unreal.MaterialEditingLibrary
PATH='/Game/Developers/dbals/UI/WBP_OptionsWindow'
PRIVATE='/Game/Developers/MinkiCho/Art/UITheme/SettingsPalettePilot'
RADIO='/Game/Developers/dbals/UI/WBP_OptionRadioGroup'
COPY=PRIVATE+'/WBP_OptionsPaletteRadio'
HEX={'navy':'16293D','surface':'1E303D','gold':'C8B691','text':'F1EDE6','muted':'AAB6BF','ink':'243641'}
def rgb(key):
 h=HEX[key];s=[int(h[i:i+2],16)/255 for i in (0,2,4)]
 return tuple(v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in s)
def col(key,a=1):return unreal.LinearColor(*rgb(key),a)
def slate(key,a=1):return unreal.SlateColor(specified_color=col(key,a))
def obj(t,n):
 o=unreal.find_object(t,n);assert o,n;o.modify();return o
def snapshot(p):return [{'name':r.widget_name,'class':r.widget_class,'parent':r.parent_widget,'slot':r.slot_info.export_text(),'props':{v.property_name:v.current_value for v in r.properties}} for r in W.get_widget_snapshot(p)]
PAT=re.compile(r'\(R=([\d.eE+-]+),G=([\d.eE+-]+),B=([\d.eE+-]+),A=([\d.eE+-]+)\)')
def recolor(p):
 for row in snapshot(p):
  for prop,value in row['props'].items():
   if prop not in ['Background','Brush','WidgetStyle','ItemStyle','ScrollBarStyle','ColorAndOpacity','ForegroundColor','SliderBarColor','SliderHandleColor','LabelColor']:continue
   def change(m):
    r,g,b,a=map(float,m.groups());key=None
    if a==0 or (r,g,b) in [(0,0,0),(1,1,1)]:return m.group()
    if any(max(abs(x-y) for x,y in zip((r,g,b),rgb(k)))<.000002 for k in HEX):return m.group()
    if min(r,g,b)>.7:key='text'
    elif r>g>b and r-b>.08:key='gold'
    elif max(r,g,b)<.15 and b>r:key='ink' if row['name']=='ApplyButtonLabel' else 'surface'
    elif b>=r and max(r,g,b)<.8:key='muted'
    if not key:return m.group()
    rr,gg,bb=rgb(key)
    return f'(R={rr:.9f},G={gg:.9f},B={bb:.9f},A={a:.9f})'
   new=PAT.sub(change,value)
   if new!=value:assert W.set_property(p,row['name'],prop,new),(row['name'],prop)
def material():
 name='M_OptionsPaletteFrame';path=PRIVATE+'/'+name
 a=unreal.load_asset(path) if E.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,PRIVATE,unreal.Material,unreal.MaterialFactoryNew())
 a.modify();M.delete_all_material_expressions(a)
 a.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);a.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 tex=M.create_material_expression(a,unreal.MaterialExpressionTextureSample,-600,0)
 tex.set_editor_property('texture',unreal.load_asset('/Game/Developers/MinkiCho/Art/Settings/Textures/T_Settings_Frame'))
 n=M.create_material_expression(a,unreal.MaterialExpressionCustom,-100,0)
 n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 inp=unreal.CustomInput();inp.set_editor_property('input_name','C');n.set_editor_property('inputs',[inp])
 navy=','.join(map(str,rgb('navy')));gold=','.join(map(str,rgb('gold')))
 n.set_editor_property('code',f'''float l=dot(C,float3(.2126,.7152,.0722));
float warm=smoothstep(.03,.22,(C.r-C.b)/max(.01,max(C.r,C.b)));
float shade=clamp(sqrt(l/.023),.68,1.35);
float metal=clamp(sqrt(l/.55),.45,1.25);
return lerp(float3({navy})*shade,float3({gold})*metal,warm);''')
 assert M.connect_material_expressions(tex,'RGB',n,'C')
 assert M.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 assert M.connect_material_property(tex,'A',unreal.MaterialProperty.MP_OPACITY)
 assert not M.recompile_material(a)
 assert E.save_loaded_asset(a);return a

def apply():
 assert not W.is_pie_running()
 backup=OUT/datetime.now().strftime('%Y%m%d_%H%M%S');backup.mkdir()
 file=ROOT/'Content/Developers/dbals/UI/WBP_OptionsWindow.uasset';shutil.copy2(file,backup/file.name)
 protected=[ROOT/'Content/Developers/dbals/UI'/f'{n}.uasset' for n in ['WBP_KeyBindingsWindow','WBP_KeyBindingRow','WBP_KeyBindingGroup','WBP_OptionRadioGroup']]
 protected.append(ROOT/'Content/Developers/MinkiCho/Art/Settings/Textures/T_Settings_Frame.uasset')
 hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
 before=snapshot(PATH);(backup/'before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
 bindings=unreal.ObjectReflectionService.get_property(PATH,'Bindings')
 if not E.does_asset_exist(COPY):assert E.duplicate_asset(RADIO,COPY)
 recolor(COPY)
 rt=unreal.find_object(unreal.load_asset(COPY),'WidgetTree')
 for i in range(5):obj(rt,'Option'+str(i)).set_editor_property('label_color',col('text'))
 c=B.compile_blueprint(COPY);assert c.success and c.num_errors==0;assert E.save_asset(COPY,False)
 bp=unreal.load_asset(PATH);bp.modify();t=unreal.find_object(bp,'WidgetTree');t.modify()
 # Only this screen receives private radio templates; shared radio and common button assets remain unchanged.
 for row in before:
  if row['class']!='WBP_OptionRadioGroup_C':continue
  old=obj(t,row['name']);slot=old.slot
  props={k:slot.get_editor_property(k) for k in ['padding','size','horizontal_alignment','vertical_alignment']}
  size=old.get_editor_property('label_font_size')
  assert W.remove_component(PATH,row['name'])
  result=W.add_component(PATH,'WBP_OptionsPaletteRadio',row['name'],row['parent'],True);assert result.success,result.export_text()
  new=obj(t,row['name']);new.set_editor_property('label_font_size',size)
  for k,v in props.items():new.slot.set_editor_property(k,v)
 recolor(PATH)
 for name in ['WindowModeCombo','ResolutionCombo','FrameLimitCombo']:
  o=obj(t,name);o.set_editor_property('foreground_color',slate('text'))
  st=o.get_editor_property('widget_style');cb=st.combo_button_style;bs=cb.button_style
  for state in ['normal','hovered','pressed','disabled']:
   br=getattr(bs,state);br.tint_color=slate('surface');outline=br.outline_settings;outline.color=slate('gold',1 if state=='hovered' else .45);br.outline_settings=outline;setattr(bs,state,br)
  cb.button_style=bs;st.combo_button_style=cb;o.set_editor_property('widget_style',st)
 frame=obj(t,'WindowFrame');brush=frame.get_editor_property('background');brush.set_editor_property('resource_object',material());frame.set_editor_property('background',brush)
 # Text colors are explicit roles, unaffected by old texture tints.
 for row in snapshot(PATH):
  if row['class']!='TextBlock':continue
  name=row['name'];key='text'
  if name in ['SoundNote','SettingsControlsHelp'] or name.endswith('TabButtonLabel'):key='muted'
  if name.startswith('SettingsTabLabel') or name in ['QualityRowLabel','HeaderCloseGlyph']:key='gold'
  if name=='ApplyButtonLabel':key='ink'
  obj(t,name).set_color_and_opacity(slate(key))
 # Keep state contrast derived from the same two fills.
 for name in ['DefaultsButton','CloseButton','ApplyButton','KeyBindingsButton']:
  o=obj(t,name);s=o.get_editor_property('widget_style');key='gold' if name=='ApplyButton' else 'surface'
  for state,factor in [('normal',1),('hovered',1.20),('pressed',.80)]:
   br=getattr(s,state);r,g,b=rgb(key);br.tint_color=unreal.SlateColor(specified_color=unreal.LinearColor(r*factor,g*factor,b*factor,1));setattr(s,state,br)
  o.set_style(s)
 after=snapshot(PATH)
 assert [(r['name'],r['parent'],r['slot']) for r in before]==[(r['name'],r['parent'],r['slot']) for r in after],'layout changed'
 assert unreal.ObjectReflectionService.get_property(PATH,'Bindings')==bindings
 c=B.compile_blueprint(PATH);assert c.success and c.num_errors==0,c.export_text();assert E.save_asset(PATH,False)
 assert hashes=={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
 report={'palette_srgb':HEX,'compile':c.export_text(),'layout_unchanged':True,'bindings_unchanged':True,'shared_assets_unchanged':True,'private_radio':COPY,'backup':str(backup)}
 (OUT/'applied.json').write_text(json.dumps(report,indent=2));print(report)
if __name__=='__main__':apply()

