"""Read-only reconstruction of fixed review-camera rays; exact nearest vertices from MCP."""
import json
import numpy as np
from precision_workflow import record

def probe(name,view,pixels,axis,plane):
    t=record(name+'_Normals24_after')['topology'];v=np.array(t['vertices']);center=(v.min(axis=0)+v.max(axis=0))/2;scale=np.ptp(v,axis=0).max()*1.32
    d=np.array({'front_right':[1,-1,.68],'back_left':[-1,1,.68]}[view]);d/=np.linalg.norm(d);forward=-d;right=np.cross(forward,[0,0,1]);right/=np.linalg.norm(right);up=np.cross(right,forward)
    for px,py in pixels:
        origin=center+right*((px-384)/768*scale)+up*((384-py)/768*scale)
        point=origin+forward*((plane-origin[axis])/forward[axis]);distance=np.linalg.norm(v-point,axis=1);ids=np.argsort(distance)[:8]
        loops=sorted(t['boundary_loops'],key=lambda loop:np.linalg.norm(np.array(loop['bounds']['center'])-point))[:3]
        print(json.dumps({'pixel':[px,py],'plane_hit':point.tolist(),'vertices':[{'id':int(i),'co':v[i].tolist(),'distance':float(distance[i])} for i in ids],'loops':[{'id':l['id'],'simple':l['simple_closed'],'vertices':l['vertex_ids'],'bounds':l['bounds']} for l in loops]}))

probe('LG_05_WatermillHouse','back_left',[(446,547),(446,481)],0,-1.065)
probe('LG_05_WatermillHouse','front_right',[(176,349),(194,357)],1,-1.49)
probe('LG_04_Workshop','front_right',[(350,506),(675,468)],0,1.06)
