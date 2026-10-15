// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Inventory/UI/NUSheetWidgetBase.h"
#include "NUNaiaRosterWindowWidget.generated.h"

class UImage;
class UNUInventoryCellBorder;
class UNUNaiaRosterComponent;
class UPaperFlipbook;
class USizeBox;
struct FNUNaiaDefinition;

// 애니메이션 중인 파티 카드 초상화 하나
// Slate는 플립북을 직접 못 그리므로 매 프레임 스프라이트를 브러시를 갈아 끼우는 방식으로 처리해야 함
struct FNUNaiaPortraitAnim
{
	TWeakObjectPtr<UImage> Image;
	TWeakObjectPtr<UPaperFlipbook> Flipbook;
	float Duration = 0.f;
};

// 나이아 칸 드래그 페이로드
// 아이템과 같은 조합이지만 오퍼레이션 타입을 나눠 서로의 드롭을 받지 않게 함
UCLASS()
class UNUNaiaDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	int32 SourceContainer = 1;
	int32 SourceSlotNo = INDEX_NONE;
};

// 편성창 한 칸에 표시할 정보
// 파티는 액터에서, 보관함은 데이터에서 만들지만 창은 같은 형태로 그림
USTRUCT(BlueprintType)
struct FNUNaiaSheetEntry
{
	GENERATED_BODY()

	// 동일 나이아끼리도 구분해야해서 개별 인스턴스 ID
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia")
	int64 InstanceId = 0;

	// 파티창, 보관함창 구분
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia")
	int32 Container = 1;

	// 파티 슬롯 번호
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia")
	int32 SlotNo = INDEX_NONE;

	// 정령 구분용 ID (볼또, 찌르도치 ...) 
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia")
	int32 NaiaId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia")
	int32 Level = 0;

	// 체력바가 쓰는 값, 파티든 보관함이든 항상 채워짐
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia")
	float HealthRatio = 0.f;

	// 절대 수치, ASC 가 있는 파티만 채워짐
	// 보관함은 액터가 없어 최대치를 알 수 없으므로 0으로 남고 숫자를 그리지 않음
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia")
	float CurrentHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia")
	float MaxHealth = 0.f;

	// 파티 0번, 전투에 바로 참가하는 나이아
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia")
	bool bIsActive = false;
};

// 편성창 - 좌측 파티 카드 4장 + 우측 보관함 스크롤 그리드, 드래그로 칸 이동/교환.
// 인벤토리 창과 같은 구조
//
// WBP 에서 필요한 것
//   "PartyListBox"   - 큰 카드 4장
//   "StorageListBox" - 6열 그리드
//   "NaiaListBox"    - 위 둘 중 하나라도 없을 때, 한 패널에 그리드 둘을 나란히
//
// 조작: 드래그 = 칸 이동/교환, 우클릭 = 반대 컨테이너 첫 빈 칸으로 빠른 이동
UCLASS(Abstract)
class PROJECTM_API UNUNaiaRosterWindowWidget : public UNUSheetWidgetBase
{
	GENERATED_BODY()
	float RecoveryDisplayElapsed = 0.0f;
	void RefreshRecoveryDisplay();
	TMap<int64, TWeakObjectPtr<class UProgressBar>> RecoveryHealthBars;
	TMap<int64, TWeakObjectPtr<class UTextBlock>> RecoveryHealthTexts;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void RefreshData() override;
	virtual void CloseSelf() override;

	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
		UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Naia")
	void OnRosterListChanged();

protected:
	// 파티 편성창 
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia")
	TArray<FNUNaiaSheetEntry> PartyEntries;

	// 보관함
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia")
	TArray<FNUNaiaSheetEntry> StorageEntries;

	// 보관함 그리드 한 줄 칸 수 / 칸 픽셀 크기 — WBP 클래스 기본값에서 조정
	UPROPERTY(EditAnywhere, Category = "NU|Naia", meta = (ClampMin = 1))
	int32 GridColumns = 6;

	// 그리드 한 칸 크기
	UPROPERTY(EditAnywhere, Category = "NU|Naia", meta = (ClampMin = 32.f))
	float CellSize = 80.f;

	// 파티 카드 한 장 너비
	UPROPERTY(EditAnywhere, Category = "NU|Naia", meta = (ClampMin = 48.f))
	float PartyCardWidth = 106.f;

	// 파티 카드 한 장 높이
	UPROPERTY(EditAnywhere, Category = "NU|Naia", meta = (ClampMin = 48.f))
	float PartyCardHeight = 260.f;

	// 카드 초상화 높이, 원본 비율을 유지한 채 이 높이에 맞춘다
	UPROPERTY(EditAnywhere, Category = "NU|Naia", meta = (ClampMin = 16.f))
	float PortraitHeight = 132.f;

	// 나이아 정의에 그림이 없을 때 쓸 대체 외형 (플립북 / 스프라이트 / 텍스처)
	// 플립북을 넣으면 카드가 실제로 움직임, 나이아 에셋이 없을때 대체하기 위한 용도
	UPROPERTY(EditAnywhere, Category = "NU|Naia")
	TSoftObjectPtr<UObject> DefaultPortrait;

private:
	UFUNCTION()
	void HandleRosterChanged();

	// 로스터, 파티, 보관함 채우기
	void PopulateRosterList();
	void PopulatePartyPanel(UPanelWidget* Panel);
	void PopulateStoragePanel(UPanelWidget* Panel);

	// 폴백 경로 - 파티·보관함을 같은 모양의 그리드로 나란히
	UWidget* BuildContainerPanel(int32 Container);

	UWidget* BuildGridCell(int32 Container, int32 SlotNo, const FNUNaiaSheetEntry* Entry);
	USizeBox* BuildCellContent(const FNUNaiaSheetEntry& Entry);
	UWidget* BuildPartyCard(int32 SlotNo, const FNUNaiaSheetEntry* Entry);
	UWidget* BuildNaiaTooltip(const FNUNaiaSheetEntry& Entry) const;
	UTextBlock* BuildPanelHeader(int32 Container) const;
	UTextBlock* BuildHintText() const;

	// 초상화 - 정의에 그림이 없으면 FallbackPortrait, 플립북이면 OutFlipbook 을 채워 애니메이션 등록
	UObject* ResolvePortraitResource(int32 NaiaId, UPaperFlipbook*& OutFlipbook) const;
	UImage* BuildPortraitImage(UObject* Resource) const;

	const FNUNaiaDefinition* FindNaiaDefinition(int32 NaiaId) const;

	const FNUNaiaSheetEntry* FindEntry(int32 Container, int32 SlotNo) const;
	const TArray<FNUNaiaSheetEntry>& GetEntries(int32 Container) const;

	FEventReply HandleCellMouseDown(int32 Container, int32 SlotNo, const FPointerEvent& MouseEvent);

	// 우클릭 빠른 이동 - 반대 컨테이너의 첫 빈 칸으로 (빈 공간 없으면 무시)
	void QuickMove(int32 FromContainer, int32 FromSlotNo);

	// 드래그가 어디서 끝나든 커서 비주얼 정리
	// 비주얼 생성/추적/정리는 베이스(ShowDragVisual · ClearDragVisual · NativeTick)가 담당함
	UFUNCTION()
	void HandleDragEnded(UDragDropOperation* InOperation);

	UNUNaiaRosterComponent* ResolveRoster();
	int32 GetSlotCount(int32 Container) const;
	int32 FindFreeSlot(int32 Container) const;

	// 전투 중이면 드래그/우클릭을 받지 않음
	bool IsInteractionLocked() const;

private:
	TWeakObjectPtr<UNUNaiaRosterComponent> Roster;

	// 드래그 출발 (마우스 다운 기록 -> OnDragDetected에서 사용)
	int32 PendingDragContainer = 1;
	int32 PendingDragSlotNo = INDEX_NONE;

	// 드롭 판정용 캐시 - 키 (X=컨테이너, Y=칸). populate마다 재구성
	TMap<FIntPoint, TWeakObjectPtr<UNUInventoryCellBorder>> CellByCoord;

	// 매 틱 프레임을 갈아 끼울 카드 초상화들, populate마다 재구성
	TArray<FNUNaiaPortraitAnim> AnimatedPortraits;

	// 창을 연 뒤 흐른 시간, 목록이 갱신돼도 이어서 재생하려고 populate에서 건드리지 않음
	float PortraitElapsed = 0.f;
};
