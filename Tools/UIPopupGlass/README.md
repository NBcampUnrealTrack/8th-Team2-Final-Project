# Common popups — glass card (concept A, no key hints)

`apply.py` (run through the editor Python bridge with PIE stopped; safe to re-run) restyles
WBP_ConfirmDialog, WBP_CountDialog and WBP_RewardPopup to the character-menu glass card:
520px card, icon + title + crest + gold rule, right-aligned 150x48 pill buttons.

- Confirm: destructive = dark pill with red outline (designer style); positive/notice = cream pill
  via `UNUConfirmDialog::bUseIntentStyles` + `PositiveConfirmStyle` on the WBP class defaults.
  Name contract `IntentIconDestructive` / `IntentIconPositive` picks the title icon.
- Count: rebuilt on a canvas with the same bound names; optional `DecreaseButton` / `IncreaseButton` / `MaxButton`.
- Reward: `RewardRows` + hidden `Reward*Template` widgets make one row per entry (icon or gold diamond, name, +amount).

Private assets: /Game/Art/UI/PopupGlass (icons from GPT, button subclasses duplicated from CommonPopupPalette).
Concepts: ArtSource/UI/PopupConcept_20261015. Backups: Saved/UIPopupGlass/backup_*.
