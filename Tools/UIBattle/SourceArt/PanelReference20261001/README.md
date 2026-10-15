# Battle command panel skin

**Superseded by the user's updated reference later on 2026-10-01.** Current open-row/round-badge design and shader sources: `../PanelRevision20261001/README.md`. The material asset paths below are reused; their former rectangular-panel behavior described below is historical.

Reference: `/Game/Developers/MinkiCho/인게임_전투_UI_패널부분`.

Only the command menu's background brush and the command button background styles are changed. Widget hierarchy, positioning/tracking graphs, text, fonts, icons, padding, input and server wiring are unchanged.

Assets: `/Game/Developers/MinkiCho/Art/Battle/PanelReference20261001/`

- `MI_BattleCommandPanel_Reference`: used by `WBP_BattleCommand / Bd_Background`. Override `PanelTexture` to replace the artwork, or `FillOpacity` to adjust the dark surface without weakening the gold frame. Default 0.64 multiplies the original texture alpha.
- `M_BattleCommandPanel_Reference`: places the original alpha texture around the existing content using the 460×460 design coordinate system. FrameOrigin/FrameSize and SourceOrigin/SourceSpan control placement and source margins independently of viewport DPI.
- `M_BattleCommandRow_Reference`: normal/disabled cut-corner row background. Keeps the existing button tint-based selection contract.
- `MI_BattleCommandRow_Hover`: selected-style gold rim and glow for hover/pressed states.
- `T_BattleCommandPanel_Reference`: generated 1132×1389 RGBA texture, UI compression, no mipmaps, source alpha preserved.

The PNG and exact built-in image_gen prompt (`generation.md`) are kept here. No live play test was performed for this skin-only change.

Persistence correction: after direct widget brush/style edits, call `modify()` on the owning Blueprint and force-save with `EditorAssetLibrary.save_asset(path, False)`. A compile alone does not guarantee that these setters marked the package dirty. The initial skin assignment was missing from the on-disk WBPs and was reapplied and verified on 2026-10-01 after the user reported the login route still showed the previous design.
