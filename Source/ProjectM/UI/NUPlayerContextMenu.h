// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUPlayerContextMenu.generated.h"

UENUM(BlueprintType)
enum class ENUPlayerContextAction : uint8
{
	CharacterInfo,
	Whisper,
	PartyInvite,
	Trade,
	AddFriend,		// 미구현
	PartyLeave,		// 내 파티원 줄에서만
	PartyKick,		// 리더가 남의 파티원 줄에서만
	Cancel
};

// 메뉴를 여는 쪽이 채워서 넘김
USTRUCT(BlueprintType)
struct FNUPlayerContextTarget
{
	GENERATED_BODY()

	// 파티/거래/귓속말이 대상을 지목하는 키
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	int64 CharacterIdx = 0;

	// 0이면 시야 밖, 필드 액터가 필요한 항목만 이 값을 확인함
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	int64 PlayerId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	FText DisplayName;

	// 보여줄 항목 비트마스크
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	int32 ActionMask = 0;
};

namespace NUPlayerContext
{
	constexpr int32 ActionBit(ENUPlayerContextAction Action)
	{
		return 1 << static_cast<int32>(Action);
	}

	// 필드에서 다른 플레이어를 클릭했을 때
	PROJECTM_API int32 FieldActions();

	// 파티 목록에서 남의 줄을 눌렀을 때, 추방은 리더만
	PROJECTM_API int32 PartyMemberActions(bool bIsLeader);

	// 파티 목록에서 내 줄을 눌렀을 때
	PROJECTM_API int32 PartySelfActions();
}

UCLASS(Abstract)
class PROJECTM_API UNUPlayerContextMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	// FollowActor를 주면 메뉴가 그 캐릭터를 따라다닌다 (열릴 때의 간격을 지킨다). 파티 목록처럼 화면에 고정할 때는 비운다
	void Open(const FNUPlayerContextTarget& InTarget, const FVector2D& ScreenPos, AActor* FollowActor = nullptr);

	void Close();

	bool IsOpen() const;

	int64 GetTargetPlayerId() const;

	// WBP가 OnTargetChanged에서 버튼마다 SetVisibility에 물림
	// 컨텍스트 메뉴 창을 열 때 한 번만 정해짐
	UFUNCTION(BlueprintPure, Category = "NU|UI")
	ESlateVisibility GetActionVisibility(ENUPlayerContextAction Action) const;

	// 각 버튼이 자기 항목을 넣어 호출함
	UFUNCTION(BlueprintCallable, Category = "NU|UI")
	void ExecuteAction(ENUPlayerContextAction Action);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 패드 LB/RB = 가까운 순으로 다른 사람 (필드에서 연 메뉴만). 닫기(B·ESC)는 매니저가 맡는다
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 대상이 변경되면 WBP가 이름 텍스트와 항목 표시를 갱신함
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|UI")
	void OnTargetChanged();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	FText TargetDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	FNUPlayerContextTarget Target;

private:
	UFUNCTION()
	void HandleRemotePlayerDespawned(int64 PlayerId);

	class ANUPlayerController* GetOwningNUPlayerController() const;

	class UNUPartySubsystem* GetPartySubsystem() const;

	class UNUChatSubsystem* GetChatSubsystem() const;

private:
	// PlayerId로 판정할 수 없음, 시야 밖 파티원은 0이라 열려 있어도 0이 됨
	bool bIsOpen = false;
};
