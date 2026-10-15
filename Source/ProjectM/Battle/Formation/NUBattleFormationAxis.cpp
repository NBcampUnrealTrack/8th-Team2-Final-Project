// Fill out your copyright notice in the Description page of Project Settings.


#include "Battle/Formation/NUBattleFormationAxis.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"


#if WITH_EDITOR
#include "DrawDebugHelpers.h"
#endif


ANUBattleFormationAxis::ANUBattleFormationAxis()
{
	// 에디터 미리보기에만 필요해서 런타임엔 Tick이 필요없음
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Icon = CreateDefaultSubobject<UBillboardComponent>(TEXT("Icon"));
	SetRootComponent(Icon);

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(Icon);

#if WITH_EDITOR
	// 에디터 환경에선 뷰포트에서 미리볼 수 있게 Tick을 켬
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
#endif
}

ENUBattleFormationRow ANUBattleFormationAxis::GetRow() const
{
	return Row;
}

FTransform ANUBattleFormationAxis::GetSlotTransform(int32 SlotIndex, int32 SlotCount) const
{
	// 축을 기준으로 가운데 정렬
	// 1명은 축 위, 2명은 축 위아래로 같은 간격
	const int32 SafeCount = FMath::Max(SlotCount, 1);
	const float CenteredIndex = SlotIndex - (SafeCount - 1) * 0.5f;

	const FVector SlotLocation =
		GetActorLocation() + GetActorForwardVector() * (CenteredIndex * SlotSpacing);

	return FTransform(GetActorRotation(), SlotLocation);
}


// ---------------------
//	에디터 환경 전용 함수
// ---------------------

#if WITH_EDITOR
bool ANUBattleFormationAxis::ShouldTickIfViewportsOnly() const
{
	return true;
}

void ANUBattleFormationAxis::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();

	// 실행 중에는 그리지 않음, 에디터 뷰포트 전용 표시임
	if (!IsValid(World) || World->IsGameWorld())
	{
		return;
	}

	const int32 SafePreview = FMath::Max(PreviewSlotCount, 1);
	for (int32 Index = 0; Index < SafePreview; ++Index)
	{
		DrawDebugSphere(
			World,
			GetSlotTransform(Index, SafePreview).GetLocation(),
			40.f,
			12,
			FColor::Green,
			false,
			-1.f,
			SDPG_Foreground
			);
	}
}
#endif

// -------------------------------