import json
from pathlib import Path
root=Path(__file__).resolve().parent
project=root.parents[2]
ex=Path.home()/'AppData/Local/Revvy/BlenderExchange/Cindehearth'
for name in ['pine','rock']:
    r=json.loads((project/'Saved/CindehearthRepair'/('restyle_'+name+'_topology_001')/'result.json').read_text(encoding='utf-8'))['result']
    groups=r['topology']['boundary_loops']
    assert all(c['simple_closed'] for c in groups)
    args=dict(job_id='restyle_'+name+'_cap_001',blend_path=str(ex/('Restyle/CH_'+name.title()+'_Game_v001.blend')),
       object_name=r['report']['object']['name'],expected_fingerprint=r['report']['fingerprint'],
       output_object_name='CH_'+name.title()+'_Closed_v002',output_blend=str(ex/('Restyle/CH_'+name.title()+'_Closed_v002.blend')),
       operation='fill_loops',selection_ids=[c['id'] for c in groups],max_faces=250,max_diameter=2.)
    (root/(name+'_cap.json')).write_text(json.dumps(args,indent=2),encoding='utf-8')
name='cart';r=json.loads((project/'Saved/CindehearthRepair/restyle_cart_topology_001/result.json').read_text(encoding='utf-8'))['result']
args=dict(job_id='restyle_cart_outlier_001',blend_path=str(ex/'Restyle/CH_Cart_Game_v001.blend'),object_name='CH_Cart_Game_v001',expected_fingerprint=r['report']['fingerprint'],output_object_name='CH_Cart_NoOutlier_v002',output_blend=str(ex/'Restyle/CH_Cart_NoOutlier_v002.blend'),operation='delete_components',selection_ids=[59],max_faces=1,max_diameter=.01)
(root/'cart_outlier.json').write_text(json.dumps(args,indent=2),encoding='utf-8')
