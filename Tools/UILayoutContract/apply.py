"""Canonical panel-local layout and single-owner hub row rendering. Run LAST."""
import unreal, importlib.util, json, shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
def module(name,path):
 s=importlib.util.spec_from_file_location(name,ROOT/path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
k=module('contract_key','Tools/UISettingsReference/apply_key_polish.py');p=k.p
d=module('contract_depth','Tools/UIHubSidePanels/apply_depth.py');h=d.h;M=h.M

def font(o,size):
 f=o.get_editor_property('font').copy();f.size=size;o.set_font(f);o.set_render_translation(unreal.Vector2D())

def panels():
 opt=k.s.Skin('WBP_OptionsWindow');opt.get('WindowFrame').set_padding(unreal.Margin())
 # A single panel-local origin, no inherited 42/30 padding compensation.
 opt.get('FrameWidth').slot.set_padding(unreal.Margin())
 for name,w,hh,content in [('WBP_OptionsWindow',1040,700,'TabSwitcher'),('WBP_KeyBindingsWindow',1200,820,'RowScroll')]:
  sk=k.s.Skin(name);left=round(w*.085);right=w-left;header=round(w*.07);top=round(hh*.105)
  sk.rect('SettingsGearIcon',header,top+9,30,30,4);sk.rect('TitleText',header+42,top,right-header-90,48,4);font(sk.get('TitleText'),28)
  close='HeaderCloseButton' if name== 'WBP_OptionsWindow' else 'CloseButtonBox'
  sk.rect(close,right-40,top+4,40,40,4)
  if close.endswith('Box'):sk.get(close).set_width_override(40);sk.get(close).set_height_override(40)
  bodytop=round(hh*.285);bottom=hh-(194 if name=='WBP_KeyBindingsWindow' else 160)
  sk.rect(content,left,bodytop,right-left,bottom-bodytop,2)
  sk.rect('SettingsFooterRule',left,hh-144,right-left,1,2)
  if name=='WBP_OptionsWindow':
   sk.rect('SettingsHeaderRule',left,bodytop-24,right-left,2,2)
   # Align all tab buttons and selection underlines to the same four columns.
   stage=sk.get('SettingsStage')
   tabs=[]
   for i in range(stage.get_children_count()):
    o=stage.get_child_at(i)
    if 'Tab' in o.get_name() and isinstance(o.slot,unreal.CanvasPanelSlot) and o.get_name()!='TabSwitcher':tabs.append(o)
   for o in tabs:
    pos=o.slot.get_position();size=o.slot.get_size();idx=min(3,max(0,round((pos.x-76)/201)))
    o.slot.set_position(unreal.Vector2D(left+idx*(right-left)/4+(0 if size.y>5 else 32),(bodytop-63 if 'TabLabel' in o.get_name() else bodytop-72) if size.y>5 else bodytop-36))
    o.slot.set_size(unreal.Vector2D((right-left)/4-(0 if size.y>5 else 64),size.y))
   for n,x,width in [('DefaultsButton',left,222),('ApplyButton',right-316,150),('CloseButton',right-150,150)]:
    sk.rect(n,x,hh-124,width,44,3);label=sk.get(n+'Label');font(label,18)
    label.slot.set_padding(unreal.Margin());label.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);label.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
  else:
   # Column labels retain exact fractions of the native 986px row width.
   sk.rect('SettingsHeaderRule',left,bodytop-24,right-left,2,2)
   for n,a,b in [('Action',.012,.43),('Keyboard',.445,.695),('Gamepad',.735,.985)]:sk.rect('Settings'+n+'Heading',left+986*a,bodytop-62,986*(b-a),30,3)
   sk.rect('GuideText',left,hh-182,right-left,26,3)
   for n,x,width in [('DefaultsButtonBox',left,222),('ApplyButtonBox',right-316,150),('FooterCloseButton',right-150,150)]:
    sk.rect(n,x,hh-124,width,44,3)
    if n.endswith('Box'):sk.get(n).set_width_override(width);sk.get(n).set_height_override(44)
   for n,source,kind in [('DefaultsButton','DefaultsButton','Secondary'),('ApplyButton','ApplyButton','Primary')]:
    old=p.PRIVATE+'/WBP_KeySettings'+kind;new=p.PRIVATE+'/WBP_SettingsFooter'+kind
    if not p.E.does_asset_exist(new):assert p.E.duplicate_asset(old,new)
    bp=unreal.load_asset(new);bp.modify();t=unreal.find_object(bp,'WidgetTree')
    st=opt.get(source).get_editor_property('widget_style').copy();unreal.find_object(t,'Btn_Common').set_style(st)
    txt=unreal.find_object(t,'Txt_Label');font(txt,18);txt.slot.set_padding(unreal.Margin())
    assert p.B.compile_blueprint(new).success;assert p.E.save_asset(new,False)
    o=k.replace(sk,n,'WBP_SettingsFooter'+kind);o.set_editor_property('bIconOnly',False);o.set_editor_property('bOverrideButtonSize',False);o.set_editor_property('LabelFontSize',18)
   sk.get('FooterCloseButton').set_style(opt.get('CloseButton').get_editor_property('widget_style').copy())
   sk.get('FooterCloseLabel').set_text('닫기');font(sk.get('FooterCloseLabel'),18)
  assert p.B.compile_blueprint(sk.path).success;assert p.E.save_asset(sk.path,False)

def row_material(state):
 a=h.mat('M_Hub_ContractRow_'+state);uv=M.create_material_expression(a,unreal.MaterialExpressionTextureCoordinate,-700,0)
 row=M.create_material_expression(a,unreal.MaterialExpressionScalarParameter,-700,180);row.set_editor_property('parameter_name','RowIndex')
 # One complete first-row frame for ALL items. Other atlas rows contribute icons only.
 code='float2 q=float2(47,142)+UV*float2(385,72);if(q.x>76&&q.x<354)q.x=300;return q/float2(1672,941);'
 mapped=h.custom(a,code,unreal.CustomMaterialOutputType.CMOT_FLOAT2,[('UV',uv,''),('RowIndex',row,'')])
 sample=d.r.sample(a,unreal.load_asset(h.ART+'/T_Hub_CleanHeaderProfile'));M.connect_material_expressions(mapped,'',sample,'UVs')
 iconuv=h.custom(a,'return (float2(47,142+RowIndex*68.5)+UV*float2(385,72))/float2(1672,941);',unreal.CustomMaterialOutputType.CMOT_FLOAT2,[('UV',uv,''),('RowIndex',row,'')])
 icons=d.r.sample(a,unreal.load_asset(h.ART+'/T_Hub_CleanHeaderProfile'));M.connect_material_expressions(iconuv,'',icons,'UVs')
 combined=h.custom(a,'float2 q=float2(47,142)+UV*float2(385,72);float inside=step(78,q.x)*step(q.x,128)*step(152,q.y)*step(q.y,204);float glyph=smoothstep(.09,.22,dot(I,float3(.2126,.7152,.0722)))*inside;return lerp(C,I,glyph);',unreal.CustomMaterialOutputType.CMOT_FLOAT3,[('UV',uv,''),('C',sample,'RGB'),('I',icons,'RGB')])
 code=d.p.COOL
 if state!='normal':
  code='float l=dot(C,float3(.2126,.7152,.0722));float ink=smoothstep(.10,.32,l);return lerp(float3(.70,.64,.51),float3(.11,.16,.20),ink);'
  if state=='pressed':code=code.replace('return lerp','return .88*lerp')
 color=h.custom(a,code,unreal.CustomMaterialOutputType.CMOT_FLOAT3,[('C',combined,'')])
 # Expand the silhouette beyond the complete border, including the bottom rail.
 mask=h.custom(a,'float2 p=UV*float2(385,72);float y=p.y-2;float e[9]={380,391,398,404,412,406,400,394,381};float t=clamp(y/8.5,0,8);int j=min((int)t,7);float r=lerp(e[j],e[j+1],t-j)-47+1;float l=19*smoothstep(0,34,abs(y-34))-1;float dist=min(min(p.x-l,r-p.x),min(p.y-1,71-p.y));return saturate(dist/max(.6,fwidth(dist)));',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('UV',uv,'')])
 M.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY);h.finishmat(a);return a

def hub():
 # Keep the noisy fade but remove all baked rows. Each Button now draws itself.
 dst=h.ART+'/M_Hub_ContractRail'
 if not h.E.does_asset_exist(dst):assert h.E.duplicate_asset(h.ART+'/M_Hub_DepthRail',dst)
 graph=json.loads(unreal.MaterialNodeService.export_material_graph(dst))
 for e in graph['expressions']:
  if e['class']=='Custom' and 'float header=Mask' in e['properties']['Code']:
   code='float2 q=UV*float2(740,899);float header=Mask*smoothstep(.10,.22,max(C.r,max(C.g,C.b)))*(1-step(144,q.y));if(q.x>132&&q.y<92)header=0;float footer=step(829,q.y)*step(q.y,873)*step(52,q.x)*step(q.x,545);if(q.y>854&&((q.x>105&&q.x<199)||(q.x>241&&q.x<394)||q.x>442))footer=0;float ink=smoothstep(.035,.10,dot(C,float3(.2126,.7152,.0722)));return max(header,footer*ink);'
   assert unreal.MaterialNodeService.set_expression_property(dst,e['id'],'Code',code)
 h.finishmat(unreal.load_asset(dst));bp,t=h.tree(h.MAIN);h.obj(t,'HubLeftBackplate').set_brush_from_material(unreal.load_asset(dst))
 view=h.obj(t,'MainMenuTileView');h.rect(view,47,128,385,692);view.set_editor_property('entry_height',76);view.set_editor_property('vertical_entry_spacing',4);view.set_editor_property('bEntrySizeIncludesEntrySpacing',True)
 title=h.obj(t,'HubBannerProject');h.rect(title,126,40,330,44);font(title,24)
 fit=h.obj(t,'HubFit');fit.slot.set_offsets(unreal.Margin(0,24,24,0))
 for n,x,w in [('OptionsButton',44,123),('CharacterSelectButton',185,186),('QuitGameButton',385,149)]:h.rect(h.obj(t,n),x,825,w,49)
 assert h.B.compile_blueprint(h.MAIN).success;assert h.E.save_asset(h.MAIN,False)
 mats={s:row_material(s) for s in ['normal','hovered','pressed']}
 bp,t=h.tree(h.TILE);btn=h.obj(t,'Btn_Common');btn.set_render_transform_pivot(unreal.Vector2D(0,.5));btn.set_render_scale(unreal.Vector2D(.75,.75));st=btn.get_editor_property('widget_style').copy()
 for state,mat in mats.items():setattr(st,state,h.brush(mat))
 st.disabled=h.brush(mats['normal']);st.normal_padding=unreal.Margin();st.pressed_padding=unreal.Margin()
 st.normal_foreground=p.slate('text');st.hovered_foreground=p.slate('ink');st.pressed_foreground=p.slate('ink');btn.set_style(st)
 txt=h.obj(t,'Txt_Label');txt.set_render_opacity(1);txt.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE);font(txt,22);h.rect(txt,100,0,254,72)
 # Center using auto-size at a center-height anchor, not a font-specific y correction.
 txt.slot.set_anchors(unreal.Anchors(unreal.Vector2D(0,.5),unreal.Vector2D(0,.5)));txt.slot.set_alignment(unreal.Vector2D(0,.5));txt.slot.set_position(unreal.Vector2D(100,0));txt.slot.set_auto_size(True)
 for n in ['Img_Icon','HubStatBar1','HubStatBar2','HubStatBar3','HubStatBaseline','ShortcutKeycap']:h.obj(t,n).set_render_opacity(0)
 c=unreal.get_default_object(bp.generated_class());c.set_editor_property('bOverrideButtonSize',False);c.set_editor_property('LabelFontSize',22)
 assert h.B.compile_blueprint(h.TILE).success;assert h.E.save_asset(h.TILE,False)
 assert h.B.compile_blueprint(h.MAIN).success;assert h.E.save_asset(h.MAIN,False)

def apply():
 assert not p.W.is_pie_running()
 out=ROOT/'Saved/UILayoutContract'/datetime.now().strftime('%Y%m%d_%H%M%S');out.mkdir(parents=True)
 for path in [p.PATH,k.BASE+'WBP_KeyBindingsWindow',h.MAIN,h.TILE]:shutil.copy2(ROOT/'Content'/(path[6:]+'.uasset'),out/(path.split('/')[-1]+'.uasset'))
 panels();hub();module('optical_final','Tools/UILayoutContract/align_optical.py').apply();print('Layout contract applied',out)
if __name__=='__main__':apply()
