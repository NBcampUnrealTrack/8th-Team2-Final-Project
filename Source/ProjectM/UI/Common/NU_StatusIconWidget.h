// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Battle/Data/NUStatusEffectDefinition.h"
#include "NU_StatusIconWidget.generated.h"

class UImage;
class UTextBlock;

/*
상태이상 아이콘 한 칸. UNU_CombatantStatusWidget 이 걸려 있는 개수만큼 만들어 붙인다
(UI/HUD/NU_BattleHUDWidget 이 전투원 위젯을 만드는 것과 같은 방식).

아이콘 텍스처는 기획 시트에 컬럼이 없다 --- 아트 산출물이라 기획 데이터가 아니고,
UNUUISettings::StatusIconFolder 아래에서 태그의 마지막 조각으로 찾는다
(NU.State.Ailment.Burn -> T_Burn). 상태이상이 늘어나면 같은 규칙의 이름으로 텍스처만
넣으면 되고, Inventory/UI/NUSheetWidgetBase::LoadItemIconTexture 와 같은 규칙이다.
이미 로드된 에셋이면 TryLoad 가 레지스트리 조회라 12종뿐인 여기서는 캐시를 두지 않는다.

숫자(스택 수 / 남은 턴)를 어느 쪽으로 띄울지는 이 위젯이 고르지 않는다 ---
UNUAbilitySystemComponent::GetActiveStatusViews 가 이미 갈라서 BadgeNumber 로 넘긴다.
표시 규칙을 두 곳에서 각자 해석하지 않게 하기 위함이다.
*/
UCLASS()
class PROJECTM_API UNU_StatusIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 만드는 쪽이 한 번 호출한다. 이후 갱신은 이 위젯을 다시 만드는 방식이라 구독하지 않는다.
	void SetStatusEffect(const FNUStatusEffectView& InView);

protected:
	// 텍스처와 숫자를 채운 뒤 1회 발화. BP 는 분류별 테두리 색 등 추가 표현만 하면 된다. 
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Battle|Status")
	void OnStatusEffectChanged();

	// 태그에서 텍스처를 찾는다. 없으면 nullptr 이고 이름당 한 번만 경고를 남긴다. 
	static UTexture2D* ResolveStatusIcon(const FGameplayTag& StatusEffectTag);

protected:
	// 아이콘 텍스처. WBP 에 같은 이름의 Image 를 두면 그래프 배선 없이 C++ 이 채운다. 
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|Status")
	TObjectPtr<UImage> Img_Icon;

	// 아이콘 위에 겹쳐 띄우는 작은 숫자. 띄울 값이 없으면 접는다. 
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "NU|Battle|Status")
	TObjectPtr<UTextBlock> Txt_Badge;

	// 이름/설명/분류/상한까지 들어 있어 툴팁이나 테두리 색에 그대로 쓸 수 있다. 
	UPROPERTY(BlueprintReadOnly, Category = "NU|Battle|Status")
	FNUStatusEffectView View;
};
