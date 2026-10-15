import unreal
import json
p='/Game/Cindehearth/Materials/M_CH_ChimneySmoke'
n=unreal.MaterialNodeService
g=json.loads(n.export_material_graph(p))
e={x.get('parameter_name',x['class']):x['id'] for x in g['expressions']}
assert n.connect_expressions(p,e['Fresnel'],'',e['OneMinus'],'')
assert n.connect_expressions(p,e['OneMinus'],'',e['Multiply'],'A')
assert n.connect_expressions(p,e['Density'],'',e['Multiply'],'B')
assert n.connect_to_output(p,e['SmokeColor'],'','BaseColor')
assert n.connect_to_output(p,e['Multiply'],'','Opacity')
t=n.create_expression(p,'Time',-650,520)
s=n.create_expression(p,'Sine',-450,520)
w=n.create_parameter(p,'Vector','Wind','','14,4,7,1',-450,680)
m=n.create_expression(p,'Multiply',-200,550)
assert n.connect_expressions(p,t.id,'',s.id,'')
assert n.connect_expressions(p,s.id,'',m.id,'A')
assert n.connect_expressions(p,w.id,'',m.id,'B')
assert n.connect_to_output(p,m.id,'','WorldPositionOffset')
assert unreal.MaterialService.compile_material(p)
assert unreal.MaterialService.save_material(p)
print(n.export_material_graph_summary(p))
