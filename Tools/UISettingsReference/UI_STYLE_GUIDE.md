# UI spacing and button baseline

Superseded by [the shared layout skill](../UILayoutContract/SKILL.md) and
`../UILayoutContract/apply.py`. The measurements below describe the previous
feedback iteration; do not use them for new positioning work.

Authored UMG design units; viewport DPI scaling happens after layout.

| Control | Height | Font size | Horizontal padding |
| --- | ---: | ---: | ---: |
| Compact option choice | 36 | 16 | 12 |
| Standard action / launcher | 44 | 18 | 16 |
| Hub illustrated navigation | 56.5 | artwork typography | existing atlas |

The hub keeps its illustrated bilingual labels; the source atlas is mapped to a
56.5-high visible row with a 12-unit gap on a 68.5-unit pitch. Live titles use
26-point Korean text, independently of the illustration. Do not bake new UI copy
into artwork.

Settings content and footer rules span x72..886. Scrollbar width 5 plus a 10-unit
gutter is included in the content viewport. Tab divider y147, height 2; content begins
y172 (23 clear units). Cards use 16-unit horizontal padding, 14-unit vertical
padding, 14-unit group separation. Card headings use 16-point champagne, row
labels 18-point ivory. Header X sits independently at x888.

Use group cards for related settings; do not outline every label. Each card
background is a Border, its content is a separate VerticalBox. Existing frame,
background, foreground and hover layers remain separate and decorative elements
do not intercept input. No full-screen blur or extra dim layer is introduced.

Current authoring order ends with `apply_feedback_polish.py` (settings) and
`../UIHubSidePanels/apply_feedback_polish.py` (hub). Earlier passes can overwrite
these choices; reapply final passes when rebuilding.
