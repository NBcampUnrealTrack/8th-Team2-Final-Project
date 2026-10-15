// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/Formation/NUBattleFormationTypes.h"
#include "GameFramework/Actor.h"
#include "NUBattleFormationSlot.generated.h"

class UArrowComponent;
class UBillboardComponent;

// 배경 서브레벨에 배치되는 아바타 스폰 포인트
// 보스와 잡몹처럼 정렬하지 않고 손으로 배치해야 하는 구도에 사용함
// 배치 전용이라 복제하지 않음
UCLASS()
class PROJECTM_API ANUBattleFormationSlot : public AActor
{
	GENERATED_BODY()

public:
	ANUBattleFormationSlot();

	ENUBattleFormationRow GetRow() const;
	int32 GetSlotIndex() const;

protected:
	// 이 자리가 속한 열
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Battle|Formation")
	ENUBattleFormationRow Row = ENUBattleFormationRow::Enemy;

	// 같은 열 안에서의 자리 번호
	// 액터 순회 순서는 보장되지 않으므로 순서를 데이터로 들고 있어야 함
	// 적 열은 EnemyPartyIds 의 작성 순서와 이 번호가 맞아야 특정 적을 원하는 위치에 세울 수 있음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Battle|Formation", meta = (ClampMin = "0"))
	int32 SlotIndex = 0;

private:
	UPROPERTY(VisibleAnywhere, Category = "NU|Battle|Formation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBillboardComponent> Icon;

	UPROPERTY(VisibleAnywhere, Category = "NU|Battle|Formation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArrowComponent> DirectionArrow;
};
