// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Party/NUPartyTypes.h"
#include "NUPartyMemberRow.generated.h"

// 줄을 우클릭했을 때 목록이 받아서 컨텍스트 메뉴를 열음
DECLARE_DELEGATE_OneParam(FNUOnPartyRowRightClicked, int64);

// 파티 목록의 한 줄. 번호/이름/리더 왕관/초상화/체력바 등
// 체력은 이후에 서버에서 받아서 SetPercent로 조절해야 함
UCLASS(Abstract)
class PROJECTM_API UNUPartyMemberRow : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMember(int32 InSlotNumber, const FNUPartyMember& Member, bool bInIsMe);

	// 시야안에 있는지 확인, 들어올 때 받아둔 PlayerId를 나갈 때 확인함
	void SetNearby(bool bInNearby, int64 InPlayerId = 0);

	// 지금은 항상 1.0, 서버가 체력을 주면 여기서 조절함
	void SetHealthPercent(float Percent);

	int64 GetCharacterIdx() const;
	int64 GetNearbyPlayerId() const;
	bool IsMe() const;

public:
	FNUOnPartyRowRightClicked OnRowRightClicked;

protected:
	// 우클릭이면 컨텍스트 메뉴를 열고, 좌클릭은 창 이동에 쓰이도록 목록으로 흘려보냄
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
	// WBP가 번호·이름·왕관·초상화를 그림
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Party")
	void OnMemberSet(int32 InSlotNumber, const FText& MemberName, bool bInIsLeader, bool bInIsMe);

	// 멀리 있으면 회색,반투명. 표현은 WBP가 정함
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Party")
	void OnNearbyChanged(bool bInNearby);

	// 체력바 갱신
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Party")
	void OnHealthChanged(float Percent);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	int32 SlotNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	FText MemberDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	bool bIsLeader = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	bool bIsMe = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	bool bNearby = false;

	// 선택: 있으면 C++가 직접 채운다 (BP 이벤트도 그대로 불린다)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> RowNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> RowLeaderMark;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UProgressBar> RowHealthBar;

	// 멀리 있는 파티원은 이 위젯(보통 줄 전체)을 흐리게 한다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> RowBody;

private:
	int64 CharacterIdx = 0;

	// 시야 안일 때만 유효, 나가면 0
	int64 NearbyPlayerId = 0;
};
