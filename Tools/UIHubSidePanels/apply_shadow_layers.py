"""Final optional hub pass: independent time-driven UI shadows, no widget motion."""
import unreal,importlib.util,json,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
s=importlib.util.spec_from_file_location('shadow_depth',ROOT/'Tools/UIHubSidePanels/apply_depth.py');d=importlib.util.module_from_spec(s);s.loader.exec_module(d);h=d.h;M=h.M
OUT=ROOT/'Saved/UIHubShadows'

def scalar(a,name,value):
 n=M.create_material_expression(a,unreal.MaterialExpressionScalarParameter,-700,250);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n

def base(name):
 a=h.mat(name);u=M.create_material_expression(a,unreal.MaterialExpressionTextureCoordinate,-900,0)
 t=M.create_material_expression(a,unreal.MaterialExpressionTime,-900,200);t.set_editor_property('ignore_pause',True)
 speed=scalar(a,'MotionSpeed',.9);amp=scalar(a,'MotionPixels',8);strength=scalar(a,'ShadowOpacity',.16);swing=scalar(a,'SwingDegrees',6.5)
 time=h.custom(a,'return T*Speed;',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('T',t,''),('Speed',speed,'')])
 return a,u,[('UV',u,''),('T',time,''),('Amplitude',amp,''),('Strength',strength,''),('Swing',swing,'')]

def finish(a,alpha):
 rgb=M.create_material_expression(a,unreal.MaterialExpressionConstant3Vector,400,100);rgb.set_editor_property('constant',unreal.LinearColor(.012,.019,.03,1))
 M.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY);h.finishmat(a);return a

def clean(old,new,marker):
 dst=h.ART+'/'+new
 if not h.E.does_asset_exist(dst):assert h.E.duplicate_asset(h.ART+'/'+old,dst)
 for e in json.loads(unreal.MaterialNodeService.export_material_graph(dst))['expressions']:
  if e['class']=='Custom' and marker in e['properties']['Code']:assert unreal.MaterialNodeService.set_expression_property(dst,e['id'],'Code','return C;')
 a=unreal.load_asset(dst);h.finishmat(a);return a

def panel_drop():
 a,u,ins=base('M_Hub_ProfileDropShadow')
 code='float2 p=UV*float2(1672,941);float2 offset=float2(-4,8)+Amplitude*.15*float2(sin(T),cos(T*.7));float2 q=abs(p-float2(1329.5,300)-offset)-float2(332.5,149);float sd=length(max(q,0))+min(max(q.x,q.y),0)-12;return Strength*(1-smoothstep(-4,17,sd));'
 return finish(a,h.custom(a,code,unreal.CustomMaterialOutputType.CMOT_FLOAT1,ins))

def lower_controls(a,ins):
 for name,pin,value in [('LowerTravelX','LowerX',52),('LowerTravelY','LowerY',24),('LowerPhase','LowerPhase',2.1)]:ins.append((pin,scalar(a,name,value),''))

# Local wind sway rooted at the original lower-right silhouette, not an arc
# around the upper light source. Different axis frequencies avoid shared paths.
LOWER_MOTION='float2 drift=float2(LowerX*sin(T*.93+LowerPhase),LowerY*sin(T*1.37-.7));float shear=.045*sin(T*.57+LowerPhase);'

def surface_shadow(desk=False):
 a,u,ins=base('M_Hub_DeskMovingShade' if desk else 'M_Hub_ProfileMovingShade')
 if desk:
  lower_controls(a,ins)
  sm=d.r.sample(a,unreal.load_asset(h.ART+'/T_Hub_WoodDesk'));ins.append(('A',sm,'A'))
  code='float clip=A;float2 world=float2(400,225)+UV*float2(1272,716);'
 else:
  code='float2 q=(UV*float2(1672,941)-float2(985,139))/float2(687,322);float clip=step(0,q.x)*step(q.x,1)*step(0,q.y)*step(q.y,1);float2 world=UV*float2(1672,941)-float2(0,139);'
 if desk:
  code+=LOWER_MOTION+'float weight=saturate(length((world-float2(1672,941))/float2(900,600))+.35);float2 rotated=world-drift*float2(1.8,.65)*weight;rotated.x+=shear*(941-world.y);'
 else:
  code+='float angle=radians(Swing)*sin(T*.66);float2 pivot=float2(1672,0);float sn=sin(angle),cs=cos(angle);float2 v=world-pivot;float2 rotated=float2(cs*v.x+sn*v.y,-sn*v.x+cs*v.y)+pivot;'
 if desk:
  # Project the existing foliage silhouette onto the desk plane. A diffuse
  # analytic band alone reads as a static gradient on the narrow wooden top.
  projection=code+'float2 p=(rotated-float2(400,225))/float2(1272,716);return float2(.50+.50*p.x,.68+.30*p.y)'
  terms=[]
  for i,(x,y) in enumerate([(0,0),(-1,0),(1,0),(0,-1),(0,1)]):
   mapped=h.custom(a,projection+f'+float2({x}*10.0/1672,{y}*10.0/941);',unreal.CustomMaterialOutputType.CMOT_FLOAT2,ins)
   leaf=d.r.sample(a,unreal.load_asset(h.ART+'/T_Hub_DepthForeground'));M.connect_material_expressions(mapped,'',leaf,'UVs')
   bounded=h.custom(a,'float edge=min(min(Q.x,1-Q.x),min(Q.y,1-Q.y));return F*smoothstep(0,.025,edge);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('Q',mapped,''),('F',leaf,'A')])
   ins.append(('F'+str(i),bounded,''));terms.append('F'+str(i))
  code='return A*Strength*2.4*('+ '+'.join(terms)+')/5;'
 else:
  code+='float2 p=(rotated-float2(985,0))/float2(687,322);float band=exp(-pow((p.x*.75+p.y*.35-.78)/.12,2));float upper=1-smoothstep(.04,.78,p.y);return clip*Strength*(.42*smoothstep(.57,.94,p.x)+.58*band*upper);'
 return finish(a,h.custom(a,code,unreal.CustomMaterialOutputType.CMOT_FLOAT1,ins))

def desk_drop():
 a,u,ins=base('M_Hub_DeskDropShadow');samples=[]
 tex=unreal.load_asset(h.ART+'/T_Hub_WoodDesk')
 for i,(x,y) in enumerate([(0,0),(-1,0),(1,0),(0,-1),(0,1)]):
  mapped=h.custom(a,f'return UV-(float2(-5,8)+Amplitude*.3*float2(sin(T),cos(T*.7))+float2({x},{y})*6)/float2(1272,716);',unreal.CustomMaterialOutputType.CMOT_FLOAT2,ins)
  sm=d.r.sample(a,tex);M.connect_material_expressions(mapped,'',sm,'UVs');ins.append(('A'+str(i),sm,'A'));samples.append('A'+str(i))
 return finish(a,h.custom(a,'return Strength*('+ '+'.join(samples)+')/5;',unreal.CustomMaterialOutputType.CMOT_FLOAT1,ins))

def foreground():
 a,u,ins=base('M_Hub_MovingForegroundShadow');terms=[];lower_controls(a,ins);ins.append(('UpperScale',scalar(a,'UpperShadowScale',1.3),''))
 for i,(x,y) in enumerate([(0,0),(-1,0),(1,0),(0,-1),(0,1)]):
  code=LOWER_MOTION+f"float aspect=abs(ddy(UV.y)/max(abs(ddx(UV.x)),.000001));float upper=1-smoothstep(.35,.65,UV.y);float angle=radians(Swing)*sin(T*.66);float sn=sin(angle),cs=cos(angle);float2 v=(UV-float2(1,0))*float2(aspect,1)/UpperScale;float2 top=float2(cs*v.x+sn*v.y,-sn*v.x+cs*v.y)/float2(aspect,1)+float2(1,0);float weight=saturate(length((UV-float2(1,1))*float2(2,2))+.35);float2 bottom=UV-drift/float2(1672,941)*weight;bottom.x+=shear*(1-UV.y);float2 q=lerp(bottom,top,upper);return lerp(UV,q,smoothstep(.45,.7,UV.x))+float2({x}*7.0/1672,{y}*7.0/941);"
  mapped=h.custom(a,code,unreal.CustomMaterialOutputType.CMOT_FLOAT2,ins);sm=d.r.sample(a,unreal.load_asset(h.ART+'/T_Hub_DepthForeground'));M.connect_material_expressions(mapped,'',sm,'UVs')
  bounded=h.custom(a,'float edge=min(min(Q.x,1-Q.x),min(Q.y,1-Q.y));return A*smoothstep(0,.035,edge);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('Q',mapped,''),('A',sm,'A')]);ins.append(('A'+str(i),bounded,''));terms.append('A'+str(i))
 code='float a=('+ '+'.join(terms)+')/5;float left=lerp(1-smoothstep(.025,.07,UV.x),1,step(.5,UV.x));float clearX=smoothstep(.02,.055,length((UV-float2(.94,.06))*float2(1,.56)));return a*Strength*2*left*clearX;'
 return finish(a,h.custom(a,code,unreal.CustomMaterialOutputType.CMOT_FLOAT1,ins))

def layer(name,parent,rect,z,mat):
 if not h.W.widget_exists(h.MAIN,name):assert h.W.add_component(h.MAIN,'Image',name,parent,True).success
 o=h.obj(h.tree(h.MAIN)[1],name);h.rect(o,*rect);o.slot.set_z_order(z);o.set_brush_from_material(mat);o.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)

def apply():
 assert not h.W.is_pie_running();out=OUT/datetime.now().strftime('%Y%m%d_%H%M%S');out.mkdir(parents=True)
 shutil.copy2(ROOT/'Content/Developers/dbals/UI/WBP_GameMenu.uasset',out/'WBP_GameMenu.uasset')
 profile=clean('M_Hub_PolishProfile','M_Hub_ProfileUnshaded','float edge=pow')
 desk=clean('M_Hub_WoodDesk','M_Hub_DeskUnshaded','float bottom=smoothstep')
 bp,t=h.tree(h.MAIN);h.obj(t,'HubBackplate').set_brush_from_material(profile);h.obj(t,'HubDeskDecor').set_brush_from_material(desk)
 layer('HubProfileContactShadow','HubCanvas',(0,-139,1672,941),-2,panel_drop())
 layer('HubProfileMovingShade','HubCanvas',(0,-139,1672,941),0,surface_shadow())
 layer('HubDeskContactShadow','HubDeskCanvas',(400,225,1272,716),-1,desk_drop())
 layer('HubDeskMovingShade','HubDeskCanvas',(400,225,1272,716),1,surface_shadow(True))
 bp,t=h.tree(h.MAIN);h.obj(t,'HubDepthForeground').set_brush_from_material(foreground())
 result=h.B.compile_blueprint(h.MAIN);assert result.success and not result.num_errors;assert h.E.save_asset(h.MAIN,False)
 (out/'compile.json').write_text(json.dumps({'compile':result.export_text(),'real_time_while_paused':True,'layout_changed':False},indent=2));print(result.export_text())
if __name__=='__main__':apply()
