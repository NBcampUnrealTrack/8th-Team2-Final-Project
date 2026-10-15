"""Inspect and repair evidenced export-precision UV slivers via typed MCP."""
import json
from precision_workflow import DOC
from precision_stage import inspect,stage

name='LG_04_Workshop';h=json.loads((DOC/(name+'_UV33.json')).read_text(encoding='utf-8'))
r=inspect(h['output_blend'],h['output_object'],name+'_GLB_SLIVER_INSPECT',uv=True)
risks=r['uv_export_precision']['risk_faces'];assert risks
print(json.dumps({'risks':risks}),flush=True)
ids=sorted({i['face_id'] for i in risks})
stage(name,'UV34','UV33',{'tool':'blender_uv_faces_reproject_copy','face_ids':ids,'margin_fraction':.001,'minimum_uv_area':1e-7})
