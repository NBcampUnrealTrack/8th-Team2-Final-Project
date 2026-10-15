from pathlib import Path
import json
root=Path(__file__).resolve().parent
ex=Path.home()/'AppData/Local/Revvy/BlenderExchange/Cindehearth/Restyle'
report=json.loads((root.parent/'Pipeline/verification/pine_volume_import.json').read_text(encoding='utf-8'))[-1]['structuredContent']['result']['result']['mesh_reports'][0]
lo=report['bounds']['local_min'];hi=report['bounds']['local_max']
sz=[hi[i]-lo[i] for i in range(3)]
assert min(sz[:2])/max(sz[:2])>.65,sz
s=2.7/sz[2]
name='CH_PineVolume_Game_v003'
def write(n,v):
    p=root/n;assert not p.exists();p.write_text(json.dumps(v,indent=2),encoding='utf-8')
write('pine_volume_prepare.json',dict(source_object_name=report['object']['name'],output_object_name=name,operations=[dict(type='transform_object',rotation_degrees=[0,0,0]),dict(type='transform_vertices',scale=[s]*3,translation=[-(lo[0]+hi[0])*.5*s,-(lo[1]+hi[1])*.5*s,-lo[2]*s]),dict(type='delete_loose')]))
write('pine_volume_save.json',dict(path=str(ex/(name+'.blend')),overwrite=False))
write('pine_volume_topology.json',dict(job_id='pine_volume_topology_003',blend_path=str(ex/(name+'.blend')),object_name=name))
for view,mode in [('front_right','textured'),('back_left','textured'),('top','clay')]:
    write('pine_volume_review_'+view+'.json',dict(job_id='pine_volume_review_'+view+'_003',blend_path=str(ex/(name+'.blend')),object_name=name,output_path=str(ex/('Review/PineVolume_'+view+'_v003.png')),view=view,mode=mode))
print('NATURAL DIMENSIONS',sz,'UNIFORM SCALE',s)
