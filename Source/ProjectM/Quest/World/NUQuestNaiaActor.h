#pragma once

#include "CoreMinimal.h"
#include "Quest/World/NUQuestActionActor.h"
#include "NUQuestNaiaActor.generated.h"

class APlayerController;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
class UNUSpriteVisualComponent;

// 퀘스트 상호작용 대상을 필드 나이아 모습으로 보여준다. 목표 진행과 전투 진입은 부모와 같다.
UCLASS(Blueprintable)
class PROJECTM_API ANUQuestNaiaActor : public ANUQuestActionActor
{
	GENERATED_BODY()

public:
	ANUQuestNaiaActor();

	virtual void Tick(float DeltaSeconds) override;

	// 보여줄 나이아. 나이아 정의의 애님 BP를 그대로 써서 전투·동행 나이아와 같은 그림이 나온다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest", meta = (ClampMin = "0"))
	int32 NaiaId = 0;

protected:
	virtual void BeginPlay() override;
	virtual void HandleActorPresence(const FNUActorPresenceRow* Step) override;

private:
	// 등장 표가 맴돌기를 켠 단계에서만 돈다. 연출·대화·전투 중에는 그 자리에 멈춘다.
	bool ShouldPauseHover(const APlayerController& Controller) const;

	// 자리에서 이 거리 안에 온 플레이어 주변을 맴돈다. 멀어지면 자리로 돌아온다.
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Hover", meta = (ClampMin = "0"))
	float HoverLeashRadius = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Hover", meta = (ClampMin = "0"))
	float HoverRadius = 100.f;

	// 플레이어 둘레를 한 바퀴 도는 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Hover", meta = (ClampMin = "0.1"))
	float HoverOrbitSeconds = 9.f;

	// 동행 나이아처럼 목표 위치로 미끄러지듯 따라가는 속도
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Hover", meta = (ClampMin = "0.1"))
	float HoverInterpSpeed = 2.5f;

	FVector HoverHome = FVector::ZeroVector;
	float HoverAngle = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "NU|Quest")
	TObjectPtr<UPaperFlipbookComponent> Sprite;

	UPROPERTY(VisibleAnywhere, Category = "NU|Quest")
	TObjectPtr<UPaperZDAnimationComponent> Animation;

	UPROPERTY(VisibleAnywhere, Category = "NU|Quest")
	TObjectPtr<UNUSpriteVisualComponent> SpriteVisual;

	// 가만히 서 있는 대상이라 NPC와 같은 주기로만 카메라를 바라보게 한다.
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest", meta = (ClampMin = "0.01"))
	float BillboardUpdateInterval = 0.1f;
};
