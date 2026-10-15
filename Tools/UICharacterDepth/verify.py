"""Validate real desktop-pointer snapshots, fixed transforms, and native pixel caps."""
import json,math
from pathlib import Path
out=Path('D:/UnrealEngine/ProjectM/Saved/UICharacterDepth')
tags=['baseline','center','right_edge','top_edge','outside','keyboard','disabled','reopened']
base=json.loads((out/'baseline.json').read_text(encoding='utf-8-sig'))
report={};limits={'DepthFarTexture':4,'CharacterIllustration':6,'DepthRearOrbit':8,'DepthFrontLine':1}
for tag in tags:
 d=json.loads((out/(tag+'.json')).read_text(encoding='utf-8-sig'))
 assert d['fixed']==base['fixed'],(tag,'character/floor/card/shell changed')
 native=d['native'];assert native['state_count']==1,(tag,'duplicate state')
 if tag!='disabled':assert native['ticker_registered'] and native['pages'][0]['active'],tag
 layers=native['pages'][0]['layers']
 for layer in layers:
  assert layer['screen_pixels']<=limits[layer['name']]+.01,(tag,layer)
  assert abs(layer['angle'])<=1.31,(tag,layer)
 if tag in ['outside','keyboard','reopened']:
  page=native['pages'][0];assert math.hypot(page['pointer_x'],page['pointer_y'])<.03,(tag,'did not return to center')
 if tag=='disabled':
  assert not native['input_registered'] and not native['pages'][0]['active']
  assert all(x['screen_pixels']<.001 and abs(x['angle'])<.001 for x in layers)
 report[tag]={'pixel_caps':True,'fixed_character_floor_ui':True,'pointer':[native['pages'][0]['pointer_x'],native['pages'][0]['pointer_y']]}
for tag,axis,sign in [('right_edge','pointer_x',1),('top_edge','pointer_y',-1)]:
 d=json.loads((out/(tag+'.json')).read_text());assert d['native']['pages'][0][axis]*sign>.7,(tag,'desktop pointer not reflected')
closed=json.loads((out/'closed_native.json').read_text(encoding='utf-8-sig'))
assert closed['state_count']==0 and not closed['ticker_registered'] and not closed['input_registered'],closed
samples=json.loads((out/'samples_lifecycle.json').read_text())
assert all(s.get('closed') or all(s['fixed'][n]==base['fixed'][n]['transform'] for n in base['fixed']) for s in samples)
assert max(abs(s['moving']['DepthRearOrbit'][2]) for s in samples if 'moving' in s)>.5
report['close_cleanup']=True;report['sample_count']=len(samples)
(out/'verification.json').write_text(json.dumps(report,indent=2));print('DEPTH_RUNTIME_QA_PASSED',len(report),'checks')
