"""Read GLB structure and geometry outside Blender for exported-unit/UV QA."""
import json,struct,sys
from pathlib import Path
import numpy as np

def read_glb(path):
    raw=Path(path).read_bytes(); magic,version,size=struct.unpack_from('<III',raw)
    assert magic==0x46546C67 and version==2 and size==len(raw)
    offset=12;model=None; binary=None
    while offset<len(raw):
        length,kind=struct.unpack_from('<II',raw,offset);offset+=8
        chunk=raw[offset:offset+length];offset+=length
        if kind==0x4E4F534A:model=json.loads(chunk)
        elif kind==0x004E4942:binary=chunk
    return model,binary

def accessor(model,binary,index):
    a=model['accessors'][index];v=model['bufferViews'][a['bufferView']]
    dtype={5126:'<f4',5125:'<u4',5123:'<u2',5122:'<i2',5121:'u1',5120:'i1'}[a['componentType']]
    count={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
    dt=np.dtype(dtype);stride=v.get('byteStride',dt.itemsize*count)
    return np.ndarray((a['count'],count),dtype=dt,buffer=binary,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(stride,dt.itemsize)).copy()

def node_matrix(node):
    if 'matrix' in node:return np.array(node['matrix']).reshape(4,4).T
    t=np.eye(4);t[:3,3]=node.get('translation',[0,0,0]);s=np.diag(node.get('scale',[1,1,1])+[1])
    x,y,z,w=node.get('rotation',[0,0,0,1]);r=np.eye(4)
    r[:3,:3]=[[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]]
    return t@r@s

def inspect(path):
    model,binary=read_glb(path);points=[];primitives=[]
    def walk(index,parent):
        n=model['nodes'][index];matrix=parent@node_matrix(n)
        if 'mesh' in n:
            for p in model['meshes'][n['mesh']]['primitives']:
                pos=accessor(model,binary,p['attributes']['POSITION']);world=(np.column_stack((pos,np.ones(len(pos))))@matrix.T)[:,:3]
                points.extend(world.tolist())
                indices=accessor(model,binary,p['indices']).reshape(-1) if 'indices' in p else np.arange(len(pos))
                info={'node':n.get('name'),'vertices':len(pos),'triangles':len(indices)//3,'attributes':list(p['attributes'])}
                if 'TEXCOORD_0' in p['attributes']:
                    uv=accessor(model,binary,p['attributes']['TEXCOORD_0'])
                    info['uv_min']=uv.min(axis=0).tolist();info['uv_max']=uv.max(axis=0).tolist();info['uv_nonfinite']=int((~np.isfinite(uv)).sum())
                primitives.append(info)
        for child in n.get('children',[]):walk(child,matrix)
    for root in model['scenes'][model.get('scene',0)]['nodes']:walk(root,np.eye(4))
    vertices=np.array(points)
    return {'path':str(path),'world_min_m':vertices.min(axis=0).tolist(),'world_max_m':vertices.max(axis=0).tolist(),'dimensions_m_xyz_y_up':np.ptp(vertices,axis=0).tolist(),'primitives':primitives,'total_render_vertices':sum(p['vertices'] for p in primitives),'total_triangles':sum(p['triangles'] for p in primitives),'materials':model.get('materials',[]),'images':model.get('images',[])}

if __name__=='__main__':print(json.dumps(inspect(sys.argv[1]),indent=2))
