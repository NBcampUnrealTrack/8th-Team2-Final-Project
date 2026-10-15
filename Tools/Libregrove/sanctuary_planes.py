import json
import numpy as np
from precision_workflow import record
t=record('LG_07_Sanctuary_Normals24_after')['topology'];v=np.array(t['vertices']);n=np.array(t['face_normals']);area=np.array(t['face_areas']);c=np.array([v[f].mean(axis=0) for f in t['faces']])
ids=np.flatnonzero((n[:,2]>.96)&(area>.012)&(c[:,2]<1.3))
for i in sorted(ids,key=lambda i:-area[i])[:22]:print(json.dumps({'id':int(i),'area':float(area[i]),'center':c[i].tolist(),'radius_range':np.linalg.norm(v[t['faces'][i]][:,:2],axis=1)[[np.argmin(np.linalg.norm(v[t['faces'][i]][:,:2],axis=1)),np.argmax(np.linalg.norm(v[t['faces'][i]][:,:2],axis=1))]].tolist()}))
