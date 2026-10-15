#include "Field/NUCharacterPreviewSubsystem.h"

#include "Engine/World.h"
#include "Field/NUCharacterPreviewActor.h"
#include "Inventory/NUInventoryTypes.h" // LogNUInventory

ANUCharacterPreviewActor* UNUCharacterPreviewSubsystem::Acquire()
{
	// 쉬는 것부터 꺼내 쓴다
	for (const TObjectPtr<ANUCharacterPreviewActor>& Actor : Actors)
	{
		if (IsValid(Actor) && !InUse.Contains(Actor))
		{
			InUse.Add(Actor);
			Actor->SetCaptureEnabled(true);
			return Actor;
		}
	}

	if (Actors.Num() >= MaxActors)
	{
		UE_LOG(LogNUInventory, Warning,
			TEXT("미리보기 액터가 모자랍니다 (상한 %d) --- 창이 자리표시로 남습니다"), MaxActors);
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	// 화면 밖 아주 먼 곳 --- 캡처가 자기 자신만 찍으므로 서로 겹쳐도 상관없지만,
	// 게임 화면에 끼어들지 않도록 떼어 놓는다
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	ANUCharacterPreviewActor* Spawned = World->SpawnActor<ANUCharacterPreviewActor>(
		ANUCharacterPreviewActor::StaticClass(),
		FVector(0.f, 0.f, 100000.f + Actors.Num() * 1000.f), FRotator::ZeroRotator, Params);
	if (Spawned == nullptr)
	{
		return nullptr;
	}

	Actors.Add(Spawned);
	InUse.Add(Spawned);
	Spawned->SetCaptureEnabled(true);
	return Spawned;
}

void UNUCharacterPreviewSubsystem::Release(ANUCharacterPreviewActor* Actor)
{
	if (Actor == nullptr)
	{
		return;
	}
	InUse.Remove(Actor);

	// 액터는 남겨 둔다 --- 다음에 창을 열 때 다시 만들지 않으려는 것이 이 대여소의 목적이다
	if (IsValid(Actor))
	{
		Actor->SetCaptureEnabled(false);
	}
}

void UNUCharacterPreviewSubsystem::Deinitialize()
{
	for (const TObjectPtr<ANUCharacterPreviewActor>& Actor : Actors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	Actors.Reset();
	InUse.Reset();

	Super::Deinitialize();
}
