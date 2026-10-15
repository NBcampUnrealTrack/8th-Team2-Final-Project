# Hub depth follow-up, 2026-10-06

Final authoring: `Tools/UIHubSidePanels/apply_depth.py`, including the polish and earlier passes. Palette correction is unchanged. Profile/desk shading multiplies RGB equally; no warmer tint is introduced.

`T_Hub_DepthForeground.png` was created with the built-in image generation tool from the original reference. Prompt: extract/recreate only near-camera defocused leaves/branches along the far left edge, a small upper-right cluster and a lower-right/bottom cluster, on a transparent 1672x941 canvas. Use neutral charcoal/navy and gray-sage, feathered translucent alpha, no orange, no UI/text/scene/furniture, and leave the center clear. Runtime uses alpha-aware nine-tap filtering, restrained opacity, and masks protecting left labels, profile value fields and X.

The rail is now a 740x899 material with a finite smooth alpha gradient and static interpolated noise. Complete button rows and bright header artwork remain legible; footer actions retain only isolated glyphs/icons over the background. It replaces the previous hard polygon backdrop and redundant left black shadow strip. Foreground images are hit-test invisible. Evidence: `Saved/UIHubDepth`.

Button-edge cleanup: `Tools/UIHubSidePanels/row_mask.py` defines per-row silhouette coverage shared by idle and hover/pressed materials. Opaque rectangle strips between rows and beyond the frames are removed; the underlying noisy rail fade shows through. Evidence: `Saved/UIHubButtonEdges`.

Footer follow-up: removed the opaque navy footer rectangle and baked separator rules. Footer text/icon coverage is isolated using the polished source luminance; the common rail broadens smoothly over design y710..855 and fades out by x740, supporting all three actions without separate navy button backplates. Evidence: `Saved/UIHubFooterFade`.
