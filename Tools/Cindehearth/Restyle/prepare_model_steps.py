from pathlib import Path
import json
root=Path(__file__).resolve().parent
ex=Path.home()/'AppData/Local/Revvy/BlenderExchange/Cindehearth'
def write(name,args):
    p=root/name
    assert not p.exists(),str(p)
    p.write_text(json.dumps(args,indent=2),encoding='utf-8')
for mode,view in [('textured','front_right'),('clay','back_left')]:
    write('pine_render_'+mode+'.json',dict(job_id='restyle_pine_'+mode+'_001',blend_path=str(ex/'Restyle/CH_Pine_Game_v001.blend'),object_name='CH_Pine_Game_v001',output_path=str(ex/('Restyle/Review/Pine_'+mode+'_v001.png')),view=view,mode=mode))
for name,size in [('rock',1.8),('cart',1.1)]:
    d=json.loads((root.parent/'Pipeline/verification'/('restyle_'+name+'_import.json')).read_text(encoding='utf-8'))[-1]['structuredContent']['result']['result']
    assert len(d['mesh_reports'])==1
    r=d['mesh_reports'][0];lo=r['bounds']['local_min'];hi=r['bounds']['local_max']
    scale=size/max(hi[i]-lo[i] for i in range(3))
    shift=[-(lo[i]+hi[i])*.5*scale for i in range(2)]+[-lo[2]*scale]
    output='CH_'+name.title()+'_Game_v001'
    write(name+'_prepare.json',dict(source_object_name=r['object']['name'],output_object_name=output,
          operations=[dict(type='transform_object',rotation_degrees=[0,0,0]),dict(type='transform_vertices',scale=[scale]*3,translation=shift),dict(type='delete_loose')]))
    write(name+'_inspect.json',dict(object_name=output,requirements=dict(require_uv=True,require_material=True,max_triangles=4000)))
    write(name+'_save.json',dict(path=str(ex/('Restyle/'+output+'.blend')),overwrite=False))
    write(name+'_topology.json',dict(job_id='restyle_'+name+'_topology_001',blend_path=str(ex/('Restyle/'+output+'.blend')),object_name=output))
