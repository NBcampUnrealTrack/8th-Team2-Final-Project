#include "Interaction/NUInteractionSpatialRules.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "Field/Interior/NUWalkableDoor.h"
#include "Field/Interior/NUWalkableInteriorComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"

namespace NUInteractionSpatialRules
{
	UNUWalkableInteriorComponent* FindInterior(AActor* Actor)
	{
		if (const UNUInteriorParticipantComponent* Participant = Actor->FindComponentByClass<UNUInteriorParticipantComponent>())
		{
			return Participant->GetCurrentInterior();
		}
		// 참가자 컴포넌트가 없는 상호작용 소품도 실제 실내 범위로 판정한다.
		for (TObjectIterator<UNUWalkableInteriorComponent> It; It; ++It)
		{
			if (It->GetWorld() == Actor->GetWorld() && It->IsRegistered() && It->ContainsParticipant(Actor, false))
			{
				return *It;
			}
		}
		return nullptr;
	}

	bool CanInteract(AActor* Interactor, AActor* Target)
	{
		if (!IsValid(Interactor) || !IsValid(Target) || Interactor->GetWorld() != Target->GetWorld())
		{
			return false;
		}
		for (AActor* Actor : { Interactor, Target })
		{
			const UNUInteriorParticipantComponent* Participant = Actor->FindComponentByClass<UNUInteriorParticipantComponent>();
			if (Participant != nullptr && Participant->IsInteriorTransitionPending())
			{
				return false;
			}
		}
		UNUWalkableInteriorComponent* InteractorInterior = FindInterior(Interactor);
		// 문은 연결된 실내와 실외 양쪽에서 접근하는 경계 액터다.
		if (const ANUWalkableDoor* Door = Cast<ANUWalkableDoor>(Target))
		{
			const UNUWalkableInteriorComponent* DoorInterior = IsValid(Door->InteriorOwner) ? Door->InteriorOwner->FindComponentByClass<UNUWalkableInteriorComponent>() : nullptr;
			return InteractorInterior == nullptr || InteractorInterior == DoorInterior;
		}
		return InteractorInterior == FindInterior(Target);
	}
}
