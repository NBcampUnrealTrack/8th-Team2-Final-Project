"""Final pass: offset headings and independently scrollable settings tabs."""
import unreal,importlib.util,json,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
s=importlib.util.spec_from_file_location('key',ROOT/'Tools/UISettingsReference/apply_key_polish.py')
k=importlib.util.module_from_spec(s);s.loader.exec_module(k);p=k.p

def apply():
 assert not p.W.is_pie_running()
 out=ROOT/'Saved/UISettingsScroll'/datetime.now().strftime('%Y%m%d_%H%M%S');out.mkdir(parents=True)
 for name in ['WBP_OptionsWindow','WBP_KeyBindingsWindow']:
  shutil.copy2(ROOT/'Content/Developers/dbals/UI'/(name+'.uasset'),out/(name+'.uasset'))
  sk=k.s.Skin(name)
  for n,x in [('SettingsGearIcon',12 if name=='WBP_OptionsWindow' else 40),('TitleText',54 if name=='WBP_OptionsWindow' else 82)]:
   pos=sk.get(n).slot.get_position();sk.get(n).slot.set_position(unreal.Vector2D(x,pos.y))
 sk=k.s.Skin('WBP_OptionsWindow')
 pages=['Display','Graphics','Sound','Controls']
 ref=k.s.Skin('WBP_KeyBindingsWindow').get('RowScroll')
 style=ref.get_editor_property('widget_style');barstyle=ref.get_editor_property('widget_bar_style')
 # Create all parents before moving pages: WidgetService may compile after adding.
 for page in pages:sk.add('ScrollBox','Settings'+page+'Scroll','TabSwitcher')
 for page in pages:
  scroll=sk.get('Settings'+page+'Scroll');scroll.add_child(sk.get(page+'Page'))
  scroll.set_visibility(unreal.SlateVisibility.VISIBLE)
  scroll.set_editor_property('widget_style',style);scroll.set_editor_property('widget_bar_style',barstyle)
  scroll.set_editor_property('scrollbar_thickness',unreal.Vector2D(5,5));scroll.set_editor_property('scrollbar_padding',unreal.Margin(10,0,0,0))
  scroll.set_editor_property('always_show_scrollbar',False);scroll.set_editor_property('allow_overscroll',False)
  scroll.set_editor_property('animate_wheel_scrolling',True)
  scroll.set_consume_mouse_wheel(unreal.ConsumeMouseWheel.WHEN_SCROLLING_POSSIBLE)
  scroll.set_editor_property('wheel_scroll_multiplier',1.25)
  scroll.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
  scroll.set_editor_property('scroll_when_focus_changes',unreal.ScrollWhenFocusChanges.ANIMATED_SCROLL)
  scroll.slot.set_padding(unreal.Margin());scroll.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL);scroll.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
  content=sk.get(page+'Page');content.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL);content.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP);content.slot.set_padding(unreal.Margin(12,18,18,22))
  for i in range(content.get_children_count()):
   row=content.get_child_at(i)
   row.slot.set_size(unreal.SlateChildSize(1,unreal.SlateSizeRule.AUTOMATIC))
   if row.get_name().endswith('Row'):
    row.slot.set_padding(unreal.Margin(0,12,0,12))
 # Natural-height radio rows replace the compact all-at-once presentation.
 for prefix in ['Quality','ShadowQuality','GlobalIlluminationQuality','ReflectionQuality','PostProcessQuality','EffectsQuality','TextureQuality','ViewDistanceQuality','Upscale']:
  group=sk.get(prefix+'Group');group.set_editor_property('label_font_size',17)
  label=sk.get(prefix+'RowLabel');font=label.get_editor_property('font');font.size=18;label.set_font(font)
 switcher=sk.get('TabSwitcher')
 assert [switcher.get_child_at(i).get_name() for i in range(switcher.get_children_count())]==['Settings'+n+'Scroll' for n in pages]
 switcher.set_active_widget_index(0)
 report={}
 for name in ['WBP_OptionsWindow','WBP_KeyBindingsWindow']:
  path=k.BASE+name;c=p.B.compile_blueprint(path);assert c.success and not c.num_errors,c.export_text();assert p.E.save_asset(path,False);report[name]=c.export_text()
 (out/'compile.json').write_text(json.dumps(report,indent=2));print(report)
if __name__=='__main__':apply()
