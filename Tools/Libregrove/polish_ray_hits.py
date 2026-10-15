"""Read-only Moller-Trumbore ray tests against inspected triangle arrays."""
import json
import numpy as np
from polish_v3 import ASSETS,DOC,record

def hits(n,pixels,view='front_right',stage='BASE_INSPECT'):
    t=record(f'V3_{ASSETS[n]["id"]}_{stage}')['topology'];v=np.array(t['vertices']);f=np.array(t['faces']);tri=v[f]
    center=(v.min(0)+v.max(0))/2;scale=np.ptp(v,axis=0).max()*1.32
    d=np.array({'front_right':[1.,-1,.68],'back_left':[-1.,1,.68]}[view]);d/=np.linalg.norm(d);forward=-d;right=np.cross(forward,[0,0,1]);right/=np.linalg.norm(right);up=np.cross(right,forward)
    e1=tri[:,1]-tri[:,0];e2=tri[:,2]-tri[:,0];h=np.cross(forward,e2);a=np.einsum('ij,ij->i',e1,h);inv=np.divide(1,a,out=np.zeros_like(a),where=np.abs(a)>1e-10)
    out=[]
    for px,py in pixels:
        origin=center+right*((px-384)/768*scale)+up*((384-py)/768*scale)+d*10
        s=origin-tri[:,0];u=inv*np.einsum('ij,ij->i',s,h);q=np.cross(s,e1);b=inv*(q@forward);dist=inv*np.einsum('ij,ij->i',e2,q)
        valid=np.flatnonzero((np.abs(a)>1e-10)&(u>=0)&(b>=0)&(u+b<=1)&(dist>0));valid=valid[np.argsort(dist[valid])][:5]
        out.append({'pixel':[px,py],'hits':[{'face':int(i),'point':(origin+forward*dist[i]).tolist(),'vertices':f[i].tolist(),'coords':tri[i].tolist(),'normal':t['face_normals'][i]} for i in valid]})
    return {'n':n,'view':view,'stage':stage,'probes':out}
if __name__=='__main__':
    out=[hits(4,[(350,506),(675,370)]),hits(5,[(173,348),(194,357)]),hits(1,[(277,260),(286,253)]),hits(5,[(446,547),(446,451)],'back_left')]
    dest=DOC/'ray_hits01.json';assert not dest.exists();dest.write_text(json.dumps(out,indent=2),encoding='utf-8');print(json.dumps(out))
