"""Fit candidate backing panels to inspected roof surfaces. Requires visual QA after applying."""
import json,math
import numpy as np
from precision_workflow import record,DOC

names=['LG_01_Cottage_A','LG_02_Inn','LG_03_VillageHall','LG_04_Workshop','LG_05_WatermillHouse']
plans=[]
for name in names:
    t=record('precision_base_'+name)['topology'];v=np.array(t['vertices']);norm=np.array(t['face_normals']);area=np.array(t['face_areas']);center=np.array([v[f].mean(axis=0) for f in t['faces']]);height=np.ptp(v[:,2])
    base=(center[:,2]>.63*height)&(norm[:,2]>.4)&(norm[:,2]<.93)&(area>1e-5)
    strength=[float(area[base&(np.abs(norm[:,k])>.4)].sum()) for k in (0,1)]
    axis=int(np.argmax(strength));along=1-axis;planes=[];panels=[]
    for side in (-1,1):
        sel=np.flatnonzero(base&(norm[:,axis]*side>.4)&(np.abs(norm[:,along])<.25)&(center[:,axis]*side>.06))
        pts=center[sel];wt=area[sel];assert len(sel)>20,(name,axis,side,len(sel))
        fit=np.polyfit(pts[:,axis],pts[:,2],1,w=np.sqrt(wt))
        for _ in range(5):
            err=np.abs(pts[:,2]-(pts[:,axis]*fit[0]+fit[1]));keep=err<max(.018,float(np.quantile(err,.62)))
            fit=np.polyfit(pts[keep,axis],pts[keep,2],1,w=np.sqrt(wt[keep]))
        lo,hi=np.quantile(v[:,axis],[0,1]);along_lo,along_hi=v[:,along].min(),v[:,along].max()
        # Backing stops inside the fascia; top face is a few millimeters beneath
        # the robust shingle plane. Two slopes remain independent closed solids.
        start=0.;end=(lo+.025 if side<0 else hi-.025);mid=(start+end)*.5;z=fit[0]*mid+fit[1]-.023
        length=abs(end-start)*math.sqrt(1+fit[0]**2);span=along_hi-along_lo-.075;thickness=.022
        c=[0.,0.,float(z)];c[axis]=float(mid);c[along]=float((along_lo+along_hi)/2)
        dims=[0.,0.,thickness];dims[axis]=float(length);dims[along]=float(span)
        rotation=[0.,0.,0.];rotation[along]=float(math.degrees(math.atan(fit[0]))*(1 if axis==1 else -1))
        panels.append({'center':c,'dimensions':dims,'rotation_degrees':rotation})
        planes.append({'side':side,'across_axis':axis,'slope':float(fit[0]),'intercept':float(fit[1]),'face_count':len(sel),'residual_median':float(np.median(np.abs(pts[:,2]-(pts[:,axis]*fit[0]+fit[1])))),'support_face_ids':[int(x) for x in sel]})
    out={'id':name,'panels':panels,'roof_planes':planes};plans.append(out)
    print(json.dumps({'id':name,'across_axis':axis,'panels':panels,'fits':[{k:v for k,v in p.items() if k!='support_face_ids'} for p in planes]}),flush=True)
(DOC/'roof_backing_plan.json').write_text(json.dumps(plans,indent=2),encoding='utf-8')
