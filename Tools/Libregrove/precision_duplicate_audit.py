"""Read-only duplicate triangle audit using exact MCP-inspected topology."""
import json
from collections import defaultdict
from precision_workflow import DOC,record

for p in sorted(DOC.glob('LG_*_DELIVERY.json')):
    h=json.loads(p.read_text(encoding='utf-8'));name=h['id'];r=record(name+'_EXPORT_V2_BEFORE');t=r['topology'];v=t['vertices'];groups=defaultdict(list);co=defaultdict(list)
    for i,face in enumerate(t['faces']):
        groups[tuple(sorted(face))].append(i);co[tuple(sorted(tuple(v[j]) for j in face))].append(i)
    duplicates=[ids for ids in groups.values() if len(ids)>1];coordinate=[ids for ids in co.values() if len(ids)>1]
    normals=t['face_normals'];details=[{'ids':g,'same_vertex_ids':tuple(sorted(t['faces'][g[0]]))==tuple(sorted(t['faces'][g[1]])),'normal_dot':sum(a*b for a,b in zip(normals[g[0]],normals[g[1]]))} for g in coordinate]
    print(json.dumps({'id':name,'duplicate_faces':details}))
