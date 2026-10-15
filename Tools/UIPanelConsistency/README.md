# Settings-derived menu palette

Latest hub pass: `hub_personalize.py` runs after `restore_hub_right.py` / `compact_hub.py`. It uses `T_MenuHub_RightCompass` on the right, sets the 1040px TileView to LeftAligned with 208px entries (five columns), right-aligns the three footer controls, and adds the inline `HubMottoInput`.

The native hub binds/unbinds text commit in its widget lifecycle. Enter/focus loss saves; Escape reverts; blank input restores the default greeting; committed text is capped at 32 UTF-16 units without interrupting IME composition. Local persistence is `Saved/Config/HubProfiles.ini`, with separate sections hashed from server endpoint/account plus character ID (offline preview has a separate identity). Save failure is shown on the input. This is local profile customization, not server synchronization. The original HubMotto text remains collapsed for styling compatibility.

Personalization verification: C++ Live Coding build/patch succeeded; WBP compiles with no errors/warnings. PIE verified eight entries, left alignment and the rendered result. Commit-delegate tests cover Korean text, Enter, focus loss, cancellation, new widget reload, length cap and blank fallback, restoring the pre-test config afterward (`personalize_runtime.json`). Physical keyboard Enter also persisted correctly; physical Escape was intercepted by the editor's Stop PIE shortcut, so standalone Escape is not directly verified. UMG RevertTextOnEscape is enabled; cancellation handler is verified separately. `motto_build.log` and screenshots are in Saved/UIPanelConsistency.

Compact hub follow-up: `compact_hub.py` insets HubFit anchors to 7.5% on each side (85% of the previous fit), retaining responsive aspect ratio. Label fonts increase 10% within that fit and main icons use 96px instead of 86px, reducing apparent whitespace while keeping readable content. Original ivory right panel, navy profile and compass are retained. Logical design canvas stays 1672x941; this is UI presentation sizing, not a game-resolution change.

Hub follow-up: the right button panel uses its original ivory artwork and original text/icon styles again. Left profile art and the new compass remain. `M_Hub_OriginalRight` samples current art on the left and original `T_GameMenuHub_GoldPattern` on the right (including alpha). `restore_hub_right.py` applies this scoped restoration; the three hub WBP are excluded from the broad palette pass.

2026-10-06 scope correction: CharacterMenu (8 WBP) and quest windows (5 WBP) were restored from the pre-palette backup `Saved/UIPanelConsistency/20261006_153811`. They are now excluded from `apply.py`, including quest graph color edits. Their original design takes precedence over the initial validation/scope notes below. Hub, settings and other panels retain the new palette.

The settings panel is the palette reference: navy surfaces, ivory readable text, gold emphasis. Hub artwork changes both color and compass motif. Other panels retain their geometry, ornaments and source textures; only colors/material outputs change.

`apply.py` covers the hub, six character pages and shell, inventory/equipment/storage/trade/craft/map windows, Naia roster, quest journal and offer, shop and dialogue panels, plus their listed entries. Settings and HUD/chat are not restyled by this pass. The quest-offer branch is isolated from its shared tracker blueprint.

Run `apply()` in the Unreal Python console outside PIE, using an explicit module import from this directory (other UI tools also have an `apply.py`). Backups, widget snapshots, compilation reports and runtime screenshots are under `Saved/UIPanelConsistency`. The script asserts unchanged widget identities, hierarchy, slots, GUIDs and bindings.

New art and dedicated recolor materials live under `/Game/Developers/MinkiCho/Art/UITheme/SettingsNavy`. The material transforms sampled color only and preserves original UVs/opacity, including the shop's existing silhouette trim. Gold pixels remain gold. Shop-specific material-instance surface parameters are also changed because its runtime tab selection restores those original brush references. Shared master materials are untouched. Semantic item/character textures are not recolored.

Apply this palette pass after older page-authoring scripts if regenerating layouts. The source PNGs and generation notes are in `ArtSource/UI/PanelConsistency`.

Quest category selection restores text colors through five Select Color nodes. This pass updates only their selected/idle color literals and asserts unchanged graph edges. Character navigation uses the matching CDO state colors. Small symbols and shortcut keycaps retain readable contrast on navy.

Validation: all 29 blueprints compile with zero errors/warnings; the PIE hub contains eight entries and all six character navigation buttons open the expected pages. Shop and quest journal were visually inspected, including quest category selection. Widget hierarchy, slots, GUIDs and bindings are checked on every application. Runtime evidence is under `Saved/UIPanelConsistency`.
