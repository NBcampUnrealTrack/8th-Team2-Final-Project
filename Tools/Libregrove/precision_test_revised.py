import json
from blender_mcp_client import Client
from precision_workflow import DOC,OUT
c=Client('precision')
try:
    schema=c.rpc('tools/list',{});(DOC/'live_precision_tools_v2_uvfloor.json').write_text(json.dumps(schema,indent=2),encoding='utf-8')
    print(json.dumps(c.call('blender_precision_self_test',{'job_id':'LG_PRECISION_SELFTEST_002','output_blend':str(OUT/'TestFixtures_002.blend')},'precision_selftest_002')))
finally:c.close()
