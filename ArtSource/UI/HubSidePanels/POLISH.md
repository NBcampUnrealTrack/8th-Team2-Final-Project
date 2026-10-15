# Hub polish, 2026-10-06

Generated with the built-in image generation tool, preserving the preceding layout.
Sources: `T_Hub_PolishIdle.png`, `T_Hub_PolishHover.png`, `T_Hub_PolishDesk.png`.
Final authoring: `Tools/UIHubSidePanels/apply_polish.py` (includes prior edge/interaction passes).

Prompt set:
1. Edit the existing 1672x941 cleaned reference atlas in place. Preserve Korean/English glyphs, icons, layout and silhouettes. Remove grunge, cracks, photographic texture, stains, blurred foreground shadows and noisy flecks. Use clean illustrated shapes, restrained two-to-three-tone matte shading and crisp outlines, neutral slate navy and pale champagne accents. Keep all nine idle rows navy and profile live fields blank. Simplify the books/quill/desk into clean illustrated surfaces.
2. From that idle atlas, change only all nine left button interiors to cool ivory with dark navy lettering and muted taupe/champagne icons; preserve positions and all other content.
3. Extract books, compass, quill, inkpot, flowers, rolled map and desk from the polished image to true transparency. Remove UI panels and world background, keep a clean curved desk edge.

The cutout was composed larger than requested; its runtime image is uniformly scaled to 1272x716 at (400,225) within the existing 1672x941 desk canvas. Runtime materials desaturate warm hues toward neutral champagne without changing the navy base. Original reference sources remain available.

The left mask follows the new illustrated contour to remove background fragments. X moves to (1570,24) on the clean paper; input bindings and the profile's live field positions are unchanged.
