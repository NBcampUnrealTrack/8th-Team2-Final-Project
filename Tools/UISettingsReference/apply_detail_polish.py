"""Settings-only detail polish after apply_palette_pilot; preserve other screens."""
import unreal,importlib.util,json,shutil,hashlib
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
spec=importlib.util.spec_from_file_location('palette',ROOT/'Tools/UISettingsReference/apply_palette_pilot.py');p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
E=p.E;M=p.M;W=p.W;B=p.B
OUT=ROOT/'Saved/UISettingsPolish';OUT.mkdir(exist_ok=True)
TEX=p.PRIVATE+'/T_Settings_FramePolished'
def brush(mat):
 b=unreal.SlateBrush();b.draw_as=unreal.SlateBrushDrawType.IMAGE;b.set_editor_property('resource_object',mat);b.tint_color=unreal.SlateColor(specified_color=unreal.LinearColor(1,1,1,1));return b

def button_material(name,key,state,small=False):
 path=p.PRIVATE+'/'+name
 if not E.does_asset_exist(path):assert E.duplicate_asset('/Game/Developers/MinkiCho/Art/FieldHUD/Alerts20260930/MI_Alert_Button',path)
 a=unreal.load_asset(path);a.modify()
 M.set_material_instance_parent(a,unreal.load_asset(p.PRIVATE+'/M_OptionsPopupFrame'))
 factor={'normal':1,'hovered':1.18,'pressed':.8}[state]
 r,g,b=p.rgb(key)
 M.set_material_instance_vector_parameter_value(a,'SurfaceColor',unreal.LinearColor(r*factor,g*factor,b*factor,1))
 M.set_material_instance_vector_parameter_value(a,'LineColor',p.col('ink' if key=='gold' else 'gold'))
 for k,v in [('CornerPx',4 if small else 6),('CornerAccent',0 if small else .35),('FrameOpacity',(.38 if small else .72) if state=='normal' else 1),('FillOpacity',1),('HoverAmount',.7 if small and state=='hovered' else .35 if state=='hovered' else 0)]:M.set_material_instance_scalar_parameter_value(a,k,v)
 assert E.save_loaded_asset(a);return a

def apply():
 assert not W.is_pie_running()
 backup=OUT/datetime.now().strftime('%Y%m%d_%H%M%S');backup.mkdir()
 for path in [p.PATH,p.COPY]:
  src=ROOT/'Content'/(path[6:]+'.uasset');shutil.copy2(src,backup/src.name)
 before=p.snapshot(p.PATH);(backup/'before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
 bindings=unreal.ObjectReflectionService.get_property(p.PATH,'Bindings')
 parent_path=p.PRIVATE+'/M_OptionsPopupFrame'
 if not E.does_asset_exist(parent_path):assert E.duplicate_asset('/Game/Developers/MinkiCho/Art/FieldHUD/Concept20260929/M_HUDCutFrame',parent_path)
 graph=json.loads(unreal.MaterialNodeService.export_material_graph(parent_path))
 for exp in graph['expressions']:
  if exp['class']=='Custom':
   code=exp['properties']['Code']
   if 'smoothstep(28.0,42.0,size.y)' not in code:
    code=code.replace('*0.28;','*0.28*smoothstep(28.0,42.0,size.y);')
    assert unreal.MaterialNodeService.set_expression_property(parent_path,exp['id'],'Code',code)
 parent=unreal.load_asset(parent_path);assert not M.recompile_material(parent);assert E.save_loaded_asset(parent)
 if not E.does_asset_exist(TEX):
  imported,error=unreal.AssetDiscoveryService.import_asset(str(ROOT/'ArtSource/UI/SettingsReference/T_Settings_FramePolished.png'),p.PRIVATE,'T_Settings_FramePolished');assert imported,error
 tex=unreal.load_asset(TEX)
 for k,v in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('srgb',True),('compression_no_alpha',False),('never_stream',True)]:tex.set_editor_property(k,v)
 assert E.save_loaded_asset(tex)
 # Preserve antialiased neutral highlight pixels as metal, not navy flecks.
 name='M_OptionsPolishedFrame';path=p.PRIVATE+'/'+name
 a=unreal.load_asset(path) if E.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,p.PRIVATE,unreal.Material,unreal.MaterialFactoryNew())
 M.delete_all_material_expressions(a);a.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);a.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 sample=M.create_material_expression(a,unreal.MaterialExpressionTextureSample,0,0);sample.set_editor_property('texture',tex)
 tone=M.create_material_expression(a,unreal.MaterialExpressionCustom,250,0)
 tone.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 ci=unreal.CustomInput();ci.set_editor_property('input_name','C');tone.set_editor_property('inputs',[ci])
 navy=','.join(map(str,p.rgb('navy')));gold=','.join(map(str,p.rgb('gold')))
 tone.set_editor_property('code',f'''float l=dot(C,float3(.2126,.7152,.0722));
 float warm=smoothstep(-.04,.32,(C.r-C.b)/max(.015,max(C.r,C.b)));
 float pale=smoothstep(.06,.18,l)*(1-saturate((C.b-C.r)*8));
 float mask=max(warm,pale);
 return lerp(float3({navy})*clamp(sqrt(l/.023),.68,1.35),float3({gold})*clamp(sqrt(l/.55),.45,1.25),mask);''')
 M.connect_material_expressions(sample,'RGB',tone,'C')
 M.connect_material_property(tone,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(sample,'A',unreal.MaterialProperty.MP_OPACITY)
 assert not M.recompile_material(a);assert E.save_loaded_asset(a)
 bp=unreal.load_asset(p.PATH);bp.modify();t=unreal.find_object(bp,'WidgetTree');t.modify()
 p.obj(t,'WindowFrame').set_editor_property('background',brush(a))
 materials={}
 for kind,key in [('Secondary','surface'),('Primary','gold')]:
  for state in ['normal','hovered','pressed']:materials[kind,state]=button_material('MI_Options'+kind+'_'+state,key,state)
 for name in ['DefaultsButton','CloseButton','ApplyButton','KeyBindingsButton']:
  o=p.obj(t,name);st=o.get_editor_property('widget_style');kind='Primary' if name=='ApplyButton' else 'Secondary'
  for state in ['normal','hovered','pressed']:setattr(st,state,brush(materials[kind,state]))
  st.disabled=brush(materials[kind,'normal']);st.normal_padding=unreal.Margin(12,0,12,0);st.pressed_padding=unreal.Margin(12,1,12,-1);o.set_style(st)
  o.set_background_color(unreal.LinearColor(1,1,1,1))
  label=p.obj(t,name+'Label');slot=label.slot;slot.set_padding(unreal.Margin(0,0,0,0));slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
  label.set_editor_property('justification',unreal.TextJustify.CENTER);label.set_render_translation(unreal.Vector2D(0,-1))
 # Dropdowns reuse the same popup frame; vertical padding remains symmetric.
 for name in ['WindowModeCombo','ResolutionCombo','FrameLimitCombo']:
  o=p.obj(t,name);st=o.get_editor_property('widget_style');cb=st.combo_button_style;bs=cb.button_style
  for state in ['normal','hovered','pressed']:setattr(bs,state,brush(materials['Secondary',state]))
  bs.disabled=brush(materials['Secondary','normal']);bs.normal_padding=unreal.Margin(0,0,0,0);bs.pressed_padding=unreal.Margin(0,0,0,0)
  cb.button_style=bs;st.combo_button_style=cb;o.set_editor_property('widget_style',st);o.set_editor_property('content_padding',unreal.Margin(16,8,16,8))
 # Compact choice cells share the popup outline through their existing noninteractive backgrounds.
 rt=unreal.find_object(unreal.load_asset(p.COPY),'WidgetTree');base=button_material('MI_OptionsChoice','surface','normal',True)
 selected=button_material('MI_OptionsChoiceSelected','surface','hovered',True)
 for i in range(5):
  p.obj(rt,'SettingsRadioBase'+str(i)).set_editor_property('background',brush(base))
  p.obj(rt,'SelectedMark'+str(i)).set_editor_property('background',brush(selected))
 # Overlay selection remains below the label/button so it cannot obscure text.
 for i in range(5):
  cell=p.obj(rt,'OptionCell'+str(i));children=[cell.get_child_at(j) for j in range(cell.get_children_count())]
  order=sorted(children,key=lambda x:0 if x.get_name().startswith('SettingsRadioBase') else 1 if x.get_name().startswith('SelectedMark') else 2)
  refs=','.join('"'+x.slot.get_path_name()+'"' for x in order)
  assert unreal.ObjectReflectionService.set_property(cell.get_path_name(),'Slots','('+refs+')')
 results={}
 for path in [p.COPY,p.PATH]:
  c=B.compile_blueprint(path);assert c.success and c.num_errors==0,c.export_text();assert E.save_asset(path,False);results[path]=c.export_text()
 assert unreal.ObjectReflectionService.get_property(p.PATH,'Bindings')==bindings
 (OUT/'applied.json').write_text(json.dumps({'compile':results,'popup_style_source':'MI_Alert_Button / M_HUDCutFrame','backup':str(backup),'frame':TEX},indent=2));print(results)
if __name__=='__main__':apply()

