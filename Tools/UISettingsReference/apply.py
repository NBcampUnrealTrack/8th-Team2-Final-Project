"""Author the navy/gold options and key-binding design without replacing bound controls.

Run apply() from the editor console outside PIE. Backups and readback are in Saved.
"""
import json
import re
import shutil
from datetime import datetime
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
W, B, E, R = unreal.WidgetService, unreal.BlueprintService, unreal.EditorAssetLibrary, unreal.ObjectReflectionService
BASE = '/Game/Developers/dbals/UI/'
FRAME = '/Game/Developers/MinkiCho/Art/Settings/Textures/T_Settings_Frame'
FONT = '/Engine/EngineFonts/Roboto'
WHITE = (0.84, .88, .93, 1)
GOLD = (.83, .64, .32, 1)
MUTED = (.40, .52, .64, 1)
NAVY = (.013, .029, .047, 1)
EDGE = (.19, .25, .30, 1)
ZERO = (0, 0, 0, 0)


def color(c):
    return '(R=%s,G=%s,B=%s,A=%s)' % c


def brush(fill=NAVY, edge=EDGE, radius=4, width=1):
    return ('(DrawAs=RoundedBox,ResourceObject=None,TintColor=(SpecifiedColor=%s),'
            'OutlineSettings=(CornerRadii=(X=%s,Y=%s,Z=%s,W=%s),RoundingType=FixedRadius,'
            'Color=(SpecifiedColor=%s),Width=%s))' % (color(fill),radius,radius,radius,radius,color(edge),width))


class Skin:
    def __init__(self, name):
        self.path = BASE+name
        self.bp = unreal.load_asset(self.path)
        assert self.bp, self.path
        self.tree = unreal.find_object(self.bp, 'WidgetTree')
        self.bp.modify(); self.tree.modify()

    def get(self, name):
        result = unreal.find_object(self.tree, name)
        assert result, (self.path, name)
        result.modify()
        return result

    def prop(self, name, key, value):
        assert W.set_property(self.path, name, key, str(value)), (name,key,value)

    def move(self, name, parent):
        item, target = self.get(name), self.get(parent)
        if item.get_parent() != target:
            assert target.add_child(item), (name,parent)
        return item

    def add(self, kind, name, parent):
        item = unreal.find_object(self.tree, name)
        if not item:
            result = W.add_component(self.path,kind,name,parent,True)
            assert result.success, result.error_message
        else:
            self.move(name,parent)
        item = self.get(name)
        item.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
        return item

    def rect(self, name, x, y, width, height, z=1):
        slot = self.get(name).slot
        slot.modify()
        slot.set_anchors(unreal.Anchors(unreal.Vector2D(0,0),unreal.Vector2D(0,0)))
        slot.set_alignment(unreal.Vector2D(0,0))
        slot.set_auto_size(False)
        slot.set_position(unreal.Vector2D(x,y)); slot.set_size(unreal.Vector2D(width,height)); slot.set_z_order(z)

    def centered(self,name,width,height):
        self.rect(name,0,0,width,height)
        slot=self.get(name).slot
        slot.set_anchors(unreal.Anchors(unreal.Vector2D(.5,.5),unreal.Vector2D(.5,.5)))
        slot.set_alignment(unreal.Vector2D(.5,.5))

    def relative(self,name,left,right,top=3,bottom=3,z=1):
        slot=self.get(name).slot
        slot.set_anchors(unreal.Anchors(unreal.Vector2D(left,0),unreal.Vector2D(right,1)))
        slot.set_alignment(unreal.Vector2D(0,0)); slot.set_auto_size(False)
        slot.set_offsets(unreal.Margin(0,top,0,bottom)); slot.set_z_order(z)

    def text(self,name,size=18,tint=WHITE,value=None,bold=False):
        item=self.get(name)
        font=item.get_editor_property('font').copy()
        font.size=size; font.typeface_font_name='Bold' if bold else 'Regular'
        item.set_font(font); item.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(*tint)))
        item.set_shadow_offset(unreal.Vector2D(0,0))
        item.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        if value is not None: item.set_text(value)

    def label(self,name,parent,rect,text,size=18,tint=WHITE,bold=False):
        self.add('TextBlock',name,parent); self.rect(name,*rect,3); self.text(name,size,tint,text,bold)

    def border(self,name,parent,rect,fill=NAVY,edge=EDGE,radius=3,width=1,z=0):
        self.add('Border',name,parent); self.rect(name,*rect,z)
        self.prop(name,'Background',brush(fill,edge,radius,width))
        self.prop(name,'BrushColor',color((1,1,1,1)))
        self.prop(name,'Padding','()')
        self.get(name).set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)

    def button(self,name,gold=False,flat=False):
        self.prop(name,'BackgroundColor',color((1,1,1,1)))
        for state,fill,edge in [('Normal',GOLD if gold else NAVY,GOLD if gold else EDGE),
                                ('Hovered',(.96,.78,.44,1) if gold else (.034,.065,.092,1),GOLD),
                                ('Pressed',(.57,.39,.15,1) if gold else (.008,.017,.028,1),GOLD),
                                ('Disabled',(.03,.04,.05,.45),(.12,.14,.16,.4))]:
            self.prop(name,'WidgetStyle.'+state,brush(ZERO if flat and state=='Normal' else fill,ZERO if flat else edge,5,0 if flat else 1))
        self.prop(name,'WidgetStyle.NormalPadding','(Left=12,Top=7,Right=12,Bottom=7)')
        self.prop(name,'WidgetStyle.PressedPadding','(Left=12,Top=8,Right=12,Bottom=6)')
        self.get(name).set_visibility(unreal.SlateVisibility.VISIBLE)

    def common(self,name,label=None,size=17,tint=WHITE):
        self.prop(name,'bIconOnly','True')
        self.prop(name,'bOverrideButtonSize','False')
        self.prop(name,'LabelFontSize',size)
        self.prop(name,'LabelColor',color(tint))
        if label is not None: self.prop(name,'LabelText',label)

    def frame(self,name):
        info=unreal.WidgetBrushInfo(); info.resource_path=FRAME; info.draw_as='Image';info.tint_color=color((1,1,1,1))
        assert W.set_brush(self.path,name,'Background',info)
        self.prop(name,'BrushColor',color((1,1,1,1)));self.prop(name,'Padding','()')


def options():
    s=Skin('WBP_OptionsWindow')
    s.add('CanvasPanel','SettingsStage','RootCanvas')
    s.move('ContentBox','SettingsStage');s.get('ContentBox').set_visibility(unreal.SlateVisibility.COLLAPSED)
    s.move('SettingsStage','FrameWidth')
    s.get('FrameWidth').set_width_override(1040);s.get('FrameWidth').set_height_override(700)
    s.centered('WindowFrame',1040,700);s.frame('WindowFrame')
    s.move('TitleText','SettingsStage');s.rect('TitleText',156,43,630,45);s.text('TitleText',28,WHITE,'옵션')
    s.border('SettingsHeaderRule','SettingsStage',(150,139,814,1),(.2,.26,.3,.55),ZERO,0,0)
    s.border('SettingsFooterRule','SettingsStage',(150,540,814,1),(.2,.26,.3,.55),ZERO,0,0)
    for i,tab in enumerate(['Display','Graphics','Sound','Controls']):
        button=tab+'TabButton';label=button+'Label';x=154+i*201
        s.move(button,'SettingsStage');s.rect(button,x,94,188,44,2);s.button(button,flat=True)
        s.text(label,19,MUTED)
        s.get(label).set_editor_property('justification',unreal.TextJustify.CENTER)
        s.border('SettingsTabMark'+str(i),'SettingsStage',(x+18,136,152,3),GOLD,ZERO,0,0,3)
        s.label('SettingsTabLabel'+str(i),'SettingsStage',(x,102,188,30),['화면','그래픽','사운드','조작'][i],19,GOLD,True)
        s.get('SettingsTabLabel'+str(i)).set_editor_property('justification',unreal.TextJustify.CENTER)
        for name in ['SettingsTabMark'+str(i),'SettingsTabLabel'+str(i)]:
            s.get(name).set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE if i==0 else unreal.SlateVisibility.COLLAPSED)
    s.move('TabSwitcher','SettingsStage');s.rect('TabSwitcher',164,161,806,369,1)
    s.get('TabSwitcher').set_active_widget_index(0)
    for row in W.get_widget_snapshot(s.path):
        if row.widget_class=='TextBlock' and row.widget_name.endswith('RowLabel'):
            s.text(row.widget_name,18)
        if row.widget_class=='SizeBox' and row.widget_name.endswith('LabelBox'):
            s.get(row.widget_name).set_width_override(225)
    for page in ['DisplayPage','SoundPage','ControlsPage']:
        s.prop(page,'Slot.Padding','(Left=12,Top=38,Right=38,Bottom=8)')
    for name in ['WindowMode','Resolution','VSync','FrameLimit']:
        s.prop(name+'Row','Slot.Padding','(Left=0,Top=8,Right=0,Bottom=18)')
    for name in ['WindowModeCombo','ResolutionCombo','FrameLimitCombo']:
        s.prop(name,'ContentPadding','(Left=16,Top=9,Right=12,Bottom=9)')
        s.prop(name,'ForegroundColor','(SpecifiedColor='+color(WHITE)+')')
        s.prop(name,'Font.Size',18)
        for state in ['Normal','Hovered','Pressed','Disabled']:
            s.prop(name,'WidgetStyle.ComboButtonStyle.ButtonStyle.'+state,brush(NAVY,GOLD if state=='Hovered' else EDGE,5))
        s.prop(name,'WidgetStyle.ComboButtonStyle.MenuBorderBrush',brush(NAVY,EDGE,4))
    for name in ['UncheckedImage','UncheckedHoveredImage','UncheckedPressedImage']:
        s.prop('VSyncCheck','WidgetStyle.'+name,brush(NAVY,EDGE,12))
        s.prop('VSyncCheck','WidgetStyle.'+name+'.ImageSize','(X=54,Y=28)')
    for name in ['CheckedImage','CheckedHoveredImage','CheckedPressedImage']:
        s.prop('VSyncCheck','WidgetStyle.'+name,brush(GOLD,GOLD,12))
        s.prop('VSyncCheck','WidgetStyle.'+name+'.ImageSize','(X=54,Y=28)')
    s.text('QualityRowLabel',17,GOLD,'그래픽 프리셋')
    s.prop('GraphicsPage','Slot.Padding','(Left=12,Top=6,Right=0,Bottom=0)')
    for name in ['Quality','ShadowQuality','GlobalIlluminationQuality','ReflectionQuality','PostProcessQuality','EffectsQuality','TextureQuality','ViewDistanceQuality','Upscale']:
        s.prop(name+'Row','Slot.Padding','(Left=0,Top=4,Right=0,Bottom=4)')
        s.get(name+'LabelBox').set_width_override(218)
        s.prop(name+'Group','LabelFontSize',15)
    s.text('ShadowQualityRowLabel',17,WHITE,'그림자 품질');s.text('UpscaleRowLabel',17,WHITE,'업스케일링')
    s.get('UpscaleGap').set_size(unreal.Vector2D(0,7))
    s.text('MasterVolumeRowLabel',18,WHITE,'마스터 음량')
    s.text('BgmVolumeRowLabel',18,WHITE,'배경 음악')
    s.text('SoundNote',14,MUTED,'효과음 조절은 준비 중입니다.')
    for prefix in ['MasterVolume','BgmVolume','SfxVolume']:
        s.prop(prefix+'Row','Slot.Padding','(Left=0,Top=22,Right=0,Bottom=22)')
        slider=prefix+'Slider'
        s.prop(slider,'SliderBarColor',color((.25,.32,.40,1)))
        s.prop(slider,'SliderHandleColor',color(GOLD))
        for state in ['Normal','Hovered','Disabled']:
            s.prop(slider,'WidgetStyle.'+state+'ThumbImage',brush((1,1,1,1),GOLD,4))
            s.prop(slider,'WidgetStyle.'+state+'ThumbImage.ImageSize','(X=19,Y=26)')
            s.prop(slider,'WidgetStyle.'+state+'BarImage',brush((1,1,1,1),ZERO,2,0))
        s.prop(slider,'WidgetStyle.BarThickness',5)
    for name in ['MasterVolumeValueText','BgmVolumeValueText']:s.text(name,16)
    # A full-width existing key-settings button; unavailable features are clearly marked.
    s.get('KeyBindingsRowLabel').set_visibility(unreal.SlateVisibility.COLLAPSED)
    s.get('KeyBindingsLabelBox').set_visibility(unreal.SlateVisibility.COLLAPSED)
    s.button('KeyBindingsButton');s.text('KeyBindingsButtonLabel',20,WHITE,'키보드 · 마우스 · 게임패드 키 설정  ›')
    s.prop('KeyBindingsButton','Slot.Size','(SizeRule=Fill,Value=1)')
    s.prop('KeyBindingsButton','WidgetStyle.NormalPadding','(Left=20,Top=20,Right=20,Bottom=20)')
    s.prop('KeyBindingsButton','WidgetStyle.PressedPadding','(Left=20,Top=21,Right=20,Bottom=19)')
    s.add('TextBlock','SettingsControlsHelp','ControlsPage')
    s.text('SettingsControlsHelp',16,MUTED,'이동, 전투, 상호작용에 사용할 키를 변경합니다.\n\n마우스 감도 · 게임패드 입력 방식 설정은 준비 중입니다.')
    s.prop('SettingsControlsHelp','Slot.Padding','(Left=18,Top=22,Right=0,Bottom=0)')
    for name,x,width,gold,label in [('DefaultsButton',150,208,False,'기본값으로 복원'),('CloseButton',686,116,False,'닫기'),('ApplyButton',816,148,True,'적용')]:
        s.move(name,'SettingsStage');s.rect(name,x,560,width,42,3);s.button(name,gold)
        s.text(name+'Label',17,NAVY if gold else WHITE,label)
    s.add('Button','HeaderCloseButton','SettingsStage');s.rect('HeaderCloseButton',920,41,40,40,4);s.button('HeaderCloseButton',flat=True)
    s.add('TextBlock','HeaderCloseGlyph','HeaderCloseButton');s.text('HeaderCloseGlyph',28,GOLD,'×')
    s.get('HeaderCloseGlyph').set_editor_property('justification',unreal.TextJustify.CENTER)
    return s


def radio():
    s=Skin('WBP_OptionRadioGroup')
    for i in range(5):
        name='Option'+str(i);cell='OptionCell'+str(i)
        s.common(name,size=15)
        s.prop(name,'bOverrideButtonSize','True');s.prop(name,'ButtonSize','(X=104,Y=29)')
        s.add('Border','SettingsRadioBase'+str(i),cell)
        s.prop('SettingsRadioBase'+str(i),'Background',brush((.025,.046,.067,.85),(.07,.10,.14,.6),0,1))
        s.prop('SettingsRadioBase'+str(i),'Slot.HorizontalAlignment','HAlign_Fill')
        s.prop('SettingsRadioBase'+str(i),'Slot.VerticalAlignment','VAlign_Fill')
        s.prop('SettingsRadioBase'+str(i),'Slot.Padding','()')
        s.get('SettingsRadioBase'+str(i)).set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        parent=s.get(cell)
        slots=[parent.get_child_at(j).slot.get_path_name() for j in range(parent.get_children_count())]
        new_slot=s.get('SettingsRadioBase'+str(i)).slot.get_path_name();slots.remove(new_slot);slots.insert(0,new_slot)
        assert R.set_property(parent.get_path_name(),'Slots','('+','.join('"'+p+'"' for p in slots)+')')
        s.prop('SelectedMark'+str(i),'Background',brush((.35,.25,.09,.12),GOLD,0,1.2))
        s.prop('SelectedMark'+str(i),'BrushColor',color((1,1,1,1)))
        s.get('SelectedBar'+str(i)).set_visibility(unreal.SlateVisibility.COLLAPSED)
    return s


def key_window():
    s=Skin('WBP_KeyBindingsWindow')
    s.add('CanvasPanel','SettingsStage','Root');s.centered('SettingsStage',1200,820)
    s.move('WindowBG','SettingsStage');s.rect('WindowBG',0,0,1200,820,0);s.frame('WindowBG')
    s.move('TitleText','SettingsStage');s.rect('TitleText',156,46,800,48,2);s.text('TitleText',29,WHITE,'키 설정')
    s.move('CloseButtonBox','SettingsStage');s.rect('CloseButtonBox',1100,60,34,34,3)
    s.get('CloseButtonBox').set_width_override(34);s.get('CloseButtonBox').set_height_override(34)
    s.get('CloseButton').set_render_scale(unreal.Vector2D(.65,.65))
    s.get('CloseButton').set_color_and_opacity(unreal.LinearColor(*GOLD))
    s.border('SettingsHeaderRule','SettingsStage',(150,111,1000,1),(.2,.26,.3,.65),ZERO,0,0)
    for name,x,width,label in [('ActionHeading',168,370,'행동'),('KeyboardHeading',608,255,'키보드 · 마우스'),('GamepadHeading',910,235,'게임패드')]:
        s.label('Settings'+name,'SettingsStage',(x,127,width,28),label,18,WHITE)
        if name!='ActionHeading':s.get('Settings'+name).set_editor_property('justification',unreal.TextJustify.CENTER)
    s.move('RowScroll','SettingsStage');s.rect('RowScroll',152,165,1000,483,2)
    s.prop('RowScroll','ScrollbarThickness','(X=5,Y=5)')
    s.prop('RowScroll','ScrollbarPadding','(Left=8,Top=0,Right=0,Bottom=0)')
    s.prop('RowScroll','WidgetBarStyle.NormalThumbImage',brush(GOLD,ZERO,2,0))
    s.prop('RowScroll','WidgetBarStyle.HoveredThumbImage',brush(GOLD,ZERO,2,0))
    s.prop('RowScroll','WidgetBarStyle.DraggedThumbImage',brush(GOLD,ZERO,2,0))
    s.border('SettingsFooterRule','SettingsStage',(150,662,1000,1),(.2,.26,.3,.65),ZERO,0,0)
    s.move('GuideText','SettingsStage');s.rect('GuideText',154,675,984,24,2);s.text('GuideText',14,MUTED)
    for name,box,x,width,gold,label in [('DefaultsButton','DefaultsButtonBox',150,222,False,'기본값으로 복원'),('ApplyButton','ApplyButtonBox',1002,150,True,'적용')]:
        s.move(box,'SettingsStage');s.rect(box,x,711,width,38,3)
        s.get(box).set_width_override(width);s.get(box).set_height_override(38)
        s.common(name,label,17,NAVY if gold else WHITE)
        s.border('Settings'+name+'BG','SettingsStage',(x,711,width,38),GOLD if gold else NAVY,GOLD,5,1,2)
    s.add('Button','FooterCloseButton','SettingsStage');s.rect('FooterCloseButton',866,711,120,38,3);s.button('FooterCloseButton')
    s.add('TextBlock','FooterCloseLabel','FooterCloseButton');s.text('FooterCloseLabel',17,WHITE,'취소')
    s.get('FooterCloseLabel').set_editor_property('justification',unreal.TextJustify.CENTER)
    return s


def key_row():
    s=Skin('WBP_KeyBindingRow')
    s.get('RowLine').remove_from_parent()
    s.add('CanvasPanel','SettingsRowStage','RowSizeBox')
    # AddChild replaces the single SizeBox content; preserve the old structure as a hidden child.
    s.move('RowLine','SettingsRowStage');s.get('RowLine').set_visibility(unreal.SlateVisibility.COLLAPSED)
    s.get('RowSizeBox').set_height_override(46)
    s.move('NameText','SettingsRowStage');s.relative('NameText',.012,.43,10,2,2);s.text('NameText',18)
    for prefix,left,right,overlay,button in [('Key',.445,.695,'KeyOverlay','ChangeButton'),('Pad',.735,.985,'PadKeyOverlay','PadChangeButton')]:
        bg='Settings'+prefix+'Cell'
        s.add('Border',bg,'SettingsRowStage');s.relative(bg,left,right,3,3,0)
        s.prop(bg,'Background',brush(NAVY,EDGE,4));s.prop(bg,'Padding','()')
        s.get(bg).set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        s.move(button,'SettingsRowStage');s.relative(button,left,right,3,3,1);s.common(button,'',17)
        s.move(overlay,'SettingsRowStage');s.relative(overlay,left,right,3,3,2)
        s.get(overlay).set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    s.text('KeyText',18,NAVY,bold=True);s.text('PadKeyText',17,(.46,.62,.82,1))
    s.prop('KeyboardKeycap','Background',brush((.82,.74,.59,1),(.56,.43,.25,1),3))
    s.prop('KeyboardKeycap','BrushColor',color((1,1,1,1)));s.prop('KeyboardKeycap','Padding','(Left=12,Top=2,Right=12,Bottom=2)')
    s.prop('GamepadKeycap','Background',brush(ZERO,ZERO,0,0));s.prop('GamepadKeycap','Padding','(Left=4,Top=2,Right=4,Bottom=2)')
    for name in ['EmptyText','PadEmptyText']:
        s.text(name,16,(.86,.32,.25,1));s.get(name).set_editor_property('justification',unreal.TextJustify.CENTER)
        s.get(name).set_visibility(unreal.SlateVisibility.COLLAPSED)
    for name,text in [('WaitingBox','WaitingText'),('PadWaitingBox','PadWaitingText')]:
        s.prop(name,'Background',brush((.07,.07,.04,1),GOLD,3))
        s.prop(name,'BrushColor',color((1,1,1,1)))
        s.text(text,14,GOLD);s.get(text).set_editor_property('justification',unreal.TextJustify.CENTER)
        s.get(name).set_visibility(unreal.SlateVisibility.COLLAPSED)
    for name,left,right in [('ChangedBox',.683,.694),('PadChangedBox',.973,.984)]:
        s.move(name,'SettingsRowStage');s.relative(name,left,right,4,32,4)
        s.prop(name,'BrushColor',color(GOLD));s.get(name).set_visibility(unreal.SlateVisibility.COLLAPSED)
    s.add('Border','SettingsRowRule','SettingsRowStage');s.relative('SettingsRowRule',0,1,45,0,0)
    s.prop('SettingsRowRule','Background',brush((.13,.18,.23,.55),ZERO,0,0))
    s.get('SettingsRowRule').set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    return s


def key_group():
    s=Skin('WBP_KeyBindingGroup')
    s.get('GroupSizeBox').set_height_override(38)
    s.prop('GroupBG','Background',brush((.036,.065,.095,.85),(.18,.24,.29,.65),0,1))
    s.prop('GroupBG','BrushColor',color((1,1,1,1)))
    s.prop('GroupBG','Padding','(Left=14,Top=6,Right=12,Bottom=4)')
    s.text('GroupText',18,GOLD)
    return s


def apply():
    assert not W.is_pie_running(), 'Stop PIE before authoring.'
    folder=ROOT/'Saved/UISettingsReference'/datetime.now().strftime('%Y%m%d_%H%M%S')
    folder.mkdir(parents=True)
    names=['WBP_OptionsWindow','WBP_KeyBindingsWindow','WBP_KeyBindingRow','WBP_KeyBindingGroup','WBP_OptionRadioGroup']
    before={}
    for name in names:
        p=BASE+name
        shutil.copy2(ROOT/'Content/Developers/dbals/UI'/f'{name}.uasset',folder/(name+'.uasset'))
        before[name]={'guids':R.get_property(p,'WidgetVariableNameToGuidMap'),'bindings':R.get_property(p,'Bindings'),
                      'widgets':{x.widget_name:x.widget_class for x in W.get_widget_snapshot(p)}}
    (folder/'before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
    if not E.does_asset_exist(FRAME):
        imported,error=unreal.AssetDiscoveryService.import_asset(str(ROOT/'ArtSource/UI/SettingsReference/T_Settings_Frame.png'),FRAME.rsplit('/',1)[0],FRAME.rsplit('/',1)[1])
        assert imported,error
    frame=unreal.load_asset(FRAME)
    frame.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    frame.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    frame.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    frame.set_editor_property('srgb',True);frame.set_editor_property('never_stream',True)
    frame.set_editor_property('compression_no_alpha',False)
    assert E.save_loaded_asset(frame)
    result={}
    with unreal.ScopedEditorTransaction('Settings windows: navy and gold reference design'):
        for fn in [radio,key_group,key_row,options,key_window]:
            s=fn();name=s.path.rsplit('/',1)[1]
            current={x.widget_name:x.widget_class for x in W.get_widget_snapshot(s.path)}
            assert all(current.get(n)==kind for n,kind in before[name]['widgets'].items()),'Bound widget identity changed: '+name
            assert R.get_property(s.path,'Bindings')==before[name]['bindings'],'Existing bindings changed: '+name
            compiled=B.compile_blueprint(s.path)
            assert compiled.success and compiled.num_errors==0,compiled.export_text()
            guids=R.get_property(s.path,'WidgetVariableNameToGuidMap')
            for n,g in re.findall(r'\("([^\"]+)"\s*,\s*([0-9A-Fa-f]+)\)',before[name]['guids']):
                assert n in guids and g in guids,('GUID changed',n)
            assert E.save_asset(s.path,False),s.path
            result[name]={'errors':compiled.num_errors,'warnings':compiled.num_warnings,'widgets':len(current)}
    (folder/'result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result))
    return result
