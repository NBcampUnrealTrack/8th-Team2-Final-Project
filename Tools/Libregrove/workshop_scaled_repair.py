import json,math,os,sys
from blender_mcp_client import Client,EVIDENCE
BASE=os.path.expandvars(r'%LOCALAPPDATA%/Revvy/BlenderExchange/Cindehearth/Libregrove_20260908')
c=Client('repair');name='LG_04_Workshop';is_tri='--tri' in sys.argv
source=name+('_UVTri16' if is_tri else '_Sized09');blend=BASE+('/stages/Libregrove_UV_Review16.blend' if is_tri else '/stages/Libregrove_All_Resized09.blend')
tag='tri17' if is_tri else 'sized'
try:
    value=c.call('blender_topology_inspect',{'job_id':name+'_'+tag+'_inspect','blend_path':blend,'object_name':source},tag+'_topology_'+name)['result']
    vs=value['topology']['vertices'];faces=value['topology']['faces'];ids=[]
    for i,face in enumerate(faces):
        area_vector=[0,0,0]
        points=[vs[v] for v in face]
        for a,b in zip(points,points[1:]+points[:1]):
            cross=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
            area_vector=[a+b for a,b in zip(area_vector,cross)]
        if math.sqrt(sum(v*v for v in area_vector))*.5<=(1e-8 if is_tri else 1e-10):ids.append(i)
    assert ids and len(ids)<50
    new=name+('_TriClean17' if is_tri else '_SizedClean15');dest=BASE+'/repaired/'+new+'.blend'
    c.call('blender_repair_mesh_copy',{'job_id':name+'_'+tag+'_zero15','blend_path':blend,'object_name':source,'expected_fingerprint':value['report']['fingerprint'],'output_object_name':new,'output_blend':dest,'operation':'delete_faces','selection_ids':ids,'max_faces':50,'max_diameter':3},tag+'_zero_'+name)
    post=c.call('blender_topology_inspect',{'job_id':name+'_'+tag+'_post15','blend_path':dest,'object_name':new},tag+'_zero_post_'+name)['result']
    assert post['report']['geometry']['degenerate_faces']==0
    export=BASE+'/repaired_exports/'+new+'.fbx'
    c.call('blender_repair_export',{'job_id':name+'_'+tag+'_export15','blend_path':dest,'object_name':new,'expected_fingerprint':post['report']['fingerprint'],'output_path':export},tag+'_zero_export_'+name)
    (EVIDENCE/('workshop_tri_repair.json' if is_tri else 'workshop_sized_repair.json')).write_text(json.dumps({'object_name':new,'blend_path':dest,'export_path':export,'deleted_faces':ids,'report':post['report']},indent=2),encoding='utf-8')
    print('DELETED ZERO-AREA FACES',ids,flush=True)
finally:c.close()
