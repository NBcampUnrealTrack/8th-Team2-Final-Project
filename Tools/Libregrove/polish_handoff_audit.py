"""Read-only final package/links/artifact audit; writes a new audit receipt only."""
import csv,hashlib,json,zipfile,xml.etree.ElementTree as ET
from collections import Counter
from html.parser import HTMLParser
from pathlib import Path
from polish_v3 import DOC,ROOT,record,ASSETS
from polish_package import DEST,ARCHIVE

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
manifest=json.loads((DEST/'manifest.json').read_text(encoding='utf-8'))
for f in manifest['files']:
    p=(DEST/f['path']).resolve();p.relative_to(DEST.resolve());assert p.stat().st_size==f['bytes'] and sha(p)==f['sha256']
assert sha(manifest['original_file'])==manifest['original_sha256']
with zipfile.ZipFile(ARCHIVE) as z:
    assert z.testzip() is None
    for name in z.namelist():assert z.read(name)==(DEST/name).read_bytes()
    zipped=len(z.namelist())
class Links(HTMLParser):
    def __init__(self):super().__init__();self.links=[]
    def handle_starttag(self,tag,attrs):
        for key,value in attrs:
            if key in ('href','src'):self.links.append(value)
page=Links();page.feed((DEST/'미리보기.html').read_text(encoding='utf-8'))
for link in page.links:assert (DEST/link).is_file(),link
types=Counter(p.suffix for p in DEST.rglob('*') if p.is_file())
assert (types['.fbx'],types['.glb'],types['.blend'],types['.png'],types['.svg'])==(9,9,10,18,9)
uv_triangles=sum(len(ET.parse(p).findall('.//{http://www.w3.org/2000/svg}polygon')) for p in DEST.rglob('*.svg'))
assert uv_triangles==manifest['triangle_total_v3']
with (DEST/'모델별_버텍스_크기_UV_검수표.csv').open(encoding='utf-8-sig',newline='') as f:rows=list(csv.DictReader(f))
assert len(rows)==9 and sum(int(r['final_triangles']) for r in rows)==manifest['triangle_total_v3']
unchanged=[]
for n in (6,8,9):
    name=ASSETS[n]['id'];base=record(f'V3_{name}_BASE_INSPECT')['topology'];out=record(f'V3_{name}_DELIVERY_REOPEN')['topology']
    assert base['vertices']==out['vertices'] and base['faces']==out['faces'];unchanged.append(name)
jobs=[]
for parent in (ROOT/'Saved/LibregrovePrecision',ROOT/'Saved/CindehearthRepair'):
    for p in parent.glob('V3_*'):
        if (p/'request.json').is_file():
            assert (p/'result.json').is_file(),('Nonterminal job',p.name)
            result=json.loads((p/'result.json').read_text(encoding='utf-8'));assert result['state'] in ('finished','failed');jobs.append(result['state'])
result={'passed':True,'package_files':zipped,'hashes_verified':len(manifest['files']),'html_links_verified':len(page.links),'format_counts':dict(types),'uv_triangles':uv_triangles,'csv_models':9,'original_sha256_unchanged':True,'v2_geometry_exactly_unchanged':unchanged,'terminal_job_counts':dict(Counter(jobs)),'pending_mutation_jobs':0,'zip_sha256':sha(ARCHIVE)}
dest=DOC/'handoff_audit.json';assert not dest.exists();dest.write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result),flush=True)
