# Cindehearth density audit — 2026-09-05

Read-only review of `layout.json`, `generate_layout.py`, v003 `level_before.json` / `final_placements.json`, final overview and actual player captures, and `NUCindehearthDirector.cpp/.h`. No editor mutations. This is an implementation recommendation, not a validation result.

## Finding

`final_village_day.png` confirms very large, continuously exposed dirt terraces surrounding isolated buildings. The P3 stone cladding leaves the old smithy and tavern massing recognizable. The player square is already reasonably occupied; shrinking every asset would make the emptiness and character-scale mismatch worse. Compress distances and terrain, rebuild the two landmarks, and preserve the hearth's local seating arrangement.

## Recommended transform

Use the root agent's proposed town transform in UE centimetres:

`F(x,y,z) = (540 + 0.74*(x-540), 0.67*y, z)`.

- Town footprint: 150×180 → **111×120.6 design metres**; physical UE footprint 67.5×81 → **49.95×54.27 metres**. Ground area becomes 49.58% of its previous value, approximately doubling object density without doubling object count.
- North remains +Y. Preserve terrace Z = 0 / 157.5 / 315 cm. Retaining rows move from Y ±1350 to **±904.5**; central ramps span **Y −1206…−603** and **603…1206**.
- Preserve player capsule dimensions: radius 24 / half-height 48 cm (`FieldPlayerCharacter.cpp:43`). Target clear minor alleys ≥100 cm, primary approaches 150–180 cm, and a continuous ≥90 cm pedestrian shoulder beside the rails. 48 cm is the mathematical body diameter, not a useful design clearance.
- Keep furniture, ore carts, foliage, Naia and functional props at current size. A uniform 0.9 housing scale is tolerable where needed; do not use nonuniform tree scaling.

## Geometry versus cluster transformations

For **unrotated baked world-coordinate meshes**, both location and geometry need F. At original actor origin (0,0,0), set location to **(140.4,0,0)**, multiply actor scale X by .74 and Y by .67, leave Z unchanged. These actors have far-away geometry despite their zero actor origin:

| Affine geometry family | Notes |
|---|---|
| `CH_Terrace0_Soil`, `CH_Terrace1_Soil`, `CH_Terrace2_Soil` | Includes baked assay hatch cutout; account for it below. |
| `CH_Railbed_Path` | Baked continuous ramps; correct world-affine Z preservation. |
| `CH_RetainingWalls_Stone`, `CH_RetainingWalls_StoneLight` | Contains both terrace retaining rows and stair gaps. |
| `CH_AshslopeRamps_Path`, `CH_CemeteryTrail_Path` | Baked slopes; move floor and adjacent collision together. |
| `CH_DeepveinTunnel_Path`, `CH_DeepveinTunnel_Rock` | The rock is a hidden source collider. Still transform it with the visible replacement. |
| `CH_OrganicVeins_Vein`, `CH_Chamber_25m_Rock`, `CH_Chamber_25m_Vein` | If compacting the whole connected world, transform the geometry, not just its origin. |

Also affine terrain-sized `CH_HD2D_BasinUnderlay`, `CH_P3_BackgroundBedrock`, `CH_P3_DepotFloor`, `CH_P3_ChamberFloor`, `*LandSkirt*`, `*MineBackfill*`, `*Bench*`, `CemeteryHill`, `PinehollowGround`, connection/path slabs and their collision. Re-query current actors because v003 final placements lists only the 711 new visible meshes, not all source/collision/lighting/quest actors.

**Exclude `CH_GreyveinSky` from terrain compaction.** Do not accidentally shrink the skydome or change its offset. Sun, skylight and post-process are not local dressing.

**Rail slope exception:** tilted `CH_Rail_*_Iron/Brass/Timber` modules contain local rotation. Merely multiplying local Y scale by .67 leaves their ends floating above or buried in the compressed railbed. For original slope angle a, use `a2=atan(tan(a)/.67)` and local Y length factor `sqrt((.67*cos(a))^2+sin(a)^2)`; preserve sign. Flat pieces may scale Y by .67. Keep rail X gauge/sleeper width and carts mutually consistent; retaining the old track gauge is preferable to narrowing it while retaining the old carts.

**Rigid groups:** for anchor C and local point p, use `p2 = F(C)+(p-C)`. For a uniformly .9 scaled house family use `p2=F(C)+.9*(p-C)` and scale each local accessory by .9. Do not individually affine all wall modules, as that opens seams around unscaled building pieces.

| Rigid family | Include all of these |
|---|---|
| Hearth | `CommunalHearth_10m`, `LivingEmbers`, `FeastTable_*`, `WarmingAshNaia_*`, `HearthSquare`, `PlazaPavers`, corresponding `CH_HD2D_YardEdge_HearthSquare_*`, `CH_EveningResident_*`, hearth lights and fire effects. Keep the 24 nighttime residents together. |
| Smithy | `DorikSmithy*`, `WorkingForge*`, `Anvil*`, `BellowsNaia*`, `SmithyApron*`, `CH_Dorik`, smithy light/sign/soot; replace the old building body and P3 cladding as one operation. |
| Tavern | `FreyaTavern*`, `TavernYard*`, `TavernCask*`, `CH_Freya`, tavern sign, window/soot/light/accessory families. |
| Assay / archive | `AssayOffice*`, `AssayWalk*`, `AssayWorktable*`, `AssayHatch*`, `ArchiveFloor*`, `ArchiveWall*`, `ArchiveEnd*`, `LedgerShelf*`, `UniqueQ7Documents*`, `CH_YoungAssayer`, Q7 and ladder anchors. Exception: hatch must align to the affine cutout. |
| Clinic | `MarenClinic*`, `ClinicCot*`, `CH_Maren`, clinic sign and Q6 marker. |
| Each house | `CH_GormHouse_P2` or `CH_MinersHome_XX_HD2D`, all original body colliders, `_ToolSign_*` and `_ToolSign_P3`, `CH_HD2D_Window_<ID>`, `CH_HD2D_Doorstep_<ID>_*`, `CH_HD2D_BurrowBank_<ID>`, `<ID>_HD2DSoot*`. Prefix-only matching misses accessories starting `CH_HD2D_`. |
| Gorm cinematics | Above Gorm family plus `CH_Messenger`, sealed letter/seal and epilogue-only lettering, Q12 marker, camera targets and serialized director route. |
| Work yards | `CH_HD2D_Yard_West_*`, `_East_*`, `_Smithy_*` each around its yard centre. Includes replacements ending `_P3`, roof, posts and long timber stacks. |
| Pine / rock replacements | All corresponding source collision, visible replacement and HD2D/P3 variants must move together, including originally hidden sources. Preserve hidden states. |

`CH_P3_TunnelWall_*`, `CH_P3_OrganicWall_*`, `CH_P3_ChamberRock_*`, `CH_P3_DistantRidge_*` and other large repeated wall/ridge pieces require either affine dimensions as well as centres or a new spacing pass; rigidly moving centres alone may make them overlap. Overlap is acceptable only for buried backfill, not walking corridors.

## Numeric cluster corrections after F

Most transformed building centres can be used directly. With the planned **560×400 cm tavern** and **470×350 cm smithy**:

| Group | Recommended new anchor (UE cm) | Reason |
|---|---:|---|
| Hearth | (−59.4, 0, 161.55) | F of old hearth; retain local seating geometry. |
| Dorik smithy | (−92.7, **715**, 157.5) | Plain F would place the centre at Y603 and its front among the northern feast table. Back edge 890 fits just below the retaining row at904.5. |
| Tavern | (−126, −1718.55, 0) | Affine centre and the new unscaled landmark footprint fit the lower band. |
| Assay | (1172.7, 241.2, 157.5) | Keep its −90 yaw; avoid losing cellar alignment. |
| Clinic | (1172.7, **−1530**, 0) | Plain F gives Y−1447.2 and only ~37 cm clear to Home04. The south offset restores ~120 cm. |
| Unloading shelter | (73.8, 1869.3, 315) | F centre; rigid interior contents. |
| Mine office | (939.6, 2291.4, 315) | F centre. |
| Main mine portal | (540, 2683.35, 315) | F centre; preserve opening size, align inner slope/walls. |

If needed, move the north feast table and associated seats south 40–50 cm to keep ≥100 cm in front of the compact smithy. No table should intrude into its entry or the rail footpath.

With 0.9 uniform house family scale, F causes **Home01/Home02 to overlap** and leaves only 35.5 cm between Gorm/Home01 and 52.2 cm between Home06/Home07. Use these additional final XY overrides; leave all other house centres at F:

| House | New X | New Y |
|---|---:|---:|
| Home01 | −1060 | −1230 |
| Home02 | −1435 | −1080 |
| Home03 | −1860 | −1260 |
| Home07 | −1350 | −1688.4 |
| Home08 | −1790 | −1780 |

These restore approximately 100–143 cm clear gaps in the affected pairs. Gorm's default new centre is (−692.1,−1085.4). Its adjacent west stair's default X is−459; the new gap to the house is only ~27 cm. Consider shifting the **entire lower west stair/cheek family +60…65 cm X** within the existing wider retaining-wall cutout, or moving Gorm southwest, then verify the approach. Do not move the stair alone while leaving blocking cheeks in place.

## Runtime and opening dependencies

- `NUCindehearthDirector.cpp:165` hardcodes archive proximity centre(1395,292.5), radius160; :166 below thresholdZ120; :168 upper destination(1300,292.5,215), lower(1395,350,29). The assigned runtime agent is replacing this with tagged anchors. Preserve destination capsule height offsets when moving anchors.
- The affine terrain hole centre becomes **(1172.7,195.975)**; original hole footprint99×117 cm becomes **73.26×78.39 cm**. An unscaled hatch translated with the assay building would instead be centred atY173.7. Align the hatch and tagged anchors with the actual opening, or repair the cutout. Do not assume moving the cellar alone solves it.
- `epilogue_camera_route` is a serialized transform array; moving `CH_Epilogue_*` TargetPoints does not alter it. Rebuild camera poses from transformed targets plus retained eye offsets so the close-up still frames the letter. Update `CH_EpilogueOnly` actors/letter, not only the camera.
- Move `CH_Q1_Arrival`, Q2/Q3/Q6/Q7/Q8/Q9/Q12 markers, Q4/Q5/bear/cemetery/Q10/Q11 markers, all named residents, and PlayerStart. Preserve hidden marker status.
- Preserve `CH.Q7.Blocker`, `CH.Q8.Blocker`, `CH.Q11.Cocoon`, `CH.Q11.Freed`, `CH.EpilogueOnly`, `CH.Window`, `CH.InteriorRoof`. Actor hidden state and mesh component visibility differ: former source colliders can remain hidden at component level even when a chapter transition shows an actor.
- `CH.InteriorRoof` tests an axis-aligned world bounds box against the pawn. New landmark roofs need the tag and correct bounds; do not tag a whole building because it would vanish on entry.
- Existing movement QA routes and actual-player capture coordinates are hardcoded old design positions. Transform waypoints with F and apply cluster-specific exceptions; retain Z floor references. Passing the old routes after a layout change proves nothing.

## Transition dressing priorities

All positions below are **new UE cm** under F. Keep details along the sides; avoid filling the readable walking strip with arbitrary rock scatter.

| Hotspot | Centre / corridor | Practical detail pass |
|---|---|---|
| South arrival / Pinehollow join | Around(540,−2713,0); outgoing road towardY−3600 | 3 short stone retaining segments per side, sparse roots/low rocks, timber bundles and a rail buffer off-axis. Blend cobble→ash→pine needles over400–600 cm; frame with two volumetric trees behind the path edges. No sudden rectangular texture seam. |
| Lower rail ramp | X540, Y−1206…−603, Z0…157.5 | Low coping/retaining cheeks follow the slope, iron drainage grates, sparse broken paving at the landings. Preserve≥90 cm walk shoulder. Two 60–80 cm-high ore/crate clusters at outer corners, not at the slope centre. |
| Lower west stair / Gorm | Around(−395,−1090,0) after possible stair offset | Worn landing stones, wall-mounted tool and lamp, 3–5 low sacks/crates against the retaining wall, laundry across the recess behind the walking line. Foreground props≤65–70 cm high to keep the88 cm sprite readable. |
| Upper ramp / unloading | X540, Y603…1206, Z157.5…315 | Embed heavy retaining stones, chain anchor and low ore grades at both landings. Guide the eye to the mine using repeated trackside sleepers and a warmer task lamp at the management hut. |
| West Ashslope join | Old(−79,12)→approximately(−2490,362,151) | Replace the clean slab bridge impression with 2 low retaining returns plus matching rock base. Split paving into irregular aprons without narrowing the8m original connector. Continue abandoned rail fragments beside the route and place a broken cart wheel at the field threshold. |
| NE cemetery trail | Start(2338,1447,315) to(3271,1930,630) | Bury exposed rectangular path sides in shallow stone ledges and dry grass. A small turning landing midway, low tool offerings only near graves. Leave the vista edge clear and keep the region quiet; no dense Naia/population additions. |
| Mine threshold | (540,2683,315) throughgateY2955 | Add realistic footing blocks and low ore spill at the outer jambs, chain/lantern equipment in the management recess, and a darkening/soot progression into the tunnel. Preserve the hidden overhead tunnel actors from prior player-occlusion QA. |

The overview's repeated triangular ridge line should be interrupted by overlapping natural rock buttresses of varied yaw/height and short retaining-wall returns. Keep the same grey/ochre base palette; limit orange practical light to hearth/forge/inhabited windows. Treat roofs, walls, narrow lane bends and foreground silhouettes as the visual density, not large quantities of identical crates in empty field centres.

## Acceptance checks after implementation

1. Save a fresh before-transform actor snapshot including components and hidden colliders. Do not regenerate `layout.json` over v003 changes without applying their actual current positions.
2. Verify17 house families, new tavern/smithy bodies,95 volumetric trees, terrace floor Z and complete connection coverage. Check source collider displacement and visual mesh displacement are identical per family.
3. Actual player screenshots at square, tavern door, both stair landings, mine threshold and each field join; overhead view alone hid the old sparsity and hard seams.
4. Re-run six actual movement routes, plus tavern/smithy entry and corrected narrow housing seams. Re-run Q7 ladder, Q8 gate, Q11 release, nighttime crowd count and Q12 camera/input recovery.

No current claim of collision or visual acceptance is made by this read-only report.
