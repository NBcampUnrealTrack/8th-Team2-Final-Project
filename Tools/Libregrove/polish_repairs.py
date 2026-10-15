"""Typed V3 contact separation, surface cleanup and normal repair."""
import json,sys
from concurrent.futures import ThreadPoolExecutor
from polish_v3 import ASSETS,DOC,OUT,transport,call,record,stage,render

def test():
    c=transport.Client('precision')
    try:
        schema=c.rpc('tools/list',{});dest=DOC/'precision_tools_v3contacts.json';assert not dest.exists();dest.write_text(json.dumps(schema,indent=2),encoding='utf-8')
    finally:c.close()
    print(json.dumps(call('precision','blender_precision_self_test',{'job_id':'V3_PRECISION_TEST_002','output_blend':str(OUT/'V3_Test002.blend')},'V3_test002')),flush=True)

def repair(n):
    assert record('V3_test002')['tests']['bounded_component_offset']
    source='Reduce02'
    if n==1:
        stage(n,source,'Fold03','blender_repair_mesh_copy',{'operation':'delete_faces','selection_ids':[2104],'max_faces':1,'max_diameter':.5},'repair');source='Fold03'
    if n==4:
        stage(n,source,'Wire03','blender_delete_wire_edges_copy',lambda r:{'edge_ids':[e['id'] for e in r['topology']['edges'] if not e['face_ids']]});source='Wire03'
        stage(n,source,'Contact04','blender_components_offset_copy',{'component_ids':[55,57],'offset_m':[.002,0,0]});source='Contact04'
    if n==5:
        stage(n,source,'Contact03','blender_components_offset_copy',{'component_ids':[446],'offset_m':[0,-.002,0]});source='Contact03'
        stage(n,source,'Contact04','blender_components_offset_copy',{'component_ids':[504],'offset_m':[-.002,0,0]});source='Contact04'
    stage(n,source,'Normals05','blender_sharp_normals_copy',{'angle_degrees':60 if n==8 else 45})
    for view in ('front_right','back_left'):render(n,'Normals05',view)

if __name__=='__main__':
    if sys.argv[1]=='test':test()
    elif sys.argv[1]=='mill_back':
        stage(5,'Normals05','Contact12','blender_components_offset_copy',{'component_ids':[495],'offset_m':[0,.002,0]})
        render(5,'Contact12','back_left')
    elif sys.argv[1]=='repair':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(repair,[int(v) for v in sys.argv[2:]]))
