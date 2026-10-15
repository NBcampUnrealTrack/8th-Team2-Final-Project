#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NUActorPresenceTransitionSubsystem.generated.h"

class UNUActorPresenceComponent;
class ANUPlayerController;

// 걷기 애니메이션이 없어 재배치는 순간이동이다. 옮기는 순간이 화면에 보이지 않게 한다.
// 지금 화면 밖이면 바로, 곧 나올 연출의 카메라가 다른 곳을 비추면 그때 옮긴다.
// 둘 다 아니면 대화가 끝난 직후 짧은 암전 한 번에 남은 재배치를 모두 옮긴다.
UCLASS()
class PROJECTM_API UNUActorPresenceTransitionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void Enqueue(UNUActorPresenceComponent* Component);
	void Cancel(UNUActorPresenceComponent* Component);
	bool IsTransitionPending() const { return !Pending.IsEmpty() || Phase != 0; }
	bool IsFading() const { return Phase != 0; }
	virtual void Deinitialize() override;

private:
	void UpdateTransition();
	void ApplyPending();
	void ReleaseFade();
	TArray<TWeakObjectPtr<UNUActorPresenceComponent>> Pending;
	TWeakObjectPtr<ANUPlayerController> FadeController;
	FTimerHandle UpdateTimer;
	double PhaseEnd = 0.0;
	// 화면에 보이는 재배치가 대화·연출 없이 남은 시각. 곧 대기열에 오를 연출을 잠깐 기다린 뒤 암전한다.
	double UncoveredSince = -1.0;
	uint8 Phase = 0;
};
