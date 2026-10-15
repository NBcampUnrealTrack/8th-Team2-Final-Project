// 통합 캐릭터 메뉴의 장비 페이지 (WBP_CharacterEquipmentPage, 민기 님 디자인) --- 장착 칸 9개와 종합 능력치, 고른 장비의 상세 카드.
// 부모가 정보 페이지인 이유: 가운데 캐릭터 미리보기(CharacterPortrait·PortraitFallback)의 대여·반환·장비 반영을 그대로 물려 쓴다.
// 칸은 WBP에 그려진 것을 그대로 쓴다 (C++가 칸 위젯을 만들지 않는다) --- 이름으로 찾아 아이콘과 보임만 바꾼다.
//
// 이름 계약 (없는 위젯은 그 기능만 빠진다):
//   장착 칸: Equip{Head,Chest,Gloves,Boots,Weapon,Ring1,Ring2,Earring,Legs}... (FNUCharacterMenuSlot 참고, 부위 이름 글자는 WBP 것 그대로)
//   종합 능력치: Summary{Attack,Defense,Health,Speed}Value(TextBlock) --- 장비까지 반영된 지금 수치
//   상세 카드: FNUCharacterMenuItemCard 참고 (버튼은 [장비 해제])
//
// 조작: 칸 클릭 = 고르기, [장비 해제] 버튼, 칸 우클릭 = 바로 해제 (옛 장비창과 같은 규칙). 전투 중에는 장비를 못 바꾼다.
// 패드: D패드 이동·A 누르기는 매니저 창 공용 규칙, 포커스가 선 칸에 RT = 바로 해제
#pragma once

#include "CoreMinimal.h"
#include "UI/CharacterMenu/NUCharacterInfoPage.h"
#include "UI/CharacterMenu/NUCharacterMenuItemCard.h"
#include "NUCharacterEquipmentPage.generated.h"

class UNUAbilitySystemComponent;
class UNUEquipmentComponent;
class UNUInventoryComponent;
struct FOnAttributeChangeData;

UCLASS(Abstract)
class PROJECTM_API UNUCharacterEquipmentPage : public UNUCharacterInfoPage
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NU|CharacterMenu")
	void RefreshSlots();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 우클릭은 버튼이 안 받는다 --- 터널 단계에서 받아 가리키는 칸을 찾는다
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu|Style")
	FNUCharacterMenuSlotStyle SlotStyle;

private:
	// 고른 칸 표시와 상세 카드
	void UpdateSelection();
	void UpdateSummary();

	// 그 칸에 낀 장비의 행 이름 (비었으면 NAME_None)
	FName GetWornRowName(int32 SlotIndex) const;
	bool IsInBattle() const;
	void UnequipSlot(int32 SlotIndex);

	// 조건(마우스가 가리킨다 / 포커스가 있다)에 맞는 칸의 번호
	int32 FindSlot(bool (UWidget::*Test)() const) const;

	void HandleSlotClicked(int32 SlotIndex);
	void HandleBattleChanged();
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);

	UFUNCTION()
	void HandleActionClicked();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNUIndexedButtonRelay>> ButtonRelays;

	// ENUEquipSlot 순서 (WBP에 그 부위 칸이 없으면 Button이 빈 채로 남는다)
	TArray<FNUCharacterMenuSlot> Slots;
	FNUCharacterMenuItemCard Card;
	int32 SelectedSlot = INDEX_NONE;

	TWeakObjectPtr<UNUInventoryComponent> Inventory;
	TWeakObjectPtr<UNUEquipmentComponent> Equipment;
	TWeakObjectPtr<UNUAbilitySystemComponent> ASC;
};
