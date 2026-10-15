# Hub side-panel art

## Hover and desk assets

- `T_Hub_ButtonStates.png`: imagegen state atlas from the unchanged original reference. Row 0 is navy idle; rows 1–8 are ivory hover. The original reference holds the complementary states. Only the relevant row rectangle is sampled at runtime. Output ID: `exec-b2c72501-f12e-4335-b1a5-a50c46392418.png` under the same generation session directory below.
- `T_Hub_DeskCutout.png`: imagegen transparent cutout of the original lower-right books, quill, ink pot, compass, rolled map and tabletop, on the same 1672 × 941 canvas. Runtime uses its alpha only and samples RGB from `T_Hub_Reference2`, preserving original book text/colors. Output ID: `exec-b2279ace-8d96-4da1-9143-2e4a002a4737.png`.

Prompts required exact original coordinates and dimensions, unchanged lettering/icons/borders outside the color-state edits, and no new composition. The desk prompt required transparent scene removal and a natural silhouette around the feather, compass and book corners. `apply_interaction.py` is the current final import/material/layout pass.

## Current original-reference assets

- `T_Hub_Reference2.png`: unmodified copy of the user-uploaded concept export (1672 × 941). Used directly for the static header, bilingual buttons, icons, footer and parchment frame.
- `T_Hub_ProfileClean.png`: OpenAI image edit, same dimensions and coordinates. Only small rectangles behind live profile values are sampled from this texture. Everything else is sampled from the unmodified reference. Generation output: `01a10f08-336e-7783-ab4d-9aa6cb1947c9/exec-8f87e5a4-e5a0-42c2-94e6-850c1a89f90a.png`.

Cleanup prompt: "Surgical edit of this exact UI screenshot. Preserve the entire input composition and pixel positions, dimensions 1672x941. ONLY on the upper-right parchment profile card, erase the dynamic data: 'Lv. 15', 'testKnight', '1,819 / 5,273', the experience progress bar, and right-aligned values '리베로그로브', '나이아 나이트', '0', '1000000197'. Fill each erased small region seamlessly with adjacent matching parchment texture. KEEP absolutely everything else unchanged including the four small brown profile icons, Korean labels 현재 위치 직업 소지 골드 캐릭터 ID, divider lines, the little compass in the card lower left, all parchment edges, all nine left buttons with text/icons, and the entire background. Do not redesign or regenerate the card. Need blank parchment only where the dynamic data used to be so real runtime text can be placed on top."

Rendering masks and live-widget positions are in `Tools/UIHubSidePanels/apply_reference.py`.

## Earlier approximation (superseded)

`T_Hub_SidePanels.png` is a 1672 × 941 RGBA backplate generated with OpenAI image generation on 2026-10-06, using the user's `/Game/Developers/MinkiCho/Art/메인_메뉴_허브_시안2` as the visual reference.

Generation brief: separate blank navy left navigation panel and ivory parchment upper-right profile card, thin gold ornament and leaf borders, with transparent center and lower-right space. Omit text, buttons, icons, people, scenery, lantern and books. Text, functional buttons and the compass are rendered separately by UMG.

Generated image provenance ID: `01a10f08-336e-7783-ab4d-9aa6cb1947c9/exec-c3a8a567-4133-4de2-bc2d-c5252d43a1eb.png`.

The runtime UI material rejects alpha below 0.01 and clips to the left panel and upper-right card bounds so stray near-transparent pixels cannot obscure the game world. Import uses UI compression, alpha preserved, no mipmaps and sRGB. The concept asset itself is not a runtime dependency. See `Tools/UIHubSidePanels/apply.py`.
