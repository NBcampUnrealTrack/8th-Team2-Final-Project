# Compact Cindehearth QA — authored, not executed

These scripts preserve v003 files and evidence. They have not been executed against Unreal by the authoring agent. A syntax check or route listing does not prove map correctness.

The coordinator must finish the v004 actor transformations/building replacement, save the map, and rebuild/reload the archive-anchor C++ before running the full suite:

```powershell
python -X utf8 Tools/Cindehearth/Dense_v004/QA/run.py --execute
```

Use `--reuse-pie` only for an already-running fresh session in `L_Cindehearth`. A session that has already played the one-shot epilogue must be restarted before a full-suite rerun. Without `--execute`, the script only lists its intended checks and creates no receipts.

Evidence is isolated in `QA/receipts/<new-run-id>/`. Existing run folders are rejected. `QA/latest_run.json` is written only when a real run starts. Its `passed` value remains false until all requested checks complete. A subset run has `full_suite:false`, even when its selected checks pass.

`routes.py` encodes the compact affine transform, the 0.9 Gorm-local offsets, lower west stair X−399, and the agreed landmark/alley exceptions. It contains **14 routes / 72 waypoints**: six original routes (39 waypoints), four stair routes, smithy/tavern approaches and two housing seams. Update the data if the final layout shifts these anchors; do not relax collision checks to conceal incorrect routes.

`qa_runtime.py` uses real movement input after one initial placement per isolated route. Every waypoint requires XY proximity, a walkable floor and the expected terrace height within40 cm. It records samples, failures and progress timeouts. Tight alley/Gorm tolerances are26 cm; general routes58 cm; stairs/approaches40 cm.

The full suite also verifies displayed mesh-component states, 24 nighttime residents through Q9/Q12, Q7-only clue placement, the runtime agent's five archive-anchor cases, settled floor physics on both ladder sides, cinematic ladder gating, mandatory epilogue input/camera recovery and the configured gameplay camera. It leaves PIE running for screenshots and saves no editor-level state.

For a focused retest after a documented correction:

```powershell
python -X utf8 Tools/Cindehearth/Dense_v004/QA/run.py --execute --reuse-pie --movement-only --routes housing_north_alley stairs_lower_west
```

The runtime anchor integration changes only its output directory in the executed code string; the original `Runtime/qa_archive_anchors.py` remains untouched. All five-case evidence is copied directly into the current run folder by that script.
