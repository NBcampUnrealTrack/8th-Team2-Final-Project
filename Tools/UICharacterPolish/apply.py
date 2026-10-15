"""Character menu production polish. Run with PIE stopped through authenticated Revvy.
Reference is visual only; no production material samples the uploaded screenshot.
"""
import unreal,json,re,shutil
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
FOLDER='/Game/Developers/MinkiCho/UI/CharacterMenu/'
ART='/Game/Developers/MinkiCho/Art/CharacterMenu/Polish20261014'
E=unreal.EditorAssetLibrary;M=unreal.MaterialEditingLibrary

def color(h,alpha=1):
 v=[int(h[i:i+2],16)/255 for i in (0,2,4)]
 v=[x/12.92 if x<=.04045 else ((x+.055)/1.055)**2.4 for x in v]
 return unreal.LinearColor(*v,alpha)
NAVY=color('1E3443');IVORY=color('F3EEE3');MIST=color('DCE2E3');CREAM=color('F5ECD5');GOLD=color('B9A374');RED=color('8E3D49');MUTED=color('667681')

def alpha(c,a):return unreal.LinearColor(c.r,c.g,c.b,a)
def rect(w,x,y,ww,hh):
 if isinstance(w.slot,unreal.CanvasPanelSlot):
  w.slot.set_position(unreal.Vector2D(x,y));w.slot.set_size(unreal.Vector2D(ww,hh))
def material(name,code):
 p=ART+'/'+name;mat=unreal.load_asset(p)
 if not mat:mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ART,unreal.Material,unreal.MaterialFactoryNew())
 mat.modify();M.delete_all_material_expressions(mat)
 mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 uv=M.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-600,0)
 custom=M.create_material_expression(mat,unreal.MaterialExpressionCustom,-300,0)
 ci=unreal.CustomInput();ci.set_editor_property('input_name','UV');custom.set_editor_property('inputs',[ci]);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4);custom.set_editor_property('code',code)
 M.connect_material_expressions(uv,'',custom,'UV')
 for comp,prop in [('RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR),('A',unreal.MaterialProperty.MP_OPACITY)]:
  mask=M.create_material_expression(mat,unreal.MaterialExpressionComponentMask,0,0 if comp=='RGB' else 140)
  for c in ['r','g','b','a']:mask.set_editor_property(c,c.upper() in comp)
  M.connect_material_expressions(custom,'',mask,'');M.connect_material_property(mask,'',prop)
 M.recompile_material(mat);d=unreal.MaterialNodeService.get_material_diagnostics(p);assert d.success and d.is_compiled_ok,d.export_text();assert E.save_loaded_asset(mat)
 return mat

def rgb(c):return f'float3({c.r:.8f},{c.g:.8f},{c.b:.8f})'

def image_brush(w,c=None,res=None):
 b=w.get_editor_property('brush').copy()
 if c is not None:b.tint_color=unreal.SlateColor(specified_color=c)
 if res is not None:b.resource_object=res;b.draw_as=unreal.SlateBrushDrawType.IMAGE
 w.set_brush(b);w.set_color_and_opacity(unreal.LinearColor(1,1,1,1))

def palette(c):
 # Preserve semantic saturated attribute/rarity colors; remap common UI tokens.
 r,g,b,a=c.r,c.g,c.b,c.a
 if max(r,g,b)<.12:return alpha(NAVY,a)
 if r>.45 and g>.35 and b<g*.85:return alpha(GOLD,a)
 if min(r,g,b)>.6:return alpha(IVORY,a)
 if b>r*1.1 and min(r,g,b)>.35:return alpha(MIST,a)
 if max(r,g,b)-min(r,g,b)<.12:return alpha(MUTED,a)
 return c

def is_light_text(n,anc):
 return ('RibbonText' in n or 'Heading' in n and any('Ribbon' in x for x in anc) or n.startswith('Nav') or
         any(x.endswith('ButtonContent') for x in anc) or n.startswith('Equip') and n.endswith('Empty'))

def apply():
 assert not unreal.WidgetService.is_pie_running()
 cardcode='float2 size=1/max(fwidth(UV),float2(.00001,.00001));float2 p=UV*size;float2 e=min(p,size-p);float d=min(min(e.x,e.y),e.x+e.y-7);float a=saturate(d+.5);float edge=1-smoothstep(1,2,d);float inner=(1-smoothstep(.5,1.5,abs(d-5)))*.24;return float4(lerp('+rgb(IVORY)+','+rgb(GOLD)+',max(edge,inner)),a);'
 card=material('M_CharacterMenuCard',cardcode)
 # Simple armor silhouette, separate from navigation label and button state.
 armorcode='float2 p=UV;float body=step(.26,p.x)*step(p.x,.74)*step(.28,p.y)*step(p.y,.82);float sleeves=step(.12,p.x)*step(p.x,.88)*step(.20,p.y)*step(p.y,.40);float neck=step(.39,p.x)*step(p.x,.61)*step(p.y,.32);float a=saturate(max(body,sleeves)-neck);return float4(1,1,1,a);'
 armor=material('M_EquipmentArmorIcon',armorcode)
 out=ROOT/'Saved/UICharacterPolish';report=[]
 for path in unreal.WidgetService.list_widget_blueprints(FOLDER):
  path=str(path).split('.')[0];name=path.rsplit('/',1)[-1];bp=unreal.load_asset(path);bp.modify();tree=unreal.find_object(bp,'WidgetTree')
  objs={s.widget_name:unreal.find_object(tree,s.widget_name) for s in unreal.WidgetService.get_widget_snapshot(path)}
  def get(n):return objs.get(n)
  def ancestors(o):
   a=[];p=o.get_parent()
   while p:a.append(p.get_name());p=p.get_parent()
   return a
  for n,o in objs.items():
   if n.startswith('PolishShadow_'):continue
   o.modify()
   anc=ancestors(o)
   if isinstance(o,unreal.Image):
    b=o.get_editor_property('brush').copy();res=b.resource_object
    if not res:
     old=b.tint_color.specified_color;new=palette(old)
     if 'SelectedBody' in n or 'SelectedTip' in n:new=alpha(CREAM,old.a)
     elif n.endswith('Surface'):new=IVORY
     elif n.endswith('RibbonFill') or n in ['NavySurface','MenuTitleSurface','CloseFace','TraitsModeFill']:new=NAVY
     elif any(x in n for x in ['Edge','Frame','Corner','Diamond','Jewel']):new=alpha(GOLD,min(old.a,.65))
     elif n.endswith('Rule') or 'Separator' in n:new=alpha(GOLD,.28)
     b.tint_color=unreal.SlateColor(specified_color=new);o.set_brush(b)
    else:
     if n.endswith('Paper'):o.set_render_opacity(.055)
     elif 'Watermark' in n:o.set_render_opacity(.05)
     elif n=='CharacterIllustration':o.set_render_opacity(.17);o.set_color_and_opacity(alpha(MIST,1))
     elif n=='BackdropAtmosphere':o.set_render_opacity(.16)
     elif n in ['NavyGrain','TitleCompassOrbit']:o.set_render_opacity(.12)
     elif n in ['EquipmentCelestial','EquipmentFloor','MirrorFloorSurface']:o.set_render_opacity(.18)
     elif n=='CharacterReflection':o.set_render_opacity(.22)
     elif n.startswith('Ambient') or 'Leaf' in n:o.set_render_opacity(.18)
     elif n.startswith('JobEmblem') or n in ['ProfileCompass','InfoNameIcon','InfoJobIcon','InfoLevelIcon','InfoExperienceIcon']:image_brush(o,NAVY)
     elif n in ['RailCompass','RailFooterCompass']:image_brush(o,alpha(GOLD,.9))
    if n.endswith('Surface') and n not in ['IvorySurface','NavySurface','MenuTitleSurface','MirrorFloorSurface','NaiaPortraitFrameSurface']:
     image_brush(o,unreal.LinearColor(1,1,1,1),card)
   elif isinstance(o,unreal.TextBlock):
    old=o.get_editor_property('color_and_opacity').specified_color
    c=CREAM if is_light_text(n,anc) else NAVY
    parent=o.get_parent()
    while parent:
     if isinstance(parent,unreal.Button):
      tint=parent.get_editor_property('widget_style').normal.tint_color.specified_color
      c=CREAM if max(tint.r,tint.g,tint.b)<.3 else NAVY
      break
     parent=parent.get_parent()
    if n=='TraitsModeText':c=CREAM
    if n.startswith('Nav'):continue # Native navigation owns active/idle color.
    if n.startswith('Equip') and 'Value' in n:c=GOLD
    if any(x in n for x in ['Warning','Shortage','Unavailable']):c=RED
    o.set_color_and_opacity(unreal.SlateColor(specified_color=alpha(c,max(old.a,.78))))
    if isinstance(o.slot,unreal.SizeBoxSlot):
     if n.endswith('Value') or n.endswith('ValueText') or n in ['NameText','JobText','LevelText','ExperienceText','LocationText','CharacterIdText','GoldText','RequiredLevelText','RequiredParentText','PointCostText','EffectValueText']:
      o.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_RIGHT);o.set_editor_property('justification',unreal.TextJustify.LEFT)
     elif n.endswith('Label') and not any(x.endswith('ButtonContent') for x in anc) and not n.startswith('Nav'):
      o.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT);o.set_editor_property('justification',unreal.TextJustify.LEFT)
   elif isinstance(o,unreal.Button):
    if n in ['EquipActionButton','DismantleButton','LockButton','MoveToStorageButton','AcquireTraitButton']:o.set_render_opacity(.88)
    style=o.get_editor_property('widget_style').copy()
    for state in ['normal','hovered','pressed','disabled']:
     br=style.get_editor_property(state).copy();old=br.tint_color.specified_color
     if br.draw_as!=unreal.SlateBrushDrawType.NO_DRAW_TYPE and not br.resource_object:
      c=palette(old)
      if old.r<.12 and old.g<.12:c=alpha(NAVY,old.a)
      elif old.b>old.r*1.2 and old.b>.12:c=alpha(NAVY,old.a)
      if 'Dismantle' in n:c=alpha(RED,.78 if state=='normal' else .9)
      br.tint_color=unreal.SlateColor(specified_color=c)
      outline=br.outline_settings.copy();outline.color=unreal.SlateColor(specified_color=alpha(GOLD,.9 if state in ['hovered','pressed'] else .5));br.outline_settings=outline
      style.set_editor_property(state,br)
    o.set_style(style)
   elif isinstance(o,unreal.ProgressBar):
    style=o.get_editor_property('widget_style').copy()
    for state,c in [('background_image',alpha(MIST,1)),('fill_image',GOLD),('marquee_image',GOLD)]:
     br=style.get_editor_property(state).copy();br.tint_color=unreal.SlateColor(specified_color=c);style.set_editor_property(state,br)
    o.set_editor_property('widget_style',style);o.set_fill_color_and_opacity(unreal.LinearColor(1,1,1,1))
   elif isinstance(o,unreal.ScrollBox):
    try:
     st=o.get_editor_property('widget_bar_style').copy()
     for prop in ['normal_thumb_image','hovered_thumb_image','dragged_thumb_image']:
      br=st.get_editor_property(prop).copy();br.tint_color=unreal.SlateColor(specified_color=GOLD);st.set_editor_property(prop,br)
     o.set_editor_property('widget_bar_style',st)
    except Exception:pass
  cls=unreal.load_class(None,path+'.'+name+'_C');cdo=unreal.get_default_object(cls);cdo.modify()
  if name.endswith('MenuBase'):
   cdo.set_editor_property('selected_navigation_color',NAVY);cdo.set_editor_property('idle_navigation_color',IVORY)
   image_brush(get('NavEquipmentIcon'),IVORY,armor)
   image_brush(get('IvorySurface'),IVORY);image_brush(get('NavySurface'),NAVY);image_brush(get('MenuTitleSurface'),NAVY)
   for n in ['ProfileName','ProfileJob','ProfileLevel']:get(n).set_color_and_opacity(unreal.SlateColor(specified_color=NAVY))
  if name.endswith('EquipmentPage') or name.endswith('InventoryPage'):
   value=f'(R={GOLD.r},G={GOLD.g},B={GOLD.b},A=1)';R=unreal.ObjectReflectionService
   style=R.get_property(cdo.get_path_name(),'SlotStyle')
   style=re.sub(r'SelectedOutlineColor=\([^)]*\)', 'SelectedOutlineColor='+value,style)
   style=re.sub(r'SelectedCornerTint=\([^)]*\)', 'SelectedCornerTint='+value,style)
   style=re.sub(r'SelectedOutlineWidth=[0-9.]+','SelectedOutlineWidth=1.2',style)
   assert R.set_property(cdo.get_path_name(),'SlotStyle',style)
  if name.endswith('InfoPage'):
   for n in ['NameLabel','JobLabel','LevelLabel','ExperienceLabel','LocationLabel','IdentifierLabel','CurrencyLabel']:
    box=get('Align_'+n)
    if box:
     pos=box.slot.get_position();rect(box,84,pos.y,140,32)
   for n in ['InfoWatermarkCompass','InfoCornerTL','InfoCornerTR','InfoCornerBL','InfoCornerBR']:
    if get(n):get(n).set_render_opacity(.12)
  # Keep built-in traits coordinates/topology and all preview/gameplay bindings intact.
  result=unreal.BlueprintService.compile_blueprint(path);assert result.success,result.export_text()
  assert E.save_asset(path,False)
  report.append({'asset':path,'compile':result.export_text()})
 (out/'applied.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 print('POLISHED',len(report),'WBPs')
apply()




