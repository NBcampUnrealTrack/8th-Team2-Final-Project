"""Resume untouched captures after read-only transport preflight interruption."""
import sys,json,runpy
import textured_handoff as h
rows=json.loads((h.DEST/'export_manifest.json').read_text(encoding='utf-8'))
for row in rows[4:]:
    for angle in ['FrontReview','BackReview']:
        sys.argv=['capture_unreal_v7.py',row['id'],angle]
        runpy.run_path(str(h.ROOT/'Tools/Libregrove/capture_unreal_v7.py'),run_name='__main__')
for angle in ['Top','Oblique']:
    sys.argv=['capture_unreal_v7.py','Overview',angle]
    runpy.run_path(str(h.ROOT/'Tools/Libregrove/capture_unreal_v7.py'),run_name='__main__')
