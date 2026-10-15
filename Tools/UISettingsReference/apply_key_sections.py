"""Final key-panel pass: quiet section bands and clean, scale-aware button rims."""
import unreal, importlib.util, json, shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
spec=importlib.util.spec_from_file_location('keypolish',ROOT/'Tools/UISettingsReference/apply_key_polish.py')
k=importlib.util.module_from_spec(spec);spec.loader.exec_module(k)
p=k.p;q=k.q;E=p.E;M=p.M

BUTTON='''
float2 size=max(PixelSize,float2(8,8));
float2 v=abs((UV-.5)*size);
float2 h=size*.5-1.5;
float cut=min(4.0,min(h.x,h.y)*.4);
float d=max(max(v.x-h.x,v.y-h.y),(v.x+v.y-h.x-h.y+cut)*.70710678);
float aa=max(fwidth(d),.65);
float body=1-smoothstep(-aa*.5,aa*.5,d);
float inside=1-smoothstep(-aa*.5,aa*.5,d+1.15);
float rim=saturate((body-inside)/max(body,.001))*FrameOpacity;
float3 fill=SurfaceColor.rgb*lerp(1.04,.94,UV.y);
fill+=saturate(HoverAmount)*float3(.008,.013,.02);
return float4(lerp(fill,LineColor.rgb,rim),body*FillOpacity);
'''

SECTION='''
float2 size=max(PixelSize,float2(8,8));
float2 pos=UV*size;
float fade=1-smoothstep(.45,1,UV.x);
float aa=max(fwidth(pos.y),.65);
float rule=1-smoothstep(.45,.45+aa,abs(pos.y-(size.y-2)));
float stem=(1-smoothstep(2,2+max(fwidth(pos.x),.65),pos.x))
 *smoothstep(5,6,pos.y)*(1-smoothstep(size.y-6,size.y-5,pos.y));
float ink=max(rule*.35*fade,stem*.85);
float fill=.58*fade*smoothstep(1,3,pos.y)*(1-smoothstep(size.y-3,size.y-1,pos.y));
float alpha=ink+fill*(1-ink);
float3 rgb=(LineColor.rgb*ink+SurfaceColor.rgb*fill*(1-ink))/max(alpha,.001);
return float4(rgb,alpha);
'''

def material(name,code):
 path=p.PRIVATE+'/'+name
 if not E.does_asset_exist(path):assert E.duplicate_asset(p.PRIVATE+'/M_OptionsPopupFrame',path)
 graph=json.loads(unreal.MaterialNodeService.export_material_graph(path))
 node=next(e for e in graph['expressions'] if e['class']=='Custom')
 assert unreal.MaterialNodeService.set_expression_property(path,node['id'],'Code',code)
 for e in graph['expressions']:
  key={'SurfaceColor':'surface','LineColor':'gold'}.get(e.get('parameter_name'))
  if key:assert unreal.MaterialNodeService.set_expression_property(path,e['id'],'DefaultValue',p.col(key).export_text())
 a=unreal.load_asset(path);assert not M.recompile_material(a);assert E.save_loaded_asset(a)
 return a

def apply():
 assert not p.W.is_pie_running()
 out=ROOT/'Saved/UIKeySettingsPolish'/('sections_'+datetime.now().strftime('%Y%m%d_%H%M%S'));out.mkdir()
 targets=[k.BASE+n for n in ['WBP_KeyBindingGroup','WBP_KeyBindingRow','WBP_KeyBindingsWindow']]
 targets += [p.PRIVATE+'/WBP_KeySettings'+kind for kind in ['Secondary','Primary']]
 for path in targets:
  f=ROOT/'Content'/(path[6:]+'.uasset');shutil.copy2(f,out/f.name)
 parent=material('M_KeySettingsCleanFrame',BUTTON)
 section=material('M_KeySettingsSection',SECTION)
 styles={}
 for kind in ['Secondary','Primary']:
  st=unreal.ButtonStyle()
  for state in ['normal','hovered','pressed']:
   src=p.PRIVATE+'/MI_Options'+kind+'_'+state
   dst=p.PRIVATE+'/MI_KeySettings'+kind+'_'+state
   if not E.does_asset_exist(dst):assert E.duplicate_asset(src,dst)
   mi=unreal.load_asset(dst);M.set_material_instance_parent(mi,parent)
   M.set_material_instance_scalar_parameter_value(mi,'FrameOpacity',.65 if state=='normal' else 1)
   assert E.save_loaded_asset(mi);setattr(st,state,q.brush(mi))
  st.disabled=st.normal;st.normal_padding=unreal.Margin(12,0,12,0);st.pressed_padding=unreal.Margin(12,1,12,-1)
  styles[kind]=st
  bp=unreal.load_asset(p.PRIVATE+'/WBP_KeySettings'+kind);bp.modify()
  p.obj(unreal.find_object(bp,'WidgetTree'),'Btn_Common').set_style(st)
 sk=k.s.Skin('WBP_KeyBindingGroup')
 sk.get('GroupBG').set_editor_property('background',q.brush(section))
 sk.get('GroupBG').set_brush_color(unreal.LinearColor(1,1,1,1))
 sk.get('GroupBG').set_padding(unreal.Margin(14,0,12,0))
 sk.get('GroupSizeBox').set_height_override(40)
 sk.get('GroupText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
 sk=k.s.Skin('WBP_KeyBindingRow')
 for n in ['WaitingBox','PadWaitingBox']:sk.get(n).set_editor_property('background',styles['Secondary'].hovered)
 sk=k.s.Skin('WBP_KeyBindingsWindow');sk.get('FooterCloseButton').set_style(styles['Secondary'])
 report={}
 for path in targets[3:]+targets[:3]:
  c=p.B.compile_blueprint(path);assert c.success and c.num_errors==0,c.export_text()
  assert E.save_asset(path,False);report[path]=c.export_text()
 (out/'compile.json').write_text(json.dumps(report,indent=2));print(report)

if __name__=='__main__':apply()
