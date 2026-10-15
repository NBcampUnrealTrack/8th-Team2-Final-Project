// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Party/NUPartyTypes.h"
#include "NUPartyListWidget.generated.h"

class AFieldRemoteCharacter;
class UNUPartyMemberRow;
class UNUPartySubsystem;
class UVerticalBox;


// 화면 좌측 파티원 목록
UCLASS(Abstract)
class PROJECTM_API UNUPartyListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 전투 중처럼 잠깐 가려야 할 때 PlayerController가 호출해서 켜고 끔
	void SetSuppressed(bool bInSuppressed);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 창 이동, 좌클릭으로 잡아서 끌면 파티창의 캔버스 슬롯을 옮김
	// 줄에서 올라온 좌클릭도 여기서 받으므로 창 어디를 잡아도 이동함
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 알트탭 등으로 캡처를 잃으면 끌던 상태가 남으므로 캡처를 잃었을때도 놓음 처리
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	// Visibility 처리는 이곳에서 한번에 처리
	void RefreshVisibility();

	// 파티 구성이 바뀜
	void HandlePartyChanged(const FNUPartySnapshot& Snapshot, ENUPartyUpdateReason Reason, int64 TargetIdx);

	// 원격 캐릭터 등장, Dynamic 델리게이트라 UFUNCTION
	UFUNCTION()
	void HandleRemoteSpawned(AFieldRemoteCharacter* RemotePlayer);

	// 원격 캐릭터 소멸, Dynamic 델리게이트라 UFUNCTION
	UFUNCTION()
	void HandleRemoteDespawned(int64 PlayerId);

	// 파티원 목록 행을 재생성함
	void RebuildRows(const FNUPartySnapshot& Snapshot);

	// 행을 우클릭했을 때 컨텍스트 메뉴 열기
	void HandleRowRightClicked(int64 CharacterIdx);

	UNUPartySubsystem* GetPartySubsystem() const;
	class UFieldSyncSubsystem* GetFieldSync() const;

	UNUPartyMemberRow* FindRow(int64 CharacterIdx) const;

private:
	// 줄이 들어갈 세로 박스
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> MemberBox;

	// 창 프레임, 이 위젯 자체가 드래그 가능한 면적
	UPROPERTY(Transient)
	TObjectPtr<UWidget> PartyFrame;

	// PartyFrame의 캔버스 슬롯, 드래그로 이 위치를 옮김
	UPROPERTY(Transient)
	TObjectPtr<class UCanvasPanelSlot> FrameSlot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNUPartyMemberRow>> Rows;

	// 줄 위젯 클래스, WBP에서 지정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Party", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UNUPartyMemberRow> MemberRowClass;

	// 표시 조건 두 가지, 둘 다 만족해야 보임
	
	bool bHasParty = false;	// 파티가 있는지, true여야 보임
	bool bSuppressed = false; // 가려야 하는 상황인지, false여야 보임

	// 창을 끌고 있는 중인지
	bool bDragging = false;
};
