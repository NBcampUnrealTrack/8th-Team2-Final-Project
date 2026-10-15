// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/Formation/NUBattleFormationTypes.h"
#include "GameFramework/Actor.h"
#include "NUBattleFormationAxis.generated.h"

class UArrowComponent;
class UBillboardComponent;

// 배경 서브레벨에 놓는 진형 기준축, 열 하나를 담당함
// 인원수만큼 축을 따라 가운데 정렬함 (1명은 축 위, 2명은 축 위아래 같은 간격 등)
// 배치 전용이라 복제하지 않음, 세트 자체가 클라마다 로컬로 로딩됨
UCLASS()
class PROJECTM_API ANUBattleFormationAxis : public AActor
{
	GENERATED_BODY()

public:
	ANUBattleFormationAxis();

	ENUBattleFormationRow GetRow() const;

	// 인원 SlotCount 명 중 SlotIndex 번째가 설 자리
	// 회전은 축의 회전을 그대로 물려줌
	FTransform GetSlotTransform(int32 SlotIndex, int32 SlotCount) const;

#if WITH_EDITOR
	// 에디터 뷰포트에서 미리보기를 그리기 위함
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void Tick(float DeltaSeconds) override;
#endif

protected:
	// 이 축이 담당하는 열
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Battle|Formation")
	ENUBattleFormationRow Row = ENUBattleFormationRow::Player;

	// 축 Forward 방향으로 한 칸 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Battle|Formation", meta = (ClampMin = "0"))
	float SlotSpacing = 150.f;

	// 에디터에서 몇 명 기준으로 미리 그려볼지, 실행에는 영향 없음
	UPROPERTY(EditAnywhere, Category = "NU|Battle|Formation|Preview", meta = (ClampMin = "1", ClampMax = "8"))
	int32 PreviewSlotCount = 2;

private:
	UPROPERTY(VisibleAnywhere, Category = "NU|Battle|Formation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBillboardComponent> Icon;

	// 어느 방향으로 정렬되는지 에디터에서 보이게 함
	UPROPERTY(VisibleAnywhere, Category = "NU|Battle|Formation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArrowComponent> DirectionArrow;
};
