// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Inventory/NUEquipmentTypes.h"
#include "NUSheetEntryButton.generated.h"

// 셀 마우스다운을 창에 위임 — 창이 드래그 감지(FEventReply)까지 결정한다
DECLARE_DELEGATE_RetVal_ThreeParams(FEventReply, FOnNUInventoryCellMouseDown, int32 /*Container*/, int32 /*SlotNo*/, const FPointerEvent& /*MouseEvent*/);

// 인벤토리 그리드 한 칸. UButton은 좌클릭만 알려주므로, 마우스다운을 칸 번호와 함께
// 창으로 위임하는 Border 기반 셀 (C++ 동적 생성 전용). 드래그 시작 여부는 창이 결정.
// 거래 창의 내 제안 칸이 아직 쓴다 --- 인벤·장비 칸은 WBP_ItemCell(UNUItemCellWidget)로 옮겼다
UCLASS()
class PROJECTM_API UNUInventoryCellBorder : public UBorder
{
	GENERATED_BODY()

public:
	// (저장소, 칸) 좌표 — 인벤/창고 패널이 한 창에 섞여도 셀이 자기 출처를 안다
	void SetCellCoords(int32 InContainer, int32 InSlotNo);
	int32 GetCellContainer() const { return CellContainer; }
	int32 GetCellSlotNo() const { return CellSlotNo; }

	FOnNUInventoryCellMouseDown OnCellMouseDown;

private:
	UFUNCTION()
	FEventReply HandleMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	int32 CellContainer = 1;
	int32 CellSlotNo = INDEX_NONE;
};
