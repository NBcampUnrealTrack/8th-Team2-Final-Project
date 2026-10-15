"""Seven copied typed transforms to resolve observed coplanar facade artifacts."""
import hashlib,json,shutil
from pathlib import Path
import blender_mcp_client as transport
from build_door_cottage import ROOT,DEST,EXCHANGE,DOOR_EXCHANGE,unwrap
transport.EVIDENCE=DEST/'evidence'

def main():
    old=json.loads((DEST/'assemble_001.json').read_text());changes=[('LGV4_Cottage_Gable','LGV5_Cottage_Gable_Clean',{'location':[0,0,1.646666666667],'scale':[1.385640646,.773333333333,.975]})]
    for side in ['L','R']:
        for jamb in ['JambL','JambR']:
            name=f'LGV5_Window_{side}_{jamb}';changes.append((name,name+'_Clean',{'scale':[.062,.05,.43]}))
        name=f'LGV5_Window_{side}_Top';changes.append((name,name+'_Clean',{'scale':[.062,.52,.05]}))
    client=transport.Client();replacement={}
    try:
        for i,(source,out,values) in enumerate(changes,1):
            scene=unwrap(client.call('blender_scene_inspect',{},f'contact_r2_{i:02d}_scene'))
            assert out not in {o['name'] for o in scene['objects']}
            before=unwrap(client.call('blender_mesh_inspect',{'object_name':source},f'contact_r2_{i:02d}_before'))
            client.call('blender_mesh_edit',{'source_object_name':source,'output_object_name':out,'create_copy':True,'operations':[{'type':'transform_object',**values}]},f'contact_r2_{i:02d}_copy')
            after=unwrap(client.call('blender_mesh_inspect',{'object_name':out},f'contact_r2_{i:02d}_after'))
            original=unwrap(client.call('blender_mesh_inspect',{'object_name':source},f'contact_r2_{i:02d}_source_check'))
            assert original['fingerprint']==before['fingerprint']
            replacement[source]={'object_name':out,'expected_fingerprint':after['fingerprint']}
        client.call('blender_scene_inspect',{},'contact_r2_before_save')
        source=EXCHANGE/'02_Refined_Facade_Parts.blend'
        client.call('blender_save_copy',{'path':str(source),'overwrite':False},'contact_r2_save')
        dest=DOOR_EXCHANGE/'source/02_Refined_Facade_Parts.blend';assert not dest.exists();shutil.copy2(source,dest)
        old.update(job_id='DoorV5_Assemble002',blend_path=str(dest),expected_sha256=hashlib.sha256(dest.read_bytes()).hexdigest(),output_blend=str(DOOR_EXCHANGE/'revision02/02_Cottage_Assembly.blend'))
        old['static_parts']=[replacement.get(r['object_name'],r) for r in old['static_parts']]
        with (DEST/'assemble_002.json').open('x',encoding='utf-8') as f:json.dump(old,f,indent=2)
    finally:client.close()

if __name__=='__main__':main()
