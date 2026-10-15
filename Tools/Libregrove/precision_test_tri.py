import json
from blender_mcp_client import Client
from precision_workflow import DOC,OUT
c=Client('precision')
try:
    (DOC/'live_precision_tools_v2_complete.json').write_text(json.dumps(c.rpc('tools/list',{}),indent=2),encoding='utf-8')
    print(json.dumps(c.call('blender_precision_self_test',{'job_id':'LG_PRECISION_SELFTEST_003','output_blend':str(OUT/'TestFixtures_003.blend')},'precision_selftest_003')))
    print(json.dumps(c.call('blender_precision_surface_compare',{'job_id':'LG_PRECISION_COMPARE_TEST003','blend_path':str(OUT/'TestFixtures_003.blend'),'object_name':'TEST_Plane','reference_object_name':'TEST_Plane'},'precision_compare_test003')))
finally:c.close()
