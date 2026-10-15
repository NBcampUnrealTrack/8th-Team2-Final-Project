# -*- coding: utf-8 -*-
"""Points the login screen at the Google login widget (or back): plain-text edit of Config/DefaultGame.ini, no editor needed.

  python Tools/GoogleLogin/apply_ini.py            # apply
  python Tools/GoogleLogin/apply_ini.py --revert   # remove our lines (the old WBP_Login comes back)

Writes only its own lines, keeps everything else (other sessions add lines to this file too) and the file's line endings.
ClientId / ClientSecret are never overwritten once someone has filled them in.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
INI = ROOT / 'Config' / 'DefaultGame.ini'
SECTION = '[/Script/ProjectM.NUGoogleAuthSettings]'
OVERRIDE = 'LoginWidgetOverride=/Game/Art/UI/Login/WBP_Login_Google.WBP_Login_Google_C'
COOK = '+DirectoriesToAlwaysCook=(Path="/Game/Art/UI/Login")'
PACKAGING = '[/Script/UnrealEd.ProjectPackagingSettings]'


def main(revert):
    raw = INI.read_text(encoding='utf-8-sig')
    nl = '\r\n' if '\r\n' in raw else '\n'
    lines = raw.split(nl)

    if revert:
        lines = [l for l in lines if l.strip() not in (OVERRIDE, COOK)]
    else:
        if SECTION not in lines:
            if lines and lines[-1].strip():
                lines.append('')
            lines += [SECTION, OVERRIDE, 'ClientId=', 'ClientSecret=', 'bServerVerifiesIdToken=True', '']
        else:
            start = lines.index(SECTION)
            end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith('[')), len(lines))
            body = lines[start + 1:end]
            body = [l for l in body if not l.startswith('LoginWidgetOverride=')]
            body.insert(0, OVERRIDE)
            lines = lines[:start + 1] + body + lines[end:]
        if COOK not in lines:
            start = lines.index(PACKAGING)
            end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith('[')), len(lines))
            last = max(i for i in range(start, end) if lines[i].startswith('+DirectoriesToAlwaysCook') or i == start)
            lines.insert(last + 1, COOK)

    out = nl.join(lines)
    if out != raw:
        INI.write_text(out, encoding='utf-8', newline='')
        print('DefaultGame.ini', 'reverted' if revert else 'applied')
    else:
        print('DefaultGame.ini unchanged')


if __name__ == '__main__':
    main('--revert' in sys.argv)
