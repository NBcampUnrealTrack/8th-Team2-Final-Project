---
name: naia-ui-layout
description: Apply ProjectM / Naia Unbound panel layout, typography, action-button and hub geometry conventions when editing its Unreal UMG menus. Use for settings, key bindings, popup alignment and hub layout changes.
---

# Naia UI layout

Read `Tools/UILayoutContract/apply.py` in the project before changing these layouts. It is the current final authoring pass, after the historical settings/hub passes. Do not rerun an older pass alone over current assets. This skill supports the requested UI scope; it does not authorize restyling unrelated windows.

## One panel coordinate system

- Centered modal: anchor and alignment `(0.5,0.5)`. Its stage origin is the panel origin; remove inherited Border/SizeBox padding before positioning children. Never compensate unexplained parent offsets with arbitrary negative child offsets.
- Current design canvases: options 1040×700, key bindings 1200×820. Preserve aspect ratio under viewport DPI/fit scaling.
- Header icon left: `round(width * .07)`; title left: icon left +42. Header top: `round(height * .105)`; title height48, icon30×30 at top+9. Title28pt. Header close40×40 at content right−40, header top+4.
- Content left/right inset: `round(width * .085)`, symmetric. Content starts `round(height * .285)`. Include scrollbar and gutter inside this width. Header separator sits24 above content. Tabs/column headers fit in the reserved band; never crowd the title to retain scroll height.
- Footer action top: height−124. Height44; Apply/Close both150 wide; gap16; pair right-aligned to content right. Restore222×44 at content left. Fonts18 and centered, symmetric padding16. Long localized labels require a wider shared control token, not smaller text or asymmetric action widths.
- Footer separator: height−144. Body ends height−160; key settings reserves additional help space and ends height−194, help at height−182. Reserve bottom breathing room inside the visible ornamental contour, not merely the texture bounds.
- Key headings and dynamic row columns must use the same 986px row width and fractions. Preserve bound widget names, tab order and native delegates.

## Hub is an edge-layout family

- Preserve the transparent gameplay center. Left design height899, height-fit with uniform scale. Profile is top/right aligned with24 UI-unit safe inset.
- Nine navigation rows:385×72 layout bounds, gap4, pitch76; list starts47,128. Root Button uniform render scale .75 about left/vertical-center makes visible buttons 25% smaller, including text and hit geometry. Preserve aspect ratio and row pitch. Reserve enough list height for the ninth bottom border.
- Each actual Button owns all its normal/hover/pressed rendering in identical local UVs. Never bake normal-state rows into the rail while separate widgets draw hover states.
- `M_Hub_ContractRow_*` uses a complete shared first-row frame; only icons vary by RowIndex. Other atlas rows have inconsistent outlines and must not supply borders.
- Labels are live localized FText, one language at a time. No baked Korean/English duplicate subtitles. Live title and compass form one balanced header.
- Decorative images do not intercept input. Depth remains separate background/content/foreground layers.
- Run `Tools/UIHubSidePanels/apply_shadow_layers.py` after rebuilding hub art/layout. Profile/desk base materials omit the extra fixed shader shade; independent contact/surface/foreground layers own animated shadows. Live text stays above surface shadows. See `Tools/UIHubSidePanels/SHADOW_LAYERS.md`.

## Verification before reporting completion

Compile/save changed WBP and materials; inspect runtime options AND keys together, including footer widths/styles and title/content alignment. Inspect the hub first/last rows and complete borders. Check normal/hover/pressed share geometry, and verify actual hover with focus if possible. A mouse warp into an occluded preview is not proof of hover; report that limitation accurately. Check at least two viewport aspect ratios when changing anchors/scaling. If PIE ignores window-resolution overrides, test a transient root layout area at the second ratio and distinguish this from an actual viewport-resolution test. Preserve player settings during checks: do not click Apply or Restore.

Use `.copy()` before changing SlateFontInfo/ButtonStyle structs. Save backups before binary asset edits. Record final application order and evidence in `.revvy/shared-memory.md`.

- Settings explicitly registers the shared popup hover helper during NativeConstruct (after controls exist), and unregisters in NativeDestruct: 0.97 scale over 0.12s. Window-layer menus are outside automatic popup-layer registration.
- Hub footer hit/hover rectangles: x44/185/385, y825, widths123/186/149, height49. Centers follow the baked icon+label groups; preserve the artwork position.

## Optical alignment override (2026-10-08)

The contract apply now ends with `Tools/UILayoutContract/align_optical.py`. Reapply it after rebuilding old hub artwork. Settings gear36px at70,80; title115,78 aligns the visible Korean glyph center with the gear. Selected tab marks sit at separatorY-markHeight+1. Header X uses a symmetric procedural Image inside the actual Button (zero style padding, equal8px content inset); do not restore the font glyph with its baseline offset. Hub title102,45 aligns with the compass; footer atlas label samples move down5px while icons and hit rectangles stay fixed. Judge alignment by visible ink centers, not just slot boxes.

- Hub entrance/exit is owned by native `NUGameMenuMotion`: animate parent groups / row wrappers, never replace authored Button scale or split icon/text/hit geometry. Slate time supports paused gameplay. Close is asynchronous (.15s); callers must wait for removal, and action callbacks run after input release. See `Tools/UIHubSidePanels/HUB_MOTION.md`.
