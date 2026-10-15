# Cindehearth architecture v004

Two new Tripo P2 native-quad building generations, repaired through typed Blender MCP. These are replacement building assemblies; Unreal placement and final composite acceptance belong to the main Dense_v004 task.

Authoritative receipt: [architecture_handoff.json](verification/architecture_handoff.json).

| Assembly part | Export receipt | Render triangles |
| --- | --- | ---: |
| Tavern exterior | [Tavern](verification/Tavern_export.json) | 3,142 |
| Tavern rear/side wall repairs | [TavernWallsFrame](verification/TavernWallsFrame_export.json) | 36 |
| Tavern inset front wall repair | [TavernWallsInset](verification/TavernWallsInset_export.json) | 12 |
| Open smithy structure | [SmithyStructure](verification/SmithyStructure_export.json) | 1,966 |
| Separate smithy roof | [SmithyRoof](verification/SmithyRoof_export.json) | 1,236 |
| Smithy rear/side wall repairs | [SmithyWalls](verification/SmithyWalls_export.json) | 24 |

All parts retain the same original building origin and identity object transforms. Front is +X; Y is width. Compute placement/scale from the complete Tavern or Smithy building bounds, then copy that transform to its parts. Never center or fit the roof/walls separately. Give only SmithyRoof the `CH.InteriorRoof` visibility tag.

**Tavern wall correction:** The bright front-view Unreal review exposed that the original `TavernWalls` front solid covered the door and first-floor window frames. It is superseded by `TavernWallsFrame` plus `TavernWallsInset`. Only the inspected front solid moved by local X=-0.14m; the other three solids retain identical coordinates. Front maximum X is now 0.53358m, behind the door at 0.63815m and window trim at 0.5484m or greater. Both replacement meshes remain closed. See [TavernWallsInset_fix.json](verification/TavernWallsInset_fix.json). Do not apply the older `TavernWalls_export.json`.

Use original nearest-sampled P2 textures on the body/roof and the existing HD2D stone world-aligned material on Walls. The wall meshes carry neutral matte preview materials and deliberately repeating cube UVs. The discarded unified caps stretched the P2 atlas; `WallReview010/012` show this rejected intermediate appearance and must not be used as final evidence.

Every final part has zero non-manifold edges, loose vertices, degenerate faces, invalid normals and non-finite geometry/UVs. The solidified repair walls are closed. P2 body surface patches and separated roof seams retain documented open boundaries, so the complete asset is not certified globally watertight. Front/back/top single-object review renders are under the exchange `Dense_v004/Architecture/Review` folder; final assembled material and gameplay camera QA must be completed in Unreal.

Source GLBs, reference images, original normalized `.blend` files, failed-operation receipts, successful repair copies and UV inspections are preserved. No arbitrary Blender modeling scripts were used. Stable job receipts live under `Saved/CindehearthRepair/dense_arch_*` and `Pipeline/verification/dense_arch_*`.

Tavern task: `4caa7db6-bf5c-462b-a2f7-2b6fc1428808`; Smithy task: `dda91e9f-07fe-4fb0-b1fa-683ddbfe6b0a`. Cost: 220 credits; verified account balance: 1,560, frozen: 0.
