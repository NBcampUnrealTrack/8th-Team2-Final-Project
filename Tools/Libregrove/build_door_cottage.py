"""Build minimal facade parts exclusively with existing granular typed MCP."""
import hashlib,json,shutil,sys
from pathlib import Path
import blender_mcp_client as transport
ROOT=Path(__file__).resolve().parents[2]
DEST=ROOT/'Docs/Libregrove/DirectModel_Cottage_V5_20260908'
transport.EVIDENCE=DEST/'evidence'
EXCHANGE=Path('C:/Users/Ndus_072/AppData/Local/Revvy/BlenderExchange/Libregrove_DirectCottage_V5_20260908')
DOOR_EXCHANGE=ROOT/'Saved/Revvy/BlenderExchange/LibregroveDoorV5'

def unwrap(v):
    v=v.get('result',v)
    return v.get('result',v) if v.get('state')=='finished' else v

def main():
    fixture=json.loads((ROOT/'Saved/LibregroveDoorJobs/DoorV5_SelfTest001/result.json').read_text())
    assert fixture['success'], 'Extension self-test must pass first'
    parts=[]
    def cube(name,loc,size,moving=False):parts.append({'name':'LGV5_'+name,'location':loc,'scale':size,'moving':moving})
    # Hollow shell built from six closed slabs; no Boolean, no wall remains in doorway.
    for side,sign in [('L',-1),('R',1)]:
        cube('Wall_Front_'+side,(.915,sign*.755,.67),(.12,.89,1.18))
        cube('Wall_Side_'+side,(0,sign*1.14,.67),(1.71,.12,1.18))
    cube('Wall_Back',(-.915,0,.67),(.12,2.4,1.18))
    cube('Wall_AboveDoor',(.915,0,1.19),(.12,.62,.14))
    for side,sign in [('L',-1),('R',1)]:cube('Door_Frame_'+side,(.98,sign*.335,.60),(.09,.05,1.04))
    cube('Door_Frame_Header',(.98,0,1.16),(.09,.72,.08))
    for side,sign in [('L',-1),('R',1)]:
        cy=sign*.765
        cube('Window_'+side+'_Pane',(.984,cy,.78),(.018,.40,.43))
        for label,d in [('JambL',-.235),('JambR',.235)]:cube('Window_'+side+'_'+label,(1.006,cy+d,.78),(.062,.05,.53))
        cube('Window_'+side+'_Top',(1.006,cy,1.02),(.062,.42,.05))
        cube('Window_'+side+'_Sill',(1.025,cy,.54),(.10,.52,.05))
        cube('Window_'+side+'_Mullion',(1.01,cy,.78),(.06,.027,.43))
    cube('Door_Leaf',(1.02,0,.595),(.04,.57,.99),True)
    cube('Door_Handle_Front',(1.055,.205,.62),(.03,.045,.025),True)
    cube('Door_Handle_Back',(.985,.205,.62),(.03,.045,.025),True)
    client=transport.Client()
    try:
        before=unwrap(client.call('blender_scene_inspect',{},'facade_before_001'))
        sources={o['name']:o['fingerprint'] for o in before['objects']}
        assert len(sources)==5 and all(k.startswith('LGV4_Cottage_') for k in sources)
        baseline=[]
        for i,name in enumerate(sources):baseline.append(unwrap(client.call('blender_mesh_inspect',{'object_name':name},f'facade_base_{i:02d}')))
        backup=EXCHANGE/'Backup/00_V4_before_door.blend'
        client.call('blender_save_copy',{'path':str(backup),'overwrite':False},'facade_backup_001')
        scene=unwrap(client.call('blender_scene_inspect',{},'facade_before_collection_001'))
        assert 'LG_V5_Facade_Parts' not in scene['collections']
        client.call('blender_collection_create',{'collection_name':'LG_V5_Facade_Parts'},'facade_collection_001')
        client.call('blender_scene_inspect',{},'facade_after_collection_001')
        reports=[]
        for i,part in enumerate(parts,1):
            scene=unwrap(client.call('blender_scene_inspect',{},f'facade_{i:02d}_before'))
            assert not any(o['name']==part['name'] for o in scene['objects'])
            client.call('blender_primitive_create',{'object_name':part['name'],'collection_name':'LG_V5_Facade_Parts','primitive':{'type':'cube','size':1,'calc_uvs':True},'location':part['location'],'scale':part['scale']},f'facade_{i:02d}_create')
            report=unwrap(client.call('blender_mesh_inspect',{'object_name':part['name'],'requirements':{'require_manifold':True,'require_uv':True,'max_triangles':12}},f'facade_{i:02d}_after'))
            assert report['geometry']['vertices']==8 and report['geometry']['triangles']==12
            reports.append(report);print(f"BUILT {i}/{len(parts)} {part['name']}",flush=True)
        final=unwrap(client.call('blender_scene_inspect',{},'facade_after_all_001'))
        for o in final['objects']:
            if o['name'] in sources:assert o['fingerprint']==sources[o['name']]
        source=EXCHANGE/'01_Facade_Parts.blend'
        client.call('blender_save_copy',{'path':str(source),'overwrite':False},'facade_parts_save_001')
        copied=DOOR_EXCHANGE/'source/01_Facade_Parts.blend';copied.parent.mkdir(parents=True,exist_ok=True);assert not copied.exists();shutil.copy2(source,copied)
        def ref(report):return {'object_name':report['object']['name'],'expected_fingerprint':report['fingerprint']}
        moving={p['name'] for p in parts if p['moving']}
        static=[ref(r) for r in baseline if r['object']['name']!='LGV4_Cottage_Walls']+[ref(r) for r in reports if r['object']['name'] not in moving]
        moving_refs=[ref(r) for r in reports if r['object']['name'] in moving]
        request={'job_id':'DoorV5_Assemble001','blend_path':str(copied),'expected_sha256':hashlib.sha256(copied.read_bytes()).hexdigest(),'static_parts':static,'moving_parts':moving_refs,'pivot_world_m':[1.04,-.285,.10],'body_name':'SM_LG_Cottage_Body_V5','door_name':'SM_LG_Cottage_Door_V5','output_blend':str(DOOR_EXCHANGE/'02_Cottage_Assembly.blend')}
        with (DEST/'assemble_001.json').open('x',encoding='utf-8') as f:json.dump(request,f,indent=2)
        with (DEST/'part_catalog.json').open('x',encoding='utf-8') as f:json.dump({'parts':parts,'source_baseline':baseline,'created_reports':reports,'backup':str(backup)},f,indent=2)
        print('READY '+str(DEST/'assemble_001.json'),flush=True)
    finally:client.close()

if __name__=='__main__':main()
