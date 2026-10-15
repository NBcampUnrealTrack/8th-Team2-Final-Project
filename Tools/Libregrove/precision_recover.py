import json,sys
from concurrent.futures import ThreadPoolExecutor
from precision_workflow import DOC
from precision_stage import stage
from precision_finish import repair_uv

def inn(name='LG_02_Inn'):
    def opts(r):
        ids=sorted({t['source_face_id'] for t in r['triangulation_preview']['tiny_triangles_below_1e_8']})
        assert ids
        return {'tool':'blender_triangulate_repair_copy','degenerate_source_face_ids':ids,'max_removed_triangle_area':1e-8}
    stage(name,'Tri25R2','Normals24',opts)
    stage(name,'UV26','Tri25R2',{'tool':'blender_uv_native_copy','method':'smart_project','angle_degrees':60,'margin_fraction':.001})

def split_then_uv(name):
    stage(name,'UVSplit27','UV26',lambda r:{'tool':'blender_topology_split_copy','edge_ids':[e['id'] for e in r['topology']['edges'] if len(e['face_ids'])>2]})
    repair_uv(name,'UVSplit27','UV28')

if __name__=='__main__':
    if sys.argv[1]=='inn':inn()
    elif sys.argv[1]=='sanctuary':inn('LG_07_Sanctuary')
    elif sys.argv[1]=='split':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(split_then_uv,['LG_03_VillageHall','LG_05_WatermillHouse']))
    elif sys.argv[1]=='tree_split':split_then_uv('LG_08_BroadleafTree')
    elif sys.argv[1]=='sanctuary_split':split_then_uv('LG_07_Sanctuary')
