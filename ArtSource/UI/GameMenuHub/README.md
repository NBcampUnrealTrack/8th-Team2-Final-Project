# Menu hub art

Requested scope: use `/Game/Developers/MinkiCho/Art/메뉴_허브_시안` for the navy panel's gold patterns and borderless menu presentation. Keep the existing menu registry, profile data, five-column layout and close control. The reference's additional menu entries are not part of this change.

All four PNGs were made with the built-in `image_gen` tool. Original RGBA outputs are retained without pixel processing. Generation specifications are in `prompts.md`.

- `T_GameMenuHub_GoldPattern.png`: 1672×941 backplate. Replaces castle/landscape silhouettes with gold astronomical tracery while preserving the original frame, empty portrait ring and ivory panel.
- `T_GameMenu_Settings.png`: settings cog.
- `T_GameMenu_CharacterChange.png`: character bust with two circular arrows at its lower right.
- `T_GameMenu_Quit.png`: open door and exit arrow.

Unreal assets: `/Game/Developers/dbals/UI/GameMenuHub/Textures/`, matching the filenames. The icon sources are navy; Unreal's non-destructive texture adjustments (Saturation 0, Brightness 100) build a white mask with the original alpha. Button foregrounds supply navy/gold state colors. Icons build at 256×256 with SimpleAverage mips, UI RGBA and trilinear filtering, and display in a 56×56 box.

Application script: `Tools/UIMenuHubReference/apply.py`. It expects the textures to be imported and changes only the three menu WBPs. Backups, widget snapshots, compilation and runtime checks are in `Saved/UIMenuHubReference/`.
