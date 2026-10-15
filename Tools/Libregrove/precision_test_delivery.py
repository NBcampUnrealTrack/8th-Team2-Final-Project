import json
from blender_mcp_client import Client
from precision_workflow import DOC,OUT,record
c=Client('precision')
try:
    (DOC/'live_precision_tools_v2_delivery.json').write_text(json.dumps(c.rpc('tools/list',{}),indent=2),encoding='utf-8')
    print(json.dumps(c.call('blender_precision_self_test',{'job_id':'LG_PRECISION_SELFTEST_004','output_blend':str(OUT/'TestFixtures_004.blend')},'precision_selftest_004')))
    inspect=c.call('blender_precision_inspect',{'job_id':'LG_PRECISION_ASSEMBLY_INSPECT004','blend_path':str(OUT/'TestFixtures_004.blend'),'object_name':'TEST_Dissolved','include_uv_overlap':True},'precision_assembly_inspect004')['result']
    print(json.dumps(c.call('blender_assemble_clean_review',{'job_id':'LG_PRECISION_ASSEMBLY_TEST004','assets':[{'blend_path':str(OUT/'TestFixtures_004.blend'),'object_name':'TEST_Dissolved','expected_fingerprint':inspect['report']['fingerprint'],'display_name':'REVIEW_TEST'}],'output_blend':str(OUT/'TestAssembly_004.blend'),'layout':'grid'},'precision_assembly_test004')))
finally:c.close()
