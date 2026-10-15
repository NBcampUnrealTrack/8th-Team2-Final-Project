"""Five additive typed primitive calls, with inspection between mutations."""
import json
import math
import sys
from pathlib import Path
import blender_mcp_client as transport

ROOT = Path(__file__).resolve().parents[2]
transport.EVIDENCE = ROOT / 'Docs/Libregrove/DirectModel_Cottage_V4_20260908/evidence'
COLLECTION = 'LG_V4_Direct_Cottage'

def unwrap(value):
    value = value.get('result', value)
    return value.get('result', value) if value.get('state') == 'finished' else value

def main():
    client = transport.Client()
    parts = [
        {'object_name':'LGV4_Cottage_Plinth','primitive':{'type':'cube','size':1,'calc_uvs':True},'scale':[2.01,2.46,0.08],'location':[0,0,0.04]},
        {'object_name':'LGV4_Cottage_Walls','primitive':{'type':'cube','size':1,'calc_uvs':True},'scale':[1.95,2.4,1.18],'location':[0,0,0.67]},
        {'object_name':'LGV4_Cottage_Gable','primitive':{'type':'cylinder','radius':1,'depth':2,'vertices':3,'cap_fill':'NGON','calc_uvs':True},'scale':[2.4/math.sqrt(3),1.165/1.5,1.95/2],'rotation_degrees':[90,0,90],'location':[0,0,1.255+1.165/3]},
    ]
    theta = math.atan(0.94)
    thick = 0.04
    center_y = (1.35-math.sin(theta)*thick/2)/2
    slope_length = 2*center_y/math.cos(theta)
    center_z = 2.43 - math.sin(theta)*slope_length/2 - math.cos(theta)*thick/2
    for side, sign in [('Roof_Right',1),('Roof_Left',-1)]:
        parts.append({'object_name':'LGV4_Cottage_'+side,'primitive':{'type':'cube','size':1,'calc_uvs':True},'scale':[2.25,slope_length,thick],'rotation_degrees':[-sign*math.degrees(theta),0,0],'location':[0,sign*center_y,center_z]})
    try:
        for index, part in enumerate(parts,1):
            before = unwrap(client.call('blender_scene_inspect',{},f'build_{index:02d}_before'))
            assert COLLECTION in before['collections']
            assert not any(o['name']==part['object_name'] for o in before['objects'])
            part['collection_name'] = COLLECTION
            client.call('blender_primitive_create',part,f'build_{index:02d}_create')
            after = unwrap(client.call('blender_mesh_inspect',{'object_name':part['object_name'],'requirements':{'require_manifold':True,'require_uv':True,'max_triangles':16}},f'build_{index:02d}_inspect'))
            print(json.dumps({'part':part['object_name'],'report':after},ensure_ascii=False),flush=True)
    finally:
        client.close()

if __name__=='__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    main()
