# Player profile and context menu

`apply.py` styles only `dbals/UI/WBP_CharacterProfile` and `dongh/UI/WBP_PlayerContextMenu`. It uses the settings palette, independent straight gold border segments with pointed end ornaments and rounded corner links, matching settings buttons, and a centered procedural close icon. Profile width is 360 UI units including symmetric 30-unit insets; context width is 320 with 28-unit insets. Profile native equipment cells are 72 units. Context height follows its visible actions instead of clipping a fixed 320-unit box.

Run with PIE stopped. Backup originals before edits; the earliest original backup for this change is `Saved/UIProfilePreview/20261008_185632`. Do not rebuild the context action list by clearing children: a partial failure can orphan bound buttons. Keep their original hierarchy and Blueprint event bindings.

`preview.py` runs inside PIE after opening `/Game/Maps/L_VillageEntrance`. It creates temporary sample windows without server requests, invokes the profile's existing refresh to build native empty equipment slots, and shows the six standard field actions. Actual online profile lookup, party invitations, whispers and trade are not exercised. It intentionally leaves PIE open for the user's design review; stop it before the next asset edit. Existing standalone play is independent.

Shared item-cell art is unchanged. No inventory, character-menu, quest-journal or settings assets are edited by this pass.
