import sys
import level_v8_client as c
for stage in sys.argv[1:]:
    label='V8_'+stage+'_run'+__import__('datetime').datetime.now().strftime('%H%M%S')
    c.run('level_v8_build.py',label,{'STAGE':stage})
