# 퀘스트 수락창

시안 `/Game/Developers/MinkiCho/Art/퀘스트_수락창_시안`을 기준으로 `WBP_QuestWidget.OfferPanel`에 적용·저장했다.

- 중앙 980 × 660 패널: 남색 왼쪽 장식부와 밝은 종이색 본문, 금색 테두리.
- 나침반·도시 그림은 `Art/Quest/Offer/Textures/T_QuestOffer_Compass`와 `T_QuestOffer_City`로 분리했다. 원본 영역의 픽셀과 가장자리 필터링 여백 2px를 보존하며 시안 텍스처를 직접 참조하지 않는다. 제목·버튼·설명은 실제 UMG 위젯으로 구성한다.
- 지역명은 퀘스트의 LocationId를 기존 월드 데이터에서 조회한다. 지역명이 없으면 줄을 숨긴다.
- 목표는 기존 WBP_QuestJournalObjectiveRow, 보상은 WBP_QuestJournalRewardTile을 템플릿으로 생성한다. 숨긴 목표와 선행 목표 조건을 따르며, 경험치·골드·아이템 수량은 실제 정의에서 읽는다.
- 골드 아이콘은 공용 `T_Icon_Gold`의 시안 기반 금화로 표시한다. 제작 원본·설정·프롬프트는 `ArtSource/UI/Currency/README.md`에 기록했다.
- 템플릿이 없는 예전 레이아웃과 무보상 퀘스트는 기존 텍스트 표시를 사용한다.
- 긴 본문만 스크롤하고 하단 버튼은 고정한다. 다시 열 때 스크롤을 처음으로 돌린다.
- 기존 수락·거절 버튼 객체와 C++ 이벤트 연결을 유지한다. 추적창 구성은 변경하지 않았다.
- 수락 버튼은 보상창 `WBP_RewardPopup.CloseButton`, 나중에 버튼은 확인창 `WBP_ConfirmDialog.CancelButton`의 시각 스타일을 그대로 재사용한다. 모서리가 깎인 이중 금색 테두리, 강조색/남색 바탕, 호버·눌림·비활성 상태와 눌림 시 글자 이동을 공유한다. `apply.py`의 `style_action_buttons()`에 반영되어 전체 레이아웃을 다시 적용해도 유지된다.

`apply.py`는 재적용 가능한 제작 스크립트다. 원본 파일·위젯 스냅샷을 Saved 아래에 보관하고 하나의 Undo 트랜잭션으로 편집한다. 임포트만으로는 적용되지 않는다. `apply()`는 미저장 편집, `apply(save=True)`는 컴파일 성공 후 저장이다.

2026-10-06 시안 의존성 분리: 두 이미지 브러시 외 위젯 스냅샷이 동일하며, 에디터 재시작 후 새 텍스처 연결과 컴파일 오류·경고 0을 확인했다. `메뉴_허브_시안`과 `퀘스트_수락창_시안`의 에셋 참조는 모두 0개다. 검증 기록은 `Saved/QuestOfferSeparateArt/verification.json`, 픽셀 원본은 `ArtSource/UI/QuestOffer/`에 있다. 미리보기 캡처는 Revvy `CapturePreview` 크래시로 완료하지 못했으며 에디터는 재시작했다.

## 확인 결과

- C++ Live Coding 컴파일·실행 중 에디터 패치 적용 성공.
- 최종 위젯 컴파일: 오류 0, 경고 0. 재적용 시 추적창 스냅샷 동일.
- PIE에서 ‘시안의 첫 거래’의 경험치 140·골드 120 표시 확인.
- 실제 버튼 델리게이트로 퀘스트 10900 수락 → Active 전환·창 닫힘·이동 차단 해제 확인.
- 나중에 버튼 → 닫기·재열기, 무보상 퀘스트, 세 가지 보상 퀘스트 확인.
- 긴 제목·28줄 설명의 스크롤과 고정 버튼, 재열기 시 스크롤 초기화 확인.
- 두 버튼에 사용자 포커스를 줄 수 있음을 확인.

실제 마우스 클릭·패드 입력과 1280×720 창 크기는 검증하지 않았다. PIE의 r.SetRes가 창 크기를 바꾸지 않아 화면 검증은 1914×994 뷰포트에서 수행했다.

검증 기록: `Saved/UIQuestOfferReference/runtime_checks.json`.
화면: `Saved/UIQuestOfferReference/offer_final00001.png`.
원본 백업과 적용 전후 스냅샷: `Saved/UIQuestOfferReference/`의 실행별 폴더.

## 버튼 스타일 확인 결과

버튼과 라벨 네 개 외에는 위젯 스냅샷이 동일하며, 저장 후 컴파일 오류·경고는 0이다. PIE에서 일반 상태와 호버 브러시 표시를 확인했다. 호버 이미지는 실행 중 인스턴스에 호버 브러시를 임시 적용해 캡처한 후 복원했으며, 실제 마우스 이동을 통한 확인은 아니다. 기존 클릭 델리게이트를 통해 10900 퀘스트 Active 전환, 나중에 닫기, 재열기, 이동 차단 해제를 다시 확인했다.

검증 기록: `Saved/UIQuestOfferReference/buttons_checks.json`.
최종 화면: `Saved/UIQuestOfferReference/buttons_final00000.png`.
호버 브러시 미리보기: `Saved/UIQuestOfferReference/buttons_hover_preview00000.png`.
