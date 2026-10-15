// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tools/VFXViewer/NUVFXViewerTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ANUVFXViewerDirector;
class ITableRow;
class STableViewBase;
enum class ENUVFXViewerTransportIcon : uint8;

// 전투 VFX 뷰어 화면, 개발 도구라 WBP 없이 Slate 로만 그림 (SNUInventoryDebugOverlay 와 같은 방식)
// 왼쪽 패널 · 3D 화면 아래 진행바 · 감는 동안 3D 칸을 덮는 직전 화면을 한 위젯이 그림
// 상태는 전부 감독(ANUVFXViewerDirector)이 들고, 패널은 매 프레임 그 값을 읽어 표시함
class SNUVFXViewerPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNUVFXViewerPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ANUVFXViewerDirector>, Director)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// 단축키(Shift+[ ])로 스킬이 바뀌면 목록 선택 · 스크롤을 따라감
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// 감독이 목록을 다시 만든 뒤(새로고침) 부름
	void RefreshLists();

	// 왼쪽 패널을 끝까지 내림 (자동 캡처로 아래쪽 카드를 찍을 때)
	void ScrollPanelToEnd();

private:
	ANUVFXViewerDirector* GetDirector() const { return Director.Get(); }
	EVisibility GetPanelVisibility() const;

	/* 공용 조각 */

	TSharedRef<SWidget> MakeCard(const FText& Title, const TSharedRef<SWidget>& Content) const;
	TSharedRef<SWidget> MakeButton(const TAttribute<FText>& Label, TFunction<bool()> IsSelected,
		TFunction<void()> OnClick, const FLinearColor& BaseColor, const FText& ToolTip = FText::GetEmpty()) const;
	TSharedRef<SWidget> MakeButton(const TAttribute<FText>& Label, TFunction<bool()> IsSelected,
		TFunction<void()> OnClick) const;
	TSharedRef<SWidget> MakeLabeledRow(const FText& Label, const TSharedRef<SWidget>& Content) const;
	TSharedRef<class SWrapBox> MakeWrap() const;

	/* 왼쪽 패널 */

	TSharedRef<SWidget> MakeHeader();
	TSharedRef<SWidget> MakeSceneCard();
	TSharedRef<SWidget> MakeStageRow();
	TSharedRef<SWidget> MakeCasterRow();
	TSharedRef<SWidget> MakeGenderRow();
	TSharedRef<SWidget> MakeJobRow();
	TSharedRef<SWidget> MakeNaiaRow(bool bEnemy);
	TSharedRef<SWidget> MakeEnemyCountRow();
	TSharedRef<SWidget> MakeTargetRow();
	TSharedRef<SWidget> MakeSourceCard();
	TSharedRef<SWidget> MakeSkillSection();
	TSharedRef<SWidget> MakeCustomSection();
	TSharedRef<SWidget> MakeCustomSlotRow(int32 SlotIndex, const FText& SlotName);
	TSharedRef<SWidget> MakeColorRows();
	TSharedRef<SWidget> MakeOptionsCard();

	// 고른 스킬에 후보(-VFXCompare)가 있을 때 스킬 목록 아래에 나옴, 후보 버튼은 MaxCandidateButtons 개까지
	TSharedRef<SWidget> MakeCandidateSection();

	// 적용 대기 목록, 감독의 목록이 바뀌면(GetPendingVersion) Tick 에서 줄을 다시 만듦
	TSharedRef<SWidget> MakePendingCard();
	void RebuildPendingRows();
	TSharedRef<SWidget> MakeReportCard();
	TSharedRef<SWidget> MakeShortcutHint() const;

	/* 3D 화면 아래 진행바 */

	TSharedRef<SWidget> MakeTransportBar();
	TSharedRef<SWidget> MakeTransportButton(TAttribute<ENUVFXViewerTransportIcon> Icon, const FText& ToolTip,
		TFunction<void()> OnClick) const;

	/* 목록 */

	void RebuildStageFilter();
	TSharedRef<ITableRow> GenerateStageRow(TSharedPtr<FNUVFXViewerChoice> Item, const TSharedRef<STableViewBase>& Owner);
	void HandleStageClicked(TSharedPtr<FNUVFXViewerChoice> Item);

	void RebuildSkillFilter();
	void RebuildAssetFilter();
	TSharedRef<ITableRow> GenerateSkillRow(TSharedPtr<FNUVFXViewerSkillItem> Item, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> GenerateAssetRow(TSharedPtr<FNUVFXViewerAssetItem> Item, const TSharedRef<STableViewBase>& Owner);
	void HandleSkillClicked(TSharedPtr<FNUVFXViewerSkillItem> Item);
	void HandleSkillSelectionChanged(TSharedPtr<FNUVFXViewerSkillItem> Item, ESelectInfo::Type SelectInfo);

	// 나이아 목록에서 한 칸 앞 · 뒤
	void StepNaia(bool bEnemy, int32 Delta);

	FText GetCustomSlotText(int32 SlotIndex) const;

private:
	TWeakObjectPtr<ANUVFXViewerDirector> Director;

	// 무대는 늘어날 수 있어 스킬처럼 검색 목록으로 고름
	FString StageFilter;
	TArray<TSharedPtr<FNUVFXViewerChoice>> StageItems;
	TArray<TSharedPtr<FNUVFXViewerChoice>> FilteredStages;
	TSharedPtr<SListView<TSharedPtr<FNUVFXViewerChoice>>> StageList;

	FString SkillFilter;

	// 후보가 있는 스킬만 목록에 둠
	bool bCandidatesOnly = false;

	// 목록 선택을 맞춘 마지막 스킬
	int32 LastSyncedSkillId = INDEX_NONE;

	TSharedPtr<class SScrollBox> PanelScroll;

	// 적용 대기 줄을 담는 칸과 마지막으로 그린 목록 판
	TSharedPtr<SVerticalBox> PendingRows;
	int32 LastPendingVersion = INDEX_NONE;
	TArray<TSharedPtr<FNUVFXViewerSkillItem>> FilteredSkills;
	TSharedPtr<SListView<TSharedPtr<FNUVFXViewerSkillItem>>> SkillList;

	FString AssetFilter;
	TArray<TSharedPtr<FNUVFXViewerAssetItem>> FilteredAssets;
	TSharedPtr<SListView<TSharedPtr<FNUVFXViewerAssetItem>>> AssetList;
};
