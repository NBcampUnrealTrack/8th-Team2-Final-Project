#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/NUUIManager.h"
#include "Types/SlateEnums.h"
#include "NUGameMenuWidget.generated.h"

class UNU_CommonButtonWidget;
class ANUCharacterPreviewActor;
class UTileView;
class UTexture2D;

// ESC 전체 메뉴 — 주요 메뉴 TileView + 하단 시스템 줄.
// 항목은 매니저의 게임 메뉴 레지스트리에서 읽는다 — 이 위젯은 어떤 기능도 직접 모른다.
//
// 팀 패턴: C++는 로직/계약만, 모양은 WBP가 담당한다.
//   - WBP가 TileView의 배치·타일 크기·EntryWidgetClass·아이콘을 정한다.
//   - RefreshEntries()는 등록된 주요 메뉴의 표시 데이터를 채운다. 빠진 항목은 빈칸을 남기지 않는다.
//   - 시스템 버튼 3개는 고정 BindWidget을 유지하며, 모든 클릭은 EntryId로 등록된 동작을 실행한다.
UCLASS(Abstract)
class PROJECTM_API UNUGameMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UNUGameMenuWidget(const FObjectInitializer& ObjectInitializer);

	// 매니저가 열 때마다 레지스트리, 라벨, 사용자 단축키를 갱신한다.
	void RefreshEntries();

	// 타일과 고정 시스템 버튼이 공유하는 실행 경로. 실행 전에 등록 상태를 다시 확인한다.
	void ActivateEntry(FName EntryId);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 고정 버튼 하나를 EntryId로 찾은 항목과 동기화한다 --- 항목이 없으면 Collapsed
	void BindEntryButton(UNU_CommonButtonWidget* Button, FName EntryId);

	UFUNCTION() void HandleOptionsClicked();
	UFUNCTION() void HandleCharacterSelectClicked();
	UFUNCTION() void HandleQuitGameClicked();
	UFUNCTION() void HandleHubCloseClicked();

	// 허브의 데이터만 채운다. 배치와 스타일은 WBP에서 편집한다.
	void RefreshHubProfile();
	void ReleaseHubPortrait();

	void InitializeHubMotto();
	UFUNCTION() void HandleHubMottoCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	FString HubMottoSection;
	FString SavedHubMotto;

	// 현재 표시된 항목 — 고정 시스템 버튼의 표시를 갱신할 때 사용한다.
	TArray<FNUGameMenuEntry> ShownEntries;

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UButton> HubCloseButton;

	UPROPERTY(Transient)
	TObjectPtr<ANUCharacterPreviewActor> HubPreviewActor;

	// 타일의 외형과 열 수는 WBP 디자이너에서 정한다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTileView> MainMenuTileView;

	UPROPERTY(EditDefaultsOnly, Category = "NU|GameMenu")
	TMap<FName, TObjectPtr<UTexture2D>> MenuEntryIcons;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> OptionsButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> CharacterSelectButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNU_CommonButtonWidget> QuitGameButton;
};
