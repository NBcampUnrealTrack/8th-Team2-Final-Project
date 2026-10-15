"""Compatibility entry point for the v004 mixed-transform Gorm route."""
from pathlib import Path
import unreal
STAGE = 'walk:gorm'
exec((Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004/QA/qa_runtime.py').read_text(encoding='utf-8'))
