# Trade / craft theme

Run `apply.py` with PIE stopped. Changes six dbals/UI widgets: TradeWindow, CraftWindow, CraftCategory, CraftEntry, CraftDetail, CraftMaterial. Backups and compile/binding checks are under Saved/UITradeCraft.

Reuses the settings palette, PlayerPanels ornamental frame, settings close icon and private common-popup button subclasses without editing those shared resources. Trade is 780x560 with equal 150x44 footer buttons. Craft is height700, main560 plus detail360 and gap16; the combined container stays centered. Titles28, actions18. Borders own their content padding; do not zero the direct child BorderSlot afterward, which aliases the same padding.

Inventory slot art, transaction logic, native BindWidget names and warning colors remain unchanged. `LockedBox` uses an opaque navy veil to prevent recipe-price text competing with the overlaid lock reason.

Development PIE/Standalone console previews already provided by the project:

- `nu.Craft.Open 8006` opens the recipe-table-driven alchemy workshop.
- `nu.Trade.Open Preview` opens a dummy trading partner and companion inventory. It does not connect a real partner. Closing that dummy transaction requests server cancellation, so without a server response the debug session may stay open; end PIE to clean it up. Do not confirm trades or craft items just to check styling.

The wider palette rollout excludes these finished windows so it cannot overwrite the final pass.
