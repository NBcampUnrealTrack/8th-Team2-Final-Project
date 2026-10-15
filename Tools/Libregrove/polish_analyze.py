"""Read-only selection planning from freshly inspected V3 topology."""
import json
from polish_v3 import ASSETS,DOC,record

rows=[]
for n in range(1,10):
    name=ASSETS[n]['id'];r=record(f'V3_{name}_BASE_INSPECT');t=r['topology']
    big=sorted(t['components'],key=lambda c:-c['face_count'])[:12]
    result={'id':name,'components':len(t['components']),'large':[{'id':c['id'],'faces':c['face_count'],'area':round(c['area'],3),'bounds':c['bounds']} for c in big]}
    rows.append(result);print(json.dumps(result))
(DOC/'component_analysis.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
