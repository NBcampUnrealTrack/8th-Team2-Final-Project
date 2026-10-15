# Settings frame

`T_Settings_Frame.png` is the independent runtime frame source (1536 × 1024), generated on 2026-10-06 with the imagegen tool using the user's settings and key-settings design images as visual references.

Direction: blank deep-navy fantasy settings panel, thin ornate gold perimeter, left hanging compass ribbon and botanical decoration, subtle compass on the right; no baked labels or buttons. All text and interactions are authored separately in UMG.

Imported asset: `/Game/Developers/MinkiCho/Art/Settings/Textures/T_Settings_Frame`. UI texture group, editor-icon compression, sRGB, no mipmaps, never stream. `Tools/UISettingsReference/apply.py` maintains these settings.

2026-10-06 revision: removed the duplicate central compass emblem from the left ribbon with imagegen; retained the upper compass, gold leaves and frame. Reimported into the same shared texture for both settings windows.

2026-10-06 exterior cleanup: removed the cloudy backdrop outside the panel silhouette to transparent alpha with imagegen. Preserve the RGBA source and `compression_no_alpha=False` on import; the gold outline, ribbon and navy interior remain visible.
