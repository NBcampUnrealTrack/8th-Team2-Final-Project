"""Verify the exact curated index, source syntax, and LFS objects without mutation."""
import ast
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[2]
REPORT=ROOT/'Saved/FernbrookGitDelivery_20260911'
manifest=json.loads((REPORT/'selection.json').read_text(encoding='utf-8'))
def git(*args):
    return subprocess.check_output(['git',*args],cwd=ROOT)
expected={f['path'] for f in manifest['files']}
actual=set(git('diff','--cached','--name-only','-z').decode().split('\0'))-{''}
assert actual==expected,(sorted(actual-expected),sorted(expected-actual))
subprocess.run(['git','diff','--cached','--check'],cwd=ROOT,check=True)
counts={'files':len(actual),'lfs_pointers':0,'lfs_bytes':0,'python':0,'json':0}
for row in manifest['files']:
    path=row['path'];raw=(ROOT/path).read_bytes()
    assert hashlib.sha256(raw).hexdigest()==row['sha256'],path
    staged=git('show',':'+path)
    suffix=Path(path).suffix
    if suffix in {'.uasset','.umap','.png'}:
        match=re.fullmatch(rb'version https://git-lfs.github.com/spec/v1\noid sha256:([a-f0-9]{64})\nsize (\d+)\n',staged)
        assert match,path
        oid=match[1].decode();size=int(match[2]);assert oid==row['sha256'] and size==row['size'],path
        obj=ROOT/'.git/lfs/objects'/oid[:2]/oid[2:4]/oid
        assert obj.is_file() and obj.stat().st_size==size,path
        assert hashlib.sha256(obj.read_bytes()).hexdigest()==oid,path
        counts['lfs_pointers']+=1;counts['lfs_bytes']+=size
    else:
        assert staged.replace(b'\r\n',b'\n')==raw.replace(b'\r\n',b'\n'),path
        if suffix=='.py':
            ast.parse(staged.decode('utf-8-sig'),filename=path);counts['python']+=1
        elif suffix=='.json':
            json.loads(staged.decode('utf-8-sig'));counts['json']+=1
result={'passed':True,**counts,'unrelated_staged_files':0,'whitespace_errors':0,
        'secrets_findings':manifest['potential_secret_findings'],
        'unresolved_package_strings':manifest['unresolved_reference_strings']}
(REPORT/'staged_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
