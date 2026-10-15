# Main menu hub — reference 2

Animated shadows: apply `apply_shadow_layers.py` after the layout contract.
See [SHADOW_LAYERS.md](SHADOW_LAYERS.md) for layering and tuning. It removes the
extra fixed profile/desk shader shade and adds independent real-time shadow
layers, preserving all menu geometry.

Current final pass: `Tools/UILayoutContract/apply.py`, after the historical
feedback/depth passes below. Read `Tools/UILayoutContract/SKILL.md` for the shared
layout contract. Actual row Buttons now own normal/hover/pressed art; the rail
contains no baked navigation rows. Live localized labels replace bilingual atlas
labels. Rows are 385×72 with4 gaps; profile has24 top/right safe inset.

Latest (2026-10-07): final `apply_depth.py` also removes both decorative English slogans, widens the top-right profile to nearly rectangular paper and raises the editable greeting 16 design pixels. Source/prompt details: `ArtSource/UI/HubSidePanels/COPY_CLEANUP.md`.

## Current: depth and noisy background fade

Run `py D:/UnrealEngine/ProjectM/Tools/UIHubSidePanels/apply_depth.py` last with PIE/standalone stopped. It includes polish and prior passes, preserves the neutral palette, adds mild multiplicative profile/desk shading and a separate blurred foreground leaves texture, and replaces the left background with a finite static-noise fade. Row artwork remains opaque/readable. Footer actions show isolated text/icons; their common background widens gradually near the bottom and fades to zero within 740 design pixels. Center stays clear and foreground does not block input. See `ArtSource/UI/HubSidePanels/DEPTH.md` for source/prompt details; evidence `Saved/UIHubDepth`.

## Previous pass: illustrated polish and neutral palette

Run `py D:/UnrealEngine/ProjectM/Tools/UIHubSidePanels/apply_polish.py` last with PIE/standalone stopped. It includes the earlier passes. Clean illustrated atlas siblings replace distressed texture detail; warm hues are desaturated toward neutral champagne, paper becomes neutral ivory, and navy stays slate navy. The nine hover states, editable greeting, top/right anchoring, finite fades and transparent center remain. Desk art uses a dedicated transparent cutout at a slightly smaller presentation size. See `ArtSource/UI/HubSidePanels/POLISH.md` for built-in image-generation prompt set and provenance; evidence is in `Saved/UIHubPolish`.

## Previous pass: top-right profile and clear center

Run `py D:/UnrealEngine/ProjectM/Tools/UIHubSidePanels/apply_edge_fade.py` with editor PIE and standalone game stopped. It includes `apply_interaction.py` and is the final authoring pass.

- Profile design fit aligns right/top. Source paper starts at y=139, so art and live values shift up by 139 design pixels together; X sits within the upper-right paper.
- `NUUIManager::OpenGameMenu` disables global dim for this hub. Field HUD is temporarily hidden while the hub is open and restored on close. Cursor, input blocking, Escape and other windows' dim behavior remain intact.
- Three black translucent UI materials behind the left rail, upper-right profile and lower-right desk use finite smooth gradients. All fade quads end before the central game view; their center alpha is exactly zero. They do not intercept input.
- Build/runtime evidence: `Saved/UIHubEdgeFade`.

## Previous pass: edge anchoring and hover states

Run `py D:/UnrealEngine/ProjectM/Tools/UIHubSidePanels/apply_interaction.py` with PIE stopped. It includes the reference pass below and must run last.

- `HubLeftFit` is a separate full-viewport ScaleBox with ScaleToFitY, left/bottom alignment and a 568 × 899 design size. Left art and its live buttons scale together to viewport height without left/bottom letterboxing. The existing upper-right profile fit stays unchanged.
- `HubDeskFit` anchors the original reference's books/quill/compass/table to the lower right. `T_Hub_DeskCutout` supplies alpha only; the material still uses the original reference RGB, including book-cover lettering.
- Idle row 0 samples its navy variant. Each button's Hovered/Pressed brush samples its own reference atlas row using a unique dynamic material. `NUGameMenuTileEntry.cpp` assigns RowIndex again when list entries are reused; no hover state is shared between rows. First-row hover uses original ivory artwork, other rows use the matching state atlas. Leaving a button restores the navy backplate.
- Footer buttons have transparent normal backgrounds, a soft gold hover fill/outline and a stronger pressed state. Embedded system buttons explicitly disable icon-only styling so the state brushes render.

Sources: `T_Hub_ButtonStates.png` and `T_Hub_DeskCutout.png` in `ArtSource/UI/HubSidePanels`. Evidence: `Saved/UIHubInteraction`. The row material binding is native code and requires its build/Live Coding patch as well as the assets.

## Previous pass: original-reference correction

Run `py D:/UnrealEngine/ProjectM/Tools/UIHubSidePanels/apply_reference.py` in the Unreal editor console with PIE stopped. This supersedes `apply.py` and all older centered/five-column/compact hub passes. The earlier generated frame was rejected by the user because it did not match the concept.

The current material samples the original 1672 × 941 reference directly for the left header, nine bilingual menu rows, their icons and outlines, the three footer actions, and the right profile card. The uploaded source and `ArtSource/UI/HubSidePanels/T_Hub_Reference2.png` are byte-identical. Polygon masks keep only the left menu and upper-right card. Only the live name/level/experience/value rectangles use cleaned parchment from `T_Hub_ProfileClean`; the remaining pixels are the original reference. The center and lower-right scene/character artwork are masked out.

The real buttons overlay the original static lettering. Their native delegates, tooltips, focus, hover and pressed feedback remain active. Reference order is Character, Equipment, Stats, Skills, Growth, Naia, Quest, Map, Inventory. `RegisterGameMenuEntries` adds Equipment through `ToggleEquipmentMenuUI`, which opens the unified equipment section without changing legacy companion equipment windows. The hub keeps registry categories unchanged and sorts its own presentation to match the reference. The nine baked labels must be updated together with row order if the navigation design changes or localization is added.

Dynamic profile text uses the reference positions. X replaces the reference ESC close control, as requested. Editable greeting remains in the unused lower part of the paper card.

Runtime art: `/Game/Developers/MinkiCho/Art/UITheme/HubReference2`. Backups, compile/build reports and gameplay QA: `Saved/UIHubReferenceExact`. This revision includes native menu registration/order changes and requires an editor-target build before testing nine rows.

Verified: editor-target build succeeded, three WBP compile without errors/warnings, PIE `runtime.json` confirms exact nine-entry order, Character/Equipment destination sections, X close and greeting save/reopen. Final gameplay screenshot: `Saved/UIHubReferenceExact/reference_gameplay00001.png`. Test greeting configuration was restored.

## Earlier approximation (superseded)

The description below records the first version; do not reapply it over the reference correction.

This is the current hub layout. It supersedes the centered hub, compact 85% layout, five-column tile layout and compass relocation from `Tools/UIPanelConsistency`. Apply this script last if replaying older authoring scripts.

- Left: eight existing menu entries in a vertical list; three existing system actions at the bottom.
- Upper right: live profile fields and editable greeting. Close button displays X.
- Center and lower right: transparent. The old blur, portrait capture image and fallback portrait widgets are removed. Existing native profile code safely skips capture when the portrait widget is absent.
- Dedicated runtime texture/materials live under `/Game/Developers/MinkiCho/Art/UITheme/HubSidePanels`; no runtime dependency on the uploaded concept texture.
- Existing menu registry, event bindings and greeting persistence are retained. No new native code is required for this layout.

The script backs up all three affected WBP files under `Saved/UIHubSidePanels/<timestamp>`, clears only removed-widget GUID entries, verifies preserved bindings, compiles dependencies before the main widget and saves the results. Compilation report: `Saved/UIHubSidePanels/applied.json`.

PIE evidence in `Saved/UIHubSidePanels`: `personalize_runtime.json` covers eight entries, removal of portrait/blur, Korean greeting commit/cancel/focus loss/reload, length cap and empty default; `actions_runtime.json` covers the managed hub close action and character menu entry. Final screenshot: `hub_gameplay00001.png` (X repositioned below HUD icons). Greeting tests restore the original local profile configuration.

## Final feedback polish (2026-10-07)
Run `apply_feedback_polish.py` after the existing depth/copy-cleanup passes. Private `M_Hub_FeedbackRail` compresses nine atlas rows to 56.5px visible height with 12px gaps at the existing 68.5px pitch; TileView spacing follows the artwork. Baked PROJECT M is masked; `HubBannerProject` displays live Korean title text. Separate `M_Hub_WoodDesk` uses new RGBA artwork without moving the foreground objects or covering the center. Existing hover materials and native actions are preserved. Do not rerun older full builders alone over this revision. QA: `Tools/UISettingsReference/qa_feedback_polish.py`.
