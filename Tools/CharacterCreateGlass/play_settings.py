# -*- coding: utf-8 -*-
"""Keeps the user's play-window settings across a capture run.

The capture opens a 1920x1078 floating play window; the editor writes those values to
Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini when play starts, so they have to be put back on disk afterwards.
  python play_settings.py save <snapshot.json>      before the run
  python play_settings.py restore <snapshot.json>   after the editor has exited
"""
import json
import pathlib
import sys

INI = pathlib.Path(__file__).resolve().parents[2] / 'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini'
KEYS = ('NewWindowWidth', 'NewWindowHeight', 'LastExecutedPlayModeType')


def read():
    raw = INI.read_bytes()
    encoding = 'utf-16' if raw[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig' if raw[:3] == b'\xef\xbb\xbf' else 'utf-8'
    return raw.decode(encoding), encoding


mode, snapshot = sys.argv[1], pathlib.Path(sys.argv[2])
text, encoding = read()
newline = '\r\n' if '\r\n' in text else '\n'
lines = text.split(newline)
if mode == 'save':
    found = {key: line for line in lines for key in KEYS if line.startswith(key + '=')}
    snapshot.write_text(json.dumps(found, indent=1), encoding='utf-8')
    print('play settings saved:', found)
else:
    wanted = json.loads(snapshot.read_text(encoding='utf-8'))
    changed = 0
    for index, line in enumerate(lines):
        for key, original in wanted.items():
            if line.startswith(key + '=') and line != original:
                lines[index] = original; changed += 1
    if changed:
        INI.write_bytes(newline.join(lines).encode(encoding))
    print('play settings restored (%d line(s) changed)' % changed)
