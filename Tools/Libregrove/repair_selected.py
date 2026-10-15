"""Call existing typed repair tools with pre-inspected bounded face/loop IDs."""
import json, math, os, re
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from blender_mcp_client import Client,EVIDENCE

BASE=os.path.expandvars(r'%LOCALAPPDATA%/Revvy/BlenderExchange/Cindehearth/Libregrove_20260908')
BLEND=BASE+'/stages/Libregrove_Selected03.blend'

def area(points):
    total=0
    for k in range(1,len(points)-1):
        a=[points[k][j]-points[0][j] for j in range(3)];b=[points[k+1][j]-points[0][j] for j in range(3)]
        cross=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
        total+=math.sqrt(sum(v*v for v in cross))*.5
    return total

def repair(plan):
    c=Client('repair'); name=plan['name']; base=name.split('_Reduced')[0].split('_Balanced')[0]
    try:
        if (EVIDENCE/('repair_handoff_'+base+'.json')).exists():
            print('ALREADY COMPLETE '+base,flush=True)
            return
        # The selected snapshot contains the same inspected source fingerprint.
        pre=c.call('blender_topology_inspect',{'job_id':base+'_sel_pre06','blend_path':BLEND,'object_name':name},'selected_pre06_'+base)['result']
        assert pre['report']['fingerprint']==plan['fingerprint']
        caps=[x['loop_id'] for x in plan['cap_candidates']]
        skipped=[]
        current_name=name; current_file=BLEND
        if caps:
            for attempt in range(1,33):
                output=base+'_Caps06_'+str(attempt);dest=BASE+'/repaired/'+output+'.blend'
                if attempt>1:
                    verify=c.call('blender_topology_inspect',{'job_id':base+'_retry_pre06_'+str(attempt),'blend_path':BLEND,'object_name':name},'selected_retry_pre06_'+base+'_'+str(attempt))['result']
                    assert verify['report']['fingerprint']==plan['fingerprint']
                try:
                    result=c.call('blender_repair_mesh_copy',{'job_id':base+'_caps06_'+str(attempt),'blend_path':BLEND,'object_name':name,'expected_fingerprint':plan['fingerprint'],'output_object_name':output,'output_blend':dest,'operation':'fill_loops','selection_ids':caps,'max_faces':500,'max_diameter':.2},'selected_caps06_'+base+'_'+str(attempt))['result']
                except RuntimeError as error:
                    failure=json.loads(str(error))
                    match=re.search(r'No faces created for boundary (\d+)',failure.get('error',''))
                    if failure.get('state')!='failed' or not match or Path(dest).exists():raise
                    rejected=int(match.group(1))
                    if rejected not in caps:raise
                    # Definitively failed, no file committed: fresh inspection then a NEW,
                    # narrower operation excluding the unfillable exact boundary.
                    caps.remove(rejected);skipped.append(rejected)
                    print('UNFILLABLE BOUNDARY '+base+' '+str(rejected),flush=True)
                    continue
                assert result['source_preserved']
                current_name=output;current_file=dest
                break
            else:raise RuntimeError('Too many unfillable boundaries; manual review required')
        inspected=c.call('blender_topology_inspect',{'job_id':base+'_caps_post06','blend_path':current_file,'object_name':current_name},'selected_post06_'+base)['result']
        top=inspected['topology']; vs=top['vertices']
        zero=[i for i,face in enumerate(top['faces']) if area([vs[j] for j in face])<=1e-10]
        if zero:
            new_name=base+'_Clean07';new_file=BASE+'/repaired/'+new_name+'.blend'
            result=c.call('blender_repair_mesh_copy',{'job_id':base+'_zero07','blend_path':current_file,'object_name':current_name,'expected_fingerprint':inspected['report']['fingerprint'],'output_object_name':new_name,'output_blend':new_file,'operation':'delete_faces','selection_ids':zero,'max_faces':50,'max_diameter':2},'selected_zero07_'+base)['result']
            assert result['source_preserved']
            current_name=new_name;current_file=new_file
            inspected=c.call('blender_topology_inspect',{'job_id':base+'_clean_post07','blend_path':current_file,'object_name':current_name},'selected_clean_post07_'+base)['result']
        export_path=BASE+'/repaired_exports/'+base+'_Repaired.fbx'
        c.call('blender_repair_export',{'job_id':base+'_repair_fbx','blend_path':current_file,'object_name':current_name,'expected_fingerprint':inspected['report']['fingerprint'],'output_path':export_path},'selected_export_'+base)
        handoff={'id':base,'object_name':current_name,'blend_path':current_file,'export_path':export_path,'caps':caps,'skipped_boundary_ids':skipped,'removed_zero_faces':zero,'report':inspected['report']}
        (EVIDENCE/('repair_handoff_'+base+'.json')).write_text(json.dumps(handoff,indent=2),encoding='utf-8')
        print(json.dumps({'id':base,'caps':len(caps),'removed_zero':len(zero),'geometry':inspected['report']['geometry']}),flush=True)
        return handoff
    finally:c.close()

if __name__=='__main__':
    plans=json.loads((EVIDENCE/'repair_selected_plan.json').read_text(encoding='utf-8'))
    with ThreadPoolExecutor(max_workers=2) as pool: list(pool.map(repair,plans))
