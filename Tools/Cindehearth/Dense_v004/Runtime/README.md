# Archive anchors for the compact Cindehearth layout

`ANUCindehearthDirector::UseArchiveLadder()` now reads the world locations of exactly one `CH.Archive.Entry` actor and one `CH.Archive.Lower` actor when F is pressed. Each actor must mark the desired **standing pawn origin**, not the floor contact point. Actor rotation and scale do not change the pawn's facing or size.

The player must be within 160 cm horizontally and 80 cm vertically of a side. The nearest eligible side leads to the opposite anchor. Q7 and cinematic gating remain. There is no absolute Z=120 split. This preserves interaction when the assay/archive cluster moves horizontally or vertically.

Maps with neither tag retain the old entry `(1300, 292.5, 215)` and lower `(1395, 350, 29)` positions. A partially configured pair, duplicate tag, one actor tagged as both sides, or non-finite location cancels the interaction instead of teleporting into the old layout. Header and reflection schema are unchanged.

## Verification and coordinated rebuild

`python -X utf8 Tools/Cindehearth/Dense_v004/Runtime/check_syntax.py` passed with MSVC 14.44 `/Zs` against the real UE 5.8 translation unit and generated include definitions. This pass generated no object or DLL and did not modify the editor. Its compiler working directory is `UE_5.8/Engine/Source`, as required by the existing relative include paths. See `syntax_only.json` and `syntax_only.log`.

The existing successful full build route is recorded in `C:/Users/Ndus_072/AppData/Local/UnrealBuildTool/Log.txt` (2026-09-05 07:08:29 UTC command, `Result: Succeeded`, 23.90 seconds) and `Saved/Revvy/BuildLogs/octopath_camera_build.out.log` (same target and options). After saving and closing the editor, the coordinator can use:

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' ProjectMEditor Win64 Development '-Project=C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM/ProjectM.uproject' -WaitMutex -NoHotReloadFromIDE
```

No verified live-patch invocation was found. Do not treat syntax success as a loaded runtime change.

After rebuilding/reopening and starting PIE, run `qa_archive_anchors.py` through the existing UE Python executor. It verifies Q6 gating, bidirectional Q7 traversal at the new anchors, and rejection from remote horizontal and vertical positions. It restores the prior pawn position, chapter stage and epilogue setting. It does not start or stop PIE. The coordinator must additionally allow floor physics to settle on both sides and rerun the existing cinematic/input-restoration checks. This script has been authored but not executed by the runtime-anchor subagent.

## Coordinated full build and reopen completed

On 2026-09-05, after the three compact-layout review captures completed, the runtime subagent saved the current map and orderly closed editor PID13572. Dirty-package audit found no dirty or unrelated packages. `build_editor.py` compiled `NUCindehearthDirector.cpp`, linked `UnrealEditor-ProjectM.dll`, and completed successfully in 8.19 seconds. Evidence: `prebuild_dirty.json`, `full_build.log`, and `full_build.json`.

Editor PID48548 reopened `/Game/Maps/L_Cindehearth` using normal project settings, and the existing proxy on port8091 reconnected. Read-only verification found 3,303 actors, all106 connection-dressing actors with collision disabled, one archive anchor per side, Q0/day, PIE off, and no dirty packages. The west lower stair is deliberately at X=-399 following the coordinator's final +60cm clearance adjustment. Saved archive entry is `(1077.7,195.975,215)`; lower is `(1172.7,253.475,29)`. See `restart_launch.json` and `reopen_verified.json`. No scene edits or runtime PIE tests were performed during this build/restart task.
