"""Curate Fernbrook runtime assets and final evidence; never commit or push.

Usage: python Tools/FernbrookDelivery/prepare_git.py [--stage]
Preserves unrelated changes. Writes audit reports under ignored Saved/ only.
"""
import ast
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from inspect_package_names import read_names

ROOT = Path(__file__).resolve().parents[2]
REPORT = ROOT / 'Saved/FernbrookGitDelivery_20260911'
REPORT.mkdir(parents=True, exist_ok=True)

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT).decode('utf-8').strip()

selected = set()
def add(path):
    path = Path(path)
    if not path.is_absolute():
        path = ROOT / path
    path = path.resolve()
    rel = path.relative_to(ROOT).as_posix()
    assert rel.startswith(('Content/Fernbrook/', 'Docs/Fernbrook', 'Tools/Fernbrook')), rel
    assert path.is_file(), rel
    assert not set(path.parts) & {'evidence', '__pycache__'}, rel
    assert path.suffix not in {'.pyc', '.zip', '.blend', '.log'}, rel
    selected.add(rel)

for path in (ROOT / 'Content/Fernbrook').rglob('*'):
    if path.is_file():
        assert path.suffix in {'.uasset', '.umap'}, path
        add(path)

docs = {
    'Fernbrook': ['README.md'],
    'Fernbrook_CharacterLighting_20260911': [
        'README.md', 'final_signoff.json', 'graph_verified.json',
        'source_material_graph.json', 'baseline_hashes.json', 'movement_qa_002.json',
    ],
    'Fernbrook_Entrance_V1_20260911': ['HD2D_CONTROLS.md'],
    'Fernbrook_Polish_V3_20260911': [
        'CONTROLS.md', 'final_signoff.json', 'final_live_state.json',
        'final_material_and_postprocess.json',
    ],
    'Fernbrook_Textured_V5_20260911': [
        'README.md', 'final_signoff.json', 'final_readback_handoff001.json',
        'asset_runtime_stats.csv', 'GameplayQA/summary_V5_Final_002.json',
    ],
    'Fernbrook_TreePolish_V6_20260911': [
        'README.md', 'final_signoff.json', 'final_readback_handoff001.json',
        'baseline.json', 'meshes_001.json', 'GameplayQA/summary_V6_001.json',
    ],
}
for folder, names in docs.items():
    for name in names:
        add(Path('Docs') / folder / name)
for path in (ROOT / 'Tools/Fernbrook_CharacterLighting_20260911').iterdir():
    if path.is_file() and path.suffix in {'.py', '.json'}:
        add(path)
for path in [
    'Tools/Fernbrook_Entrance_V1_20260911/client.py',
    'Tools/Fernbrook_Entrance_V1_20260911/game_shot.py',
    'Tools/FernbrookDelivery/prepare_git.py',
    'Tools/FernbrookDelivery/inspect_package_names.py',
    'Tools/FernbrookDelivery/validate_staged.py',
]:
    add(path)

# Include every relative Markdown-linked file, without copying raw evidence trees.
for _ in range(4):
    previous = len(selected)
    for rel in sorted(selected.copy()):
        if not rel.endswith('.md'):
            continue
        body = (ROOT / rel).read_text(encoding='utf-8-sig')
        for match in re.finditer(r'\]\(([^)#]+)(?:#[^)]*)?\)', body):
            target = match[1].strip('<>')
            if re.match(r'^[a-zA-Z]+:|^/', target):
                continue
            add((ROOT / rel).parent / target)
    if previous == len(selected):
        break
# Retain original capture receipts for included PNG files when available.
for rel in sorted(selected.copy()):
    if rel.endswith('.png'):
        receipt = (ROOT / rel).with_name(Path(rel).stem + '_receipt.json')
        if receipt.is_file():
            add(receipt)

known_secrets = [os.environ.get(key, '') for key in (
    'REVVY_FULL_ACCESS_TOKEN', 'TRIPO_API_KEY', 'TRIPO3D_API_KEY')]
patterns = [
    r'(?i)\b(?:ghp_|github_pat_|sk-|tsk_)[A-Za-z0-9_-]{20,}',
    r'(?i)\b(?:Bearer|Basic)\s+[A-Za-z0-9+/_.=-]{24,}',
    r'(?i)[?&](?:X-Amz-Signature|X-Goog-Signature|signature|sig|access_token|token)=',
    r'-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----',
    r'\beyJ[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{16,}',
]
findings = []
for rel in sorted(selected):
    f = ROOT / rel
    if f.suffix in {'.py', '.json', '.md', '.csv'}:
        body = f.read_text(encoding='utf-8-sig')
        if any(len(s) >= 16 and s in body for s in known_secrets) or any(re.search(p, body) for p in patterns):
            findings.append(rel)
        if f.suffix == '.py':
            ast.parse(body, filename=rel)
        elif f.suffix == '.json':
            json.loads(body)
assert not findings, 'Potential credentials/signed URLs; paths only: ' + repr(findings)

# Conservative offline reference-string audit, not an Unreal registry/cook test.
tracked = set(git('ls-files', '-z').split('\0'))
available = tracked | selected
package_refs = set()
for rel in sorted(selected):
    if not rel.startswith('Content/'):
        continue
    raw = (ROOT / rel).read_bytes()
    for data in (raw, raw.decode('utf-16-le', errors='ignore').encode('ascii', errors='ignore')):
        for match in re.finditer(rb'/Game/[A-Za-z0-9_/-]+(?:\.[A-Za-z0-9_]+)?', data):
            package_refs.add(match[0].decode().split('.')[0])
name_table_refs=set()
package_audit=[]
for rel in sorted(selected):
    if rel.startswith('Content/'):
        row=read_names(ROOT/rel);row['file']=rel
        name_table_refs.update(row['package_names']);package_audit.append(row)
# This FBX-import history string is outside the serialized header; the real
# mesh name table references the included MI_FB1_Leaf, not the old material package.
bush=next(r for r in package_audit if r['file']=='Content/Fernbrook/Entrance_V1/Models/Reuse/SM_FB1_Bush.uasset')
assert not bush['legacy_leaf_package_in_name_table']
assert bush['legacy_leaf_utf16_offsets']==[26606] and bush['header_size']==15163
assert '/Game/Fernbrook/Entrance_V1/Materials/Instances/MI_FB1_Leaf' in bush['package_names']
metadata_strings={'/Game/Village/Meshes/MAT_Leaf'}
assert not metadata_strings & name_table_refs
unresolved = []
for ref in sorted(package_refs | name_table_refs):
    stem = 'Content/' + ref.removeprefix('/Game/')
    if (ROOT / stem).is_dir():
        continue
    if ref not in metadata_strings and not any(stem + ext in available for ext in ('.uasset', '.umap')):
        unresolved.append(ref)

files = [{'path':p, 'size':(ROOT/p).stat().st_size,
          'sha256':hashlib.sha256((ROOT/p).read_bytes()).hexdigest()} for p in sorted(selected)]
summary = {'files':files, 'count':len(files), 'bytes':sum(f['size'] for f in files),
           'potential_secret_findings':findings, 'offline_package_references':len(package_refs),
           'unresolved_reference_strings':unresolved,
           'name_table_package_references':len(name_table_refs),
           'known_import_history_strings':sorted(metadata_strings),
           'note':'Offline strings are a safety check, not a complete cooked asset dependency graph.'}
(REPORT / 'selection.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
(REPORT / 'package_name_tables.json').write_text(json.dumps(package_audit,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in summary.items() if k != 'files'}, indent=2))
assert not unresolved, 'Resolve missing package strings before staging; see local selection.json.'
if '--stage' in sys.argv:
    assert not git('diff','--cached','--name-only'), 'Index is not empty; preserve existing staged work.'
    paths = sorted(selected)
    for index in range(0,len(paths),50):
        subprocess.run(['git','add','--',*paths[index:index+50]],cwd=ROOT,check=True)
    actual=set(git('diff','--cached','--name-only','-z').split('\0'))-{''}
    assert actual==selected, 'Index differs from selected scope; inspect before any commit.'
    print('Scoped staging complete. No commit or push performed.')
