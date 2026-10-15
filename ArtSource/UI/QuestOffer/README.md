# Quest-offer artwork extraction

Source: `퀘스트 수락창 시안.png`, 1742 × 903, MD5 `1d22556733abb67127b4135769d86cb9`.

These are exact pixel extractions for the existing `WBP_QuestWidget` brushes:

| Widget | PNG | Original region (x, y, width, height) | Output size |
|---|---|---|---|
| QO_Compass | T_QuestOffer_Compass.png | 484, 186, 117, 117 | 121 × 121 |
| QO_LowerCompass | T_QuestOffer_City.png | 446, 422, 198, 214 | 202 × 218 |

Each PNG retains two original pixels beyond every edge. The replacement UV rectangle excludes this gutter, preserving bilinear samples at the original region boundary. Extraction performs a pixel-by-pixel comparison against the source.

Reproduce with `Tools/UIQuestOfferReference/extract_art.ps1 -SourcePng <original PNG>`.
Destination: `/Game/Developers/MinkiCho/Art/Quest/Offer/Textures`.

The two textures are imported and `WBP_QuestWidget` uses them. `apply.py` also uses these independent assets, so reapplying the layout no longer requires the original reference. `separate_art.py` records the one-time migration with a widget backup, brush-only snapshot comparison, compilation and reference checks. Original reference assets were preserved.
