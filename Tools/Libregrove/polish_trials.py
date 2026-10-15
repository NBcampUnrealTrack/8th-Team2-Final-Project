import json,sys
from concurrent.futures import ThreadPoolExecutor
from polish_v3 import DOC,OUT,call,stage,render,transport

if sys.argv[1]=='test':
    c=transport.Client('precision')
    try:(DOC/'precision_tools_v3decimate.json').write_text(json.dumps(c.rpc('tools/list',{}),indent=2),encoding='utf-8')
    finally:c.close()
    print(json.dumps(call('precision','blender_precision_self_test',{'job_id':'V3_PRECISION_TEST_001','output_blend':str(OUT/'V3_Test001.blend')},'V3_test001')))
elif sys.argv[1] in ('reduce','conservative'):
    def trial(n):
        name='Reduce02' if sys.argv[1]=='conservative' else 'Reduce01'
        try:
            stage(n,'BASE',name,'blender_surface_guarded_decimate_copy',{'ratio':.8 if name=='Reduce02' else .65,'max_surface_error_m':.015,'max_bounds_drift_m':.005})
            for view in ('front_right','back_left'):render(n,name,view)
        except Exception as e:print(json.dumps({'id':n,'candidate':name,'error':str(e)}),flush=True)
    with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(trial,[int(n) for n in sys.argv[2:]]))
