import sys,runpy
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools/Fernbrook_Entrance_V1_20260911'))
import client as c
c.DOC=ROOT/'Docs/Fernbrook_CharacterLighting_20260911'
sys.stdout.reconfigure(encoding='utf-8')
runpy.run_path(str(ROOT/'Tools/Fernbrook_Entrance_V1_20260911/game_shot.py'),run_name='__main__')
