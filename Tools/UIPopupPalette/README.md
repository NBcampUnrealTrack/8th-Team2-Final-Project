# Common popup palette

Run `apply.py` through the editor Python bridge with PIE stopped. Scope:

- `/Game/Developers/dbals/UI/WBP_ConfirmDialog`
- `/Game/Developers/dbals/UI/WBP_CountDialog`
- `/Game/Developers/zxcv1/Quest/UI/WBP_RewardPopup` (the currently referenced quest reward class)

Uses the settings pilot palette: navy `#16293D`, raised surface `#1E303D`, gold `#C8B691`, text `#F1EDE6`, muted text `#AAB6BF`, dark text `#243641`. Preserve destructive/positive confirmation colors, layout, texture alpha, and bindings. Materials and count-button subclasses are private to `CommonPopupPalette`; settings, key bindings, the original common button and alert panel are checked for unchanged file hashes.

Backups and verification reports go to `Saved/UIPopupPalette/<timestamp>`.

`qa.py` creates temporary runtime previews without granting rewards or mutating inventory. It checks confirmation/cancellation and count cancellation; the reward popup is a visual preview only because it is not registered with the notification controller. The count preview supplies the label normally initialized by native `Show`. Screenshots and results go to `Saved/UIPopupPalette/QA`. Stop PIE and restore standalone preview mode when done.
