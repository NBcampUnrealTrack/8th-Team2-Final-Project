# Built-in image generation prompts

## Backplate edit

Inputs: `ArtSource/UI/GameMenuHub_20260921/backplate.png` (edit target) and the user-added `메뉴 허브 시안.png` (style reference). Transparent background enabled.

Use case: precise-object-edit. Asset type: transparent fantasy game UI backplate texture. Image 1 is the edit target (existing blank UI backplate); Image 2 is style reference ONLY for the LEFT NAVY panel's fine gold astrolabe/star-chart patterns. Make exactly one change to Image 1: remove ALL castle silhouettes, mountains and landscape silhouettes from its LEFT NAVY vertical panel and replace them with subtle, elegant, fine muted antique-gold astronomical circles, arcs, radial ticks and star-map tracery, like the bottom-right of Image 2's navy profile panel. Dark navy ground, crisp restrained gold line work; quiet behind profile data, more visible at lower right and upper right of this narrow navy panel. KEEP the existing panel layout, exact wide canvas aspect ratio, left banner and compass, EMPTY circular portrait ring, outer gold borders, separator lines, right ivory panel and faint leaves/compass absolutely unchanged in position and scale. Preserve transparent pixels outside the panel silhouettes. No text, no characters, no menu icons, no new buttons, no filled portrait. This is a reusable BACKGROUND asset, not a mockup. The right ivory 75% should remain as image 1. Do not copy any text or UI content from image 2. Output same framing and canvas proportions as image 1; do not crop the banner or bottom frame.

## Icon generation

One built-in call per icon, transparent background enabled. Shared specification: a centered single opaque flat geometric glyph on a square transparent canvas, 12% margin, no frame, text, shadow, glow or gradient, recognizable at 40 pixels.

- Settings: eight broad squared cog teeth, rotational symmetry, circular transparent central cutout.
- Character change: anonymous head circle above shoulder/bust silhouette; a smaller two-arrow cycle emblem attached at the lower right, with transparent clearance where it overlaps the bust.
- Quit: open door leaf in an upright doorway with a thick arrow pointing right, strong silhouette and transparent negative spaces.

## Final icon refinement

Each first icon was edited separately with the following prompt and transparency enabled. The final dark source avoids the alpha-edge artifacts in the discarded white first pass.

Edit this single UI icon: replace the white glyph with a single completely flat very dark NAVY #173044 glyph, keeping the same icon silhouette, geometry and layout. Crucially clean every edge and all negative spaces: remove ALL irregular white clouds, flakes, rough edging, spots, flecks and artifacts. Make smooth crisp geometric boundaries, like a professionally exported flat SVG icon. All cutouts and all areas outside the exact glyph must be 100% fully transparent. Absolutely no white or light pixels, no outline, no texture, no shadow, no gradients. Keep square canvas, centered icon and existing padding. This must be clean and readable when reduced to 40 pixels in a game footer.
