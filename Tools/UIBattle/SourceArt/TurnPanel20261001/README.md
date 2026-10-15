# Battle turn HUD, 2026-10-01

`/Game/Developers/MinkiCho/UI/WBP_BattleHUD` follows the top-left TURN design in `/Game/Developers/MinkiCho/인게임_전투_UI_패널부분`: a gold star crest, TURN and number on one line, thin gold divider with a diamond endpoint, and the phase below. Uses the project's `MaruBuri-SemiBold_Font`.

`T_BattleTurnCrest.png` is the unmodified transparent output from built-in image_gen. Exact prompt: [generation.md](generation.md). Unreal texture: `/Game/Developers/MinkiCho/Art/Battle/TurnPanel20261001/T_BattleTurnCrest` (UI texture, sRGB, no mipmaps).

Only six existing widgets were restyled: `TurnCompass`, `TurnCompassDetail`, `TurnRule`, `Txt_Title`, `Txt_Turn`, `Txt_Phase`. `TurnCompassDetail` is reused as the divider endpoint. No widget deletion. Existing names, hierarchy, valid GUIDs, native text bindings, two-digit number formatting, server/local turn and phase updates remain intact. Other HUD decorations are unchanged.

Backup, apply script, property snapshots and static preview: `Saved/BattleTurnPanel_20261001/`. The preview renders saved widget properties in a temporary UserWidget; it is not a gameplay screenshot. That fixture is deleted after capture. WBP compile: 0 errors, 0 warnings. WBP and texture explicitly force-saved; fresh commandlet reads verify persistence. No PIE/login/server battle test.
