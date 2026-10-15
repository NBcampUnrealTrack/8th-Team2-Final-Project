"""Final hub feedback pass; no rebuilding older art/layout passes."""
import unreal,importlib.util,json,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
s=importlib.util.spec_from_file_location('depth',ROOT/'Tools/UIHubSidePanels/apply_depth.py');d=importlib.util.module_from_spec(s);s.loader.exec_module(d);h=d.h;M=h.M

def apply():
 assert not h.W.is_pie_running()
 out=ROOT/'Saved/UIFeedbackPolish'/('hub_'+datetime.now().strftime('%Y%m%d_%H%M%S'));out.mkdir(parents=True)
 for path in [h.MAIN,h.TILE]:
  src=ROOT/'Content'/(path[6:]+'.uasset');shutil.copy2(src,out/src.name)
 path=h.ART+'/T_Hub_WoodDesk'
 if not h.E.does_asset_exist(path):
  ok,error=unreal.AssetDiscoveryService.import_asset(str(ROOT/'ArtSource/UI/HubSidePanels/T_Hub_WoodDesk.png'),h.ART,'T_Hub_WoodDesk');assert ok,error
 tex=unreal.load_asset(path)
 for key,value in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('srgb',True),('compression_no_alpha',False),('never_stream',True)]:tex.set_editor_property(key,value)
 assert h.E.save_loaded_asset(tex)
 bp,t=h.tree(h.MAIN)
 for old,new in [('M_Hub_DepthRail','M_Hub_FeedbackRail'),('M_Hub_PolishDesk','M_Hub_WoodDesk')]:
  dst=h.ART+'/'+new
  if not h.E.does_asset_exist(dst):assert h.E.duplicate_asset(h.ART+'/'+old,dst)
  graph=json.loads(unreal.MaterialNodeService.export_material_graph(dst));(out/(new+'.json')).write_text(json.dumps(graph))
  for e in graph['expressions']:
   if new.endswith('WoodDesk') and e['class']=='TextureSample':
    assert unreal.MaterialNodeService.set_expression_property(dst,e['id'],'Texture',tex.get_path_name())
   if e['class']!='Custom':continue
   code=e['properties']['Code'];updated=code
   if new.endswith('FeedbackRail'):
    if code.strip()=='return UV*float2(740,899)/float2(1672,941);':
     updated='float2 q=UV*float2(740,899);float row=floor((q.y-144)/68.5);if(row>=0&&row<9){float y=q.y-144-row*68.5;q.y=144+row*68.5+saturate(y/56.5)*68.5;}return q/float2(1672,941);'
    elif 'float edges[81]' in code and 'float2 q=UV' in code and '56.5' not in code:
     updated=code.replace('float2 p=float2(q.x-47,q.y-144-row*68.5);','float local=q.y-144-row*68.5;if(local>56.5)return 0;float2 p=float2(q.x-47,local*68.5/56.5);')
    elif 'float header=Mask' in code and 'q.x>132' not in code:
     updated=code.replace('float footer=', 'if(q.x>132&&q.y<92)header=0;float footer=')
   elif 'float w=saturate' in code:
    # Wood stays brown; keep the same neutral shading and alpha layers.
    updated='float l=dot(C,float3(.2126,.7152,.0722));return lerp(l.xxx,C,.82)*float3(.98,1,1.02);'
   if updated!=code:assert unreal.MaterialNodeService.set_expression_property(dst,e['id'],'Code',updated)
  mat=unreal.load_asset(dst);assert not M.recompile_material(mat);diag=unreal.MaterialNodeService.get_material_diagnostics(dst);assert diag.is_compiled_ok;assert h.E.save_loaded_asset(mat)
  h.obj(t,'HubLeftBackplate' if new.endswith('FeedbackRail') else 'HubDeskDecor').set_brush_from_material(mat)
 view=h.obj(t,'MainMenuTileView');view.set_editor_property('entry_height',68.5);view.set_editor_property('vertical_entry_spacing',12.0);view.set_editor_property('bEntrySizeIncludesEntrySpacing',True)
 # Baked PROJECT M is suppressed in rail material; live text supports localization.
 title=h.obj(t,'HubBannerProject');h.obj(t,'HubLeftCanvas').add_child_to_canvas(title);h.rect(title,145,29,410,45);title.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE);title.set_text('나이아 언바운드')
 ft=unreal.find_object(unreal.find_object(unreal.load_asset('/Game/Developers/dbals/UI/WBP_OptionsWindow'),'WidgetTree'),'TitleText').get_editor_property('font').copy();ft.size=26;title.set_font(ft);title.set_color_and_opacity(h.slate((.571,.468,.283)));title.slot.set_z_order(3)
 result={}
 for path in [h.MAIN]:
  c=h.B.compile_blueprint(path);assert c.success and not c.num_errors,c.export_text();assert h.E.save_asset(path,False);result[path]=c.export_text()
 (out/'compile.json').write_text(json.dumps(result,indent=2));print(result)
if __name__=='__main__':apply()
