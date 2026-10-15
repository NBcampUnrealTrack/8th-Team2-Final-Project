# Settings reference design

`apply.py` authors the options and key-binding windows from the supplied 설정_패널_디자인 and 키설정_패널_디자인 references. The design uses a navy panel, gold frame and active-tab underline, ivory labels, and a gold apply button. Labels and interactive controls remain UMG widgets.

Targets under `/Game/Developers/dbals/UI/`: `WBP_OptionsWindow`, `WBP_KeyBindingsWindow`, `WBP_KeyBindingRow`, `WBP_KeyBindingGroup`, `WBP_OptionRadioGroup`.

Run outside PIE in the Unreal Python console (Revvy editor services required):

```python
import sys, importlib, unreal
sys.path.insert(0, unreal.Paths.project_dir() + 'Tools/UISettingsReference')
import apply
importlib.reload(apply)
apply.apply()
```

The script backs up all five packages under `Saved/UISettingsReference/<timestamp>`, preserves existing named controls/classes, bindings and widget GUIDs, imports the frame when needed, compiles, and saves. It is repeatable. Re-run only when intentionally restoring this layout; it overwrites authored styling.

The original design textures are reference-only. Runtime artwork is `/Game/Developers/MinkiCho/Art/Settings/Textures/T_Settings_Frame`, sourced from `ArtSource/UI/SettingsReference`.

Native contracts:

- `SettingsTabMark0..3` and `SettingsTabLabel0..3` are optional authored active-tab visuals selected by `NUOptionsWindowWidget`.
- Options `HeaderCloseButton` uses the existing native `Close` path.
- Key window `FooterCloseButton` uses `HandleCloseClicked`, including pending input-capture cleanup.
- Existing settings controls and key-row delegates retain their identities. Keyboard and mouse remain one mapping column; gamepad is the second. Unimplemented effects-volume and extra control options are marked unavailable.

Validation on 2026-10-06: five blueprints compiled with zero errors/warnings; native Live Coding build succeeded. PIE exercised all four tabs/selection marks, options header close, dynamic key rows (19 groups/rows), keyboard and gamepad capture-wait indicators, footer cancellation during capture, and reopening. Captures and the runtime report are under `Saved/UISettingsReference`. No user key mappings were changed or applied by the test. This is not a complete resolution/controller-navigation QA pass.


## Options-only palette pilot (2026-10-07)

Run `apply_palette_pilot.py` in the editor with PIE stopped, after any full reference authoring pass. The only existing WBP modified is `/Game/Developers/dbals/UI/WBP_OptionsWindow`. A private radio widget copy and `M_OptionsPaletteFrame` live under `/Game/Developers/MinkiCho/Art/UITheme/SettingsPalettePilot`; the original radio, key settings, common buttons, and frame texture stay unchanged.

Palette is authored as sRGB HEX, explicitly converted to linear: navy #16293D, surface #1E303D, champagne #C8B691, text #F1EDE6, secondary #AAB6BF, dark text #243641. Frame material preserves source alpha, motifs and luminance relief while recoloring navy and metal regions. No raster regeneration. The frame's shaded pixels intentionally vary around the base color.

The pilot preserves widget names, slot layouts, fonts, text and native interactions. It checks compilation and shared-file hashes. Evidence/backups: `Saved/UISettingsPalette`; the initial original WBP backup is `20261007_170310/WBP_OptionsWindow.uasset`. Runtime QA opens all four tabs and closes via X, without applying or resetting user settings.

## Latest: detail polish
Run `apply_detail_polish.py` LAST after the palette pilot. Repairs frame/banner artwork, retains palette, reuses popup cut-corner frame materials for buttons and choice cells, and centers footer/launcher text. Source/prompt: `ArtSource/UI/SettingsReference/POLISH.md`; runtime evidence: `Saved/UISettingsPolish`.

## Latest: key settings continuation
Run `apply_key_polish.py` after detail polish. Launcher is a 190x48 SizeBox with short centered label; full device description remains in help and tooltip. Key window reuses the polished frame and shared palette. Header and dynamic row columns use a fixed 986px content width and matching fractions (.012-.43, .445-.695, .735-.985). Private native-derived common-button templates preserve native delegates while using popup normal/hover/pressed materials without the old common button construction graph. Rows/groups and footer adopt those styles; capture/changed/unassigned states remain distinct. Evidence: Saved/UIKeySettingsPolish. Close standalone ProjectM preview before asset saving; WidgetService.is_pie_running does not detect that standalone file lock.

## Latest: key section and corner polish

Run `apply_key_sections.py` last, after `apply_key_polish.py`. Noninteractive group headings use a fading surface band, short gold left accent, and fine bottom rule instead of a button frame. Key-only button materials use one continuous 1.15px rim, restrained 4px clipped corners, and derivative-based antialiasing; stacked inner/corner strokes are removed. Palette, hover/pressed states, layout and native bindings remain intact. The options and shared popup materials are unchanged. Backups and compile results: `Saved/UIKeySettingsPolish/sections_*`.

## Final outer-frame artwork pass

Run `apply_key_frame_corners.py` after the stages above. This fixes the actual outer panel frame: `T_KeySettings_FrameRounded.png` replaces scalloped/vine-like corner contours with smooth circular curves while keeping crisp star points, banner and compass. The private `M_KeySettingsRoundedFrame` retains the existing palette mapping; only the key window uses this artwork. Source is RGBA 1536x1024, generated with imagegen from `T_Settings_FramePolished.png`; exterior alpha is preserved. Backups: `Saved/UIKeySettingsPolish/outer_*`.

## Final analytic frame (supersedes raster corner pass)
Run apply_key_geometric_frame.py LAST. M_KeySettingsGeometricFrame replaces the raster border with a signed-distance rounded rectangle (44 source-pixel radius), constant-width rails and derivative antialiasing. Seven independently calculated symmetric pointed ornaments overlay it; compass disc and interior artwork are retained. Original raster edge is blended out inward of the analytic contour, so image-generated wobble cannot define the outer silhouette. Only WBP_KeyBindingsWindow uses this material. Backups/diagnostics: Saved/UIKeySettingsPolish/geometry_*.
The compass outer rings are also analytic concentric circles (53/62/69 source-pixel radii), preserving the central needle artwork and independent sharp tips. This removes residual wobble at the upper-left corner.

2026-10-07 ornamental revision: analytic outer contour now separates four straight double-engraved rods from thinner quarter-circle connectors (47px radius). Paired four-point ornaments sit at each exposed rod endpoint, not at the midpoint of each corner. The upper-left compass occupies its corner mounts. Slight champagne facet shading preserves sparkle without ragged texture contours. Final pass remains apply_key_geometric_frame.py.

## Options frame adoption and footer order
Run apply_options_shared_frame.py LAST after apply_key_geometric_frame.py. Options WindowFrame now shares the approved analytic ornamental material with key settings. Both footer groups read Apply then Cancel from left to right; original button widths, gaps, right bounds, labels, styles and callbacks are preserved. Options positions: 686/848 at y560. Keys: 866/1032 at y711. This pass changes only WBP_OptionsWindow and WBP_KeyBindingsWindow; no global popup rollout.

Banner source: import ArtSource/UI/SettingsReference/T_Settings_BannerLaurel.png into SettingsPalettePilot (UI texture, sRGB, RGBA, no mips). apply_key_geometric_frame.py uses it only in the banner region; when absent it falls back to the previous texture. Shared frame updates both options and keys; footer order remains governed by apply_options_shared_frame.py.

Current banner preference is T_Settings_BannerCelestial (same UI import settings). Geometric-frame authoring uses celestial first, then laurel as legacy fallback. Both options and key window share the constellation motif; scope remains these two panels.

## Final: no banner, centered content
Run apply_settings_no_banner.py LAST after all previous passes. Both settings windows now omit banner/fold/top-left compass. Geometric frame samples calm navy for the former banner strip and has symmetric paired mounts on all four corners. Title gear is an analytic M_SettingsGearIcon, 30x30. Options title is 설정. Content moves left 78px (options, including inherited stage offset) /52px (keys), preserving sizes and Apply-before-Close/Cancel. banner_layout_baseline.json holds canonical pre-removal positions so reruns are idempotent. Earlier source banner assets remain archival.

## Final: scrolling tabs and offset headings
Run apply_settings_scroll.py LAST after no-banner pass. Each TabSwitcher page is wrapped in its own ScrollBox while retaining native index order (Display/Graphics/Sound/Controls), widget names and delegates. Tabs/header/footer remain fixed. Graphics rows use natural height, 12px vertical padding each side, 17px choices and 18px labels rather than squeezing all rows into view. Scrollbars appear on overflow, wheel uses WhenScrollingPossible with animation, and focus navigation scrolls into view. Title+gear move 60px left relative to previous layout. QA: qa_settings_scroll.py / Saved/UISettingsScroll.

## Final feedback polish (2026-10-07, historical)
Run `apply_feedback_polish.py` after `apply_settings_scroll.py`. Options close moves to x888; tabs gain a muted gold divider at y147 and content starts at y172. Related settings are grouped in softly outlined navy cards; existing native row names, tab indices and scroll behavior remain. Compact choices are 36px/16pt and primary footer controls 44px/18pt, centered. See `UI_STYLE_GUIDE.md`. This is the final options authoring pass; earlier LAST instructions are historical. QA: `qa_feedback_polish.py`, evidence under `Saved/UIFeedbackPolish`.

## Current: shared layout contract
Run `Tools/UILayoutContract/apply.py` LAST after the previous passes. Its
`SKILL.md` owns current layout values and verification rules. Options no longer
has hidden 42/30 parent padding; both windows use proportional header/content
insets. Footer sizes are Restore222×44 and Apply/Close150×44 with16 gap. Private
WBP_SettingsFooterPrimary/Secondary copy the actual options button styles for
key-window footer controls without restyling key-entry controls.
