// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/UI/NUSheetEntryButton.h"

void UNUInventoryCellBorder::SetCellCoords(int32 InContainer, int32 InSlotNo)
{
	CellContainer = InContainer;
	CellSlotNo = InSlotNo;
	// CDO 생성자에서 다이내믹 바인딩은 직렬화에 섞이므로 여기(런타임 초기화)에서 건다
	OnMouseButtonDownEvent.BindDynamic(this, &UNUInventoryCellBorder::HandleMouseDown);
}

FEventReply UNUInventoryCellBorder::HandleMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	if (OnCellMouseDown.IsBound())
	{
		return OnCellMouseDown.Execute(CellContainer, CellSlotNo, MouseEvent);
	}
	return FEventReply(true); // 셀 클릭이 창 드래그로 번지지 않게 소비
}
