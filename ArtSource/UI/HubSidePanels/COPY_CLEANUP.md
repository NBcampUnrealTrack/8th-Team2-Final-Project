# Hub copy and profile cleanup ? 2026-10-07

Final authoring: `Tools/UIHubSidePanels/apply_depth.py` (run last, PIE stopped).

Built-in image generation edited the previous polished atlas and transparent desk in place:
- `T_Hub_CleanHeaderProfile.png`: delete A SMALL STEP / FOR ANOTHER WORLD under PROJECT M; preserve logo/compass, restore navy. Replace upper-right trapezoid with wider almost rectangular ivory paper, horizontal edges and tiny chamfered corners; retain Korean labels/icons and blank live-value areas. Preserve remaining atlas composition.
- `T_Hub_CleanDesk.png`: delete only TO A / BRIGHTER / TOMORROW from upright navy book; restore plain navy, preserve border/stars, all desk elements, placement, colors and transparency.

Runtime only samples the cleaned header/profile region of the first texture, and the dedicated cleaned desk texture. The existing neutral palette shader is retained. Profile source bounds (1018,149)?(1650,430) map to (985,139)?(1672,461), whose top aligns to the viewport after the existing -139 image offset. Live values follow the same mapping. Greeting moves from y230 to214 (16 design pixels up), x1096. Editable greeting behavior remains native.

Evidence: `Saved/UIHubCopyCleanup`.
