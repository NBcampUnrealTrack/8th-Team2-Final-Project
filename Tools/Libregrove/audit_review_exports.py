import json
from blender_mcp_client import EVIDENCE
from uv_export_qa import audit
from inspect_glb import inspect
for path in sorted(EVIDENCE.glob('review_handoff_*.json')):
    h=json.loads(path.read_text(encoding='utf-8'));h['id']=h['id']+'_REVIEW16'
    if h['uv_report']['uv_layers']:audit(h)
    else:
        result={'id':h['id'],'status':'MISSING_UV','export':inspect(h['exports']['glb']),'positive_overlap_pair_count':-1,'zero_area_uv_triangles_1e_12':-1,'uv_triangle_area_sum':0}
        (EVIDENCE/('export_qa_'+h['id']+'.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
        print(json.dumps({'id':h['id'],'status':'MISSING_UV'}))
