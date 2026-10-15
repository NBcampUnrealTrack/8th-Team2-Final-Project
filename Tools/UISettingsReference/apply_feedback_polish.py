"""Latest feedback: grouped settings, balanced margins and button typography."""
import unreal,importlib.util,json,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
s=importlib.util.spec_from_file_location('key',ROOT/'Tools/UISettingsReference/apply_key_polish.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k);p=k.p
GROUPS={
 'Display':[('Screen','화면 모드',['WindowModeRow','ResolutionRow']),('Output','화면 출력',['VSyncRow','FrameLimitRow'])],
 'Graphics':[('Preset','전체 품질',['QualityRow']),('Lighting','조명 및 렌더링',['ShadowQualityRow','GlobalIlluminationQualityRow','ReflectionQualityRow','PostProcessQualityRow']),('Detail','세부 표현',['EffectsQualityRow','TextureQualityRow','ViewDistanceQualityRow','UpscaleRow'])],
 'Sound':[('Volume','음량',['SoundNote','MasterVolumeRow','BgmVolumeRow','SfxVolumeRow'])],
 'Controls':[('Input','입력 장치',['KeyBindingsRow','SettingsControlsHelp'])]}

def cardbrush():
 b=unreal.SlateBrush();b.draw_as=unreal.SlateBrushDrawType.ROUNDED_BOX;b.tint_color=p.slate('surface',.78)
 o=b.outline_settings;o.corner_radii=unreal.Vector4(7,7,7,7);o.rounding_type=unreal.SlateBrushRoundingType.FIXED_RADIUS;o.width=1;o.color=p.slate('muted',.14);b.outline_settings=o;return b

def apply():
 assert not p.W.is_pie_running()
 out=ROOT/'Saved/UIFeedbackPolish'/datetime.now().strftime('%Y%m%d_%H%M%S');out.mkdir(parents=True)
 for path in [p.PATH,p.COPY]:
  src=ROOT/'Content'/(path[6:]+'.uasset');shutil.copy2(src,out/src.name)
 sk=k.s.Skin('WBP_OptionsWindow')
 sk.rect('HeaderCloseButton',888,41,40,40,4)
 # Fixed viewport edge includes the scroll bar gutter; cards have equal padding.
 sk.rect('TabSwitcher',72,172,814,358,1)
 sk.rect('SettingsHeaderRule',72,147,814,2,2)
 rule=sk.get('SettingsHeaderRule');rule.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
 brush=unreal.SlateBrush();brush.draw_as=unreal.SlateBrushDrawType.IMAGE;brush.tint_color=p.slate('gold',.4);rule.set_editor_property('background',brush);rule.set_brush_color(unreal.LinearColor(1,1,1,1))
 title=sk.get('TitleText');f=title.get_editor_property('font').copy();f.size=28;title.set_font(f)
 for page,groups in GROUPS.items():
  sc=sk.get('Settings'+page+'Scroll');sc.set_editor_property('scrollbar_padding',unreal.Margin(10,0,0,0))
  content=sk.get(page+'Page');content.slot.set_padding(unreal.Margin(0,0,0,16))
  for ident,label,rows in groups:
   base='SettingsCard'+ident
   border=sk.add('Border',base,page+'Page');border.set_editor_property('background',cardbrush());border.set_padding(unreal.Margin(16,14,16,14));border.slot.set_padding(unreal.Margin(0,0,0,14))
   box=sk.add('VerticalBox',base+'Content',base)
   heading=sk.add('TextBlock',base+'Heading',base+'Content');heading.set_text(label)
   f=sk.get('TitleText').get_editor_property('font').copy();f.size=16;heading.set_font(f);heading.set_color_and_opacity(p.slate('gold'));heading.slot.set_padding(unreal.Margin(0,0,0,10))
   for n in rows:
    row=sk.get(n);box.add_child(row);row.slot.set_size(unreal.SlateChildSize(1,unreal.SlateSizeRule.AUTOMATIC));row.slot.set_padding(unreal.Margin(0,7,0,7))
    if n.endswith('Row'):
     labelbox=unreal.find_object(sk.tree,n[:-3]+'LabelBox')
     if labelbox:labelbox.set_width_override(204)
  if page=='Graphics':sk.get('UpscaleGap').set_visibility(unreal.SlateVisibility.COLLAPSED)
 # Choice controls 36/16; their background and selected layer follow root height.
 bp=unreal.load_asset(p.COPY);bp.modify();tree=unreal.find_object(bp,'WidgetTree')
 for i in range(5):
  o=p.obj(tree,'Option'+str(i));o.set_editor_property('ButtonSize',unreal.Vector2D(104,36));o.set_editor_property('LabelFontSize',16)
 for row in p.W.get_widget_snapshot(p.PATH):
  if row.widget_name.endswith('Group') and row.widget_class=='WBP_OptionsPaletteRadio_C':sk.get(row.widget_name).set_editor_property('LabelFontSize',16)
 for name in ['DefaultsButton','ApplyButton','CloseButton','KeyBindingsButton']:
  button=sk.get(name)
  if isinstance(button.slot,unreal.CanvasPanelSlot):size=button.slot.get_size();button.slot.set_size(unreal.Vector2D(size.x,44))
  label=sk.get(name+'Label');f=label.get_editor_property('font').copy();f.size=18;label.set_font(f)
  label.slot.set_padding(unreal.Margin());label.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER);label.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
  st=button.get_editor_property('widget_style');st.normal_padding=unreal.Margin(16,0,16,0);st.pressed_padding=unreal.Margin(16,1,16,-1);button.set_style(st)
 sk.get('KeySettingsLauncherSize').set_height_override(44)
 for name in ['WindowModeCombo','ResolutionCombo','FrameLimitCombo']:
  o=sk.get(name);f=o.get_editor_property('font').copy();f.size=18;o.set_editor_property('font',f);o.set_editor_property('content_padding',unreal.Margin(16,10,16,10))
 # Each card is its own background/content layer. No whole-window blur or dim added.
 result={}
 for path in [p.COPY,p.PATH]:
  c=p.B.compile_blueprint(path);assert c.success and not c.num_errors,c.export_text();assert p.E.save_asset(path,False);result[path]=c.export_text()
 (out/'settings_compile.json').write_text(json.dumps(result,indent=2));print(result)
if __name__=='__main__':apply()
