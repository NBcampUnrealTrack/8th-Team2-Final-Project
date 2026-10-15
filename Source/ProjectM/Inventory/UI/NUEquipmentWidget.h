// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/UI/NUSheetWidgetBase.h"
#include "NUEquipmentWidget.generated.h"

// 장비창 — 장비 슬롯 그리드 + 미리보기. 인벤 창(I키)을 열면 함께 뜬다 (8/25 회의 결정).
// 칸을 우클릭하면 해제한다 --- 장착은 인벤 창의 우클릭이 맡는다. Ctrl+우클릭 = 메뉴(해제).
// 패드(9/16): 소지품에서 집어 온(A) 장비를 칸에서 A = 장착, RT = 해제, X = 메뉴, LB/RB = 창 넘기기, 포커스 칸 옆에 툴팁.
// 칸은 WBP_ItemCell(프로젝트 세팅 NU UI의 ItemCellClass), 격자는 WBP의 MainSlotGrid(장비)/SubSlotGrid(장신구) --- 모양은 WBP, C++는 칸 순서와 상태만
UCLASS(Abstract)
class PROJECTM_API UNUEquipmentWidget : public UNUSheetWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NU|Sheet")
	void UnequipSlot(ENUEquipSlot InSlot);

	// 패드로 이 창에 오면 첫 장비 칸
	virtual void FocusForPad() override;

protected:
	virtual void RefreshData() override;
	virtual void CloseSelf() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FName GetCellItemRowName(int32 Container, int32 SlotNo) const override;

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Sheet")
	void OnEquipmentSlotsChanged();

	UPROPERTY(BlueprintReadOnly, Category = "NU|Sheet")
	TArray<FNUSheetSlotEntry> EquipSlots;

private:
	// MainSlotGrid(장비)/SubSlotGrid(장신구)에 칸을 채운다 --- 격자 위치·정렬은 WBP, 칸 순서는 여기
	void PopulateSlotList();

	// 슬롯 목록을 격자에 칸으로 놓는다 (인벤 창과 같은 칸 WBP)
	void BuildSlotGrid(class UUniformGridPanel* Grid, const TArray<ENUEquipSlot>& Order, int32 Columns);
	class UNUItemCellWidget* BuildSlotCell(const FNUSheetSlotEntry& Entry);

	// 칸 우클릭 = 해제 (전투 중·빈 칸은 무시) --- 인벤 창의 우클릭 = 장착과 짝을 맞춘다
	FReply HandleCellMouseDown(int32 Container, int32 SlotNo, const FPointerEvent& MouseEvent);

	// 부위 번호로 칸 위젯 찾기 (두 격자 중 어디든)
	class UNUItemCellWidget* FindSlotCell(int32 SlotNo) const;

	// 패드 X·Ctrl+우클릭 --- 낀 것이 있으면 [해제]
	void OpenSlotMenu(int32 SlotNo, const FVector2D& ScreenPos);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UUniformGridPanel> MainSlotGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UUniformGridPanel> SubSlotGrid;

	// 장비 칸 픽셀 크기 — WBP 클래스 기본값에서 조정
	UPROPERTY(EditAnywhere, Category = "NU|Sheet", meta = (ClampMin = 32.f))
	float SlotCellSize = 72.f;

	// 장비 칸 그리드의 한 줄 칸 수. 1이면 세로 한 줄
	UPROPERTY(EditAnywhere, Category = "NU|Sheet", meta = (ClampMin = 1))
	int32 SlotGridColumns = 2;

	// 장신구는 3칸뿐이라 따로 둔다 (메인 장비와 열 수가 다를 수 있다)
	UPROPERTY(EditAnywhere, Category = "NU|Sheet", meta = (ClampMin = 1))
	int32 SubSlotGridColumns = 1;
};
