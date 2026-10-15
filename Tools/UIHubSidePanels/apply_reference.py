"""Reference-2 correction: preserve original concept pixels, live data and actions."""
import unreal,importlib.util,json,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
spec=importlib.util.spec_from_file_location('hub_helpers',ROOT/'Tools/UIHubSidePanels/apply.py')
h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)
h.ART='/Game/Developers/MinkiCho/Art/UITheme/HubReference2'
OUT=ROOT/'Saved/UIHubReferenceExact';OUT.mkdir(exist_ok=True)
E=h.E;M=h.M;W=h.W;B=h.B

def polygon(points):
 # Convex/concave polygon mask in reference pixel coordinates, plus soft outer edge.
 s='float inside=0; float dist=10000; float2 a,b,e,v; float c;'
 for a,b in zip(points,points[1:]+points[:1]):
  s+=f'a=float2{a};b=float2{b};e=b-a;v=p-a;dist=min(dist,length(v-e*saturate(dot(v,e)/dot(e,e))));'
  s+='if((a.y>p.y)!=(b.y>p.y)) { c=(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x; if(p.x<c) inside=1-inside; }'
 return s+'return inside*saturate(dist/1.0);'

def sample(a,tex):
 n=M.create_material_expression(a,unreal.MaterialExpressionTextureSample,-500,0);n.set_editor_property('texture',tex);return n

def apply():
 assert not W.is_pie_running()
 backup=OUT/datetime.now().strftime('%Y%m%d_%H%M%S');backup.mkdir()
 bindings={}
 for p in [h.MAIN,h.TILE,h.SYSTEM]:
  shutil.copy2(ROOT/'Content'/(p[6:]+'.uasset'),backup/(p.rsplit('/',1)[1]+'.uasset'))
  bindings[p]=unreal.ObjectReflectionService.get_property(p,'Bindings')
 textures={}
 for name in ['T_Hub_Reference2','T_Hub_ProfileClean']:
  p=h.ART+'/'+name
  if not E.does_asset_exist(p):
   imported,error=unreal.AssetDiscoveryService.import_asset(str(ROOT/'ArtSource/UI/HubSidePanels'/(name+'.png')),h.ART,name);assert imported,error
  tex=unreal.load_asset(p)
  for k,v in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('srgb',True),('compression_no_alpha',False),('never_stream',True)]:tex.set_editor_property(k,v)
  assert E.save_loaded_asset(tex);textures[name]=tex
 a=h.mat('M_Hub_Reference2');s=sample(a,textures['T_Hub_Reference2']);clean=sample(a,textures['T_Hub_ProfileClean'])
 uv=M.create_material_expression(a,unreal.MaterialExpressionTextureCoordinate,-600,400)
 left=[(0,0),(447,0),(420,140),(433,182),(405,215),(394,340),(391,465),(411,632),(421,775),(549,795),(568,838),(561,876),(545,899),(0,899)]
 right=[(1024,180),(1100,171),(1320,160),(1510,147),(1560,146),(1600,142),(1672,139),(1672,424),(1625,430),(1585,427),(1540,446),(1430,436),(1340,426),(1250,416),(1115,408),(1040,405),(1019,378),(1019,240),(1017,195)]
 # Bounds are traced directly against the original reference; no replacement frame artwork.
 lm=h.custom(a,'float2 p=UV*float2(1672,941);'+polygon(left),unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('UV',uv,'')])
 rm=h.custom(a,'float2 p=UV*float2(1672,941);'+polygon(right),unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('UV',uv,'')])
 mask=h.custom(a,'return max(L,R);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('L',lm,''),('R',rm,'')])
 code='float2 p=UV*float2(1672,941);float m=0;float2 d;'
 # Only numeric/name/experience regions use the cleaned paper. Everything else is original RGB.
 for x,y,w,hh in [(1068,207,246,81),(1068,292,240,43),(1483,227,113,25),(1483,266,113,25),(1483,306,113,25),(1483,343,113,25)]:
  code+=f'd=min(p-float2({x},{y}),float2({x+w},{y+hh})-p);m=max(m,saturate(min(d.x,d.y)/2));'
 code+='return lerp(Original,Clean,m);'
 rgb=h.custom(a,code,unreal.CustomMaterialOutputType.CMOT_FLOAT3,[('UV',uv,''),('Original',s,'RGB'),('Clean',clean,'RGB')])
 M.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY);h.finishmat(a)
 bp,t=h.tree(h.MAIN);h.obj(t,'HubBackplate').set_brush_from_material(a)
 # Original artwork includes the header, exact bilingual row lettering, icons and footer.
 for n in ['HubBannerProject','HubSubtitle','HubTitleCompass','HubHeaderRule','HubFooterRule','HubBannerM','HubWordmark','TitleText','HubFooterSeparator0','HubFooterSeparator1','HubMotto','HubLocationLabel','HubJobLabel','HubGoldLabel','HubCharacterIdLabel','HubMottoHint']:
  h.obj(t,n).set_visibility(unreal.SlateVisibility.COLLAPSED)
 h.rect(h.obj(t,'MainMenuTileView'),47,144,385,620)
 for k,v in [('entry_width',385.0),('entry_height',68.5),('horizontal_entry_spacing',0.0),('vertical_entry_spacing',0.0),('bEntrySizeIncludesEntrySpacing',True),('tile_alignment',unreal.ListItemAlignment.LEFT_ALIGNED)]:h.obj(t,'MainMenuTileView').set_editor_property(k,v)
 for n,x,w in [('OptionsButton',61,123),('CharacterSelectButton',204,186),('QuitGameButton',399,149)]:h.rect(h.obj(t,n),x,829,w,49)
 h.rect(h.obj(t,'HubCloseButton'),1588,98,43,40)
 close=h.obj(t,'HubCloseX');close.set_text('X');close.set_color_and_opacity(h.slate((.86,.85,.80)));font=close.get_editor_property('font');font.size=21;close.set_font(font)
 # Live values occupy the reference's original baselines, never the baked sample values.
 ink=(.009,.018,.025)
 for n,x,y,w,hh,size in [('HubLevel',1073,208,230,34,21),('HubName',1073,244,239,38,24),('HubExperience',1193,309,108,25,12),('HubLocation',1483,228,108,25,12),('HubJob',1483,267,108,25,12),('HubGold',1483,307,108,25,12),('HubCharacterId',1483,345,108,25,12)]:
  o=h.text(t,n,x,y,w,hh,size,ink);o.set_editor_property('justification',unreal.TextJustify.LEFT if n in ['HubLevel','HubName'] else unreal.TextJustify.RIGHT)
 h.rect(h.obj(t,'HubExperienceBar'),1073,297,226,7)
 bar=h.obj(t,'HubExperienceBar');style=bar.get_editor_property('widget_style')
 for key,rgb in [('background_image',(.30,.25,.19)),('fill_image',(.016,.029,.039))]:
  b=unreal.SlateBrush();b.draw_as=unreal.SlateBrushDrawType.ROUNDED_BOX;b.tint_color=h.slate(rgb);setattr(style,key,b)
 bar.set_editor_property('widget_style',style);bar.set_fill_color_and_opacity(h.color((1,1,1)))
 # Keep the user's editable greeting discreetly within the unused lower paper area.
 h.rect(h.obj(t,'HubMottoInput'),1120,369,440,27)
 inp=h.obj(t,'HubMottoInput');st=inp.get_editor_property('widget_style');ts=st.text_style;f=ts.font;f.size=11;ts.font=f;st.text_style=ts
 for key in ['background_image_normal','background_image_hovered','background_image_focused','background_image_read_only']:
  b=getattr(st,key);b.draw_as=unreal.SlateBrushDrawType.NO_DRAW_TYPE;setattr(st,key,b)
 inp.set_editor_property('widget_style',st);inp.set_tool_tip_text('인사말 수정 · Enter 저장')
 # Invisible live buttons overlay the exact original row/footer pixels.
 for p in [h.TILE,h.SYSTEM]:
  bp,t=h.tree(p);cdo=unreal.get_default_object(bp.generated_class());cdo.modify();cdo.set_editor_property('bIconOnly',False)
  st=h.obj(t,'Btn_Common').get_editor_property('widget_style')
  for key,alpha in [('normal',0),('hovered',.10),('pressed',.19),('disabled',0)]:
   b=unreal.SlateBrush();b.draw_as=unreal.SlateBrushDrawType.IMAGE;b.tint_color=h.slate((.8,.6,.32),alpha);setattr(st,key,b)
  h.obj(t,'Btn_Common').set_style(st)
  for n in ['Img_Icon','Txt_Label','Txt_Shortcut','ShortcutKeycap','HubStatBar1','HubStatBar2','HubStatBar3','HubStatBaseline']:
   o=unreal.find_object(t,n)
   if o:o.modify();o.set_render_opacity(0)
 report={}
 for p in [h.TILE,h.SYSTEM,h.MAIN]:
  r=B.compile_blueprint(p);assert r.success and r.num_errors==0,r.export_text()
  assert unreal.ObjectReflectionService.get_property(p,'Bindings')==bindings[p]
  assert E.save_asset(p,False);report[p]={'errors':r.num_errors,'warnings':r.num_warnings}
 (OUT/'applied.json').write_text(json.dumps(report,indent=2))
if __name__=='__main__':apply()
