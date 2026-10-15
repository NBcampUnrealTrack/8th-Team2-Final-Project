#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/UI/NUProgressionWindowBase.h"
#include "UI/NUConfirmDialog.h"
#include "CharacterProgression/UI/Job/NUJobSelectionCardWidget.h"   // TSubclassOf 멤버 --- 여는 쪽 번역 단위에도 완전한 형이 필요
#include "NUJobSelectionWidget.generated.h"

class UNUJobComponent;
class UNUJobSelectionEntryButton;
class UNUProgressionComponent;
class UHorizontalBox;
class UImage;
class UPanelWidget;
class UTextBlock;
class UTexture2D;
class UWidget;

// 직업 정의를 선택 화면에 표시하기 위한 읽기 전용 데이터
USTRUCT(BlueprintType)
struct PROJECTM_API FNUJobSelectionViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Job|UI")
	int32 JobId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Job|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Job|UI")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Job|UI")
	FLinearColor AccentColor = FLinearColor::White;
};

// 다섯 직업을 표시하고 선택 확인 후 서버에 최초 직업을 요청
UCLASS(Abstract)
class PROJECTM_API UNUJobSelectionWidget : public UNUProgressionWindowBase
{
	GENERATED_BODY()

public:
	// 현재 플레이어의 직업 상태와 창 닫기 요청을 연결
	void InitializeSelection(UNUJobComponent* InJobComponent, UNUProgressionComponent* InProgression, FSimpleDelegate InCloseRequested, FSimpleDelegate InSelectionCompleted);

	// 목록에서 고른 직업을 임시 선택하고 확인 가능한 상태를 갱신
	UFUNCTION(BlueprintCallable, Category = "NU|Job|UI")
	void SelectJob(int32 JobId);

	UFUNCTION(BlueprintPure, Category = "NU|Job|UI")
	const TArray<FNUJobSelectionViewData>& GetJobEntries() const { return JobEntries; }

	// 창을 여는 쪽(UNUProgressionUIController)이 창 옵션에 싣는다
	bool ShouldHideFieldHUD() const { return bHideFieldHUDWhileOpen; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool CanDragWindow() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// ---------- 무대형 화면 (선택, 기본 꺼짐 --- 비우면 옛 회색 카드와 "선택: 이름" 문구 그대로) ----------

	// 직업 한 칸. 지정하면 JobListPanel에 이 칸을 만든다 (내 캐릭터가 그 직업 차림으로 선다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	TSubclassOf<UNUJobSelectionCardWidget> JobCardClass;

	// 직업 문장 (키 = 직업 표 JobKey). 칸과 아래 판의 문장 그림
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	TMap<FName, TSoftObjectPtr<UTexture2D>> JobEmblems;

	// 직업 색 (키 = JobKey). 비면 GetJobAccent. 빛 기둥·판 왼쪽 띠·문장 색
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	TMap<FName, FLinearColor> JobAccents;

	// 열면 첫 직업을 골라 둔다 (아래 판이 빈 채로 시작하지 않게)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	bool bSelectFirstOnOpen = false;

	// 열려 있는 동안 필드 HUD를 접는다 (대화창·상점과 같은 방식)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	bool bHideFieldHUDWhileOpen = false;

	// 시작 스킬 한 줄 (이름으로 찾는 글자: SkillKind · SkillName · SkillDesc, 종류 칩 테두리 SkillKindChip). 지정하면 SkillList를 채운다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	TSubclassOf<UUserWidget> SkillRowClass;

	// 전직 갈래 칩 (글자 ChipText). 지정하면 AdvancementList를 채운다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	TSubclassOf<UUserWidget> AdvancementChipClass;

	// 확인 창 (비우면 프로젝트 세팅 NU UI의 공용 확인 창)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	TSubclassOf<UNUConfirmDialog> ConfirmDialogClass;

	// 확인 버튼 글자 --- {0} = 직업 이름 + 로/으로
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Job|UI")
	FText ConfirmLabelFormat = NSLOCTEXT("NUJob", "ConfirmLabelFormat", "{0} 정하기");

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> DetailEmblem;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> AccentBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> SkillList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> AdvancementList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ConfirmLabel;

	UFUNCTION(BlueprintNativeEvent, Category = "NU|Job|UI", meta = (DisplayName = "직업 선택 목록 갱신"))
	void BP_OnJobEntriesChanged(const TArray<FNUJobSelectionViewData>& InJobEntries);
	virtual void BP_OnJobEntriesChanged_Implementation(const TArray<FNUJobSelectionViewData>& InJobEntries);

	UPROPERTY(BlueprintReadOnly, Category = "NU|Job|UI")
	TArray<FNUJobSelectionViewData> JobEntries;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> JobListPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> LevelStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> SelectedJobText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> KeyHintText;

private:
	// WBP 버튼 교체 중에도 순정 Button과 공용 버튼을 모두 지원한다.
	bool BindConfirmButton();
	void UnbindConfirmButton();
	void SetConfirmButtonEnabled(bool bEnabled);
	// 검증된 직업 DataTable을 화면용 데이터와 카드 목록으로 변환
	void RebuildJobEntries();
	UWidget* BuildDefaultJobCard(const FNUJobSelectionViewData& Entry);
	UWidget* BuildStageCard(const FNUJobSelectionViewData& Entry);
	bool UsesStage() const { return JobCardClass != nullptr; }
	// 아래 판: 문장·띠 색·시작 스킬·전직 갈래·확인 버튼 글자
	void RefreshDetail(const FNUJobSelectionViewData* Entry);
	// 고른 칸을 Step만큼 옮긴다 (키보드 ←→ · 패드 D패드)
	void StepSelection(int32 Step);
	FLinearColor ResolveAccent(int32 JobId) const;
	UTexture2D* ResolveEmblem(int32 JobId) const;
	// 현재 직업 상태를 기준으로 선택 강조 및 확인 버튼을 갱신
	void RefreshState();
	// 선택한 JobId를 서버 검증 경로로 전달
	void SubmitJobSelection();
	const FNUJobSelectionViewData* FindSelectedEntry() const;
	static FLinearColor GetJobAccent(int32 JobId);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleSelectionConfirmed();

	UFUNCTION()
	void HandleSelectionCancelled();

	UFUNCTION()
	void HandleJobChanged();
	UFUNCTION()
	void HandleProgressionRequestStateChanged();
	UFUNCTION()
	void HandleProgressionRequestFailed(int32 ResultCode, const FString& Message);

	TWeakObjectPtr<UNUJobComponent> JobComponent;
	TWeakObjectPtr<UNUProgressionComponent> Progression;
	FSimpleDelegate SelectionCompleted;
	int32 SelectedJobId = 0;

	UPROPERTY(Transient)
	TObjectPtr<UNUConfirmDialog> ActiveConfirmDialog;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UNUJobSelectionEntryButton>> JobButtons;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UNUJobSelectionCardWidget>> JobCards;
};
