# 골드 아이콘

`T_Icon_Gold.png`는 작은 표시 크기를 위해 다시 만든 공용 금화다. 시안의 색감과 중앙 문양을 유지하면서 정면 원형, 굵은 테두리, 큰 단일 문양으로 단순화했다. 이전 이미지의 잔흠집·가는 동심원·기울어진 측면을 제거했다. 내장 image_gen 도구로 제작했다.

- 적용 에셋: `/Game/Developers/dbals/UI/Icons/T_Icon_Gold`
- 공용 사용처: 퀘스트 수락창·퀘스트 일지의 보상 타일, 소지품 창, 상점 상품 행, 상점 합계.
- 기존 텍스처 경로를 유지해 연결된 위젯에 공통 적용한다. 표시 크기와 레이아웃은 기존 값을 사용한다.
- UE 설정: UI 텍스처 그룹, UserInterface2D(RGBA) 압축, sRGB, Trilinear 필터, SimpleAverage 밉맵, NeverStream. 원본은 1254×1254이며 Resize To Specific Resolution으로 정확히 128×128로 빌드한다. 최대 크기는 128이다.
- 퀘스트 금화는 46×46, 소지품은 22×22, 상점 상품은 24×24 UMG 슬롯을 사용한다. 화면 픽셀 크기에는 DPI 배율이 적용된다. 새 아이콘은 22~46 단위의 작은 크기에서 문양이 구분되도록 설계했다.
- 재임포트 원본 경로는 프로젝트 안의 이 PNG를 가리킨다. 도구 임시 폴더에 의존하지 않는다.
- 이전 세밀한 버전의 에셋·PNG·문서는 `Saved/UIGoldIcon/small_before_20261002_173257/`에 보관했다.

## 소형 버전 확인

PIE 수락창의 골드 120 표시와 동일 텍스처를 사용한 22·24·32·46 UMG 단위의 샘플을 시각 확인했다. 샘플의 정사각형 SizeBox 설정을 검증했으며 샘플 위젯은 PIE에만 임시 생성하고 제거했다. 네 소비 위젯의 기존 브러시 크기와 에셋 경로도 동일하다. 소지품·상점 전체 화면과 별도 화면 해상도는 이번에 실행하지 않았다. CachedGeometry 조회는 0을 반환해 실측 픽셀 크기로 기록하지 않았다.

- 수락창: `Saved/UIGoldIcon/SmallSize/offer_final00000.png`
- 크기별 샘플: `Saved/UIGoldIcon/SmallSize/size_check00000.png`
- 설정·검증: `Saved/UIGoldIcon/SmallSize/checks.json`, `sizes.json`, `consumer_brushes.json`

## 이전 버전 확인

PIE에서 수락창(골드 120)과 퀘스트 일지(골드 100)의 새 아이콘 표시를 확인했다. 연결된 위젯 네 개의 텍스처 참조·브러시 크기는 교체 전과 동일하고 컴파일 오류·경고는 0이다. 소지품·상점은 참조와 컴파일을 확인했으며 해당 게임 화면을 열어 검증하지는 않았다.

화면: `Saved/UIGoldIcon/quest_offer00000.png`, `Saved/UIGoldIcon/quest_journal_gold00000.png`.
검증 기록: `Saved/UIGoldIcon/checks.json`.

## 새 소형 아이콘 생성 프롬프트

최종 프롬프트는 `prompt-small.txt`에 보관했다. 내장 image_gen, transparent_background=true. 입력은 이전 금화 원본과 퀘스트 수락창 시안이다.

## 이전 버전 생성 프롬프트

Reference: `C:/Users/user/Downloads/퀘스트 수락창 시안.png`. Built-in image_gen, transparent_background=true.

Asset type: a single transparent gold-currency icon for the ProjectM fantasy RPG UI. Image 1 is a STYLE REFERENCE, not a scene to reproduce: the supplied quest-acceptance design. Re-create ONLY the small engraved gold coin shown next to the Korean word 골드 in the reference, as a clean high-resolution standalone game icon on true transparency. One upright round coin, nearly front-facing with a very slight perspective tilt matching the reference. Thin double circular raised rim, a simple small embossed lozenge/seed-shaped crest in the center, subtle concentric engraved ornamental lines. Restrained antique brass / warm muted gold, softly shaded shallow metallic relief, fine tasteful wear, brighter thin edge so the silhouette reads clearly on both navy and parchment. Same mature illustrated fantasy aesthetic as the reference. Prioritize legibility at 24-48 px: clean readable silhouette, restrained detail, no tiny text. Center the coin and make it occupy roughly 85 percent of a square canvas. True transparent background; no navy tile, no card, no frame outside the coin, no ground or cast shadow. No letters, numbers, currency signs, stars, magical sparkles, glow, cartoon bevels, thick pixelated black outline, saturated lemon yellow, plastic/mobile-game appearance, or multiple coins. Deliver only the icon artwork as a transparent PNG.
